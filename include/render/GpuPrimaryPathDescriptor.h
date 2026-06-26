#pragma once

#include "core/math/Rect.h"

#include <array>
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

  struct alignas(16) GpuRectilinearPrimaryPathDescriptor {
    std::array<float, 4> originOrDirection{};
    std::array<float, 4> topLeft{};
    std::array<float, 4> right{};
    std::array<float, 4> down{};
    std::array<float, 4> lensRight{};
    std::array<float, 4> lensUp{};
    std::array<float, 4> forward{};
    std::array<float, 4> lensParameters{};
    std::int32_t requestedLeft{0};
    std::int32_t requestedTop{0};
    std::uint32_t requestedWidth{0};
    std::uint32_t requestedHeight{0};
    std::int32_t actualLeft{0};
    std::int32_t actualTop{0};
    std::uint32_t actualWidth{0};
    std::uint32_t actualHeight{0};
    std::uint32_t samplesPerPixel{0};
    std::uint32_t sampleSeed{0};
    std::array<std::uint32_t, 2> reserved{};
  };

  struct GpuPrimaryPathDescriptor {
    std::uint32_t mode{gpuPrimaryPathGenerationModeHostPathStates};
    GpuRectilinearPrimaryPathDescriptor rectilinear;

    [[nodiscard]] bool generatesOnDevice() const;
    [[nodiscard]] Recti requestedRect() const;
    [[nodiscard]] Recti actualRect() const;
    [[nodiscard]] std::uint64_t pathCount() const;
  };
}
