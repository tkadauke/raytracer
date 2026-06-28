#pragma once

#include "render/GpuDiffusePathLoopLaunch.h"

#include <array>
#include <string>
#include <vector>

namespace render {
  struct VulkanGpuDiffusePathLoopKernelResult {
    GpuDiffusePathLoopLaunchParameters echoedParameters;
    GpuDiffusePathLoopLaunchBufferSizes bufferSizes;
    std::vector<GpuDiffusePathStateRecord> resolvedPathStates;
    std::vector<GpuDiffusePathStateRecord> nextPathStates;
    std::vector<GpuDiffusePathStepRecord> stepRecords;
    std::vector<GpuDiffusePathDenoiserFeatureRecord> denoiserFeatureRecords;
    std::vector<std::uint32_t> retainedPathIndices;
    std::uint32_t retainedPathCount{0};
    std::vector<std::uint32_t> activePathCountsPerDepth;
    std::vector<double> radianceDeltaSquaredSumPerDepth;
    std::vector<double> maxRadianceDeltaPerDepth;
    std::vector<std::array<float, 4>> accumulationColorSums;
    std::vector<std::uint32_t> accumulationSampleCounts;
    std::vector<unsigned int> resolvedDisplayPixels;
    std::string executionPath{"vulkan_diffuse_path_loop_wavefront"};
    std::string schedule{gpuDiffusePathLoopScheduleDepthFrontier};
    std::string pathStateResidency{"vulkan_host_visible_diffuse_path_state"};
    bool retainedFrontierDispatchesIndirect{false};
    bool sceneUploadCacheHit{false};
    std::uint64_t sceneUploadBytesWritten{0};
    double uploadWorkerSeconds{0.0};
    double kernelWorkerSeconds{0.0};
    double readbackWorkerSeconds{0.0};
  };

  class VulkanGpuDiffusePathLoopKernel {
  public:
    [[nodiscard]] bool deviceAvailable() const;
    [[nodiscard]] std::string deviceUnavailableReason() const;
    [[nodiscard]] bool launchPathAvailable() const;
    [[nodiscard]] std::string launchPathUnavailableReason() const;

    [[nodiscard]] VulkanGpuDiffusePathLoopKernelResult
    runWavefrontPathLoop(const GpuDiffusePathLoopLaunchPlan& plan,
                         const std::vector<GpuDiffusePathStateRecord>& initialPathStates,
                         bool capturePlatformAccumulation = true,
                         bool captureResolvedDisplay = false,
                         bool clearPlatformAccumulation = true) const;
  };
}
