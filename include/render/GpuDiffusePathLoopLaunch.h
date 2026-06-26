#pragma once

#include "render/GpuDiffusePathStepReference.h"
#include "render/TracingAccumulationLayout.h"

#include <array>
#include <cstdint>
#include <vector>

namespace render {
  inline constexpr std::uint32_t gpuDiffusePathLoopLaunchLayoutVersion = 7u;

  struct alignas(16) GpuDiffusePathLoopLaunchParameters {
    std::uint32_t layoutVersion{gpuDiffusePathLoopLaunchLayoutVersion};
    std::uint32_t maxDepth{0};
    std::uint32_t russianRouletteDepth{0};
    std::uint32_t directLightSamples{0};
    std::uint32_t captureDiagnostics{1};
    std::uint32_t initialPathCount{0};
    std::uint32_t imageWidth{0};
    std::uint32_t imageHeight{0};
    std::uint32_t materialCount{0};
    std::uint32_t textureCount{0};
    std::uint32_t lightCount{0};
    std::uint32_t environmentCount{0};
    std::uint32_t debugIdCount{0};
    std::uint32_t geometryByteOffset{0};
    std::uint32_t materialByteOffset{0};
    std::uint32_t textureByteOffset{0};
    std::uint32_t lightByteOffset{0};
    std::uint32_t environmentByteOffset{0};
    std::uint32_t debugIdByteOffset{0};
    std::uint32_t sceneUploadBytes{0};
    std::uint32_t accumulationTargetMode{gpuDiffusePathLoopAccumulationTargetPixel};
    std::uint32_t bvhByteOffset{0};
    std::uint32_t primitiveByteOffset{0};
    std::uint32_t triangleByteOffset{0};
    std::uint32_t sphereByteOffset{0};
    std::uint32_t planeByteOffset{0};
    std::uint32_t rectangleByteOffset{0};
    std::uint32_t diskByteOffset{0};
    std::uint32_t openCylinderByteOffset{0};
    std::uint32_t torusByteOffset{0};
    std::uint32_t transformByteOffset{0};
    std::uint32_t bvhNodeCount{0};
    std::uint32_t primitiveCount{0};
    std::uint32_t triangleCount{0};
    std::uint32_t sphereCount{0};
    std::uint32_t planeCount{0};
    std::uint32_t rectangleCount{0};
    std::uint32_t diskCount{0};
    std::uint32_t openCylinderCount{0};
    std::uint32_t torusCount{0};
    std::uint32_t transformCount{0};
    std::uint32_t primaryPathGenerationMode{gpuPrimaryPathGenerationModeHostPathStates};
    std::uint32_t primaryPathSamplesPerPixel{0};
    std::uint32_t primaryPathSampleSeed{0};
    std::uint32_t primaryPathRequestedWidth{0};
    std::int32_t primaryPathRequestedLeft{0};
    std::int32_t primaryPathRequestedTop{0};
    std::uint32_t primaryPathRequestedHeight{0};
    std::uint32_t primaryPathActualWidth{0};
    std::int32_t primaryPathActualLeft{0};
    std::int32_t primaryPathActualTop{0};
    std::uint32_t primaryPathActualHeight{0};
    std::uint32_t reserved0{0};
    std::uint32_t reserved1{0};
    std::uint32_t reserved2{0};
    std::uint32_t reserved3{0};
    std::array<float, 4> primaryPathOrigin{};
    std::array<float, 4> primaryPathTopLeft{};
    std::array<float, 4> primaryPathRight{};
    std::array<float, 4> primaryPathDown{};
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
    std::vector<std::uint8_t> sceneUpload;

    [[nodiscard]] bool generatesPrimaryPathsOnDevice() const;
  };

  class GpuDiffusePathLoopLaunchPlanner {
  public:
    [[nodiscard]] GpuDiffusePathLoopLaunchPlan
    plan(const GpuTracingSceneSections& scene,
         const std::vector<GpuDiffusePathStateRecord>& initialPathStates,
         const TracingAccumulationLayout& accumulationLayout,
         const GpuDiffusePathLoopSettings& settings) const;
    [[nodiscard]] GpuDiffusePathLoopLaunchPlan
    plan(const GpuTracingSceneSections& scene,
         const GpuDiffusePrimaryPathStateGeneration& primaryPathGeneration,
         const TracingAccumulationLayout& accumulationLayout,
         const GpuDiffusePathLoopSettings& settings) const;
  };
}
