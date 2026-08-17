#include <QCoreApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

#include "RenderCliSceneLoader.h"

#include "world/objects/Scene.h"
#include "world/objects/Camera.h"
#include "world/objects/Group.h"
#include "world/objects/Material.h"
#include "world/objects/StepVisibilityEvaluator.h"
#include "world/objects/Texture.h"

#include "engine/graph/GraphRenderEngine.h"
#include "engine/graph/RasterPassState.h"
#include "engine/graph/RenderEngineOptions.h"
#include "engine/graph/RenderGraphExecutionTrace.h"
#include "engine/graph/RenderGraphRequest.h"
#include "engine/graph/TracingExecutionPreference.h"
#include "engine/graph/WireframePassState.h"
#include "render/lights/PointLight.h"
#include "render/RenderEngine.h"
#include "engine/raytracer/Raytracer.h"
#include "render/PathTracingIntegrator.h"
#include "render/RayFamilyQueuePolicy.h"
#include "engine/raster/RasterBackend.h"
#include "engine/raster/Rasterizer.h"
#include "engine/wavefront/WavefrontRaytracer.h"
#include "engine/wireframe/Wireframe.h"
#include "render/denoise/BilateralDenoiser.h"
#include "render/denoise/BoxDenoiser.h"
#include "render/materials/Material.h"
#include "render/primitives/Scene.h"
#include "render/cameras/Camera.h"
#include "render/samplers/SamplerFactory.h"
#include "render/tonemap/TonemapFactory.h"
#include "render/viewplanes/TiledViewPlane.h"

#include "core/Buffer.h"
#include "core/math/Constants.h"

#include "engine/graph/RenderAOV.h"

#include <QThread>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iomanip>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <numeric>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

Q_DECLARE_METATYPE(Vector3d);
Q_DECLARE_METATYPE(Angled);
Q_DECLARE_METATYPE(Colord);

namespace {
  using Clock = std::chrono::steady_clock;
  constexpr std::uint64_t maxExactJsonInteger = 9007199254740991ULL;
  constexpr int defaultWavefrontAdaptiveMinimumSamples = 2;
  constexpr double defaultWavefrontAdaptiveStddevThreshold = 0.05;

  struct TimingStats {
    double minMs;
    double medianMs;
    double avgMs;
    double maxMs;
  };

  double elapsedMilliseconds(Clock::time_point start, Clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
  }

  TimingStats summarizeTimings(std::vector<double> timings) {
    std::sort(timings.begin(), timings.end());
    const std::size_t n = timings.size();
    const double median =
      (n % 2 == 0) ? (timings[n / 2 - 1] + timings[n / 2]) / 2.0 : timings[n / 2];
    const double total = std::accumulate(timings.begin(), timings.end(), 0.0);
    return {timings.front(), median, total / static_cast<double>(n), timings.back()};
  }

  void printTimings(const std::vector<double>& timings) {
    const auto stats = summarizeTimings(timings);
    std::cout << std::fixed << std::setprecision(3) << "render_ms"
              << " runs=" << timings.size() << " min=" << stats.minMs
              << " median=" << stats.medianMs << " avg=" << stats.avgMs << " max=" << stats.maxMs
              << '\n';
  }

  bool isRasterMetricsObject(const QJsonObject& metadata) {
    return metadata.contains("timings") && metadata.contains("fragments") &&
           metadata.contains("tessellation");
  }

  bool isIntersectionServiceMetricsObject(const QJsonObject& metadata) {
    return metadata.value("intersectionService").isObject();
  }

  std::uint64_t unsignedJsonValue(const QJsonObject& object, const char* key) {
    return static_cast<std::uint64_t>(object.value(key).toDouble());
  }

  std::string compactSummaryText(const QJsonValue& value, const std::string& empty) {
    std::string result = value.toString().toStdString();
    if (result.empty()) {
      return empty;
    }
    std::replace_if(
      result.begin(), result.end(), [](unsigned char ch) { return std::isspace(ch) != 0; }, '_');
    return result;
  }

  std::string compactSummaryBool(const QJsonValue& value, const std::string& empty) {
    if (value.isBool()) {
      return value.toBool() ? "true" : "false";
    }
    return empty;
  }

