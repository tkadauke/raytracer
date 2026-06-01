#pragma once

#include "core/Buffer.h"
#include "core/math/Rect.h"
#include "render/Integrator.h"
#include "render/denoise/Denoiser.h"

#include <cstddef>
#include <memory>
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
    WavefrontDenoiserFeatureSet(int width, int height,
                                render::DenoiserFeatureRequest requestedFeatures);

    const render::DenoiserFeatureRequest& requestedFeatures() const;
    render::DenoiserFeatureBuffers buffers() const;
    bool hasSourcePixel(int x, int y) const;
    void write(const Recti& footprint, const Colord& albedo, const Vector3d& normal, double depth);
    void copyPixelFrom(const WavefrontDenoiserFeatureSet& source, int targetX, int targetY,
                       int sourceX, int sourceY);

  private:
    render::DenoiserFeatureRequest m_requestedFeatures;
    int m_width{0};
    int m_height{0};
    std::unique_ptr<Buffer<Colord>> m_albedo;
    std::unique_ptr<Buffer<Vector3d>> m_normal;
    std::unique_ptr<Buffer<double>> m_depth;
  };
}
