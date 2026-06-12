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

  struct MetalWavefrontClosestHitKernelResult {
    std::vector<GpuIntersectionHitRecord> hits;
    WavefrontIntersectionQueryTiming timing;
  };

  struct MetalWavefrontAnyHitKernelResult {
    std::vector<GpuIntersectionOcclusionRecord> records;
    WavefrontIntersectionQueryTiming timing;
  };

  class MetalWavefrontPreparedRayBatch {
  public:
    ~MetalWavefrontPreparedRayBatch();

    MetalWavefrontPreparedRayBatch(const MetalWavefrontPreparedRayBatch&) = delete;
    MetalWavefrontPreparedRayBatch& operator=(const MetalWavefrontPreparedRayBatch&) = delete;

    [[nodiscard]] std::uint64_t rayCount() const;
    [[nodiscard]] std::uint64_t packedRayBytes() const;

  private:
    friend class MetalWavefrontPreparedScene;

    MetalWavefrontPreparedRayBatch();

    struct Private;
    std::unique_ptr<Private> p;
  };

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
    std::string deviceUnavailableReason() const;
    bool renderPathAvailable() const;
    std::string renderPathUnavailableReason() const;
    std::vector<std::uint32_t>
    runDummyHitMissKernel(const std::vector<std::uint32_t>& rayIds) const;
    std::vector<GpuIntersectionHitRecord>
    runBasicClosestHitKernel(const GpuIntersectionSceneBuffers& scene,
                             const std::vector<GpuIntersectionRay>& rays) const;
    MetalWavefrontClosestHitKernelResult
    runTimedBasicClosestHitKernel(const GpuIntersectionSceneBuffers& scene,
                                  const std::vector<GpuIntersectionRay>& rays) const;
    std::vector<GpuIntersectionOcclusionRecord>
    runBasicAnyHitKernel(const GpuIntersectionSceneBuffers& scene,
                         const std::vector<GpuIntersectionRay>& rays) const;
    MetalWavefrontAnyHitKernelResult
    runTimedBasicAnyHitKernel(const GpuIntersectionSceneBuffers& scene,
                              const std::vector<GpuIntersectionRay>& rays) const;
  };

  class MetalWavefrontPreparedScene {
  public:
    explicit MetalWavefrontPreparedScene(const GpuIntersectionSceneBuffers& scene);
    ~MetalWavefrontPreparedScene();

    MetalWavefrontPreparedScene(const MetalWavefrontPreparedScene&) = delete;
    MetalWavefrontPreparedScene& operator=(const MetalWavefrontPreparedScene&) = delete;

    [[nodiscard]] std::shared_ptr<const MetalWavefrontPreparedRayBatch>
    prepareRays(const std::vector<GpuIntersectionRay>& rays) const;
    MetalWavefrontClosestHitKernelResult
    runTimedBasicClosestHitKernel(const std::vector<GpuIntersectionRay>& rays) const;
    MetalWavefrontClosestHitKernelResult
    runTimedBasicClosestHitKernel(const MetalWavefrontPreparedRayBatch& rays) const;
    MetalWavefrontAnyHitKernelResult
    runTimedBasicAnyHitKernel(const std::vector<GpuIntersectionRay>& rays) const;
    MetalWavefrontAnyHitKernelResult
    runTimedBasicAnyHitKernel(const MetalWavefrontPreparedRayBatch& rays) const;

  private:
    struct Private;
    std::unique_ptr<Private> p;
  };
}
