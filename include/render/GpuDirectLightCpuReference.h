#pragma once

#include "render/GpuDirectLightWork.h"
#include "render/GpuTracingScene.h"

#include <array>
#include <cstdint>
#include <vector>

namespace render {
  class IntersectionService;

  inline constexpr std::uint32_t gpuDirectLightVisibilityValid = 1u << 0u;
  inline constexpr std::uint32_t gpuDirectLightVisibilityDeltaLight = 1u << 1u;
  inline constexpr std::uint32_t gpuDirectLightContributionValid = 1u << 0u;
  inline constexpr std::uint32_t gpuDirectLightContributionOccluded = 1u << 1u;
  inline constexpr std::uint32_t gpuDirectLightContributionContributing = 1u << 2u;

  struct alignas(16) GpuDirectLightContributionRecord {
    std::uint32_t workIndex{0};
    std::uint32_t lightIndex{0};
    std::uint32_t flags{0};
    std::uint32_t occluded{0};
    std::array<float, 4> contribution{};
  };

  struct GpuDirectLightCpuReferenceBatch {
    std::vector<GpuDirectLightVisibilityRecord> visibility;
    std::vector<GpuDirectLightContributionRecord> contributions;
  };

  [[nodiscard]] GpuDirectLightVisibilityRecord
  makeGpuDirectLightCpuVisibilityRecord(const GpuTracingSceneSections& scene,
                                        const GpuDirectLightWorkRecord& work,
                                        std::uint32_t workIndex = 0);

  [[nodiscard]] std::vector<GpuDirectLightVisibilityRecord>
  makeGpuDirectLightCpuVisibilityBatch(const GpuTracingSceneSections& scene,
                                       const std::vector<GpuDirectLightWorkRecord>& work);

  [[nodiscard]] std::vector<GpuDirectLightVisibilityRecord>
  resolveGpuDirectLightCpuVisibilityOcclusionBatch(
    IntersectionService& intersectionService,
    const std::vector<GpuDirectLightVisibilityRecord>& visibility);

  [[nodiscard]] GpuDirectLightContributionRecord
  makeGpuDirectLightCpuContributionRecord(const GpuTracingSceneSections& scene,
                                          const GpuDirectLightWorkRecord& work,
                                          const GpuDirectLightVisibilityRecord& visibility);

  [[nodiscard]] std::vector<GpuDirectLightContributionRecord>
  makeGpuDirectLightCpuContributionBatch(
    const GpuTracingSceneSections& scene, const std::vector<GpuDirectLightWorkRecord>& work,
    const std::vector<GpuDirectLightVisibilityRecord>& visibility);

  [[nodiscard]] GpuDirectLightCpuReferenceBatch
  makeGpuDirectLightCpuReferenceBatch(const GpuTracingSceneSections& scene,
                                      const std::vector<GpuDirectLightWorkRecord>& work);

  [[nodiscard]] GpuDirectLightCpuReferenceBatch
  makeGpuDirectLightCpuReferenceBatch(const GpuTracingSceneSections& scene,
                                      const std::vector<GpuDirectLightWorkRecord>& work,
                                      IntersectionService& intersectionService);
}
