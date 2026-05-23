#pragma once

#include "RasterPipelineTypes.h"
#include "engine/raster/Rasterizer.h"

#include "core/Buffer.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace engine::raster::detail {

  // Deterministic subpixel sample pattern used by the software MSAA paths. The
  // public Rasterizer clamps requested sample counts to 1, 2, 4, or 8; this type
  // converts that count into fixed offsets that are reused every frame.
  struct MSAASamplePattern {
    explicit MSAASamplePattern(int sampleCount) {
      switch (sampleCount) {
      case 2:
        offsets[0] = {-0.25, -0.25};
        offsets[1] = {0.25, 0.25};
        count = 2;
        break;
      case 4:
        offsets[0] = {-0.125, -0.375};
        offsets[1] = {0.375, -0.125};
        offsets[2] = {-0.375, 0.125};
        offsets[3] = {0.125, 0.375};
        count = 4;
        break;
      case 8:
        offsets[0] = {0.0625, -0.1875};
        offsets[1] = {-0.0625, 0.1875};
        offsets[2] = {0.3125, 0.0625};
        offsets[3] = {-0.1875, -0.3125};
        offsets[4] = {-0.3125, 0.3125};
        offsets[5] = {-0.4375, -0.0625};
        offsets[6] = {0.1875, 0.4375};
        offsets[7] = {0.4375, -0.4375};
        count = 8;
        break;
      default:
        offsets[0] = {0.0, 0.0};
        count = 1;
        break;
      }
    }

    std::array<Vector2d, 8> offsets{};
    int count{1};
  };

  inline void accumulateMSAASample(Buffer<Colord>& target, const Buffer<Colord>& sample);
  inline void resolveMSAATile(Buffer<Colord>& buffer, const Buffer<Colord>& accumulated,
                              const Recti& rect, int sampleCount);

  inline std::atomic<std::size_t>& msaaTileScratchAllocationCounter() {
    static std::atomic<std::size_t> counter{0};
    return counter;
  }

  inline void resetMSAATileScratchAllocationCount() {
    msaaTileScratchAllocationCounter().store(0, std::memory_order_release);
  }

  inline std::size_t msaaTileScratchAllocationCount() {
    return msaaTileScratchAllocationCounter().load(std::memory_order_acquire);
  }

  template<class T>
  inline bool msaaScratchBufferMatches(const std::unique_ptr<Buffer<T>>& buffer, int width,
                                       int height) {
    return buffer && buffer->width() == width && buffer->height() == height;
  }

  struct MSAAFragmentShadeKey {
    const RasterTriangle* triangle;
    int x;
    int y;

    bool operator==(const MSAAFragmentShadeKey& other) const {
      return triangle == other.triangle && x == other.x && y == other.y;
    }
  };

  struct MSAAFragmentShadeKeyHash {
    std::size_t operator()(const MSAAFragmentShadeKey& key) const {
      const auto triangleBits = reinterpret_cast<std::uintptr_t>(key.triangle);
      std::size_t hash = static_cast<std::size_t>(triangleBits >> 4);
      hash ^= static_cast<std::size_t>(key.x) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
      hash ^= static_cast<std::size_t>(key.y) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
      return hash;
    }
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

    inline RasterFragment shade(const RasterTriangle& triangle, int x, int y, double w0b,
                                double w1b, double w2b,
                                const InterpolatedFragment& fragment) const {
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
    void prepare(const Rasterizer& rasterizer, const Recti& rect) {
      m_rect = rect;
      if (!msaaScratchBufferMatches(m_accumulated, rect.width(), rect.height()) ||
          !msaaScratchBufferMatches(m_sampleColor, rect.width(), rect.height()) ||
          !msaaScratchBufferMatches(m_depth, rect.width(), rect.height())) {
        m_accumulated = std::make_unique<Buffer<Colord>>(rect.width(), rect.height());
        m_sampleColor = std::make_unique<Buffer<Colord>>(rect.width(), rect.height());
        m_depth = std::make_unique<Buffer<double>>(rect.width(), rect.height());
        msaaTileScratchAllocationCounter().fetch_add(1, std::memory_order_acq_rel);
      }
      m_accumulated->clear(Colord::black());

      if (rasterizer.stencilTestEnabled()) {
        if (!msaaScratchBufferMatches(m_stencil, rect.width(), rect.height())) {
          m_stencil = std::make_unique<Buffer<std::uint8_t>>(rect.width(), rect.height());
          msaaTileScratchAllocationCounter().fetch_add(1, std::memory_order_acq_rel);
        }
      } else {
        m_stencil.reset();
      }
    }

    void clearSample(const Rasterizer& rasterizer, const Buffer<Colord>* loadedColor = nullptr) {
      if (loadedColor) {
        for (int y = 0; y != m_rect.height(); ++y)
          for (int x = 0; x != m_rect.width(); ++x)
            (*m_sampleColor)[y][x] = (*loadedColor)[m_rect.top() + y][m_rect.left() + x];
      } else {
        m_sampleColor->clear(rasterizer.backgroundColor());
      }
      m_depth->clear(rasterizer.depthClearValue());
      if (m_stencil) {
        m_stencil->clear(rasterizer.stencilClearValue());
      }
    }

    Buffer<Colord>& sampleColor() {
      return *m_sampleColor;
    }

    Buffer<double>& depth() {
      return *m_depth;
    }

    Buffer<std::uint8_t>* stencil() {
      return m_stencil.get();
    }

    void accumulateSample() {
      accumulateMSAASample(*m_accumulated, *m_sampleColor);
    }

    void resolveTo(Buffer<Colord>& target, int sampleCount) const {
      resolveMSAATile(target, *m_accumulated, m_rect, sampleCount);
    }

  private:
    Recti m_rect;
    std::unique_ptr<Buffer<Colord>> m_accumulated;
    std::unique_ptr<Buffer<Colord>> m_sampleColor;
    std::unique_ptr<Buffer<double>> m_depth;
    std::unique_ptr<Buffer<std::uint8_t>> m_stencil;
  };

  // Add one rendered sample into an MSAA accumulation buffer of the same size.
  inline void accumulateMSAASample(Buffer<Colord>& target, const Buffer<Colord>& sample) {
    for (int y = 0; y < target.height(); ++y)
      for (int x = 0; x < target.width(); ++x)
        target[y][x] += sample[y][x];
  }

  // Resolve a full-frame accumulation buffer in place.
  inline void resolveMSAA(Buffer<Colord>& buffer, int sampleCount) {
    const double resolveScale = 1.0 / static_cast<double>(sampleCount);
    for (int y = 0; y < buffer.height(); ++y)
      for (int x = 0; x < buffer.width(); ++x)
        buffer[y][x] = buffer[y][x] * resolveScale;
  }

  // Resolve a tile-local accumulation buffer into its destination rectangle in
  // the final framebuffer.
  inline void resolveMSAATile(Buffer<Colord>& buffer, const Buffer<Colord>& accumulated,
                              const Recti& rect, int sampleCount) {
    const double resolveScale = 1.0 / static_cast<double>(sampleCount);
    for (int y = 0; y < accumulated.height(); ++y)
      for (int x = 0; x < accumulated.width(); ++x)
        buffer[rect.top() + y][rect.left() + x] = accumulated[y][x] * resolveScale;
  }

}
