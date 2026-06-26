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
    std::vector<std::array<float, 4>> accumulationColorSums;
    std::vector<std::uint32_t> accumulationSampleCounts;
    std::string executionPath{"vulkan_diffuse_path_loop_all_miss"};
    std::string pathStateResidency{"vulkan_host_visible_diffuse_path_state"};
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
    runAllMissPathLoop(const GpuDiffusePathLoopLaunchPlan& plan,
                       const std::vector<GpuDiffusePathStateRecord>& initialPathStates) const;
  };
}
