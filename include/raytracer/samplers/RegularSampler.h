#pragma once

#include "raytracer/samplers/Sampler.h"

namespace raytracer {
  class RegularSampler : public Sampler {
  protected:
    virtual std::vector<Vector2d> generateSet();
  };
}
