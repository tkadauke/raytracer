#pragma once

#include "core/Factory.h"
#include "core/Singleton.h"
#include "raytracer/samplers/Sampler.h"

namespace raytracer {
  typedef Singleton<Factory<Sampler>> SamplerFactory;
}
