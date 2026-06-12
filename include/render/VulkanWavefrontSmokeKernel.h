#pragma once

#include "render/WavefrontIntersectionQueryTiming.h"

#include <cstdint>
#include <memory>
#include <string>
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

  class VulkanWavefrontPreparedRayBatch {
  public:
    ~VulkanWavefrontPreparedRayBatch();

    VulkanWavefrontPreparedRayBatch(const VulkanWavefrontPreparedRayBatch&) = delete;
    VulkanWavefrontPreparedRayBatch& operator=(const VulkanWavefrontPreparedRayBatch&) = delete;

    [[nodiscard]] std::uint64_t rayCount() const;
    [[nodiscard]] std::uint64_t packedRayBytes() const;

  private:
    friend class VulkanWavefrontPreparedScene;

    VulkanWavefrontPreparedRayBatch();

    struct Private;
    std::unique_ptr<Private> p;
  };

  /**
    * @brief Vulkan platform probe for experimental wavefront-intersection work.
    *
    * The dummy kernel validates platform plumbing. The basic hit kernels cover
    * the first narrow Vulkan render-path subset and share the packed ABI used by
    * the CPU parity intersector.
    */
  class VulkanWavefrontSmokeKernel {
  public:
    bool deviceAvailable() const;
    std::string deviceUnavailableReason() const;
    bool renderPathAvailable() const;
    std::string renderPathUnavailableReason() const;
    std::vector<std::uint32_t>
    runDummyHitMissKernel(const std::vector<std::uint32_t>& rayIds) const;
    std::vector<GpuIntersectionHitRecord>
    runBasicClosestHitKernel(const GpuIntersectionSceneBuffers& scene,
                             const std::vector<GpuIntersectionRay>& rays) const;
    VulkanWavefrontClosestHitKernelResult
    runTimedBasicClosestHitKernel(const GpuIntersectionSceneBuffers& scene,
                                  const std::vector<GpuIntersectionRay>& rays) const;
    std::vector<GpuIntersectionOcclusionRecord>
    runBasicAnyHitKernel(const GpuIntersectionSceneBuffers& scene,
                         const std::vector<GpuIntersectionRay>& rays) const;
    VulkanWavefrontAnyHitKernelResult
    runTimedBasicAnyHitKernel(const GpuIntersectionSceneBuffers& scene,
                              const std::vector<GpuIntersectionRay>& rays) const;

  private:
    std::string probeDeviceUnavailableReason() const;
    std::string probeRenderPathUnavailableReason() const;
  };

  class VulkanWavefrontPreparedScene {
  public:
    explicit VulkanWavefrontPreparedScene(const GpuIntersectionSceneBuffers& scene);
    ~VulkanWavefrontPreparedScene();

    VulkanWavefrontPreparedScene(const VulkanWavefrontPreparedScene&) = delete;
    VulkanWavefrontPreparedScene& operator=(const VulkanWavefrontPreparedScene&) = delete;

    [[nodiscard]] std::shared_ptr<const VulkanWavefrontPreparedRayBatch>
    prepareRays(const std::vector<GpuIntersectionRay>& rays) const;
    [[nodiscard]] std::shared_ptr<const VulkanWavefrontPreparedRayBatch>
    compactRays(const VulkanWavefrontPreparedRayBatch& sourceRays,
                const std::vector<std::uint32_t>& retainedRayIndices) const;
    VulkanWavefrontClosestHitKernelResult
    runTimedBasicClosestHitKernel(const std::vector<GpuIntersectionRay>& rays) const;
    VulkanWavefrontClosestHitKernelResult
    runTimedBasicClosestHitKernel(const VulkanWavefrontPreparedRayBatch& rays) const;
    VulkanWavefrontAnyHitKernelResult
    runTimedBasicAnyHitKernel(const std::vector<GpuIntersectionRay>& rays) const;
    VulkanWavefrontAnyHitKernelResult
    runTimedBasicAnyHitKernel(const VulkanWavefrontPreparedRayBatch& rays) const;

  private:
    struct Private;
    std::shared_ptr<Private> p;
  };
}
