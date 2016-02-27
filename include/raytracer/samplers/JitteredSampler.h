#pragma once

#include "raytracer/samplers/Sampler.h"

namespace raytracer {
  class JitteredSampler : public Sampler {
  protected:
    virtual std::vector<Vector2d> generateSet();
  };
}