  std::string compactSummaryToken(std::string value) {
    if (value.empty()) {
      return "unknown";
    }
    std::replace_if(
      value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch) != 0; }, '_');
    return value;
  }

  std::string compactUnsignedObjectPairs(const QJsonObject& object) {
    if (object.isEmpty()) {
      return "none";
    }

    std::vector<std::string> pairs;
    pairs.reserve(object.size());
    for (auto it = object.begin(); it != object.end(); ++it) {
      pairs.push_back(compactSummaryToken(it.key().toStdString()) + ":" +
                      std::to_string(static_cast<std::uint64_t>(it.value().toDouble())));
    }
    std::sort(pairs.begin(), pairs.end());

    std::string result = pairs.front();
    for (std::size_t index = 1; index != pairs.size(); ++index) {
      result += ",";
      result += pairs[index];
    }
    return result;
  }

  void printRasterMetricsSummary(int run, const QString& passId, const QJsonObject& metrics) {
    const QJsonObject timings = metrics.value("timings").toObject();
    const QJsonObject tessellation = metrics.value("tessellation").toObject();
    const QJsonObject scheduling = metrics.value("scheduling").toObject();
    const QJsonObject fragments = metrics.value("fragments").toObject();
    const QJsonObject depthPrepass = metrics.value("depthPrepass").toObject();
    std::cout << std::fixed << std::setprecision(3) << "raster_metrics"
              << " run=" << run;
    if (!passId.isEmpty()) {
      std::cout << " pass=" << passId.toStdString();
    }
    std::cout
      << " total_ms=" << timings.value("totalRenderSeconds").toDouble() * 1000.0
      << " queue=" << static_cast<std::uint64_t>(scheduling.value("resolvedQueueSize").toDouble())
      << " queue_decision=" << scheduling.value("decision").toString().toStdString()
      << " queue_reason=" << scheduling.value("reason").toString().toStdString()
      << " depth_prepass=" << depthPrepass.value("requested").toString().toStdString()
      << " depth_prepass_decision=" << depthPrepass.value("decision").toString().toStdString()
      << " depth_prepass_ms=" << depthPrepass.value("totalMeasuredSeconds").toDouble() * 1000.0
      << " raster_ms=" << timings.value("rasterLoopSeconds").toDouble() * 1000.0 << " triangles="
      << static_cast<std::uint64_t>(tessellation.value("trianglesAfterClipping").toDouble())
      << " cull_rejects="
      << static_cast<std::uint64_t>(tessellation.value("trianglesRejectedByCulling").toDouble())
      << " winding_degenerate_rejects="
      << static_cast<std::uint64_t>(
           tessellation.value("trianglesRejectedByWindingOrDegeneracy").toDouble())
      << " covered_samples="
      << static_cast<std::uint64_t>(fragments.value("coveredSamples").toDouble())
      << " depth_tests=" << static_cast<std::uint64_t>(fragments.value("depthTests").toDouble())
      << " coverage_minus_shaded="
      << static_cast<std::uint64_t>(fragments.value("coverageMinusShadedFragments").toDouble())
      << " depth_tests_minus_color_writes="
      << static_cast<std::uint64_t>(fragments.value("depthTestsMinusColorWrites").toDouble())
      << " coarse_depth_rejects="
      << static_cast<std::uint64_t>(
           fragments.value("conservativeDepthRejectedTriangleTiles").toDouble())
      << " shaded_fragments="
      << static_cast<std::uint64_t>(fragments.value("shadedFragments").toDouble())
      << " color_writes=" << static_cast<std::uint64_t>(fragments.value("colorWrites").toDouble())
      << '\n';
  }

  void printIntersectionServiceSummary(int run, const QString& passId,
                                       const QJsonObject& metadata) {
    const QJsonObject service = metadata.value("intersectionService").toObject();
    if (service.isEmpty()) {
      return;
    }

    std::cout << "intersection_service"
              << " run=" << run;
    if (!passId.isEmpty()) {
      std::cout << " pass=" << passId.toStdString();
    }
    std::cout
      << " query_family=" << compactSummaryText(service.value("queryFamily"), "unknown")
      << " query_tag=" << compactSummaryText(service.value("queryTag"), "none")
      << " requested_backend=" << compactSummaryText(service.value("requestedBackend"), "unknown")
      << " selected_backend=" << compactSummaryText(service.value("selectedBackend"), "unknown")
      << " availability=" << compactSummaryText(service.value("availability"), "unknown")
      << " platform=" << compactSummaryText(service.value("platformName"), "none")
      << " fallback=" << compactSummaryText(service.value("fallbackReason"), "none")
      << " execution_path=" << compactSummaryText(service.value("executionPath"), "none")
      << " closest_hit_execution="
      << compactSummaryText(service.value("closestHitExecutionPath"), "none")
      << " any_hit_execution=" << compactSummaryText(service.value("anyHitExecutionPath"), "none")
      << " compiled_scene=" << compactSummaryBool(service.value("compiledScene"), "false")
      << " scene_primitives=" << unsignedJsonValue(service, "scenePrimitives")
      << " scene_supported_primitives=" << unsignedJsonValue(service, "sceneSupportedPrimitives")
      << " scene_unsupported_primitives="
      << unsignedJsonValue(service, "sceneUnsupportedPrimitives") << " scene_unsupported_by_reason="
      << compactUnsignedObjectPairs(service.value("sceneUnsupportedReasons").toObject())
      << " scene_upload_bytes=" << unsignedJsonValue(service, "sceneUploadBytes")
      << " queries=" << unsignedJsonValue(service, "queryCount")
      << " hits=" << unsignedJsonValue(service, "hitCount")
      << " primary_queries=" << unsignedJsonValue(service, "primaryQueryCount")
      << " primary_hits=" << unsignedJsonValue(service, "primaryHitCount")
      << " shadow_queries=" << unsignedJsonValue(service, "shadowQueryCount")
      << " occluded=" << unsignedJsonValue(service, "occludedCount") << " closest_hit_upload_bytes="
      << unsignedJsonValue(service, "closestHitRayUploadBytesEstimate")
      << " closest_hit_readback_bytes="
      << unsignedJsonValue(service, "closestHitReadbackBytesEstimate")
      << " any_hit_upload_bytes=" << unsignedJsonValue(service, "anyHitRayUploadBytesEstimate")
      << " any_hit_readback_bytes=" << unsignedJsonValue(service, "anyHitReadbackBytesEstimate")
      << " query_transfer_bytes=" << unsignedJsonValue(service, "queryTransferBytesEstimate")
      << " closest_hit_frontier_residency="
      << compactSummaryText(service.value("closestHitFrontierResidency"), "none")
      << " any_hit_frontier_residency="
      << compactSummaryText(service.value("anyHitFrontierResidency"), "none")
      << " closest_hit_frontier_packed_ray_bytes="
      << unsignedJsonValue(service, "closestHitFrontierPackedRayBytes")
      << " closest_hit_frontier_host_packed_ray_bytes="
      << unsignedJsonValue(service, "closestHitFrontierHostPackedRayBytes")
      << " closest_hit_frontier_host_query_bytes="
      << unsignedJsonValue(service, "closestHitFrontierHostQueryBytes")
      << " closest_hit_frontier_state_handle_bytes="
      << unsignedJsonValue(service, "closestHitFrontierStateHandleBytes")
      << " any_hit_frontier_packed_ray_bytes="
      << unsignedJsonValue(service, "anyHitFrontierPackedRayBytes")
      << " any_hit_frontier_host_packed_ray_bytes="
      << unsignedJsonValue(service, "anyHitFrontierHostPackedRayBytes")
      << " any_hit_frontier_host_query_bytes="
      << unsignedJsonValue(service, "anyHitFrontierHostQueryBytes")
      << " any_hit_frontier_state_handle_bytes="
      << unsignedJsonValue(service, "anyHitFrontierStateHandleBytes") << '\n';
  }

  class WavefrontMetricsFormatter {
  public:
    bool isMetricsObject(const QJsonObject& metadata) const {
      if (!metadata.contains("batching") || !metadata.contains("input")) {
        return false;
      }
      if (metadata.contains("timings") && metadata.contains("convergence")) {
        return true;
      }
      const QJsonObject batching = metadata.value("batching").toObject();
      return metadata.contains("compiledDiffusePathLoop") &&
             batching.value("executionMode").toString() == "compiled_diffuse_path_loop";
    }

    void printSummary(int run, const QString& passId, const QJsonObject& metrics) const {
      const QJsonObject timings = metrics.value("timings").toObject();
      const QJsonObject input = metrics.value("input").toObject();
      const QJsonObject tiling = metrics.value("tiling").toObject();
      const QJsonObject accumulation = metrics.value("accumulation").toObject();
      const QJsonObject accumulationLayout = accumulation.value("layout").toObject();
      const QJsonObject batching = metrics.value("batching").toObject();
      const QJsonObject tracingExecution = metrics.value("tracingExecution").toObject();
      const QJsonObject convergence = metrics.value("convergence").toObject();
      const QJsonObject adaptiveSampling = metrics.value("adaptiveSampling").toObject();
      const QJsonObject denoise = metrics.value("denoise").toObject();
      const QJsonArray activeSamples = batching.value("activeSamplesPerDepth").toArray();
      const QJsonArray retainedActiveSamples =
        batching.value("retainedActiveSamplesPerDepth").toArray();
      const QJsonArray activeHostPathStateBytes =
        batching.value("activeHostPathStateBytesPerDepth").toArray();
      const QJsonArray activeHitHostBytes = batching.value("activeHitHostBytesPerDepth").toArray();
      const QJsonArray retainedHostPathStateBytes =
        batching.value("retainedHostPathStateBytesPerDepth").toArray();
      const QJsonArray spawnedContinuations =
        batching.value("spawnedContinuationSamplesPerDepth").toArray();
      const QJsonArray spawnedContinuationHostPathStateBytes =
        batching.value("spawnedContinuationHostPathStateBytesPerDepth").toArray();
      const QJsonArray frontierHits = batching.value("frontierRayHitsPerDepth").toArray();
      const QJsonArray frontierMisses = batching.value("frontierRayMissesPerDepth").toArray();
      const QJsonArray frontierPackets = batching.value("frontierPacketChunksPerDepth").toArray();
      const QJsonArray frontierPacketRays = batching.value("frontierPacketRaysPerDepth").toArray();
      const QJsonArray frontierClosestHitBatches =
        batching.value("frontierClosestHitBatchChunksPerDepth").toArray();
      const QJsonArray frontierClosestHitBatchRays =
        batching.value("frontierClosestHitBatchRaysPerDepth").toArray();
      const QJsonArray directLightAnyHitBatches =
        batching.value("directLightAnyHitBatchChunksPerDepth").toArray();
      const QJsonArray directLightAnyHitBatchRays =
        batching.value("directLightAnyHitBatchRaysPerDepth").toArray();
      const QJsonArray directLightSelectionHostBytes =
        batching.value("directLightSelectionHostBytesPerDepth").toArray();
      const QJsonArray directLightOcclusionHostBytes =
        batching.value("directLightOcclusionHostBytesPerDepth").toArray();
      const QJsonArray directLightContributionHostBytes =
        batching.value("directLightContributionHostBytesPerDepth").toArray();
      const QJsonArray directLightAnyHitFrontierPackedRayBytes =
        batching.value("directLightAnyHitFrontierPackedRayBytesPerDepth").toArray();
      const QJsonArray directLightAnyHitFrontierHostPackedRayBytes =
        batching.value("directLightAnyHitFrontierHostPackedRayBytesPerDepth").toArray();
      const QJsonArray directLightAnyHitFrontierHostQueryBytes =
        batching.value("directLightAnyHitFrontierHostQueryBytesPerDepth").toArray();
      const QJsonArray directLightAnyHitFrontierStateHandleBytes =
        batching.value("directLightAnyHitFrontierStateHandleBytesPerDepth").toArray();
      const QJsonArray frontierRay4Packets =
        batching.value("frontierRay4PacketChunksPerDepth").toArray();
      const QJsonArray frontierRay8Packets =
        batching.value("frontierRay8PacketChunksPerDepth").toArray();
      const QJsonArray frontierScalarRays = batching.value("frontierScalarRaysPerDepth").toArray();
      const QJsonArray frontierPacketScalarFallbackRays =
        batching.value("frontierPacketScalarFallbackRaysPerDepth").toArray();
      const QJsonObject frontierPacketScalarFallbackByReason =
        batching.value("frontierPacketScalarFallbackRaysByReason").toObject();
      const QJsonObject intersectionSceneUnsupportedReasons =
        batching.value("intersectionSceneUnsupportedReasons").toObject();
      const QJsonObject tracingSceneUnsupportedMaterialReasons =
        batching.value("tracingSceneUnsupportedMaterialReasons").toObject();
      const QJsonObject tracingSceneUnsupportedTextureReasons =
        batching.value("tracingSceneUnsupportedTextureReasons").toObject();
      const QJsonObject tracingSceneUnsupportedLightReasons =
        batching.value("tracingSceneUnsupportedLightReasons").toObject();
      const QJsonArray frontierPacketRefinedRays =
        batching.value("frontierPacketRefinedRaysPerDepth").toArray();
      const QJsonObject frontierPacketRefinedByMaterial =
        batching.value("frontierPacketRefinedRaysByMaterial").toObject();
      const QJsonArray rmsDelta = batching.value("radianceDeltaRmsPerDepth").toArray();
      const std::uint64_t frontierPacketRayCount = unsignedArraySum(frontierPacketRays);
      const std::uint64_t frontierClosestHitBatchChunkCount =
        unsignedArraySum(frontierClosestHitBatches);
      const std::uint64_t frontierClosestHitBatchRayCount =
        unsignedArraySum(frontierClosestHitBatchRays);
      const std::uint64_t directLightAnyHitBatchChunkCount =
        unsignedArraySum(directLightAnyHitBatches);
      const std::uint64_t directLightAnyHitBatchRayCount =
        unsignedArraySum(directLightAnyHitBatchRays);
      const std::uint64_t frontierRay4PacketChunkCount = unsignedArraySum(frontierRay4Packets);
      const std::uint64_t frontierRay8PacketChunkCount = unsignedArraySum(frontierRay8Packets);
      const std::uint64_t frontierScalarRayCount = unsignedArraySum(frontierScalarRays);
      const std::uint64_t frontierPacketScalarFallbackRayCount =
        unsignedArraySum(frontierPacketScalarFallbackRays);
      const std::string tracingBackendFallback =
        tracingBackendFallbackSummary(batching, tracingExecution);
      const double frontierPacketLaneCapacity =
        static_cast<double>(frontierRay8PacketChunkCount) * 8.0 +
        static_cast<double>(frontierRay4PacketChunkCount) * 4.0;
      std::cout << std::fixed << std::setprecision(3) << "wavefront_metrics"
                << " run=" << run;
      if (!passId.isEmpty()) {
        std::cout << " pass=" << passId.toStdString();
      }
      std::cout
        << " sampling_seed=" << compactValue(input.value("samplingSeed"), "none")
        << " sample_stream_mode=" << compactSummaryText(input.value("sampleStreamMode"), "unknown")
        << " total_ms=" << timings.value("totalRenderSeconds").toDouble() * 1000.0
        << " sample_gen_worker_ms="
        << timings.value("sampleGenerationWorkerSeconds").toDouble() * 1000.0
        << " sample_stream_worker_ms="
        << timings.value("sampleStreamWorkerSeconds").toDouble() * 1000.0
        << " sample_primary_ray_worker_ms="
        << timings.value("primaryRayWorkerSeconds").toDouble() * 1000.0
        << " sample_enqueue_worker_ms="
        << timings.value("sampleEnqueueWorkerSeconds").toDouble() * 1000.0
        << " sample_gen_overhead_worker_ms="
        << timings.value("sampleGenerationOverheadWorkerSeconds").toDouble() * 1000.0
        << " integrator_worker_ms="
        << timings.value("integratorBatchWorkerSeconds").toDouble() * 1000.0
        << " integrator_intersection_worker_ms="
        << timings.value("integratorIntersectionWorkerSeconds").toDouble() * 1000.0
        << " integrator_shading_worker_ms="
        << timings.value("integratorShadingWorkerSeconds").toDouble() * 1000.0
        << " integrator_overhead_worker_ms="
        << timings.value("integratorOverheadWorkerSeconds").toDouble() * 1000.0
        << " integrator_path_setup_worker_ms="
        << timings.value("integratorPathSetupWorkerSeconds").toDouble() * 1000.0
        << " integrator_frontier_partition_worker_ms="
        << timings.value("integratorFrontierPartitionWorkerSeconds").toDouble() * 1000.0
        << " integrator_frontier_bookkeeping_worker_ms="
        << timings.value("integratorFrontierBookkeepingWorkerSeconds").toDouble() * 1000.0
        << " integrator_progress_snapshot_worker_ms="
        << timings.value("integratorProgressSnapshotWorkerSeconds").toDouble() * 1000.0
        << " integrator_convergence_test_worker_ms="
        << timings.value("integratorConvergenceTestWorkerSeconds").toDouble() * 1000.0
        << " integrator_residual_worker_ms="
        << timings.value("integratorResidualWorkerSeconds").toDouble() * 1000.0
        << " integrator=" << batching.value("integrator").toString().toStdString()
        << " execution=" << batching.value("executionMode").toString().toStdString()
        << " tracing_backend_request="
        << compactSummaryText(batching.value("tracingBackendRequest"), "unknown")
        << " tracing_backend=" << compactSummaryText(batching.value("tracingBackend"), "unknown")
        << " tracing_backend_mode="
        << compactSummaryText(batching.value("tracingBackendMode"), "unknown")
        << " tracing_backend_platform="
        << compactSummaryText(batching.value("tracingBackendPlatform"), "none")
        << " tracing_backend_fallback=" << tracingBackendFallback
        << " tracing_backend_capabilities="
        << batching.value("tracingBackendCapabilities").toArray().size()
        << " tracing_backend_fallback_capabilities="
        << fallbackCapabilitySummary(batching.value("tracingBackendCapabilities").toArray())
        << " tracing_backend_restricted_capabilities="
        << restrictedCapabilitySummary(batching.value("tracingBackendCapabilities").toArray())
        << " intersection_backend_request="
        << batching.value("intersectionBackendRequest").toString().toStdString()
        << " intersection_backend="
        << batching.value("intersectionBackend").toString().toStdString()
        << " intersection_backend_platform="
        << compactSummaryText(batching.value("intersectionBackendPlatform"), "none")
        << " intersection_backend_availability="
        << batching.value("intersectionBackendAvailability").toString().toStdString()
        << " intersection_backend_fallback="
        << compactSummaryText(batching.value("intersectionBackendFallbackReason"), "none")
        << " intersection_backend_execution="
        << compactSummaryText(batching.value("intersectionBackendExecutionPath"), "unknown")
        << " closest_hit_execution="
        << compactSummaryText(batching.value("intersectionBackendClosestHitExecutionPath"), "none")
        << " closest_hit_frontier_residency="
        << compactSummaryText(batching.value("intersectionBackendClosestHitFrontierResidency"),
                            "none")
        << " closest_hit_frontier_packed_ray_bytes="
        << unsignedJsonValue(batching, "intersectionBackendClosestHitFrontierPackedRayBytes")
        << " closest_hit_frontier_host_packed_ray_bytes="
        << unsignedJsonValue(batching, "intersectionBackendClosestHitFrontierHostPackedRayBytes")
        << " closest_hit_frontier_host_query_bytes="
        << unsignedJsonValue(batching, "intersectionBackendClosestHitFrontierHostQueryBytes")
        << " closest_hit_frontier_state_handle_bytes="
        << unsignedJsonValue(batching, "intersectionBackendClosestHitFrontierStateHandleBytes")
        << " any_hit_execution="
        << compactSummaryText(batching.value("intersectionBackendAnyHitExecutionPath"), "none")
        << " any_hit_frontier_residency="
        << compactSummaryText(batching.value("intersectionBackendAnyHitFrontierResidency"), "none")
        << " any_hit_frontier_packed_ray_bytes="
        << unsignedJsonValue(batching, "intersectionBackendAnyHitFrontierPackedRayBytes")
        << " any_hit_frontier_host_packed_ray_bytes="
        << unsignedJsonValue(batching, "intersectionBackendAnyHitFrontierHostPackedRayBytes")
        << " any_hit_frontier_host_query_bytes="
        << unsignedJsonValue(batching, "intersectionBackendAnyHitFrontierHostQueryBytes")
        << " any_hit_frontier_state_handle_bytes="
        << unsignedJsonValue(batching, "intersectionBackendAnyHitFrontierStateHandleBytes")
        << " intersection_backend_gpu_device="
        << (batching.value("intersectionBackendPlatformGpuDeviceAvailable").toBool() ? "true"
                                                                                     : "false")
        << " intersection_backend_gpu_render_path="
        << (batching.value("intersectionBackendPlatformGpuRenderPathAvailable").toBool() ? "true"
                                                                                         : "false")
        << " intersection_expected_rays="
        << unsignedJsonValue(batching, "intersectionBackendExpectedRays")
        << " intersection_expected_closest_hit_rays="
        << unsignedJsonValue(batching, "intersectionBackendExpectedClosestHitRays")
        << " intersection_expected_any_hit_rays="
        << unsignedJsonValue(batching, "intersectionBackendExpectedAnyHitRays")
        << " intersection_auto_minimum_gpu_rays="
        << unsignedJsonValue(batching, "intersectionBackendAutoMinimumGpuRays")
        << " intersection_auto_estimated_query_transfer_bytes="
        << unsignedJsonValue(batching, "intersectionBackendAutoEstimatedQueryTransferBytes")
        << " intersection_scene_compiled="
        << (batching.value("intersectionSceneCompiled").toBool() ? "true" : "false")
        << " intersection_scene_bvh_nodes=" << unsignedJsonValue(batching, "intersectionSceneBvhNodes")
        << " intersection_scene_primitives="
        << unsignedJsonValue(batching, "intersectionScenePrimitives")
        << " intersection_scene_triangles=" << unsignedJsonValue(batching, "intersectionSceneTriangles")
        << " intersection_scene_spheres=" << unsignedJsonValue(batching, "intersectionSceneSpheres")
        << " intersection_scene_planes=" << unsignedJsonValue(batching, "intersectionScenePlanes")
        << " intersection_scene_rectangles="
        << unsignedJsonValue(batching, "intersectionSceneRectangles")
        << " intersection_scene_disks=" << unsignedJsonValue(batching, "intersectionSceneDisks")
        << " intersection_scene_open_cylinders="
        << unsignedJsonValue(batching, "intersectionSceneOpenCylinders")
        << " intersection_scene_tori=" << unsignedJsonValue(batching, "intersectionSceneTori")
        << " intersection_scene_transforms="
        << unsignedJsonValue(batching, "intersectionSceneTransforms")
        << " intersection_scene_unsupported="
        << unsignedJsonValue(batching, "intersectionSceneUnsupportedPrimitives")
        << " intersection_scene_unsupported_by_reason="
        << compactUnsignedObjectPairs(intersectionSceneUnsupportedReasons)
        << " intersection_scene_upload_bytes="
        << unsignedJsonValue(batching, "intersectionSceneUploadBytes")
        << " intersection_scene_triangle_kernel_eligible="
        << (batching.value("intersectionSceneTriangleClosestHitEligible").toBool() ? "true"
                                                                                   : "false")
        << " intersection_scene_basic_hit_kernel_eligible="
        << (batching.value("intersectionSceneBasicHitEligible").toBool() ? "true" : "false")
        << " intersection_scene_packed_closest_hit_eligible="
        << (batching.value("intersectionScenePackedClosestHitEligible").toBool() ? "true" : "false")
        << " intersection_scene_packed_any_hit_eligible="
        << (batching.value("intersectionScenePackedAnyHitEligible").toBool() ? "true" : "false")
        << " tracing_scene_compiled="
        << (batching.value("tracingSceneCompiled").toBool() ? "true" : "false")
        << " tracing_scene_materials=" << unsignedJsonValue(batching, "tracingSceneMaterials")
        << " tracing_scene_textures=" << unsignedJsonValue(batching, "tracingSceneTextures")
        << " tracing_scene_lights=" << unsignedJsonValue(batching, "tracingSceneLights")
        << " tracing_scene_environment=" << unsignedJsonValue(batching, "tracingSceneEnvironment")
        << " tracing_scene_debug_ids=" << unsignedJsonValue(batching, "tracingSceneDebugIds")
        << " tracing_scene_unsupported_materials="
        << unsignedJsonValue(batching, "tracingSceneUnsupportedMaterials")
        << " tracing_scene_unsupported_textures="
        << unsignedJsonValue(batching, "tracingSceneUnsupportedTextures")
        << " tracing_scene_unsupported_lights="
        << unsignedJsonValue(batching, "tracingSceneUnsupportedLights")
        << " tracing_scene_unsupported_materials_by_reason="
        << compactUnsignedObjectPairs(tracingSceneUnsupportedMaterialReasons)
        << " tracing_scene_unsupported_textures_by_reason="
        << compactUnsignedObjectPairs(tracingSceneUnsupportedTextureReasons)
        << " tracing_scene_unsupported_lights_by_reason="
        << compactUnsignedObjectPairs(tracingSceneUnsupportedLightReasons)
        << " tracing_scene_upload_bytes=" << unsignedJsonValue(batching, "tracingSceneUploadBytes")
        << " intersection_estimated_ray_upload_bytes="
        << unsignedJsonValue(batching, "intersectionEstimatedRayUploadBytes")
        << " intersection_estimated_closest_hit_ray_upload_bytes="
        << unsignedJsonValue(batching, "intersectionEstimatedClosestHitRayUploadBytes")
        << " intersection_estimated_any_hit_ray_upload_bytes="
        << unsignedJsonValue(batching, "intersectionEstimatedAnyHitRayUploadBytes")
        << " intersection_estimated_closest_hit_readback_bytes="
        << unsignedJsonValue(batching, "intersectionEstimatedClosestHitReadbackBytes")
        << " intersection_estimated_any_hit_readback_bytes="
        << unsignedJsonValue(batching, "intersectionEstimatedAnyHitReadbackBytes")
        << " intersection_estimated_query_transfer_bytes="
        << unsignedJsonValue(batching, "intersectionEstimatedQueryTransferBytes")
        << " intersection_estimated_closest_hit_query_transfer_bytes="
        << unsignedJsonValue(batching, "intersectionEstimatedClosestHitQueryTransferBytes")
        << " intersection_estimated_any_hit_query_transfer_bytes="
        << unsignedJsonValue(batching, "intersectionEstimatedAnyHitQueryTransferBytes")
        << " intersection_estimated_query_round_trips="
        << unsignedJsonValue(batching, "intersectionEstimatedQueryRoundTrips")
        << " intersection_estimated_closest_hit_query_round_trips="
        << unsignedJsonValue(batching, "intersectionEstimatedClosestHitQueryRoundTrips")
        << " intersection_estimated_any_hit_query_round_trips="
        << unsignedJsonValue(batching, "intersectionEstimatedAnyHitQueryRoundTrips")
        << " intersection_backend_upload_worker_ms="
        << batching.value("intersectionBackendUploadWorkerSeconds").toDouble() * 1000.0
        << " intersection_backend_kernel_worker_ms="
        << batching.value("intersectionBackendKernelWorkerSeconds").toDouble() * 1000.0
        << " intersection_backend_readback_worker_ms="
        << batching.value("intersectionBackendReadbackWorkerSeconds").toDouble() * 1000.0
        << " intersection_rays_per_worker_second="
        << batching.value("intersectionRaysPerWorkerSecond").toDouble()
        << " intersection_backend_kernel_rays_per_second="
        << batching.value("intersectionBackendKernelRaysPerSecond").toDouble()
        << " intersection_rays=" << unsignedJsonValue(batching, "intersectionRaysSubmitted")
        << " closest_hit_rays=" << unsignedJsonValue(batching, "closestHitRaysSubmitted")
        << " any_hit_rays=" << unsignedJsonValue(batching, "anyHitRaysSubmitted")
        << " closest_hit_queries=" << unsignedJsonValue(batching, "closestHitQueries")
        << " any_hit_queries=" << unsignedJsonValue(batching, "anyHitQueries")
        << " frontier_query_round_trips=" << unsignedJsonValue(batching, "frontierQueryRoundTrips")
        << " frontier_resident_query_round_trips_estimate="
        << unsignedJsonValue(batching, "frontierResidentQueryRoundTripsEstimate")
        << " frontier_resident_query_round_trip_savings_estimate="
        << unsignedJsonValue(batching, "frontierResidentQueryRoundTripSavingsEstimate")
        << " frontier_mixed_query_depths=" << unsignedJsonValue(batching, "frontierMixedQueryDepths")
        << " frontier_mixed_query_round_trips="
        << unsignedJsonValue(batching, "frontierMixedQueryRoundTrips")
        << " frontier_mixed_query_rays=" << unsignedJsonValue(batching, "frontierMixedQueryRays")
        << " frontier_mixed_query_closest_hit_rays="
        << unsignedJsonValue(batching, "frontierMixedQueryClosestHitRays")
        << " frontier_mixed_query_any_hit_rays="
        << unsignedJsonValue(batching, "frontierMixedQueryAnyHitRays")
        << " frontier_mixed_query_readback_bytes="
        << unsignedJsonValue(batching, "frontierMixedQueryReadbackBytes")
        << " frontier_mixed_query_closest_hit_readback_bytes="
        << unsignedJsonValue(batching, "frontierMixedQueryClosestHitReadbackBytes")
        << " frontier_mixed_query_any_hit_readback_bytes="
        << unsignedJsonValue(batching, "frontierMixedQueryAnyHitReadbackBytes")
        << " closest_hit_batch_preferred="
        << (batching.value("intersectionBackendPrefersClosestHitBatch").toBool() ? "true" : "false")
        << " any_hit_batch_preferred="
        << (batching.value("intersectionBackendPrefersAnyHitBatch").toBool() ? "true" : "false")
        << " resident_frontiers_supported="
        << (batching.value("intersectionBackendSupportsResidentFrontiers").toBool() ? "true"
                                                                                    : "false")
        << " gpu_frontier_compaction_supported="
        << (batching.value("intersectionBackendSupportsGpuFrontierCompaction").toBool() ? "true"
                                                                                        : "false")
        << " gpu_frontier_compaction_unavailable_reason="
        << compactSummaryText(
             batching.value("intersectionBackendGpuFrontierCompactionUnavailableReason"), "none")
        << " prepared_ray_batch_compaction_supported="
        << (batching.value("intersectionBackendSupportsPreparedRayBatchCompaction").toBool()
              ? "true"
              : "false")
        << " resident_direct_light_batches_supported="
        << (batching.value("intersectionBackendSupportsResidentDirectLightBatches").toBool()
              ? "true"
              : "false")
        << " resident_direct_light_batches_unavailable_reason="
        << compactSummaryText(
             batching.value("intersectionBackendResidentDirectLightBatchesUnavailableReason"),
             "none")
        << " resident_path_loop_execution="
        << compactSummaryText(batching.value("residentPathLoopExecutionPath"), "none")
        << " resident_path_loop_schedule="
        << compactSummaryText(batching.value("residentPathLoopSchedule"), "none")
        << " resident_path_loop_residency="
        << compactSummaryText(batching.value("residentPathLoopResidency"), "none")
        << " resident_path_loop_platform="
        << compactSummaryText(batching.value("residentPathLoopPlatformName"), "none")
        << " resident_path_loop_depths=" << unsignedJsonValue(batching, "residentPathLoopDepths")
        << " resident_path_loop_active_paths_per_depth="
        << unsignedArraySummary(batching.value("activePathsPerDepth").toArray())
        << " resident_path_loop_input_paths="
        << unsignedJsonValue(batching, "residentPathLoopInputPaths")
        << " resident_path_loop_retained_paths="
        << unsignedJsonValue(batching, "residentPathLoopRetainedPaths")
        << " resident_path_loop_removed_paths="
        << unsignedJsonValue(batching, "residentPathLoopRemovedPaths")
        << " resident_path_loop_moved_paths="
        << unsignedJsonValue(batching, "residentPathLoopMovedPaths")
        << " resident_path_loop_retained_index_bytes="
        << unsignedJsonValue(batching, "residentPathLoopRetainedIndexBytes")
        << " resident_path_loop_resident_path_state_bytes="
        << unsignedJsonValue(batching, "residentPathLoopResidentPathStateBytes")
        << " resident_path_loop_input_resident_path_state_bytes="
        << unsignedJsonValue(batching, "residentPathLoopInputResidentPathStateBytes")
        << " resident_path_loop_retained_resident_path_state_bytes="
        << unsignedJsonValue(batching, "residentPathLoopRetainedResidentPathStateBytes")
        << " resident_path_loop_removed_resident_path_state_bytes="
        << unsignedJsonValue(batching, "residentPathLoopRemovedResidentPathStateBytes")
        << " resident_path_loop_compaction_passes="
        << unsignedJsonValue(batching, "residentPathLoopCompactionPasses")
        << " resident_path_loop_round_trips="
        << unsignedJsonValue(batching, "residentPathLoopRoundTrips")
        << " resident_path_loop_submitted_intersection_rays="
        << unsignedJsonValue(batching, "residentPathLoopSubmittedIntersectionRays")
        << " resident_path_loop_full_platform_gpu_kernel="
        << (batching.value("residentPathLoopFullPlatformGpuKernel").toBool() ? "true" : "false")
        << " resident_path_loop_saved_host_readbacks="
        << unsignedJsonValue(batching, "residentPathLoopSavedHostReadbacks")
        << " resident_path_loop_saved_host_readback_bytes="
        << unsignedJsonValue(batching, "residentPathLoopSavedHostReadbackBytes")
        << " samples=" << unsignedJsonValue(input, "primarySamples")
        << " tiles=" << unsignedJsonValue(tiling, "tileCount")
        << " tile_grid=" << unsignedJsonValue(tiling, "tileColumns") << "x"
        << unsignedJsonValue(tiling, "tileRows")
        << " max_tile_width=" << unsignedJsonValue(tiling, "maxTileWidth")
        << " max_tile_height=" << unsignedJsonValue(tiling, "maxTileHeight")
        << " max_tile_pixels=" << unsignedJsonValue(tiling, "maxTilePixels")
        << " avg_tile_pixels=" << tiling.value("averageTilePixels").toDouble()
        << " nonempty_tiles=" << unsignedJsonValue(tiling, "nonEmptyTileCount")
        << " min_tile_samples=" << unsignedJsonValue(tiling, "minNonEmptyTileSamples")
        << " avg_tile_samples=" << tiling.value("averageNonEmptyTileSamples").toDouble()
        << " max_tile_samples=" << unsignedJsonValue(tiling, "maxTileSamples")
        << " accumulation_backend=" << compactSummaryText(accumulation.value("backend"), "none")
        << " accumulation_residency=" << compactSummaryText(accumulation.value("residency"), "none")
        << " accumulation_resident_bytes=" << unsignedJsonValue(accumulation, "residentBytes")
        << " accumulation_color_sum_bytes=" << unsignedJsonValue(accumulationLayout, "colorSumBytes")
        << " accumulation_sample_count_bytes="
        << unsignedJsonValue(accumulationLayout, "sampleCountBytes")
        << " accumulation_moment_bytes=" << unsignedJsonValue(accumulationLayout, "momentBytes")
        << " accumulation_resolve_bytes=" << unsignedJsonValue(accumulationLayout, "resolveBytes")
        << " accumulation_clear_ops=" << unsignedJsonValue(accumulation, "clearOperations")
        << " accumulation_add_ops=" << unsignedJsonValue(accumulation, "addOperations")
        << " accumulation_added_samples=" << unsignedJsonValue(accumulation, "addedSamples")
        << " accumulation_resolve_ops=" << unsignedJsonValue(accumulation, "resolveOperations")
        << " accumulation_readback_ops=" << unsignedJsonValue(accumulation, "readbackOperations")
        << " accumulation_readback_bytes=" << unsignedJsonValue(accumulation, "readbackBytes")
        << " active_sample_depths=" << unsignedJsonValue(batching, "activeSampleDepthsProcessed")
        << " active_host_path_state_bytes="
        << unsignedJsonValue(batching, "activeHostPathStateBytesProcessed")
        << " active_hit_host_bytes=" << unsignedJsonValue(batching, "activeHitHostBytesProcessed")
        << " spawned_continuations=" << unsignedJsonValue(batching, "spawnedContinuationSamples")
        << " spawned_continuation_host_path_state_bytes="
        << unsignedJsonValue(batching, "spawnedContinuationHostPathStateBytes")
        << " frontier_compaction_execution="
        << compactSummaryText(batching.value("frontierCompactionExecutionPath"), "none")
        << " frontier_compaction_path_state_residency="
        << compactSummaryText(batching.value("frontierCompactionPathStateResidency"), "none")
        << " frontier_compaction_passes=" << unsignedJsonValue(batching, "frontierCompactionPasses")
        << " frontier_compaction_input_samples="
        << unsignedJsonValue(batching, "frontierCompactionInputSamples")
        << " frontier_compaction_retained_samples="
        << unsignedJsonValue(batching, "frontierCompactionRetainedSamples")
        << " frontier_compaction_removed_samples="
        << unsignedJsonValue(batching, "frontierCompactionRemovedSamples")
        << " frontier_compaction_removed_fraction="
        << batching.value("frontierCompactionRemovedSampleFraction").toDouble()
        << " frontier_compaction_moved_samples="
        << unsignedJsonValue(batching, "frontierCompactionMovedSamples")
        << " frontier_compaction_moved_retained_fraction="
        << batching.value("frontierCompactionMovedRetainedSampleFraction").toDouble()
        << " frontier_compaction_retained_index_bytes="
        << unsignedJsonValue(batching, "frontierCompactionRetainedIndexBytes")
        << " frontier_compaction_input_host_path_state_bytes="
        << unsignedJsonValue(batching, "frontierCompactionInputHostPathStateBytes")
        << " frontier_compaction_retained_host_path_state_bytes="
        << unsignedJsonValue(batching, "frontierCompactionRetainedHostPathStateBytes")
        << " frontier_compaction_removed_host_path_state_bytes="
        << unsignedJsonValue(batching, "frontierCompactionRemovedHostPathStateBytes")
        << " frontier_compaction_upload_ms="
        << batching.value("frontierCompactionUploadWorkerSeconds").toDouble() * 1000.0
        << " frontier_compaction_kernel_ms="
        << batching.value("frontierCompactionKernelWorkerSeconds").toDouble() * 1000.0
        << " frontier_compaction_readback_ms="
        << batching.value("frontierCompactionReadbackWorkerSeconds").toDouble() * 1000.0
        << " batches=" << unsignedJsonValue(batching, "batches")
        << " avg_batch=" << batching.value("averageBatchSize").toDouble()
        << " max_batch=" << unsignedJsonValue(batching, "maxBatchSize")
        << " active_depths=" << activeSamples.size()
        << " last_active=" << unsignedArrayBack(activeSamples)
        << " last_retained_active=" << unsignedArrayBack(retainedActiveSamples)
        << " last_active_host_path_state_bytes=" << unsignedArrayBack(activeHostPathStateBytes)
        << " last_active_hit_host_bytes=" << unsignedArrayBack(activeHitHostBytes)
        << " last_retained_host_path_state_bytes=" << unsignedArrayBack(retainedHostPathStateBytes)
        << " last_spawned_continuations=" << unsignedArrayBack(spawnedContinuations)
        << " last_spawned_continuation_host_path_state_bytes="
        << unsignedArrayBack(spawnedContinuationHostPathStateBytes)
        << " frontier_compaction_candidate_depths="
        << unsignedJsonValue(batching, "frontierCompactionCandidateDepths")
        << " frontier_compaction_candidate_samples="
        << unsignedJsonValue(batching, "frontierCompactionCandidateSamples")
        << " frontier_compaction_candidate_packed_ray_bytes="
        << unsignedJsonValue(batching, "frontierCompactionCandidatePackedRayBytes")
        << " frontier_compaction_candidate_state_handle_bytes="
        << unsignedJsonValue(batching, "frontierCompactionCandidateStateHandleBytes")
        << " frontier_compaction_candidate_host_path_state_bytes="
        << unsignedJsonValue(batching, "frontierCompactionCandidateHostPathStateBytes")
        << " frontier_compaction_candidate_fraction="
        << batching.value("frontierCompactionCandidateSampleFraction").toDouble()
        << " frontier_largest_compaction_candidate_depth="
        << unsignedJsonValue(batching, "frontierLargestCompactionCandidateDepth")
        << " frontier_largest_compaction_candidate_samples="
        << unsignedJsonValue(batching, "frontierLargestCompactionCandidateSamples")
        << " frontier_largest_compaction_candidate_packed_ray_bytes="
        << unsignedJsonValue(batching, "frontierLargestCompactionCandidatePackedRayBytes")
        << " frontier_largest_compaction_candidate_state_handle_bytes="
        << unsignedJsonValue(batching, "frontierLargestCompactionCandidateStateHandleBytes")
        << " frontier_largest_compaction_candidate_host_path_state_bytes="
        << unsignedJsonValue(batching, "frontierLargestCompactionCandidateHostPathStateBytes")
        << " frontier_largest_compaction_candidate_fraction="
        << batching.value("frontierLargestCompactionCandidateSampleFraction").toDouble()
        << " frontier_hit_rays=" << unsignedArraySum(frontierHits)
        << " frontier_miss_rays=" << unsignedArraySum(frontierMisses)
        << " frontier_packet_chunks=" << unsignedArraySum(frontierPackets)
        << " frontier_packet_rays=" << frontierPacketRayCount
        << " frontier_closest_hit_batch_chunks=" << frontierClosestHitBatchChunkCount
        << " frontier_closest_hit_batch_rays=" << frontierClosestHitBatchRayCount
        << " frontier_closest_hit_batch_avg="
        << ratio(static_cast<double>(frontierClosestHitBatchRayCount),
                 static_cast<double>(frontierClosestHitBatchChunkCount))
        << " direct_light_any_hit_batch_chunks=" << directLightAnyHitBatchChunkCount
        << " direct_light_any_hit_batch_rays=" << directLightAnyHitBatchRayCount
        << " direct_light_any_hit_batch_avg="
        << ratio(static_cast<double>(directLightAnyHitBatchRayCount),
                 static_cast<double>(directLightAnyHitBatchChunkCount))
        << " direct_light_any_hit_chunks=" << directLightAnyHitBatchChunkCount
        << " direct_light_any_hit_chunk_rays=" << directLightAnyHitBatchRayCount
        << " direct_light_any_hit_chunk_avg="
        << ratio(static_cast<double>(directLightAnyHitBatchRayCount),
                 static_cast<double>(directLightAnyHitBatchChunkCount))
        << " direct_light_any_hit_round_trips="
        << unsignedJsonValue(batching, "directLightAnyHitQueryRoundTrips")
        << " resident_direct_light_round_trips_estimate="
        << unsignedJsonValue(batching, "residentDirectLightBatchRoundTripsEstimate")
        << " resident_direct_light_round_trip_savings_estimate="
        << unsignedJsonValue(batching, "residentDirectLightBatchRoundTripSavingsEstimate")
        << " resident_direct_light_candidate_depths="
        << unsignedJsonValue(batching, "residentDirectLightBatchCandidateDepths")
        << " resident_direct_light_candidate_rays="
        << unsignedJsonValue(batching, "residentDirectLightBatchCandidateRays")
        << " resident_direct_light_candidate_host_bytes="
        << unsignedJsonValue(batching, "residentDirectLightBatchCandidateHostBytes")
        << " resident_largest_direct_light_depth="
        << unsignedJsonValue(batching, "residentLargestDirectLightBatchDepth")
        << " resident_largest_direct_light_rays="
        << unsignedJsonValue(batching, "residentLargestDirectLightBatchRays")
        << " resident_largest_direct_light_packed_ray_bytes="
        << unsignedJsonValue(batching, "residentLargestDirectLightBatchPackedRayBytes")
        << " resident_largest_direct_light_host_bytes="
        << unsignedJsonValue(batching, "residentLargestDirectLightBatchHostBytes")
        << " direct_light_selection_host_bytes="
        << unsignedJsonValue(batching, "directLightSelectionHostBytes")
        << " direct_light_occlusion_host_bytes="
        << unsignedJsonValue(batching, "directLightOcclusionHostBytes")
        << " direct_light_contribution_host_bytes="
        << unsignedJsonValue(batching, "directLightContributionHostBytes")
        << " direct_light_contribution_execution="
        << compactSummaryText(batching.value("directLightContributionExecutionPath"), "none")
        << " direct_light_contribution_fallback="
        << compactSummaryText(batching.value("directLightContributionFallbackReason"), "none")
        << " direct_light_any_hit_frontier_packed_ray_bytes="
        << unsignedJsonValue(batching, "directLightAnyHitFrontierPackedRayBytes")
        << " direct_light_any_hit_frontier_host_packed_ray_bytes="
        << unsignedJsonValue(batching, "directLightAnyHitFrontierHostPackedRayBytes")
        << " direct_light_any_hit_frontier_host_query_bytes="
        << unsignedJsonValue(batching, "directLightAnyHitFrontierHostQueryBytes")
        << " direct_light_any_hit_frontier_state_handle_bytes="
        << unsignedJsonValue(batching, "directLightAnyHitFrontierStateHandleBytes")
        << " last_direct_light_any_hit_frontier_packed_ray_bytes="
        << unsignedArrayBack(directLightAnyHitFrontierPackedRayBytes)
        << " last_direct_light_any_hit_frontier_host_packed_ray_bytes="
        << unsignedArrayBack(directLightAnyHitFrontierHostPackedRayBytes)
        << " last_direct_light_any_hit_frontier_host_query_bytes="
        << unsignedArrayBack(directLightAnyHitFrontierHostQueryBytes)
        << " last_direct_light_any_hit_frontier_state_handle_bytes="
        << unsignedArrayBack(directLightAnyHitFrontierStateHandleBytes)
        << " last_direct_light_selection_host_bytes="
        << unsignedArrayBack(directLightSelectionHostBytes)
        << " last_direct_light_occlusion_host_bytes="
        << unsignedArrayBack(directLightOcclusionHostBytes)
        << " last_direct_light_contribution_host_bytes="
        << unsignedArrayBack(directLightContributionHostBytes)
        << " frontier_ray4_packet_chunks=" << frontierRay4PacketChunkCount
        << " frontier_ray8_packet_chunks=" << frontierRay8PacketChunkCount
        << " frontier_packet_fill="
        << ratio(static_cast<double>(frontierPacketRayCount), frontierPacketLaneCapacity)
        << " frontier_scalar_tail_fraction="
        << ratio(static_cast<double>(frontierScalarRayCount),
                 static_cast<double>(frontierPacketRayCount + frontierScalarRayCount))
        << " frontier_scalar_rays=" << frontierScalarRayCount
        << " frontier_packet_scalar_fallback_rays=" << frontierPacketScalarFallbackRayCount
        << " frontier_packet_scalar_fallback_fraction="
        << ratio(static_cast<double>(frontierPacketScalarFallbackRayCount),
                 static_cast<double>(frontierPacketRayCount))
        << " frontier_packet_scalar_fallback_by_reason="
        << compactUnsignedObjectPairs(frontierPacketScalarFallbackByReason)
        << " frontier_packet_refined_rays=" << unsignedArraySum(frontierPacketRefinedRays)
        << " frontier_packet_refined_by_material="
        << compactUnsignedObjectPairs(frontierPacketRefinedByMaterial)
        << " sample_variance_pixels=" << unsignedJsonValue(batching, "sampleVariancePixelArea")
        << " sample_stddev_rms=" << batching.value("sampleRadianceStddevRms").toDouble()
        << " max_sample_stddev=" << batching.value("maxSampleRadianceStddev").toDouble()
        << " emitter_hit_samples=" << unsignedJsonValue(batching, "emitterHitSamples")
        << " primary_emitter_hit_samples=" << unsignedJsonValue(batching, "primaryEmitterHitSamples")
        << " delta_emitter_hit_samples=" << unsignedJsonValue(batching, "deltaEmitterHitSamples")
        << " bsdf_emitter_hit_samples=" << unsignedJsonValue(batching, "bsdfEmitterHitSamples")
        << " mis_weighted_emitter_hit_samples="
        << unsignedJsonValue(batching, "misWeightedEmitterHitSamples")
        << " direct_light_samples=" << unsignedJsonValue(batching, "directLightSamples")
        << " gpu_primary_sample_chunk_size="
        << unsignedJsonValue(batching, "residentPathLoopPrimarySampleChunkSize")
        << " direct_light_contributing_samples="
        << unsignedJsonValue(batching, "directLightContributingSamples")
        << " direct_light_occluded_samples="
        << unsignedJsonValue(batching, "directLightOccludedSamples")
        << " emitted_luminance=" << batching.value("emittedRadianceLuminanceSum").toDouble()
        << " direct_light_luminance="
        << batching.value("directLightRadianceLuminanceSum").toDouble()
        << " primary_direct_light_luminance="
        << batching.value("primaryDirectLightRadianceLuminanceSum").toDouble()
        << " secondary_direct_light_luminance="
        << batching.value("secondaryDirectLightRadianceLuminanceSum").toDouble()
        << " ambient_luminance=" << batching.value("ambientRadianceLuminanceSum").toDouble()
        << " miss_luminance=" << batching.value("missRadianceLuminanceSum").toDouble()
        << " compatibility_shade_luminance="
        << batching.value("compatibilityShadeRadianceLuminanceSum").toDouble()
        << " adaptive=" << (adaptiveSampling.value("enabled").toBool() ? "enabled" : "disabled")
        << " adaptive_min_samples=" << unsignedJsonValue(adaptiveSampling, "minimumSamples")
        << " adaptive_stddev_threshold=" << adaptiveSampling.value("stddevThreshold").toDouble()
        << " adaptive_max_samples=" << unsignedJsonValue(adaptiveSampling, "maximumPrimarySamples")
        << " adaptive_skipped_samples=" << unsignedJsonValue(adaptiveSampling, "skippedPrimarySamples")
        << " adaptive_skipped_fraction="
        << adaptiveSampling.value("skippedPrimarySampleFraction").toDouble()
        << " last_rms_delta=" << doubleArrayBack(rmsDelta)
        << " compatibility_shade_samples=" << unsignedJsonValue(batching, "compatibilityShadeSamples")
        << " unsupported_path_material_samples="
        << unsignedJsonValue(batching, "unsupportedPathMaterialSamples")
        << " convergence=" << convergence.value("decision").toString().toStdString()
        << " stopped_tiles=" << unsignedJsonValue(convergence, "stoppedTileCount")
        << " earliest_stop_depth=" << unsignedJsonValue(convergence, "earliestStoppedAfterDepth")
        << " latest_stop_depth=" << unsignedJsonValue(convergence, "latestStoppedAfterDepth")
        << " feedback_depths=" << unsignedJsonValue(convergence, "feedbackDepthCount") << " denoiser="
        << (denoise.value("enabled").toBool() ? denoise.value("denoiser").toString().toStdString()
                                              : std::string("none"))
        << " denoise_ms=" << denoise.value("seconds").toDouble() * 1000.0
        << " denoise_feature_prepass_ms=" << denoise.value("featureSeconds").toDouble() * 1000.0;
      const QJsonObject denoiseParameters = denoise.value("parameters").toObject();
      for (auto it = denoiseParameters.begin(); it != denoiseParameters.end(); ++it) {
        std::cout << " denoise_" << it.key().toStdString() << "=" << it.value().toDouble();
      }
      const QJsonObject denoiseFeatures = denoise.value("features").toObject();
      for (auto it = denoiseFeatures.begin(); it != denoiseFeatures.end(); ++it) {
        std::cout << " denoise_feature_" << it.key().toStdString() << "="
                  << (it.value().toBool() ? 1 : 0);
      }
      std::cout << '\n';
    }

  private:
    std::uint64_t unsignedArrayBack(const QJsonArray& array) const {
      if (array.isEmpty()) {
        return 0;
      }
      return static_cast<std::uint64_t>(array.at(array.size() - 1).toDouble());
    }

    std::uint64_t unsignedArraySum(const QJsonArray& array) const {
      std::uint64_t result = 0;
      for (const QJsonValue& value : array) {
        result += static_cast<std::uint64_t>(value.toDouble());
      }
      return result;
    }

    std::string unsignedArraySummary(const QJsonArray& array) const {
      if (array.isEmpty()) {
        return "none";
      }
      std::string result;
      for (int index = 0; index != array.size(); ++index) {
        if (!result.empty()) {
          result += ",";
        }
        result += std::to_string(static_cast<std::uint64_t>(array.at(index).toDouble()));
      }
      return result;
    }

    double ratio(double numerator, double denominator) const {
      return denominator == 0.0 ? 0.0 : numerator / denominator;
    }

    std::string fallbackCapabilitySummary(const QJsonArray& capabilities) const {
      std::vector<std::string> names;
      names.reserve(static_cast<std::size_t>(capabilities.size()));
      for (const QJsonValue& value : capabilities) {
        const QJsonObject capability = value.toObject();
        const QJsonObject fallback = capability.value("fallback").toObject();
        const bool activeFallback = fallback.value("active").toBool();
        if (capability.value("support").toString() != "fallback" && !activeFallback) {
          continue;
        }

        const QString fallbackRequested = fallback.value("requestedDevice").toString();
        const QString fallbackResolved = fallback.value("resolvedDevice").toString();
        const QString fallbackReason = fallback.value("reason").toString();
        std::string summary =
          compactSummaryToken(capability.value("name").toString(QStringLiteral("unknown")).toStdString());
        summary += "=";
        summary +=
          compactSummaryToken((fallbackRequested.isEmpty() ? capability.value("requestedDevice").toString()
                                                    : fallbackRequested)
                         .toStdString());
        summary += "->";
        summary +=
          compactSummaryToken((fallbackResolved.isEmpty() ? capability.value("resolvedDevice").toString()
                                                   : fallbackResolved)
                         .toStdString());

        const QString reason = fallbackReason.isEmpty()
                                 ? capability.value("unsupportedReason").toString()
                                 : fallbackReason;
        if (!reason.isEmpty()) {
          summary += ":";
          summary += compactSummaryToken(reason.toStdString());
        }
        names.push_back(summary);
      }

      if (names.empty()) {
        return "none";
      }
      std::sort(names.begin(), names.end());
      std::string result = std::to_string(names.size()) + ":" + names.front();
      for (std::size_t index = 1; index != names.size(); ++index) {
        result += ",";
        result += names[index];
      }
      return result;
    }

    std::string restrictedCapabilitySummary(const QJsonArray& capabilities) const {
      std::vector<std::string> names;
      names.reserve(static_cast<std::size_t>(capabilities.size()));
      for (const QJsonValue& value : capabilities) {
        const QJsonObject capability = value.toObject();
        if (capability.value("support").toString() != "restricted") {
          continue;
        }

        std::string summary =
          compactSummaryToken(capability.value("name").toString(QStringLiteral("unknown")).toStdString());
        summary += "=";
        summary += compactSummaryToken(
          capability.value("resolvedDevice").toString(QStringLiteral("unknown")).toStdString());
        const QString executionPath = capability.value("executionPath").toString();
        if (!executionPath.isEmpty()) {
          summary += ":";
          summary += compactSummaryToken(executionPath.toStdString());
        }
        const QString reason = capability.value("unsupportedReason").toString();
        if (!reason.isEmpty()) {
          summary += ":";
          summary += compactSummaryToken(reason.toStdString());
        }
        names.push_back(summary);
      }

      if (names.empty()) {
        return "none";
      }
      std::sort(names.begin(), names.end());
      std::string result = std::to_string(names.size()) + ":" + names.front();
      for (std::size_t index = 1; index != names.size(); ++index) {
        result += ",";
        result += names[index];
      }
      return result;
    }

    std::string tracingBackendFallbackSummary(const QJsonObject& batching,
                                              const QJsonObject& tracingExecution) const {
      if (batching.value("executionMode").toString() ==
          QStringLiteral("compiled_diffuse_path_loop")) {
        const std::string fallbackReason =
          compactSummaryText(tracingExecution.value("fallbackReason"), "");
        if (!fallbackReason.empty()) {
          return fallbackReason;
        }
        const std::string actualFallbackReason =
          compactSummaryText(tracingExecution.value("actualFallbackReason"), "");
        if (!actualFallbackReason.empty()) {
          return actualFallbackReason;
        }
      }
      return compactSummaryText(batching.value("tracingBackendFallback").toObject().value("reason"),
                              "none");
    }

    std::string compactValue(const QJsonValue& value, const std::string& empty) const {
      if (value.isUndefined() || value.isNull()) {
        return empty;
      }
      if (value.isDouble()) {
        return std::to_string(static_cast<std::uint64_t>(value.toDouble()));
      }
      return compactSummaryText(value, empty);
    }

    double doubleArrayBack(const QJsonArray& array) const {
      return array.isEmpty() ? 0.0 : array.at(array.size() - 1).toDouble();
    }
  };

  QString normalizedRasterOption(QString value) {
    value = value.trimmed().toLower();
    value.remove('_');
    value.remove('-');
    value.remove(',');
    value.remove(' ');
    return value;
  }

  bool parseColorWriteMask(const QString& value, std::uint8_t* mask) {
    const QString normalized = normalizedRasterOption(value);
    if (normalized == "none" || normalized == "0") {
      *mask = 0;
      return true;
    }
    if (normalized == "all") {
      *mask = engine::raster::Rasterizer::ColorWriteAll;
      return true;
    }
    if (normalized.isEmpty()) {
      return false;
    }

    std::uint8_t parsed = 0;
    for (const QChar ch : normalized) {
      if (ch == 'r') {
        parsed |= engine::raster::Rasterizer::ColorWriteRed;
      } else if (ch == 'g') {
        parsed |= engine::raster::Rasterizer::ColorWriteGreen;
      } else if (ch == 'b') {
        parsed |= engine::raster::Rasterizer::ColorWriteBlue;
      } else {
        return false;
      }
    }
    *mask = parsed;
    return true;
  }

  bool parseBlendFactor(const QString& value, engine::raster::Rasterizer::BlendFactor* factor) {
    const QString normalized = normalizedRasterOption(value);
    using BlendFactor = engine::raster::Rasterizer::BlendFactor;
    if (normalized == "zero") {
      *factor = BlendFactor::Zero;
    } else if (normalized == "one") {
      *factor = BlendFactor::One;
    } else if (normalized == "sourcecolor" || normalized == "srccolor") {
      *factor = BlendFactor::SourceColor;
    } else if (normalized == "oneminussourcecolor" || normalized == "1minussourcecolor" ||
               normalized == "1srccolor") {
      *factor = BlendFactor::OneMinusSourceColor;
    } else if (normalized == "sourcealpha" || normalized == "srcalpha") {
      *factor = BlendFactor::SourceAlpha;
    } else if (normalized == "oneminussourcealpha" || normalized == "1minussourcealpha" ||
               normalized == "1srcalpha") {
      *factor = BlendFactor::OneMinusSourceAlpha;
    } else if (normalized == "destinationcolor" || normalized == "dstcolor") {
      *factor = BlendFactor::DestinationColor;
    } else if (normalized == "oneminusdestinationcolor" || normalized == "1minusdestinationcolor" ||
               normalized == "1dstcolor") {
      *factor = BlendFactor::OneMinusDestinationColor;
    } else if (normalized == "constantcolor" || normalized == "constcolor") {
      *factor = BlendFactor::ConstantColor;
    } else if (normalized == "oneminusconstantcolor" || normalized == "1minusconstantcolor" ||
               normalized == "1constcolor") {
      *factor = BlendFactor::OneMinusConstantColor;
    } else if (normalized == "constantalpha" || normalized == "constalpha") {
      *factor = BlendFactor::ConstantAlpha;
    } else if (normalized == "oneminusconstantalpha" || normalized == "1minusconstantalpha" ||
               normalized == "1constalpha") {
      *factor = BlendFactor::OneMinusConstantAlpha;
    } else {
      return false;
    }
    return true;
  }

  bool parseAlphaFunc(const QString& value, engine::raster::Rasterizer::AlphaFunc* func) {
    const QString normalized = normalizedRasterOption(value);
    using AlphaFunc = engine::raster::Rasterizer::AlphaFunc;
    if (normalized == "never") {
      *func = AlphaFunc::Never;
    } else if (normalized == "less") {
      *func = AlphaFunc::Less;
    } else if (normalized == "equal") {
      *func = AlphaFunc::Equal;
    } else if (normalized == "lessequal") {
      *func = AlphaFunc::LessEqual;
    } else if (normalized == "greater") {
      *func = AlphaFunc::Greater;
    } else if (normalized == "greaterequal") {
      *func = AlphaFunc::GreaterEqual;
    } else if (normalized == "notequal") {
      *func = AlphaFunc::NotEqual;
    } else if (normalized == "always") {
      *func = AlphaFunc::Always;
    } else {
      return false;
    }
    return true;
  }

  bool parseBlendOp(const QString& value, engine::raster::Rasterizer::BlendOp* op) {
    const QString normalized = normalizedRasterOption(value);
    using BlendOp = engine::raster::Rasterizer::BlendOp;
    if (normalized == "add") {
      *op = BlendOp::Add;
    } else if (normalized == "subtract" || normalized == "sub") {
      *op = BlendOp::Subtract;
    } else if (normalized == "reversesubtract" || normalized == "revsub") {
      *op = BlendOp::ReverseSubtract;
    } else if (normalized == "min") {
      *op = BlendOp::Min;
    } else if (normalized == "max") {
      *op = BlendOp::Max;
    } else {
      return false;
    }
    return true;
  }

  template<class T>
  std::string rasterEnumName(T value, std::initializer_list<std::pair<T, const char*>> names,
                             const char* fallback) {
    for (const auto& [candidate, name] : names) {
      if (value == candidate)
        return name;
    }
    return fallback;
  }

  std::string blendFactorName(engine::raster::Rasterizer::BlendFactor factor) {
    using BlendFactor = engine::raster::Rasterizer::BlendFactor;
    return rasterEnumName<BlendFactor>(
      factor,
      {{BlendFactor::Zero, "zero"},
       {BlendFactor::One, "one"},
       {BlendFactor::SourceColor, "source_color"},
       {BlendFactor::OneMinusSourceColor, "one_minus_source_color"},
       {BlendFactor::SourceAlpha, "source_alpha"},
       {BlendFactor::OneMinusSourceAlpha, "one_minus_source_alpha"},
       {BlendFactor::DestinationColor, "destination_color"},
       {BlendFactor::OneMinusDestinationColor, "one_minus_destination_color"},
       {BlendFactor::ConstantColor, "constant_color"},
       {BlendFactor::OneMinusConstantColor, "one_minus_constant_color"},
       {BlendFactor::ConstantAlpha, "constant_alpha"},
       {BlendFactor::OneMinusConstantAlpha, "one_minus_constant_alpha"}},
      "one");
  }

  std::string blendOpName(engine::raster::Rasterizer::BlendOp op) {
    using BlendOp = engine::raster::Rasterizer::BlendOp;
    return rasterEnumName<BlendOp>(op,
                                   {{BlendOp::Add, "add"},
                                    {BlendOp::Subtract, "subtract"},
                                    {BlendOp::ReverseSubtract, "reverse_subtract"},
                                    {BlendOp::Min, "min"},
                                    {BlendOp::Max, "max"}},
                                   "add");
  }

  std::string alphaFuncName(engine::raster::Rasterizer::AlphaFunc func) {
    using AlphaFunc = engine::raster::Rasterizer::AlphaFunc;
    return rasterEnumName<AlphaFunc>(func,
                                     {{AlphaFunc::Never, "never"},
                                      {AlphaFunc::Less, "less"},
                                      {AlphaFunc::Equal, "equal"},
                                      {AlphaFunc::LessEqual, "less_equal"},
                                      {AlphaFunc::Greater, "greater"},
                                      {AlphaFunc::GreaterEqual, "greater_equal"},
                                      {AlphaFunc::NotEqual, "not_equal"},
                                      {AlphaFunc::Always, "always"}},
                                     "always");
  }

  bool parseColorTriplet(const QString& value, Colord* color) {
    const QStringList parts = value.split(',', Qt::KeepEmptyParts);
    if (parts.size() != 3) {
      return false;
    }

    double components[3];
    for (int i = 0; i < 3; ++i) {
      bool ok = false;
      components[i] = parts[i].trimmed().toDouble(&ok);
      if (!ok || !std::isfinite(components[i]) || components[i] < 0.0 || components[i] > 1.0) {
        return false;
      }
    }

    *color = Colord(components);
    return true;
  }

  bool parseRasterRect(const QString& value, Recti* rect) {
    const QStringList parts = value.split(',', Qt::KeepEmptyParts);
    if (parts.size() != 4) {
      return false;
    }

    int components[4];
    for (int i = 0; i < 4; ++i) {
      bool ok = false;
      components[i] = parts[i].trimmed().toInt(&ok);
      if (!ok) {
        return false;
      }
    }

    const Recti parsedRect(components);
    if (!parsedRect.hasNonNegativeSize()) {
      return false;
    }

    *rect = parsedRect;
    return true;
  }

  QStringList splitOptionValues(const QStringList& values) {
    QStringList result;
    for (const QString& value : values) {
      const QStringList parts = value.split(',', Qt::SkipEmptyParts);
      for (const QString& part : parts) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty()) {
          result.push_back(trimmed);
        }
      }
    }
    return result;
  }

  bool parseRenderPassKind(const QString& value, engine::graph::RenderPassKind* kind) {
    const QString normalized = normalizedRasterOption(value);
    using RenderPassKind = engine::graph::RenderPassKind;
    if (normalized == "beauty") {
      *kind = RenderPassKind::Beauty;
    } else if (normalized == "shadow") {
      *kind = RenderPassKind::Shadow;
    } else if (normalized == "overlay") {
      *kind = RenderPassKind::Overlay;
    } else if (normalized == "composite") {
      *kind = RenderPassKind::Composite;
    } else if (normalized == "tonemap") {
      *kind = RenderPassKind::Tonemap;
    } else if (normalized == "postprocess") {
      *kind = RenderPassKind::PostProcess;
    } else if (normalized == "visibility") {
      *kind = RenderPassKind::Visibility;
    } else if (normalized == "aov") {
      *kind = RenderPassKind::AOV;
    } else if (normalized == "debug") {
      *kind = RenderPassKind::Debug;
    } else if (normalized == "custom") {
      *kind = RenderPassKind::Custom;
    } else {
      return false;
    }
    return true;
  }

  bool parseRenderExecutorKind(const QString& value, engine::graph::RenderExecutorKind* executor) {
    const QString normalized = normalizedRasterOption(value);
    using RenderExecutorKind = engine::graph::RenderExecutorKind;
    if (normalized == "raytracer" || normalized == "raytrace") {
      *executor = RenderExecutorKind::Raytracer;
    } else if (normalized == "wavefront") {
      *executor = RenderExecutorKind::Wavefront;
    } else if (normalized == "rasterizer" || normalized == "raster") {
      *executor = RenderExecutorKind::Rasterizer;
    } else if (normalized == "wireframe") {
      *executor = RenderExecutorKind::Wireframe;
    } else if (normalized == "composite") {
      *executor = RenderExecutorKind::Composite;
    } else if (normalized == "postprocess") {
      *executor = RenderExecutorKind::PostProcess;
    } else {
      return false;
    }
    return true;
  }

  bool parseRenderExecutorPreference(const QString& value,
                                     engine::graph::RenderExecutorPreference* executor) {
    const QString normalized = normalizedRasterOption(value);
    using RenderExecutorPreference = engine::graph::RenderExecutorPreference;
    if (normalized == "raytracer" || normalized == "raytrace") {
      *executor = RenderExecutorPreference::Raytracer;
    } else if (normalized == "pathtracer" || normalized == "pt") {
      *executor = RenderExecutorPreference::PathTracer;
    } else if (normalized == "wavefront") {
      *executor = RenderExecutorPreference::Wavefront;
    } else if (normalized == "rasterizer" || normalized == "raster") {
      *executor = RenderExecutorPreference::Rasterizer;
    } else if (normalized == "wireframe") {
      *executor = RenderExecutorPreference::Wireframe;
    } else {
      return false;
    }
    return true;
  }

  bool parseImplementedRenderViewMode(const QString& value,
                                      engine::graph::RenderViewMode* viewMode) {
    const QString normalized = normalizedRasterOption(value);
    using RenderViewMode = engine::graph::RenderViewMode;
    if (normalized == "default") {
      *viewMode = RenderViewMode::Default;
    } else if (normalized == "beauty") {
      *viewMode = RenderViewMode::Beauty;
    } else if (normalized == "wireframe") {
      *viewMode = RenderViewMode::Wireframe;
    } else if (normalized == "depth") {
      *viewMode = RenderViewMode::Depth;
    } else if (normalized == "stencil") {
      *viewMode = RenderViewMode::Stencil;
    } else if (normalized == "stencilcomposite") {
      *viewMode = RenderViewMode::StencilComposite;
    } else if (normalized == "normal") {
      *viewMode = RenderViewMode::Normal;
    } else if (normalized == "objectid") {
      *viewMode = RenderViewMode::ObjectId;
    } else if (normalized == "materialid") {
      *viewMode = RenderViewMode::MaterialId;
    } else if (normalized == "worldposition") {
      *viewMode = RenderViewMode::WorldPosition;
    } else if (normalized == "samplestddev" || normalized == "sampleradiancestddev") {
      *viewMode = RenderViewMode::SampleStddev;
    } else if (normalized == "samplestddevcolor" || normalized == "samplecolorstddev" ||
               normalized == "sampleradiancestddevcolor") {
      *viewMode = RenderViewMode::SampleStddevColor;
    } else if (normalized == "rastercoveragecount") {
      *viewMode = RenderViewMode::RasterCoverageCount;
    } else if (normalized == "rasterdepthtestcount") {
      *viewMode = RenderViewMode::RasterDepthTestCount;
    } else if (normalized == "rasterdepthpasscount") {
      *viewMode = RenderViewMode::RasterDepthPassCount;
    } else if (normalized == "rastershadecount") {
      *viewMode = RenderViewMode::RasterShadeCount;
    } else if (normalized == "rastercolorwritecount") {
      *viewMode = RenderViewMode::RasterColorWriteCount;
    } else if (normalized == "hybridvisibility") {
      *viewMode = RenderViewMode::HybridVisibility;
    } else {
      return false;
    }
    return true;
  }

  struct RenderGraphAOVOutput {
    engine::graph::RenderViewMode viewMode;
    QString output;
  };

  enum class CommandLineStepMode {
    Single,
    Cumulative,
    Sequence,
  };

  struct CommandLineStepSelection {
    CommandLineStepMode mode = CommandLineStepMode::Single;
    int step = 0;
    bool rangeSet = false;
    int firstStep = 0;
    int lastStep = 0;
  };

  struct RenderGraphViewOverrideInput {
    engine::graph::RenderViewOverride viewOverride;

    bool parse(const QString& value, QString* errorMessage) {
      const QStringList parts = value.split(',', Qt::KeepEmptyParts);
      if (parts.size() < 2) {
        *errorMessage = "Render graph view override must use selector,key=value syntax";
        return false;
      }

      if (!parseSelector(parts.front().trimmed(), errorMessage)) {
        return false;
      }

      bool hasField = false;
      for (int i = 1; i != parts.size(); ++i) {
        const QString field = parts.at(i).trimmed();
        if (field.isEmpty()) {
          *errorMessage = "Render graph view override fields must not be empty";
          return false;
        }
        if (!parseField(field, errorMessage)) {
          return false;
        }
        hasField = true;
      }

      if (!hasField) {
        *errorMessage = "Render graph view override must set at least one field";
        return false;
      }
      return true;
    }

  private:
    bool parseSelector(const QString& text, QString* errorMessage) {
      const int separator = text.indexOf(':');
      const QString kind = separator < 0 ? text.trimmed() : text.left(separator).trimmed();
      const QString value = separator < 0 ? QString() : text.mid(separator + 1).trimmed();
      const QString normalizedKind = normalizedRasterOption(kind);

      if (normalizedKind == "all") {
        if (!value.isEmpty()) {
          *errorMessage = "Render graph view override selector 'all' takes no value";
          return false;
        }
        viewOverride.selector = engine::graph::SceneSelector::all();
      } else if (value.isEmpty()) {
        *errorMessage = "Render graph view override selector must use all, object_id:value, "
                        "object_name:value, tag:value, layer:value, or material_role:value";
        return false;
      } else if (normalizedKind == "objectid") {
        viewOverride.selector = engine::graph::SceneSelector::objectId(value.toStdString());
      } else if (normalizedKind == "objectname") {
        viewOverride.selector = engine::graph::SceneSelector::objectName(value.toStdString());
      } else if (normalizedKind == "tag") {
        viewOverride.selector = engine::graph::SceneSelector::tag(value.toStdString());
      } else if (normalizedKind == "layer") {
        viewOverride.selector = engine::graph::SceneSelector::layer(value.toStdString());
      } else if (normalizedKind == "materialrole") {
        viewOverride.selector = engine::graph::SceneSelector::materialRole(value.toStdString());
      } else {
        *errorMessage = "Render graph view override selector must use all, object_id:value, "
                        "object_name:value, tag:value, layer:value, or material_role:value";
        return false;
      }
      return true;
    }

    bool parseField(const QString& text, QString* errorMessage) {
      const int separator = text.indexOf('=');
      if (separator <= 0 || separator == text.size() - 1) {
        *errorMessage = "Render graph view override fields must use key=value syntax";
        return false;
      }

      const QString key = text.left(separator).trimmed();
      const QString rawValue = text.mid(separator + 1).trimmed();
      if (key.isEmpty() || rawValue.isEmpty()) {
        *errorMessage = "Render graph view override field key and value must not be empty";
        return false;
      }

      const QString normalizedKey = normalizedRasterOption(key);
      if (normalizedKey == "executor") {
        engine::graph::RenderExecutorPreference executor;
        if (!parseRenderExecutorPreference(rawValue, &executor)) {
          *errorMessage = "Render graph view override executor must be 'raytracer', 'pathtracer', "
                          "'wavefront', 'rasterizer', or 'wireframe'";
          return false;
        }
        viewOverride.executor = executor;
      } else if (normalizedKey == "view" || normalizedKey == "viewmode") {
        engine::graph::RenderViewMode viewMode;
        if (!parseImplementedRenderViewMode(rawValue, &viewMode)) {
          *errorMessage =
            "Render graph view override view must be 'default', 'beauty', 'wireframe', "
            "'depth', 'stencil', 'stencil_composite', 'normal', 'object_id', "
            "'material_id', 'world_position', 'sample_stddev', 'sample_stddev_color', "
            "'raster_coverage_count', 'raster_depth_test_count', 'raster_depth_pass_count', "
            "'raster_shade_count', 'raster_color_write_count', or 'hybrid_visibility'";
          return false;
        }
        viewOverride.viewMode = viewMode;
      } else if (normalizedKey == "camera") {
        viewOverride.camera = engine::graph::RenderCameraRef{rawValue.toStdString(), std::nullopt};
      } else if (normalizedKey == "shadingprofile" || normalizedKey == "profile") {
        engine::graph::ShadingProfileRef profile =
          viewOverride.shadingProfile.value_or(engine::graph::ShadingProfileRef{});
        profile.name = rawValue.toStdString();
        viewOverride.shadingProfile = std::move(profile);
      } else if (normalizedKey.startsWith("parameter") ||
                 normalizedKey.startsWith("shadingparameter")) {
        const int parameterSeparator = key.indexOf(':');
        if (parameterSeparator <= 0 || parameterSeparator == key.size() - 1) {
          *errorMessage =
            "Render graph view override shading parameters must use parameter:name=value";
          return false;
        }
        engine::graph::ShadingProfileRef profile =
          viewOverride.shadingProfile.value_or(engine::graph::ShadingProfileRef{});
        profile.setParameter(
          key.mid(parameterSeparator + 1).trimmed().toStdString(),
          engine::graph::ShadingProfileParameterValue::fromText(rawValue.toStdString()));
        viewOverride.shadingProfile = std::move(profile);
      } else {
        *errorMessage =
          "Render graph view override field must be executor, view, camera, shading_profile, "
          "or parameter:name";
        return false;
      }
      return true;
    }
  };

  struct RenderGraphImageInput {
    std::string resourceId;
    QString input;

    bool parse(const QString& value, const char* optionName, QString* errorMessage) {
      const int separator = value.indexOf('=');
      if (separator <= 0 || separator == value.size() - 1) {
        *errorMessage = QString("%1 must use resource=file syntax").arg(optionName);
        return false;
      }

      const QString id = value.left(separator).trimmed();
      if (id.isEmpty()) {
        *errorMessage = QString("%1 resource id must not be empty").arg(optionName);
        return false;
      }

      resourceId = id.toStdString();
      input = value.mid(separator + 1);
      return true;
    }
  };

  bool parseRenderGraphAOVOutput(const QString& value, RenderGraphAOVOutput* output,
                                 QString* errorMessage) {
    const int separator = value.indexOf('=');
    if (separator <= 0 || separator == value.size() - 1) {
      *errorMessage =
        "Render graph AOV output must use view=file syntax with view 'depth', 'stencil', "
        "'normal', 'object_id', 'material_id', 'world_position', 'sample_stddev', "
        "'sample_stddev_color', 'raster_coverage_count', 'raster_depth_test_count', "
        "'raster_depth_pass_count', 'raster_shade_count', 'raster_color_write_count', or "
        "'hybrid_visibility'";
      return false;
    }

    const auto* aov = engine::graph::renderAOVDefinitionForName(
      normalizedRasterOption(value.left(separator)).toStdString());
    if (!aov) {
      *errorMessage =
        "Render graph AOV output view must be 'depth', 'stencil', 'normal', 'object_id', "
        "'material_id', 'world_position', 'sample_stddev', 'sample_stddev_color', "
        "'raster_coverage_count', 'raster_depth_test_count', 'raster_depth_pass_count', "
        "'raster_shade_count', 'raster_color_write_count', or 'hybrid_visibility'";
      return false;
    }

    output->viewMode = aov->viewMode();
    output->output = value.mid(separator + 1);
    return true;
  }

  bool parseShadingProfileParameter(
    const QString& value,
    std::pair<std::string, engine::graph::ShadingProfileParameterValue>* parameter,
    QString* errorMessage) {
    const int separator = value.indexOf('=');
    if (separator <= 0 || separator == value.size() - 1) {
      *errorMessage = "Render graph shading parameter must use key=value syntax";
      return false;
    }

    const QString key = value.left(separator).trimmed();
    const QString rawValue = value.mid(separator + 1).trimmed();
    if (key.isEmpty() || rawValue.isEmpty()) {
      *errorMessage = "Render graph shading parameter key and value must not be empty";
      return false;
    }

    *parameter = {key.toStdString(),
                  engine::graph::ShadingProfileParameterValue::fromText(rawValue.toStdString())};
    return true;
  }

  bool parseImportOption(const QString& value, std::pair<QString, QVariant>* option,
                         QString* errorMessage) {
    const int separator = value.indexOf('=');
    if (separator <= 0 || separator == value.size() - 1) {
      *errorMessage = "Import option must use key=value syntax";
      return false;
    }

    const QString key = value.left(separator).trimmed();
    const QString rawValue = value.mid(separator + 1).trimmed();
    if (key.isEmpty()) {
      *errorMessage = "Import option key must not be empty";
      return false;
    }

    option->first = key;
    option->second = rawValue;
    return true;
  }

  bool parseNonNegativeStepIndex(const QString& value, int* step, QString* errorMessage) {
    bool ok = false;
    const int parsed = value.trimmed().toInt(&ok);
    if (!ok || parsed < 0) {
      *errorMessage = "Step selection values must be non-negative integers";
      return false;
    }

    *step = parsed;
    return true;
  }

  bool parseStepRange(const QString& value, int* firstStep, int* lastStep, QString* errorMessage) {
    const QString range = value.trimmed();
    int separator = range.indexOf("..");
    int separatorLength = 2;
    if (separator < 0) {
      separator = range.indexOf('-');
      separatorLength = 1;
    }
    if (separator <= 0 || separator + separatorLength >= range.size()) {
      *errorMessage = "Step sequence range must use FIRST-LAST or FIRST..LAST";
      return false;
    }

    int first = 0;
    int last = 0;
    if (!parseNonNegativeStepIndex(range.left(separator), &first, errorMessage) ||
        !parseNonNegativeStepIndex(range.mid(separator + separatorLength), &last, errorMessage)) {
      return false;
    }

    if (last < first)
      std::swap(first, last);

    *firstStep = first;
    *lastStep = last;
    return true;
  }

  bool parseStepSelection(const QString& value, CommandLineStepSelection* selection,
                          QString* errorMessage) {
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
      *errorMessage = "Step selection must be N, single:N, cumulative:N, or sequence[:FIRST-LAST]";
      return false;
    }

    const int colon = trimmed.indexOf(':');
    const int equals = trimmed.indexOf('=');
    int separator = -1;
    if (colon >= 0 && equals >= 0) {
      separator = std::min(colon, equals);
    } else {
      separator = std::max(colon, equals);
    }

    QString modeText;
    QString argumentText;
    if (separator < 0) {
      modeText = trimmed;
    } else {
      modeText = trimmed.left(separator).trimmed();
      argumentText = trimmed.mid(separator + 1).trimmed();
    }

    const QString normalizedMode = normalizedRasterOption(modeText);
    if (separator < 0 && normalizedMode != "sequence") {
      selection->mode = CommandLineStepMode::Single;
      bool ok = false;
      trimmed.toInt(&ok);
      if (!ok) {
        *errorMessage =
          "Step must be an integer. Step selection must be N, single:N, cumulative:N, or "
          "sequence[:FIRST-LAST]";
        return false;
      }
      return parseNonNegativeStepIndex(trimmed, &selection->step, errorMessage);
    }

    if (normalizedMode == "single" || normalizedMode == "only" || normalizedMode == "step") {
      if (argumentText.isEmpty()) {
        *errorMessage = "Single step selection requires a step index";
        return false;
      }
      selection->mode = CommandLineStepMode::Single;
      return parseNonNegativeStepIndex(argumentText, &selection->step, errorMessage);
    }

    if (normalizedMode == "cumulative" || normalizedMode == "through" || normalizedMode == "upto") {
      if (argumentText.isEmpty()) {
        *errorMessage = "Cumulative step selection requires a step index";
        return false;
      }
      selection->mode = CommandLineStepMode::Cumulative;
      return parseNonNegativeStepIndex(argumentText, &selection->step, errorMessage);
    }

    if (normalizedMode == "sequence") {
      selection->mode = CommandLineStepMode::Sequence;
      selection->rangeSet = false;
      if (!argumentText.isEmpty()) {
        selection->rangeSet = true;
        return parseStepRange(argumentText, &selection->firstStep, &selection->lastStep,
                              errorMessage);
      }
      return true;
    }

    *errorMessage = "Step selection must be N, single:N, cumulative:N, or sequence[:FIRST-LAST]";
    return false;
  }

  void collectStepIndices(const Element& root, std::set<int>* steps) {
    if (const auto* group = qobject_cast<const Group*>(&root)) {
      if (const auto stepIndex = group->stepIndex()) {
        steps->insert(*stepIndex);
      }
    }

    for (const auto& child : root.childElements()) {
      collectStepIndices(*child, steps);
    }
  }

  bool selectionMatchesAnyStep(const std::set<int>& steps,
                               const StepVisibilitySelection& selection) {
    return std::any_of(steps.begin(), steps.end(), [&](int step) {
      switch (selection.mode()) {
      case StepVisibilityMode::OnlyStep:
        return selection.firstStep() && step == *selection.firstStep();
      case StepVisibilityMode::Cumulative:
        return selection.lastStep() && step <= *selection.lastStep();
      case StepVisibilityMode::All:
        return true;
      case StepVisibilityMode::Range:
        return selection.firstStep() && selection.lastStep() && step >= *selection.firstStep() &&
               step <= *selection.lastStep();
      }
      return false;
    });
  }

  StepVisibilitySelection
  commandLineStepVisibilitySelection(const CommandLineStepSelection& selection,
                                     int sequenceStep = 0) {
    switch (selection.mode) {
    case CommandLineStepMode::Single:
      return StepVisibilitySelection::onlyStep(selection.step);
    case CommandLineStepMode::Cumulative:
      return StepVisibilitySelection::cumulativeThrough(selection.step);
    case CommandLineStepMode::Sequence:
      return StepVisibilitySelection::cumulativeThrough(sequenceStep);
    }

    return StepVisibilitySelection::all();
  }

  void applyStepVisibilitySelection(Scene& scene, const StepVisibilitySelection& selection) {
    const StepVisibilityEvaluator evaluator(selection);
    evaluator.forEachGroup(scene, [](const Group& group, bool directlyVisible, bool) {
      const_cast<Group&>(group).setVisible(directlyVisible);
    });
  }

  std::vector<std::string> rasterRecursiveMaterialFallbackWarnings(const render::Scene& scene) {
    std::set<std::string> warnings;
    scene.forEachLeaf([&](const render::Primitive*, std::shared_ptr<render::Material> material) {
      if (!material)
        return;
      if (const char* warning = material->rasterRecursiveFallbackWarning()) {
        warnings.insert(warning);
      }
    });

    return {warnings.begin(), warnings.end()};
  }

  bool usesRasterizer(const engine::graph::RenderPlan& plan) {
    return std::any_of(
      plan.passes().begin(), plan.passes().end(), [](const engine::graph::RenderPassNode& pass) {
        return pass.enabled && pass.executor == engine::graph::RenderExecutorKind::Rasterizer;
      });
  }

  class RenderCliApplication {
  public:
    RenderCliApplication(int& argc, char** argv)
        : m_application(std::make_unique<QCoreApplication>(argc, argv)) {
    }

  private:
    std::unique_ptr<QCoreApplication> m_application;
  };
}

