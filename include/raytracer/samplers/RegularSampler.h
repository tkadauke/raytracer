#pragma once

#include "raytracer/samplers/Sampler.h"

namespace raytracer {
  class RegularSampler : public Sampler {
  public:
    RegularSampler(int numSamples);
  };
}
