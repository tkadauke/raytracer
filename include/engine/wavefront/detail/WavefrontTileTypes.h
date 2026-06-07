#pragma once

#include "core/Buffer.h"
#include "core/math/Rect.h"
#include "render/Integrator.h"
#include "render/denoise/Denoiser.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace engine::wavefront::detail {
  struct WavefrontTilePixel {
    Recti footprint;
    Colord color{Colord::black()};
    double sampleRadianceStddev{0.0};

    int area() const {
      return std::max(0, footprint.width()) * std::max(0, footprint.height());
    }

    void writeSampleRadianceStddevTo(Buffer<double>& buffer) const;
  };

  struct WavefrontTileTraceResult {
    std::vector<WavefrontTilePixel> pixels;
    std::size_t sampleCount{0};
    std::uint64_t sampleVariancePixelArea{0};
    double sampleRadianceVarianceSum{0.0};
    double maxSampleRadianceStddev{0.0};
    render::IntegratorBatchMetrics batchMetrics;
    double sampleGenerationWorkerSeconds{0.0};
    double sampleStreamWorkerSeconds{0.0};
    double primaryRayWorkerSeconds{0.0};
    double sampleEnqueueWorkerSeconds{0.0};
    double integratorBatchWorkerSeconds{0.0};

    void recordSampleVariance(const std::vector<Colord>& sampleColors,
                              const std::vector<std::size_t>& samplePixelIndices);
    void writeSampleRadianceStddevTo(Buffer<double>& buffer) const;
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
