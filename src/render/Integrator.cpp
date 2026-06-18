#include "render/Integrator.h"

#include "render/GpuIntersectionScene.h"
#include "render/GpuTracingScene.h"
#include "render/TracingPathStateBuffer.h"
#include "render/WavefrontIntersectionBackend.h"
#include "render/State.h"

#include "core/util/ScopedTimer.h"

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

    template<typename T>
    void addVectorValues(std::vector<T>& target, const std::vector<T>& source) {
      if (target.size() < source.size()) {
        target.resize(source.size(), T{});
      }
      for (std::size_t index = 0; index != source.size(); ++index) {
        target[index] += source[index];
      }
    }

    void addMapValues(std::map<std::string, std::uint64_t>& target,
                      const std::map<std::string, std::uint64_t>& source) {
      for (const auto& [key, value] : source) {
        target[key] += value;
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
    directLightSelectionHostBytesPerDepth.clear();
    directLightOcclusionHostBytesPerDepth.clear();
    directLightContributionHostBytesPerDepth.clear();
    directLightSelectionHostBytes = 0;
    directLightOcclusionHostBytes = 0;
    directLightContributionHostBytes = 0;
    directLightContributionExecutionPath.clear();
    directLightContributionFallbackReason.clear();
    directLightAnyHitFrontierPackedRayBytes = 0;
    directLightAnyHitFrontierHostQueryBytes = 0;
    directLightAnyHitFrontierStateHandleBytes = 0;
    directLightAnyHitFrontierPackedRayBytesPerDepth.clear();
    directLightAnyHitFrontierHostQueryBytesPerDepth.clear();
    directLightAnyHitFrontierStateHandleBytesPerDepth.clear();
    frontierRay4PacketChunksPerDepth.clear();
    frontierRay8PacketChunksPerDepth.clear();
    frontierScalarRaysPerDepth.clear();
    frontierPacketScalarFallbackRaysPerDepth.clear();
    frontierPacketScalarFallbackRaysByReason.clear();
    frontierPacketRefinedRaysPerDepth.clear();
    frontierPacketRefinedRaysByMaterial.clear();
    activeSampleDepthsProcessed = 0;
    retainedActiveSamplesPerDepth.clear();
    activeHitHostBytesPerDepth.clear();
    activeHitHostBytesProcessed = 0;
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
    intersectionBackendClosestHitFrontierResidency.clear();
    intersectionBackendAnyHitFrontierResidency.clear();
    intersectionBackendClosestHitFrontierPackedRayBytes = 0;
    intersectionBackendAnyHitFrontierPackedRayBytes = 0;
    intersectionBackendClosestHitFrontierHostQueryBytes = 0;
    intersectionBackendAnyHitFrontierHostQueryBytes = 0;
    intersectionBackendClosestHitFrontierStateHandleBytes = 0;
    intersectionBackendAnyHitFrontierStateHandleBytes = 0;
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
    tracingSceneCompiled = false;
    tracingSceneMaterials = 0;
    tracingSceneTextures = 0;
    tracingSceneLights = 0;
    tracingSceneEnvironment = 0;
    tracingSceneDebugIds = 0;
    tracingSceneUnsupportedMaterials = 0;
    tracingSceneUnsupportedTextures = 0;
    tracingSceneUnsupportedLights = 0;
    tracingSceneUnsupportedMaterialReasons.clear();
    tracingSceneUnsupportedTextureReasons.clear();
    tracingSceneUnsupportedLightReasons.clear();
    tracingSceneUploadBytes = 0;
    intersectionEstimatedRayUploadBytes = 0;
    intersectionEstimatedClosestHitRayUploadBytes = 0;
    intersectionEstimatedAnyHitRayUploadBytes = 0;
    intersectionEstimatedClosestHitReadbackBytes = 0;
    intersectionEstimatedAnyHitReadbackBytes = 0;
    intersectionEstimatedQueryTransferBytes = 0;
    intersectionEstimatedClosestHitQueryTransferBytes = 0;
    intersectionEstimatedAnyHitQueryTransferBytes = 0;
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
    intersectionBackendSupportsResidentFrontiers = false;
    intersectionBackendSupportsGpuFrontierCompaction = false;
    intersectionBackendGpuFrontierCompactionUnavailableReason.clear();
    intersectionBackendSupportsPreparedRayBatchCompaction = false;
    intersectionBackendSupportsResidentDirectLightBatches = false;
    intersectionBackendResidentDirectLightBatchesUnavailableReason.clear();
    residentPathLoopExecutionPath.clear();
    residentPathLoopResidency.clear();
    residentPathLoopDepths = 0;
    residentPathLoopInputPaths = 0;
    residentPathLoopRetainedPaths = 0;
    residentPathLoopRemovedPaths = 0;
    residentPathLoopMovedPaths = 0;
    residentPathLoopRetainedIndexBytes = 0;
    residentPathLoopResidentPathStateBytes = 0;
    residentPathLoopInputResidentPathStateBytes = 0;
    residentPathLoopRetainedResidentPathStateBytes = 0;
    residentPathLoopRemovedResidentPathStateBytes = 0;
    residentPathLoopCompactionPasses = 0;
    residentPathLoopRoundTrips = 0;
    residentPathLoopSavedHostReadbacks = 0;
    residentPathLoopSavedHostReadbackBytes = 0;
    intersectionWorkerSeconds = 0.0;
    shadingWorkerSeconds = 0.0;
    pathSetupWorkerSeconds = 0.0;
    frontierPartitionWorkerSeconds = 0.0;
    frontierBookkeepingWorkerSeconds = 0.0;
    progressSnapshotWorkerSeconds = 0.0;
    convergenceTestWorkerSeconds = 0.0;
    observerConvergenceFeedbackDepths = 0;
    activeHostPathStateBytesPerDepth.clear();
    retainedHostPathStateBytesPerDepth.clear();
    activeHostPathStateBytesProcessed = 0;
    spawnedContinuationSamplesPerDepth.clear();
    spawnedContinuationHostPathStateBytesPerDepth.clear();
    spawnedContinuationSamples = 0;
    spawnedContinuationHostPathStateBytes = 0;
    frontierCompactionPasses = 0;
    frontierCompactionInputSamples = 0;
    frontierCompactionRetainedSamples = 0;
    frontierCompactionRemovedSamples = 0;
    frontierCompactionMovedSamples = 0;
    frontierCompactionRetainedIndexBytes = 0;
    frontierCompactionInputHostPathStateBytes = 0;
    frontierCompactionRetainedHostPathStateBytes = 0;
    frontierCompactionRemovedHostPathStateBytes = 0;
    frontierCompactionExecutionPath.clear();
    residentPathLoopAccumulation.reset();
  }

  void IntegratorBatchMetrics::mergeFrom(const IntegratorBatchMetrics& source) {
    usedScalarFallback = usedScalarFallback || source.usedScalarFallback;
    mergeIntersectionBackendMetrics(source);
    addVectorValues(activeSamplesPerDepth, source.activeSamplesPerDepth);
    addVectorValues(frontierRayHitsPerDepth, source.frontierRayHitsPerDepth);
    addVectorValues(frontierRayMissesPerDepth, source.frontierRayMissesPerDepth);
    addVectorValues(frontierPacketChunksPerDepth, source.frontierPacketChunksPerDepth);
    addVectorValues(frontierPacketRaysPerDepth, source.frontierPacketRaysPerDepth);
    addVectorValues(frontierClosestHitBatchChunksPerDepth,
                    source.frontierClosestHitBatchChunksPerDepth);
    addVectorValues(frontierClosestHitBatchRaysPerDepth,
                    source.frontierClosestHitBatchRaysPerDepth);
    addVectorValues(directLightAnyHitBatchChunksPerDepth,
                    source.directLightAnyHitBatchChunksPerDepth);
    addVectorValues(directLightAnyHitBatchRaysPerDepth, source.directLightAnyHitBatchRaysPerDepth);
    addVectorValues(directLightSelectionHostBytesPerDepth,
                    source.directLightSelectionHostBytesPerDepth);
    addVectorValues(directLightOcclusionHostBytesPerDepth,
                    source.directLightOcclusionHostBytesPerDepth);
    addVectorValues(directLightContributionHostBytesPerDepth,
                    source.directLightContributionHostBytesPerDepth);
    addVectorValues(directLightAnyHitFrontierPackedRayBytesPerDepth,
                    source.directLightAnyHitFrontierPackedRayBytesPerDepth);
    addVectorValues(directLightAnyHitFrontierHostQueryBytesPerDepth,
                    source.directLightAnyHitFrontierHostQueryBytesPerDepth);
    addVectorValues(directLightAnyHitFrontierStateHandleBytesPerDepth,
                    source.directLightAnyHitFrontierStateHandleBytesPerDepth);
    addVectorValues(frontierRay4PacketChunksPerDepth, source.frontierRay4PacketChunksPerDepth);
    addVectorValues(frontierRay8PacketChunksPerDepth, source.frontierRay8PacketChunksPerDepth);
    addVectorValues(frontierScalarRaysPerDepth, source.frontierScalarRaysPerDepth);
    addVectorValues(frontierPacketScalarFallbackRaysPerDepth,
                    source.frontierPacketScalarFallbackRaysPerDepth);
    addMapValues(frontierPacketScalarFallbackRaysByReason,
                 source.frontierPacketScalarFallbackRaysByReason);
    addVectorValues(frontierPacketRefinedRaysPerDepth, source.frontierPacketRefinedRaysPerDepth);
    addMapValues(frontierPacketRefinedRaysByMaterial, source.frontierPacketRefinedRaysByMaterial);
    activeSampleDepthsProcessed += source.activeSampleDepthsProcessed;
    activeHostPathStateBytesProcessed += source.activeHostPathStateBytesProcessed;
    activeHitHostBytesProcessed += source.activeHitHostBytesProcessed;
    addVectorValues(activeHostPathStateBytesPerDepth, source.activeHostPathStateBytesPerDepth);
    addVectorValues(activeHitHostBytesPerDepth, source.activeHitHostBytesPerDepth);
    addVectorValues(retainedHostPathStateBytesPerDepth, source.retainedHostPathStateBytesPerDepth);
    spawnedContinuationSamples += source.spawnedContinuationSamples;
    spawnedContinuationHostPathStateBytes += source.spawnedContinuationHostPathStateBytes;
    addVectorValues(spawnedContinuationSamplesPerDepth, source.spawnedContinuationSamplesPerDepth);
    addVectorValues(spawnedContinuationHostPathStateBytesPerDepth,
                    source.spawnedContinuationHostPathStateBytesPerDepth);
    addVectorValues(radianceDeltaSquaredSumPerDepth, source.radianceDeltaSquaredSumPerDepth);
    if (maxRadianceDeltaPerDepth.size() < source.maxRadianceDeltaPerDepth.size()) {
      maxRadianceDeltaPerDepth.resize(source.maxRadianceDeltaPerDepth.size(), 0.0);
    }
    for (std::size_t index = 0; index != source.maxRadianceDeltaPerDepth.size(); ++index) {
      maxRadianceDeltaPerDepth[index] =
        std::max(maxRadianceDeltaPerDepth[index], source.maxRadianceDeltaPerDepth[index]);
    }
    compatibilityShadeSamples += source.compatibilityShadeSamples;
    unsupportedPathMaterialSamples += source.unsupportedPathMaterialSamples;
    emitterHitSamples += source.emitterHitSamples;
    primaryEmitterHitSamples += source.primaryEmitterHitSamples;
    deltaEmitterHitSamples += source.deltaEmitterHitSamples;
    bsdfEmitterHitSamples += source.bsdfEmitterHitSamples;
    misWeightedEmitterHitSamples += source.misWeightedEmitterHitSamples;
    directLightSamples += source.directLightSamples;
    directLightContributingSamples += source.directLightContributingSamples;
    directLightOccludedSamples += source.directLightOccludedSamples;
    directLightSelectionHostBytes += source.directLightSelectionHostBytes;
    directLightOcclusionHostBytes += source.directLightOcclusionHostBytes;
    directLightContributionHostBytes += source.directLightContributionHostBytes;
    mergeLabel(directLightContributionExecutionPath, source.directLightContributionExecutionPath);
    mergeLabel(directLightContributionFallbackReason, source.directLightContributionFallbackReason);
    emittedRadianceLuminanceSum += source.emittedRadianceLuminanceSum;
    directLightRadianceLuminanceSum += source.directLightRadianceLuminanceSum;
    primaryDirectLightRadianceLuminanceSum += source.primaryDirectLightRadianceLuminanceSum;
    secondaryDirectLightRadianceLuminanceSum += source.secondaryDirectLightRadianceLuminanceSum;
    ambientRadianceLuminanceSum += source.ambientRadianceLuminanceSum;
    missRadianceLuminanceSum += source.missRadianceLuminanceSum;
    compatibilityShadeRadianceLuminanceSum += source.compatibilityShadeRadianceLuminanceSum;
    stoppedByConvergence = stoppedByConvergence || source.stoppedByConvergence;
    stoppedAfterDepth = std::max(stoppedAfterDepth, source.stoppedAfterDepth);
    intersectionWorkerSeconds += source.intersectionWorkerSeconds;
    shadingWorkerSeconds += source.shadingWorkerSeconds;
    pathSetupWorkerSeconds += source.pathSetupWorkerSeconds;
    frontierPartitionWorkerSeconds += source.frontierPartitionWorkerSeconds;
    frontierBookkeepingWorkerSeconds += source.frontierBookkeepingWorkerSeconds;
    progressSnapshotWorkerSeconds += source.progressSnapshotWorkerSeconds;
    convergenceTestWorkerSeconds += source.convergenceTestWorkerSeconds;
    observerConvergenceFeedbackDepths += source.observerConvergenceFeedbackDepths;
    addVectorValues(retainedActiveSamplesPerDepth, source.retainedActiveSamplesPerDepth);
    directLightAnyHitFrontierPackedRayBytes += source.directLightAnyHitFrontierPackedRayBytes;
    directLightAnyHitFrontierHostQueryBytes += source.directLightAnyHitFrontierHostQueryBytes;
    directLightAnyHitFrontierStateHandleBytes += source.directLightAnyHitFrontierStateHandleBytes;
    frontierCompactionPasses += source.frontierCompactionPasses;
    frontierCompactionInputSamples += source.frontierCompactionInputSamples;
    frontierCompactionRetainedSamples += source.frontierCompactionRetainedSamples;
    frontierCompactionRemovedSamples += source.frontierCompactionRemovedSamples;
    frontierCompactionMovedSamples += source.frontierCompactionMovedSamples;
    frontierCompactionRetainedIndexBytes += source.frontierCompactionRetainedIndexBytes;
    frontierCompactionInputHostPathStateBytes += source.frontierCompactionInputHostPathStateBytes;
    frontierCompactionRetainedHostPathStateBytes +=
      source.frontierCompactionRetainedHostPathStateBytes;
    frontierCompactionRemovedHostPathStateBytes +=
      source.frontierCompactionRemovedHostPathStateBytes;
    mergeLabel(frontierCompactionExecutionPath, source.frontierCompactionExecutionPath);
    mergeLabel(residentPathLoopExecutionPath, source.residentPathLoopExecutionPath);
    mergeLabel(residentPathLoopResidency, source.residentPathLoopResidency);
    residentPathLoopDepths += source.residentPathLoopDepths;
    residentPathLoopInputPaths += source.residentPathLoopInputPaths;
    residentPathLoopRetainedPaths += source.residentPathLoopRetainedPaths;
    residentPathLoopRemovedPaths += source.residentPathLoopRemovedPaths;
    residentPathLoopMovedPaths += source.residentPathLoopMovedPaths;
    residentPathLoopRetainedIndexBytes += source.residentPathLoopRetainedIndexBytes;
    residentPathLoopResidentPathStateBytes = std::max(
      residentPathLoopResidentPathStateBytes, source.residentPathLoopResidentPathStateBytes);
    residentPathLoopInputResidentPathStateBytes +=
      source.residentPathLoopInputResidentPathStateBytes;
    residentPathLoopRetainedResidentPathStateBytes +=
      source.residentPathLoopRetainedResidentPathStateBytes;
    residentPathLoopRemovedResidentPathStateBytes +=
      source.residentPathLoopRemovedResidentPathStateBytes;
    residentPathLoopCompactionPasses += source.residentPathLoopCompactionPasses;
    residentPathLoopRoundTrips += source.residentPathLoopRoundTrips;
    residentPathLoopSavedHostReadbacks += source.residentPathLoopSavedHostReadbacks;
    residentPathLoopSavedHostReadbackBytes += source.residentPathLoopSavedHostReadbackBytes;
    if (source.residentPathLoopAccumulation) {
      if (!residentPathLoopAccumulation) {
        residentPathLoopAccumulation = *source.residentPathLoopAccumulation;
      } else {
        residentPathLoopAccumulation->clearOperations +=
          source.residentPathLoopAccumulation->clearOperations;
        residentPathLoopAccumulation->addOperations +=
          source.residentPathLoopAccumulation->addOperations;
        residentPathLoopAccumulation->addedSamples +=
          source.residentPathLoopAccumulation->addedSamples;
        residentPathLoopAccumulation->resolveOperations +=
          source.residentPathLoopAccumulation->resolveOperations;
        residentPathLoopAccumulation->readbackOperations +=
          source.residentPathLoopAccumulation->readbackOperations;
        residentPathLoopAccumulation->readbackBytes +=
          source.residentPathLoopAccumulation->readbackBytes;
        residentPathLoopAccumulation->residentBytes =
          std::max(residentPathLoopAccumulation->residentBytes,
                   source.residentPathLoopAccumulation->residentBytes);
        mergeLabel(residentPathLoopAccumulation->backend,
                   source.residentPathLoopAccumulation->backend);
        mergeLabel(residentPathLoopAccumulation->residency,
                   source.residentPathLoopAccumulation->residency);
      }
    }
  }

  void IntegratorBatchMetrics::recordActiveDepth(std::uint64_t activeSamples) {
    activeSamplesPerDepth.push_back(activeSamples);
    activeSampleDepthsProcessed += activeSamples;
  }

  void IntegratorBatchMetrics::recordRetainedActiveDepth(std::uint64_t activeSamples) {
    retainedActiveSamplesPerDepth.push_back(activeSamples);
  }

  void IntegratorBatchMetrics::recordActiveHostPathStateBytes(std::uint64_t bytes) {
    activeHostPathStateBytesPerDepth.push_back(bytes);
    activeHostPathStateBytesProcessed += bytes;
  }

  void IntegratorBatchMetrics::recordActiveHitHostBytes(std::uint64_t bytes) {
    activeHitHostBytesPerDepth.push_back(bytes);
    activeHitHostBytesProcessed += bytes;
  }

  void IntegratorBatchMetrics::recordRetainedHostPathStateBytes(std::uint64_t bytes) {
    retainedHostPathStateBytesPerDepth.push_back(bytes);
  }

  void IntegratorBatchMetrics::recordSpawnedContinuations(std::uint64_t samples,
                                                          std::uint64_t hostPathStateBytes) {
    spawnedContinuationSamplesPerDepth.push_back(samples);
    spawnedContinuationHostPathStateBytesPerDepth.push_back(hostPathStateBytes);
    spawnedContinuationSamples += samples;
    spawnedContinuationHostPathStateBytes += hostPathStateBytes;
  }

  void IntegratorBatchMetrics::recordFrontierCompaction(
    std::uint64_t inputSamples, std::uint64_t retainedSamples, std::uint64_t movedSamples,
    const std::string& executionPath, std::uint64_t retainedIndexBytes,
    std::uint64_t inputHostPathStateBytes, std::uint64_t retainedHostPathStateBytes,
    std::uint64_t removedHostPathStateBytes) {
    ++frontierCompactionPasses;
    mergeLabel(frontierCompactionExecutionPath, executionPath.empty() ? "unknown" : executionPath);
    frontierCompactionInputSamples += inputSamples;
    frontierCompactionRetainedSamples += retainedSamples;
    frontierCompactionRemovedSamples +=
      inputSamples > retainedSamples ? inputSamples - retainedSamples : 0;
    frontierCompactionMovedSamples += movedSamples;
    frontierCompactionRetainedIndexBytes += retainedIndexBytes;
    frontierCompactionInputHostPathStateBytes += inputHostPathStateBytes;
    frontierCompactionRetainedHostPathStateBytes += retainedHostPathStateBytes;
    frontierCompactionRemovedHostPathStateBytes += removedHostPathStateBytes;
  }

  void IntegratorBatchMetrics::recordHostFrontierCompaction(std::uint64_t inputSamples,
                                                            std::uint64_t retainedSamples,
                                                            std::uint64_t movedSamples) {
    recordFrontierCompaction(inputSamples, retainedSamples, movedSamples, "host",
                             retainedSamples * sizeof(std::uint32_t));
  }

  void IntegratorBatchMetrics::recordResidentPathLoopExecution(
    const ResidentPathLoopDiagnostics& diagnostics, std::uint64_t roundTrips) {
    mergeLabel(residentPathLoopExecutionPath, "gpu_resident_path_loop");
    mergeLabel(residentPathLoopResidency, diagnostics.buffers.residency);
    residentPathLoopDepths += diagnostics.depths.size();
    residentPathLoopResidentPathStateBytes =
      std::max(residentPathLoopResidentPathStateBytes, diagnostics.buffers.residentBytes);
    residentPathLoopRoundTrips += roundTrips;

    for (const ResidentPathLoopDepthDiagnostics& depth : diagnostics.depths) {
      const ResidentPathCompactionContract& compaction = depth.compaction;
      ++residentPathLoopCompactionPasses;
      mergeLabel(residentPathLoopExecutionPath, compaction.executionPath());
      residentPathLoopInputPaths += compaction.inputPathCount();
      residentPathLoopRetainedPaths += compaction.retainedPathCount();
      residentPathLoopRemovedPaths += compaction.removedPathCount();
      residentPathLoopMovedPaths += compaction.movedPathCount();
      residentPathLoopRetainedIndexBytes += compaction.retainedIndexBytes();
      residentPathLoopInputResidentPathStateBytes += compaction.inputResidentPathStateBytes();
      residentPathLoopRetainedResidentPathStateBytes += compaction.retainedResidentPathStateBytes();
      residentPathLoopRemovedResidentPathStateBytes += compaction.removedResidentPathStateBytes();
      ++residentPathLoopSavedHostReadbacks;
      residentPathLoopSavedHostReadbackBytes += compaction.inputResidentPathStateBytes();
    }
  }

  void IntegratorBatchMetrics::recordResidentPathLoopAccumulation(
    const TracingAccumulationDiagnostics& diagnostics) {
    residentPathLoopAccumulation = diagnostics;
  }

  double IntegratorBatchMetrics::frontierCompactionRemovedSampleFraction() const {
    if (frontierCompactionInputSamples == 0) {
      return 0.0;
    }
    return static_cast<double>(frontierCompactionRemovedSamples) /
           static_cast<double>(frontierCompactionInputSamples);
  }

  double IntegratorBatchMetrics::frontierCompactionMovedRetainedSampleFraction() const {
    if (frontierCompactionRetainedSamples == 0) {
      return 0.0;
    }
    return static_cast<double>(frontierCompactionMovedSamples) /
           static_cast<double>(frontierCompactionRetainedSamples);
  }

  bool IntegratorBatchMetrics::hasCompactionCandidateDepth(std::size_t depth) const {
    if (depth >= activeSamplesPerDepth.size() || depth >= retainedActiveSamplesPerDepth.size()) {
      return false;
    }
    return activeSamplesPerDepth[depth] > retainedActiveSamplesPerDepth[depth];
  }

  std::uint64_t IntegratorBatchMetrics::compactionCandidateSamplesAtDepth(std::size_t depth) const {
    if (!hasCompactionCandidateDepth(depth)) {
      return 0;
    }
    return activeSamplesPerDepth[depth] - retainedActiveSamplesPerDepth[depth];
  }

  std::uint64_t IntegratorBatchMetrics::compactionCandidateDepthCount() const {
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

  std::uint64_t IntegratorBatchMetrics::compactionCandidateSampleCount() const {
    const std::size_t depthCount =
      std::min(activeSamplesPerDepth.size(), retainedActiveSamplesPerDepth.size());
    std::uint64_t count = 0;
    for (std::size_t depth = 0; depth != depthCount; ++depth) {
      count += compactionCandidateSamplesAtDepth(depth);
    }
    return count;
  }

  std::uint64_t IntegratorBatchMetrics::compactionCandidatePackedRayBytes() const {
    return compactionCandidateSampleCount() * sizeof(GpuIntersectionRay);
  }

  std::uint64_t IntegratorBatchMetrics::compactionCandidateStateHandleBytes() const {
    return compactionCandidateSampleCount() * sizeof(State*);
  }

  std::uint64_t IntegratorBatchMetrics::compactionCandidateHostPathStateBytes() const {
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

  double IntegratorBatchMetrics::compactionCandidateSampleFraction() const {
    if (activeSampleDepthsProcessed == 0) {
      return 0.0;
    }
    return static_cast<double>(compactionCandidateSampleCount()) /
           static_cast<double>(activeSampleDepthsProcessed);
  }

  std::uint64_t IntegratorBatchMetrics::largestCompactionCandidateDepth() const {
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

  std::uint64_t IntegratorBatchMetrics::largestCompactionCandidateSampleCount() const {
    const std::size_t depthCount =
      std::min(activeSamplesPerDepth.size(), retainedActiveSamplesPerDepth.size());
    std::uint64_t largestSamples = 0;
    for (std::size_t depth = 0; depth != depthCount; ++depth) {
      largestSamples = std::max(largestSamples, compactionCandidateSamplesAtDepth(depth));
    }
    return largestSamples;
  }

  std::uint64_t IntegratorBatchMetrics::largestCompactionCandidatePackedRayBytes() const {
    return largestCompactionCandidateSampleCount() * sizeof(GpuIntersectionRay);
  }

  std::uint64_t IntegratorBatchMetrics::largestCompactionCandidateStateHandleBytes() const {
    return largestCompactionCandidateSampleCount() * sizeof(State*);
  }

  std::uint64_t IntegratorBatchMetrics::largestCompactionCandidateHostPathStateBytes() const {
    const std::uint64_t depth = largestCompactionCandidateDepth();
    if (depth >= activeHostPathStateBytesPerDepth.size() ||
        depth >= retainedHostPathStateBytesPerDepth.size() ||
        activeHostPathStateBytesPerDepth[depth] <= retainedHostPathStateBytesPerDepth[depth]) {
      return 0;
    }
    return activeHostPathStateBytesPerDepth[depth] - retainedHostPathStateBytesPerDepth[depth];
  }

  double IntegratorBatchMetrics::largestCompactionCandidateSampleFraction() const {
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

  void IntegratorBatchMetrics::recordDirectLightSelectionHostBytes(std::uint64_t depth,
                                                                   std::uint64_t bytes) {
    if (directLightSelectionHostBytesPerDepth.size() <= depth) {
      directLightSelectionHostBytesPerDepth.resize(depth + 1);
    }
    directLightSelectionHostBytesPerDepth[depth] += bytes;
    directLightSelectionHostBytes += bytes;
  }

  void IntegratorBatchMetrics::recordDirectLightContributionHostBytes(std::uint64_t depth,
                                                                      std::uint64_t bytes) {
    if (directLightContributionHostBytesPerDepth.size() <= depth) {
      directLightContributionHostBytesPerDepth.resize(depth + 1);
    }
    directLightContributionHostBytesPerDepth[depth] += bytes;
    directLightContributionHostBytes += bytes;
  }

  void IntegratorBatchMetrics::recordDirectLightContributionExecution(std::string executionPath,
                                                                      std::string fallbackReason) {
    mergeLabel(directLightContributionExecutionPath,
               executionPath.empty() ? std::string("unknown") : executionPath);
    mergeLabel(directLightContributionFallbackReason, fallbackReason);
  }

  void IntegratorBatchMetrics::recordDirectLightOcclusionHostBytes(std::uint64_t depth,
                                                                   std::uint64_t bytes) {
    if (directLightOcclusionHostBytesPerDepth.size() <= depth) {
      directLightOcclusionHostBytesPerDepth.resize(depth + 1);
    }
    directLightOcclusionHostBytesPerDepth[depth] += bytes;
    directLightOcclusionHostBytes += bytes;
  }

  void IntegratorBatchMetrics::recordDirectLightAnyHitBatch(
    std::uint64_t depth, std::uint64_t batchChunks, std::uint64_t batchRays,
    std::uint64_t packedRayBytes, std::uint64_t hostQueryBytes, std::uint64_t stateHandleBytes) {
    if (directLightAnyHitBatchChunksPerDepth.size() <= depth) {
      directLightAnyHitBatchChunksPerDepth.resize(depth + 1);
    }
    if (directLightAnyHitBatchRaysPerDepth.size() <= depth) {
      directLightAnyHitBatchRaysPerDepth.resize(depth + 1);
    }
    if (directLightAnyHitFrontierPackedRayBytesPerDepth.size() <= depth) {
      directLightAnyHitFrontierPackedRayBytesPerDepth.resize(depth + 1);
    }
    if (directLightAnyHitFrontierHostQueryBytesPerDepth.size() <= depth) {
      directLightAnyHitFrontierHostQueryBytesPerDepth.resize(depth + 1);
    }
    if (directLightAnyHitFrontierStateHandleBytesPerDepth.size() <= depth) {
      directLightAnyHitFrontierStateHandleBytesPerDepth.resize(depth + 1);
    }
    directLightAnyHitBatchChunksPerDepth[depth] += batchChunks;
    directLightAnyHitBatchRaysPerDepth[depth] += batchRays;
    directLightAnyHitFrontierPackedRayBytesPerDepth[depth] += packedRayBytes;
    directLightAnyHitFrontierHostQueryBytesPerDepth[depth] += hostQueryBytes;
    directLightAnyHitFrontierStateHandleBytesPerDepth[depth] += stateHandleBytes;
    directLightAnyHitFrontierPackedRayBytes += packedRayBytes;
    directLightAnyHitFrontierHostQueryBytes += hostQueryBytes;
    directLightAnyHitFrontierStateHandleBytes += stateHandleBytes;
  }

  bool IntegratorBatchMetrics::hasMixedQueryDepth(std::size_t depth) const {
    const std::uint64_t closestHitChunks = depth < frontierClosestHitBatchChunksPerDepth.size()
                                             ? frontierClosestHitBatchChunksPerDepth[depth]
                                             : 0;
    const std::uint64_t anyHitChunks = depth < directLightAnyHitBatchChunksPerDepth.size()
                                         ? directLightAnyHitBatchChunksPerDepth[depth]
                                         : 0;
    return closestHitChunks > 0 && anyHitChunks > 0;
  }

  std::uint64_t IntegratorBatchMetrics::frontierQueryRoundTrips() const {
    std::uint64_t roundTrips = 0;
    for (const std::uint64_t chunks : frontierClosestHitBatchChunksPerDepth) {
      roundTrips += chunks;
    }
    for (const std::uint64_t chunks : directLightAnyHitBatchChunksPerDepth) {
      roundTrips += chunks;
    }
    return roundTrips;
  }

  std::uint64_t IntegratorBatchMetrics::residentFrontierQueryRoundTripsEstimate() const {
    return frontierQueryRoundTrips() - residentFrontierQueryRoundTripSavingsEstimate();
  }

  std::uint64_t IntegratorBatchMetrics::residentFrontierQueryRoundTripSavingsEstimate() const {
    const std::uint64_t mixedRoundTrips = mixedQueryDepthRoundTrips();
    const std::uint64_t mixedResidentBoundaries = mixedQueryDepthCount();
    if (mixedRoundTrips <= mixedResidentBoundaries) {
      return 0;
    }
    return mixedRoundTrips - mixedResidentBoundaries;
  }

  std::uint64_t IntegratorBatchMetrics::directLightAnyHitQueryRoundTrips() const {
    std::uint64_t roundTrips = 0;
    for (const std::uint64_t chunks : directLightAnyHitBatchChunksPerDepth) {
      roundTrips += chunks;
    }
    return roundTrips;
  }

  std::uint64_t IntegratorBatchMetrics::residentDirectLightBatchRoundTripsEstimate() const {
    return directLightAnyHitQueryRoundTrips() - residentDirectLightBatchRoundTripSavingsEstimate();
  }

  std::uint64_t IntegratorBatchMetrics::residentDirectLightBatchRoundTripSavingsEstimate() const {
    return directLightAnyHitQueryRoundTrips();
  }

  std::uint64_t IntegratorBatchMetrics::mixedQueryDepthCount() const {
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

  std::uint64_t IntegratorBatchMetrics::mixedQueryDepthRoundTrips() const {
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

  std::uint64_t IntegratorBatchMetrics::mixedQueryDepthRays() const {
    return mixedQueryDepthClosestHitRays() + mixedQueryDepthAnyHitRays();
  }

  std::uint64_t IntegratorBatchMetrics::mixedQueryDepthClosestHitRays() const {
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

  std::uint64_t IntegratorBatchMetrics::mixedQueryDepthAnyHitRays() const {
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

  std::uint64_t IntegratorBatchMetrics::mixedQueryDepthReadbackBytes() const {
    return mixedQueryDepthClosestHitReadbackBytes() + mixedQueryDepthAnyHitReadbackBytes();
  }

  std::uint64_t IntegratorBatchMetrics::mixedQueryDepthClosestHitReadbackBytes() const {
    return mixedQueryDepthClosestHitRays() * sizeof(GpuIntersectionHitRecord);
  }

  std::uint64_t IntegratorBatchMetrics::mixedQueryDepthAnyHitReadbackBytes() const {
    return mixedQueryDepthAnyHitRays() * sizeof(GpuIntersectionOcclusionRecord);
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
    intersectionBackendSupportsResidentFrontiers =
      intersectionBackendSupportsResidentFrontiers || backend.supportsResidentFrontiers();
    intersectionBackendSupportsGpuFrontierCompaction =
      intersectionBackendSupportsGpuFrontierCompaction || backend.supportsGpuFrontierCompaction();
    mergeLabel(intersectionBackendGpuFrontierCompactionUnavailableReason,
               nonEmptyLabel(backend.gpuFrontierCompactionUnavailableReason(), ""));
    intersectionBackendSupportsPreparedRayBatchCompaction =
      intersectionBackendSupportsPreparedRayBatchCompaction ||
      backend.supportsPreparedRayBatchCompaction();
    intersectionBackendSupportsResidentDirectLightBatches =
      intersectionBackendSupportsResidentDirectLightBatches ||
      backend.supportsResidentDirectLightBatches();
    mergeLabel(intersectionBackendResidentDirectLightBatchesUnavailableReason,
               nonEmptyLabel(backend.residentDirectLightBatchesUnavailableReason(), ""));
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

  void IntegratorBatchMetrics::recordTracingScene(const Scene& scene,
                                                  const WavefrontIntersectionBackend& backend) {
    const WavefrontIntersectionSceneDiagnostics intersectionDiagnostics =
      backend.compiledSceneDiagnostics();
    if (!intersectionDiagnostics.compiled) {
      return;
    }

    const GpuTracingSceneDiagnostics diagnostics =
      backend.compiledScene() ? compileGpuTracingSceneDiagnostics(*backend.compiledScene(), scene)
                              : compileGpuTracingSceneDiagnostics(scene);
    tracingSceneCompiled = tracingSceneCompiled || diagnostics.compiled;
    tracingSceneMaterials = std::max(tracingSceneMaterials, diagnostics.materials);
    tracingSceneTextures = std::max(tracingSceneTextures, diagnostics.textures);
    tracingSceneLights = std::max(tracingSceneLights, diagnostics.lights);
    tracingSceneEnvironment = std::max(tracingSceneEnvironment, diagnostics.environment);
    tracingSceneDebugIds = std::max(tracingSceneDebugIds, diagnostics.debugIds);
    tracingSceneUnsupportedMaterials =
      std::max(tracingSceneUnsupportedMaterials, diagnostics.unsupportedMaterials);
    tracingSceneUnsupportedTextures =
      std::max(tracingSceneUnsupportedTextures, diagnostics.unsupportedTextures);
    tracingSceneUnsupportedLights =
      std::max(tracingSceneUnsupportedLights, diagnostics.unsupportedLights);
    mergeMapMaximums(tracingSceneUnsupportedMaterialReasons,
                     diagnostics.unsupportedMaterialReasons);
    mergeMapMaximums(tracingSceneUnsupportedTextureReasons, diagnostics.unsupportedTextureReasons);
    mergeMapMaximums(tracingSceneUnsupportedLightReasons, diagnostics.unsupportedLightReasons);
    tracingSceneUploadBytes = std::max(tracingSceneUploadBytes, diagnostics.uploadBytes);
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

  void IntegratorBatchMetrics::recordClosestHitFrontierResidency(const std::string& residency,
                                                                 std::uint64_t packedRayBytes,
                                                                 std::uint64_t hostQueryBytes,
                                                                 std::uint64_t stateHandleBytes) {
    mergeLabel(intersectionBackendClosestHitFrontierResidency,
               residency.empty() ? "unknown" : residency);
    intersectionBackendClosestHitFrontierPackedRayBytes += packedRayBytes;
    intersectionBackendClosestHitFrontierHostQueryBytes += hostQueryBytes;
    intersectionBackendClosestHitFrontierStateHandleBytes += stateHandleBytes;
  }

  void IntegratorBatchMetrics::recordAnyHitFrontierResidency(const std::string& residency,
                                                             std::uint64_t packedRayBytes,
                                                             std::uint64_t hostQueryBytes,
                                                             std::uint64_t stateHandleBytes) {
    mergeLabel(intersectionBackendAnyHitFrontierResidency,
               residency.empty() ? "unknown" : residency);
    intersectionBackendAnyHitFrontierPackedRayBytes += packedRayBytes;
    intersectionBackendAnyHitFrontierHostQueryBytes += hostQueryBytes;
    intersectionBackendAnyHitFrontierStateHandleBytes += stateHandleBytes;
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
    intersectionEstimatedClosestHitRayUploadBytes += rayUploadBytes;
    intersectionEstimatedClosestHitReadbackBytes += readbackBytes;
    intersectionEstimatedClosestHitQueryTransferBytes += rayUploadBytes + readbackBytes;
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
    intersectionEstimatedAnyHitRayUploadBytes += rayUploadBytes;
    intersectionEstimatedAnyHitReadbackBytes += readbackBytes;
    intersectionEstimatedAnyHitQueryTransferBytes += rayUploadBytes + readbackBytes;
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
    mergeLabel(intersectionBackendClosestHitFrontierResidency,
               source.intersectionBackendClosestHitFrontierResidency);
    mergeLabel(intersectionBackendAnyHitFrontierResidency,
               source.intersectionBackendAnyHitFrontierResidency);
    intersectionBackendClosestHitFrontierPackedRayBytes +=
      source.intersectionBackendClosestHitFrontierPackedRayBytes;
    intersectionBackendAnyHitFrontierPackedRayBytes +=
      source.intersectionBackendAnyHitFrontierPackedRayBytes;
    intersectionBackendClosestHitFrontierHostQueryBytes +=
      source.intersectionBackendClosestHitFrontierHostQueryBytes;
    intersectionBackendAnyHitFrontierHostQueryBytes +=
      source.intersectionBackendAnyHitFrontierHostQueryBytes;
    intersectionBackendClosestHitFrontierStateHandleBytes +=
      source.intersectionBackendClosestHitFrontierStateHandleBytes;
    intersectionBackendAnyHitFrontierStateHandleBytes +=
      source.intersectionBackendAnyHitFrontierStateHandleBytes;
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
    tracingSceneCompiled = tracingSceneCompiled || source.tracingSceneCompiled;
    tracingSceneMaterials = std::max(tracingSceneMaterials, source.tracingSceneMaterials);
    tracingSceneTextures = std::max(tracingSceneTextures, source.tracingSceneTextures);
    tracingSceneLights = std::max(tracingSceneLights, source.tracingSceneLights);
    tracingSceneEnvironment = std::max(tracingSceneEnvironment, source.tracingSceneEnvironment);
    tracingSceneDebugIds = std::max(tracingSceneDebugIds, source.tracingSceneDebugIds);
    tracingSceneUnsupportedMaterials =
      std::max(tracingSceneUnsupportedMaterials, source.tracingSceneUnsupportedMaterials);
    tracingSceneUnsupportedTextures =
      std::max(tracingSceneUnsupportedTextures, source.tracingSceneUnsupportedTextures);
    tracingSceneUnsupportedLights =
      std::max(tracingSceneUnsupportedLights, source.tracingSceneUnsupportedLights);
    mergeMapMaximums(tracingSceneUnsupportedMaterialReasons,
                     source.tracingSceneUnsupportedMaterialReasons);
    mergeMapMaximums(tracingSceneUnsupportedTextureReasons,
                     source.tracingSceneUnsupportedTextureReasons);
    mergeMapMaximums(tracingSceneUnsupportedLightReasons,
                     source.tracingSceneUnsupportedLightReasons);
    tracingSceneUploadBytes = std::max(tracingSceneUploadBytes, source.tracingSceneUploadBytes);
    intersectionEstimatedRayUploadBytes += source.intersectionEstimatedRayUploadBytes;
    intersectionEstimatedClosestHitRayUploadBytes +=
      source.intersectionEstimatedClosestHitRayUploadBytes;
    intersectionEstimatedAnyHitRayUploadBytes += source.intersectionEstimatedAnyHitRayUploadBytes;
    intersectionEstimatedClosestHitReadbackBytes +=
      source.intersectionEstimatedClosestHitReadbackBytes;
    intersectionEstimatedAnyHitReadbackBytes += source.intersectionEstimatedAnyHitReadbackBytes;
    intersectionEstimatedQueryTransferBytes += source.intersectionEstimatedQueryTransferBytes;
    intersectionEstimatedClosestHitQueryTransferBytes +=
      source.intersectionEstimatedClosestHitQueryTransferBytes;
    intersectionEstimatedAnyHitQueryTransferBytes +=
      source.intersectionEstimatedAnyHitQueryTransferBytes;
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
    intersectionBackendSupportsResidentFrontiers =
      intersectionBackendSupportsResidentFrontiers ||
      source.intersectionBackendSupportsResidentFrontiers;
    intersectionBackendSupportsGpuFrontierCompaction =
      intersectionBackendSupportsGpuFrontierCompaction ||
      source.intersectionBackendSupportsGpuFrontierCompaction;
    mergeLabel(intersectionBackendGpuFrontierCompactionUnavailableReason,
               source.intersectionBackendGpuFrontierCompactionUnavailableReason);
    intersectionBackendSupportsPreparedRayBatchCompaction =
      intersectionBackendSupportsPreparedRayBatchCompaction ||
      source.intersectionBackendSupportsPreparedRayBatchCompaction;
    intersectionBackendSupportsResidentDirectLightBatches =
      intersectionBackendSupportsResidentDirectLightBatches ||
      source.intersectionBackendSupportsResidentDirectLightBatches;
    mergeLabel(intersectionBackendResidentDirectLightBatchesUnavailableReason,
               source.intersectionBackendResidentDirectLightBatchesUnavailableReason);
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

  bool IntegratorBatchSettings::publishDepthProgressAndCheckConvergence(
    const IntegratorBatchDepthProgress& progress, IntegratorBatchMetrics* metrics) const {
    IntegratorBatchFeedback feedback;
    if (progressObserver && progress.sampleColors) {
      core::util::ScopedTimer timer(metrics ? &metrics->progressSnapshotWorkerSeconds : nullptr);
      feedback = progressObserver->depthCompleted(progress.completedDepth, *progress.sampleColors,
                                                  progress.retainedActiveSamples);
    }

    if (!convergenceEnabled || progress.totalSamples == 0) {
      return false;
    }

    core::util::ScopedTimer timer(metrics ? &metrics->convergenceTestWorkerSeconds : nullptr);
    const double activeFraction = static_cast<double>(progress.retainedActiveSamples) /
                                  static_cast<double>(progress.totalSamples);
    const double rawRadianceDeltaRms =
      progress.activeSamplesAtDepth == 0
        ? 0.0
        : std::sqrt(progress.radianceDeltaSquaredSum /
                    static_cast<double>(progress.activeSamplesAtDepth));
    const double radianceDeltaRms =
      feedback.convergenceRadianceDeltaRms.value_or(rawRadianceDeltaRms);
    if (metrics && feedback.convergenceRadianceDeltaRms) {
      ++metrics->observerConvergenceFeedbackDepths;
    }
    if (activeFraction <= activeSampleFractionThreshold &&
        radianceDeltaRms <= radianceDeltaRmsThreshold) {
      if (metrics) {
        metrics->stoppedByConvergence = true;
        metrics->stoppedAfterDepth = metrics->activeSamplesPerDepth.size();
      }
      return true;
    }
    return false;
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
