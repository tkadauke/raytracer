#pragma once

#include "render/GpuDiffusePathLoopLaunch.h"

#include <array>
#include <string>
#include <vector>

namespace render {
  struct MetalGpuDiffusePathLoopKernelResult {
    GpuDiffusePathLoopLaunchParameters echoedParameters;
    GpuDiffusePathLoopLaunchBufferSizes bufferSizes;
    std::vector<GpuDiffusePathStateRecord> copiedInitialPathStates;
    std::vector<GpuDiffusePathStateRecord> resolvedPathStates;
    std::vector<GpuDiffusePathStateRecord> nextPathStates;
    std::vector<GpuIntersectionHitRecord> closestHitRecords;
    std::vector<GpuDiffusePathStepRecord> stepRecords;
    std::vector<GpuDiffusePathDenoiserFeatureRecord> denoiserFeatureRecords;
    std::vector<std::uint32_t> retainedPathIndices;
    std::uint32_t retainedPathCount{0};
    std::vector<std::uint32_t> activePathCountsPerDepth;
    std::vector<std::array<float, 4>> accumulationColorSums;
    std::vector<std::uint32_t> accumulationSampleCounts;
    std::vector<unsigned int> resolvedDisplayPixels;
    std::string executionPath{"metal_diffuse_path_loop_launch_probe"};
    std::string pathStateResidency{"metal_shared_diffuse_path_state"};
    double uploadWorkerSeconds{0.0};
    double kernelWorkerSeconds{0.0};
    double readbackWorkerSeconds{0.0};
  };

  class MetalGpuDiffusePathLoopKernel {
  public:
    [[nodiscard]] bool deviceAvailable() const;
    [[nodiscard]] std::string deviceUnavailableReason() const;
    [[nodiscard]] bool launchPathAvailable() const;
    [[nodiscard]] std::string launchPathUnavailableReason() const;

    [[nodiscard]] MetalGpuDiffusePathLoopKernelResult
    runLaunchProbe(const GpuDiffusePathLoopLaunchPlan& plan,
                   const std::vector<GpuDiffusePathStateRecord>& initialPathStates) const;

    [[nodiscard]] MetalGpuDiffusePathLoopKernelResult
    runAllMissProbe(const GpuDiffusePathLoopLaunchPlan& plan,
                    const std::vector<GpuDiffusePathStateRecord>& initialPathStates) const;

    [[nodiscard]] MetalGpuDiffusePathLoopKernelResult
    runClosestHitProbe(const GpuDiffusePathLoopLaunchPlan& plan,
                       const std::vector<GpuDiffusePathStateRecord>& initialPathStates) const;

    [[nodiscard]] MetalGpuDiffusePathLoopKernelResult
    runMatteHitShadingProbe(const GpuDiffusePathLoopLaunchPlan& plan,
                            const std::vector<GpuDiffusePathStateRecord>& initialPathStates) const;

    [[nodiscard]] MetalGpuDiffusePathLoopKernelResult runMatteContinuationProbe(
      const GpuDiffusePathLoopLaunchPlan& plan,
      const std::vector<GpuDiffusePathStateRecord>& initialPathStates) const;

    [[nodiscard]] MetalGpuDiffusePathLoopKernelResult
    runMattePathLoop(const GpuDiffusePathLoopLaunchPlan& plan,
                     const std::vector<GpuDiffusePathStateRecord>& initialPathStates,
                     bool capturePlatformAccumulation = true,
                     bool captureResolvedDisplay = false) const;

    [[nodiscard]] MetalGpuDiffusePathLoopKernelResult
    runWavefrontPathLoop(const GpuDiffusePathLoopLaunchPlan& plan,
                         const std::vector<GpuDiffusePathStateRecord>& initialPathStates,
                         bool capturePlatformAccumulation = true,
                         bool captureResolvedDisplay = false) const;
  };
}