class Renderer {
public:
  enum CommandLineParseResult {
    CommandLineOk,
    CommandLineError,
    CommandLineVersionRequested,
    CommandLineHelpRequested
  };

  Renderer();
  void render() const;
  CommandLineParseResult parseCommandLine(QString* errorMessage);
  std::shared_ptr<render::Sampler> sampler() const;
  QImage bufferToImage(const Buffer<unsigned int>& buffer) const;
  QImage colorBufferToImage(const Buffer<Colord>& buffer) const;
  QImage scalarBufferToImage(const Buffer<double>& buffer) const;
  QImage normalizedColorBufferToImage(const Buffer<Colord>& buffer) const;

  QCommandLineParser parser;

private:
  QString m_filename;
  QString m_output;
  QString m_ldrawLibraryRoot;
  bool m_ldrawInput;
  bool m_ldrawPreserveAuthoringHierarchy;
  double m_ldrawScale;
  QString m_ldrawCoordinateConversion;
  bool m_ldrawPreserveHierarchy;
  QString m_ldrawNormalMode;
  bool m_ldrawIncludeEdgeOverlays;
  int m_ldrawMaxRecursion;
  QString m_ldrawMissingPartPolicy;
  QString m_ldrawBackgroundColor;

  int m_maximumRecursionDepth;
  bool m_maximumRecursionDepthSet;
  int m_pathTracerRussianRouletteDepth;
  bool m_pathTracerRussianRouletteDepthSet;
  int m_pathTracerDirectLightSamples;
  bool m_pathTracerDirectLightSamplesSet;
  int m_gpuPrimarySampleChunkSize;
  bool m_gpuPrimarySampleChunkSizeSet;
  QString m_pathTracingSchedule;
  bool m_pathTracingScheduleSet;
  QString m_tracingBackend;
  bool m_tracingBackendSet;
  QString m_tracingExecution;
  bool m_tracingExecutionSet;
  QString m_wavefrontIntersectionBackend;
  bool m_wavefrontIntersectionBackendSet;
  int m_width;
  int m_height;
  bool m_widthSet;
  bool m_heightSet;
  QString m_sampler;
  bool m_samplerSet;
  QString m_sampleStreamMode;
  bool m_sampleStreamModeSet;
  int m_samplesPerPixel;
  bool m_samplesPerPixelSet;
  std::optional<std::uint64_t> m_samplingSeed;
  int m_threads;
  int m_queueSize;
  bool m_threadsSet;
  bool m_queueSizeSet;
  QString m_tonemap;
  QString m_importFormat;
  world::ImportOptions m_importOptions;
  QString m_engine;
  bool m_engineSet;
  QString m_integrator;
  bool m_integratorSet;
  bool m_wavefrontConvergenceEnabled;
  bool m_wavefrontConvergenceSet;
  double m_wavefrontConvergenceActiveFraction;
  bool m_wavefrontConvergenceActiveFractionSet;
  double m_wavefrontConvergenceRmsDelta;
  bool m_wavefrontConvergenceRmsDeltaSet;
  bool m_wavefrontAdaptiveSamplingEnabled;
  bool m_wavefrontAdaptiveSamplingSet;
  int m_wavefrontAdaptiveMinimumSamples;
  bool m_wavefrontAdaptiveMinimumSamplesSet;
  double m_wavefrontAdaptiveStddevThreshold;
  bool m_wavefrontAdaptiveStddevThresholdSet;
  QString m_wavefrontDenoiser;
  bool m_wavefrontDenoiserSet;
  int m_wavefrontDenoiseRadius;
  bool m_wavefrontDenoiseRadiusSet;
  double m_wavefrontDenoiseColorSigma;
  bool m_wavefrontDenoiseColorSigmaSet;
  bool m_renderGraph;
  bool m_directEngine;
  bool m_renderGraphOnly;
  QString m_renderGraphFormat;
  QString m_renderGraphOut;
  QString m_renderGraphIn;
  QString m_renderGraphTraceOut;
  QString m_rasterMetricsOut;
  bool m_rasterMetricsSummary;
  QString m_wavefrontMetricsOut;
  bool m_wavefrontMetricsSummary;
  QString m_wavefrontSampleStddevOut;
  QString m_wavefrontSampleStddevColorOut;
  std::vector<RenderGraphAOVOutput> m_renderGraphAOVOutputs;
  std::vector<RenderGraphViewOverrideInput> m_renderGraphViewOverrides;
  std::vector<RenderGraphImageInput> m_renderGraphColorInputs;
  std::vector<RenderGraphImageInput> m_renderGraphDepthInputs;
  std::vector<RenderGraphImageInput> m_renderGraphStencilInputs;
  std::vector<RenderGraphImageInput> m_renderGraphObjectIdInputs;
  std::vector<RenderGraphImageInput> m_renderGraphMaterialIdInputs;
  bool m_renderGraphExecutorSet;
  engine::graph::RenderExecutorPreference m_renderGraphExecutor;
  bool m_renderGraphViewModeSet;
  engine::graph::RenderViewMode m_renderGraphViewMode;
  QString m_renderGraphCamera;
  QString m_renderGraphShadingProfile;
  engine::graph::ShadingProfileParameters m_renderGraphShadingProfileParameters;
  bool m_renderGraphWireframeOverlay;
  bool m_renderGraphCurveOverlay;
  engine::graph::RenderGraphOverrides m_renderGraphOverrides;
  int m_wireframeLod;
  QString m_rasterCullMode;
  bool m_rasterCullModeSet;
  QString m_rasterBackend;
  bool m_rasterBackendSet;
  QString m_rasterVisibilityCulling;
  QString m_rasterTessellationQuality;
  bool m_rasterTessellationQualitySet;
  double m_rasterMaxScreenSpaceError;
  bool m_rasterMaxScreenSpaceErrorSet;
  QString m_rasterDepthPrepass;
  int m_rasterMsaaSamples;
  QString m_rasterMsaaShadingMode;
  QString m_rasterPostProcessAA;
  bool m_rasterPostProcessAASet;
  std::uint8_t m_rasterColorWriteMask;
  bool m_rasterBlending;
  engine::raster::Rasterizer::BlendFactor m_rasterBlendSourceFactor;
  engine::raster::Rasterizer::BlendFactor m_rasterBlendDestinationFactor;
  engine::raster::Rasterizer::BlendOp m_rasterBlendOp;
  Colord m_rasterBlendConstantColor;
  double m_rasterBlendConstantAlpha;
  bool m_rasterAlphaTest;
  engine::raster::Rasterizer::AlphaFunc m_rasterAlphaFunc;
  double m_rasterAlphaReference;
  bool m_rasterViewportSet;
  Recti m_rasterViewport;
  bool m_rasterScissorSet;
  Recti m_rasterScissor;
  double m_rasterDepthBias;
  bool m_rasterShadowMaps;
  int m_rasterShadowMapSize;
  int m_rasterShadowCascadeCount;
  double m_rasterShadowCascadeSplitLambda;
  double m_rasterShadowBias;
  double m_rasterShadowSlopeBias;
  int m_rasterShadowFilterRadius;
  QString m_rasterShadowFilterMode;
  QString m_rasterShadowMode;
  int m_repeat;
  bool m_timing;
  int m_frame;
  bool m_frameSet;
  bool m_animation;
  int m_frameStart;
  int m_frameEnd;
  double m_fps;
  bool m_frameStartSet;
  bool m_frameEndSet;
  bool m_fpsSet;
  bool m_stepSelectionSet;
  CommandLineStepSelection m_stepSelection;
  StepPlaybackStyle m_stepPlaybackStyle;
  bool m_gcodeOptionsSet;
  bool m_gcodeLayerSet;
  int m_gcodeLayer;
  bool m_gcodeCumulativeLayers;

