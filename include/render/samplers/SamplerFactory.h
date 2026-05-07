#pragma once

#include "core/Factory.h"
#include "core/Singleton.h"
#include "render/samplers/Sampler.h"

namespace render {
  typedef Singleton<Factory<Sampler>> SamplerFactory;
}
