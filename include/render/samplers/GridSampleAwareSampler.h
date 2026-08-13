#pragma once

#include <cstdint>

#include "render/samplers/BuiltInSampler.h"

namespace render {
  /**
    * @brief Shared `sampleForDimension` override for the grid-based built-in
    *        samplers (`JitteredSampler`, `RegularSampler`).
    *
    * Both samplers use the same scrambled, path-tracing-aware sample logic;
    * only their `generateSet` legacy-set generation differs.
    */
  class GridSampleAwareSampler : public BuiltInSampler {
  public:
    Vector2d sampleForDimension(int sampleIndex, uint64_t pixelHash,
                                uint64_t dimension) const override;
  };
}
