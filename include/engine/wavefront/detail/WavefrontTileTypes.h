#pragma once

#include "core/Buffer.h"
#include "core/math/Rect.h"
#include "render/Integrator.h"

#include <cstddef>
#include <vector>

namespace engine::wavefront::detail {
  struct WavefrontTilePixel {
    Recti footprint;
    Colord color{Colord::black()};
  };

  struct WavefrontTileTraceResult {
    std::vector<WavefrontTilePixel> pixels;
    std::size_t sampleCount{0};
    render::IntegratorBatchMetrics batchMetrics;
  };

  struct WavefrontDenoiserFeatureSet {
    explicit WavefrontDenoiserFeatureSet(int width, int height);

    Buffer<Colord> albedo;
    Buffer<Vector3d> normal;
    Buffer<double> depth;
  };
}
