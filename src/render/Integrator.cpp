#include "render/Integrator.h"

#include "render/WavefrontIntersectionBackend.h"
#include "render/State.h"

#include <algorithm>
#include <cmath>

namespace render {
  namespace {
    std::string nonEmptyLabel(const char* label, const std::string& fallback) {
      if (label && *label) {
        return label;
      }
      return fallback;
    }

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

    void mergeMapMaximums(std::map<std::string, std::uint64_t>& target,
                          const std::map<std::string, std::uint64_t>& source) {
      for (const auto& [key, count] : source) {
        const std::string label = key.empty() ? "unknown" : key;
        target[label] = std::max(target[label], count);
      }
    }
  }

  const char* Integrator::diagnosticName() const {
    return "custom";
  }

  const char* Integrator::batchExecutionMode() const {
    return "scalar_loop";
  }

  bool Integrator::prefersProgressiveSamplePublishing() const {
    return false;
  }

  std::uint64_t Integrator::estimatedIntersectionRaysPerPrimarySample() const {
    return estimatedClosestHitRaysPerPrimarySample() + estimatedAnyHitRaysPerPrimarySample();
  }

  std::uint64_t Integrator::estimatedClosestHitRaysPerPrimarySample() const {
    return 1;
  }

  std::uint64_t Integrator::estimatedAnyHitRaysPerPrimarySample() const {
    return 0;
  }

  void IntegratorBatchMetrics::reset(bool scalarFallback) {
    usedScalarFallback = scalarFallback;
    activeSamplesPerDepth.clear();
    frontierRayHitsPerDepth.clear();
    frontierRayMissesPerDepth.clear();
    frontierPacketChunksPerDepth.clear();
    frontierPacketRaysPerDepth.clear();
    frontierClosestHitBatchChunksPerDepth.clear();
    frontierClosestHitBatchRaysPerDepth.clear();
    directLightAnyHitBatchChunksPerDepth.clear();
    directLightAnyHitBatchRaysPerDepth.clear();
    frontierRay4PacketChunksPerDepth.clear();
    frontierRay8PacketChunksPerDepth.clear();
    frontierScalarRaysPerDepth.clear();
    frontierPacketScalarFallbackRaysPerDepth.clear();
    frontierPacketScalarFallbackRaysByReason.clear();
    frontierPacketRefinedRaysPerDepth.clear();
    frontierPacketRefinedRaysByMaterial.clear();
    activeSampleDepthsProcessed = 0;
    retainedActiveSamplesPerDepth.clear();
    radianceDeltaSquaredSumPerDepth.clear();
    maxRadianceDeltaPerDepth.clear();
    compatibilityShadeSamples = 0;
    unsupportedPathMaterialSamples = 0;
    emitterHitSamples = 0;
    primaryEmitterHitSamples = 0;
    deltaEmitterHitSamples = 0;
    bsdfEmitterHitSamples = 0;
    misWeightedEmitterHitSamples = 0;
    directLightSamples = 0;
    directLightContributingSamples = 0;
    directLightOccludedSamples = 0;
    emittedRadianceLuminanceSum = 0.0;
    directLightRadianceLuminanceSum = 0.0;
    primaryDirectLightRadianceLuminanceSum = 0.0;
    secondaryDirectLightRadianceLuminanceSum = 0.0;
    ambientRadianceLuminanceSum = 0.0;
    missRadianceLuminanceSum = 0.0;
    compatibilityShadeRadianceLuminanceSum = 0.0;
    stoppedByConvergence = false;
    stoppedAfterDepth = 0;
    intersectionBackendRequest.clear();
    intersectionBackend.clear();
    intersectionBackendPlatform.clear();
    intersectionBackendAvailability.clear();
    intersectionBackendFallbackReason.clear();
    intersectionBackendExecutionPath.clear();
    intersectionBackendClosestHitExecutionPath.clear();
    intersectionBackendAnyHitExecutionPath.clear();
    intersectionBackendPlatformGpuDeviceAvailable = false;
    intersectionBackendPlatformGpuRenderPathAvailable = false;
    intersectionSceneCompiled = false;
    intersectionSceneBvhNodes = 0;
    intersectionScenePrimitives = 0;
    intersectionSceneTriangles = 0;
    intersectionSceneSpheres = 0;
    intersectionScenePlanes = 0;
    intersectionSceneRectangles = 0;
    intersectionSceneDisks = 0;
    intersectionSceneOpenCylinders = 0;
    intersectionSceneTori = 0;
    intersectionSceneTransforms = 0;
    intersectionSceneUnsupportedPrimitives = 0;
    intersectionSceneUnsupportedReasons.clear();
    intersectionSceneUploadBytes = 0;
    intersectionSceneTriangleClosestHitEligible = false;
    intersectionSceneBasicHitEligible = false;
    intersectionScenePackedClosestHitEligible = false;
    intersectionScenePackedAnyHitEligible = false;
    intersectionEstimatedRayUploadBytes = 0;
    intersectionEstimatedClosestHitReadbackBytes = 0;
    intersectionEstimatedAnyHitReadbackBytes = 0;
    intersectionEstimatedQueryTransferBytes = 0;
    intersectionEstimatedQueryRoundTrips = 0;
    intersectionEstimatedClosestHitQueryRoundTrips = 0;
    intersectionEstimatedAnyHitQueryRoundTrips = 0;
    intersectionBackendUploadWorkerSeconds = 0.0;
    intersectionBackendKernelWorkerSeconds = 0.0;
    intersectionBackendReadbackWorkerSeconds = 0.0;
    intersectionRaysSubmitted = 0;
    closestHitRaysSubmitted = 0;
    anyHitRaysSubmitted = 0;
    closestHitQueries = 0;
    anyHitQueries = 0;
    intersectionBackendPrefersClosestHitBatch = false;
    intersectionBackendPrefersAnyHitBatch = false;
    intersectionWorkerSeconds = 0.0;
    shadingWorkerSeconds = 0.0;
    pathSetupWorkerSeconds = 0.0;
    frontierPartitionWorkerSeconds = 0.0;
    frontierBookkeepingWorkerSeconds = 0.0;
    progressSnapshotWorkerSeconds = 0.0;
    convergenceTestWorkerSeconds = 0.0;
    observerConvergenceFeedbackDepths = 0;
  }

