#pragma once

#include "render/primitives/Scene.h"

#include <cstddef>
#include <vector>

namespace render {
  class Light;

  class LightSampler {
  public:
    struct Selection {
      const Light* light{nullptr};
      std::size_t lightIndex{0};
      double pdf{0.0};

      explicit operator bool() const {
        return light != nullptr && pdf > 0.0;
      }
    };

    explicit LightSampler(const Scene::Lights& lights);

    bool empty() const;
    std::size_t size() const;
    Selection select(double unitSample) const;
    double selectionPdf(std::size_t entryIndex) const;

  private:
    struct Entry {
      const Light* light{nullptr};
      std::size_t lightIndex{0};
      double weight{0.0};
    };

    double weightFor(const Light& light) const;
    void normalizeWeights();
    void useUniformWeights();

    std::vector<Entry> m_entries;
    double m_totalWeight{0.0};
  };
}