  std::unique_ptr<Scene> loadScene() const;
  std::vector<double> renderScene(const Scene& scene, const QString& output) const;
  void renderAnimation(const Scene& scene) const;
  void renderStepSequence(const Scene& scene) const;
  void validateStepSelection(const Scene& scene, const StepVisibilitySelection& selection) const;
  std::vector<int> sequenceSteps(const Scene& scene) const;
  bool pathTracerEngineSelected() const;
  bool pathTracingRequested() const;
  bool scalarPathTracingScheduleSelected() const;
  std::optional<engine::graph::RenderExecutorPreference> engineExecutorPreference() const;
  engine::graph::RenderSceneAnalysis
  renderGraphSceneAnalysis(const Scene& scene,
                           const std::shared_ptr<render::Scene>& raytracerScene = nullptr) const;
  engine::graph::RenderGraphRequest
  renderGraphRequest(const Scene& scene,
                     const std::shared_ptr<render::Scene>& raytracerScene = nullptr) const;
  engine::graph::RenderIntent
  renderIntent(const Scene& scene,
               const std::shared_ptr<render::Scene>& raytracerScene = nullptr) const;
  int renderGraphSampleCount(const engine::graph::RenderIntent& intent) const;
  int rayFamilyQueueSize() const;
  int rayFamilyQueueSize(int samplesPerPixel) const;
  engine::graph::RenderEngineOptions commandLineEngineOptions() const;
  engine::graph::RenderPostProcessAA commandLinePostProcessAA() const;
  engine::graph::RasterBeautyPassState
  rasterBeautyPassState(engine::graph::RenderPostProcessAA postProcessAA,
                        bool includeImagePostProcessAA, bool includeShadowMapEnable) const;
  engine::graph::RasterShadowPassState rasterShadowPassState() const;
  engine::graph::WireframePassState wireframePassState() const;
  engine::graph::RenderPlan
  compileRenderGraphPlan(const Scene& scene,
                         const std::shared_ptr<render::Scene>& raytracerScene = nullptr) const;
  engine::graph::RenderPlan loadRenderGraphPlan() const;
  engine::graph::RenderPlan
  renderGraphPlan(const Scene& scene,
                  const std::shared_ptr<render::Scene>& raytracerScene = nullptr) const;
  void applyRenderGraphOutputSize(const engine::graph::RenderPlan& plan, int* width,
                                  int* height) const;
  void validateRenderGraphPlan(const engine::graph::RenderPlan& plan) const;
  void writeRenderGraphPlan(const engine::graph::RenderPlan& plan, const QString& output) const;
  void writeRenderGraphTrace(const engine::graph::RenderGraphExecutionTrace& trace,
                             const QString& output) const;
  void writeRasterMetricsReport(const QJsonArray& runs, const QString& output) const;
  void writeWavefrontMetricsReport(const QJsonArray& runs, const QString& output) const;
  void writeRenderGraphAOVOutputs(const engine::graph::RenderGraphExecutionTrace& trace,
                                  const engine::graph::RenderIntent& intent) const;
  std::shared_ptr<Buffer<Colord>>
  loadRenderGraphColorInput(const RenderGraphImageInput& input) const;
  std::shared_ptr<Buffer<double>>
  loadRenderGraphDepthInput(const RenderGraphImageInput& input) const;
  std::shared_ptr<Buffer<std::uint8_t>>
  loadRenderGraphStencilInput(const RenderGraphImageInput& input) const;
  std::shared_ptr<Buffer<std::uint32_t>>
  loadRenderGraphIntegerIdInput(const RenderGraphImageInput& input) const;
  void bindRenderGraphExternalInputs(engine::graph::GraphRenderEngine& graphEngine) const;
  QString renderGraphOutputPath() const;
  QString outputForFrame(int frame) const;
  static bool hasFramePlaceholder(const QString& pattern, QString* errorMessage);
  static bool isKnownSampler(const QString& sampler);
};

Renderer::Renderer()
    : m_ldrawInput(false),
      m_ldrawPreserveAuthoringHierarchy(false),
      m_ldrawScale(1.0),
      m_ldrawCoordinateConversion("none"),
      m_ldrawPreserveHierarchy(true),
      m_ldrawNormalMode("flat"),
      m_ldrawIncludeEdgeOverlays(true),
      m_ldrawMaxRecursion(64),
      m_ldrawMissingPartPolicy("skip"),
      m_maximumRecursionDepth(10),
      m_maximumRecursionDepthSet(false),
      m_pathTracerRussianRouletteDepth(3),
      m_pathTracerRussianRouletteDepthSet(false),
      m_pathTracerDirectLightSamples(1),
      m_pathTracerDirectLightSamplesSet(false),
      m_gpuPrimarySampleChunkSize(0),
      m_gpuPrimarySampleChunkSizeSet(false),
      m_pathTracingSchedule("wavefront"),
      m_pathTracingScheduleSet(false),
      m_tracingBackend("auto"),
      m_tracingBackendSet(false),
      m_tracingExecution("auto"),
      m_tracingExecutionSet(false),
      m_wavefrontIntersectionBackend("auto"),
      m_wavefrontIntersectionBackendSet(false),
      m_width(640),
      m_height(480),
      m_widthSet(false),
      m_heightSet(false),
      m_sampler("Regular"),
      m_samplerSet(false),
      m_sampleStreamMode("sampler"),
      m_sampleStreamModeSet(false),
      m_samplesPerPixel(1),
      m_samplesPerPixelSet(false),
      m_samplingSeed(),
      m_threads(QThread::idealThreadCount()),
      m_queueSize(render::RayFamilyQueuePolicy::DefaultMaximumQueueSize),
      m_threadsSet(false),
      m_queueSizeSet(false),
      m_tonemap("Linear"),
      m_importFormat(),
      m_importOptions(),
      m_engine("raytracer"),
      m_engineSet(false),
      m_integrator("whitted"),
      m_integratorSet(false),
      m_wavefrontConvergenceEnabled(false),
      m_wavefrontConvergenceSet(false),
      m_wavefrontConvergenceActiveFraction(0.0),
      m_wavefrontConvergenceActiveFractionSet(false),
      m_wavefrontConvergenceRmsDelta(0.0),
      m_wavefrontConvergenceRmsDeltaSet(false),
      m_wavefrontAdaptiveSamplingEnabled(false),
      m_wavefrontAdaptiveSamplingSet(false),
      m_wavefrontAdaptiveMinimumSamples(defaultWavefrontAdaptiveMinimumSamples),
      m_wavefrontAdaptiveMinimumSamplesSet(false),
      m_wavefrontAdaptiveStddevThreshold(defaultWavefrontAdaptiveStddevThreshold),
      m_wavefrontAdaptiveStddevThresholdSet(false),
      m_wavefrontDenoiser("none"),
      m_wavefrontDenoiserSet(false),
      m_wavefrontDenoiseRadius(1),
      m_wavefrontDenoiseRadiusSet(false),
      m_wavefrontDenoiseColorSigma(0.1),
      m_wavefrontDenoiseColorSigmaSet(false),
      m_renderGraph(true),
      m_directEngine(false),
      m_renderGraphOnly(false),
      m_renderGraphFormat("text"),
      m_renderGraphOut(),
      m_renderGraphIn(),
      m_renderGraphTraceOut(),
      m_rasterMetricsOut(),
      m_rasterMetricsSummary(false),
      m_wavefrontMetricsOut(),
      m_wavefrontMetricsSummary(false),
      m_wavefrontSampleStddevOut(),
      m_wavefrontSampleStddevColorOut(),
      m_renderGraphAOVOutputs(),
      m_renderGraphExecutorSet(false),
      m_renderGraphExecutor(engine::graph::RenderExecutorPreference::Raytracer),
      m_renderGraphViewModeSet(false),
      m_renderGraphViewMode(engine::graph::RenderViewMode::Beauty),
      m_renderGraphCamera(),
      m_renderGraphShadingProfile(),
      m_renderGraphShadingProfileParameters(),
      m_renderGraphWireframeOverlay(false),
      m_renderGraphCurveOverlay(false),
      m_renderGraphOverrides(),
      m_wireframeLod(0),
      m_rasterCullMode("both"),
      m_rasterCullModeSet(false),
      m_rasterBackend("cpu"),
      m_rasterBackendSet(false),
      m_rasterVisibilityCulling("off"),
      m_rasterTessellationQuality("balanced"),
      m_rasterTessellationQualitySet(false),
      m_rasterMaxScreenSpaceError(0.0),
      m_rasterMaxScreenSpaceErrorSet(false),
      m_rasterDepthPrepass("off"),
      m_rasterMsaaSamples(1),
      m_rasterMsaaShadingMode("per_sample"),
      m_rasterPostProcessAA("none"),
      m_rasterPostProcessAASet(false),
      m_rasterColorWriteMask(engine::raster::Rasterizer::ColorWriteAll),
      m_rasterBlending(false),
      m_rasterBlendSourceFactor(engine::raster::Rasterizer::BlendFactor::One),
      m_rasterBlendDestinationFactor(engine::raster::Rasterizer::BlendFactor::Zero),
      m_rasterBlendOp(engine::raster::Rasterizer::BlendOp::Add),
      m_rasterBlendConstantColor(Colord::white()),
      m_rasterBlendConstantAlpha(1.0),
      m_rasterAlphaTest(false),
      m_rasterAlphaFunc(engine::raster::Rasterizer::AlphaFunc::Always),
      m_rasterAlphaReference(0.0),
      m_rasterViewportSet(false),
      m_rasterViewport(),
      m_rasterScissorSet(false),
      m_rasterScissor(),
      m_rasterDepthBias(0.0),
      m_rasterShadowMaps(false),
      m_rasterShadowMapSize(256),
      m_rasterShadowCascadeCount(1),
      m_rasterShadowCascadeSplitLambda(0.5),
      m_rasterShadowBias(1e-3),
      m_rasterShadowSlopeBias(0.0),
      m_rasterShadowFilterRadius(0),
      m_rasterShadowFilterMode("pcf"),
      m_rasterShadowMode("shadow_maps"),
      m_repeat(1),
      m_timing(false),
      m_frame(0),
      m_frameSet(false),
      m_animation(false),
      m_frameStart(0),
      m_frameEnd(0),
      m_fps(0.0),
      m_frameStartSet(false),
      m_frameEndSet(false),
      m_fpsSet(false),
      m_stepSelectionSet(false),
      m_stepSelection(),
      m_stepPlaybackStyle(),
      m_gcodeOptionsSet(false),
      m_gcodeLayerSet(false),
      m_gcodeLayer(0),
      m_gcodeCumulativeLayers(false) {
  parser.setApplicationDescription(
    QCoreApplication::translate("rendercli", "Command line renderer."));
}

std::unique_ptr<Scene> Renderer::loadScene() const {
  RenderCliSceneLoadOptions options;
  options.filename = m_filename;
  options.importFormat = m_importFormat;
  options.importOptions = m_importOptions;
  options.ldrawLibraryRoot = m_ldrawLibraryRoot;
  options.ldrawInput = m_ldrawInput;
  options.ldrawPreserveAuthoringHierarchy = m_ldrawPreserveAuthoringHierarchy;
  options.ldrawScale = m_ldrawScale;
  options.ldrawCoordinateConversion = m_ldrawCoordinateConversion;
  options.ldrawPreserveHierarchy = m_ldrawPreserveHierarchy;
  options.ldrawNormalMode = m_ldrawNormalMode;
  options.ldrawIncludeEdgeOverlays = m_ldrawIncludeEdgeOverlays;
  options.ldrawMaxRecursion = m_ldrawMaxRecursion;
  options.ldrawMissingPartPolicy = m_ldrawMissingPartPolicy;
  options.ldrawBackgroundColor = m_ldrawBackgroundColor;
  options.stepPlaybackStyle = m_stepPlaybackStyle;
  return RenderCliSceneLoader(std::move(options)).load();
}

engine::graph::RenderSceneAnalysis
Renderer::renderGraphSceneAnalysis(const Scene& scene,
                                   const std::shared_ptr<render::Scene>& raytracerScene) const {
  engine::graph::RenderSceneAnalysis analysis = scene.renderGraphAnalysis();
  if (raytracerScene) {
    analysis.setFullGpuTracingSupportFromScene(*raytracerScene);
  }
  return analysis;
}

engine::graph::RenderIntent
Renderer::renderIntent(const Scene& scene,
                       const std::shared_ptr<render::Scene>& raytracerScene) const {
  return renderGraphRequest(scene, raytracerScene).resolvedIntent();
}

bool Renderer::pathTracerEngineSelected() const {
  return m_engine == "pathtracer" || m_engine == "pt";
}

bool Renderer::pathTracingRequested() const {
  return pathTracerEngineSelected() || m_integrator == "pathtracer" ||
         m_integrator == "path_tracer" || m_integrator == "pt";
}

bool Renderer::scalarPathTracingScheduleSelected() const {
  return m_pathTracingSchedule == "scalar";
}

std::optional<engine::graph::RenderExecutorPreference> Renderer::engineExecutorPreference() const {
  if (!m_engineSet) {
    if (m_tracingBackendSet && !pathTracingRequested()) {
      return engine::graph::RenderExecutorPreference::Wavefront;
    }
    if (m_pathTracingScheduleSet && pathTracingRequested()) {
      return scalarPathTracingScheduleSelected()
               ? engine::graph::RenderExecutorPreference::Raytracer
               : engine::graph::RenderExecutorPreference::PathTracer;
    }
    return std::nullopt;
  }
  if (m_engine == "raster") {
    return engine::graph::RenderExecutorPreference::Rasterizer;
  }
  if (m_engine == "wireframe") {
    return engine::graph::RenderExecutorPreference::Wireframe;
  }
  if (pathTracerEngineSelected()) {
    return scalarPathTracingScheduleSelected()
             ? engine::graph::RenderExecutorPreference::Raytracer
             : engine::graph::RenderExecutorPreference::PathTracer;
  }
  if (m_engine == "wavefront") {
    return engine::graph::RenderExecutorPreference::Wavefront;
  }
  if (m_engine == "raytracer" && m_tracingBackendSet) {
    return engine::graph::RenderExecutorPreference::Wavefront;
  }
  return engine::graph::RenderExecutorPreference::Raytracer;
}

engine::graph::RenderGraphRequest
Renderer::renderGraphRequest(const Scene& scene,
                             const std::shared_ptr<render::Scene>& raytracerScene) const {
  engine::graph::RenderIntent baseIntent = scene.renderIntentWithActiveCameraDefault();
  engine::graph::RenderEngineOptions commandLineOptions = commandLineEngineOptions();
  if (!baseIntent.engineOptions.raytracer().viewPlane() &&
      !commandLineOptions.raytracer().viewPlane()) {
    commandLineOptions.raytracer().setViewPlane("TiledViewPlane");
  }
  baseIntent.engineOptions = baseIntent.engineOptions.mergedWith(commandLineOptions);
  if (!baseIntent.engineOptions.raytracer().queueSize()) {
    const int raySamples =
      baseIntent.engineOptions.raytracer().samplesPerPixel().value_or(m_samplesPerPixel);
    baseIntent.engineOptions.raytracer().setQueueSize(rayFamilyQueueSize(raySamples));
  }
  engine::graph::RenderGraphRequest request(baseIntent);
  request.setSceneAnalysis(renderGraphSceneAnalysis(scene, raytracerScene));
  if (m_renderGraphExecutorSet) {
    request.setExecutorOverride(m_renderGraphExecutor);
  } else if (const auto executor = engineExecutorPreference()) {
    request.setExecutorShortcut(*executor);
  }
  if (m_renderGraphViewModeSet) {
    request.setViewModeOverride(m_renderGraphViewMode);
  }
  if (!m_renderGraphCamera.isEmpty()) {
    request.setCameraOverride(
      engine::graph::RenderCameraRef{m_renderGraphCamera.toStdString(), std::nullopt});
  }
  if (!m_renderGraphShadingProfile.isEmpty()) {
    request.setShadingProfileOverride(
      engine::graph::ShadingProfileRef{m_renderGraphShadingProfile.toStdString(), {}});
  }
  for (const auto& [key, value] : m_renderGraphShadingProfileParameters) {
    request.setShadingProfileParameterOverride(key, value);
  }
  if (m_renderGraphWireframeOverlay) {
    request.setWireframeOverlayOverride(true);
  }
  if (m_renderGraphCurveOverlay) {
    request.setCurveOverlayOverride(true);
  }
  if (m_rasterShadowMaps) {
    request.setPreviewShadowsOverride(true);
  }
  if (m_rasterPostProcessAASet) {
    request.setPostProcessAAOverride(commandLinePostProcessAA());
  }
  for (const auto& aovOutput : m_renderGraphAOVOutputs) {
    request.requestExportedAOV(aovOutput.viewMode);
  }
  for (const auto& overrideInput : m_renderGraphViewOverrides) {
    request.addViewOverride(overrideInput.viewOverride);
  }
  return request;
}

int Renderer::renderGraphSampleCount(const engine::graph::RenderIntent& intent) const {
  return intent.targetSampleCountHint(intent.defaultExecutorKind() ==
                                          engine::graph::RenderExecutorKind::Rasterizer
                                        ? m_rasterMsaaSamples
                                        : m_samplesPerPixel);
}

int Renderer::rayFamilyQueueSize() const {
  return rayFamilyQueueSize(m_samplesPerPixel);
}

int Renderer::rayFamilyQueueSize(int samplesPerPixel) const {
  if (m_queueSizeSet) {
    return m_queueSize;
  }
  return render::RayFamilyQueuePolicy(m_width, m_height, samplesPerPixel, m_threads).queueSize();
}

