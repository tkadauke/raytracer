#pragma once

#include "render/GpuDiffusePathStepReference.h"
#include "render/TracingAccumulationLayout.h"

#include <cstdint>
#include <vector>

namespace render {
  inline constexpr std::uint32_t gpuDiffusePathLoopLaunchLayoutVersion = 1u;

  struct alignas(16) GpuDiffusePathLoopLaunchParameters {
    std::uint32_t layoutVersion{gpuDiffusePathLoopLaunchLayoutVersion};
    std::uint32_t maxDepth{0};
    std::uint32_t russianRouletteDepth{0};
    std::uint32_t directLightSamples{0};
    std::uint32_t initialPathCount{0};
    std::uint32_t imageWidth{0};
    std::uint32_t imageHeight{0};
    std::uint32_t materialCount{0};
    std::uint32_t textureCount{0};
    std::uint32_t lightCount{0};
    std::uint32_t environmentCount{0};
    std::uint32_t debugIdCount{0};
    std::uint32_t bvhNodeCount{0};
    std::uint32_t primitiveCount{0};
    std::uint32_t transformCount{0};
    std::uint32_t reserved0{0};
  };

  struct GpuDiffusePathLoopLaunchBufferSizes {
    std::uint64_t sceneUploadBytes{0};
    std::uint64_t initialPathStateBytes{0};
    std::uint64_t activePathStateBytes{0};
    std::uint64_t nextPathStateBytes{0};
    std::uint64_t stepRecordBytes{0};
    std::uint64_t retainedIndexBytes{0};
    std::uint64_t accumulationBytes{0};
    std::uint64_t totalUploadBytes{0};
    std::uint64_t totalResidentBytes{0};
  };

  struct GpuDiffusePathLoopLaunchPlan {
    GpuDiffusePathLoopLaunchParameters parameters;
    GpuDiffusePathLoopLaunchBufferSizes buffers;
  };

  class GpuDiffusePathLoopLaunchPlanner {
  public:
    [[nodiscard]] GpuDiffusePathLoopLaunchPlan
    plan(const GpuTracingSceneSections& scene,
         const std::vector<GpuDiffusePathStateRecord>& initialPathStates,
         const TracingAccumulationLayout& accumulationLayout,
         const GpuDiffusePathLoopSettings& settings) const;
  };
}
