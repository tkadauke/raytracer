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
    std::vector<GpuIntersectionHitRecord> closestHitRecords;
    std::vector<GpuDiffusePathStepRecord> stepRecords;
    std::vector<std::array<float, 4>> accumulationColorSums;
    std::vector<std::uint32_t> accumulationSampleCounts;
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
  };
}
