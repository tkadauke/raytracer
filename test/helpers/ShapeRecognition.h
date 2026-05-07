#pragma once

#include <vector>
#include "core/Buffer.h"
#include "core/Color.h"

namespace testing {
  class ShapeRecognition {
  public:
    /// Construct a recogniser that scans for pixels of `targetColor`.
    /// Default is red — matches the historical Raytracer-only
    /// behaviour. Wireframe output is white-on-black, so its
    /// fixtures pass `Colord::white()`.
    inline explicit ShapeRecognition(const Colord& targetColor = Colord(1, 0, 0))
      : m_targetColor(targetColor)
    {
    }

    bool recognizeRect(const Buffer<unsigned int>& buffer) const;
    bool recognizeCircle(const Buffer<unsigned int>& buffer) const;

  private:
    std::vector<int> lines(const Buffer<unsigned int>& buffer) const;

    Colord m_targetColor;
  };
}