  void IntegratorBatchMetrics::recordActiveDepth(std::uint64_t activeSamples) {
    activeSamplesPerDepth.push_back(activeSamples);
    activeSampleDepthsProcessed += activeSamples;
  }

  void IntegratorBatchMetrics::recordRetainedActiveDepth(std::uint64_t activeSamples) {
    retainedActiveSamplesPerDepth.push_back(activeSamples);
  }

  void IntegratorBatchMetrics::recordFrontierIntersections(std::uint64_t hitRays,
                                                           std::uint64_t missRays) {
    frontierRayHitsPerDepth.push_back(hitRays);
    frontierRayMissesPerDepth.push_back(missRays);
  }

  void IntegratorBatchMetrics::recordFrontierTraversal(
    std::uint64_t packetChunks, std::uint64_t packetRays, std::uint64_t ray4PacketChunks,
    std::uint64_t ray8PacketChunks, std::uint64_t scalarRays,
    std::uint64_t packetScalarFallbackRays, std::uint64_t packetRefinedRays) {
    frontierPacketChunksPerDepth.push_back(packetChunks);
    frontierPacketRaysPerDepth.push_back(packetRays);
    frontierRay4PacketChunksPerDepth.push_back(ray4PacketChunks);
    frontierRay8PacketChunksPerDepth.push_back(ray8PacketChunks);
    frontierScalarRaysPerDepth.push_back(scalarRays);
    frontierPacketScalarFallbackRaysPerDepth.push_back(packetScalarFallbackRays);
    frontierPacketRefinedRaysPerDepth.push_back(packetRefinedRays);
  }

