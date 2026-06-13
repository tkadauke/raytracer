#pragma once

#include "core/Buffer.h"
#include "core/Color.h"
#include "render/TracingAccumulationLayout.h"

#include <cstdint>
#include <memory>

namespace render {
  class Tonemap;

  /**
    * CPU reference storage and operations for `TracingAccumulationLayout`.
    *
    * The reference keeps the layout's logical planes separate so platform
    * accumulation kernels can compare color sums, sample counts, optional raw
    * second moments, and resolved display output independently.
    */
  class TracingAccumulationBuffer {
  public:
    explicit TracingAccumulationBuffer(const TracingAccumulationLayout& layout);
    TracingAccumulationBuffer(int width, int height);
    ~TracingAccumulationBuffer();

    [[nodiscard]] const TracingAccumulationLayout& layout() const;
    [[nodiscard]] Buffer<Colord>& colorSum();
    [[nodiscard]] const Buffer<Colord>& colorSum() const;
    [[nodiscard]] Buffer<std::uint32_t>& sampleCount();
    [[nodiscard]] const Buffer<std::uint32_t>& sampleCount() const;
    [[nodiscard]] bool hasSecondMoment() const;
    [[nodiscard]] Buffer<Colord>* secondMoment();
    [[nodiscard]] const Buffer<Colord>* secondMoment() const;
    [[nodiscard]] const TracingAccumulationDiagnostics& diagnostics() const;

    void clear();
    void addSample(int x, int y, const Colord& color);
    void addSamples(const Buffer<Colord>& colors);
    [[nodiscard]] Colord resolvedColor(int x, int y) const;
    void resolve(Buffer<unsigned int>& target, const Tonemap* tonemap = nullptr) const;

  private:
    void validatePixel(int x, int y) const;
    void validateTargetShape(int width, int height, const char* targetName) const;

    TracingAccumulationLayout m_layout;
    Buffer<Colord> m_colorSum;
    Buffer<std::uint32_t> m_sampleCount;
    std::unique_ptr<Buffer<Colord>> m_secondMoment;
    mutable TracingAccumulationDiagnostics m_diagnostics;
  };
}
