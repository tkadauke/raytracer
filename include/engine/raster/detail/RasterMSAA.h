#pragma once

#include "engine/raster/detail/RasterPipelineTypes.h"
#include "engine/raster/Rasterizer.h"

#include "core/Buffer.h"

#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace engine::raster::detail {

  // Deterministic subpixel sample pattern used by the software MSAA paths. The
  // public Rasterizer clamps requested sample counts to 1, 2, 4, or 8; this type
  // converts that count into fixed offsets that are reused every frame.
  struct MSAASamplePattern {
    explicit MSAASamplePattern(int sampleCount);

    std::array<Vector2d, 8> offsets{};
    int count{1};
  };

  void resetMSAATileScratchAllocationCount();
  std::size_t msaaTileScratchAllocationCount();

  struct MSAAFragmentShadeKey {
    const RasterTriangle* triangle;
    int x;
    int y;

    bool operator==(const MSAAFragmentShadeKey& other) const;
  };

  struct MSAAFragmentShadeKeyHash {
    std::size_t operator()(const MSAAFragmentShadeKey& key) const;
  };

  // Per-frame or per-tile cache for `MSAAShadingMode::PerFragment`. The
  // software MSAA pass still reruns coverage/depth/stencil for each sample,
  // but expensive material or fragment-shader color evaluation is reused for
  // the same prepared triangle and pixel after the first passing sample.
  class MSAAFragmentShadeCache {
  public:
    template<class Compute>
    RasterFragment shade(const RasterTriangle* triangle, int x, int y, Compute&& compute) {
      const MSAAFragmentShadeKey key{triangle, x, y};
      const auto found = m_fragments.find(key);
      if (found != m_fragments.end()) {
        return found->second;
      }

      const RasterFragment fragment = compute();
      m_fragments.emplace(key, fragment);
      return fragment;
    }

  private:
    std::unordered_map<MSAAFragmentShadeKey, RasterFragment, MSAAFragmentShadeKeyHash> m_fragments;
  };

  template<class FragmentPolicy>
  struct CachedMSAAFragmentPolicy {
    FragmentPolicy fragmentPolicy;
    MSAAFragmentShadeCache* cache;

    RasterFragment shade(const RasterTriangle& triangle, int x, int y, double w0b, double w1b,
                         double w2b, const InterpolatedFragment& fragment) const {
      return cache->shade(&triangle, x, y, [&]() {
        return fragmentPolicy.shade(triangle, x, y, w0b, w1b, w2b, fragment);
      });
    }
  };

  // Tile-local scratch storage for the queued MSAA path. One worker owns one of
  // these while it rerenders the tile at each sample offset, accumulates colors,
  // and resolves back into the final framebuffer.
  class MSAATileScratch {
  public:
    void prepare(const Rasterizer& rasterizer, const Recti& rect);

    void clearSample(const Rasterizer& rasterizer, const Buffer<Colord>* loadedColor = nullptr);

    Buffer<Colord>& sampleColor();

    Buffer<double>& depth();

    Buffer<std::uint8_t>* stencil();

    void accumulateSample();

    void resolveTo(Buffer<Colord>& target, int sampleCount) const;

  private:
    Recti m_rect;
    std::unique_ptr<Buffer<Colord>> m_accumulated;
    std::unique_ptr<Buffer<Colord>> m_sampleColor;
    std::unique_ptr<Buffer<double>> m_depth;
    std::unique_ptr<Buffer<std::uint8_t>> m_stencil;
  };

  // Add one rendered sample into an MSAA accumulation buffer of the same size.
  void accumulateMSAASample(Buffer<Colord>& target, const Buffer<Colord>& sample);

  // Resolve a full-frame accumulation buffer in place.
  void resolveMSAA(Buffer<Colord>& buffer, int sampleCount);

  // Resolve a tile-local accumulation buffer into its destination rectangle in
  // the final framebuffer.
  void resolveMSAATile(Buffer<Colord>& buffer, const Buffer<Colord>& accumulated, const Recti& rect,
                       int sampleCount);

}