  void IntegratorBatchMetrics::recordFrontierClosestHitBatch(std::uint64_t batchChunks,
                                                             std::uint64_t batchRays) {
    frontierClosestHitBatchChunksPerDepth.push_back(batchChunks);
    frontierClosestHitBatchRaysPerDepth.push_back(batchRays);
  }

  void IntegratorBatchMetrics::recordDirectLightAnyHitBatch(std::uint64_t depth,
                                                            std::uint64_t batchChunks,
                                                            std::uint64_t batchRays) {
    if (directLightAnyHitBatchChunksPerDepth.size() <= depth) {
      directLightAnyHitBatchChunksPerDepth.resize(depth + 1);
    }
    if (directLightAnyHitBatchRaysPerDepth.size() <= depth) {
      directLightAnyHitBatchRaysPerDepth.resize(depth + 1);
    }
    directLightAnyHitBatchChunksPerDepth[depth] += batchChunks;
    directLightAnyHitBatchRaysPerDepth[depth] += batchRays;
  }

  void IntegratorBatchMetrics::recordPacketScalarFallbacksByReason(
    const std::map<std::string, std::uint64_t>& reasons) {
    for (const auto& [reason, count] : reasons) {
      const std::string label = reason.empty() ? "unknown" : reason;
      frontierPacketScalarFallbackRaysByReason[label] += count;
    }
  }

  void IntegratorBatchMetrics::recordPacketHitRefinement(const std::string& materialLabel) {
    const std::string label = materialLabel.empty() ? "unknown" : materialLabel;
    ++frontierPacketRefinedRaysByMaterial[label];
  }

  void
  IntegratorBatchMetrics::recordIntersectionBackend(const WavefrontIntersectionBackend& backend) {
    mergeLabel(intersectionBackendRequest, nonEmptyLabel(backend.requestedName(), "unknown"));
    mergeLabel(intersectionBackend, nonEmptyLabel(backend.name(), "unknown"));
    mergeLabel(intersectionBackendPlatform, nonEmptyLabel(backend.platformName(), ""));
    mergeLabel(intersectionBackendAvailability, nonEmptyLabel(backend.availability(), "unknown"));
    mergeLabel(intersectionBackendFallbackReason, nonEmptyLabel(backend.fallbackReason(), ""));
    intersectionBackendPlatformGpuDeviceAvailable =
      intersectionBackendPlatformGpuDeviceAvailable || backend.platformGpuDeviceAvailable();
    intersectionBackendPlatformGpuRenderPathAvailable =
      intersectionBackendPlatformGpuRenderPathAvailable || backend.platformGpuRenderPathAvailable();
    const WavefrontIntersectionSceneDiagnostics diagnostics = backend.compiledSceneDiagnostics();
    intersectionSceneCompiled = intersectionSceneCompiled || diagnostics.compiled;
    intersectionSceneBvhNodes = std::max(intersectionSceneBvhNodes, diagnostics.bvhNodes);
    intersectionScenePrimitives = std::max(intersectionScenePrimitives, diagnostics.primitives);
    intersectionSceneTriangles = std::max(intersectionSceneTriangles, diagnostics.triangles);
    intersectionSceneSpheres = std::max(intersectionSceneSpheres, diagnostics.spheres);
    intersectionScenePlanes = std::max(intersectionScenePlanes, diagnostics.planes);
    intersectionSceneRectangles = std::max(intersectionSceneRectangles, diagnostics.rectangles);
    intersectionSceneDisks = std::max(intersectionSceneDisks, diagnostics.disks);
    intersectionSceneOpenCylinders =
      std::max(intersectionSceneOpenCylinders, diagnostics.openCylinders);
    intersectionSceneTori = std::max(intersectionSceneTori, diagnostics.tori);
    intersectionSceneTransforms = std::max(intersectionSceneTransforms, diagnostics.transforms);
    intersectionSceneUnsupportedPrimitives =
      std::max(intersectionSceneUnsupportedPrimitives, diagnostics.unsupportedPrimitives);
    mergeMapMaximums(intersectionSceneUnsupportedReasons, diagnostics.unsupportedReasons);
    intersectionSceneUploadBytes = std::max(intersectionSceneUploadBytes, diagnostics.uploadBytes);
    intersectionSceneTriangleClosestHitEligible =
      intersectionSceneTriangleClosestHitEligible || diagnostics.triangleClosestHitKernelEligible;
    intersectionSceneBasicHitEligible =
      intersectionSceneBasicHitEligible || diagnostics.basicHitKernelEligible;
    intersectionScenePackedClosestHitEligible =
      intersectionScenePackedClosestHitEligible || diagnostics.packedClosestHitKernelEligible;
    intersectionScenePackedAnyHitEligible =
      intersectionScenePackedAnyHitEligible || diagnostics.packedAnyHitKernelEligible;
  }