engine::graph::RenderEngineOptions Renderer::commandLineEngineOptions() const {
  engine::graph::RenderEngineOptions options;

  if (m_maximumRecursionDepthSet)
    options.raytracer().setMaximumRecursionDepth(m_maximumRecursionDepth);
  if (m_pathTracerRussianRouletteDepthSet)
    options.raytracer().setRussianRouletteDepth(m_pathTracerRussianRouletteDepth);
  if (m_pathTracerDirectLightSamplesSet)
    options.raytracer().setDirectLightSamples(m_pathTracerDirectLightSamples);
  if (m_gpuPrimarySampleChunkSizeSet)
    options.raytracer().setGpuPrimarySampleChunkSize(m_gpuPrimarySampleChunkSize);
  if (pathTracingRequested()) {
    options.raytracer().setIntegrator("pathtracer");
  } else if (m_tracingBackendSet || m_tracingExecutionSet) {
    options.raytracer().setIntegrator("whitted");
  } else if (m_integratorSet) {
    options.raytracer().setIntegrator(m_integrator.toStdString());
  }
  if (m_wavefrontIntersectionBackendSet)
    options.raytracer().setIntersectionBackend(m_wavefrontIntersectionBackend.toStdString());
  if (m_tracingBackendSet)
    options.raytracer().setTracingBackend(m_tracingBackend.toStdString());
  if (m_tracingExecutionSet)
    options.raytracer().setTracingExecution(m_tracingExecution.toStdString());
  if (pathTracingRequested() && m_tracingExecutionSet && m_tracingExecution == "gpu" &&
      !m_sampleStreamModeSet && !m_samplerSet) {
    options.raytracer().setSampleStreamMode("gpu_sample_stream");
  }
  if (m_samplerSet)
    options.raytracer().setSampler(m_sampler.toStdString());
  if (m_sampleStreamModeSet)
    options.raytracer().setSampleStreamMode(m_sampleStreamMode.toStdString());
  if (m_samplesPerPixelSet)
    options.raytracer().setSamplesPerPixel(m_samplesPerPixel);
  if (m_samplingSeed)
    options.raytracer().setSamplingSeed(*m_samplingSeed);
  if (m_wavefrontConvergenceSet) {
    options.raytracer().setConvergenceEnabled(m_wavefrontConvergenceEnabled);
    if (m_wavefrontConvergenceEnabled) {
      options.raytracer().setConvergenceActiveSampleFractionThreshold(
        RAYTRACER_WAVEFRONT_ACTIVE_SAMPLE_FRACTION_THRESHOLD);
      options.raytracer().setConvergenceRadianceDeltaRmsThreshold(
        RAYTRACER_WAVEFRONT_RADIANCE_DELTA_RMS_THRESHOLD);
    }
  }
  if (m_wavefrontConvergenceActiveFractionSet) {
    options.raytracer().setConvergenceActiveSampleFractionThreshold(
      m_wavefrontConvergenceActiveFraction);
  }
  if (m_wavefrontConvergenceRmsDeltaSet)
    options.raytracer().setConvergenceRadianceDeltaRmsThreshold(m_wavefrontConvergenceRmsDelta);
  if (m_wavefrontAdaptiveSamplingSet)
    options.raytracer().setAdaptiveSamplingEnabled(m_wavefrontAdaptiveSamplingEnabled);
  if (m_wavefrontAdaptiveSamplingSet && m_wavefrontAdaptiveSamplingEnabled) {
    options.raytracer().setAdaptiveMinimumSamples(defaultWavefrontAdaptiveMinimumSamples);
    options.raytracer().setAdaptiveStddevThreshold(defaultWavefrontAdaptiveStddevThreshold);
  }
  if (m_wavefrontAdaptiveMinimumSamplesSet)
    options.raytracer().setAdaptiveMinimumSamples(m_wavefrontAdaptiveMinimumSamples);
  if (m_wavefrontAdaptiveStddevThresholdSet)
    options.raytracer().setAdaptiveStddevThreshold(m_wavefrontAdaptiveStddevThreshold);
  if (m_wavefrontDenoiserSet)
    options.raytracer().setDenoiser(m_wavefrontDenoiser.toStdString());
  if (m_wavefrontDenoiseRadiusSet)
    options.raytracer().setDenoiseRadius(m_wavefrontDenoiseRadius);
  if (m_wavefrontDenoiseColorSigmaSet)
    options.raytracer().setDenoiseColorSigma(m_wavefrontDenoiseColorSigma);
  if (m_threadsSet) {
    options.raytracer().setMaximumThreads(m_threads);
    options.rasterizer().setMaximumThreads(m_threads);
  }
  if (m_queueSizeSet) {
    options.raytracer().setQueueSize(m_queueSize);
    options.rasterizer().setQueueSize(m_queueSize);
  }

  if (m_wireframeLod != 0) {
    options.wireframe().setLod(m_wireframeLod);
    options.rasterizer().setLod(m_wireframeLod);
    options.rasterizer().setTessellationQuality("final");
  }
  if (m_rasterTessellationQualitySet)
    options.rasterizer().setTessellationQuality(m_rasterTessellationQuality.toStdString());
  if (m_rasterMaxScreenSpaceErrorSet)
    options.rasterizer().setMaximumScreenSpaceError(m_rasterMaxScreenSpaceError);
  if (m_rasterBackendSet)
    options.rasterizer().setBackend(m_rasterBackend.toStdString());
  if (m_rasterCullModeSet)
    options.rasterizer().setCullMode(m_rasterCullMode.toStdString());
  if (m_rasterVisibilityCulling != "off")
    options.rasterizer().setVisibilityCulling(m_rasterVisibilityCulling.toStdString());
  if (m_rasterDepthPrepass != "off")
    options.rasterizer().setDepthPrepass(m_rasterDepthPrepass.toStdString());
  if (m_rasterMsaaSamples != 1)
    options.rasterizer().setMSAASamples(m_rasterMsaaSamples);
  if (m_rasterMsaaShadingMode != "per_sample")
    options.rasterizer().setMSAAShadingMode(m_rasterMsaaShadingMode.toStdString());
  if (m_rasterColorWriteMask != engine::raster::Rasterizer::ColorWriteAll)
    options.rasterizer().setColorWriteMask(m_rasterColorWriteMask);
  if (m_rasterBlending)
    options.rasterizer().setBlendingEnabled(true);
  if (m_rasterBlendSourceFactor != engine::raster::Rasterizer::BlendFactor::One ||
      m_rasterBlendDestinationFactor != engine::raster::Rasterizer::BlendFactor::Zero) {
    options.rasterizer().setBlendFactors(blendFactorName(m_rasterBlendSourceFactor),
                                         blendFactorName(m_rasterBlendDestinationFactor));
  }
  if (m_rasterBlendOp != engine::raster::Rasterizer::BlendOp::Add)
    options.rasterizer().setBlendOp(blendOpName(m_rasterBlendOp));
  if (!(m_rasterBlendConstantColor == Colord::white()) || m_rasterBlendConstantAlpha != 1.0)
    options.rasterizer().setBlendConstant(m_rasterBlendConstantColor, m_rasterBlendConstantAlpha);
  if (m_rasterAlphaTest)
    options.rasterizer().setAlphaTestEnabled(true);
  if (m_rasterAlphaFunc != engine::raster::Rasterizer::AlphaFunc::Always ||
      m_rasterAlphaReference != 0.0) {
    options.rasterizer().setAlphaFunc(alphaFuncName(m_rasterAlphaFunc), m_rasterAlphaReference);
  }
  if (m_rasterViewportSet)
    options.rasterizer().setViewportRect(m_rasterViewport);
  if (m_rasterScissorSet)
    options.rasterizer().setScissorRect(m_rasterScissor);
  if (m_rasterDepthBias != 0.0)
    options.rasterizer().setDepthBias(m_rasterDepthBias);
  if (m_rasterShadowMaps) {
    options.rasterizer().setShadowMapSize(m_rasterShadowMapSize);
    options.rasterizer().setShadowCascadeCount(m_rasterShadowCascadeCount);
    options.rasterizer().setShadowCascadeSplitLambda(m_rasterShadowCascadeSplitLambda);
    options.rasterizer().setShadowBias(m_rasterShadowBias);
    options.rasterizer().setShadowSlopeBias(m_rasterShadowSlopeBias);
    options.rasterizer().setShadowFilterRadius(m_rasterShadowFilterRadius);
    options.rasterizer().setShadowFilterMode(m_rasterShadowFilterMode.toStdString());
    options.rasterizer().setShadowMode(m_rasterShadowMode.toStdString());
  }

  return options;
}

engine::graph::RenderPostProcessAA Renderer::commandLinePostProcessAA() const {
  if (m_rasterPostProcessAA == "fxaa") {
    return engine::graph::RenderPostProcessAA::FXAA;
  }
  if (m_rasterPostProcessAA == "smaa") {
    return engine::graph::RenderPostProcessAA::SMAA;
  }
  if (m_rasterPostProcessAA == "taa") {
    return engine::graph::RenderPostProcessAA::TAA;
  }
  return engine::graph::RenderPostProcessAA::None;
}

engine::graph::RasterBeautyPassState
Renderer::rasterBeautyPassState(engine::graph::RenderPostProcessAA postProcessAA,
                                bool includeImagePostProcessAA, bool includeShadowMapEnable) const {
  engine::graph::RasterBeautyPassState state;
  state.geometry().setLod(m_wireframeLod);
  if (m_wireframeLod != 0) {
    state.geometry().setTessellationQuality(engine::raster::Rasterizer::TessellationQuality::Final);
  }
  if (m_rasterTessellationQualitySet) {
    if (m_rasterTessellationQuality == "preview") {
      state.geometry().setTessellationQuality(
        engine::raster::Rasterizer::TessellationQuality::Preview);
    } else if (m_rasterTessellationQuality == "final") {
      state.geometry().setTessellationQuality(
        engine::raster::Rasterizer::TessellationQuality::Final);
    } else {
      state.geometry().setTessellationQuality(
        engine::raster::Rasterizer::TessellationQuality::Balanced);
    }
  }
  if (m_rasterMaxScreenSpaceErrorSet) {
    state.geometry().setMaximumScreenSpaceError(m_rasterMaxScreenSpaceError);
  }
  if (m_threadsSet) {
    state.execution().setMaximumThreads(m_threads);
  }
  if (m_queueSizeSet) {
    state.execution().setQueueSize(m_queueSize);
  }
  if (m_rasterCullModeSet) {
    if (m_rasterCullMode == "back") {
      state.geometry().setCullMode(engine::raster::Rasterizer::CullMode::Back);
    } else if (m_rasterCullMode == "front") {
      state.geometry().setCullMode(engine::raster::Rasterizer::CullMode::Front);
    } else {
      state.geometry().setCullMode(engine::raster::Rasterizer::CullMode::Both);
    }
  }

  state.sampling().setMSAASamples(m_rasterMsaaSamples);
  if (m_rasterDepthPrepass == "on") {
    state.depthPrepass().setMode(engine::raster::Rasterizer::DepthPrepassMode::On);
  } else if (m_rasterDepthPrepass == "auto") {
    state.depthPrepass().setMode(engine::raster::Rasterizer::DepthPrepassMode::Auto);
  }
  if (m_rasterMsaaShadingMode == "per_fragment") {
    state.sampling().setMSAAShadingMode(engine::raster::Rasterizer::MSAAShadingMode::PerFragment);
  }
  if (includeImagePostProcessAA && postProcessAA == engine::graph::RenderPostProcessAA::FXAA) {
    state.sampling().setPostProcessAA(engine::raster::Rasterizer::PostProcessAA::FXAA);
  } else if (includeImagePostProcessAA &&
             postProcessAA == engine::graph::RenderPostProcessAA::SMAA) {
    state.sampling().setPostProcessAA(engine::raster::Rasterizer::PostProcessAA::SMAA);
  } else if (postProcessAA == engine::graph::RenderPostProcessAA::TAA) {
    state.sampling().setPostProcessAA(engine::raster::Rasterizer::PostProcessAA::TAA);
  }

  state.framebuffer().setColorWriteMask(m_rasterColorWriteMask);
  state.framebuffer().setBlendingEnabled(m_rasterBlending);
  state.framebuffer().setBlendFactors(m_rasterBlendSourceFactor, m_rasterBlendDestinationFactor);
  state.framebuffer().setBlendOp(m_rasterBlendOp);
  state.framebuffer().setBlendConstant(m_rasterBlendConstantColor, m_rasterBlendConstantAlpha);
  state.framebuffer().setAlphaTestEnabled(m_rasterAlphaTest);
  state.framebuffer().setAlphaFunc(m_rasterAlphaFunc, m_rasterAlphaReference);
  if (m_rasterViewportSet) {
    state.framebuffer().setViewportRect(m_rasterViewport);
  }
  if (m_rasterScissorSet) {
    state.framebuffer().setScissorRect(m_rasterScissor);
  }
  state.framebuffer().setDepthBias(m_rasterDepthBias);
  if (includeShadowMapEnable) {
    state.shadows().setShadowMapsEnabled(m_rasterShadowMaps);
    state.shadows().setShadowMapSize(m_rasterShadowMapSize);
    state.shadows().setShadowCascadeCount(m_rasterShadowCascadeCount);
    state.shadows().setShadowCascadeSplitLambda(m_rasterShadowCascadeSplitLambda);
    state.shadows().setShadowBias(m_rasterShadowBias);
    state.shadows().setShadowSlopeBias(m_rasterShadowSlopeBias);
    state.shadows().setShadowFilterRadius(m_rasterShadowFilterRadius);
    if (m_rasterShadowFilterMode == "pcss") {
      state.shadows().setShadowFilterMode(engine::raster::Rasterizer::ShadowFilterMode::PCSS);
    }
  }
  return state;
}

engine::graph::RasterShadowPassState Renderer::rasterShadowPassState() const {
  engine::graph::RasterShadowPassState state;
  state.shadows().setShadowMapsEnabled(true);
  state.shadows().setShadowMapSize(m_rasterShadowMapSize);
  state.shadows().setShadowCascadeCount(m_rasterShadowCascadeCount);
  state.shadows().setShadowCascadeSplitLambda(m_rasterShadowCascadeSplitLambda);
  state.shadows().setShadowBias(m_rasterShadowBias);
  state.shadows().setShadowSlopeBias(m_rasterShadowSlopeBias);
  state.shadows().setShadowFilterRadius(m_rasterShadowFilterRadius);
  if (m_rasterShadowFilterMode == "pcss") {
    state.shadows().setShadowFilterMode(engine::raster::Rasterizer::ShadowFilterMode::PCSS);
  }
  return state;
}

engine::graph::WireframePassState Renderer::wireframePassState() const {
  engine::graph::WireframePassState state;
  state.setLod(m_wireframeLod);
  return state;
}

engine::graph::RenderPlan
Renderer::compileRenderGraphPlan(const Scene& scene,
                                 const std::shared_ptr<render::Scene>& raytracerScene) const {
  const auto request = renderGraphRequest(scene, raytracerScene);
  const auto intent = request.resolvedIntent();
  const auto frameIntent = intent.withWholeFrameOverridesApplied();
  auto plan = request.compile({m_width, m_height, renderGraphSampleCount(frameIntent)});
  return plan.withOverrides(m_renderGraphOverrides);
}

engine::graph::RenderPlan Renderer::loadRenderGraphPlan() const {
  QFile file(m_renderGraphIn);
  if (!file.open(QIODevice::ReadOnly)) {
    throw std::runtime_error(
      QString("Unable to read render graph: %1").arg(m_renderGraphIn).toStdString());
  }

  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
  if (parseError.error != QJsonParseError::NoError) {
    throw std::runtime_error(QString("Unable to parse render graph JSON %1: %2")
                               .arg(m_renderGraphIn, parseError.errorString())
                               .toStdString());
  }
  if (!document.isObject()) {
    throw std::runtime_error(
      QString("Render graph JSON must contain an object: %1").arg(m_renderGraphIn).toStdString());
  }

  return engine::graph::RenderPlan::fromJson(document.object())
    .withOverrides(m_renderGraphOverrides);
}

engine::graph::RenderPlan
Renderer::renderGraphPlan(const Scene& scene,
                          const std::shared_ptr<render::Scene>& raytracerScene) const {
  if (!m_renderGraphIn.isEmpty()) {
    return loadRenderGraphPlan();
  }
  return compileRenderGraphPlan(scene, raytracerScene);
}

void Renderer::applyRenderGraphOutputSize(const engine::graph::RenderPlan& plan, int* width,
                                          int* height) const {
  if (m_renderGraphIn.isEmpty()) {
    return;
  }

  const auto& output = plan.exportedColorResource();

  if (m_widthSet && *width != output.width) {
    throw std::runtime_error(QString("Render graph output width is %1 but --width is %2")
                               .arg(output.width)
                               .arg(*width)
                               .toStdString());
  }
  if (m_heightSet && *height != output.height) {
    throw std::runtime_error(QString("Render graph output height is %1 but --height is %2")
                               .arg(output.height)
                               .arg(*height)
                               .toStdString());
  }

  if (!m_widthSet) {
    *width = output.width;
  }
  if (!m_heightSet) {
    *height = output.height;
  }
}

void Renderer::validateRenderGraphPlan(const engine::graph::RenderPlan& plan) const {
  const auto validation = plan.validate();
  if (validation.valid()) {
    return;
  }

  std::ostringstream out;
  out << "Render graph is invalid";
  for (const auto& error : validation.errors()) {
    out << "; " << engine::graph::toString(error.code) << ": " << error.message;
  }
  throw std::runtime_error(out.str());
}

void Renderer::writeRenderGraphPlan(const engine::graph::RenderPlan& plan,
                                    const QString& output) const {
  std::string graph;
  if (m_renderGraphFormat == "text") {
    graph = plan.toText();
  } else if (m_renderGraphFormat == "dot") {
    graph = plan.toDot();
  } else if (m_renderGraphFormat == "json") {
    graph = QJsonDocument(plan.toJson()).toJson(QJsonDocument::Indented).toStdString();
  } else {
    throw std::runtime_error("Unknown render graph format");
  }

  if (output.isEmpty()) {
    std::cout << graph;
    return;
  }

  QFile file(output);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    throw std::runtime_error(QString("Unable to write render graph: %1").arg(output).toStdString());
  }
  const QByteArray bytes = QByteArray::fromStdString(graph);
  if (file.write(bytes) != bytes.size()) {
    throw std::runtime_error(QString("Unable to write render graph: %1").arg(output).toStdString());
  }
}

void Renderer::writeRenderGraphTrace(const engine::graph::RenderGraphExecutionTrace& trace,
                                     const QString& output) const {
  QFile file(output);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    throw std::runtime_error(
      QString("Unable to write render graph trace: %1").arg(output).toStdString());
  }

  const QByteArray bytes = QJsonDocument(trace.toJson()).toJson(QJsonDocument::Indented);
  if (file.write(bytes) != bytes.size()) {
    throw std::runtime_error(
      QString("Unable to write render graph trace: %1").arg(output).toStdString());
  }
}

void Renderer::writeRasterMetricsReport(const QJsonArray& runs, const QString& output) const {
  QJsonObject report;
  report["schema"] = QStringLiteral("raytracer.raster_metrics.v1");
  report["runs"] = runs;

  QFile file(output);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    throw std::runtime_error(
      QString("Unable to write raster metrics report: %1").arg(output).toStdString());
  }

  const QByteArray bytes = QJsonDocument(report).toJson(QJsonDocument::Indented);
  if (file.write(bytes) != bytes.size()) {
    throw std::runtime_error(
      QString("Unable to write raster metrics report: %1").arg(output).toStdString());
  }
}

void Renderer::writeWavefrontMetricsReport(const QJsonArray& runs, const QString& output) const {
  QJsonObject report;
  report["schema"] = QStringLiteral("raytracer.wavefront_metrics.v1");
  report["runs"] = runs;

  QFile file(output);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    throw std::runtime_error(
      QString("Unable to write wavefront metrics report: %1").arg(output).toStdString());
  }

  const QByteArray bytes = QJsonDocument(report).toJson(QJsonDocument::Indented);
  if (file.write(bytes) != bytes.size()) {
    throw std::runtime_error(
      QString("Unable to write wavefront metrics report: %1").arg(output).toStdString());
  }
}

void Renderer::writeRenderGraphAOVOutputs(const engine::graph::RenderGraphExecutionTrace& trace,
                                          const engine::graph::RenderIntent& intent) const {
  for (const auto& aovOutput : m_renderGraphAOVOutputs) {
    const auto* aov = engine::graph::renderAOVDefinition(aovOutput.viewMode);
    if (!aov) {
      throw std::runtime_error("Render graph AOV output view is not supported");
    }
    const std::string resourceId =
      aovOutput.viewMode == intent.defaultViewMode ? "main_color" : aov->previewColorResourceId();
    const auto snapshots = trace.outputSnapshotsForResource(resourceId);
    if (snapshots.empty()) {
      throw std::runtime_error("Render graph AOV output resource '" + resourceId +
                               "' was not written");
    }

    const auto* snapshot = snapshots.back();
    if (!snapshot->hasColorPreview()) {
      throw std::runtime_error("Render graph AOV output resource '" + resourceId +
                               "' has no color preview");
    }

    const QImage image = colorBufferToImage(snapshot->colorPreview());
    if (!image.save(aovOutput.output)) {
      throw std::runtime_error(QString("Unable to write render graph AOV output image: %1")
                                 .arg(aovOutput.output)
                                 .toStdString());
    }
  }
}

std::shared_ptr<Buffer<Colord>>
Renderer::loadRenderGraphColorInput(const RenderGraphImageInput& input) const {
  QImage image(input.input);
  if (image.isNull()) {
    throw std::runtime_error(
      QString("Unable to read render graph color input '%1' for resource '%2'")
        .arg(input.input, QString::fromStdString(input.resourceId))
        .toStdString());
  }

  const QImage rgb = image.convertToFormat(QImage::Format_RGB32);
  auto buffer = std::make_shared<Buffer<Colord>>(rgb.width(), rgb.height());
  for (int y = 0; y != rgb.height(); ++y) {
    for (int x = 0; x != rgb.width(); ++x) {
      const QRgb pixel = rgb.pixel(x, y);
      (*buffer)[y][x] =
        Colord(static_cast<double>(qRed(pixel)) / 255.0, static_cast<double>(qGreen(pixel)) / 255.0,
               static_cast<double>(qBlue(pixel)) / 255.0);
    }
  }
  return buffer;
}

std::shared_ptr<Buffer<double>>
Renderer::loadRenderGraphDepthInput(const RenderGraphImageInput& input) const {
  QImage image(input.input);
  if (image.isNull()) {
    throw std::runtime_error(
      QString("Unable to read render graph depth input '%1' for resource '%2'")
        .arg(input.input, QString::fromStdString(input.resourceId))
        .toStdString());
  }

  const QImage rgb = image.convertToFormat(QImage::Format_RGB32);
  auto buffer = std::make_shared<Buffer<double>>(rgb.width(), rgb.height());
  for (int y = 0; y != rgb.height(); ++y) {
    for (int x = 0; x != rgb.width(); ++x) {
      (*buffer)[y][x] = static_cast<double>(qGray(rgb.pixel(x, y))) / 255.0;
    }
  }
  return buffer;
}

std::shared_ptr<Buffer<std::uint8_t>>
Renderer::loadRenderGraphStencilInput(const RenderGraphImageInput& input) const {
  QImage image(input.input);
  if (image.isNull()) {
    throw std::runtime_error(
      QString("Unable to read render graph stencil input '%1' for resource '%2'")
        .arg(input.input, QString::fromStdString(input.resourceId))
        .toStdString());
  }

  const QImage rgb = image.convertToFormat(QImage::Format_RGB32);
  auto buffer = std::make_shared<Buffer<std::uint8_t>>(rgb.width(), rgb.height());
  for (int y = 0; y != rgb.height(); ++y) {
    for (int x = 0; x != rgb.width(); ++x) {
      (*buffer)[y][x] = static_cast<std::uint8_t>(qGray(rgb.pixel(x, y)));
    }
  }
  return buffer;
}

std::shared_ptr<Buffer<std::uint32_t>>
Renderer::loadRenderGraphIntegerIdInput(const RenderGraphImageInput& input) const {
  QImage image(input.input);
  if (image.isNull()) {
    throw std::runtime_error(
      QString("Unable to read render graph integer-id input '%1' for resource '%2'")
        .arg(input.input, QString::fromStdString(input.resourceId))
        .toStdString());
  }

  const QImage rgb = image.convertToFormat(QImage::Format_RGB32);
  auto buffer = std::make_shared<Buffer<std::uint32_t>>(rgb.width(), rgb.height());
  for (int y = 0; y != rgb.height(); ++y) {
    for (int x = 0; x != rgb.width(); ++x) {
      const QRgb pixel = rgb.pixel(x, y);
      const auto red = static_cast<std::uint32_t>(qRed(pixel));
      const auto green = static_cast<std::uint32_t>(qGreen(pixel));
      const auto blue = static_cast<std::uint32_t>(qBlue(pixel));
      (*buffer)[y][x] = red == green && green == blue ? red : (red << 16u) | (green << 8u) | blue;
    }
  }
  return buffer;
}

void Renderer::bindRenderGraphExternalInputs(engine::graph::GraphRenderEngine& graphEngine) const {
  for (const auto& input : m_renderGraphColorInputs) {
    graphEngine.setExternalColorResource(input.resourceId, loadRenderGraphColorInput(input));
  }
  for (const auto& input : m_renderGraphDepthInputs) {
    graphEngine.setExternalDepthResource(input.resourceId, loadRenderGraphDepthInput(input));
  }
  for (const auto& input : m_renderGraphStencilInputs) {
    graphEngine.setExternalStencilResource(input.resourceId, loadRenderGraphStencilInput(input));
  }
  for (const auto& input : m_renderGraphObjectIdInputs) {
    graphEngine.setExternalObjectIdResource(input.resourceId, loadRenderGraphIntegerIdInput(input));
  }
  for (const auto& input : m_renderGraphMaterialIdInputs) {
    graphEngine.setExternalObjectIdResource(input.resourceId, loadRenderGraphIntegerIdInput(input));
  }
}

QString Renderer::renderGraphOutputPath() const {
  if (!m_renderGraphOut.isEmpty()) {
    return m_renderGraphOut;
  }
  if (m_renderGraphOnly) {
    return m_output;
  }
  return {};
}

