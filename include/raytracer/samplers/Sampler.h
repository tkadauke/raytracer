#pragma once

#include <vector>

#include "core/math/Vector.h"
#include "core/math/Number.h"

#include <iostream>

namespace raytracer {
  class Sampler {
  public:
    Sampler(int numSamples, int numSets)
      : m_numSamples(numSamples), m_numSets(numSets)
    {
    }

    inline int numSamples() const { return m_numSamples; }
    inline const std::vector<Vector2d>& sampleSet() const {
      return m_sampleSets[randomInt(m_numSets)];
    }
    
  protected:
    inline void addSet(const std::vector<Vector2d>& set) {
      m_sampleSets.push_back(set);
    }
    
  private:
    std::vector<std::vector<Vector2d>> m_sampleSets;
    int m_numSamples;
    int m_numSets;
  };
}
