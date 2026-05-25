#include "engine/raster/detail/RasterMSAA.h"

#include "core/util/BufferUtils.h"

#include <atomic>

namespace engine::raster::detail {
  namespace {
    std::atomic<std::size_t>& msaaTileScratchAllocationCounter() {
      static std::atomic<std::size_t> counter{0};
      return counter;
    }
  }

  MSAASamplePattern::MSAASamplePattern(int sampleCount) {
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

  void resetMSAATileScratchAllocationCount() {
    msaaTileScratchAllocationCounter().store(0, std::memory_order_release);
  }

  std::size_t msaaTileScratchAllocationCount() {
    return msaaTileScratchAllocationCounter().load(std::memory_order_acquire);
  }

  bool MSAAFragmentShadeKey::operator==(const MSAAFragmentShadeKey& other) const {
    return triangle == other.triangle && x == other.x && y == other.y;
  }

  std::size_t MSAAFragmentShadeKeyHash::operator()(const MSAAFragmentShadeKey& key) const {
    const auto triangleBits = reinterpret_cast<std::uintptr_t>(key.triangle);
    std::size_t hash = static_cast<std::size_t>(triangleBits >> 4);
    hash ^= static_cast<std::size_t>(key.x) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= static_cast<std::size_t>(key.y) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    return hash;
  }

  void MSAATileScratch::prepare(const Rasterizer& rasterizer, const Recti& rect) {
    m_rect = rect;
    if (!core::util::bufferDimensionsMatch(m_accumulated, rect.width(), rect.height()) ||
        !core::util::bufferDimensionsMatch(m_sampleColor, rect.width(), rect.height()) ||
        !core::util::bufferDimensionsMatch(m_depth, rect.width(), rect.height())) {
      m_accumulated = std::make_unique<Buffer<Colord>>(rect.width(), rect.height());
      m_sampleColor = std::make_unique<Buffer<Colord>>(rect.width(), rect.height());
      m_depth = std::make_unique<Buffer<double>>(rect.width(), rect.height());
      msaaTileScratchAllocationCounter().fetch_add(1, std::memory_order_acq_rel);
    }
    m_accumulated->clear(Colord::black());

    if (rasterizer.stencilTestEnabled()) {
      if (!core::util::bufferDimensionsMatch(m_stencil, rect.width(), rect.height())) {
        m_stencil = std::make_unique<Buffer<std::uint8_t>>(rect.width(), rect.height());
        msaaTileScratchAllocationCounter().fetch_add(1, std::memory_order_acq_rel);
      }
    } else {
      m_stencil.reset();
    }
  }

  void MSAATileScratch::clearSample(const Rasterizer& rasterizer,
                                    const Buffer<Colord>* loadedColor) {
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

  Buffer<Colord>& MSAATileScratch::sampleColor() {
    return *m_sampleColor;
  }

  Buffer<double>& MSAATileScratch::depth() {
    return *m_depth;
  }

  Buffer<std::uint8_t>* MSAATileScratch::stencil() {
    return m_stencil.get();
  }

  void MSAATileScratch::accumulateSample() {
    accumulateMSAASample(*m_accumulated, *m_sampleColor);
  }

  void MSAATileScratch::resolveTo(Buffer<Colord>& target, int sampleCount) const {
    resolveMSAATile(target, *m_accumulated, m_rect, sampleCount);
  }

  void accumulateMSAASample(Buffer<Colord>& target, const Buffer<Colord>& sample) {
    for (int y = 0; y < target.height(); ++y)
      for (int x = 0; x < target.width(); ++x)
        target[y][x] += sample[y][x];
  }

  void resolveMSAA(Buffer<Colord>& buffer, int sampleCount) {
    const double resolveScale = 1.0 / static_cast<double>(sampleCount);
    for (int y = 0; y < buffer.height(); ++y)
      for (int x = 0; x < buffer.width(); ++x)
        buffer[y][x] = buffer[y][x] * resolveScale;
  }

  void resolveMSAATile(Buffer<Colord>& buffer, const Buffer<Colord>& accumulated, const Recti& rect,
                       int sampleCount) {
    const double resolveScale = 1.0 / static_cast<double>(sampleCount);
    for (int y = 0; y < accumulated.height(); ++y)
      for (int x = 0; x < accumulated.width(); ++x)
        buffer[rect.top() + y][rect.left() + x] = accumulated[y][x] * resolveScale;
  }
}