std::vector<double> Renderer::renderScene(const Scene& scene, const QString& output) const {
  auto raytracerScene = scene.toRaytracerScene(m_stepPlaybackStyle);
  const auto graphIntent =
    m_renderGraph ? std::optional<engine::graph::RenderIntent>(renderIntent(scene, raytracerScene))
                  : std::nullopt;
  int outputWidth = m_width;
  int outputHeight = m_height;

  std::shared_ptr<render::RenderEngine> engine;
  std::shared_ptr<engine::graph::GraphRenderEngine> graphEngine;
  std::shared_ptr<engine::raster::Rasterizer> directRasterEngine;
  std::shared_ptr<engine::wavefront::WavefrontRaytracer> directWavefrontEngine;
  engine::graph::RenderPlan graphPlan;

  if (m_renderGraph) {
    graphPlan = renderGraphPlan(scene, raytracerScene);
    validateRenderGraphPlan(graphPlan);
    if (usesRasterizer(graphPlan)) {
      for (const auto& warning : rasterRecursiveMaterialFallbackWarnings(*raytracerScene)) {
        std::cerr << warning << '\n';
      }
    }
    applyRenderGraphOutputSize(graphPlan, &outputWidth, &outputHeight);
    const QString graphOutput = renderGraphOutputPath();
    if (!graphOutput.isEmpty()) {
      writeRenderGraphPlan(graphPlan, graphOutput);
    }
  }

  // Engine-agnostic camera setup. Both engines need a camera with a
  // view plane sized to the output buffer; the only engine-specific
  // wiring (recursion depth, threads, sampler) lives on the engine
  // construction below.
  std::shared_ptr<render::Camera> rtCamera;
  if (graphIntent) {
    const auto planCameras = graphPlan.executionCameraRefs();
    if (graphPlan.hasMultipleExecutionCameraRefs()) {
      throw std::runtime_error("Render graph plan requires multiple execution cameras");
    }
    if (!planCameras.empty()) {
      rtCamera = scene.toRaytracerCameraForRenderCameraRef(planCameras.front());
      if (!rtCamera) {
        throw std::runtime_error("Render graph plan camera '" + planCameras.front().displayText() +
                                 "' does not resolve to a scene camera");
      }
    }
    const auto frameIntent = graphIntent->withWholeFrameOverridesApplied();
    if (!rtCamera) {
      rtCamera = scene.toRaytracerCameraForRenderIntent(*graphIntent);
    }
    if (!rtCamera && frameIntent.defaultCamera) {
      const auto cameraRef = *frameIntent.defaultCamera;
      throw std::runtime_error("Render graph camera '" + cameraRef.displayText() +
                               "' does not resolve to a scene camera");
    }
  } else if (auto camera = scene.activeCamera()) {
    rtCamera = camera->toRaytracer();
  }
  if (!rtCamera && !graphIntent) {
    qWarning("No camera found. Defaulting to Pinhole camera looking at the origin");
  }

  const bool rasterMetricsRequested = !m_rasterMetricsOut.isEmpty() || m_rasterMetricsSummary;
  const bool wavefrontMetricsRequested =
    !m_wavefrontMetricsOut.isEmpty() || m_wavefrontMetricsSummary;
  const bool wavefrontSampleStddevRequested =
    !m_wavefrontSampleStddevOut.isEmpty() || !m_wavefrontSampleStddevColorOut.isEmpty();

  if (m_renderGraph) {
    graphEngine = rtCamera
                    ? std::make_shared<engine::graph::GraphRenderEngine>(rtCamera, raytracerScene)
                    : std::make_shared<engine::graph::GraphRenderEngine>(raytracerScene);
    for (const Camera* camera : scene.cameras()) {
      if (!camera->id().isEmpty()) {
        graphEngine->setSceneCamera(camera->id().toStdString(), camera->toRaytracer());
      }
    }
    graphEngine->setIntent(*graphIntent);
    graphEngine->setSceneAnalysis(renderGraphSceneAnalysis(scene, raytracerScene));
    graphEngine->setPlan(graphPlan);
    bindRenderGraphExternalInputs(*graphEngine);
    graphEngine->setExecutionTraceEnabled(!m_renderGraphTraceOut.isEmpty() ||
                                          !m_renderGraphAOVOutputs.empty() ||
                                          rasterMetricsRequested || wavefrontMetricsRequested);
    engine = graphEngine;
  } else if (m_engine == "wireframe") {
    auto wireframe = std::make_shared<engine::wireframe::Wireframe>(raytracerScene);
    if (rtCamera)
      wireframe->setCamera(rtCamera);
    wireframe->setLod(m_wireframeLod);
    engine = wireframe;
  } else if (m_engine == "raster") {
    for (const auto& warning : rasterRecursiveMaterialFallbackWarnings(*raytracerScene)) {
      std::cerr << warning << '\n';
    }
    auto raster = std::make_shared<engine::raster::Rasterizer>(raytracerScene);
    if (rtCamera)
      raster->setCamera(rtCamera);
    rasterBeautyPassState(commandLinePostProcessAA(), true, true).applyTo(*raster);
    directRasterEngine = raster;
    engine = raster;
  } else if (m_engine == "wavefront" || (m_tracingBackendSet && !pathTracingRequested()) ||
             (pathTracerEngineSelected() && !scalarPathTracingScheduleSelected())) {
    auto wavefront = std::make_shared<engine::wavefront::WavefrontRaytracer>(raytracerScene);
    wavefront->setMaximumRecursionDepth(m_maximumRecursionDepth);
    if (pathTracingRequested()) {
      auto pt = std::make_unique<render::PathTracingIntegrator>();
      pt->setMaximumRecursionDepth(m_maximumRecursionDepth);
      if (m_pathTracerRussianRouletteDepthSet)
        pt->setRussianRouletteDepth(m_pathTracerRussianRouletteDepth);
      if (m_pathTracerDirectLightSamplesSet)
        pt->setDirectLightSamples(m_pathTracerDirectLightSamples);
      wavefront->setIntegrator(std::move(pt));
    }
    if (rtCamera) {
      wavefront->setCamera(rtCamera);
    } else {
      wavefront->camera()->setPosition(Vector3d(0, 0, -5));
    }
    wavefront->camera()->setViewPlane(std::make_shared<render::TiledViewPlane>());
    wavefront->camera()->viewPlane()->setSampler(sampler());
    wavefront->setMaximumThreads(m_threads);
    wavefront->setQueueSize(rayFamilyQueueSize());
    if (m_samplingSeed)
      wavefront->setSamplingSeed(*m_samplingSeed);
    if (m_wavefrontIntersectionBackendSet) {
      wavefront->setIntersectionBackend(render::WavefrontIntersectionBackendChoice::fromString(
        m_wavefrontIntersectionBackend.toStdString()));
    }
    if (m_tracingBackendSet) {
      wavefront->setIntersectionBackend(
        render::WavefrontIntersectionBackendChoice::fromString(m_tracingBackend.toStdString()));
    }
    wavefront->setMetricsEnabled(wavefrontMetricsRequested);
    wavefront->setSampleRadianceStddevCaptureEnabled(wavefrontSampleStddevRequested);
    if (m_wavefrontConvergenceSet)
      wavefront->setConvergenceEnabled(m_wavefrontConvergenceEnabled);
    if (m_wavefrontConvergenceActiveFractionSet) {
      wavefront->setConvergenceActiveSampleFractionThreshold(m_wavefrontConvergenceActiveFraction);
    }
    if (m_wavefrontConvergenceRmsDeltaSet)
      wavefront->setConvergenceRadianceDeltaRmsThreshold(m_wavefrontConvergenceRmsDelta);
    if (m_wavefrontAdaptiveSamplingSet)
      wavefront->setAdaptiveSamplingEnabled(m_wavefrontAdaptiveSamplingEnabled);
    if (m_wavefrontAdaptiveSamplingSet && m_wavefrontAdaptiveSamplingEnabled) {
      wavefront->setAdaptiveMinimumSamples(defaultWavefrontAdaptiveMinimumSamples);
      wavefront->setAdaptiveStddevThreshold(defaultWavefrontAdaptiveStddevThreshold);
    }
    if (m_wavefrontAdaptiveMinimumSamplesSet)
      wavefront->setAdaptiveMinimumSamples(m_wavefrontAdaptiveMinimumSamples);
    if (m_wavefrontAdaptiveStddevThresholdSet)
      wavefront->setAdaptiveStddevThreshold(m_wavefrontAdaptiveStddevThreshold);
    const int denoiseRadius = !m_wavefrontDenoiseRadiusSet && (m_wavefrontDenoiser == "bilateral" ||
                                                               m_wavefrontDenoiseColorSigmaSet)
                                ? 2
                                : m_wavefrontDenoiseRadius;
    if (m_wavefrontDenoiserSet && m_wavefrontDenoiser == "none") {
      wavefront->clearDenoiser();
    } else if (m_wavefrontDenoiserSet && m_wavefrontDenoiser == "bilateral") {
      wavefront->setDenoiser(
        std::make_unique<render::BilateralDenoiser>(denoiseRadius, m_wavefrontDenoiseColorSigma));
    } else if (m_wavefrontDenoiserSet && m_wavefrontDenoiser == "box") {
      wavefront->setDenoiser(std::make_unique<render::BoxDenoiser>(m_wavefrontDenoiseRadius));
    } else if (m_wavefrontDenoiseColorSigmaSet) {
      wavefront->setDenoiser(
        std::make_unique<render::BilateralDenoiser>(denoiseRadius, m_wavefrontDenoiseColorSigma));
    } else if (m_wavefrontDenoiseRadiusSet) {
      wavefront->setDenoiser(std::make_unique<render::BoxDenoiser>(m_wavefrontDenoiseRadius));
    }
    directWavefrontEngine = wavefront;
    engine = wavefront;
  } else {
    auto rt = std::make_shared<engine::raytracer::Raytracer>(raytracerScene);
    rt->setMaximumRecursionDepth(m_maximumRecursionDepth);
    if (pathTracingRequested()) {
      auto pt = std::make_unique<render::PathTracingIntegrator>();
      pt->setMaximumRecursionDepth(m_maximumRecursionDepth);
      if (m_pathTracerRussianRouletteDepthSet)
        pt->setRussianRouletteDepth(m_pathTracerRussianRouletteDepth);
      if (m_pathTracerDirectLightSamplesSet)
        pt->setDirectLightSamples(m_pathTracerDirectLightSamples);
      rt->setIntegrator(std::move(pt));
    }
    if (rtCamera) {
      rt->setCamera(rtCamera);
    } else {
      rt->camera()->setPosition(Vector3d(0, 0, -5));
    }
    // We don't need a fancy view plane, so optimize the active direct-render camera
    // after scene camera selection.
    rt->camera()->setViewPlane(std::make_shared<render::TiledViewPlane>());
    rt->camera()->viewPlane()->setSampler(sampler());
    rt->setMaximumThreads(m_threads);
    rt->setQueueSize(rayFamilyQueueSize());
    if (m_samplingSeed)
      rt->setSamplingSeed(*m_samplingSeed);
    engine = rt;
  }

  if (auto tonemap = render::TonemapFactory::self().createShared(m_tonemap.toStdString())) {
    engine->setTonemap(tonemap);
  } else {
    qWarning("Unknown tonemap %s; falling back to Linear.", qPrintable(m_tonemap));
  }
  engine->setProgressiveDisplayEnabled(false);

  Buffer<unsigned int> buffer(outputWidth, outputHeight);
  std::vector<double> timings;
  timings.reserve(static_cast<std::size_t>(m_repeat));
  QJsonArray rasterMetricRuns;
  QJsonArray wavefrontMetricRuns;
  const WavefrontMetricsFormatter wavefrontMetrics;
  for (int i = 0; i < m_repeat; ++i) {
    const auto start = Clock::now();
    engine->render(buffer);
    const auto end = Clock::now();
    timings.push_back(elapsedMilliseconds(start, end));

    if (!m_rasterMetricsOut.isEmpty() || m_rasterMetricsSummary) {
      QJsonObject run;
      run["run"] = i + 1;
      if (directRasterEngine) {
        const QJsonObject metrics =
          engine::raster::rasterRenderMetricsToJson(directRasterEngine->lastMetrics());
        run["metrics"] = metrics;
        if (m_rasterMetricsSummary) {
          printRasterMetricsSummary(i + 1, QString(), metrics);
        }
      } else if (graphEngine) {
        auto trace = graphEngine->lastExecutionTrace();
        QJsonArray passes;
        if (trace) {
          for (const auto& passTrace : trace->passes()) {
            if (!isRasterMetricsObject(passTrace.metadata())) {
              continue;
            }
            QJsonObject pass;
            pass["pass"] = QString::fromStdString(passTrace.passId());
            pass["metrics"] = passTrace.metadata();
            passes.push_back(pass);
            if (m_rasterMetricsSummary) {
              printRasterMetricsSummary(i + 1, pass.value("pass").toString(), passTrace.metadata());
            }
          }
        }
        run["passes"] = passes;
      }
      rasterMetricRuns.push_back(run);
    }

    if (!m_wavefrontMetricsOut.isEmpty() || m_wavefrontMetricsSummary) {
      QJsonObject run;
      run["run"] = i + 1;
      if (directWavefrontEngine) {
        const QJsonObject metrics = directWavefrontEngine->lastMetrics().toJson();
        run["metrics"] = metrics;
        if (m_wavefrontMetricsSummary) {
          wavefrontMetrics.printSummary(i + 1, QString(), metrics);
        }
      } else if (graphEngine) {
        auto trace = graphEngine->lastExecutionTrace();
        QJsonArray passes;
        if (trace) {
          for (const auto& passTrace : trace->passes()) {
            if (!wavefrontMetrics.isMetricsObject(passTrace.metadata())) {
              continue;
            }
            QJsonObject pass;
            pass["pass"] = QString::fromStdString(passTrace.passId());
            pass["metrics"] = passTrace.metadata();
            passes.push_back(pass);
            if (m_wavefrontMetricsSummary) {
              wavefrontMetrics.printSummary(i + 1, pass.value("pass").toString(),
                                            passTrace.metadata());
            }
          }
        }
        run["passes"] = passes;
      }
      wavefrontMetricRuns.push_back(run);
    }

    if (graphEngine && (m_rasterMetricsSummary || m_wavefrontMetricsSummary)) {
      auto trace = graphEngine->lastExecutionTrace();
      if (trace) {
        for (const auto& passTrace : trace->passes()) {
          if (!isIntersectionServiceMetricsObject(passTrace.metadata())) {
            continue;
          }
          printIntersectionServiceSummary(i + 1, QString::fromStdString(passTrace.passId()),
                                          passTrace.metadata());
        }
      }
    }
  }

  if (!m_renderGraphTraceOut.isEmpty() || !m_renderGraphAOVOutputs.empty()) {
    if (!graphEngine) {
      throw std::runtime_error("render graph execution outputs require graph rendering");
    }
    auto trace = graphEngine->lastExecutionTrace();
    if (!trace) {
      throw std::runtime_error("Render graph trace was not recorded");
    }
    if (!m_renderGraphTraceOut.isEmpty()) {
      writeRenderGraphTrace(*trace, m_renderGraphTraceOut);
    }
    if (!m_renderGraphAOVOutputs.empty()) {
      writeRenderGraphAOVOutputs(*trace, renderIntent(scene, raytracerScene));
    }
  }

  if (!m_rasterMetricsOut.isEmpty() || m_rasterMetricsSummary) {
    if (rasterMetricRuns.empty()) {
      throw std::runtime_error("Raster metrics were requested but no raster render ran");
    }
    bool hasMetrics = false;
    for (const auto& value : rasterMetricRuns) {
      const QJsonObject run = value.toObject();
      hasMetrics = hasMetrics || run.contains("metrics") || !run.value("passes").toArray().empty();
    }
    if (!hasMetrics) {
      throw std::runtime_error("Raster metrics were requested but no raster pass produced metrics");
    }
  }

  if (!m_rasterMetricsOut.isEmpty()) {
    writeRasterMetricsReport(rasterMetricRuns, m_rasterMetricsOut);
  }

  if (!m_wavefrontMetricsOut.isEmpty() || m_wavefrontMetricsSummary) {
    if (wavefrontMetricRuns.empty()) {
      throw std::runtime_error("Wavefront metrics were requested but no wavefront render ran");
    }
    bool hasMetrics = false;
    for (const auto& value : wavefrontMetricRuns) {
      const QJsonObject run = value.toObject();
      hasMetrics = hasMetrics || run.contains("metrics") || !run.value("passes").toArray().empty();
    }
    if (!hasMetrics) {
      throw std::runtime_error(
        "Wavefront metrics were requested but no wavefront pass produced metrics");
    }
  }

  if (!m_wavefrontMetricsOut.isEmpty()) {
    writeWavefrontMetricsReport(wavefrontMetricRuns, m_wavefrontMetricsOut);
  }

  if (!m_wavefrontSampleStddevOut.isEmpty() && directWavefrontEngine) {
    const auto sampleStddev = directWavefrontEngine->lastSampleRadianceStddev();
    if (!sampleStddev) {
      throw std::runtime_error(
        "Wavefront render did not produce a sample standard-deviation image");
    }
    const QImage sampleStddevImage = scalarBufferToImage(*sampleStddev);
    if (!sampleStddevImage.save(m_wavefrontSampleStddevOut)) {
      throw std::runtime_error(
        QString("Unable to write wavefront sample standard-deviation image: %1")
          .arg(m_wavefrontSampleStddevOut)
          .toStdString());
    }
  }

  if (!m_wavefrontSampleStddevColorOut.isEmpty() && directWavefrontEngine) {
    const auto sampleStddevColor = directWavefrontEngine->lastSampleRadianceStddevColor();
    if (!sampleStddevColor) {
      throw std::runtime_error(
        "Wavefront render did not produce a color sample standard-deviation image");
    }
    const QImage sampleStddevColorImage = normalizedColorBufferToImage(*sampleStddevColor);
    if (!sampleStddevColorImage.save(m_wavefrontSampleStddevColorOut)) {
      throw std::runtime_error(
        QString("Unable to write wavefront color sample standard-deviation image: %1")
          .arg(m_wavefrontSampleStddevColorOut)
          .toStdString());
    }
  }

  QImage image = bufferToImage(buffer);

  if (!image.save(output))
    throw std::runtime_error(QString("Unable to write output image: %1").arg(output).toStdString());

  return timings;
}

void Renderer::render() const {
  auto scene = loadScene();

  if (m_animation) {
    renderAnimation(*scene);
    return;
  }

  if (m_stepSelectionSet && m_stepSelection.mode == CommandLineStepMode::Sequence) {
    renderStepSequence(*scene);
    return;
  }

  if (m_frameSet)
    scene->evaluateAnimationAtFrame(m_frame);

  if (m_stepSelectionSet) {
    auto selection = commandLineStepVisibilitySelection(m_stepSelection);
    if (m_stepPlaybackStyle.ghostPrevious && m_stepPlaybackStyle.activeStep)
      selection = StepVisibilitySelection::cumulativeThrough(*m_stepPlaybackStyle.activeStep);
    validateStepSelection(*scene, selection);
    applyStepVisibilitySelection(*scene, selection);
  }

  if (m_renderGraphOnly) {
    const auto raytracerScene = scene->toRaytracerScene(m_stepPlaybackStyle);
    const auto plan = renderGraphPlan(*scene, raytracerScene);
    validateRenderGraphPlan(plan);
    writeRenderGraphPlan(plan, renderGraphOutputPath());
    return;
  }

  const auto timings = renderScene(*scene, m_output);
  if (m_timing || m_repeat > 1) {
    printTimings(timings);
  }
}

void Renderer::validateStepSelection(const Scene& scene,
                                     const StepVisibilitySelection& selection) const {
  std::set<int> steps;
  collectStepIndices(scene, &steps);
  if (steps.empty()) {
    throw std::runtime_error(
      "Step selection requires at least one group with integer stepIndex metadata");
  }

  if (!selectionMatchesAnyStep(steps, selection)) {
    throw std::runtime_error("Step selection matches no group with stepIndex metadata");
  }
}

std::vector<int> Renderer::sequenceSteps(const Scene& scene) const {
  std::set<int> stepSet;
  collectStepIndices(scene, &stepSet);
  if (stepSet.empty()) {
    throw std::runtime_error(
      "Step sequence requires at least one group with integer stepIndex metadata");
  }

  std::vector<int> steps;
  for (int step : stepSet) {
    if (!m_stepSelection.rangeSet ||
        (step >= m_stepSelection.firstStep && step <= m_stepSelection.lastStep)) {
      steps.push_back(step);
    }
  }

  if (steps.empty()) {
    throw std::runtime_error("Step sequence range matches no group with stepIndex metadata");
  }

  return steps;
}

void Renderer::renderStepSequence(const Scene& scene) const {
  QString placeholderError;
  if (!hasFramePlaceholder(m_output, &placeholderError)) {
    placeholderError.replace("Animation output", "Step sequence output");
    throw std::runtime_error(placeholderError.toStdString());
  }

  const auto steps = sequenceSteps(scene);
  std::vector<double> stepTimings;
  stepTimings.reserve(steps.size());

  for (std::size_t i = 0; i < steps.size(); ++i) {
    const int step = steps[i];
    auto evaluatedScene = loadScene();
    if (m_frameSet) {
      evaluatedScene->evaluateAnimationAtFrame(m_frame);
    }
    applyStepVisibilitySelection(*evaluatedScene, StepVisibilitySelection::cumulativeThrough(step));

    const auto output = outputForFrame(step);
    const auto timings = renderScene(*evaluatedScene, output);
    stepTimings.push_back(timings.front());

    std::cout << "step " << (i + 1) << "/" << steps.size() << " number=" << step
              << " output=" << output.toStdString() << " render_ms=" << std::fixed
              << std::setprecision(3) << timings.front() << '\n';
  }

  if (m_timing) {
    printTimings(stepTimings);
  }
}

void Renderer::renderAnimation(const Scene& scene) const {
  const auto* timeline = scene.animation();
  if (!timeline)
    throw std::runtime_error("Animation rendering requires a scene animation block");

  const int startFrame = m_frameStartSet ? m_frameStart : timeline->startFrame();
  const int endFrame = m_frameEndSet ? m_frameEnd : timeline->endFrame();
  const double fps = m_fpsSet ? m_fps : timeline->fps();
  if (endFrame < startFrame)
    throw std::runtime_error("Frame end must be greater than or equal to frame start");

  QString placeholderError;
  if (!hasFramePlaceholder(m_output, &placeholderError))
    throw std::runtime_error(placeholderError.toStdString());

  std::vector<double> frameTimings;
  frameTimings.reserve(static_cast<std::size_t>(endFrame - startFrame + 1));

  for (int frame = startFrame; frame <= endFrame; ++frame) {
    auto evaluatedScene = scene.evaluatedAtFrame(frame);
    const auto output = outputForFrame(frame);
    const auto timings = renderScene(*evaluatedScene, output);
    frameTimings.push_back(timings.front());

    std::cout << "frame " << (frame - startFrame + 1) << "/" << (endFrame - startFrame + 1)
              << " number=" << frame << " fps=" << fps << " output=" << output.toStdString()
              << " render_ms=" << std::fixed << std::setprecision(3) << timings.front() << '\n';
  }

  if (m_timing) {
    printTimings(frameTimings);
  }
}

QString Renderer::outputForFrame(int frame) const {
  const auto pattern = m_output.toStdString();
  const int size = std::snprintf(nullptr, 0, pattern.c_str(), frame);
  if (size < 0)
    throw std::runtime_error("Unable to format animation output filename");

  std::vector<char> formatted(static_cast<std::size_t>(size) + 1);
  std::snprintf(formatted.data(), formatted.size(), pattern.c_str(), frame);
  return QString::fromStdString(formatted.data());
}

bool Renderer::hasFramePlaceholder(const QString& pattern, QString* errorMessage) {
  int placeholderCount = 0;
  const auto text = pattern.toStdString();

  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] != '%')
      continue;

    ++i;
    if (i >= text.size()) {
      *errorMessage = "Animation output pattern has an incomplete printf placeholder";
      return false;
    }
    if (text[i] == '%')
      continue;

    while (i < text.size() && (text[i] == '-' || text[i] == '+' || text[i] == ' ' ||
                               text[i] == '#' || text[i] == '0')) {
      ++i;
    }
    while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) {
      ++i;
    }
    if (i < text.size() && text[i] == '.') {
      ++i;
      while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) {
        ++i;
      }
    }

    if (i >= text.size()) {
      *errorMessage = "Animation output pattern has an incomplete printf placeholder";
      return false;
    }
    if (text[i] == 'd' || text[i] == 'i') {
      ++placeholderCount;
      continue;
    }

    *errorMessage = "Animation output must contain exactly one printf-style signed integer "
                    "placeholder such as %04d";
    return false;
  }

  if (placeholderCount != 1) {
    *errorMessage = "Animation output must contain exactly one printf-style signed integer "
                    "placeholder such as %04d";
    return false;
  }

  return true;
}

bool Renderer::isKnownSampler(const QString& sampler) {
  const std::string samplerClass = sampler.toStdString() + "Sampler";
  const auto identifiers = render::SamplerFactory::self().identifiers();
  return std::find(identifiers.begin(), identifiers.end(), samplerClass) != identifiers.end();
}

std::shared_ptr<render::Sampler> Renderer::sampler() const {
  auto samplerClass = m_sampler.toStdString() + "Sampler";
  auto sampler = render::SamplerFactory::self().createShared(samplerClass);
  if (m_samplingSeed) {
    sampler->setup(m_samplesPerPixel, 83, *m_samplingSeed);
  } else {
    sampler->setup(m_samplesPerPixel, 83);
  }

  return sampler;
}

QImage Renderer::bufferToImage(const Buffer<unsigned int>& buffer) const {
  QImage image(buffer.width(), buffer.height(), QImage::Format_RGB32);

  for (int i = 0; i != buffer.width(); ++i) {
    for (int j = 0; j != buffer.height(); ++j) {
      image.setPixel(i, j, buffer[j][i]);
    }
  }

  return image;
}

QImage Renderer::colorBufferToImage(const Buffer<Colord>& buffer) const {
  QImage image(buffer.width(), buffer.height(), QImage::Format_RGB32);

  for (int i = 0; i != buffer.width(); ++i) {
    for (int j = 0; j != buffer.height(); ++j) {
      image.setPixel(i, j, buffer[j][i].rgb());
    }
  }

  return image;
}

QImage Renderer::scalarBufferToImage(const Buffer<double>& buffer) const {
  double maximum = 0.0;
  for (int y = 0; y != buffer.height(); ++y) {
    for (int x = 0; x != buffer.width(); ++x) {
      const double value = buffer[y][x];
      if (std::isfinite(value)) {
        maximum = std::max(maximum, value);
      }
    }
  }

  QImage image(buffer.width(), buffer.height(), QImage::Format_RGB32);
  for (int y = 0; y != buffer.height(); ++y) {
    for (int x = 0; x != buffer.width(); ++x) {
      const double value = buffer[y][x];
      const double normalized =
        maximum > 0.0 && std::isfinite(value) ? std::clamp(value / maximum, 0.0, 1.0) : 0.0;
      const int gray = static_cast<int>(std::round(normalized * 255.0));
      image.setPixel(x, y, qRgb(gray, gray, gray));
    }
  }
  return image;
}

QImage Renderer::normalizedColorBufferToImage(const Buffer<Colord>& buffer) const {
  double maximum = 0.0;
  for (int y = 0; y != buffer.height(); ++y) {
    for (int x = 0; x != buffer.width(); ++x) {
      const Colord value = buffer[y][x];
      if (std::isfinite(value.r()))
        maximum = std::max(maximum, value.r());
      if (std::isfinite(value.g()))
        maximum = std::max(maximum, value.g());
      if (std::isfinite(value.b()))
        maximum = std::max(maximum, value.b());
    }
  }

  QImage image(buffer.width(), buffer.height(), QImage::Format_RGB32);
  for (int y = 0; y != buffer.height(); ++y) {
    for (int x = 0; x != buffer.width(); ++x) {
      const Colord value = buffer[y][x];
      const double scale = maximum > 0.0 ? 255.0 / maximum : 0.0;
      const auto normalized = [scale](double channel) {
        const double scaled = std::isfinite(channel) ? channel * scale : 0.0;
        return static_cast<int>(std::round(std::clamp(scaled, 0.0, 255.0)));
      };
      image.setPixel(x, y,
                     qRgb(normalized(value.r()), normalized(value.g()), normalized(value.b())));
    }
  }
  return image;
}

