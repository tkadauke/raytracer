#include "render/TracingAccumulationReference.h"

#include "render/tonemap/Tonemap.h"

#include <limits>
#include <stdexcept>
#include <string>

namespace render {
  namespace {
    TracingAccumulationLayout validatedLayout(TracingAccumulationLayout layout) {
      layout.validate();
      return layout;
    }

    Colord squared(const Colord& color) {
      return Colord(color.r() * color.r(), color.g() * color.g(), color.b() * color.b());
    }
  }

  TracingAccumulationBuffer::TracingAccumulationBuffer(const TracingAccumulationLayout& layout)
      : m_layout(validatedLayout(layout)),
        m_colorSum(m_layout.width, m_layout.height),
        m_sampleCount(m_layout.width, m_layout.height) {
    if (m_layout.hasMomentBuffer()) {
      m_secondMoment = std::make_unique<Buffer<Colord>>(m_layout.width, m_layout.height);
    }
    clear();
  }

  TracingAccumulationBuffer::TracingAccumulationBuffer(int width, int height)
      : TracingAccumulationBuffer(TracingAccumulationLayout::image(width, height)) {
  }

  TracingAccumulationBuffer::~TracingAccumulationBuffer() = default;

  const TracingAccumulationLayout& TracingAccumulationBuffer::layout() const {
    return m_layout;
  }

  Buffer<Colord>& TracingAccumulationBuffer::colorSum() {
    return m_colorSum;
  }

  const Buffer<Colord>& TracingAccumulationBuffer::colorSum() const {
    return m_colorSum;
  }

  Buffer<std::uint32_t>& TracingAccumulationBuffer::sampleCount() {
    return m_sampleCount;
  }

  const Buffer<std::uint32_t>& TracingAccumulationBuffer::sampleCount() const {
    return m_sampleCount;
  }

  bool TracingAccumulationBuffer::hasSecondMoment() const {
    return m_secondMoment != nullptr;
  }

  Buffer<Colord>* TracingAccumulationBuffer::secondMoment() {
    return m_secondMoment.get();
  }

  const Buffer<Colord>* TracingAccumulationBuffer::secondMoment() const {
    return m_secondMoment.get();
  }

  void TracingAccumulationBuffer::clear() {
    m_colorSum.clear(Colord::black());
    m_sampleCount.clear(0u);
    if (m_secondMoment) {
      m_secondMoment->clear(Colord::black());
    }
  }

  void TracingAccumulationBuffer::addSample(int x, int y, const Colord& color) {
    validatePixel(x, y);
    if (m_sampleCount[y][x] == std::numeric_limits<std::uint32_t>::max()) {
      throw std::overflow_error("tracing accumulation sample count overflows");
    }
    m_colorSum[y][x] += color;
    ++m_sampleCount[y][x];
    if (m_secondMoment) {
      (*m_secondMoment)[y][x] += squared(color);
    }
  }

  void TracingAccumulationBuffer::addSamples(const Buffer<Colord>& colors) {
    validateTargetShape(colors.width(), colors.height(), "sample color buffer");
    for (int y = 0; y != m_layout.height; ++y) {
      for (int x = 0; x != m_layout.width; ++x) {
        addSample(x, y, colors[y][x]);
      }
    }
  }

  Colord TracingAccumulationBuffer::resolvedColor(int x, int y) const {
    validatePixel(x, y);
    const std::uint32_t count = m_sampleCount[y][x];
    if (count == 0) {
      return Colord::black();
    }
    return m_colorSum[y][x] * (1.0 / static_cast<double>(count));
  }

  void TracingAccumulationBuffer::resolve(Buffer<unsigned int>& target,
                                          const Tonemap* tonemap) const {
    validateTargetShape(target.width(), target.height(), "resolve target");
    for (int y = 0; y != m_layout.height; ++y) {
      for (int x = 0; x != m_layout.width; ++x) {
        const Colord color = resolvedColor(x, y);
        target[y][x] = (tonemap ? tonemap->apply(color) : color).rgb();
      }
    }
  }

  void TracingAccumulationBuffer::validatePixel(int x, int y) const {
    if (x < 0 || y < 0 || x >= m_layout.width || y >= m_layout.height) {
      throw std::out_of_range("tracing accumulation pixel is out of range");
    }
  }

  void TracingAccumulationBuffer::validateTargetShape(int width, int height,
                                                      const char* targetName) const {
    if (width != m_layout.width || height != m_layout.height) {
      throw std::invalid_argument(std::string("tracing accumulation ") + targetName +
                                  " dimensions do not match the layout");
    }
  }
}