  void IntegratorBatchMetrics::recordIntersectionQueryFallbackReason(
    const WavefrontIntersectionBackend& backend, const WavefrontIntersectionQueryTiming& timing) {
    if (timing.fallbackReason.empty()) {
      return;
    }
    const std::string backendFallbackReason = nonEmptyLabel(backend.fallbackReason(), "");
    if (intersectionBackendFallbackReason.empty() ||
        intersectionBackendFallbackReason == backendFallbackReason ||
        intersectionBackendFallbackReason == timing.fallbackReason) {
      intersectionBackendFallbackReason = timing.fallbackReason;
      return;
    }
    intersectionBackendFallbackReason = "mixed";
  }

  bool IntegratorBatchMetrics::recordIntersectionQueryTransfer(std::uint64_t rayUploadBytes,
                                                               std::uint64_t readbackBytes) {
    intersectionEstimatedRayUploadBytes += rayUploadBytes;
    intersectionEstimatedQueryTransferBytes += rayUploadBytes + readbackBytes;
    if (rayUploadBytes > 0 || readbackBytes > 0) {
      ++intersectionEstimatedQueryRoundTrips;
      return true;
    }
    return false;
  }

  void
  IntegratorBatchMetrics::recordClosestHitQuery(const WavefrontIntersectionBackend& backend,
                                                std::uint64_t submittedRays,
                                                const WavefrontIntersectionQueryTiming& timing) {
    recordIntersectionBackend(backend);
    recordIntersectionQueryFallbackReason(backend, timing);
    const std::string executionPath =
      timing.executionPath.empty() ? nonEmptyLabel(backend.closestHitExecutionPath(), "unknown")
                                   : timing.executionPath;
    mergeLabel(intersectionBackendExecutionPath, executionPath);
    mergeLabel(intersectionBackendClosestHitExecutionPath, executionPath);
    const std::uint64_t rayUploadBytes = backend.estimatedClosestHitRayUploadBytes(submittedRays);
    const std::uint64_t readbackBytes = backend.estimatedClosestHitReadbackBytes(submittedRays);
    intersectionEstimatedClosestHitReadbackBytes += readbackBytes;
    if (recordIntersectionQueryTransfer(rayUploadBytes, readbackBytes)) {
      ++intersectionEstimatedClosestHitQueryRoundTrips;
    }
    intersectionBackendUploadWorkerSeconds += timing.uploadSeconds;
    intersectionBackendKernelWorkerSeconds += timing.kernelSeconds;
    intersectionBackendReadbackWorkerSeconds += timing.readbackSeconds;
    ++closestHitQueries;
    intersectionBackendPrefersClosestHitBatch =
      intersectionBackendPrefersClosestHitBatch || backend.prefersClosestHitBatch(submittedRays);
    intersectionRaysSubmitted += submittedRays;
    closestHitRaysSubmitted += submittedRays;
  }

