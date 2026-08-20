#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "core/math/Vector.h"
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

  protected:
    /**
      * Walks an `n x n` grid (`n = sqrt(numSamples())`) and emits one sample
      * per cell via `cellSample(x, y, n)`, which returns the in-cell offset
      * position in `[0, 1]²`. Shared by `JitteredSampler` (random in-cell
      * offset) and `RegularSampler` (fixed center offset); only the per-cell
      * offset differs between them.
      */
    std::vector<Vector2d> generateGridSet(
        const std::function<Vector2d(int x, int y, int n)>& cellSample) const;
  };
}
