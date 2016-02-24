#pragma once

#include "raytracer/samplers/Sampler.h"

namespace raytracer {
  class RegularSampler : public Sampler {
  public:
    RegularSampler(int numSamples)
      : Sampler(numSamples, 1)
    {
    }
    
  protected:
    virtual std::vector<Vector2d> generateSet() const;
  };
}
