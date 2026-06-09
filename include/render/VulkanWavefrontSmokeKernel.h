#pragma once

#include <cstdint>
#include <vector>

namespace render {
  /**
    * @brief Vulkan platform probe for experimental wavefront-intersection work.
    *
    * This deliberately does not make the Vulkan backend render-capable. It
    * only validates that the enabled build can talk to a Vulkan loader and find
    * a physical device with a compute queue, so fallback diagnostics can
    * distinguish missing platform support from missing render-path kernels.
    */
  class VulkanWavefrontSmokeKernel {
  public:
    bool deviceAvailable() const;
    std::vector<std::uint32_t>
    runDummyHitMissKernel(const std::vector<std::uint32_t>& rayIds) const;

  private:
    bool probeDeviceAvailable() const;
  };
}
