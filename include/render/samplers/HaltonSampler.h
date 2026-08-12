#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "render/samplers/BuiltInSampler.h"

namespace render {
  /**
    * Low-discrepancy Halton sampler for path-tracing dimensions.
    *
    * The legacy set API returns the two-dimensional Halton sequence in bases
    * 2 and 3. Streamed dimensions use successive prime-base pairs plus a
    * deterministic per-pixel Cranley rotation, giving path-tracing BSDF,
    * light, and continuation dimensions less clumping than purely random
    * samples while preserving repeatability.
    */
  class HaltonSampler : public BuiltInSampler {
  public:
    Vector2d sampleForDimension(int sampleIndex, uint64_t pixelHash,
                                uint64_t dimension) const override;

  protected:
    std::vector<Vector2d> generateSet() override;

  private:
    std::uint32_t baseForDimension(std::uint64_t dimension, std::uint64_t axis) const;
    double radicalInverse(std::uint64_t index, std::uint32_t base) const;
    double rotationOffset(std::uint64_t pixelHash, std::uint64_t dimension,
                          std::uint64_t axis) const;
    double wrapUnitInterval(double value) const;
  };
}
