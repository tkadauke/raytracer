#include "engine/wavefront/WavefrontRaytracer.h"

#include "core/Buffer.h"
#include "core/math/Constants.h"
#include "core/util/BufferUtils.h"
#include "engine/TileRenderTask.h"
#include "engine/wavefront/detail/WavefrontMetricsRecorder.h"
#include "engine/wavefront/detail/WavefrontTileRenderer.h"
#include "render/Integrator.h"
#include "render/GpuIntersectionScene.h"
#include "render/RayCaster.h"
#include "render/SamplingSeed.h"
#include "render/Stats.h"
#include "render/TilePlan.h"
#include "render/WavefrontIntersectionBackend.h"
#include "render/WhittedIntegrator.h"
#include "render/cameras/Camera.h"
#include "render/denoise/Denoiser.h"
#include "render/primitives/Scene.h"
#include "render/tonemap/Tonemap.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QThread>
#include <QThreadPool>

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace engine::wavefront {
  namespace {
    void mergeLabel(std::string& target, const std::string& source) {
      if (source.empty()) {
        return;
      }
      if (target.empty()) {
        target = source;
        return;
      }
      if (target != source) {
        target = "mixed";
      }
    }

    void mergeAccumulationDiagnostics(render::TracingAccumulationDiagnostics& target,
                                      const render::TracingAccumulationDiagnostics& source) {
      mergeLabel(target.backend, source.backend);
      mergeLabel(target.residency, source.residency);
      target.residentBytes = std::max(target.residentBytes, source.residentBytes);
      target.clearOperations += source.clearOperations;
      target.addOperations += source.addOperations;
      target.addedSamples += source.addedSamples;
      target.resolveOperations += source.resolveOperations;
      target.readbackOperations += source.readbackOperations;
      target.readbackBytes += source.readbackBytes;
    }

    class RecursiveRayCasterAdapter : public render::RayCaster {
    public:
      RecursiveRayCasterAdapter(const render::Scene& scene, const render::Integrator& integrator)
          : m_scene(scene),
            m_integrator(integrator) {
      }

      Colord rayColor(const Rayd& ray, render::State& state) const override {
        return m_integrator.radiance(m_scene, ray, state, *this);
      }

    private:
      const render::Scene& m_scene;
      const render::Integrator& m_integrator;
    };

    render::TracingExecutionDevice executionDeviceForLabel(const std::string& label) {
      if (label == "metal" || label == "vulkan" || label == "gpu" || label == "gpu_resident" ||
          label == "gpu_resident_path_loop" || label == "gpu_resident_direct_light_batch") {
        return render::TracingExecutionDevice::GPU;
      }
      if (label == "mixed" || label == "hybrid" || label == "metal_shared" ||
          label == "vulkan_host_coherent") {
        return render::TracingExecutionDevice::Hybrid;
      }
      if (label == "cpu" || label == "runtime_scene" || label == "compiled_cpu" ||
          label == "compiled_cpu_reference" || label == "packed_cpu" || label == "packed_host" ||
          label == "host" || label == "host_cpu" || label == "cpu_host") {
        return render::TracingExecutionDevice::CPU;
      }
      return render::TracingExecutionDevice::Unsupported;
    }

    render::TracingExecutionDevice requestedDeviceForLabel(const std::string& label) {
      if (label == "gpu" || label == "metal" || label == "vulkan") {
        return render::TracingExecutionDevice::GPU;
      }
      if (label == "mixed" || label == "hybrid") {
        return render::TracingExecutionDevice::Hybrid;
      }
      if (label == "auto" || label == "cpu" || label.empty()) {
        return render::TracingExecutionDevice::CPU;
      }
      return render::TracingExecutionDevice::Unsupported;
    }

    render::TracingCapabilityRecord tracingRecordFromExecutionLabels(
      render::TracingExecutionDomain domain, std::string name, const std::string& request,
      const std::string& backend, const std::string& platform, const std::string& availability,
      const std::string& fallbackReason, const std::string& executionPath) {
      const render::TracingExecutionDevice requested = requestedDeviceForLabel(request);
      const render::TracingExecutionDevice resolved =
        executionDeviceForLabel(executionPath.empty() ? backend : executionPath);
      if (resolved == render::TracingExecutionDevice::Unsupported) {
        return render::TracingCapabilityRecord::unsupported(
          domain, std::move(name),
          fallbackReason.empty() ? "no execution path was reported" : fallbackReason);
      }
      if (availability == "fallback" || !fallbackReason.empty()) {
        auto record = render::TracingCapabilityRecord::fallbackRecord(
          domain, std::move(name), requested, resolved, executionPath, fallbackReason);
        record.platform = platform;
        return record;
      }
      if (resolved == render::TracingExecutionDevice::GPU) {
        return render::TracingCapabilityRecord::gpu(domain, std::move(name), platform,
                                                    executionPath);
      }
      if (resolved == render::TracingExecutionDevice::Hybrid) {
        return render::TracingCapabilityRecord::hybrid(domain, std::move(name), executionPath);
      }
      return render::TracingCapabilityRecord::cpu(domain, std::move(name), executionPath);
    }

    render::TracingCapabilityRecord
    tracingRecordForResolvedExecutionPath(render::TracingExecutionDomain domain, std::string name,
                                          render::TracingExecutionDevice resolved,
                                          const std::string& platform,
                                          const std::string& executionPath) {
      if (resolved == render::TracingExecutionDevice::GPU) {
        return render::TracingCapabilityRecord::gpu(domain, std::move(name), platform,
                                                    executionPath);
      }
      if (resolved == render::TracingExecutionDevice::Hybrid) {
        return render::TracingCapabilityRecord::hybrid(domain, std::move(name), executionPath);
      }
      if (resolved == render::TracingExecutionDevice::CPU) {
        return render::TracingCapabilityRecord::cpu(domain, std::move(name), executionPath);
      }
      return render::TracingCapabilityRecord::unsupported(domain, std::move(name),
                                                          "no execution path was reported");
    }

    QString tracingDomainLabel(render::TracingExecutionDomain domain) {
      switch (domain) {
      case render::TracingExecutionDomain::Intersection:
        return QStringLiteral("intersection");
      case render::TracingExecutionDomain::SceneRecords:
        return QStringLiteral("scene_records");
      case render::TracingExecutionDomain::Sampling:
        return QStringLiteral("sampling");
      case render::TracingExecutionDomain::DirectLighting:
        return QStringLiteral("direct_lighting");
      case render::TracingExecutionDomain::BSDF:
        return QStringLiteral("bsdf");
      case render::TracingExecutionDomain::PathState:
        return QStringLiteral("path_state");
      case render::TracingExecutionDomain::Accumulation:
        return QStringLiteral("accumulation");
      }
      return QStringLiteral("unknown");
    }

    QString tracingDeviceLabel(render::TracingExecutionDevice device) {
      switch (device) {
      case render::TracingExecutionDevice::CPU:
        return QStringLiteral("cpu");
      case render::TracingExecutionDevice::Hybrid:
        return QStringLiteral("hybrid");
      case render::TracingExecutionDevice::GPU:
        return QStringLiteral("gpu");
      case render::TracingExecutionDevice::Unsupported:
        return QStringLiteral("unsupported");
      }
      return QStringLiteral("unsupported");
    }

    QString tracingSupportLabel(render::TracingCapabilitySupport support) {
      switch (support) {
      case render::TracingCapabilitySupport::Supported:
        return QStringLiteral("supported");
      case render::TracingCapabilitySupport::Restricted:
        return QStringLiteral("restricted");
      case render::TracingCapabilitySupport::Unsupported:
        return QStringLiteral("unsupported");
      case render::TracingCapabilitySupport::Fallback:
        return QStringLiteral("fallback");
      }
      return QStringLiteral("unsupported");
    }

    QJsonObject tracingFallbackToJson(const render::TracingFallbackStatus& fallback) {
      QJsonObject json;
      json["active"] = fallback.active;
      json["requestedDevice"] = tracingDeviceLabel(fallback.requestedDevice);
      json["resolvedDevice"] = tracingDeviceLabel(fallback.resolvedDevice);
      json["reason"] = QString::fromStdString(fallback.reason);
      return json;
    }

    QJsonObject tracingCapabilityToJson(const render::TracingCapabilityRecord& record) {
      QJsonObject json;
      json["domain"] = tracingDomainLabel(record.domain);
      json["name"] = QString::fromStdString(record.name);
      json["support"] = tracingSupportLabel(record.support);
      json["requestedDevice"] = tracingDeviceLabel(record.requestedDevice);
      json["resolvedDevice"] = tracingDeviceLabel(record.resolvedDevice);
      json["executionPath"] = QString::fromStdString(record.executionPath);
      json["availability"] = QString::fromStdString(record.availability);
      json["platform"] = QString::fromStdString(record.platform);
      json["unsupportedReason"] = QString::fromStdString(record.unsupportedReason);
      json["fallback"] = tracingFallbackToJson(record.fallback);
      return json;
    }

    QJsonArray tracingCapabilitiesToJson(const render::TracingExecutionCapabilityRecords& records) {
      QJsonArray json;
      for (const auto& record : records.flattened()) {
        json.push_back(tracingCapabilityToJson(record));
      }
      return json;
    }

    QJsonObject
    tracingBackendFallbackToJson(const render::TracingExecutionCapabilityRecords& records) {
      for (const auto& record : records.flattened()) {
        if (record.fallsBack()) {
          QJsonObject json = tracingFallbackToJson(record.fallback);
          json["capability"] = QString::fromStdString(record.name);
          json["domain"] = tracingDomainLabel(record.domain);
          if (!record.fallback.active) {
            json["active"] = true;
            json["requestedDevice"] = tracingDeviceLabel(record.requestedDevice);
            json["resolvedDevice"] = tracingDeviceLabel(record.resolvedDevice);
            json["reason"] = QString::fromStdString(record.unsupportedReason);
          }
          return json;
        }
      }
      QJsonObject json = tracingFallbackToJson(render::TracingFallbackStatus::none());
      json["capability"] = QString();
      json["domain"] = QString();
      return json;
    }
  }

  void WavefrontRenderMetrics::TimingSummary::recordIntegratorBatch(
    double batchSeconds, const render::IntegratorBatchMetrics& batchMetrics) {
    integratorBatchWorkerSeconds += batchSeconds;
    integratorIntersectionWorkerSeconds += batchMetrics.intersectionWorkerSeconds;
    integratorShadingWorkerSeconds += batchMetrics.shadingWorkerSeconds;
    const double overhead = std::max(0.0, batchSeconds - batchMetrics.intersectionWorkerSeconds -
                                            batchMetrics.shadingWorkerSeconds);
    integratorOverheadWorkerSeconds += overhead;
    integratorPathSetupWorkerSeconds += batchMetrics.pathSetupWorkerSeconds;
    integratorFrontierPartitionWorkerSeconds += batchMetrics.frontierPartitionWorkerSeconds;
    integratorFrontierBookkeepingWorkerSeconds += batchMetrics.frontierBookkeepingWorkerSeconds;
    integratorProgressSnapshotWorkerSeconds += batchMetrics.progressSnapshotWorkerSeconds;
    integratorConvergenceTestWorkerSeconds += batchMetrics.convergenceTestWorkerSeconds;
    integratorResidualWorkerSeconds += std::max(
      0.0,
      overhead - batchMetrics.pathSetupWorkerSeconds - batchMetrics.frontierPartitionWorkerSeconds -
        batchMetrics.frontierBookkeepingWorkerSeconds - batchMetrics.progressSnapshotWorkerSeconds -
        batchMetrics.convergenceTestWorkerSeconds);
  }

  void WavefrontRenderMetrics::BatchSummary::addIntersectionBackendMetrics(
    const render::IntegratorBatchMetrics& metrics) {
    const auto mergeMapMaximums = [](std::map<std::string, std::uint64_t>& target,
                                     const std::map<std::string, std::uint64_t>& source) {
      for (const auto& [key, count] : source) {
        const std::string label = key.empty() ? "unknown" : key;
        target[label] = std::max(target[label], count);
      }
    };
    mergeLabel(intersectionBackendRequest, metrics.intersectionBackendRequest);
    mergeLabel(intersectionBackend, metrics.intersectionBackend);
    mergeLabel(intersectionBackendPlatform, metrics.intersectionBackendPlatform);
    mergeLabel(intersectionBackendAvailability, metrics.intersectionBackendAvailability);
    mergeLabel(intersectionBackendFallbackReason, metrics.intersectionBackendFallbackReason);
    mergeLabel(intersectionBackendExecutionPath, metrics.intersectionBackendExecutionPath);
    mergeLabel(intersectionBackendClosestHitExecutionPath,
               metrics.intersectionBackendClosestHitExecutionPath);
    mergeLabel(intersectionBackendAnyHitExecutionPath,
               metrics.intersectionBackendAnyHitExecutionPath);
    mergeLabel(intersectionBackendClosestHitFrontierResidency,
               metrics.intersectionBackendClosestHitFrontierResidency);
    mergeLabel(intersectionBackendAnyHitFrontierResidency,
               metrics.intersectionBackendAnyHitFrontierResidency);
    directLightSelectionHostBytes += metrics.directLightSelectionHostBytes;
    directLightOcclusionHostBytes += metrics.directLightOcclusionHostBytes;
    directLightContributionHostBytes += metrics.directLightContributionHostBytes;
    mergeLabel(directLightContributionExecutionPath, metrics.directLightContributionExecutionPath);
    mergeLabel(directLightContributionFallbackReason,
               metrics.directLightContributionFallbackReason);
    directLightAnyHitFrontierPackedRayBytes += metrics.directLightAnyHitFrontierPackedRayBytes;
    directLightAnyHitFrontierHostQueryBytes += metrics.directLightAnyHitFrontierHostQueryBytes;
    directLightAnyHitFrontierStateHandleBytes += metrics.directLightAnyHitFrontierStateHandleBytes;
    intersectionBackendClosestHitFrontierPackedRayBytes +=
      metrics.intersectionBackendClosestHitFrontierPackedRayBytes;
    intersectionBackendAnyHitFrontierPackedRayBytes +=
      metrics.intersectionBackendAnyHitFrontierPackedRayBytes;
    intersectionBackendClosestHitFrontierHostQueryBytes +=
      metrics.intersectionBackendClosestHitFrontierHostQueryBytes;
    intersectionBackendAnyHitFrontierHostQueryBytes +=
      metrics.intersectionBackendAnyHitFrontierHostQueryBytes;
    intersectionBackendClosestHitFrontierStateHandleBytes +=
      metrics.intersectionBackendClosestHitFrontierStateHandleBytes;
    intersectionBackendAnyHitFrontierStateHandleBytes +=
      metrics.intersectionBackendAnyHitFrontierStateHandleBytes;
    intersectionBackendPlatformGpuDeviceAvailable =
      intersectionBackendPlatformGpuDeviceAvailable ||
      metrics.intersectionBackendPlatformGpuDeviceAvailable;
    intersectionBackendPlatformGpuRenderPathAvailable =
      intersectionBackendPlatformGpuRenderPathAvailable ||
      metrics.intersectionBackendPlatformGpuRenderPathAvailable;
    intersectionSceneCompiled = intersectionSceneCompiled || metrics.intersectionSceneCompiled;
    intersectionSceneBvhNodes =
      std::max(intersectionSceneBvhNodes, metrics.intersectionSceneBvhNodes);
    intersectionScenePrimitives =
      std::max(intersectionScenePrimitives, metrics.intersectionScenePrimitives);
    intersectionSceneTriangles =
      std::max(intersectionSceneTriangles, metrics.intersectionSceneTriangles);
    intersectionSceneSpheres = std::max(intersectionSceneSpheres, metrics.intersectionSceneSpheres);
    intersectionScenePlanes = std::max(intersectionScenePlanes, metrics.intersectionScenePlanes);
    intersectionSceneRectangles =
      std::max(intersectionSceneRectangles, metrics.intersectionSceneRectangles);
    intersectionSceneDisks = std::max(intersectionSceneDisks, metrics.intersectionSceneDisks);
    intersectionSceneOpenCylinders =
      std::max(intersectionSceneOpenCylinders, metrics.intersectionSceneOpenCylinders);
    intersectionSceneTori = std::max(intersectionSceneTori, metrics.intersectionSceneTori);
    intersectionSceneTransforms =
      std::max(intersectionSceneTransforms, metrics.intersectionSceneTransforms);
    intersectionSceneUnsupportedPrimitives = std::max(
      intersectionSceneUnsupportedPrimitives, metrics.intersectionSceneUnsupportedPrimitives);
    mergeMapMaximums(intersectionSceneUnsupportedReasons,
                     metrics.intersectionSceneUnsupportedReasons);
    intersectionSceneUploadBytes =
      std::max(intersectionSceneUploadBytes, metrics.intersectionSceneUploadBytes);
    intersectionSceneTriangleClosestHitEligible =
      intersectionSceneTriangleClosestHitEligible ||
      metrics.intersectionSceneTriangleClosestHitEligible;
    intersectionSceneBasicHitEligible =
      intersectionSceneBasicHitEligible || metrics.intersectionSceneBasicHitEligible;
    intersectionScenePackedClosestHitEligible = intersectionScenePackedClosestHitEligible ||
                                                metrics.intersectionScenePackedClosestHitEligible;
    intersectionScenePackedAnyHitEligible =
      intersectionScenePackedAnyHitEligible || metrics.intersectionScenePackedAnyHitEligible;
    tracingSceneCompiled = tracingSceneCompiled || metrics.tracingSceneCompiled;
    tracingSceneMaterials = std::max(tracingSceneMaterials, metrics.tracingSceneMaterials);
    tracingSceneTextures = std::max(tracingSceneTextures, metrics.tracingSceneTextures);
    tracingSceneLights = std::max(tracingSceneLights, metrics.tracingSceneLights);
    tracingSceneEnvironment = std::max(tracingSceneEnvironment, metrics.tracingSceneEnvironment);
    tracingSceneDebugIds = std::max(tracingSceneDebugIds, metrics.tracingSceneDebugIds);
    tracingSceneUnsupportedMaterials =
      std::max(tracingSceneUnsupportedMaterials, metrics.tracingSceneUnsupportedMaterials);
    tracingSceneUnsupportedTextures =
      std::max(tracingSceneUnsupportedTextures, metrics.tracingSceneUnsupportedTextures);
    tracingSceneUnsupportedLights =
      std::max(tracingSceneUnsupportedLights, metrics.tracingSceneUnsupportedLights);
    mergeMapMaximums(tracingSceneUnsupportedMaterialReasons,
                     metrics.tracingSceneUnsupportedMaterialReasons);
    mergeMapMaximums(tracingSceneUnsupportedTextureReasons,
                     metrics.tracingSceneUnsupportedTextureReasons);
    mergeMapMaximums(tracingSceneUnsupportedLightReasons,
                     metrics.tracingSceneUnsupportedLightReasons);
    tracingSceneUploadBytes = std::max(tracingSceneUploadBytes, metrics.tracingSceneUploadBytes);
    intersectionEstimatedRayUploadBytes += metrics.intersectionEstimatedRayUploadBytes;
    intersectionEstimatedClosestHitRayUploadBytes +=
      metrics.intersectionEstimatedClosestHitRayUploadBytes;
    intersectionEstimatedAnyHitRayUploadBytes += metrics.intersectionEstimatedAnyHitRayUploadBytes;
    intersectionEstimatedClosestHitReadbackBytes +=
      metrics.intersectionEstimatedClosestHitReadbackBytes;
    intersectionEstimatedAnyHitReadbackBytes += metrics.intersectionEstimatedAnyHitReadbackBytes;
    intersectionEstimatedQueryTransferBytes += metrics.intersectionEstimatedQueryTransferBytes;
    intersectionEstimatedClosestHitQueryTransferBytes +=
      metrics.intersectionEstimatedClosestHitQueryTransferBytes;
    intersectionEstimatedAnyHitQueryTransferBytes +=
      metrics.intersectionEstimatedAnyHitQueryTransferBytes;
    intersectionEstimatedQueryRoundTrips += metrics.intersectionEstimatedQueryRoundTrips;
    intersectionEstimatedClosestHitQueryRoundTrips +=
      metrics.intersectionEstimatedClosestHitQueryRoundTrips;
    intersectionEstimatedAnyHitQueryRoundTrips +=
      metrics.intersectionEstimatedAnyHitQueryRoundTrips;
    intersectionBackendUploadWorkerSeconds += metrics.intersectionBackendUploadWorkerSeconds;
    intersectionBackendKernelWorkerSeconds += metrics.intersectionBackendKernelWorkerSeconds;
    intersectionBackendReadbackWorkerSeconds += metrics.intersectionBackendReadbackWorkerSeconds;
    intersectionRaysSubmitted += metrics.intersectionRaysSubmitted;
    closestHitRaysSubmitted += metrics.closestHitRaysSubmitted;
    anyHitRaysSubmitted += metrics.anyHitRaysSubmitted;
    closestHitQueries += metrics.closestHitQueries;
    anyHitQueries += metrics.anyHitQueries;
    intersectionBackendPrefersClosestHitBatch = intersectionBackendPrefersClosestHitBatch ||
                                                metrics.intersectionBackendPrefersClosestHitBatch;
    intersectionBackendPrefersAnyHitBatch =
      intersectionBackendPrefersAnyHitBatch || metrics.intersectionBackendPrefersAnyHitBatch;
    intersectionBackendSupportsResidentFrontiers =
      intersectionBackendSupportsResidentFrontiers ||
      metrics.intersectionBackendSupportsResidentFrontiers;
    intersectionBackendSupportsGpuFrontierCompaction =
      intersectionBackendSupportsGpuFrontierCompaction ||
      metrics.intersectionBackendSupportsGpuFrontierCompaction;
    mergeLabel(intersectionBackendGpuFrontierCompactionUnavailableReason,
               metrics.intersectionBackendGpuFrontierCompactionUnavailableReason);
    intersectionBackendSupportsPreparedRayBatchCompaction =
      intersectionBackendSupportsPreparedRayBatchCompaction ||
      metrics.intersectionBackendSupportsPreparedRayBatchCompaction;
    intersectionBackendSupportsResidentDirectLightBatches =
      intersectionBackendSupportsResidentDirectLightBatches ||
      metrics.intersectionBackendSupportsResidentDirectLightBatches;
    mergeLabel(intersectionBackendResidentDirectLightBatchesUnavailableReason,
               metrics.intersectionBackendResidentDirectLightBatchesUnavailableReason);
  }

  void WavefrontRenderMetrics::BatchSummary::addIntegratorMetrics(
    const render::IntegratorBatchMetrics& metrics) {
    addIntersectionBackendMetrics(metrics);
    activeSampleDepthsProcessed += metrics.activeSampleDepthsProcessed;
    frontierCompactionPasses += metrics.frontierCompactionPasses;
    frontierCompactionInputSamples += metrics.frontierCompactionInputSamples;
    frontierCompactionRetainedSamples += metrics.frontierCompactionRetainedSamples;
    frontierCompactionRemovedSamples += metrics.frontierCompactionRemovedSamples;
    frontierCompactionMovedSamples += metrics.frontierCompactionMovedSamples;
    frontierCompactionRetainedIndexBytes += metrics.frontierCompactionRetainedIndexBytes;
    frontierCompactionInputHostPathStateBytes += metrics.frontierCompactionInputHostPathStateBytes;
    frontierCompactionRetainedHostPathStateBytes +=
      metrics.frontierCompactionRetainedHostPathStateBytes;
    frontierCompactionRemovedHostPathStateBytes +=
      metrics.frontierCompactionRemovedHostPathStateBytes;
    mergeLabel(frontierCompactionExecutionPath, metrics.frontierCompactionExecutionPath);
    mergeLabel(frontierCompactionPathStateResidency, metrics.frontierCompactionPathStateResidency);
    if (metrics.residentPathLoopAccumulation) {
      if (!residentPathLoopAccumulation) {
        residentPathLoopAccumulation = *metrics.residentPathLoopAccumulation;
      } else {
        mergeAccumulationDiagnostics(*residentPathLoopAccumulation,
                                     *metrics.residentPathLoopAccumulation);
      }
    }
    mergeLabel(residentPathLoopExecutionPath, metrics.residentPathLoopExecutionPath);
    mergeLabel(residentPathLoopResidency, metrics.residentPathLoopResidency);
    residentPathLoopDepths += metrics.residentPathLoopDepths;
    residentPathLoopInputPaths += metrics.residentPathLoopInputPaths;
    residentPathLoopRetainedPaths += metrics.residentPathLoopRetainedPaths;
    residentPathLoopRemovedPaths += metrics.residentPathLoopRemovedPaths;
    residentPathLoopMovedPaths += metrics.residentPathLoopMovedPaths;
    residentPathLoopRetainedIndexBytes += metrics.residentPathLoopRetainedIndexBytes;
    residentPathLoopResidentPathStateBytes = std::max(
      residentPathLoopResidentPathStateBytes, metrics.residentPathLoopResidentPathStateBytes);
    residentPathLoopInputResidentPathStateBytes +=
      metrics.residentPathLoopInputResidentPathStateBytes;
    residentPathLoopRetainedResidentPathStateBytes +=
      metrics.residentPathLoopRetainedResidentPathStateBytes;
    residentPathLoopRemovedResidentPathStateBytes +=
      metrics.residentPathLoopRemovedResidentPathStateBytes;
    residentPathLoopCompactionPasses += metrics.residentPathLoopCompactionPasses;
    residentPathLoopRoundTrips += metrics.residentPathLoopRoundTrips;
    residentPathLoopSavedHostReadbacks += metrics.residentPathLoopSavedHostReadbacks;
    residentPathLoopSavedHostReadbackBytes += metrics.residentPathLoopSavedHostReadbackBytes;
    compatibilityShadeSamples += metrics.compatibilityShadeSamples;
    unsupportedPathMaterialSamples += metrics.unsupportedPathMaterialSamples;
    emitterHitSamples += metrics.emitterHitSamples;
    primaryEmitterHitSamples += metrics.primaryEmitterHitSamples;
    deltaEmitterHitSamples += metrics.deltaEmitterHitSamples;
    bsdfEmitterHitSamples += metrics.bsdfEmitterHitSamples;
    misWeightedEmitterHitSamples += metrics.misWeightedEmitterHitSamples;
    directLightSamples += metrics.directLightSamples;
    directLightContributingSamples += metrics.directLightContributingSamples;
    directLightOccludedSamples += metrics.directLightOccludedSamples;
    emittedRadianceLuminanceSum += metrics.emittedRadianceLuminanceSum;
    directLightRadianceLuminanceSum += metrics.directLightRadianceLuminanceSum;
    primaryDirectLightRadianceLuminanceSum += metrics.primaryDirectLightRadianceLuminanceSum;
    secondaryDirectLightRadianceLuminanceSum += metrics.secondaryDirectLightRadianceLuminanceSum;
    ambientRadianceLuminanceSum += metrics.ambientRadianceLuminanceSum;
    missRadianceLuminanceSum += metrics.missRadianceLuminanceSum;
    compatibilityShadeRadianceLuminanceSum += metrics.compatibilityShadeRadianceLuminanceSum;
    activeHostPathStateBytesProcessed += metrics.activeHostPathStateBytesProcessed;
    activeHitHostBytesProcessed += metrics.activeHitHostBytesProcessed;
    spawnedContinuationSamples += metrics.spawnedContinuationSamples;
    spawnedContinuationHostPathStateBytes += metrics.spawnedContinuationHostPathStateBytes;

    const auto addCounts = [](std::vector<std::uint64_t>& target,
                              const std::vector<std::uint64_t>& source) {
      if (target.size() < source.size()) {
        target.resize(source.size());
      }
      for (std::size_t depth = 0; depth != source.size(); ++depth) {
        target[depth] += source[depth];
      }
    };
    addCounts(activeSamplesPerDepth, metrics.activeSamplesPerDepth);
    addCounts(retainedActiveSamplesPerDepth, metrics.retainedActiveSamplesPerDepth);
    addCounts(activeHostPathStateBytesPerDepth, metrics.activeHostPathStateBytesPerDepth);
    addCounts(activeHitHostBytesPerDepth, metrics.activeHitHostBytesPerDepth);
    addCounts(retainedHostPathStateBytesPerDepth, metrics.retainedHostPathStateBytesPerDepth);
    addCounts(spawnedContinuationSamplesPerDepth, metrics.spawnedContinuationSamplesPerDepth);
    addCounts(spawnedContinuationHostPathStateBytesPerDepth,
              metrics.spawnedContinuationHostPathStateBytesPerDepth);
    addCounts(frontierRayHitsPerDepth, metrics.frontierRayHitsPerDepth);
    addCounts(frontierRayMissesPerDepth, metrics.frontierRayMissesPerDepth);
    addCounts(frontierPacketChunksPerDepth, metrics.frontierPacketChunksPerDepth);
    addCounts(frontierPacketRaysPerDepth, metrics.frontierPacketRaysPerDepth);
    addCounts(frontierClosestHitBatchChunksPerDepth, metrics.frontierClosestHitBatchChunksPerDepth);
    addCounts(frontierClosestHitBatchRaysPerDepth, metrics.frontierClosestHitBatchRaysPerDepth);
    addCounts(directLightAnyHitBatchChunksPerDepth, metrics.directLightAnyHitBatchChunksPerDepth);
    addCounts(directLightAnyHitBatchRaysPerDepth, metrics.directLightAnyHitBatchRaysPerDepth);
    addCounts(directLightSelectionHostBytesPerDepth, metrics.directLightSelectionHostBytesPerDepth);
    addCounts(directLightOcclusionHostBytesPerDepth, metrics.directLightOcclusionHostBytesPerDepth);
    addCounts(directLightContributionHostBytesPerDepth,
              metrics.directLightContributionHostBytesPerDepth);
    addCounts(directLightAnyHitFrontierPackedRayBytesPerDepth,
              metrics.directLightAnyHitFrontierPackedRayBytesPerDepth);
    addCounts(directLightAnyHitFrontierHostQueryBytesPerDepth,
              metrics.directLightAnyHitFrontierHostQueryBytesPerDepth);
    addCounts(directLightAnyHitFrontierStateHandleBytesPerDepth,
              metrics.directLightAnyHitFrontierStateHandleBytesPerDepth);
    addCounts(frontierRay4PacketChunksPerDepth, metrics.frontierRay4PacketChunksPerDepth);
    addCounts(frontierRay8PacketChunksPerDepth, metrics.frontierRay8PacketChunksPerDepth);
    addCounts(frontierScalarRaysPerDepth, metrics.frontierScalarRaysPerDepth);
    addCounts(frontierPacketScalarFallbackRaysPerDepth,
              metrics.frontierPacketScalarFallbackRaysPerDepth);
    for (const auto& [reason, count] : metrics.frontierPacketScalarFallbackRaysByReason) {
      frontierPacketScalarFallbackRaysByReason[reason] += count;
    }
    addCounts(frontierPacketRefinedRaysPerDepth, metrics.frontierPacketRefinedRaysPerDepth);
    for (const auto& [material, count] : metrics.frontierPacketRefinedRaysByMaterial) {
      frontierPacketRefinedRaysByMaterial[material] += count;
    }

    if (radianceDeltaSquaredSumPerDepth.size() < metrics.radianceDeltaSquaredSumPerDepth.size()) {
      radianceDeltaSquaredSumPerDepth.resize(metrics.radianceDeltaSquaredSumPerDepth.size());
    }
    for (std::size_t depth = 0; depth != metrics.radianceDeltaSquaredSumPerDepth.size(); ++depth) {
      radianceDeltaSquaredSumPerDepth[depth] += metrics.radianceDeltaSquaredSumPerDepth[depth];
    }

    if (maxRadianceDeltaPerDepth.size() < metrics.maxRadianceDeltaPerDepth.size()) {
      maxRadianceDeltaPerDepth.resize(metrics.maxRadianceDeltaPerDepth.size());
    }
    for (std::size_t depth = 0; depth != metrics.maxRadianceDeltaPerDepth.size(); ++depth) {
      maxRadianceDeltaPerDepth[depth] =
        std::max(maxRadianceDeltaPerDepth[depth], metrics.maxRadianceDeltaPerDepth[depth]);
    }
  }

  double WavefrontRenderMetrics::BatchSummary::frontierCompactionRemovedSampleFraction() const {
    if (frontierCompactionInputSamples == 0) {
      return 0.0;
    }
    return static_cast<double>(frontierCompactionRemovedSamples) /
           static_cast<double>(frontierCompactionInputSamples);
  }

  double
  WavefrontRenderMetrics::BatchSummary::frontierCompactionMovedRetainedSampleFraction() const {
    if (frontierCompactionRetainedSamples == 0) {
      return 0.0;
    }
    return static_cast<double>(frontierCompactionMovedSamples) /
           static_cast<double>(frontierCompactionRetainedSamples);
  }

  bool WavefrontRenderMetrics::BatchSummary::hasCompactionCandidateDepth(std::size_t depth) const {
    if (depth >= activeSamplesPerDepth.size() || depth >= retainedActiveSamplesPerDepth.size()) {
      return false;
    }
    return activeSamplesPerDepth[depth] > retainedActiveSamplesPerDepth[depth];
  }

  std::uint64_t
  WavefrontRenderMetrics::BatchSummary::compactionCandidateSamplesAtDepth(std::size_t depth) const {
    if (!hasCompactionCandidateDepth(depth)) {
      return 0;
    }
    return activeSamplesPerDepth[depth] - retainedActiveSamplesPerDepth[depth];
  }

  std::uint64_t WavefrontRenderMetrics::BatchSummary::compactionCandidateDepthCount() const {
    const std::size_t depthCount =
      std::min(activeSamplesPerDepth.size(), retainedActiveSamplesPerDepth.size());
    std::uint64_t count = 0;
    for (std::size_t depth = 0; depth != depthCount; ++depth) {
      if (hasCompactionCandidateDepth(depth)) {
        ++count;
      }
    }
    return count;
  }

  std::uint64_t WavefrontRenderMetrics::BatchSummary::compactionCandidateSampleCount() const {
    const std::size_t depthCount =
      std::min(activeSamplesPerDepth.size(), retainedActiveSamplesPerDepth.size());
    std::uint64_t count = 0;
    for (std::size_t depth = 0; depth != depthCount; ++depth) {
      count += compactionCandidateSamplesAtDepth(depth);
    }
    return count;
  }

  std::uint64_t WavefrontRenderMetrics::BatchSummary::compactionCandidatePackedRayBytes() const {
    return compactionCandidateSampleCount() * sizeof(render::GpuIntersectionRay);
  }

  std::uint64_t WavefrontRenderMetrics::BatchSummary::compactionCandidateStateHandleBytes() const {
    return compactionCandidateSampleCount() * sizeof(render::State*);
  }

  std::uint64_t
  WavefrontRenderMetrics::BatchSummary::compactionCandidateHostPathStateBytes() const {
    const std::size_t depthCount =
      std::min(activeHostPathStateBytesPerDepth.size(), retainedHostPathStateBytesPerDepth.size());
    std::uint64_t bytes = 0;
    for (std::size_t depth = 0; depth != depthCount; ++depth) {
      if (activeHostPathStateBytesPerDepth[depth] > retainedHostPathStateBytesPerDepth[depth]) {
        bytes +=
          activeHostPathStateBytesPerDepth[depth] - retainedHostPathStateBytesPerDepth[depth];
      }
    }
    return bytes;
  }

  double WavefrontRenderMetrics::BatchSummary::compactionCandidateSampleFraction() const {
    if (activeSampleDepthsProcessed == 0) {
      return 0.0;
    }
    return static_cast<double>(compactionCandidateSampleCount()) /
           static_cast<double>(activeSampleDepthsProcessed);
  }

  std::uint64_t WavefrontRenderMetrics::BatchSummary::largestCompactionCandidateDepth() const {
    const std::size_t depthCount =
      std::min(activeSamplesPerDepth.size(), retainedActiveSamplesPerDepth.size());
    std::uint64_t largestDepth = 0;
    std::uint64_t largestSamples = 0;
    for (std::size_t depth = 0; depth != depthCount; ++depth) {
      const std::uint64_t samples = compactionCandidateSamplesAtDepth(depth);
      if (samples > largestSamples) {
        largestSamples = samples;
        largestDepth = static_cast<std::uint64_t>(depth);
      }
    }
    return largestDepth;
  }

  std::uint64_t
  WavefrontRenderMetrics::BatchSummary::largestCompactionCandidateSampleCount() const {
    const std::size_t depthCount =
      std::min(activeSamplesPerDepth.size(), retainedActiveSamplesPerDepth.size());
    std::uint64_t largestSamples = 0;
    for (std::size_t depth = 0; depth != depthCount; ++depth) {
      largestSamples = std::max(largestSamples, compactionCandidateSamplesAtDepth(depth));
    }
    return largestSamples;
  }

  std::uint64_t
  WavefrontRenderMetrics::BatchSummary::largestCompactionCandidatePackedRayBytes() const {
    return largestCompactionCandidateSampleCount() * sizeof(render::GpuIntersectionRay);
  }

  std::uint64_t
  WavefrontRenderMetrics::BatchSummary::largestCompactionCandidateStateHandleBytes() const {
    return largestCompactionCandidateSampleCount() * sizeof(render::State*);
  }

  std::uint64_t
  WavefrontRenderMetrics::BatchSummary::largestCompactionCandidateHostPathStateBytes() const {
    const std::uint64_t depth = largestCompactionCandidateDepth();
    if (depth >= activeHostPathStateBytesPerDepth.size() ||
        depth >= retainedHostPathStateBytesPerDepth.size() ||
        activeHostPathStateBytesPerDepth[depth] <= retainedHostPathStateBytesPerDepth[depth]) {
      return 0;
    }
    return activeHostPathStateBytesPerDepth[depth] - retainedHostPathStateBytesPerDepth[depth];
  }

  double WavefrontRenderMetrics::BatchSummary::largestCompactionCandidateSampleFraction() const {
    const std::uint64_t samples = largestCompactionCandidateSampleCount();
    if (samples == 0) {
      return 0.0;
    }
    const std::uint64_t depth = largestCompactionCandidateDepth();
    if (depth >= activeSamplesPerDepth.size() || activeSamplesPerDepth[depth] == 0) {
      return 0.0;
    }
    return static_cast<double>(samples) / static_cast<double>(activeSamplesPerDepth[depth]);
  }

  bool WavefrontRenderMetrics::BatchSummary::hasMixedQueryDepth(std::size_t depth) const {
    const std::uint64_t closestHitChunks = depth < frontierClosestHitBatchChunksPerDepth.size()
                                             ? frontierClosestHitBatchChunksPerDepth[depth]
                                             : 0;
    const std::uint64_t anyHitChunks = depth < directLightAnyHitBatchChunksPerDepth.size()
                                         ? directLightAnyHitBatchChunksPerDepth[depth]
                                         : 0;
    return closestHitChunks > 0 && anyHitChunks > 0;
  }

  std::uint64_t WavefrontRenderMetrics::BatchSummary::frontierQueryRoundTrips() const {
    std::uint64_t roundTrips = 0;
    for (const std::uint64_t chunks : frontierClosestHitBatchChunksPerDepth) {
      roundTrips += chunks;
    }
    for (const std::uint64_t chunks : directLightAnyHitBatchChunksPerDepth) {
      roundTrips += chunks;
    }
    return roundTrips;
  }

  std::uint64_t
  WavefrontRenderMetrics::BatchSummary::residentFrontierQueryRoundTripsEstimate() const {
    return frontierQueryRoundTrips() - residentFrontierQueryRoundTripSavingsEstimate();
  }

  std::uint64_t
  WavefrontRenderMetrics::BatchSummary::residentFrontierQueryRoundTripSavingsEstimate() const {
    const std::uint64_t mixedRoundTrips = mixedQueryDepthRoundTrips();
    const std::uint64_t mixedResidentBoundaries = mixedQueryDepthCount();
    if (mixedRoundTrips <= mixedResidentBoundaries) {
      return 0;
    }
    return mixedRoundTrips - mixedResidentBoundaries;
  }

  std::uint64_t WavefrontRenderMetrics::BatchSummary::directLightAnyHitQueryRoundTrips() const {
    std::uint64_t roundTrips = 0;
    for (const std::uint64_t chunks : directLightAnyHitBatchChunksPerDepth) {
      roundTrips += chunks;
    }
    return roundTrips;
  }

  std::uint64_t
  WavefrontRenderMetrics::BatchSummary::residentDirectLightBatchRoundTripsEstimate() const {
    return directLightAnyHitQueryRoundTrips() - residentDirectLightBatchRoundTripSavingsEstimate();
  }

  std::uint64_t
  WavefrontRenderMetrics::BatchSummary::residentDirectLightBatchRoundTripSavingsEstimate() const {
    return directLightAnyHitQueryRoundTrips();
  }

  std::uint64_t WavefrontRenderMetrics::BatchSummary::mixedQueryDepthCount() const {
    const std::size_t depthCount = std::max(frontierClosestHitBatchChunksPerDepth.size(),
                                            directLightAnyHitBatchChunksPerDepth.size());
    std::uint64_t count = 0;
    for (std::size_t depth = 0; depth != depthCount; ++depth) {
      if (hasMixedQueryDepth(depth)) {
        ++count;
      }
    }
    return count;
  }

  std::uint64_t WavefrontRenderMetrics::BatchSummary::mixedQueryDepthRoundTrips() const {
    const std::size_t depthCount = std::max(frontierClosestHitBatchChunksPerDepth.size(),
                                            directLightAnyHitBatchChunksPerDepth.size());
    std::uint64_t roundTrips = 0;
    for (std::size_t depth = 0; depth != depthCount; ++depth) {
      if (hasMixedQueryDepth(depth)) {
        if (depth < frontierClosestHitBatchChunksPerDepth.size()) {
          roundTrips += frontierClosestHitBatchChunksPerDepth[depth];
        }
        if (depth < directLightAnyHitBatchChunksPerDepth.size()) {
          roundTrips += directLightAnyHitBatchChunksPerDepth[depth];
        }
      }
    }
    return roundTrips;
  }

  std::uint64_t WavefrontRenderMetrics::BatchSummary::mixedQueryDepthRays() const {
    return mixedQueryDepthClosestHitRays() + mixedQueryDepthAnyHitRays();
  }

  std::uint64_t WavefrontRenderMetrics::BatchSummary::mixedQueryDepthClosestHitRays() const {
    const std::size_t depthCount = std::max(frontierClosestHitBatchRaysPerDepth.size(),
                                            directLightAnyHitBatchChunksPerDepth.size());
    std::uint64_t rays = 0;
    for (std::size_t depth = 0; depth != depthCount; ++depth) {
      if (hasMixedQueryDepth(depth) && depth < frontierClosestHitBatchRaysPerDepth.size()) {
        rays += frontierClosestHitBatchRaysPerDepth[depth];
      }
    }
    return rays;
  }

  std::uint64_t WavefrontRenderMetrics::BatchSummary::mixedQueryDepthAnyHitRays() const {
    const std::size_t depthCount = std::max(frontierClosestHitBatchChunksPerDepth.size(),
                                            directLightAnyHitBatchRaysPerDepth.size());
    std::uint64_t rays = 0;
    for (std::size_t depth = 0; depth != depthCount; ++depth) {
      if (hasMixedQueryDepth(depth) && depth < directLightAnyHitBatchRaysPerDepth.size()) {
        rays += directLightAnyHitBatchRaysPerDepth[depth];
      }
    }
    return rays;
  }

  std::uint64_t WavefrontRenderMetrics::BatchSummary::mixedQueryDepthReadbackBytes() const {
    return mixedQueryDepthClosestHitReadbackBytes() + mixedQueryDepthAnyHitReadbackBytes();
  }

  std::uint64_t
  WavefrontRenderMetrics::BatchSummary::mixedQueryDepthClosestHitReadbackBytes() const {
    return mixedQueryDepthClosestHitRays() * sizeof(render::GpuIntersectionHitRecord);
  }

  std::uint64_t WavefrontRenderMetrics::BatchSummary::mixedQueryDepthAnyHitReadbackBytes() const {
    return mixedQueryDepthAnyHitRays() * sizeof(render::GpuIntersectionOcclusionRecord);
  }

  double WavefrontRenderMetrics::BatchSummary::intersectionBackendKernelRaysPerSecond() const {
    if (intersectionBackendKernelWorkerSeconds == 0.0) {
      return 0.0;
    }
    return static_cast<double>(intersectionRaysSubmitted) / intersectionBackendKernelWorkerSeconds;
  }

  render::TracingExecutionCapabilityRecords
  WavefrontRenderMetrics::BatchSummary::tracingExecutionCapabilities() const {
    using Domain = render::TracingExecutionDomain;
    using Device = render::TracingExecutionDevice;

    render::TracingExecutionCapabilityRecords records;
    records.intersection.closestHit = tracingRecordFromExecutionLabels(
      Domain::Intersection, "geometry.closest_hit", intersectionBackendRequest, intersectionBackend,
      intersectionBackendPlatform, intersectionBackendAvailability,
      intersectionBackendFallbackReason, intersectionBackendClosestHitExecutionPath);
    records.intersection.anyHit = tracingRecordFromExecutionLabels(
      Domain::Intersection, "geometry.any_hit", intersectionBackendRequest, intersectionBackend,
      intersectionBackendPlatform, intersectionBackendAvailability,
      intersectionBackendFallbackReason, intersectionBackendAnyHitExecutionPath);

    if (intersectionSceneCompiled) {
      std::string reason = "compiled intersection subset";
      if (intersectionSceneUnsupportedPrimitives != 0) {
        reason = "compiled scene has unsupported primitives";
      }
      records.scene.geometryRecords = render::TracingCapabilityRecord::restricted(
        Domain::SceneRecords, "scene.geometry_records",
        executionDeviceForLabel(intersectionBackendExecutionPath), intersectionBackendExecutionPath,
        reason);
    } else {
      records.scene.geometryRecords = render::TracingCapabilityRecord::cpu(
        Domain::SceneRecords, "scene.geometry_records",
        intersectionBackendExecutionPath.empty() ? "runtime_scene"
                                                 : intersectionBackendExecutionPath);
    }
    records.scene.materialRecords =
      render::TracingCapabilityRecord::cpu(Domain::SceneRecords, "scene.material_records");
    records.scene.textureRecords =
      render::TracingCapabilityRecord::cpu(Domain::SceneRecords, "scene.texture_records");
    records.scene.lightRecords =
      render::TracingCapabilityRecord::cpu(Domain::SceneRecords, "scene.light_records");

    records.sampling.gpuRng = render::TracingCapabilityRecord::unsupported(
      Domain::Sampling, "sampling.gpu_rng", "GPU sample stream is not implemented");
    records.sampling.namedDimensions =
      render::TracingCapabilityRecord::cpu(Domain::Sampling, "sampling.named_dimensions");

    records.directLighting.lightSampling =
      render::TracingCapabilityRecord::cpu(Domain::DirectLighting, "lighting.direct_light_sample");
    records.directLighting.visibility = records.intersection.anyHit;
    records.directLighting.visibility.domain = Domain::DirectLighting;
    records.directLighting.visibility.name = "lighting.direct_light_visibility";
    const std::string contributionPath =
      directLightContributionExecutionPath.empty() ? "cpu" : directLightContributionExecutionPath;
    if (!directLightContributionFallbackReason.empty()) {
      records.directLighting.contribution = render::TracingCapabilityRecord::fallbackRecord(
        Domain::DirectLighting, "lighting.direct_light_contribution", Device::GPU, Device::CPU,
        contributionPath, directLightContributionFallbackReason);
    } else {
      records.directLighting.contribution = render::TracingCapabilityRecord::cpu(
        Domain::DirectLighting, "lighting.direct_light_contribution", contributionPath);
    }
    if (intersectionBackendSupportsResidentDirectLightBatches) {
      const std::string residentDirectLightPath =
        !intersectionBackendAnyHitFrontierResidency.empty()
          ? intersectionBackendAnyHitFrontierResidency
          : (intersectionBackendAnyHitExecutionPath.empty()
               ? "gpu_resident_direct_light_batch"
               : intersectionBackendAnyHitExecutionPath);
      records.directLighting.residentBatch = tracingRecordForResolvedExecutionPath(
        Domain::DirectLighting, "lighting.resident_direct_light_batches",
        executionDeviceForLabel(residentDirectLightPath), intersectionBackendPlatform,
        residentDirectLightPath);
    } else {
      records.directLighting.residentBatch = render::TracingCapabilityRecord::unsupported(
        Domain::DirectLighting, "lighting.resident_direct_light_batches",
        intersectionBackendResidentDirectLightBatchesUnavailableReason.empty()
          ? "resident direct-light batches are not implemented"
          : intersectionBackendResidentDirectLightBatchesUnavailableReason);
    }

    records.bsdf.eval = render::TracingCapabilityRecord::cpu(Domain::BSDF, "shading.bsdf_eval");
    records.bsdf.sample = render::TracingCapabilityRecord::cpu(Domain::BSDF, "shading.bsdf_sample");
    records.bsdf.deltaBranches =
      render::TracingCapabilityRecord::cpu(Domain::BSDF, "shading.delta_branches");

    const std::string pathStateResidency =
      !residentPathLoopResidency.empty()
        ? residentPathLoopResidency
        : (frontierCompactionPathStateResidency.empty() ? "host"
                                                        : frontierCompactionPathStateResidency);
    records.pathState.residency = tracingRecordForResolvedExecutionPath(
      Domain::PathState, "state.path_state_residency", executionDeviceForLabel(pathStateResidency),
      intersectionBackendPlatform, pathStateResidency);

    const bool residentLoopCompactionReported =
      frontierCompactionExecutionPath.empty() && residentPathLoopCompactionPasses != 0;
    const std::string compactionPath =
      residentLoopCompactionReported
        ? (residentPathLoopExecutionPath.empty() ? "host" : residentPathLoopExecutionPath)
        : (frontierCompactionExecutionPath.empty() ? "host" : frontierCompactionExecutionPath);
    const Device compactionDevice =
      residentLoopCompactionReported && !residentPathLoopResidency.empty()
        ? executionDeviceForLabel(residentPathLoopResidency)
        : executionDeviceForLabel(compactionPath);
    records.pathState.frontierCompaction = tracingRecordForResolvedExecutionPath(
      Domain::PathState, "state.frontier_compaction", compactionDevice, intersectionBackendPlatform,
      compactionPath);
    records.pathState.spawnedContinuations =
      render::TracingCapabilityRecord::cpu(Domain::PathState, "state.spawned_continuations");

    records.accumulation.sampleAccumulation = render::TracingCapabilityRecord::cpu(
      Domain::Accumulation, "accumulation.sample_accumulation");
    records.accumulation.progressiveReadback = render::TracingCapabilityRecord::cpu(
      Domain::Accumulation, "accumulation.progressive_readback");

    return records;
  }

  void WavefrontRenderMetrics::TilingSummary::resetFromTilePlan(const render::TilePlan& tilePlan) {
    *this = TilingSummary();
    tileCount = tilePlan.size();
    tileRows = static_cast<std::uint64_t>(std::max(0, tilePlan.rows()));
    tileColumns = static_cast<std::uint64_t>(std::max(0, tilePlan.cols()));
    maxTileWidth = static_cast<std::uint64_t>(std::max(0, tilePlan.maxTileWidth()));
    maxTileHeight = static_cast<std::uint64_t>(std::max(0, tilePlan.maxTileHeight()));
    maxTilePixels = static_cast<std::uint64_t>(std::max(0, tilePlan.maxTilePixels()));
    averageTilePixels = tilePlan.averageTilePixels();
  }

  double WavefrontRenderMetrics::intersectionRaysPerWorkerSecond() const {
    if (timings.integratorIntersectionWorkerSeconds == 0.0) {
      return 0.0;
    }
    return static_cast<double>(batching.intersectionRaysSubmitted) /
           timings.integratorIntersectionWorkerSeconds;
  }

  QJsonObject WavefrontRenderMetrics::toJson() const {
    QJsonObject inputJson;
    inputJson["width"] = input.width;
    inputJson["height"] = input.height;
    inputJson["samplesPerPixel"] = input.samplesPerPixel;
    if (input.samplingSeed) {
      inputJson["samplingSeed"] = static_cast<double>(*input.samplingSeed);
    }
    inputJson["sampleStreamMode"] = QString::fromStdString(input.sampleStreamMode);
    inputJson["renderedPixels"] = static_cast<double>(input.renderedPixels);
    inputJson["primarySamples"] = static_cast<double>(input.primarySamples);

    QJsonObject tilingJson;
    tilingJson["tileCount"] = static_cast<double>(tiling.tileCount);
    tilingJson["tileRows"] = static_cast<double>(tiling.tileRows);
    tilingJson["tileColumns"] = static_cast<double>(tiling.tileColumns);
    tilingJson["maxTileWidth"] = static_cast<double>(tiling.maxTileWidth);
    tilingJson["maxTileHeight"] = static_cast<double>(tiling.maxTileHeight);
    tilingJson["maxTilePixels"] = static_cast<double>(tiling.maxTilePixels);
    tilingJson["averageTilePixels"] = tiling.averageTilePixels;
    tilingJson["nonEmptyTileCount"] = static_cast<double>(tiling.nonEmptyTileCount);
    tilingJson["minNonEmptyTileSamples"] = static_cast<double>(tiling.minNonEmptyTileSamples);
    tilingJson["maxTileSamples"] = static_cast<double>(tiling.maxTileSamples);
    tilingJson["averageNonEmptyTileSamples"] = tiling.averageNonEmptyTileSamples;

    QJsonObject schedulingJson;
    schedulingJson["configuredQueueSize"] = static_cast<double>(scheduling.configuredQueueSize);
    schedulingJson["resolvedQueueSize"] = static_cast<double>(scheduling.resolvedQueueSize);
    schedulingJson["decision"] = QString::fromStdString(scheduling.decision);

    const render::TracingAccumulationDiagnostics& accumulationDiagnostics =
      accumulation.diagnostics;
    const render::TracingAccumulationLayout& accumulationLayout = accumulationDiagnostics.layout;
    QJsonObject accumulationJson;
    accumulationJson["backend"] = QString::fromStdString(accumulationDiagnostics.backend);
    accumulationJson["residency"] = QString::fromStdString(accumulationDiagnostics.residency);
    accumulationJson["width"] = accumulationLayout.width;
    accumulationJson["height"] = accumulationLayout.height;
    if (accumulationLayout.hasImageShape()) {
      accumulationJson["pixelCount"] = static_cast<double>(accumulationLayout.pixelCount());
      accumulationJson["colorSumFormat"] =
        QString::fromLatin1(render::toString(accumulationLayout.colorSumFormat));
      accumulationJson["sampleCountFormat"] =
        QString::fromLatin1(render::toString(accumulationLayout.sampleCountFormat));
      accumulationJson["momentFormat"] =
        QString::fromLatin1(render::toString(accumulationLayout.momentFormat));
      accumulationJson["resolveFormat"] =
        QString::fromLatin1(render::toString(accumulationLayout.resolveFormat));
      accumulationJson["colorSumBytes"] = static_cast<double>(accumulationLayout.colorSumBytes());
      accumulationJson["sampleCountBytes"] =
        static_cast<double>(accumulationLayout.sampleCountBytes());
      accumulationJson["momentBytes"] = static_cast<double>(accumulationLayout.momentBytes());
      accumulationJson["resolveBytes"] = static_cast<double>(accumulationLayout.resolveBytes());
      accumulationJson["accumulationBytes"] =
        static_cast<double>(accumulationLayout.accumulationBytes());
      accumulationJson["totalBytes"] = static_cast<double>(accumulationLayout.totalBytes());
    }
    accumulationJson["residentBytes"] = static_cast<double>(accumulationDiagnostics.residentBytes);
    accumulationJson["clearOperations"] =
      static_cast<double>(accumulationDiagnostics.clearOperations);
    accumulationJson["addOperations"] = static_cast<double>(accumulationDiagnostics.addOperations);
    accumulationJson["addedSamples"] = static_cast<double>(accumulationDiagnostics.addedSamples);
    accumulationJson["resolveOperations"] =
      static_cast<double>(accumulationDiagnostics.resolveOperations);
    accumulationJson["readbackOperations"] =
      static_cast<double>(accumulationDiagnostics.readbackOperations);
    accumulationJson["readbackBytes"] = static_cast<double>(accumulationDiagnostics.readbackBytes);

    QJsonObject batchingJson;
    const auto integerArray = [](const std::vector<std::uint64_t>& values) {
      QJsonArray result;
      for (const std::uint64_t value : values) {
        result.push_back(static_cast<double>(value));
      }
      return result;
    };
    const QJsonArray activeSamplesPerDepth = integerArray(batching.activeSamplesPerDepth);
    const QJsonArray retainedActiveSamplesPerDepth =
      integerArray(batching.retainedActiveSamplesPerDepth);
    const QJsonArray activeHostPathStateBytesPerDepth =
      integerArray(batching.activeHostPathStateBytesPerDepth);
    const QJsonArray activeHitHostBytesPerDepth = integerArray(batching.activeHitHostBytesPerDepth);
    const QJsonArray retainedHostPathStateBytesPerDepth =
      integerArray(batching.retainedHostPathStateBytesPerDepth);
    const QJsonArray spawnedContinuationSamplesPerDepth =
      integerArray(batching.spawnedContinuationSamplesPerDepth);
    const QJsonArray spawnedContinuationHostPathStateBytesPerDepth =
      integerArray(batching.spawnedContinuationHostPathStateBytesPerDepth);
    const QJsonArray frontierRayHitsPerDepth = integerArray(batching.frontierRayHitsPerDepth);
    const QJsonArray frontierRayMissesPerDepth = integerArray(batching.frontierRayMissesPerDepth);
    const QJsonArray frontierPacketChunksPerDepth =
      integerArray(batching.frontierPacketChunksPerDepth);
    const QJsonArray frontierPacketRaysPerDepth = integerArray(batching.frontierPacketRaysPerDepth);
    const QJsonArray frontierClosestHitBatchChunksPerDepth =
      integerArray(batching.frontierClosestHitBatchChunksPerDepth);
    const QJsonArray frontierClosestHitBatchRaysPerDepth =
      integerArray(batching.frontierClosestHitBatchRaysPerDepth);
    const QJsonArray directLightAnyHitBatchChunksPerDepth =
      integerArray(batching.directLightAnyHitBatchChunksPerDepth);
    const QJsonArray directLightAnyHitBatchRaysPerDepth =
      integerArray(batching.directLightAnyHitBatchRaysPerDepth);
    const QJsonArray directLightSelectionHostBytesPerDepth =
      integerArray(batching.directLightSelectionHostBytesPerDepth);
    const QJsonArray directLightOcclusionHostBytesPerDepth =
      integerArray(batching.directLightOcclusionHostBytesPerDepth);
    const QJsonArray directLightContributionHostBytesPerDepth =
      integerArray(batching.directLightContributionHostBytesPerDepth);
    const QJsonArray directLightAnyHitFrontierPackedRayBytesPerDepth =
      integerArray(batching.directLightAnyHitFrontierPackedRayBytesPerDepth);
    const QJsonArray directLightAnyHitFrontierHostQueryBytesPerDepth =
      integerArray(batching.directLightAnyHitFrontierHostQueryBytesPerDepth);
    const QJsonArray directLightAnyHitFrontierStateHandleBytesPerDepth =
      integerArray(batching.directLightAnyHitFrontierStateHandleBytesPerDepth);
    const QJsonArray frontierRay4PacketChunksPerDepth =
      integerArray(batching.frontierRay4PacketChunksPerDepth);
    const QJsonArray frontierRay8PacketChunksPerDepth =
      integerArray(batching.frontierRay8PacketChunksPerDepth);
    const QJsonArray frontierScalarRaysPerDepth = integerArray(batching.frontierScalarRaysPerDepth);
    const QJsonArray frontierPacketScalarFallbackRaysPerDepth =
      integerArray(batching.frontierPacketScalarFallbackRaysPerDepth);
    const QJsonArray frontierPacketRefinedRaysPerDepth =
      integerArray(batching.frontierPacketRefinedRaysPerDepth);
    const auto integerObject = [](const std::map<std::string, std::uint64_t>& values) {
      QJsonObject result;
      for (const auto& [key, count] : values) {
        result[QString::fromStdString(key)] = static_cast<double>(count);
      }
      return result;
    };
    const QJsonObject frontierPacketScalarFallbackRaysByReason =
      integerObject(batching.frontierPacketScalarFallbackRaysByReason);
    const QJsonObject frontierPacketRefinedRaysByMaterial =
      integerObject(batching.frontierPacketRefinedRaysByMaterial);
    const QJsonObject intersectionSceneUnsupportedReasons =
      integerObject(batching.intersectionSceneUnsupportedReasons);
    const QJsonObject tracingSceneUnsupportedMaterialReasons =
      integerObject(batching.tracingSceneUnsupportedMaterialReasons);
    const QJsonObject tracingSceneUnsupportedTextureReasons =
      integerObject(batching.tracingSceneUnsupportedTextureReasons);
    const QJsonObject tracingSceneUnsupportedLightReasons =
      integerObject(batching.tracingSceneUnsupportedLightReasons);
    QJsonArray radianceDeltaL2PerDepth;
    QJsonArray radianceDeltaRmsPerDepth;
    for (std::size_t depth = 0; depth != batching.radianceDeltaSquaredSumPerDepth.size(); ++depth) {
      const double squaredSum = batching.radianceDeltaSquaredSumPerDepth[depth];
      radianceDeltaL2PerDepth.push_back(std::sqrt(squaredSum));
      const std::uint64_t activeSamples =
        depth < batching.activeSamplesPerDepth.size() ? batching.activeSamplesPerDepth[depth] : 0;
      radianceDeltaRmsPerDepth.push_back(
        activeSamples == 0 ? 0.0 : std::sqrt(squaredSum / static_cast<double>(activeSamples)));
    }
    QJsonArray maxRadianceDeltaPerDepth;
    for (const double delta : batching.maxRadianceDeltaPerDepth) {
      maxRadianceDeltaPerDepth.push_back(delta);
    }
    batchingJson["integrator"] = QString::fromStdString(batching.integrator);
    batchingJson["executionMode"] = QString::fromStdString(batching.executionMode);
    batchingJson["intersectionBackendRequest"] =
      QString::fromStdString(batching.intersectionBackendRequest);
    batchingJson["intersectionBackend"] = QString::fromStdString(batching.intersectionBackend);
    batchingJson["intersectionBackendPlatform"] =
      QString::fromStdString(batching.intersectionBackendPlatform);
    batchingJson["intersectionBackendAvailability"] =
      QString::fromStdString(batching.intersectionBackendAvailability);
    batchingJson["intersectionBackendFallbackReason"] =
      QString::fromStdString(batching.intersectionBackendFallbackReason);
    batchingJson["intersectionBackendExecutionPath"] =
      QString::fromStdString(batching.intersectionBackendExecutionPath);
    batchingJson["intersectionBackendClosestHitExecutionPath"] =
      QString::fromStdString(batching.intersectionBackendClosestHitExecutionPath);
    batchingJson["intersectionBackendAnyHitExecutionPath"] =
      QString::fromStdString(batching.intersectionBackendAnyHitExecutionPath);
    batchingJson["directLightContributionExecutionPath"] =
      QString::fromStdString(batching.directLightContributionExecutionPath);
    batchingJson["directLightContributionFallbackReason"] =
      QString::fromStdString(batching.directLightContributionFallbackReason);
    batchingJson["intersectionBackendClosestHitFrontierResidency"] =
      QString::fromStdString(batching.intersectionBackendClosestHitFrontierResidency);
    batchingJson["intersectionBackendAnyHitFrontierResidency"] =
      QString::fromStdString(batching.intersectionBackendAnyHitFrontierResidency);
    const render::TracingExecutionCapabilityRecords tracingCapabilities =
      batching.tracingExecutionCapabilities();
    batchingJson["tracingBackendRequest"] =
      QString::fromStdString(batching.intersectionBackendRequest);
    batchingJson["tracingBackend"] = QString::fromStdString(batching.intersectionBackend);
    batchingJson["tracingBackendMode"] = QStringLiteral("wavefront_intersection");
    batchingJson["tracingBackendCapabilities"] = tracingCapabilitiesToJson(tracingCapabilities);
    batchingJson["tracingBackendFallback"] = tracingBackendFallbackToJson(tracingCapabilities);
    batchingJson["intersectionBackendClosestHitFrontierPackedRayBytes"] =
      static_cast<double>(batching.intersectionBackendClosestHitFrontierPackedRayBytes);
    batchingJson["intersectionBackendAnyHitFrontierPackedRayBytes"] =
      static_cast<double>(batching.intersectionBackendAnyHitFrontierPackedRayBytes);
    batchingJson["intersectionBackendClosestHitFrontierHostQueryBytes"] =
      static_cast<double>(batching.intersectionBackendClosestHitFrontierHostQueryBytes);
    batchingJson["intersectionBackendAnyHitFrontierHostQueryBytes"] =
      static_cast<double>(batching.intersectionBackendAnyHitFrontierHostQueryBytes);
    batchingJson["intersectionBackendClosestHitFrontierStateHandleBytes"] =
      static_cast<double>(batching.intersectionBackendClosestHitFrontierStateHandleBytes);
    batchingJson["intersectionBackendAnyHitFrontierStateHandleBytes"] =
      static_cast<double>(batching.intersectionBackendAnyHitFrontierStateHandleBytes);
    batchingJson["intersectionBackendPlatformGpuDeviceAvailable"] =
      batching.intersectionBackendPlatformGpuDeviceAvailable;
    batchingJson["intersectionBackendPlatformGpuRenderPathAvailable"] =
      batching.intersectionBackendPlatformGpuRenderPathAvailable;
    batchingJson["intersectionBackendExpectedRays"] =
      static_cast<double>(batching.intersectionBackendExpectedRays);
    batchingJson["intersectionBackendExpectedClosestHitRays"] =
      static_cast<double>(batching.intersectionBackendExpectedClosestHitRays);
    batchingJson["intersectionBackendExpectedAnyHitRays"] =
      static_cast<double>(batching.intersectionBackendExpectedAnyHitRays);
    batchingJson["intersectionBackendAutoMinimumGpuRays"] =
      static_cast<double>(batching.intersectionBackendAutoMinimumGpuRays);
    batchingJson["intersectionBackendAutoEstimatedQueryTransferBytes"] =
      static_cast<double>(batching.intersectionBackendAutoEstimatedQueryTransferBytes);
    batchingJson["intersectionSceneCompiled"] = batching.intersectionSceneCompiled;
    batchingJson["intersectionSceneBvhNodes"] =
      static_cast<double>(batching.intersectionSceneBvhNodes);
    batchingJson["intersectionScenePrimitives"] =
      static_cast<double>(batching.intersectionScenePrimitives);
    batchingJson["intersectionSceneTriangles"] =
      static_cast<double>(batching.intersectionSceneTriangles);
    batchingJson["intersectionSceneSpheres"] =
      static_cast<double>(batching.intersectionSceneSpheres);
    batchingJson["intersectionScenePlanes"] = static_cast<double>(batching.intersectionScenePlanes);
    batchingJson["intersectionSceneRectangles"] =
      static_cast<double>(batching.intersectionSceneRectangles);
    batchingJson["intersectionSceneDisks"] = static_cast<double>(batching.intersectionSceneDisks);
    batchingJson["intersectionSceneOpenCylinders"] =
      static_cast<double>(batching.intersectionSceneOpenCylinders);
    batchingJson["intersectionSceneTori"] = static_cast<double>(batching.intersectionSceneTori);
    batchingJson["intersectionSceneTransforms"] =
      static_cast<double>(batching.intersectionSceneTransforms);
    batchingJson["intersectionSceneUnsupportedPrimitives"] =
      static_cast<double>(batching.intersectionSceneUnsupportedPrimitives);
    batchingJson["intersectionSceneUnsupportedReasons"] = intersectionSceneUnsupportedReasons;
    batchingJson["intersectionSceneUploadBytes"] =
      static_cast<double>(batching.intersectionSceneUploadBytes);
    batchingJson["intersectionSceneTriangleClosestHitEligible"] =
      batching.intersectionSceneTriangleClosestHitEligible;
    batchingJson["intersectionSceneBasicHitEligible"] = batching.intersectionSceneBasicHitEligible;
    batchingJson["intersectionScenePackedClosestHitEligible"] =
      batching.intersectionScenePackedClosestHitEligible;
    batchingJson["intersectionScenePackedAnyHitEligible"] =
      batching.intersectionScenePackedAnyHitEligible;
    batchingJson["tracingSceneCompiled"] = batching.tracingSceneCompiled;
    batchingJson["tracingSceneMaterials"] = static_cast<double>(batching.tracingSceneMaterials);
    batchingJson["tracingSceneTextures"] = static_cast<double>(batching.tracingSceneTextures);
    batchingJson["tracingSceneLights"] = static_cast<double>(batching.tracingSceneLights);
    batchingJson["tracingSceneEnvironment"] = static_cast<double>(batching.tracingSceneEnvironment);
    batchingJson["tracingSceneDebugIds"] = static_cast<double>(batching.tracingSceneDebugIds);
    batchingJson["tracingSceneUnsupportedMaterials"] =
      static_cast<double>(batching.tracingSceneUnsupportedMaterials);
    batchingJson["tracingSceneUnsupportedTextures"] =
      static_cast<double>(batching.tracingSceneUnsupportedTextures);
    batchingJson["tracingSceneUnsupportedLights"] =
      static_cast<double>(batching.tracingSceneUnsupportedLights);
    batchingJson["tracingSceneUnsupportedMaterialReasons"] = tracingSceneUnsupportedMaterialReasons;
    batchingJson["tracingSceneUnsupportedTextureReasons"] = tracingSceneUnsupportedTextureReasons;
    batchingJson["tracingSceneUnsupportedLightReasons"] = tracingSceneUnsupportedLightReasons;
    batchingJson["tracingSceneUploadBytes"] = static_cast<double>(batching.tracingSceneUploadBytes);
    batchingJson["intersectionEstimatedRayUploadBytes"] =
      static_cast<double>(batching.intersectionEstimatedRayUploadBytes);
    batchingJson["intersectionEstimatedClosestHitRayUploadBytes"] =
      static_cast<double>(batching.intersectionEstimatedClosestHitRayUploadBytes);
    batchingJson["intersectionEstimatedAnyHitRayUploadBytes"] =
      static_cast<double>(batching.intersectionEstimatedAnyHitRayUploadBytes);
    batchingJson["intersectionEstimatedClosestHitReadbackBytes"] =
      static_cast<double>(batching.intersectionEstimatedClosestHitReadbackBytes);
    batchingJson["intersectionEstimatedAnyHitReadbackBytes"] =
      static_cast<double>(batching.intersectionEstimatedAnyHitReadbackBytes);
    batchingJson["intersectionEstimatedQueryTransferBytes"] =
      static_cast<double>(batching.intersectionEstimatedQueryTransferBytes);
    batchingJson["intersectionEstimatedClosestHitQueryTransferBytes"] =
      static_cast<double>(batching.intersectionEstimatedClosestHitQueryTransferBytes);
    batchingJson["intersectionEstimatedAnyHitQueryTransferBytes"] =
      static_cast<double>(batching.intersectionEstimatedAnyHitQueryTransferBytes);
    batchingJson["intersectionEstimatedQueryRoundTrips"] =
      static_cast<double>(batching.intersectionEstimatedQueryRoundTrips);
    batchingJson["intersectionEstimatedClosestHitQueryRoundTrips"] =
      static_cast<double>(batching.intersectionEstimatedClosestHitQueryRoundTrips);
    batchingJson["intersectionEstimatedAnyHitQueryRoundTrips"] =
      static_cast<double>(batching.intersectionEstimatedAnyHitQueryRoundTrips);
    batchingJson["intersectionBackendUploadWorkerSeconds"] =
      batching.intersectionBackendUploadWorkerSeconds;
    batchingJson["intersectionBackendKernelWorkerSeconds"] =
      batching.intersectionBackendKernelWorkerSeconds;
    batchingJson["intersectionBackendReadbackWorkerSeconds"] =
      batching.intersectionBackendReadbackWorkerSeconds;
    batchingJson["intersectionRaysPerWorkerSecond"] = intersectionRaysPerWorkerSecond();
    batchingJson["intersectionBackendKernelRaysPerSecond"] =
      batching.intersectionBackendKernelRaysPerSecond();
    batchingJson["intersectionRaysSubmitted"] =
      static_cast<double>(batching.intersectionRaysSubmitted);
    batchingJson["closestHitRaysSubmitted"] = static_cast<double>(batching.closestHitRaysSubmitted);
    batchingJson["anyHitRaysSubmitted"] = static_cast<double>(batching.anyHitRaysSubmitted);
    batchingJson["closestHitQueries"] = static_cast<double>(batching.closestHitQueries);
    batchingJson["anyHitQueries"] = static_cast<double>(batching.anyHitQueries);
    batchingJson["intersectionBackendPrefersClosestHitBatch"] =
      batching.intersectionBackendPrefersClosestHitBatch;
    batchingJson["intersectionBackendPrefersAnyHitBatch"] =
      batching.intersectionBackendPrefersAnyHitBatch;
    batchingJson["intersectionBackendSupportsResidentFrontiers"] =
      batching.intersectionBackendSupportsResidentFrontiers;
    batchingJson["intersectionBackendSupportsGpuFrontierCompaction"] =
      batching.intersectionBackendSupportsGpuFrontierCompaction;
    batchingJson["intersectionBackendGpuFrontierCompactionUnavailableReason"] =
      QString::fromStdString(batching.intersectionBackendGpuFrontierCompactionUnavailableReason);
    batchingJson["intersectionBackendSupportsPreparedRayBatchCompaction"] =
      batching.intersectionBackendSupportsPreparedRayBatchCompaction;
    batchingJson["intersectionBackendSupportsResidentDirectLightBatches"] =
      batching.intersectionBackendSupportsResidentDirectLightBatches;
    batchingJson["intersectionBackendResidentDirectLightBatchesUnavailableReason"] =
      QString::fromStdString(
        batching.intersectionBackendResidentDirectLightBatchesUnavailableReason);
    batchingJson["residentPathLoopExecutionPath"] =
      QString::fromStdString(batching.residentPathLoopExecutionPath);
    batchingJson["residentPathLoopResidency"] =
      QString::fromStdString(batching.residentPathLoopResidency);
    batchingJson["residentPathLoopDepths"] = static_cast<double>(batching.residentPathLoopDepths);
    batchingJson["residentPathLoopInputPaths"] =
      static_cast<double>(batching.residentPathLoopInputPaths);
    batchingJson["residentPathLoopRetainedPaths"] =
      static_cast<double>(batching.residentPathLoopRetainedPaths);
    batchingJson["residentPathLoopRemovedPaths"] =
      static_cast<double>(batching.residentPathLoopRemovedPaths);
    batchingJson["residentPathLoopMovedPaths"] =
      static_cast<double>(batching.residentPathLoopMovedPaths);
    batchingJson["residentPathLoopRetainedIndexBytes"] =
      static_cast<double>(batching.residentPathLoopRetainedIndexBytes);
    batchingJson["residentPathLoopResidentPathStateBytes"] =
      static_cast<double>(batching.residentPathLoopResidentPathStateBytes);
    batchingJson["residentPathLoopInputResidentPathStateBytes"] =
      static_cast<double>(batching.residentPathLoopInputResidentPathStateBytes);
    batchingJson["residentPathLoopRetainedResidentPathStateBytes"] =
      static_cast<double>(batching.residentPathLoopRetainedResidentPathStateBytes);
    batchingJson["residentPathLoopRemovedResidentPathStateBytes"] =
      static_cast<double>(batching.residentPathLoopRemovedResidentPathStateBytes);
    batchingJson["residentPathLoopCompactionPasses"] =
      static_cast<double>(batching.residentPathLoopCompactionPasses);
    batchingJson["residentPathLoopRoundTrips"] =
      static_cast<double>(batching.residentPathLoopRoundTrips);
    batchingJson["residentPathLoopSavedHostReadbacks"] =
      static_cast<double>(batching.residentPathLoopSavedHostReadbacks);
    batchingJson["residentPathLoopSavedHostReadbackBytes"] =
      static_cast<double>(batching.residentPathLoopSavedHostReadbackBytes);
    batchingJson["batches"] = static_cast<double>(batching.batches);
    batchingJson["samplesSubmitted"] = static_cast<double>(batching.samplesSubmitted);
    batchingJson["maxBatchSize"] = static_cast<double>(batching.maxBatchSize);
    batchingJson["averageBatchSize"] = batching.averageBatchSize;
    batchingJson["activeSampleDepthsProcessed"] =
      static_cast<double>(batching.activeSampleDepthsProcessed);
    batchingJson["activeHostPathStateBytesProcessed"] =
      static_cast<double>(batching.activeHostPathStateBytesProcessed);
    batchingJson["activeHitHostBytesProcessed"] =
      static_cast<double>(batching.activeHitHostBytesProcessed);
    batchingJson["frontierHostCompactionPasses"] =
      static_cast<double>(batching.frontierCompactionPasses);
    batchingJson["frontierCompactionPasses"] =
      static_cast<double>(batching.frontierCompactionPasses);
    batchingJson["frontierHostCompactionInputSamples"] =
      static_cast<double>(batching.frontierCompactionInputSamples);
    batchingJson["frontierCompactionInputSamples"] =
      static_cast<double>(batching.frontierCompactionInputSamples);
    batchingJson["frontierHostCompactionRetainedSamples"] =
      static_cast<double>(batching.frontierCompactionRetainedSamples);
    batchingJson["frontierCompactionRetainedSamples"] =
      static_cast<double>(batching.frontierCompactionRetainedSamples);
    batchingJson["frontierHostCompactionRemovedSamples"] =
      static_cast<double>(batching.frontierCompactionRemovedSamples);
    batchingJson["frontierCompactionRemovedSamples"] =
      static_cast<double>(batching.frontierCompactionRemovedSamples);
    batchingJson["frontierHostCompactionMovedSamples"] =
      static_cast<double>(batching.frontierCompactionMovedSamples);
    batchingJson["frontierCompactionMovedSamples"] =
      static_cast<double>(batching.frontierCompactionMovedSamples);
    batchingJson["frontierCompactionRetainedIndexBytes"] =
      static_cast<double>(batching.frontierCompactionRetainedIndexBytes);
    batchingJson["frontierCompactionInputHostPathStateBytes"] =
      static_cast<double>(batching.frontierCompactionInputHostPathStateBytes);
    batchingJson["frontierCompactionRetainedHostPathStateBytes"] =
      static_cast<double>(batching.frontierCompactionRetainedHostPathStateBytes);
    batchingJson["frontierCompactionRemovedHostPathStateBytes"] =
      static_cast<double>(batching.frontierCompactionRemovedHostPathStateBytes);
    batchingJson["frontierHostCompactionRemovedSampleFraction"] =
      batching.frontierCompactionRemovedSampleFraction();
    batchingJson["frontierCompactionRemovedSampleFraction"] =
      batching.frontierCompactionRemovedSampleFraction();
    batchingJson["frontierCompactionMovedRetainedSampleFraction"] =
      batching.frontierCompactionMovedRetainedSampleFraction();
    batchingJson["frontierCompactionExecutionPath"] =
      QString::fromStdString(batching.frontierCompactionExecutionPath);
    batchingJson["frontierCompactionPathStateResidency"] =
      QString::fromStdString(batching.frontierCompactionPathStateResidency);
    batchingJson["compatibilityShadeSamples"] =
      static_cast<double>(batching.compatibilityShadeSamples);
    batchingJson["unsupportedPathMaterialSamples"] =
      static_cast<double>(batching.unsupportedPathMaterialSamples);
    batchingJson["emitterHitSamples"] = static_cast<double>(batching.emitterHitSamples);
    batchingJson["primaryEmitterHitSamples"] =
      static_cast<double>(batching.primaryEmitterHitSamples);
    batchingJson["deltaEmitterHitSamples"] = static_cast<double>(batching.deltaEmitterHitSamples);
    batchingJson["bsdfEmitterHitSamples"] = static_cast<double>(batching.bsdfEmitterHitSamples);
    batchingJson["misWeightedEmitterHitSamples"] =
      static_cast<double>(batching.misWeightedEmitterHitSamples);
    batchingJson["directLightSamples"] = static_cast<double>(batching.directLightSamples);
    batchingJson["directLightContributingSamples"] =
      static_cast<double>(batching.directLightContributingSamples);
    batchingJson["directLightOccludedSamples"] =
      static_cast<double>(batching.directLightOccludedSamples);
    batchingJson["emittedRadianceLuminanceSum"] = batching.emittedRadianceLuminanceSum;
    batchingJson["directLightRadianceLuminanceSum"] = batching.directLightRadianceLuminanceSum;
    batchingJson["primaryDirectLightRadianceLuminanceSum"] =
      batching.primaryDirectLightRadianceLuminanceSum;
    batchingJson["secondaryDirectLightRadianceLuminanceSum"] =
      batching.secondaryDirectLightRadianceLuminanceSum;
    batchingJson["ambientRadianceLuminanceSum"] = batching.ambientRadianceLuminanceSum;
    batchingJson["missRadianceLuminanceSum"] = batching.missRadianceLuminanceSum;
    batchingJson["compatibilityShadeRadianceLuminanceSum"] =
      batching.compatibilityShadeRadianceLuminanceSum;
    batchingJson["activeSamplesPerDepth"] = activeSamplesPerDepth;
    batchingJson["retainedActiveSamplesPerDepth"] = retainedActiveSamplesPerDepth;
    batchingJson["activeHostPathStateBytesPerDepth"] = activeHostPathStateBytesPerDepth;
    batchingJson["activeHitHostBytesPerDepth"] = activeHitHostBytesPerDepth;
    batchingJson["retainedHostPathStateBytesPerDepth"] = retainedHostPathStateBytesPerDepth;
    batchingJson["spawnedContinuationSamples"] =
      static_cast<double>(batching.spawnedContinuationSamples);
    batchingJson["spawnedContinuationHostPathStateBytes"] =
      static_cast<double>(batching.spawnedContinuationHostPathStateBytes);
    batchingJson["spawnedContinuationSamplesPerDepth"] = spawnedContinuationSamplesPerDepth;
    batchingJson["spawnedContinuationHostPathStateBytesPerDepth"] =
      spawnedContinuationHostPathStateBytesPerDepth;
    batchingJson["frontierCompactionCandidateDepths"] =
      static_cast<double>(batching.compactionCandidateDepthCount());
    batchingJson["frontierCompactionCandidateSamples"] =
      static_cast<double>(batching.compactionCandidateSampleCount());
    batchingJson["frontierCompactionCandidatePackedRayBytes"] =
      static_cast<double>(batching.compactionCandidatePackedRayBytes());
    batchingJson["frontierCompactionCandidateStateHandleBytes"] =
      static_cast<double>(batching.compactionCandidateStateHandleBytes());
    batchingJson["frontierCompactionCandidateHostPathStateBytes"] =
      static_cast<double>(batching.compactionCandidateHostPathStateBytes());
    batchingJson["frontierCompactionCandidateSampleFraction"] =
      batching.compactionCandidateSampleFraction();
    batchingJson["frontierLargestCompactionCandidateDepth"] =
      static_cast<double>(batching.largestCompactionCandidateDepth());
    batchingJson["frontierLargestCompactionCandidateSamples"] =
      static_cast<double>(batching.largestCompactionCandidateSampleCount());
    batchingJson["frontierLargestCompactionCandidatePackedRayBytes"] =
      static_cast<double>(batching.largestCompactionCandidatePackedRayBytes());
    batchingJson["frontierLargestCompactionCandidateStateHandleBytes"] =
      static_cast<double>(batching.largestCompactionCandidateStateHandleBytes());
    batchingJson["frontierLargestCompactionCandidateHostPathStateBytes"] =
      static_cast<double>(batching.largestCompactionCandidateHostPathStateBytes());
    batchingJson["frontierLargestCompactionCandidateSampleFraction"] =
      batching.largestCompactionCandidateSampleFraction();
    batchingJson["frontierRayHitsPerDepth"] = frontierRayHitsPerDepth;
    batchingJson["frontierRayMissesPerDepth"] = frontierRayMissesPerDepth;
    batchingJson["frontierPacketChunksPerDepth"] = frontierPacketChunksPerDepth;
    batchingJson["frontierPacketRaysPerDepth"] = frontierPacketRaysPerDepth;
    batchingJson["frontierClosestHitBatchChunksPerDepth"] = frontierClosestHitBatchChunksPerDepth;
    batchingJson["frontierClosestHitBatchRaysPerDepth"] = frontierClosestHitBatchRaysPerDepth;
    batchingJson["directLightAnyHitBatchChunksPerDepth"] = directLightAnyHitBatchChunksPerDepth;
    batchingJson["directLightAnyHitBatchRaysPerDepth"] = directLightAnyHitBatchRaysPerDepth;
    batchingJson["directLightSelectionHostBytes"] =
      static_cast<double>(batching.directLightSelectionHostBytes);
    batchingJson["directLightSelectionHostBytesPerDepth"] = directLightSelectionHostBytesPerDepth;
    batchingJson["directLightOcclusionHostBytes"] =
      static_cast<double>(batching.directLightOcclusionHostBytes);
    batchingJson["directLightOcclusionHostBytesPerDepth"] = directLightOcclusionHostBytesPerDepth;
    batchingJson["directLightContributionHostBytes"] =
      static_cast<double>(batching.directLightContributionHostBytes);
    batchingJson["directLightContributionHostBytesPerDepth"] =
      directLightContributionHostBytesPerDepth;
    batchingJson["directLightAnyHitFrontierPackedRayBytes"] =
      static_cast<double>(batching.directLightAnyHitFrontierPackedRayBytes);
    batchingJson["directLightAnyHitFrontierPackedRayBytesPerDepth"] =
      directLightAnyHitFrontierPackedRayBytesPerDepth;
    batchingJson["directLightAnyHitFrontierHostQueryBytes"] =
      static_cast<double>(batching.directLightAnyHitFrontierHostQueryBytes);
    batchingJson["directLightAnyHitFrontierHostQueryBytesPerDepth"] =
      directLightAnyHitFrontierHostQueryBytesPerDepth;
    batchingJson["directLightAnyHitFrontierStateHandleBytes"] =
      static_cast<double>(batching.directLightAnyHitFrontierStateHandleBytes);
    batchingJson["directLightAnyHitFrontierStateHandleBytesPerDepth"] =
      directLightAnyHitFrontierStateHandleBytesPerDepth;
    batchingJson["frontierQueryRoundTrips"] =
      static_cast<double>(batching.frontierQueryRoundTrips());
    batchingJson["frontierResidentQueryRoundTripsEstimate"] =
      static_cast<double>(batching.residentFrontierQueryRoundTripsEstimate());
    batchingJson["frontierResidentQueryRoundTripSavingsEstimate"] =
      static_cast<double>(batching.residentFrontierQueryRoundTripSavingsEstimate());
    batchingJson["directLightAnyHitQueryRoundTrips"] =
      static_cast<double>(batching.directLightAnyHitQueryRoundTrips());
    batchingJson["residentDirectLightBatchRoundTripsEstimate"] =
      static_cast<double>(batching.residentDirectLightBatchRoundTripsEstimate());
    batchingJson["residentDirectLightBatchRoundTripSavingsEstimate"] =
      static_cast<double>(batching.residentDirectLightBatchRoundTripSavingsEstimate());
    batchingJson["frontierMixedQueryDepths"] = static_cast<double>(batching.mixedQueryDepthCount());
    batchingJson["frontierMixedQueryRoundTrips"] =
      static_cast<double>(batching.mixedQueryDepthRoundTrips());
    batchingJson["frontierMixedQueryRays"] = static_cast<double>(batching.mixedQueryDepthRays());
    batchingJson["frontierMixedQueryClosestHitRays"] =
      static_cast<double>(batching.mixedQueryDepthClosestHitRays());
    batchingJson["frontierMixedQueryAnyHitRays"] =
      static_cast<double>(batching.mixedQueryDepthAnyHitRays());
    batchingJson["frontierMixedQueryReadbackBytes"] =
      static_cast<double>(batching.mixedQueryDepthReadbackBytes());
    batchingJson["frontierMixedQueryClosestHitReadbackBytes"] =
      static_cast<double>(batching.mixedQueryDepthClosestHitReadbackBytes());
    batchingJson["frontierMixedQueryAnyHitReadbackBytes"] =
      static_cast<double>(batching.mixedQueryDepthAnyHitReadbackBytes());
    batchingJson["frontierRay4PacketChunksPerDepth"] = frontierRay4PacketChunksPerDepth;
    batchingJson["frontierRay8PacketChunksPerDepth"] = frontierRay8PacketChunksPerDepth;
    batchingJson["frontierScalarRaysPerDepth"] = frontierScalarRaysPerDepth;
    batchingJson["frontierPacketScalarFallbackRaysPerDepth"] =
      frontierPacketScalarFallbackRaysPerDepth;
    batchingJson["frontierPacketScalarFallbackRaysByReason"] =
      frontierPacketScalarFallbackRaysByReason;
    batchingJson["frontierPacketRefinedRaysPerDepth"] = frontierPacketRefinedRaysPerDepth;
    batchingJson["frontierPacketRefinedRaysByMaterial"] = frontierPacketRefinedRaysByMaterial;
    batchingJson["sampleVariancePixelArea"] = static_cast<double>(batching.sampleVariancePixelArea);
    batchingJson["sampleRadianceStddevRms"] =
      batching.sampleVariancePixelArea == 0
        ? 0.0
        : std::sqrt(batching.sampleRadianceVarianceSum /
                    static_cast<double>(batching.sampleVariancePixelArea));
    batchingJson["maxSampleRadianceStddev"] = batching.maxSampleRadianceStddev;
    batchingJson["radianceDeltaL2PerDepth"] = radianceDeltaL2PerDepth;
    batchingJson["radianceDeltaRmsPerDepth"] = radianceDeltaRmsPerDepth;
    batchingJson["maxRadianceDeltaPerDepth"] = maxRadianceDeltaPerDepth;

    QJsonObject timingsJson;
    timingsJson["sampleGenerationWorkerSeconds"] = timings.sampleGenerationWorkerSeconds;
    timingsJson["sampleStreamWorkerSeconds"] = timings.sampleStreamWorkerSeconds;
    timingsJson["primaryRayWorkerSeconds"] = timings.primaryRayWorkerSeconds;
    timingsJson["sampleEnqueueWorkerSeconds"] = timings.sampleEnqueueWorkerSeconds;
    timingsJson["sampleGenerationOverheadWorkerSeconds"] =
      std::max(0.0, timings.sampleGenerationWorkerSeconds - timings.sampleStreamWorkerSeconds -
                      timings.primaryRayWorkerSeconds - timings.sampleEnqueueWorkerSeconds);
    timingsJson["integratorBatchWorkerSeconds"] = timings.integratorBatchWorkerSeconds;
    timingsJson["integratorIntersectionWorkerSeconds"] =
      timings.integratorIntersectionWorkerSeconds;
    timingsJson["integratorShadingWorkerSeconds"] = timings.integratorShadingWorkerSeconds;
    timingsJson["integratorOverheadWorkerSeconds"] = timings.integratorOverheadWorkerSeconds;
    timingsJson["integratorPathSetupWorkerSeconds"] = timings.integratorPathSetupWorkerSeconds;
    timingsJson["integratorFrontierPartitionWorkerSeconds"] =
      timings.integratorFrontierPartitionWorkerSeconds;
    timingsJson["integratorFrontierBookkeepingWorkerSeconds"] =
      timings.integratorFrontierBookkeepingWorkerSeconds;
    timingsJson["integratorProgressSnapshotWorkerSeconds"] =
      timings.integratorProgressSnapshotWorkerSeconds;
    timingsJson["integratorConvergenceTestWorkerSeconds"] =
      timings.integratorConvergenceTestWorkerSeconds;
    timingsJson["integratorResidualWorkerSeconds"] = timings.integratorResidualWorkerSeconds;
    timingsJson["totalRenderSeconds"] = timings.totalRenderSeconds;

    QJsonObject denoiseJson;
    denoiseJson["enabled"] = denoise.enabled;
    denoiseJson["seconds"] = denoise.seconds;
    denoiseJson["featureSeconds"] = denoise.featureSeconds;
    if (denoise.enabled) {
      denoiseJson["denoiser"] = QString::fromStdString(denoise.denoiser);
    }
    QJsonObject denoiseParametersJson;
    for (const auto& parameter : denoise.numericParameters) {
      denoiseParametersJson[QString::fromStdString(parameter.name)] = parameter.value;
    }
    if (!denoiseParametersJson.isEmpty()) {
      denoiseJson["parameters"] = denoiseParametersJson;
    }
    if (denoise.enabled) {
      QJsonObject featureJson;
      featureJson["albedo"] = denoise.albedoFeature;
      featureJson["normal"] = denoise.normalFeature;
      featureJson["depth"] = denoise.depthFeature;
      denoiseJson["features"] = featureJson;

      QJsonObject featurePrepassJson;
      featurePrepassJson["tileCount"] = static_cast<double>(denoise.featureTileCount);
      featurePrepassJson["completedTileCount"] =
        static_cast<double>(denoise.completedFeatureTileCount);
      featurePrepassJson["pixels"] = static_cast<double>(denoise.featurePixels);
      featurePrepassJson["seconds"] = denoise.featureSeconds;
      denoiseJson["featurePrepass"] = featurePrepassJson;
    }

    QJsonObject convergenceJson;
    QJsonArray stoppedTileDepthHistogram;
    for (const std::uint64_t count : convergence.stoppedTileDepthHistogram) {
      stoppedTileDepthHistogram.push_back(static_cast<double>(count));
    }
    convergenceJson["enabled"] = convergence.enabled;
    convergenceJson["activeSampleFractionThreshold"] = convergence.activeSampleFractionThreshold;
    convergenceJson["radianceDeltaRmsThreshold"] = convergence.radianceDeltaRmsThreshold;
    convergenceJson["stoppedTileCount"] = static_cast<double>(convergence.stoppedTileCount);
    convergenceJson["earliestStoppedAfterDepth"] =
      static_cast<double>(convergence.earliestStoppedAfterDepth);
    convergenceJson["latestStoppedAfterDepth"] =
      static_cast<double>(convergence.latestStoppedAfterDepth);
    convergenceJson["feedbackDepthCount"] = static_cast<double>(convergence.feedbackDepthCount);
    convergenceJson["stoppedTileDepthHistogram"] = stoppedTileDepthHistogram;
    convergenceJson["decision"] = QString::fromStdString(convergence.decision);

    QJsonObject adaptiveSamplingJson;
    adaptiveSamplingJson["enabled"] = adaptiveSampling.enabled;
    adaptiveSamplingJson["minimumSamples"] = adaptiveSampling.minimumSamples;
    adaptiveSamplingJson["stddevThreshold"] = adaptiveSampling.stddevThreshold;
    adaptiveSamplingJson["maximumPrimarySamples"] =
      static_cast<double>(adaptiveSampling.maximumPrimarySamples);
    adaptiveSamplingJson["skippedPrimarySamples"] =
      static_cast<double>(adaptiveSampling.skippedPrimarySamples);
    adaptiveSamplingJson["skippedPrimarySampleFraction"] =
      adaptiveSampling.skippedPrimarySampleFraction;

    QJsonObject object;
    object["input"] = inputJson;
    object["tiling"] = tilingJson;
    object["scheduling"] = schedulingJson;
    object["accumulation"] = accumulationJson;
    object["batching"] = batchingJson;
    object["convergence"] = convergenceJson;
    object["adaptiveSampling"] = adaptiveSamplingJson;
    object["denoise"] = denoiseJson;
    object["timings"] = timingsJson;
    return object;
  }

  void
  WavefrontRenderMetrics::ConvergenceSummary::recordStoppedTileAfterDepth(std::uint64_t depth) {
    ++stoppedTileCount;
    if (earliestStoppedAfterDepth == 0 || depth < earliestStoppedAfterDepth) {
      earliestStoppedAfterDepth = depth;
    }
    latestStoppedAfterDepth = std::max(latestStoppedAfterDepth, depth);
    if (depth > 0) {
      if (stoppedTileDepthHistogram.size() < depth) {
        stoppedTileDepthHistogram.resize(depth);
      }
      ++stoppedTileDepthHistogram[depth - 1];
    }
  }

  struct WavefrontRaytracer::Private {
    Private()
        : threadPool(std::make_unique<QThreadPool>()),
          queueSize(QThread::idealThreadCount()),
          integrator(std::make_unique<render::WhittedIntegrator>()),
          showProgressIndicators(false),
          convergenceActiveSampleFractionThreshold(
            RAYTRACER_WAVEFRONT_ACTIVE_SAMPLE_FRACTION_THRESHOLD),
          convergenceRadianceDeltaRmsThreshold(RAYTRACER_WAVEFRONT_RADIANCE_DELTA_RMS_THRESHOLD) {
    }

    std::unique_ptr<QThreadPool> threadPool;
    std::list<std::shared_ptr<engine::TileRenderTask>> tasks;
    std::list<std::shared_ptr<engine::TileRenderTask>> denoiserFeatureTasks;
    int queueSize;
    std::unique_ptr<render::Integrator> integrator;
    std::unique_ptr<render::Denoiser> denoiser;
    bool showProgressIndicators;
    bool metricsEnabled{false};
    bool convergenceEnabled{false};
    bool adaptiveSamplingEnabled{false};
    bool sampleRadianceStddevCaptureEnabled{false};
    render::WavefrontIntersectionBackendChoice intersectionBackend;
    std::shared_ptr<const render::WavefrontIntersectionBackend> preparedIntersectionBackend;
    double convergenceActiveSampleFractionThreshold;
    double convergenceRadianceDeltaRmsThreshold;
    int adaptiveMinimumSamples{1};
    double adaptiveStddevThreshold{0.0};
    std::optional<int> maximumRecursionDepth;
    std::optional<std::uint64_t> samplingSeed;
    std::shared_ptr<Buffer<double>> lastSampleRadianceStddev;
    std::shared_ptr<Buffer<Colord>> lastSampleRadianceStddevColor;
    detail::WavefrontMetricsRecorder metrics;

    bool denoiserRequestsFeatures() const {
      return denoiser && denoiser->requestedFeatures().any();
    }

    bool shouldRecordRenderMetrics(const render::Camera& camera) const {
      return metricsEnabled || camera.isCancelled() || denoiserRequestsFeatures();
    }

    detail::WavefrontTileRenderConfig tileRenderConfig(bool recordMetrics) const {
      return detail::WavefrontTileRenderConfig{*integrator,
                                               denoiser.get(),
                                               showProgressIndicators,
                                               convergenceEnabled,
                                               convergenceActiveSampleFractionThreshold,
                                               convergenceRadianceDeltaRmsThreshold,
                                               adaptiveSamplingEnabled,
                                               adaptiveMinimumSamples,
                                               adaptiveStddevThreshold,
                                               recordMetrics,
                                               samplingSeed,
                                               preparedIntersectionBackend
                                                 ? preparedIntersectionBackend.get()
                                                 : &intersectionBackend.resolvedBackend()};
    }

    detail::WavefrontTileRenderer tileRenderer(bool recordMetrics) {
      return detail::WavefrontTileRenderer(tileRenderConfig(recordMetrics), metrics);
    }

    Buffer<double>* prepareSampleRadianceStddevBuffer(int width, int height) {
      if (!sampleRadianceStddevCaptureEnabled) {
        lastSampleRadianceStddev.reset();
        lastSampleRadianceStddevColor.reset();
        return nullptr;
      }

      lastSampleRadianceStddev = std::make_shared<Buffer<double>>(width, height);
      lastSampleRadianceStddev->clear(0.0);
      return lastSampleRadianceStddev.get();
    }

    Buffer<Colord>* prepareSampleRadianceStddevColorBuffer(int width, int height) {
      if (!sampleRadianceStddevCaptureEnabled) {
        lastSampleRadianceStddevColor.reset();
        return nullptr;
      }

      lastSampleRadianceStddevColor = std::make_shared<Buffer<Colord>>(width, height);
      lastSampleRadianceStddevColor->clear(Colord::black());
      return lastSampleRadianceStddevColor.get();
    }

    void configureIntegratorCancellation(const WavefrontRaytracer& owner) {
      integrator->setCancellationCallback(
        [&owner] { return owner.camera() && owner.camera()->isCancelled(); });
    }

    std::uint64_t saturatedProduct(std::uint64_t lhs, std::uint64_t rhs) const {
      constexpr std::uint64_t maxValue = std::numeric_limits<std::uint64_t>::max();
      if (lhs != 0 && rhs > maxValue / lhs) {
        return maxValue;
      }
      return lhs * rhs;
    }

    std::uint64_t expectedPrimaryRayCount(const render::Camera& camera, int width,
                                          int height) const {
      const std::uint64_t pixelCount =
        saturatedProduct(static_cast<std::uint64_t>(std::max(0, width)),
                         static_cast<std::uint64_t>(std::max(0, height)));
      return saturatedProduct(pixelCount,
                              static_cast<std::uint64_t>(std::max(1, camera.samplesPerPixel())));
    }

    std::uint64_t expectedClosestHitIntersectionRayCount(const render::Camera& camera, int width,
                                                         int height) const {
      return saturatedProduct(expectedPrimaryRayCount(camera, width, height),
                              integrator->estimatedClosestHitRaysPerPrimarySample());
    }

    std::uint64_t expectedAnyHitIntersectionRayCount(const render::Camera& camera, int width,
                                                     int height) const {
      return saturatedProduct(expectedPrimaryRayCount(camera, width, height),
                              integrator->estimatedAnyHitRaysPerPrimarySample());
    }

    render::WavefrontIntersectionBackendSelectionContext
    intersectionBackendSelectionContext(std::uint64_t expectedClosestHitIntersectionRays,
                                        std::uint64_t expectedAnyHitIntersectionRays) const {
      return render::WavefrontIntersectionBackendSelectionContext::fromExpectedQueryFamilies(
        expectedClosestHitIntersectionRays, expectedAnyHitIntersectionRays);
    }

    std::uint64_t autoMinimumGpuIntersectionRayCount(
      const render::WavefrontIntersectionBackendSelectionContext& context) const {
      if (intersectionBackend.kind() != render::WavefrontIntersectionBackendChoice::Kind::Auto) {
        return 0;
      }

      const render::WavefrontIntersectionSceneDiagnostics diagnostics =
        preparedIntersectionBackend ? preparedIntersectionBackend->compiledSceneDiagnostics()
                                    : render::WavefrontIntersectionSceneDiagnostics();
      return render::WavefrontIntersectionBackendAutoSelectionPolicy().minimumExpectedRayCount(
        diagnostics, context);
    }

    std::uint64_t autoEstimatedQueryTransferBytes(
      const render::WavefrontIntersectionBackendSelectionContext& context) const {
      if (intersectionBackend.kind() != render::WavefrontIntersectionBackendChoice::Kind::Auto) {
        return 0;
      }

      const render::WavefrontIntersectionSceneDiagnostics diagnostics =
        preparedIntersectionBackend ? preparedIntersectionBackend->compiledSceneDiagnostics()
                                    : render::WavefrontIntersectionSceneDiagnostics();
      return render::WavefrontIntersectionBackendAutoSelectionPolicy().estimatedQueryTransferBytes(
        diagnostics, context);
    }

    void prepareIntersectionBackend(
      const render::Scene& scene,
      const render::WavefrontIntersectionBackendSelectionContext& context) {
      preparedIntersectionBackend = intersectionBackend.createBackendForScene(scene, context);
    }

    void clearPreparedIntersectionBackend() {
      preparedIntersectionBackend.reset();
    }
  };

  WavefrontRaytracer::WavefrontRaytracer(std::shared_ptr<render::Scene> scene)
      : RenderEngine(std::move(scene)),
        p(std::make_unique<Private>()) {
    p->configureIntegratorCancellation(*this);
  }

  WavefrontRaytracer::WavefrontRaytracer(std::shared_ptr<render::Camera> camera,
                                         std::shared_ptr<render::Scene> scene)
      : RenderEngine(std::move(camera), std::move(scene)),
        p(std::make_unique<Private>()) {
    p->configureIntegratorCancellation(*this);
  }

  WavefrontRaytracer::~WavefrontRaytracer() = default;

  std::shared_ptr<render::RenderEngine> WavefrontRaytracer::cloneForRender() const {
    auto result =
      std::make_shared<WavefrontRaytracer>(m_camera ? m_camera->clone() : nullptr, m_scene);
    copyRenderEngineStateTo(*result);
    result->setIntegrator(p->integrator->clone());
    if (p->maximumRecursionDepth) {
      result->setMaximumRecursionDepth(*p->maximumRecursionDepth);
    }
    if (p->denoiser) {
      result->setDenoiser(p->denoiser->clone());
    }
    result->setMaximumThreads(p->threadPool->maxThreadCount());
    result->setQueueSize(p->queueSize);
    result->setShowProgressIndicators(p->showProgressIndicators);
    result->setMetricsEnabled(p->metricsEnabled);
    result->setConvergenceEnabled(p->convergenceEnabled);
    result->setConvergenceActiveSampleFractionThreshold(
      p->convergenceActiveSampleFractionThreshold);
    result->setConvergenceRadianceDeltaRmsThreshold(p->convergenceRadianceDeltaRmsThreshold);
    result->setAdaptiveSamplingEnabled(p->adaptiveSamplingEnabled);
    result->setAdaptiveMinimumSamples(p->adaptiveMinimumSamples);
    result->setAdaptiveStddevThreshold(p->adaptiveStddevThreshold);
    result->setSampleRadianceStddevCaptureEnabled(p->sampleRadianceStddevCaptureEnabled);
    result->setIntersectionBackend(p->intersectionBackend);
    if (p->samplingSeed) {
      result->setSamplingSeed(*p->samplingSeed);
    }
    return result;
  }

  void WavefrontRaytracer::render(Buffer<Colord>& buffer) {
    if (!m_scene || !m_camera) {
      p->lastSampleRadianceStddev.reset();
      p->lastSampleRadianceStddevColor.reset();
      buffer.clear();
      return;
    }

    p->tasks.clear();
    p->denoiserFeatureTasks.clear();
    const std::uint64_t expectedClosestHitIntersectionRays =
      p->expectedClosestHitIntersectionRayCount(*m_camera, buffer.width(), buffer.height());
    const std::uint64_t expectedAnyHitIntersectionRays =
      p->expectedAnyHitIntersectionRayCount(*m_camera, buffer.width(), buffer.height());
    const render::WavefrontIntersectionBackendSelectionContext intersectionBackendContext =
      p->intersectionBackendSelectionContext(expectedClosestHitIntersectionRays,
                                             expectedAnyHitIntersectionRays);
    const std::uint64_t expectedIntersectionRays = intersectionBackendContext.expectedRayCount;
    p->prepareIntersectionBackend(*m_scene, intersectionBackendContext);
    const std::uint64_t autoMinimumGpuIntersectionRays =
      p->autoMinimumGpuIntersectionRayCount(intersectionBackendContext);
    const std::uint64_t autoEstimatedQueryTransferBytes =
      p->autoEstimatedQueryTransferBytes(intersectionBackendContext);

#ifdef RAYTRACER_ENABLE_STATS
    ::render::stats::Counters::instance().reset();
#endif

    m_camera->viewPlane()->setup(m_camera->matrix(), buffer.rect());
    m_camera->setShowProgressIndicators(p->showProgressIndicators);

    auto rayCaster = std::make_shared<RecursiveRayCasterAdapter>(*m_scene, *p->integrator);
    auto camera = m_camera;
    Buffer<Colord>* bufferPtr = &buffer;

    const render::TilePlan tilePlan =
      render::TilePlan::forBuffer(buffer.width(), buffer.height(), p->queueSize);
    const auto renderStart = detail::WavefrontMetricsRecorder::Clock::now();
    const bool recordMetrics = p->shouldRecordRenderMetrics(*m_camera);
    if (recordMetrics) {
      p->metrics.reset(
        *m_camera, buffer.width(), buffer.height(), tilePlan, p->queueSize, *p->integrator,
        p->denoiser.get(), expectedIntersectionRays, expectedClosestHitIntersectionRays,
        expectedAnyHitIntersectionRays, autoMinimumGpuIntersectionRays,
        autoEstimatedQueryTransferBytes, p->samplingSeed, "sampler", p->convergenceEnabled,
        p->convergenceActiveSampleFractionThreshold, p->convergenceRadianceDeltaRmsThreshold,
        p->adaptiveSamplingEnabled, p->adaptiveMinimumSamples, p->adaptiveStddevThreshold);
    } else {
      p->metrics.clear();
    }
    auto tileRenderer = p->tileRenderer(recordMetrics);
    const auto denoiserFeatures = tileRenderer.buildDenoiserFeatures(
      *m_camera, *m_scene, buffer.rect(), tilePlan, *p->threadPool, p->denoiserFeatureTasks);
    const auto* denoiserFeaturePtr = denoiserFeatures.get();
    Buffer<double>* sampleRadianceStddevBuffer =
      p->prepareSampleRadianceStddevBuffer(buffer.width(), buffer.height());
    Buffer<Colord>* sampleRadianceStddevColorBuffer =
      p->prepareSampleRadianceStddevColorBuffer(buffer.width(), buffer.height());
    const auto samplingSeed = p->samplingSeed;
    const bool publishProgressSnapshots = progressiveDisplayEnabled();
    engine::dispatchTileTasks(
      tilePlan, *p->threadPool, p->tasks,
      [this, rayCaster, camera, bufferPtr, sampleRadianceStddevBuffer,
       sampleRadianceStddevColorBuffer, samplingSeed, tileRenderer, denoiserFeaturePtr,
       publishProgressSnapshots](const Recti& rect, std::size_t tileIndex) {
        const std::optional<std::uint64_t> tileSeed =
          samplingSeed
            ? std::optional<std::uint64_t>(render::SamplingSeed::tileSeed(*samplingSeed, tileIndex))
            : std::nullopt;
        tileRenderer.renderHdrTile(*camera, *rayCaster, *m_scene, *bufferPtr, rect, tileSeed,
                                   publishProgressSnapshots, sampleRadianceStddevBuffer,
                                   sampleRadianceStddevColorBuffer, denoiserFeaturePtr);
      });
    tileRenderer.denoise(buffer, denoiserFeatures.get());
    if (recordMetrics) {
      p->metrics.finish(renderStart);
      if (!p->metricsEnabled && !m_camera->isCancelled() && !denoiserFeatures) {
        p->metrics.clear();
      }
    }
    p->clearPreparedIntersectionBackend();

#ifdef RAYTRACER_ENABLE_STATS
    ::render::stats::Counters::instance().dumpJson(std::cerr);
#endif
  }

  void WavefrontRaytracer::render(Buffer<unsigned int>& buffer) {
    if (!m_scene || !m_camera) {
      p->lastSampleRadianceStddev.reset();
      p->lastSampleRadianceStddevColor.reset();
      buffer.clear();
      return;
    }

    if (p->denoiser) {
      Buffer<Colord> hdrBuffer(buffer.width(), buffer.height());
      render(hdrBuffer, buffer, tonemap());
      return;
    }

    p->tasks.clear();
    p->denoiserFeatureTasks.clear();
    const std::uint64_t expectedClosestHitIntersectionRays =
      p->expectedClosestHitIntersectionRayCount(*m_camera, buffer.width(), buffer.height());
    const std::uint64_t expectedAnyHitIntersectionRays =
      p->expectedAnyHitIntersectionRayCount(*m_camera, buffer.width(), buffer.height());
    const render::WavefrontIntersectionBackendSelectionContext intersectionBackendContext =
      p->intersectionBackendSelectionContext(expectedClosestHitIntersectionRays,
                                             expectedAnyHitIntersectionRays);
    const std::uint64_t expectedIntersectionRays = intersectionBackendContext.expectedRayCount;
    p->prepareIntersectionBackend(*m_scene, intersectionBackendContext);
    const std::uint64_t autoMinimumGpuIntersectionRays =
      p->autoMinimumGpuIntersectionRayCount(intersectionBackendContext);
    const std::uint64_t autoEstimatedQueryTransferBytes =
      p->autoEstimatedQueryTransferBytes(intersectionBackendContext);

#ifdef RAYTRACER_ENABLE_STATS
    ::render::stats::Counters::instance().reset();
#endif

    m_camera->viewPlane()->setup(m_camera->matrix(), buffer.rect());
    m_camera->setShowProgressIndicators(p->showProgressIndicators);

    auto rayCaster = std::make_shared<RecursiveRayCasterAdapter>(*m_scene, *p->integrator);
    auto camera = m_camera;
    auto tonemapOp = tonemap();
    Buffer<unsigned int>* bufferPtr = &buffer;

    const render::TilePlan tilePlan =
      render::TilePlan::forBuffer(buffer.width(), buffer.height(), p->queueSize);
    const auto renderStart = detail::WavefrontMetricsRecorder::Clock::now();
    const bool recordMetrics = p->shouldRecordRenderMetrics(*m_camera);
    if (recordMetrics) {
      p->metrics.reset(
        *m_camera, buffer.width(), buffer.height(), tilePlan, p->queueSize, *p->integrator,
        p->denoiser.get(), expectedIntersectionRays, expectedClosestHitIntersectionRays,
        expectedAnyHitIntersectionRays, autoMinimumGpuIntersectionRays,
        autoEstimatedQueryTransferBytes, p->samplingSeed, "sampler", p->convergenceEnabled,
        p->convergenceActiveSampleFractionThreshold, p->convergenceRadianceDeltaRmsThreshold,
        p->adaptiveSamplingEnabled, p->adaptiveMinimumSamples, p->adaptiveStddevThreshold);
    } else {
      p->metrics.clear();
    }
    auto tileRenderer = p->tileRenderer(recordMetrics);
    Buffer<double>* sampleRadianceStddevBuffer =
      p->prepareSampleRadianceStddevBuffer(buffer.width(), buffer.height());
    Buffer<Colord>* sampleRadianceStddevColorBuffer =
      p->prepareSampleRadianceStddevColorBuffer(buffer.width(), buffer.height());
    const auto samplingSeed = p->samplingSeed;
    const bool publishProgressSnapshots = progressiveDisplayEnabled();
    engine::dispatchTileTasks(
      tilePlan, *p->threadPool, p->tasks,
      [this, rayCaster, camera, bufferPtr, tonemapOp, sampleRadianceStddevBuffer,
       sampleRadianceStddevColorBuffer, samplingSeed, tileRenderer,
       publishProgressSnapshots](const Recti& rect, std::size_t tileIndex) {
        const std::optional<std::uint64_t> tileSeed =
          samplingSeed
            ? std::optional<std::uint64_t>(render::SamplingSeed::tileSeed(*samplingSeed, tileIndex))
            : std::nullopt;
        tileRenderer.renderDisplayTile(*camera, *rayCaster, *m_scene, *bufferPtr, tonemapOp, rect,
                                       tileSeed, publishProgressSnapshots,
                                       sampleRadianceStddevBuffer, sampleRadianceStddevColorBuffer);
      });
    if (recordMetrics) {
      p->metrics.finish(renderStart);
      if (!p->metricsEnabled && !m_camera->isCancelled()) {
        p->metrics.clear();
      }
    }
    p->clearPreparedIntersectionBackend();

#ifdef RAYTRACER_ENABLE_STATS
    ::render::stats::Counters::instance().dumpJson(std::cerr);
#endif
  }

  void WavefrontRaytracer::render(Buffer<Colord>& hdrBuffer, Buffer<unsigned int>& displayBuffer,
                                  std::shared_ptr<render::Tonemap> displayTonemap) {
    if (!core::util::bufferDimensionsEqual(hdrBuffer, displayBuffer)) {
      throw std::runtime_error("wavefront dual-output render requires matching buffer dimensions");
    }

    if (!m_scene || !m_camera) {
      p->lastSampleRadianceStddev.reset();
      p->lastSampleRadianceStddevColor.reset();
      hdrBuffer.clear();
      displayBuffer.clear();
      return;
    }

    p->tasks.clear();
    p->denoiserFeatureTasks.clear();
    const std::uint64_t expectedClosestHitIntersectionRays =
      p->expectedClosestHitIntersectionRayCount(*m_camera, hdrBuffer.width(), hdrBuffer.height());
    const std::uint64_t expectedAnyHitIntersectionRays =
      p->expectedAnyHitIntersectionRayCount(*m_camera, hdrBuffer.width(), hdrBuffer.height());
    const render::WavefrontIntersectionBackendSelectionContext intersectionBackendContext =
      p->intersectionBackendSelectionContext(expectedClosestHitIntersectionRays,
                                             expectedAnyHitIntersectionRays);
    const std::uint64_t expectedIntersectionRays = intersectionBackendContext.expectedRayCount;
    p->prepareIntersectionBackend(*m_scene, intersectionBackendContext);
    const std::uint64_t autoMinimumGpuIntersectionRays =
      p->autoMinimumGpuIntersectionRayCount(intersectionBackendContext);
    const std::uint64_t autoEstimatedQueryTransferBytes =
      p->autoEstimatedQueryTransferBytes(intersectionBackendContext);

#ifdef RAYTRACER_ENABLE_STATS
    ::render::stats::Counters::instance().reset();
#endif

    m_camera->viewPlane()->setup(m_camera->matrix(), hdrBuffer.rect());
    m_camera->setShowProgressIndicators(p->showProgressIndicators);

    auto rayCaster = std::make_shared<RecursiveRayCasterAdapter>(*m_scene, *p->integrator);
    auto camera = m_camera;
    Buffer<Colord>* hdrBufferPtr = &hdrBuffer;
    Buffer<unsigned int>* displayBufferPtr = &displayBuffer;

    const render::TilePlan tilePlan =
      render::TilePlan::forBuffer(hdrBuffer.width(), hdrBuffer.height(), p->queueSize);
    const auto renderStart = detail::WavefrontMetricsRecorder::Clock::now();
    const bool recordMetrics = p->shouldRecordRenderMetrics(*m_camera);
    if (recordMetrics) {
      p->metrics.reset(
        *m_camera, hdrBuffer.width(), hdrBuffer.height(), tilePlan, p->queueSize, *p->integrator,
        p->denoiser.get(), expectedIntersectionRays, expectedClosestHitIntersectionRays,
        expectedAnyHitIntersectionRays, autoMinimumGpuIntersectionRays,
        autoEstimatedQueryTransferBytes, p->samplingSeed, "sampler", p->convergenceEnabled,
        p->convergenceActiveSampleFractionThreshold, p->convergenceRadianceDeltaRmsThreshold,
        p->adaptiveSamplingEnabled, p->adaptiveMinimumSamples, p->adaptiveStddevThreshold);
    } else {
      p->metrics.clear();
    }
    auto tileRenderer = p->tileRenderer(recordMetrics);
    const auto denoiserFeatures = tileRenderer.buildDenoiserFeatures(
      *m_camera, *m_scene, hdrBuffer.rect(), tilePlan, *p->threadPool, p->denoiserFeatureTasks);
    const auto* denoiserFeaturePtr = denoiserFeatures.get();
    Buffer<double>* sampleRadianceStddevBuffer =
      p->prepareSampleRadianceStddevBuffer(hdrBuffer.width(), hdrBuffer.height());
    Buffer<Colord>* sampleRadianceStddevColorBuffer =
      p->prepareSampleRadianceStddevColorBuffer(hdrBuffer.width(), hdrBuffer.height());
    const auto samplingSeed = p->samplingSeed;
    const bool publishProgressSnapshots = progressiveDisplayEnabled();
    engine::dispatchTileTasks(
      tilePlan, *p->threadPool, p->tasks,
      [this, rayCaster, camera, hdrBufferPtr, displayBufferPtr, displayTonemap,
       sampleRadianceStddevBuffer, sampleRadianceStddevColorBuffer, samplingSeed, tileRenderer,
       denoiserFeaturePtr, publishProgressSnapshots](const Recti& rect, std::size_t tileIndex) {
        const std::optional<std::uint64_t> tileSeed =
          samplingSeed
            ? std::optional<std::uint64_t>(render::SamplingSeed::tileSeed(*samplingSeed, tileIndex))
            : std::nullopt;
        tileRenderer.renderDualOutputTile(*camera, *rayCaster, *m_scene, *hdrBufferPtr,
                                          *displayBufferPtr, displayTonemap, rect, tileSeed,
                                          publishProgressSnapshots, sampleRadianceStddevBuffer,
                                          sampleRadianceStddevColorBuffer, denoiserFeaturePtr);
      });
    tileRenderer.denoise(hdrBuffer, denoiserFeatures.get());
    tileRenderer.writeDisplayBuffer(displayBuffer, hdrBuffer, displayTonemap);
    if (recordMetrics) {
      p->metrics.finish(renderStart);
      if (!p->metricsEnabled && !m_camera->isCancelled() && !denoiserFeatures) {
        p->metrics.clear();
      }
    }
    p->clearPreparedIntersectionBackend();

#ifdef RAYTRACER_ENABLE_STATS
    ::render::stats::Counters::instance().dumpJson(std::cerr);
#endif
  }

  void WavefrontRaytracer::cancel() {
    if (m_camera) {
      m_camera->cancel();
    }
  }

  void WavefrontRaytracer::uncancel() {
    if (m_camera) {
      m_camera->uncancel();
    }
  }

  std::list<Recti> WavefrontRaytracer::activeTiles() const {
    std::list<Recti> result;
    for (const auto& task : p->denoiserFeatureTasks) {
      if (task->active.load(std::memory_order_acquire)) {
        result.push_back(task->rect);
      }
    }
    for (const auto& task : p->tasks) {
      if (task->active.load(std::memory_order_acquire)) {
        result.push_back(task->rect);
      }
    }
    return result;
  }

  std::list<Recti> WavefrontRaytracer::completedTiles() const {
    std::list<Recti> result;
    for (const auto& task : p->tasks) {
      if (task->completed.load(std::memory_order_acquire)) {
        result.push_back(task->rect);
      }
    }
    return result;
  }

  void WavefrontRaytracer::setIntegrator(std::unique_ptr<render::Integrator> integrator) {
    if (!integrator) {
      throw std::invalid_argument("WavefrontRaytracer integrator cannot be null");
    }
    p->integrator = std::move(integrator);
    if (p->maximumRecursionDepth) {
      p->integrator->setMaximumRecursionDepth(*p->maximumRecursionDepth);
    }
    p->configureIntegratorCancellation(*this);
  }

  const render::Integrator& WavefrontRaytracer::integrator() const {
    return *p->integrator;
  }

  void WavefrontRaytracer::setDenoiser(std::unique_ptr<render::Denoiser> denoiser) {
    p->denoiser = std::move(denoiser);
  }

  void WavefrontRaytracer::clearDenoiser() {
    p->denoiser.reset();
  }

  const render::Denoiser* WavefrontRaytracer::denoiser() const {
    return p->denoiser.get();
  }

  void WavefrontRaytracer::setMetricsEnabled(bool enabled) {
    p->metricsEnabled = enabled;
  }

  bool WavefrontRaytracer::metricsEnabled() const {
    return p->metricsEnabled;
  }

  void WavefrontRaytracer::setMaximumRecursionDepth(int depth) {
    p->maximumRecursionDepth = depth;
    p->integrator->setMaximumRecursionDepth(depth);
  }

  void WavefrontRaytracer::setSamplingSeed(std::uint64_t seed) {
    p->samplingSeed = seed;
  }

  void WavefrontRaytracer::clearSamplingSeed() {
    p->samplingSeed.reset();
  }

  std::optional<std::uint64_t> WavefrontRaytracer::samplingSeed() const {
    return p->samplingSeed;
  }

  void WavefrontRaytracer::setMaximumThreads(int threads) {
    p->threadPool->setMaxThreadCount(threads);
  }

  void WavefrontRaytracer::setQueueSize(int queue) {
    p->queueSize = queue;
  }

  void WavefrontRaytracer::setShowProgressIndicators(bool show) {
    p->showProgressIndicators = show;
  }

  void WavefrontRaytracer::setConvergenceEnabled(bool enabled) {
    p->convergenceEnabled = enabled;
  }

  bool WavefrontRaytracer::convergenceEnabled() const {
    return p->convergenceEnabled;
  }

  void WavefrontRaytracer::setConvergenceActiveSampleFractionThreshold(double fraction) {
    p->convergenceActiveSampleFractionThreshold = std::clamp(fraction, 0.0, 1.0);
  }

  double WavefrontRaytracer::convergenceActiveSampleFractionThreshold() const {
    return p->convergenceActiveSampleFractionThreshold;
  }

  void WavefrontRaytracer::setConvergenceRadianceDeltaRmsThreshold(double threshold) {
    p->convergenceRadianceDeltaRmsThreshold = std::max(0.0, threshold);
  }

  double WavefrontRaytracer::convergenceRadianceDeltaRmsThreshold() const {
    return p->convergenceRadianceDeltaRmsThreshold;
  }

  void WavefrontRaytracer::setAdaptiveSamplingEnabled(bool enabled) {
    p->adaptiveSamplingEnabled = enabled;
  }

  bool WavefrontRaytracer::adaptiveSamplingEnabled() const {
    return p->adaptiveSamplingEnabled;
  }

  void WavefrontRaytracer::setAdaptiveMinimumSamples(int samples) {
    p->adaptiveMinimumSamples = std::max(1, samples);
  }

  int WavefrontRaytracer::adaptiveMinimumSamples() const {
    return p->adaptiveMinimumSamples;
  }

  void WavefrontRaytracer::setAdaptiveStddevThreshold(double threshold) {
    p->adaptiveStddevThreshold = std::max(0.0, threshold);
  }

  double WavefrontRaytracer::adaptiveStddevThreshold() const {
    return p->adaptiveStddevThreshold;
  }

  void WavefrontRaytracer::setSampleRadianceStddevCaptureEnabled(bool enabled) {
    p->sampleRadianceStddevCaptureEnabled = enabled;
    if (!enabled) {
      p->lastSampleRadianceStddev.reset();
      p->lastSampleRadianceStddevColor.reset();
    }
  }

  bool WavefrontRaytracer::sampleRadianceStddevCaptureEnabled() const {
    return p->sampleRadianceStddevCaptureEnabled;
  }

  void
  WavefrontRaytracer::setIntersectionBackend(render::WavefrontIntersectionBackendChoice backend) {
    p->intersectionBackend = backend;
  }

  render::WavefrontIntersectionBackendChoice WavefrontRaytracer::intersectionBackend() const {
    return p->intersectionBackend;
  }

  std::shared_ptr<const Buffer<double>> WavefrontRaytracer::lastSampleRadianceStddev() const {
    return p->lastSampleRadianceStddev;
  }

  std::shared_ptr<const Buffer<Colord>> WavefrontRaytracer::lastSampleRadianceStddevColor() const {
    return p->lastSampleRadianceStddevColor;
  }

  WavefrontRenderMetrics WavefrontRaytracer::lastMetrics() const {
    return p->metrics.snapshot();
  }
}
