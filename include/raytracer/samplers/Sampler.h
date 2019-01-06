#pragma once

#include <vector>

#include "core/math/Vector.h"
#include "core/math/Number.h"

#include <iostream>

namespace raytracer {
  class Sampler {
  public:
    inline Sampler()
      : m_numSamples(0),
        m_numSets(0)
    {
    }
    inline virtual ~Sampler() {}

    void setup(int numSamples, int numSets);

    inline int numSamples() const {
      return m_numSamples;
    }

    inline const std::vector<Vector2d>& sampleSet() const {
      return m_sampleSets[random(m_numSets)];
    }

  protected:
    virtual std::vector<Vector2d> generateSet() = 0;

  private:
    std::vector<std::vector<Vector2d>> m_sampleSets;
    int m_numSamples;
    int m_numSets;
  };
}
