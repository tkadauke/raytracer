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
      setup();
      return m_sampleSets[randomInt(m_numSets)];
    }
    
  protected:
    virtual std::vector<Vector2d> generateSet() const = 0;
    
  private:
    void setup() const {
      if (!m_sampleSets.empty()) {
        return;
      }

      for (int i = 0; i != m_numSets; ++i) {
        m_sampleSets.push_back(generateSet());
      }
    }
    
    mutable std::vector<std::vector<Vector2d>> m_sampleSets;
    int m_numSamples;
    int m_numSets;
  };
}
