#pragma once

#include <cstdint>
#include <vector>

namespace render {
  struct GpuIntersectionHitRecord;
  struct GpuIntersectionOcclusionRecord;
  struct GpuIntersectionRay;
  struct GpuIntersectionSceneBuffers;

  /**
    * @brief Tiny Metal compute dispatch used to validate the experimental
    * wavefront-intersection platform plumbing.
    *
    * The dummy kernel stays outside the render path as platform plumbing. The
    * basic hit kernels are the first narrow render-path intersection kernels
    * and share the same packed ABI used by the CPU parity intersector.
    */
  class MetalWavefrontSmokeKernel {
  public:
    bool deviceAvailable() const;
    std::vector<std::uint32_t>
    runDummyHitMissKernel(const std::vector<std::uint32_t>& rayIds) const;
    std::vector<GpuIntersectionHitRecord>
    runBasicClosestHitKernel(const GpuIntersectionSceneBuffers& scene,
                             const std::vector<GpuIntersectionRay>& rays) const;
    std::vector<GpuIntersectionOcclusionRecord>
    runBasicAnyHitKernel(const GpuIntersectionSceneBuffers& scene,
                         const std::vector<GpuIntersectionRay>& rays) const;
  };
}