Renderer::CommandLineParseResult Renderer::parseCommandLine(QString* errorMessage) {
  parser.setApplicationDescription(
    QCoreApplication::translate("rendercli", "Command line renderer."));
  const QCommandLineOption helpOption = parser.addHelpOption();
  const QCommandLineOption versionOption = parser.addVersionOption();

  parser.addOptions(
    {{"width", "Output image width", "width"},
     {"height", "Output image height", "height"},
     {"depth", "Maximum recursion depth", "depth"},
     {"ldraw_library_root",
      "LDraw parts library root used for LDraw authoring imports and direct LDraw input",
      "directory"},
     {"ldraw_input", "Treat the input file as an LDraw .ldr/.dat/.mpd model and build a scene"},
     {"ldraw_preserve_hierarchy",
      "Preserve LDraw STEP and MPD submodel structure as generic scene groups"},
     {"ldraw_scale", "Scale applied to direct LDraw input geometry", "scale"},
     {"ldraw_coordinate_conversion",
      "Coordinate conversion for direct LDraw input (none, ldraw_to_raytracer; default none)",
      "mode"},
     {"ldraw_flatten_hierarchy", "Flatten direct LDraw subfile hierarchy where supported"},
     {"ldraw_normals", "Normal mode for direct LDraw input (flat, smooth)", "mode"},
     {"ldraw_no_edge_overlays", "Do not import LDraw type-2 edge overlay lines"},
     {"ldraw_max_recursion", "Maximum LDraw subfile recursion depth", "depth"},
     {"ldraw_missing_part_policy", "Policy for unresolved LDraw subfiles (error, skip)", "policy"},
     {"ldraw-background-color", "Background color for direct LDraw imports (name or hex)", "color"},
     {"sampler", "Sampler type", "sampler"},
     {"sample_stream_mode", "Sample stream mode (sampler, gpu_sample_stream)", "mode"},
     {"samples_per_pixel", "Samples per pixel", "samples"},
     {"sampling_seed", "Deterministic render sampling seed for ray-family engines", "seed"},
     {"pathtracer_russian_roulette_depth",
      "Path-tracer bounce depth where Russian-roulette termination starts", "depth"},
     {"pathtracer_direct_light_samples", "Path-tracer direct-light samples per surface hit",
      "samples"},
     {"gpu_primary_sample_chunk_size",
      "GPU path-tracer primary sample chunk size; 0 lets the backend choose depth-aware "
      "sample/tile chunks",
      "samples"},
     {{"j", "threads"}, "Number of threads", "threads"},
     {"queue_size", "Explicit queue size for thread pool; raster defaults to automatic",
      "queue_size"},
     {"tonemap", "Tonemap operator (Linear, Reinhard, ACES)", "tonemap"},
     {"import_format", "Import input with the named scene importer format", "format"},
     {"import_option", "Set an importer option; may be repeated with key=value", "key=value"},
     {"gcode_visualization",
      "G-code color mode (move_type, layer, tool, speed, temperature, extrusion_travel)", "mode"},
     {"gcode_layer", "Render one G-code print layer index", "layer"},
     {"gcode_cumulative_layers", "Render G-code print layers cumulatively through --gcode_layer"},
     {"gcode_hide_travel", "Hide G-code travel moves during import"},
     {"engine", "Render engine (raytracer, pathtracer, wavefront, wireframe, raster)", "engine"},
     {"integrator", "Raytracer integrator (whitted, pathtracer)", "integrator"},
     {"path_tracing_schedule", "Path tracing schedule (wavefront, scalar)", "schedule"},
     {"tracing_backend", "Tracing execution backend for graph ray-family passes (auto, cpu, gpu)",
      "backend"},
     {"tracing_execution", "Tracing execution preference (auto, cpu, hybrid, gpu)", "mode"},
     {"wavefront_intersection_backend", "Wavefront ray-scene intersection backend (auto, cpu, gpu)",
      "backend"},
     {"wavefront_convergence", "Enable wavefront path-batch convergence stopping"},
     {"wavefront_no_convergence", "Disable wavefront path-batch convergence stopping"},
     {"wavefront_convergence_active_fraction",
      "Wavefront convergence active-sample fraction threshold in 0..1", "fraction"},
     {"wavefront_convergence_rms_delta",
      "Wavefront convergence per-depth RMS radiance delta threshold", "delta"},
     {"wavefront_adaptive_sampling", "Enable wavefront per-pixel adaptive sampling"},
     {"wavefront_no_adaptive_sampling", "Disable wavefront per-pixel adaptive sampling"},
     {"wavefront_adaptive_min_samples", "Wavefront adaptive sampling minimum samples per pixel",
      "samples"},
     {"wavefront_adaptive_stddev_threshold",
      "Wavefront adaptive sampling per-pixel radiance standard-deviation threshold", "threshold"},
     {"wavefront_denoiser", "Wavefront denoiser (none, box, bilateral)", "denoiser"},
     {"wavefront_denoise_radius", "Wavefront denoiser radius in pixels", "radius"},
     {"wavefront_denoise_color_sigma", "Wavefront bilateral denoiser color sigma", "sigma"},
     {"render_graph", "Render through the compiled render graph; this is the default"},
     {{"direct_engine", "no_render_graph"},
      "Bypass the render graph and render with the selected engine directly"},
     {"render_graph_only", "Compile/export the render graph and skip image rendering"},
     {"render_graph_format", "Render graph export format (text, dot, json)", "format"},
     {"render_graph_out", "Write the compiled render graph to a file", "file"},
     {"render_graph_in", "Load a JSON render graph plan instead of compiling one", "file"},
     {"render_graph_trace_out", "Write the executed render graph trace to a JSON file", "file"},
     {"raster_metrics_out", "Write aggregate raster render metrics to a JSON file", "file"},
     {"raster_metrics_summary", "Print aggregate raster render metrics to stdout"},
     {"wavefront_metrics_out", "Write aggregate wavefront render metrics to a JSON file", "file"},
     {"wavefront_metrics_summary", "Print aggregate wavefront render metrics to stdout"},
     {"wavefront_sample_stddev_out",
      "Write a grayscale per-pixel wavefront sample radiance standard-deviation image", "file"},
     {"wavefront_sample_stddev_color_out",
      "Write a color per-channel wavefront sample radiance standard-deviation image", "file"},
     {"render_graph_aov_out",
      "Write an executed graph AOV preview image; repeat with view=file for multiple AOVs",
      "view=file"},
     {"render_graph_color_in",
      "Bind an imported/history graph color resource from an image file as resource=file",
      "resource=file"},
     {"render_graph_depth_in",
      "Bind an imported/history graph depth resource from a grayscale image file as resource=file",
      "resource=file"},
     {"render_graph_stencil_in",
      "Bind an imported/history graph stencil resource from an image file as resource=file",
      "resource=file"},
     {"render_graph_object_id_in",
      "Bind an imported/history graph object-id resource from an image file as resource=file",
      "resource=file"},
     {"render_graph_material_id_in",
      "Bind an imported/history graph material-id resource from an image file as resource=file",
      "resource=file"},
     {"render_graph_executor",
      "Override graph intent executor (raytracer, pathtracer, wavefront, rasterizer, wireframe)",
      "executor"},
     {"render_graph_view",
      "Override graph intent view mode (default, beauty, wireframe, depth, stencil, normal, "
      "stencil_composite, object_id, material_id, world_position, sample_stddev, "
      "sample_stddev_color, raster_*_count, hybrid_visibility)",
      "mode"},
     {"render_graph_camera", "Override graph intent camera with a scene camera id", "camera_id"},
     {"render_graph_shading_profile", "Override graph intent shading profile", "profile"},
     {"render_graph_shading_parameter",
      "Set a default graph shading profile parameter; may be repeated with key=value", "key=value"},
     {"render_graph_view_override",
      "Add a render-intent view override as selector,key=value; selectors include all, "
      "tag:value, object_name:value, object_id:value, layer:value, and material_role:value; "
      "fields include executor, view, camera, shading_profile, and parameter:name",
      "selector,key=value"},
     {"render_graph_wireframe_overlay", "Add a wireframe overlay pass to the compiled graph"},
     {"render_graph_curve_overlay", "Add a curve center-line overlay pass to the compiled graph"},
     {"disable_pass", "Disable a render graph pass id; may be repeated or comma-separated", "id"},
     {"disable_pass_kind",
      "Disable render graph pass kind (beauty, shadow, overlay, composite, tonemap, postprocess, "
      "visibility, aov, debug, custom)",
      "kind"},
     {"disable_executor",
      "Disable render graph executor (raytracer, rasterizer, wireframe, composite, postprocess)",
      "executor"},
     {"disable_feature", "Disable a render graph feature; may be repeated or comma-separated",
      "feature"},
     {"lod", "Tessellation level of detail for wireframe / raster engines", "lod"},
     {"raster_backend", "Rasterizer backend for graph raster passes (cpu, opengl, gpu)", "backend"},
     {"cull", "Rasterizer face culling mode (both, back, front)", "mode"},
     {"raster_culling", "Request graph-visible raster visibility culling (off, on, auto)", "mode"},
     {"raster_tessellation_quality",
      "Rasterizer tessellation quality preset (preview, balanced, final)", "quality"},
     {"raster_max_screen_space_error",
      "Rasterizer maximum tessellation screen-space error in pixels", "pixels"},
     {"depth_prepass", "Request measured raster depth prepass (off, on, auto)", "mode"},
     {"msaa", "Rasterizer MSAA samples (1, 2, 4, or 8)", "samples"},
     {"msaa_shading", "Rasterizer MSAA shading mode (per_sample, per_fragment)", "mode"},
     {"post_aa", "Post-process anti-aliasing (none, fxaa, smaa; taa is rasterizer-only)", "mode"},
     {"color_write_mask", "Rasterizer color-write mask (rgb, r, g, b, rg, rb, gb, none)", "mask"},
     {"blend", "Enable rasterizer fixed-function blending"},
     {"blend_src", "Rasterizer source blend factor", "factor"},
     {"blend_dst", "Rasterizer destination blend factor", "factor"},
     {"blend_op", "Rasterizer blend operation (add, subtract, reverse_subtract, min, max)", "op"},
     {"blend_constant_color", "Rasterizer blend constant color as r,g,b in 0..1", "color"},
     {"blend_constant_alpha", "Rasterizer blend constant alpha in 0..1", "alpha"},
     {"alpha_test", "Enable rasterizer alpha test"},
     {"alpha_func",
      "Rasterizer alpha test function (never, less, equal, less_equal, greater, greater_equal, "
      "not_equal, always)",
      "func"},
     {"alpha_ref", "Rasterizer alpha test reference in 0..1", "alpha"},
     {"viewport", "Rasterizer viewport rectangle as x,y,width,height", "rect"},
     {"scissor", "Rasterizer scissor rectangle as x,y,width,height", "rect"},
     {"depth_bias", "Rasterizer constant depth bias applied before depth test/write", "bias"},
     {"shadow_maps", "Enable rasterizer directional-light shadow maps"},
     {"shadow_map_size", "Rasterizer shadow-map resolution", "pixels"},
     {"shadow_cascades", "Rasterizer directional-light shadow cascade count", "count"},
     {"shadow_cascade_split", "Rasterizer shadow cascade split blend (0=linear, 1=log)", "blend"},
     {"shadow_bias", "Rasterizer shadow-map depth bias", "bias"},
     {"shadow_slope_bias", "Rasterizer slope-scaled shadow-map depth bias", "bias"},
     {"shadow_filter_radius", "Rasterizer shadow filter radius", "radius"},
     {"shadow_filter", "Rasterizer shadow filter (pcf, pcss)", "mode"},
     {"shadow_mode", "Rasterizer preview shadow implementation (shadow_maps, ray_traced)", "mode"},
     {"timing", "Print render-only timing information to stdout"},
     {"frame", "Evaluate the scene animation at the given frame before rendering", "frame"},
     {"step",
      "Evaluate grouped step visibility before rendering: N, single:N, cumulative:N, or "
      "sequence[:FIRST-LAST]",
      "selection"},
     {"step_highlight", "Override active step groups with a highlight material"},
     {"step_ghost_previous", "Keep previous step groups visible with a dim material"},
     {"animation", "Render the scene animation as an image sequence"},
     {"frame_start", "Override the first animation frame", "frame"},
     {"frame_end", "Override the last animation frame", "frame"},
     {"fps", "Override the animation frame rate used for sequence metadata/progress", "fps"},
     {"repeat", "Render the loaded scene N times and print render-only timing statistics",
      "runs"}});

  parser.addPositionalArgument("input",
                               QCoreApplication::translate("main", "Input file to render."));
  parser.addPositionalArgument(
    "output", QCoreApplication::translate(
                "main", "Output image file, or graph file with --render_graph_only."));

  if (!parser.parse(QCoreApplication::arguments())) {
    *errorMessage = parser.errorText();
    return CommandLineError;
  }

  if (parser.isSet(versionOption))
    return CommandLineVersionRequested;

  if (parser.isSet(helpOption))
    return CommandLineHelpRequested;

  if (parser.isSet("width")) {
    const QString widthValue = parser.value("width");
    m_width = widthValue.toInt();
    if (m_width <= 0) {
      *errorMessage = "Width must be > 0";
      return CommandLineError;
    }
    m_widthSet = true;
  }

  if (parser.isSet("height")) {
    const QString heightValue = parser.value("height");
    m_height = heightValue.toInt();
    if (m_height <= 0) {
      *errorMessage = "Height must be > 0";
      return CommandLineError;
    }
    m_heightSet = true;
  }

  if (parser.isSet("depth")) {
    const QString depthValue = parser.value("depth");
    m_maximumRecursionDepth = depthValue.toInt();
    if (m_maximumRecursionDepth <= 0) {
      *errorMessage = "Depth must be > 0";
      return CommandLineError;
    }
    m_maximumRecursionDepthSet = true;
  }

  if (parser.isSet("ldraw_library_root")) {
    m_ldrawLibraryRoot = parser.value("ldraw_library_root").trimmed();
    if (m_ldrawLibraryRoot.isEmpty()) {
      *errorMessage = "LDraw library root must not be empty";
      return CommandLineError;
    }
  }

  if (parser.isSet("ldraw_input")) {
    m_ldrawInput = true;
  }

  if (parser.isSet("ldraw_preserve_hierarchy")) {
    m_ldrawPreserveAuthoringHierarchy = true;
    m_ldrawInput = true;
  }

  if (parser.isSet("ldraw_scale")) {
    bool ok = false;
    m_ldrawScale = parser.value("ldraw_scale").toDouble(&ok);
    if (!ok || !std::isfinite(m_ldrawScale) || m_ldrawScale <= 0.0) {
      *errorMessage = "LDraw scale must be a positive number";
      return CommandLineError;
    }
  }

  if (parser.isSet("ldraw_coordinate_conversion")) {
    m_ldrawCoordinateConversion = parser.value("ldraw_coordinate_conversion").trimmed().toLower();
    const QString normalized = normalizedRasterOption(m_ldrawCoordinateConversion);
    if (normalized != "none" && normalized != "ldrawtoraytracer" && normalized != "raytracer" &&
        normalized != "yup") {
      *errorMessage = "LDraw coordinate conversion must be 'none' or 'ldraw_to_raytracer'";
      return CommandLineError;
    }
    m_ldrawCoordinateConversion =
      normalized == "none" ? QString("none") : QString("ldraw_to_raytracer");
  }

  if (parser.isSet("ldraw_flatten_hierarchy")) {
    m_ldrawPreserveHierarchy = false;
  }

  if (parser.isSet("ldraw_normals")) {
    m_ldrawNormalMode = parser.value("ldraw_normals").trimmed().toLower();
    if (m_ldrawNormalMode != "flat" && m_ldrawNormalMode != "smooth") {
      *errorMessage = "LDraw normals must be 'flat' or 'smooth'";
      return CommandLineError;
    }
  }

  if (parser.isSet("ldraw_no_edge_overlays")) {
    m_ldrawIncludeEdgeOverlays = false;
  }

  if (parser.isSet("ldraw_max_recursion")) {
    bool ok = false;
    m_ldrawMaxRecursion = parser.value("ldraw_max_recursion").toInt(&ok);
    if (!ok || m_ldrawMaxRecursion <= 0) {
      *errorMessage = "LDraw max recursion must be > 0";
      return CommandLineError;
    }
  }

  if (parser.isSet("ldraw_missing_part_policy")) {
    m_ldrawMissingPartPolicy = parser.value("ldraw_missing_part_policy").trimmed().toLower();
    if (m_ldrawMissingPartPolicy != "error" && m_ldrawMissingPartPolicy != "skip") {
      *errorMessage = "LDraw missing part policy must be 'error' or 'skip'";
      return CommandLineError;
    }
  }

  if (parser.isSet("ldraw-background-color")) {
    m_ldrawBackgroundColor = parser.value("ldraw-background-color").trimmed();
    if (m_ldrawBackgroundColor.isEmpty()) {
      *errorMessage = "LDraw background color must not be empty";
      return CommandLineError;
    }
  }

  if (parser.isSet("sampler")) {
    m_sampler = parser.value("sampler");
    if (!isKnownSampler(m_sampler)) {
      *errorMessage = "Sampler must be 'Regular', 'Random', or 'Jittered'";
      return CommandLineError;
    }
    m_samplerSet = true;
  }

  if (parser.isSet("sample_stream_mode")) {
    const QString mode = parser.value("sample_stream_mode").trimmed().toLower();
    if (mode == "sampler" || mode == "sampler_backed") {
      m_sampleStreamMode = QStringLiteral("sampler");
    } else if (mode == "gpu" || mode == "gpu_sample_stream") {
      m_sampleStreamMode = QStringLiteral("gpu_sample_stream");
    } else {
      *errorMessage = "Sample stream mode must be 'sampler' or 'gpu_sample_stream'";
      return CommandLineError;
    }
    m_sampleStreamModeSet = true;
  }

  if (m_samplerSet && m_sampleStreamMode == QStringLiteral("gpu_sample_stream")) {
    *errorMessage = "GPU sample stream mode is incompatible with --sampler";
    return CommandLineError;
  }

  if (parser.isSet("samples_per_pixel")) {
    const QString samplesPerPixelValue = parser.value("samples_per_pixel");
    m_samplesPerPixel = samplesPerPixelValue.toInt();
    if (m_samplesPerPixel <= 0) {
      *errorMessage = "Samples per pixel must be > 0";
      return CommandLineError;
    }
    m_samplesPerPixelSet = true;
  }

  if (parser.isSet("sampling_seed")) {
    bool ok = false;
    const qulonglong seed = parser.value("sampling_seed").toULongLong(&ok);
    if (!ok || seed > maxExactJsonInteger) {
      *errorMessage = "Sampling seed must be a non-negative integer <= 9007199254740991";
      return CommandLineError;
    }
    m_samplingSeed = static_cast<std::uint64_t>(seed);
  }

  if (parser.isSet("pathtracer_russian_roulette_depth")) {
    const QString depthValue = parser.value("pathtracer_russian_roulette_depth");
    m_pathTracerRussianRouletteDepth = depthValue.toInt();
    if (m_pathTracerRussianRouletteDepth <= 0) {
      *errorMessage = "Path tracer Russian roulette depth must be > 0";
      return CommandLineError;
    }
    m_pathTracerRussianRouletteDepthSet = true;
  }

  if (parser.isSet("pathtracer_direct_light_samples")) {
    const QString samplesValue = parser.value("pathtracer_direct_light_samples");
    m_pathTracerDirectLightSamples = samplesValue.toInt();
    if (m_pathTracerDirectLightSamples <= 0) {
      *errorMessage = "Path tracer direct light samples must be > 0";
      return CommandLineError;
    }
    m_pathTracerDirectLightSamplesSet = true;
  }

  if (parser.isSet("gpu_primary_sample_chunk_size")) {
    const QString samplesValue = parser.value("gpu_primary_sample_chunk_size");
    bool ok = false;
    m_gpuPrimarySampleChunkSize = samplesValue.toInt(&ok);
    if (!ok || m_gpuPrimarySampleChunkSize < 0) {
      *errorMessage = "GPU primary sample chunk size must be >= 0";
      return CommandLineError;
    }
    m_gpuPrimarySampleChunkSizeSet = true;
  }

  if (parser.isSet("threads")) {
    const QString threadsValue = parser.value("threads");
    m_threads = threadsValue.toInt();
    if (m_threads <= 0) {
      *errorMessage = "Threads must be > 0";
      return CommandLineError;
    }
    m_threadsSet = true;
  }

  if (parser.isSet("queue_size")) {
    const QString queueSizeValue = parser.value("queue_size");
    m_queueSize = queueSizeValue.toInt();
    if (m_queueSize <= 0) {
      *errorMessage = "Queue size must be > 0";
      return CommandLineError;
    }
    m_queueSizeSet = true;
  }

  if (parser.isSet("tonemap")) {
    m_tonemap = parser.value("tonemap");
  }

  if (parser.isSet("import_format")) {
    m_importFormat = parser.value("import_format").trimmed();
    if (m_importFormat.isEmpty()) {
      *errorMessage = "Import format must not be empty";
      return CommandLineError;
    }
  }

  if (parser.isSet("import_option")) {
    for (const QString& value : parser.values("import_option")) {
      std::pair<QString, QVariant> option;
      if (!parseImportOption(value, &option, errorMessage)) {
        return CommandLineError;
      }
      m_importOptions.setValue(option.first, option.second);
    }
  }

  if (parser.isSet("gcode_visualization")) {
    const QString mode = parser.value("gcode_visualization").trimmed().toLower();
    const QString normalized = normalizedRasterOption(mode);
    if (normalized != "movetype" && normalized != "extrusiontravel" && normalized != "layer" &&
        normalized != "tool" && normalized != "speed" && normalized != "temperature") {
      *errorMessage =
        "G-code visualization must be 'move_type', 'layer', 'tool', 'speed', 'temperature', or "
        "'extrusion_travel'";
      return CommandLineError;
    }
    m_importOptions.setValue("visualization", mode);
    m_gcodeOptionsSet = true;
  }

  if (parser.isSet("gcode_hide_travel")) {
    m_importOptions.setValue("hide_travel", true);
    m_gcodeOptionsSet = true;
  }

  if (parser.isSet("gcode_layer")) {
    bool ok = false;
    m_gcodeLayer = parser.value("gcode_layer").toInt(&ok);
    if (!ok || m_gcodeLayer < 0) {
      *errorMessage = "G-code layer must be a non-negative integer";
      return CommandLineError;
    }
    m_gcodeLayerSet = true;
    m_importOptions.setValue("layer", m_gcodeLayer);
    m_gcodeOptionsSet = true;
  }

  if (parser.isSet("gcode_cumulative_layers")) {
    m_gcodeCumulativeLayers = true;
    m_importOptions.setValue("cumulative_layers", true);
    m_gcodeOptionsSet = true;
  }

  if (parser.isSet("engine")) {
    const QString engine = parser.value("engine").toLower();
    const QString normalizedEngine = normalizedRasterOption(engine);
    if (normalizedEngine != "raytracer" && normalizedEngine != "pathtracer" &&
        normalizedEngine != "pt" && normalizedEngine != "wavefront" &&
        normalizedEngine != "wireframe" && normalizedEngine != "raster") {
      *errorMessage =
        "Engine must be 'raytracer', 'pathtracer', 'wavefront', 'wireframe', or 'raster'";
      return CommandLineError;
    }
    m_engine = normalizedEngine == "pt" ? QStringLiteral("pathtracer") : normalizedEngine;
    m_engineSet = true;
  }

  if (parser.isSet("integrator")) {
    const QString integrator = parser.value("integrator").toLower();
    if (integrator != "whitted" && integrator != "pathtracer" && integrator != "path_tracer" &&
        integrator != "pt") {
      *errorMessage = "Integrator must be 'whitted' or 'pathtracer'";
      return CommandLineError;
    }
    m_integrator = integrator;
    m_integratorSet = true;
  }

  if (parser.isSet("path_tracing_schedule")) {
    const QString schedule = parser.value("path_tracing_schedule").toLower();
    const QString normalizedSchedule = normalizedRasterOption(schedule);
    if (normalizedSchedule != "wavefront" && normalizedSchedule != "scalar") {
      *errorMessage = "Path tracing schedule must be 'wavefront' or 'scalar'";
      return CommandLineError;
    }
    m_pathTracingSchedule = normalizedSchedule;
    m_pathTracingScheduleSet = true;
  }

  if (parser.isSet("tracing_backend")) {
    const QString backend = parser.value("tracing_backend").toLower();
    const QString normalizedBackend = normalizedRasterOption(backend);
    if (normalizedBackend != "auto" && normalizedBackend != "automatic" &&
        normalizedBackend != "cpu" && normalizedBackend != "gpu") {
      *errorMessage = "Tracing backend must be 'auto', 'cpu', or 'gpu'";
      return CommandLineError;
    }
    m_tracingBackend =
      normalizedBackend == "automatic" ? QStringLiteral("auto") : normalizedBackend;
    m_tracingBackendSet = true;
  }

  if (parser.isSet("tracing_execution")) {
    const QString mode = parser.value("tracing_execution").toLower();
    const QString normalizedMode = normalizedRasterOption(mode);
    if (normalizedMode != "auto" && normalizedMode != "automatic" && normalizedMode != "cpu" &&
        normalizedMode != "hybrid" && normalizedMode != "gpu") {
      *errorMessage = "Tracing execution must be 'auto', 'cpu', 'hybrid', or 'gpu'";
      return CommandLineError;
    }
    m_tracingExecution = normalizedMode == "automatic" ? QStringLiteral("auto") : normalizedMode;
    m_tracingExecutionSet = true;
  }

  if (parser.isSet("wavefront_intersection_backend")) {
    const QString backend = parser.value("wavefront_intersection_backend").toLower();
    const QString normalizedBackend = normalizedRasterOption(backend);
    if (normalizedBackend != "auto" && normalizedBackend != "automatic" &&
        normalizedBackend != "cpu" && normalizedBackend != "gpu") {
      *errorMessage = "Wavefront intersection backend must be 'auto', 'cpu', or 'gpu'";
      return CommandLineError;
    }
    m_wavefrontIntersectionBackend =
      normalizedBackend == "automatic" ? QStringLiteral("auto") : normalizedBackend;
    m_wavefrontIntersectionBackendSet = true;
  }

  if (parser.isSet("wavefront_convergence") && parser.isSet("wavefront_no_convergence")) {
    *errorMessage = "Cannot combine --wavefront_convergence with --wavefront_no_convergence";
    return CommandLineError;
  }

  if (parser.isSet("wavefront_convergence")) {
    m_wavefrontConvergenceEnabled = true;
    m_wavefrontConvergenceSet = true;
  }

  if (parser.isSet("wavefront_no_convergence")) {
    m_wavefrontConvergenceEnabled = false;
    m_wavefrontConvergenceSet = true;
  }

  if (parser.isSet("wavefront_convergence_active_fraction")) {
    bool ok = false;
    const double fraction = parser.value("wavefront_convergence_active_fraction").toDouble(&ok);
    if (!ok || !std::isfinite(fraction) || fraction < 0.0 || fraction > 1.0) {
      *errorMessage = "Wavefront convergence active fraction must be a number from 0 to 1";
      return CommandLineError;
    }
    m_wavefrontConvergenceActiveFraction = fraction;
    m_wavefrontConvergenceActiveFractionSet = true;
  }

  if (parser.isSet("wavefront_convergence_rms_delta")) {
    bool ok = false;
    const double threshold = parser.value("wavefront_convergence_rms_delta").toDouble(&ok);
    if (!ok || !std::isfinite(threshold) || threshold < 0.0) {
      *errorMessage = "Wavefront convergence RMS delta must be a non-negative number";
      return CommandLineError;
    }
    m_wavefrontConvergenceRmsDelta = threshold;
    m_wavefrontConvergenceRmsDeltaSet = true;
  }

  if (parser.isSet("wavefront_adaptive_sampling") &&
      parser.isSet("wavefront_no_adaptive_sampling")) {
    *errorMessage = "Cannot combine --wavefront_adaptive_sampling with "
                    "--wavefront_no_adaptive_sampling";
    return CommandLineError;
  }

  if (parser.isSet("wavefront_adaptive_sampling")) {
    m_wavefrontAdaptiveSamplingEnabled = true;
    m_wavefrontAdaptiveSamplingSet = true;
  }

  if (parser.isSet("wavefront_no_adaptive_sampling")) {
    m_wavefrontAdaptiveSamplingEnabled = false;
    m_wavefrontAdaptiveSamplingSet = true;
  }

  if (parser.isSet("wavefront_adaptive_min_samples")) {
    bool ok = false;
    const int samples = parser.value("wavefront_adaptive_min_samples").toInt(&ok);
    if (!ok || samples <= 0) {
      *errorMessage = "Wavefront adaptive minimum samples must be > 0";
      return CommandLineError;
    }
    m_wavefrontAdaptiveMinimumSamples = samples;
    m_wavefrontAdaptiveMinimumSamplesSet = true;
  }

  if (parser.isSet("wavefront_adaptive_stddev_threshold")) {
    bool ok = false;
    const double threshold = parser.value("wavefront_adaptive_stddev_threshold").toDouble(&ok);
    if (!ok || !std::isfinite(threshold) || threshold < 0.0) {
      *errorMessage = "Wavefront adaptive standard-deviation threshold must be a non-negative "
                      "number";
      return CommandLineError;
    }
    m_wavefrontAdaptiveStddevThreshold = threshold;
    m_wavefrontAdaptiveStddevThresholdSet = true;
  }

  if (parser.isSet("wavefront_denoiser")) {
    const QString denoiser = parser.value("wavefront_denoiser").trimmed().toLower();
    const QString normalized = normalizedRasterOption(denoiser);
    if (normalized != "none" && normalized != "off" && normalized != "disabled" &&
        normalized != "box" && normalized != "boxfilter" && normalized != "bilateral" &&
        normalized != "bilateralfilter" && normalized != "colorbilateral") {
      *errorMessage = "Wavefront denoiser must be 'none', 'box', or 'bilateral'";
      return CommandLineError;
    }
    m_wavefrontDenoiser = normalized == "boxfilter" ? QStringLiteral("box")
                          : normalized == "bilateralfilter" || normalized == "colorbilateral"
                            ? QStringLiteral("bilateral")
                            : denoiser;
    if (normalized == "off" || normalized == "disabled")
      m_wavefrontDenoiser = QStringLiteral("none");
    m_wavefrontDenoiserSet = true;
  }

  if (parser.isSet("wavefront_denoise_radius")) {
    bool ok = false;
    const int radius = parser.value("wavefront_denoise_radius").toInt(&ok);
    if (!ok || radius < 0) {
      *errorMessage = "Wavefront denoise radius must be a non-negative integer";
      return CommandLineError;
    }
    m_wavefrontDenoiseRadius = radius;
    m_wavefrontDenoiseRadiusSet = true;
  }

  if (parser.isSet("wavefront_denoise_color_sigma")) {
    bool ok = false;
    const double sigma = parser.value("wavefront_denoise_color_sigma").toDouble(&ok);
    if (!ok || sigma <= 0.0) {
      *errorMessage = "Wavefront denoise color sigma must be positive";
      return CommandLineError;
    }
    m_wavefrontDenoiseColorSigma = sigma;
    m_wavefrontDenoiseColorSigmaSet = true;
  }

  if (parser.isSet("render_graph")) {
    m_renderGraph = true;
  }

  if (parser.isSet("direct_engine")) {
    m_renderGraph = false;
    m_directEngine = true;
  }

  if (parser.isSet("render_graph_only")) {
    m_renderGraph = true;
    m_renderGraphOnly = true;
  }

  if (parser.isSet("render_graph_format")) {
    const QString format = parser.value("render_graph_format").toLower();
    if (format != "text" && format != "dot" && format != "json") {
      *errorMessage = "Render graph format must be 'text', 'dot', or 'json'";
      return CommandLineError;
    }
    m_renderGraphFormat = format;
  }

  if (parser.isSet("render_graph_out")) {
    m_renderGraph = true;
    m_renderGraphOut = parser.value("render_graph_out");
  }

  if (parser.isSet("render_graph_in")) {
    m_renderGraph = true;
    m_renderGraphIn = parser.value("render_graph_in");
  }

  if (parser.isSet("render_graph_trace_out")) {
    m_renderGraph = true;
    m_renderGraphTraceOut = parser.value("render_graph_trace_out");
  }

  if (parser.isSet("raster_metrics_out")) {
    m_rasterMetricsOut = parser.value("raster_metrics_out");
  }

  if (parser.isSet("raster_metrics_summary")) {
    m_rasterMetricsSummary = true;
  }

  if (parser.isSet("wavefront_metrics_out")) {
    m_wavefrontMetricsOut = parser.value("wavefront_metrics_out");
  }

  if (parser.isSet("wavefront_metrics_summary")) {
    m_wavefrontMetricsSummary = true;
  }

  if (parser.isSet("wavefront_sample_stddev_out")) {
    m_wavefrontSampleStddevOut = parser.value("wavefront_sample_stddev_out").trimmed();
    if (m_wavefrontSampleStddevOut.isEmpty()) {
      *errorMessage = "Wavefront sample standard-deviation output path must not be empty";
      return CommandLineError;
    }
    if (!m_directEngine) {
      m_renderGraph = true;
      m_renderGraphAOVOutputs.push_back(
        {engine::graph::RenderViewMode::SampleStddev, m_wavefrontSampleStddevOut});
    }
  }

  if (parser.isSet("wavefront_sample_stddev_color_out")) {
    m_wavefrontSampleStddevColorOut = parser.value("wavefront_sample_stddev_color_out").trimmed();
    if (m_wavefrontSampleStddevColorOut.isEmpty()) {
      *errorMessage = "Wavefront color sample standard-deviation output path must not be empty";
      return CommandLineError;
    }
    if (!m_directEngine) {
      m_renderGraph = true;
      m_renderGraphAOVOutputs.push_back(
        {engine::graph::RenderViewMode::SampleStddevColor, m_wavefrontSampleStddevColorOut});
    }
  }

  if (parser.isSet("render_graph_aov_out")) {
    m_renderGraph = true;
    for (const QString& value : parser.values("render_graph_aov_out")) {
      RenderGraphAOVOutput output;
      if (!parseRenderGraphAOVOutput(value, &output, errorMessage)) {
        return CommandLineError;
      }
      m_renderGraphAOVOutputs.push_back(output);
    }
  }

  if (parser.isSet("render_graph_view_override")) {
    m_renderGraph = true;
    for (const QString& value : parser.values("render_graph_view_override")) {
      RenderGraphViewOverrideInput input;
      if (!input.parse(value, errorMessage)) {
        return CommandLineError;
      }
      m_renderGraphViewOverrides.push_back(input);
    }
  }

  if (parser.isSet("render_graph_color_in")) {
    m_renderGraph = true;
    for (const QString& value : parser.values("render_graph_color_in")) {
      RenderGraphImageInput input;
      if (!input.parse(value, "--render_graph_color_in", errorMessage)) {
        return CommandLineError;
      }
      m_renderGraphColorInputs.push_back(input);
    }
  }

  if (parser.isSet("render_graph_depth_in")) {
    m_renderGraph = true;
    for (const QString& value : parser.values("render_graph_depth_in")) {
      RenderGraphImageInput input;
      if (!input.parse(value, "--render_graph_depth_in", errorMessage)) {
        return CommandLineError;
      }
      m_renderGraphDepthInputs.push_back(input);
    }
  }

  if (parser.isSet("render_graph_stencil_in")) {
    m_renderGraph = true;
    for (const QString& value : parser.values("render_graph_stencil_in")) {
      RenderGraphImageInput input;
      if (!input.parse(value, "--render_graph_stencil_in", errorMessage)) {
        return CommandLineError;
      }
      m_renderGraphStencilInputs.push_back(input);
    }
  }

  if (parser.isSet("render_graph_object_id_in")) {
    m_renderGraph = true;
    for (const QString& value : parser.values("render_graph_object_id_in")) {
      RenderGraphImageInput input;
      if (!input.parse(value, "--render_graph_object_id_in", errorMessage)) {
        return CommandLineError;
      }
      m_renderGraphObjectIdInputs.push_back(input);
    }
  }

  if (parser.isSet("render_graph_material_id_in")) {
    m_renderGraph = true;
    for (const QString& value : parser.values("render_graph_material_id_in")) {
      RenderGraphImageInput input;
      if (!input.parse(value, "--render_graph_material_id_in", errorMessage)) {
        return CommandLineError;
      }
      m_renderGraphMaterialIdInputs.push_back(input);
    }
  }

  if (parser.isSet("render_graph_executor")) {
    m_renderGraph = true;
    if (!parseRenderExecutorPreference(parser.value("render_graph_executor"),
                                       &m_renderGraphExecutor)) {
      *errorMessage =
        "Render graph executor must be 'raytracer', 'pathtracer', 'wavefront', 'rasterizer', or "
        "'wireframe'";
      return CommandLineError;
    }
    m_renderGraphExecutorSet = true;
  }

  if (parser.isSet("render_graph_view")) {
    m_renderGraph = true;
    if (!parseImplementedRenderViewMode(parser.value("render_graph_view"),
                                        &m_renderGraphViewMode)) {
      *errorMessage =
        "Render graph view mode must be 'default', 'beauty', 'wireframe', 'depth', 'stencil', "
        "'stencil_composite', 'normal', 'object_id', 'material_id', 'world_position', "
        "'sample_stddev', 'sample_stddev_color', 'raster_coverage_count', "
        "'raster_depth_test_count', 'raster_depth_pass_count', 'raster_shade_count', "
        "'raster_color_write_count', or 'hybrid_visibility'";
      return CommandLineError;
    }
    m_renderGraphViewModeSet = true;
  }

  if (parser.isSet("render_graph_camera")) {
    m_renderGraph = true;
    m_renderGraphCamera = parser.value("render_graph_camera").trimmed();
    if (m_renderGraphCamera.isEmpty()) {
      *errorMessage = "Render graph camera id must not be empty";
      return CommandLineError;
    }
  }

  if (parser.isSet("render_graph_shading_profile")) {
    m_renderGraph = true;
    m_renderGraphShadingProfile = parser.value("render_graph_shading_profile").trimmed();
    if (m_renderGraphShadingProfile.isEmpty()) {
      *errorMessage = "Render graph shading profile must not be empty";
      return CommandLineError;
    }
  }

  if (parser.isSet("render_graph_shading_parameter")) {
    m_renderGraph = true;
    for (const QString& value : parser.values("render_graph_shading_parameter")) {
      std::pair<std::string, engine::graph::ShadingProfileParameterValue> parameter;
      if (!parseShadingProfileParameter(value, &parameter, errorMessage)) {
        return CommandLineError;
      }
      m_renderGraphShadingProfileParameters.insert_or_assign(parameter.first, parameter.second);
    }
  }

  if (parser.isSet("render_graph_wireframe_overlay")) {
    m_renderGraph = true;
    m_renderGraphWireframeOverlay = true;
  }

  if (parser.isSet("render_graph_curve_overlay")) {
    m_renderGraph = true;
    m_renderGraphCurveOverlay = true;
  }

  if (parser.isSet("disable_pass")) {
    m_renderGraph = true;
    for (const QString& passId : splitOptionValues(parser.values("disable_pass"))) {
      m_renderGraphOverrides.disabledPasses.insert(passId.toStdString());
    }
  }

  if (parser.isSet("disable_pass_kind")) {
    m_renderGraph = true;
    for (const QString& value : splitOptionValues(parser.values("disable_pass_kind"))) {
      engine::graph::RenderPassKind kind;
      if (!parseRenderPassKind(value, &kind)) {
        *errorMessage = "Render graph pass kind is not recognized";
        return CommandLineError;
      }
      m_renderGraphOverrides.disabledPassKinds.insert(kind);
    }
  }

  if (parser.isSet("disable_executor")) {
    m_renderGraph = true;
    for (const QString& value : splitOptionValues(parser.values("disable_executor"))) {
      engine::graph::RenderExecutorKind executor;
      if (!parseRenderExecutorKind(value, &executor)) {
        *errorMessage = "Render graph executor is not recognized";
        return CommandLineError;
      }
      m_renderGraphOverrides.disabledExecutors.insert(executor);
    }
  }

  if (parser.isSet("disable_feature")) {
    m_renderGraph = true;
    for (const QString& feature : splitOptionValues(parser.values("disable_feature"))) {
      m_renderGraphOverrides.disabledFeatures.insert(feature.toStdString());
    }
  }

  if (parser.isSet("lod")) {
    bool ok = false;
    m_wireframeLod = parser.value("lod").toInt(&ok);
    if (!ok || m_wireframeLod < 0) {
      *errorMessage = "LOD must be a non-negative integer";
      return CommandLineError;
    }
  }

  if (parser.isSet("raster_backend")) {
    try {
      const auto backend = engine::raster::RasterBackend::fromString(
        parser.value("raster_backend").toStdString(), "--raster_backend");
      m_rasterBackend = QString::fromUtf8(backend.id());
      m_rasterBackendSet = true;
    } catch (const std::runtime_error&) {
      *errorMessage = "Raster backend must be 'cpu', 'opengl', or 'gpu'";
      return CommandLineError;
    }
  }

  if (parser.isSet("cull")) {
    const QString cull = parser.value("cull").toLower();
    if (cull != "both" && cull != "back" && cull != "front") {
      *errorMessage = "Cull mode must be 'both', 'back', or 'front'";
      return CommandLineError;
    }
    m_rasterCullMode = cull;
    m_rasterCullModeSet = true;
  }

  if (parser.isSet("raster_culling")) {
    const QString mode = normalizedRasterOption(parser.value("raster_culling"));
    if (mode != "off" && mode != "on" && mode != "auto") {
      *errorMessage = "Raster visibility culling must be 'off', 'on', or 'auto'";
      return CommandLineError;
    }
    m_rasterVisibilityCulling = mode;
  }

  if (parser.isSet("raster_tessellation_quality")) {
    const QString quality = normalizedRasterOption(parser.value("raster_tessellation_quality"));
    if (quality != "preview" && quality != "balanced" && quality != "final") {
      *errorMessage = "Raster tessellation quality must be 'preview', 'balanced', or 'final'";
      return CommandLineError;
    }
    m_rasterTessellationQuality = quality;
    m_rasterTessellationQualitySet = true;
  }

  if (parser.isSet("raster_max_screen_space_error")) {
    bool ok = false;
    const double pixels = parser.value("raster_max_screen_space_error").toDouble(&ok);
    if (!ok || pixels < 0.0) {
      *errorMessage = "Raster max screen-space error must be a non-negative number";
      return CommandLineError;
    }
    m_rasterMaxScreenSpaceError = pixels;
    m_rasterMaxScreenSpaceErrorSet = true;
  }

  if (parser.isSet("depth_prepass")) {
    const QString mode = normalizedRasterOption(parser.value("depth_prepass"));
    if (mode != "off" && mode != "on" && mode != "auto") {
      *errorMessage = "Raster depth prepass must be 'off', 'on', or 'auto'";
      return CommandLineError;
    }
    m_rasterDepthPrepass = mode;
  }

  if (parser.isSet("msaa")) {
    bool ok = false;
    m_rasterMsaaSamples = parser.value("msaa").toInt(&ok);
    if (!ok || (m_rasterMsaaSamples != 1 && m_rasterMsaaSamples != 2 && m_rasterMsaaSamples != 4 &&
                m_rasterMsaaSamples != 8)) {
      *errorMessage = "MSAA samples must be 1, 2, 4, or 8";
      return CommandLineError;
    }
  }

  if (parser.isSet("msaa_shading")) {
    const QString mode = parser.value("msaa_shading").toLower();
    if (mode != "per_sample" && mode != "per_fragment") {
      *errorMessage = "MSAA shading mode must be 'per_sample' or 'per_fragment'";
      return CommandLineError;
    }
    m_rasterMsaaShadingMode = mode;
  }

  if (parser.isSet("post_aa")) {
    const QString postAA = parser.value("post_aa").toLower();
    if (postAA != "none" && postAA != "fxaa" && postAA != "smaa" && postAA != "taa") {
      *errorMessage = "Post-process AA must be 'none', 'fxaa', 'smaa', or 'taa'";
      return CommandLineError;
    }
    m_rasterPostProcessAA = postAA;
    m_rasterPostProcessAASet = true;
  }

  if (parser.isSet("color_write_mask")) {
    if (!parseColorWriteMask(parser.value("color_write_mask"), &m_rasterColorWriteMask)) {
      *errorMessage = "Color write mask must contain only r, g, b, or be 'none'";
      return CommandLineError;
    }
  }

  if (parser.isSet("blend")) {
    m_rasterBlending = true;
  }

  if (parser.isSet("blend_src")) {
    if (!parseBlendFactor(parser.value("blend_src"), &m_rasterBlendSourceFactor)) {
      *errorMessage = "Source blend factor is not recognized";
      return CommandLineError;
    }
  }

  if (parser.isSet("blend_dst")) {
    if (!parseBlendFactor(parser.value("blend_dst"), &m_rasterBlendDestinationFactor)) {
      *errorMessage = "Destination blend factor is not recognized";
      return CommandLineError;
    }
  }

  if (parser.isSet("blend_op")) {
    if (!parseBlendOp(parser.value("blend_op"), &m_rasterBlendOp)) {
      *errorMessage = "Blend operation must be add, subtract, reverse_subtract, min, or max";
      return CommandLineError;
    }
  }

  if (parser.isSet("blend_constant_color")) {
    if (!parseColorTriplet(parser.value("blend_constant_color"), &m_rasterBlendConstantColor)) {
      *errorMessage = "Blend constant color must be three comma-separated values in 0..1";
      return CommandLineError;
    }
  }

  if (parser.isSet("blend_constant_alpha")) {
    bool ok = false;
    const double alpha = parser.value("blend_constant_alpha").toDouble(&ok);
    if (!ok || !std::isfinite(alpha) || alpha < 0.0 || alpha > 1.0) {
      *errorMessage = "Blend constant alpha must be a number from 0 to 1";
      return CommandLineError;
    }
    m_rasterBlendConstantAlpha = alpha;
  }

  if (parser.isSet("alpha_test")) {
    m_rasterAlphaTest = true;
  }

  if (parser.isSet("alpha_func")) {
    if (!parseAlphaFunc(parser.value("alpha_func"), &m_rasterAlphaFunc)) {
      *errorMessage =
        "Alpha function must be never, less, equal, less_equal, greater, greater_equal, "
        "not_equal, or always";
      return CommandLineError;
    }
  }

  if (parser.isSet("alpha_ref")) {
    bool ok = false;
    const double alpha = parser.value("alpha_ref").toDouble(&ok);
    if (!ok || !std::isfinite(alpha) || alpha < 0.0 || alpha > 1.0) {
      *errorMessage = "Alpha reference must be a number from 0 to 1";
      return CommandLineError;
    }
    m_rasterAlphaReference = alpha;
  }

  if (parser.isSet("viewport")) {
    if (!parseRasterRect(parser.value("viewport"), &m_rasterViewport)) {
      *errorMessage =
        "Viewport must be four comma-separated integers x,y,width,height with non-negative size";
      return CommandLineError;
    }
    m_rasterViewportSet = true;
  }

  if (parser.isSet("scissor")) {
    if (!parseRasterRect(parser.value("scissor"), &m_rasterScissor)) {
      *errorMessage =
        "Scissor must be four comma-separated integers x,y,width,height with non-negative size";
      return CommandLineError;
    }
    m_rasterScissorSet = true;
  }

  if (parser.isSet("depth_bias")) {
    bool ok = false;
    m_rasterDepthBias = parser.value("depth_bias").toDouble(&ok);
    if (!ok || !std::isfinite(m_rasterDepthBias)) {
      *errorMessage = "Depth bias must be a finite number";
      return CommandLineError;
    }
  }

  if (parser.isSet("shadow_maps")) {
    m_rasterShadowMaps = true;
  }

  if (parser.isSet("shadow_map_size")) {
    bool ok = false;
    m_rasterShadowMapSize = parser.value("shadow_map_size").toInt(&ok);
    if (!ok || m_rasterShadowMapSize <= 0) {
      *errorMessage = "Shadow map size must be a positive integer";
      return CommandLineError;
    }
  }

  if (parser.isSet("shadow_cascades")) {
    bool ok = false;
    m_rasterShadowCascadeCount = parser.value("shadow_cascades").toInt(&ok);
    if (!ok || m_rasterShadowCascadeCount <= 0) {
      *errorMessage = "Shadow cascade count must be a positive integer";
      return CommandLineError;
    }
  }

  if (parser.isSet("shadow_cascade_split")) {
    bool ok = false;
    m_rasterShadowCascadeSplitLambda = parser.value("shadow_cascade_split").toDouble(&ok);
    if (!ok || !std::isfinite(m_rasterShadowCascadeSplitLambda) ||
        m_rasterShadowCascadeSplitLambda < 0.0 || m_rasterShadowCascadeSplitLambda > 1.0) {
      *errorMessage = "Shadow cascade split blend must be a number from 0 to 1";
      return CommandLineError;
    }
  }

  if (parser.isSet("shadow_bias")) {
    bool ok = false;
    m_rasterShadowBias = parser.value("shadow_bias").toDouble(&ok);
    if (!ok || m_rasterShadowBias < 0.0) {
      *errorMessage = "Shadow bias must be a non-negative number";
      return CommandLineError;
    }
  }

  if (parser.isSet("shadow_slope_bias")) {
    bool ok = false;
    m_rasterShadowSlopeBias = parser.value("shadow_slope_bias").toDouble(&ok);
    if (!ok || m_rasterShadowSlopeBias < 0.0) {
      *errorMessage = "Shadow slope bias must be a non-negative number";
      return CommandLineError;
    }
  }

  if (parser.isSet("shadow_filter_radius")) {
    bool ok = false;
    m_rasterShadowFilterRadius = parser.value("shadow_filter_radius").toInt(&ok);
    if (!ok || m_rasterShadowFilterRadius < 0) {
      *errorMessage = "Shadow filter radius must be a non-negative integer";
      return CommandLineError;
    }
  }

  if (parser.isSet("shadow_filter")) {
    const QString filterMode = parser.value("shadow_filter").toLower();
    if (filterMode != "pcf" && filterMode != "pcss") {
      *errorMessage = "Shadow filter must be 'pcf' or 'pcss'";
      return CommandLineError;
    }
    m_rasterShadowFilterMode = filterMode;
  }

  if (parser.isSet("shadow_mode")) {
    const QString shadowMode = parser.value("shadow_mode").toLower();
    if (shadowMode != "shadow_maps" && shadowMode != "ray_traced") {
      *errorMessage = "Shadow mode must be 'shadow_maps' or 'ray_traced'";
      return CommandLineError;
    }
    m_rasterShadowMode = shadowMode;
    m_rasterShadowMaps = true;
  }

  if (parser.isSet("timing")) {
    m_timing = true;
  }

  if (parser.isSet("animation")) {
    m_animation = true;
  }

  if (parser.isSet("step")) {
    if (!parseStepSelection(parser.value("step"), &m_stepSelection, errorMessage)) {
      return CommandLineError;
    }
    m_stepSelectionSet = true;
  }

  if (m_gcodeCumulativeLayers && !m_gcodeLayerSet) {
    *errorMessage = "--gcode_cumulative_layers requires --gcode_layer";
    return CommandLineError;
  }

  if (parser.isSet("repeat")) {
    bool ok = false;
    m_repeat = parser.value("repeat").toInt(&ok);
    if (!ok || m_repeat <= 0) {
      *errorMessage = "Repeat must be a positive integer";
      return CommandLineError;
    }
    m_timing = true;
  }

  if (parser.isSet("frame")) {
    bool ok = false;
    m_frame = parser.value("frame").toInt(&ok);
    if (!ok) {
      *errorMessage = "Frame must be an integer";
      return CommandLineError;
    }
    m_frameSet = true;
  }

  if (parser.isSet("step_highlight")) {
    m_stepPlaybackStyle.highlightActive = true;
  }

  if (parser.isSet("step_ghost_previous")) {
    m_stepPlaybackStyle.ghostPrevious = true;
  }

  if (m_stepPlaybackStyle.highlightActive || m_stepPlaybackStyle.ghostPrevious) {
    if (!m_stepSelectionSet || m_stepSelection.mode == CommandLineStepMode::Sequence) {
      *errorMessage = "Step playback visual modes require --step";
      return CommandLineError;
    }
    m_stepPlaybackStyle.activeStep = m_stepSelection.step;
  }

  if (parser.isSet("frame_start")) {
    bool ok = false;
    m_frameStart = parser.value("frame_start").toInt(&ok);
    if (!ok) {
      *errorMessage = "Frame start must be an integer";
      return CommandLineError;
    }
    m_frameStartSet = true;
  }

  if (parser.isSet("frame_end")) {
    bool ok = false;
    m_frameEnd = parser.value("frame_end").toInt(&ok);
    if (!ok) {
      *errorMessage = "Frame end must be an integer";
      return CommandLineError;
    }
    m_frameEndSet = true;
  }

  if (parser.isSet("fps")) {
    bool ok = false;
    m_fps = parser.value("fps").toDouble(&ok);
    if (!ok || m_fps <= 0.0) {
      *errorMessage = "FPS must be a positive number";
      return CommandLineError;
    }
    m_fpsSet = true;
  }

  if (m_animation && m_frameSet) {
    *errorMessage = "Cannot combine --animation with --frame";
    return CommandLineError;
  }

  if (m_animation && m_stepSelectionSet) {
    *errorMessage = "Cannot combine --animation with --step";
    return CommandLineError;
  }

  if (m_animation && m_repeat > 1) {
    *errorMessage = "Cannot combine --animation with --repeat";
    return CommandLineError;
  }

  if (m_animation && m_renderGraphOnly) {
    *errorMessage = "Cannot combine --animation with --render_graph_only";
    return CommandLineError;
  }

  if (m_animation && !m_renderGraphOut.isEmpty()) {
    *errorMessage = "Cannot combine --animation with --render_graph_out";
    return CommandLineError;
  }

  if (m_animation && !m_renderGraphTraceOut.isEmpty()) {
    *errorMessage = "Cannot combine --animation with --render_graph_trace_out";
    return CommandLineError;
  }

  if (m_animation && !m_renderGraphAOVOutputs.empty()) {
    *errorMessage = "Cannot combine --animation with --render_graph_aov_out";
    return CommandLineError;
  }

  if (m_animation && (!m_rasterMetricsOut.isEmpty() || m_rasterMetricsSummary)) {
    *errorMessage = "Cannot combine --animation with raster metrics output";
    return CommandLineError;
  }

  if (m_animation && (!m_wavefrontMetricsOut.isEmpty() || m_wavefrontMetricsSummary)) {
    *errorMessage = "Cannot combine --animation with wavefront metrics output";
    return CommandLineError;
  }

  const bool wavefrontSampleStddevOutputSet =
    !m_wavefrontSampleStddevOut.isEmpty() || !m_wavefrontSampleStddevColorOut.isEmpty();

  if (m_pathTracingScheduleSet && !pathTracingRequested()) {
    *errorMessage = "Path tracing schedule requires --engine pathtracer or --integrator pathtracer";
    return CommandLineError;
  }

  if (m_gpuPrimarySampleChunkSizeSet && !pathTracingRequested()) {
    *errorMessage =
      "GPU primary sample chunk size requires --engine pathtracer or --integrator pathtracer";
    return CommandLineError;
  }

  if (m_pathTracingScheduleSet && m_engineSet) {
    if (m_engine == "raster" || m_engine == "wireframe" ||
        (m_engine == "wavefront" && scalarPathTracingScheduleSelected()) ||
        (m_engine == "raytracer" && !scalarPathTracingScheduleSelected())) {
      *errorMessage = "Path tracing schedule conflicts with the selected engine";
      return CommandLineError;
    }
  }

  if (m_gpuPrimarySampleChunkSizeSet && m_directEngine) {
    *errorMessage = "GPU primary sample chunk size requires graph-backed rendering";
    return CommandLineError;
  }

  const bool tracingExecutionTargetsNonTracingExecutor =
    m_renderGraphExecutorSet
      ? (m_renderGraphExecutor == engine::graph::RenderExecutorPreference::Rasterizer ||
         m_renderGraphExecutor == engine::graph::RenderExecutorPreference::Wireframe)
      : (m_engine == "raster" || m_engine == "wireframe");
  if (m_tracingExecutionSet && tracingExecutionTargetsNonTracingExecutor) {
    *errorMessage =
      "Tracing execution requires a raytracer, pathtracer, or wavefront graph executor";
    return CommandLineError;
  }

  if (m_animation && wavefrontSampleStddevOutputSet) {
    *errorMessage = "Cannot combine --animation with wavefront sample standard-deviation output";
    return CommandLineError;
  }

  if (m_renderGraphOnly && m_repeat > 1) {
    *errorMessage = "Cannot combine --render_graph_only with --repeat";
    return CommandLineError;
  }

  if (m_renderGraphOnly && !m_renderGraphTraceOut.isEmpty()) {
    *errorMessage = "Cannot combine --render_graph_only with --render_graph_trace_out";
    return CommandLineError;
  }

  if (m_renderGraphOnly && !m_renderGraphAOVOutputs.empty()) {
    *errorMessage = "Cannot combine --render_graph_only with --render_graph_aov_out";
    return CommandLineError;
  }

  if (m_renderGraphOnly && (!m_rasterMetricsOut.isEmpty() || m_rasterMetricsSummary)) {
    *errorMessage = "Cannot combine --render_graph_only with raster metrics output";
    return CommandLineError;
  }

  if (m_renderGraphOnly && (!m_wavefrontMetricsOut.isEmpty() || m_wavefrontMetricsSummary)) {
    *errorMessage = "Cannot combine --render_graph_only with wavefront metrics output";
    return CommandLineError;
  }

  if (m_renderGraphOnly && wavefrontSampleStddevOutputSet) {
    *errorMessage =
      "Cannot combine --render_graph_only with wavefront sample standard-deviation output";
    return CommandLineError;
  }

  if (m_renderGraphOnly && m_stepSelectionSet &&
      m_stepSelection.mode == CommandLineStepMode::Sequence) {
    *errorMessage = "Cannot combine --render_graph_only with --step sequence";
    return CommandLineError;
  }

  if (m_stepSelectionSet && m_stepSelection.mode == CommandLineStepMode::Sequence && m_repeat > 1) {
    *errorMessage = "Cannot combine --step sequence with --repeat";
    return CommandLineError;
  }

  if (m_stepSelectionSet && m_stepSelection.mode == CommandLineStepMode::Sequence &&
      !m_renderGraphOut.isEmpty()) {
    *errorMessage = "Cannot combine --step sequence with --render_graph_out";
    return CommandLineError;
  }

  if (m_stepSelectionSet && m_stepSelection.mode == CommandLineStepMode::Sequence &&
      !m_renderGraphTraceOut.isEmpty()) {
    *errorMessage = "Cannot combine --step sequence with --render_graph_trace_out";
    return CommandLineError;
  }

  if (m_stepSelectionSet && m_stepSelection.mode == CommandLineStepMode::Sequence &&
      !m_renderGraphAOVOutputs.empty()) {
    *errorMessage = "Cannot combine --step sequence with --render_graph_aov_out";
    return CommandLineError;
  }

  if (m_stepSelectionSet && m_stepSelection.mode == CommandLineStepMode::Sequence &&
      (!m_rasterMetricsOut.isEmpty() || m_rasterMetricsSummary)) {
    *errorMessage = "Cannot combine --step sequence with raster metrics output";
    return CommandLineError;
  }

  if (m_stepSelectionSet && m_stepSelection.mode == CommandLineStepMode::Sequence &&
      (!m_wavefrontMetricsOut.isEmpty() || m_wavefrontMetricsSummary)) {
    *errorMessage = "Cannot combine --step sequence with wavefront metrics output";
    return CommandLineError;
  }

  if (m_stepSelectionSet && m_stepSelection.mode == CommandLineStepMode::Sequence &&
      wavefrontSampleStddevOutputSet) {
    *errorMessage =
      "Cannot combine --step sequence with wavefront sample standard-deviation output";
    return CommandLineError;
  }

  if (wavefrontSampleStddevOutputSet && m_directEngine) {
    if (m_engine != "wavefront" && m_engine != "pathtracer" && m_engine != "pt") {
      *errorMessage =
        "Wavefront sample standard-deviation output requires --engine wavefront or pathtracer";
      return CommandLineError;
    }
  }

  if (wavefrontSampleStddevOutputSet && scalarPathTracingScheduleSelected()) {
    *errorMessage =
      "Wavefront sample standard-deviation output requires a wavefront path-tracing schedule";
    return CommandLineError;
  }

  if (m_directEngine &&
      (parser.isSet("render_graph") || m_renderGraphOnly || parser.isSet("render_graph_format") ||
       !m_renderGraphOut.isEmpty() || !m_renderGraphIn.isEmpty() ||
       !m_renderGraphTraceOut.isEmpty() || !m_renderGraphAOVOutputs.empty() ||
       !m_renderGraphViewOverrides.empty() || !m_renderGraphColorInputs.empty() ||
       !m_renderGraphDepthInputs.empty() || !m_renderGraphStencilInputs.empty() ||
       !m_renderGraphObjectIdInputs.empty() || !m_renderGraphMaterialIdInputs.empty() ||
       parser.isSet("render_graph_executor") || parser.isSet("render_graph_view") ||
       parser.isSet("render_graph_camera") || parser.isSet("render_graph_shading_profile") ||
       parser.isSet("render_graph_shading_parameter") || m_renderGraphWireframeOverlay ||
       m_renderGraphCurveOverlay || parser.isSet("disable_pass") ||
       parser.isSet("disable_pass_kind") || parser.isSet("disable_executor") ||
       parser.isSet("disable_feature") || parser.isSet("raster_culling") ||
       parser.isSet("depth_prepass") || m_tracingExecutionSet || m_sampleStreamModeSet)) {
    *errorMessage = "Cannot combine --direct_engine with render graph options";
    return CommandLineError;
  }

  if (m_directEngine && m_rasterBackendSet && m_rasterBackend != QStringLiteral("cpu")) {
    *errorMessage =
      "OpenGL raster backend is graph-backed; use --raster_backend cpu with --direct_engine";
    return CommandLineError;
  }

  const QStringList args = parser.positionalArguments();

  if (args.isEmpty()) {
    *errorMessage = m_renderGraphOnly ? "Need input filename" : "Need input and output filename";
    return CommandLineError;
  }

  m_filename = args.at(0);
  if (args.size() >= 2) {
    m_output = args.at(1);
  } else if (!m_renderGraphOnly) {
    *errorMessage = "Need input and output filename";
    return CommandLineError;
  }

  if (m_gcodeOptionsSet) {
    if (!m_importFormat.isEmpty() && m_importFormat.toLower() != "gcode") {
      *errorMessage = "G-code render options require the gcode importer";
      return CommandLineError;
    }
    m_importFormat = "gcode";
  }

  return CommandLineOk;
}

int main(int argc, char** argv) {
  RenderCliApplication app(argc, argv);
  QCoreApplication::setApplicationName(
    QCoreApplication::translate("rendercli", "Command line renderer"));

  qRegisterMetaType<Vector3d>();
  qRegisterMetaType<Angled>();
  qRegisterMetaType<Colord>();
  qRegisterMetaType<Material*>();
  qRegisterMetaType<Texture*>();

  Renderer r;
  QString errorMessage;

  switch (r.parseCommandLine(&errorMessage)) {
  case Renderer::CommandLineOk:
    break;
  case Renderer::CommandLineError:
    fputs(qPrintable(errorMessage), stderr);
    fputs("\n\n", stderr);
    fputs(qPrintable(r.parser.helpText()), stderr);
    return 1;
  case Renderer::CommandLineVersionRequested:
    printf("%s %s\n", qPrintable(QCoreApplication::applicationName()),
           qPrintable(QCoreApplication::applicationVersion()));
    return 0;
  case Renderer::CommandLineHelpRequested:
    r.parser.showHelp();
    Q_UNREACHABLE();
  }

  try {
    r.render();
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }

  return 0;
}
