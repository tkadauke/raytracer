#pragma once

#include "core/math/Rect.h"
#include "render/GpuFloat4.h"

#include <cstdint>
#include <optional>

namespace render {
  inline constexpr std::uint32_t gpuPrimaryPathGenerationModeHostPathStates = 0u;
  inline constexpr std::uint32_t gpuPrimaryPathGenerationModePinhole = 1u;
  inline constexpr std::uint32_t gpuPrimaryPathGenerationModeOrthographic = 2u;
  inline constexpr std::uint32_t gpuPrimaryPathGenerationModeThinLens = 3u;
  inline constexpr std::uint32_t gpuPrimaryPathGenerationModeEquirectangular = 4u;
  inline constexpr std::uint32_t gpuPrimaryPathGenerationModeSpherical = 5u;
  inline constexpr std::uint32_t gpuPrimaryPathGenerationModeFishEye = 6u;
  inline constexpr std::uint32_t gpuPrimaryPathGenerationModeTiltShift = 7u;

  inline constexpr std::uint32_t gpuPrimaryPathMotionModeOriginDelta = 0u;
  inline constexpr std::uint32_t gpuPrimaryPathMotionModeLookAt = 1u;

  struct alignas(16) GpuRectilinearPrimaryPathDescriptor {
    GpuFloat4 originOrDirection{};
    GpuFloat4 motionOriginDelta{};
    GpuFloat4 motionTarget{};
    GpuFloat4 motionTargetDelta{};
    GpuFloat4 motionParameters{};
    GpuFloat4 topLeft{};
    GpuFloat4 right{};
    GpuFloat4 down{};
    GpuFloat4 lensRight{};
    GpuFloat4 lensUp{};
    GpuFloat4 forward{};
    GpuFloat4 lensParameters{};
    std::int32_t requestedLeft{0};
    std::int32_t requestedTop{0};
    std::uint32_t requestedWidth{0};
    std::uint32_t requestedHeight{0};
    std::int32_t actualLeft{0};
    std::int32_t actualTop{0};
    std::uint32_t actualWidth{0};
    std::uint32_t actualHeight{0};
    std::uint32_t sampleOffset{0};
    std::uint32_t samplesPerPixel{0};
    std::uint32_t sampleSeed{0};
    std::uint32_t motionMode{gpuPrimaryPathMotionModeOriginDelta};
  };

  struct GpuPrimaryPathDescriptor {
    std::uint32_t mode{gpuPrimaryPathGenerationModeHostPathStates};
    GpuRectilinearPrimaryPathDescriptor rectilinear;

    [[nodiscard]] bool generatesOnDevice() const;
    [[nodiscard]] Recti requestedRect() const;
    [[nodiscard]] Recti actualRect() const;
    [[nodiscard]] std::uint64_t pathCount() const;
    [[nodiscard]] GpuPrimaryPathDescriptor withSampleRange(std::uint32_t firstSample,
                                                           std::uint32_t sampleCount) const;
    [[nodiscard]] GpuPrimaryPathDescriptor withActualRect(const Recti& rect) const;
  };
}
