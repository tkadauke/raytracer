#pragma once

#include "render/samplers/SampleStream.h"

#include <array>
#include <cstdint>

namespace render {
  inline constexpr std::uint32_t gpuDirectLightWorkLayoutVersion = 2u;

  /**
    * One surface point that is eligible for GPU diffuse direct-light work.
    *
    * The record is intentionally separate from `GpuIntersectionHitRecord`:
    * intersection records describe a ray query result, while this record is the
    * compact shading input future CPU/GPU direct-light batches consume after
    * the integrator has decided the hit participates in direct lighting.
    */
  struct alignas(16) GpuDirectLightSurfaceRecord {
    std::uint32_t material{0};
    std::uint32_t object{0};
    std::uint32_t primitiveRecord{0};
    std::uint32_t pathIndex{0};
    std::array<float, 4> point{};
    std::array<float, 4> normal{};
    std::array<float, 4> uv{};
    std::array<float, 4> incomingDirection{};
    std::array<float, 4> throughput{};
  };

  /**
    * Deterministic stochastic state for one direct-light sample at one surface.
    *
    * `lightSelectionDimension` selects the 1D sample used to pick a light.
    * `lightSurfaceDimensionBase` is the first 2D light-surface dimension for
    * this bounce/sample pair; add `lightIndex` through
    * `gpuDirectLightSurfaceSampleDimension(...)` once a light has been chosen.
    */
  struct alignas(16) GpuDirectLightSampleStateRecord {
    std::uint32_t seed{0};
    std::uint32_t pixelIndex{0};
    std::uint32_t primarySampleIndex{0};
    std::uint32_t bounce{0};
    std::uint32_t directSampleIndex{0};
    std::uint32_t lightSelectionDimension{0};
    std::uint32_t lightSurfaceDimensionBase{0};
    std::uint32_t flags{0};
  };

  /**
    * Light-selection inputs paired with a surface and sample state.
    *
    * `lightBegin` and `lightCount` address a contiguous range in
    * `GpuTracingSceneSections::lights`. The selected light starts unset; later
    * CPU reference and GPU kernels can fill it without changing the work
    * record layout.
    */
  struct alignas(16) GpuDirectLightSelectionRecord {
    std::uint32_t lightBegin{0};
    std::uint32_t lightCount{0};
    std::uint32_t selectedLight{0};
    std::uint32_t flags{0};
  };

  /**
    * Visibility ray and light-sample data produced from a direct-light work
    * item before any-hit testing.
    *
    * The fields are enough for diffuse contribution evaluation after
    * visibility resolves: light radiance, sampling pdf, surface-to-light
    * direction, and distance are preserved alongside the shadow-ray interval.
    */
  struct alignas(16) GpuDirectLightVisibilityRecord {
    std::uint32_t workIndex{0};
    std::uint32_t lightIndex{0};
    std::uint32_t flags{0};
    std::uint32_t occluded{0};
    std::array<float, 4> rayOrigin{};
    std::array<float, 4> rayDirection{};
    std::array<float, 4> lightRadiance{};
    std::array<float, 4> lightSample{};
    float minDistance{0.0f};
    float maxDistance{0.0f};
    float lightPdf{0.0f};
    float selectionPdf{0.0f};
  };

  /**
    * Complete backend-neutral input for one GPU diffuse direct-light sample.
    */
  struct alignas(16) GpuDirectLightWorkRecord {
    GpuDirectLightSurfaceRecord surface;
    GpuDirectLightSampleStateRecord sample;
    GpuDirectLightSelectionRecord lightSelection;
  };

  constexpr std::uint64_t gpuDirectLightSelectionSampleDimension(std::uint64_t bounce,
                                                                 std::uint64_t directSampleIndex) {
    return sampleDimensionIndex(SampleDimension::LightSelection,
                                SampleStream::lightSelectionSampleIndex(bounce, directSampleIndex));
  }

  constexpr std::uint64_t gpuDirectLightSurfaceSampleDimension(std::uint64_t bounce,
                                                               std::uint64_t lightIndex,
                                                               std::uint64_t directSampleIndex) {
    return sampleDimensionIndex(SampleDimension::Light, SampleStream::lightSampleIndex(
                                                          bounce, lightIndex, directSampleIndex));
  }

  constexpr GpuDirectLightSampleStateRecord
  makeGpuDirectLightSampleState(std::uint32_t seed, std::uint32_t pixelIndex,
                                std::uint32_t primarySampleIndex, std::uint32_t bounce,
                                std::uint32_t directSampleIndex) {
    return GpuDirectLightSampleStateRecord{
      seed,
      pixelIndex,
      primarySampleIndex,
      bounce,
      directSampleIndex,
      static_cast<std::uint32_t>(gpuDirectLightSelectionSampleDimension(bounce, directSampleIndex)),
      static_cast<std::uint32_t>(
        gpuDirectLightSurfaceSampleDimension(bounce, 0u, directSampleIndex)),
      0u};
  }
}
