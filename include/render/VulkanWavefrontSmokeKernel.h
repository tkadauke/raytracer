#pragma once

#include "render/WavefrontIntersectionQueryTiming.h"

#include <cstdint>
#include <vector>

namespace render {
  struct GpuIntersectionHitRecord;
  struct GpuIntersectionOcclusionRecord;
  struct GpuIntersectionRay;
  struct GpuIntersectionSceneBuffers;

  struct VulkanWavefrontClosestHitKernelResult {
    std::vector<GpuIntersectionHitRecord> hits;
    WavefrontIntersectionQueryTiming timing;
  };

  struct VulkanWavefrontAnyHitKernelResult {
    std::vector<GpuIntersectionOcclusionRecord> records;
    WavefrontIntersectionQueryTiming timing;
  };

  /**
    * @brief Vulkan platform probe for experimental wavefront-intersection work.
    *
    * The dummy kernel validates platform plumbing. The triangle closest-hit
    * kernel is the first narrow Vulkan render-path kernel and shares the packed
    * ABI used by the CPU parity intersector.
    */
  class VulkanWavefrontSmokeKernel {
  public:
    bool deviceAvailable() const;
    bool renderPathAvailable() const;
    std::vector<std::uint32_t>
    runDummyHitMissKernel(const std::vector<std::uint32_t>& rayIds) const;
    std::vector<GpuIntersectionHitRecord>
    runTriangleClosestHitKernel(const GpuIntersectionSceneBuffers& scene,
                                const std::vector<GpuIntersectionRay>& rays) const;
    VulkanWavefrontClosestHitKernelResult
    runTimedTriangleClosestHitKernel(const GpuIntersectionSceneBuffers& scene,
                                     const std::vector<GpuIntersectionRay>& rays) const;
    std::vector<GpuIntersectionOcclusionRecord>
    runTriangleAnyHitKernel(const GpuIntersectionSceneBuffers& scene,
                            const std::vector<GpuIntersectionRay>& rays) const;
    VulkanWavefrontAnyHitKernelResult
    runTimedTriangleAnyHitKernel(const GpuIntersectionSceneBuffers& scene,
                                 const std::vector<GpuIntersectionRay>& rays) const;

  private:
    bool probeDeviceAvailable() const;
  };
}