  void IntegratorBatchMetrics::recordAnyHitQuery(const WavefrontIntersectionBackend& backend,
                                                 std::uint64_t submittedRays,
                                                 const WavefrontIntersectionQueryTiming& timing) {
    recordIntersectionBackend(backend);
    recordIntersectionQueryFallbackReason(backend, timing);
    const std::string executionPath = timing.executionPath.empty()
                                        ? nonEmptyLabel(backend.anyHitExecutionPath(), "unknown")
                                        : timing.executionPath;
    mergeLabel(intersectionBackendExecutionPath, executionPath);
    mergeLabel(intersectionBackendAnyHitExecutionPath, executionPath);
    const std::uint64_t rayUploadBytes = backend.estimatedAnyHitRayUploadBytes(submittedRays);
    const std::uint64_t readbackBytes = backend.estimatedAnyHitReadbackBytes(submittedRays);
    intersectionEstimatedAnyHitReadbackBytes += readbackBytes;
    if (recordIntersectionQueryTransfer(rayUploadBytes, readbackBytes)) {
      ++intersectionEstimatedAnyHitQueryRoundTrips;
    }
    intersectionBackendUploadWorkerSeconds += timing.uploadSeconds;
    intersectionBackendKernelWorkerSeconds += timing.kernelSeconds;
    intersectionBackendReadbackWorkerSeconds += timing.readbackSeconds;
    ++anyHitQueries;
    intersectionBackendPrefersAnyHitBatch =
      intersectionBackendPrefersAnyHitBatch || backend.prefersAnyHitBatch(submittedRays);
    intersectionRaysSubmitted += submittedRays;
    anyHitRaysSubmitted += submittedRays;
  }

