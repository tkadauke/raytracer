#pragma once

#include <cstdint>

#include "render/samplers/SampleStream.h"

namespace render {
  /**
    * Explicit GPU sample request addressed by the backend-neutral sampling
    * contract. All fields are 32-bit so CPU reference tests can mirror shader
    * code without depending on 64-bit GPU integer support.
    */
  struct GpuSampleCoordinate {
    std::uint32_t seed{0};
    std::uint32_t pixelIndex{0};
    std::uint32_t primarySampleIndex{0};
    std::uint32_t dimension{0};
    std::uint32_t component{0};
  };

  /**
    * CPU reference implementation of the first GPU tracing sample stream.
    *
    * The stream is stateless with respect to sampling quality: every value is
    * a pure function of `(seed, pixelIndex, primarySampleIndex, dimension,
    * component)`. Sequential `next*` calls only advance the local dimension
    * cursor so it can satisfy the `SampleStream` interface.
    */
  class GpuSampleStream : public SampleStream {
  public:
    GpuSampleStream(std::uint32_t seed, std::uint32_t pixelIndex, std::uint32_t primarySampleIndex);

    static std::uint32_t pcgHash32(std::uint32_t input) noexcept;
    static std::uint32_t hash(const GpuSampleCoordinate& coordinate) noexcept;
    static double sample1D(const GpuSampleCoordinate& coordinate) noexcept;
    static Vector2d sample2D(std::uint32_t seed, std::uint32_t pixelIndex,
                             std::uint32_t primarySampleIndex, std::uint32_t dimension) noexcept;

    Vector2d next2D() override;
    double next1D() override;
    PrimarySample primarySample() override;
    Vector2d sample2D(SampleDimension dimension, std::uint64_t index = 0) override;
    double sample1D(SampleDimension dimension, std::uint64_t index = 0) override;

  private:
    static std::uint32_t narrowDimension(std::uint64_t dimension) noexcept;

    std::uint32_t m_seed;
    std::uint32_t m_pixelIndex;
    std::uint32_t m_primarySampleIndex;
    std::uint32_t m_dim{0};
  };
}
