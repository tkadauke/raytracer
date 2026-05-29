#pragma once

#include "render/RayCaster.h"
#include "render/State.h"

#include <cstddef>
#include <vector>

namespace test::helpers {

  class RecordingRayCaster final : public render::RayCaster {
  public:
    explicit RecordingRayCaster(Colord fallbackColor = Colord::black())
        : m_fallbackColor(fallbackColor) {
    }

    void pushColor(Colord color) {
      m_colors.push_back(color);
    }

    Colord rayColor(const Rayd& ray, render::State& state) const override {
      rays.push_back(ray);
      ++state.numRays;

      const std::size_t index = rays.size() - 1;
      if (index < m_colors.size()) {
        return m_colors[index];
      }
      return m_fallbackColor;
    }

    mutable std::vector<Rayd> rays;

  private:
    Colord m_fallbackColor;
    std::vector<Colord> m_colors;
  };
}