  void
  IntegratorBatchMetrics::mergeIntersectionBackendMetrics(const IntegratorBatchMetrics& source) {
    mergeLabel(intersectionBackendRequest, source.intersectionBackendRequest);
    mergeLabel(intersectionBackend, source.intersectionBackend);
    mergeLabel(intersectionBackendPlatform, source.intersectionBackendPlatform);
    mergeLabel(intersectionBackendAvailability, source.intersectionBackendAvailability);
    mergeLabel(intersectionBackendFallbackReason, source.intersectionBackendFallbackReason);
    mergeLabel(intersectionBackendExecutionPath, source.intersectionBackendExecutionPath);
    mergeLabel(intersectionBackendClosestHitExecutionPath,
               source.intersectionBackendClosestHitExecutionPath);
    mergeLabel(intersectionBackendAnyHitExecutionPath,
               source.intersectionBackendAnyHitExecutionPath);
    intersectionBackendPlatformGpuDeviceAvailable =
      intersectionBackendPlatformGpuDeviceAvailable ||
      source.intersectionBackendPlatformGpuDeviceAvailable;
    intersectionBackendPlatformGpuRenderPathAvailable =
      intersectionBackendPlatformGpuRenderPathAvailable ||
      source.intersectionBackendPlatformGpuRenderPathAvailable;
    intersectionSceneCompiled = intersectionSceneCompiled || source.intersectionSceneCompiled;
    intersectionSceneBvhNodes =
      std::max(intersectionSceneBvhNodes, source.intersectionSceneBvhNodes);
    intersectionScenePrimitives =
      std::max(intersectionScenePrimitives, source.intersectionScenePrimitives);
    intersectionSceneTriangles =
      std::max(intersectionSceneTriangles, source.intersectionSceneTriangles);
    intersectionSceneSpheres = std::max(intersectionSceneSpheres, source.intersectionSceneSpheres);
    intersectionScenePlanes = std::max(intersectionScenePlanes, source.intersectionScenePlanes);
    intersectionSceneRectangles =
      std::max(intersectionSceneRectangles, source.intersectionSceneRectangles);
    intersectionSceneDisks = std::max(intersectionSceneDisks, source.intersectionSceneDisks);
    intersectionSceneOpenCylinders =
      std::max(intersectionSceneOpenCylinders, source.intersectionSceneOpenCylinders);
    intersectionSceneTori = std::max(intersectionSceneTori, source.intersectionSceneTori);
    intersectionSceneTransforms =
      std::max(intersectionSceneTransforms, source.intersectionSceneTransforms);
    intersectionSceneUnsupportedPrimitives = std::max(
      intersectionSceneUnsupportedPrimitives, source.intersectionSceneUnsupportedPrimitives);
    mergeMapMaximums(intersectionSceneUnsupportedReasons,
                     source.intersectionSceneUnsupportedReasons);
    intersectionSceneUploadBytes =
      std::max(intersectionSceneUploadBytes, source.intersectionSceneUploadBytes);
    intersectionSceneTriangleClosestHitEligible =
      intersectionSceneTriangleClosestHitEligible ||
      source.intersectionSceneTriangleClosestHitEligible;
    intersectionSceneBasicHitEligible =
      intersectionSceneBasicHitEligible || source.intersectionSceneBasicHitEligible;
    intersectionScenePackedClosestHitEligible =
      intersectionScenePackedClosestHitEligible || source.intersectionScenePackedClosestHitEligible;
    intersectionScenePackedAnyHitEligible =
      intersectionScenePackedAnyHitEligible || source.intersectionScenePackedAnyHitEligible;
    intersectionEstimatedRayUploadBytes += source.intersectionEstimatedRayUploadBytes;
    intersectionEstimatedClosestHitReadbackBytes +=
      source.intersectionEstimatedClosestHitReadbackBytes;
    intersectionEstimatedAnyHitReadbackBytes += source.intersectionEstimatedAnyHitReadbackBytes;
    intersectionEstimatedQueryTransferBytes += source.intersectionEstimatedQueryTransferBytes;
    intersectionEstimatedQueryRoundTrips += source.intersectionEstimatedQueryRoundTrips;
    intersectionEstimatedClosestHitQueryRoundTrips +=
      source.intersectionEstimatedClosestHitQueryRoundTrips;
    intersectionEstimatedAnyHitQueryRoundTrips += source.intersectionEstimatedAnyHitQueryRoundTrips;
    intersectionBackendUploadWorkerSeconds += source.intersectionBackendUploadWorkerSeconds;
    intersectionBackendKernelWorkerSeconds += source.intersectionBackendKernelWorkerSeconds;
    intersectionBackendReadbackWorkerSeconds += source.intersectionBackendReadbackWorkerSeconds;
    intersectionRaysSubmitted += source.intersectionRaysSubmitted;
    closestHitRaysSubmitted += source.closestHitRaysSubmitted;
    anyHitRaysSubmitted += source.anyHitRaysSubmitted;
    closestHitQueries += source.closestHitQueries;
    anyHitQueries += source.anyHitQueries;
    intersectionBackendPrefersClosestHitBatch =
      intersectionBackendPrefersClosestHitBatch || source.intersectionBackendPrefersClosestHitBatch;
    intersectionBackendPrefersAnyHitBatch =
      intersectionBackendPrefersAnyHitBatch || source.intersectionBackendPrefersAnyHitBatch;
  }

  void IntegratorBatchMetrics::recordRadianceDeltaDepth(double squaredSum, double maxDelta) {
    radianceDeltaSquaredSumPerDepth.push_back(squaredSum);
    maxRadianceDeltaPerDepth.push_back(maxDelta);
  }

  void IntegratorBatchMetrics::recordUnsupportedPathMaterial() {
    ++unsupportedPathMaterialSamples;
  }

  void IntegratorBatchMetrics::recordEmitterHit(bool sampledFromBsdf, bool bsdfSampleDelta,
                                                bool misWeighted) {
    ++emitterHitSamples;
    if (!sampledFromBsdf) {
      ++primaryEmitterHitSamples;
    } else if (bsdfSampleDelta) {
      ++deltaEmitterHitSamples;
    } else {
      ++bsdfEmitterHitSamples;
    }
    if (misWeighted) {
      ++misWeightedEmitterHitSamples;
    }
  }

