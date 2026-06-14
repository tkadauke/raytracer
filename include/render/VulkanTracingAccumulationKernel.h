#pragma once

#include "core/Color.h"
#include "render/TracingAccumulationLayout.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace render {
  struct VulkanTracingAccumulationResult {
    int width{0};
    int height{0};
    std::vector<Colord> colorSums;
    std::vector<std::uint32_t> sampleCounts;
    std::vector<Colord> secondMoments;
    std::vector<unsigned int> resolved;
    TracingAccumulationDiagnostics diagnostics;

    [[nodiscard]] std::size_t pixelIndex(int x, int y) const;
    [[nodiscard]] Colord colorSumAt(int x, int y) const;
    [[nodiscard]] std::uint32_t sampleCountAt(int x, int y) const;
    [[nodiscard]] Colord secondMomentAt(int x, int y) const;
    [[nodiscard]] unsigned int resolvedAt(int x, int y) const;
  };

  /**
    * Optional Vulkan clear/add/resolve kernels for tracing accumulation planes.
    *
    * This wrapper is deliberately scoped to synthetic full-frame sample colors:
    * it validates the accumulation ABI against the CPU reference without
    * broadening into a GPU path tracer or render-resource manager.
    */
  class VulkanTracingAccumulationKernel {
  public:
    [[nodiscard]] bool deviceAvailable() const;
    [[nodiscard]] std::string deviceUnavailableReason() const;
    [[nodiscard]] bool accumulationAvailable() const;
    [[nodiscard]] std::string accumulationUnavailableReason() const;

    [[nodiscard]] VulkanTracingAccumulationResult
    runClearAddResolve(const TracingAccumulationLayout& layout,
                       const std::vector<std::vector<Colord>>& sampleFrames) const;

  private:
    [[nodiscard]] std::string probeDeviceUnavailableReason() const;
    [[nodiscard]] std::string probeAccumulationUnavailableReason() const;
  };
}