  void IntegratorBatchMetrics::recordDirectLightSample(bool occluded, bool contributing) {
    ++directLightSamples;
    if (contributing) {
      ++directLightContributingSamples;
    }
    if (occluded) {
      ++directLightOccludedSamples;
    }
  }

  void IntegratorBatchMetrics::recordEmittedRadiance(const Colord& contribution) {
    emittedRadianceLuminanceSum += contributionLuminance(contribution);
  }

  void IntegratorBatchMetrics::recordDirectLightRadiance(const Colord& contribution,
                                                         bool primaryBounce) {
    const double luminance = contributionLuminance(contribution);
    directLightRadianceLuminanceSum += luminance;
    if (primaryBounce) {
      primaryDirectLightRadianceLuminanceSum += luminance;
    } else {
      secondaryDirectLightRadianceLuminanceSum += luminance;
    }
  }

  void IntegratorBatchMetrics::recordAmbientRadiance(const Colord& contribution) {
    ambientRadianceLuminanceSum += contributionLuminance(contribution);
  }

  void IntegratorBatchMetrics::recordMissRadiance(const Colord& contribution) {
    missRadianceLuminanceSum += contributionLuminance(contribution);
  }

  void IntegratorBatchMetrics::recordCompatibilityShadeRadiance(const Colord& contribution) {
    compatibilityShadeRadianceLuminanceSum += contributionLuminance(contribution);
  }

  double IntegratorBatchMetrics::contributionLuminance(const Colord& contribution) const {
    return contribution.r() * 0.299 + contribution.g() * 0.587 + contribution.b() * 0.114;
  }

  const WavefrontIntersectionBackend& IntegratorBatchSettings::resolvedIntersectionBackend() const {
    return intersectionBackend ? *intersectionBackend : CpuWavefrontIntersectionBackend::instance();
  }

  std::vector<Colord> Integrator::radianceBatch(const Scene& scene,
                                                const std::vector<IntegratorRaySample>& samples,
                                                const RayCaster& recursiveRayCaster,
                                                IntegratorBatchMetrics* metrics,
                                                const IntegratorBatchSettings& settings) const {
    if (metrics) {
      metrics->reset(/*scalarFallback=*/true);
    }

    std::vector<Colord> result;
    result.reserve(samples.size());

    double deltaSquaredSum = 0.0;
    double maxDelta = 0.0;
    for (const auto& sample : samples) {
      State state;
      state.timeSample = sample.timeSample;
      state.animationFrame = sample.animationFrame;
      state.animationTime = sample.animationTime;
      state.sampleStream = sample.sampleStream();
      const Colord color = radiance(scene, sample.ray, state, recursiveRayCaster);
      if (metrics) {
        const double deltaSquared = radianceDeltaSquared(Colord::black(), color);
        deltaSquaredSum += deltaSquared;
        maxDelta = std::max(maxDelta, std::sqrt(deltaSquared));
      }
      result.push_back(color);
    }

    if (!samples.empty()) {
      if (metrics) {
        metrics->recordActiveDepth(samples.size());
        metrics->recordRetainedActiveDepth(0);
        metrics->recordRadianceDeltaDepth(deltaSquaredSum, maxDelta);
      }
      if (settings.progressObserver) {
        (void)settings.progressObserver->depthCompleted(/*completedDepth=*/1, result,
                                                        samples.size());
      }
    }

    return result;
  }

  void Integrator::setMaximumRecursionDepth(int) {
  }

  void Integrator::setCancellationCallback(CancellationCallback) {
  }

  double Integrator::radianceDeltaSquared(const Colord& before, const Colord& after) const {
    const Colord delta = after - before;
    return delta.r() * delta.r() + delta.g() * delta.g() + delta.b() * delta.b();
  }
}
