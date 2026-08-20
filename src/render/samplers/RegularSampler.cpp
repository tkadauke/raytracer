#include "render/samplers/RegularSampler.h"
#include "render/samplers/SamplerFactory.h"

using namespace render;

std::vector<Vector2d> RegularSampler::generateSet() {
  return generateGridSet([](int x, int y, int n) {
    return Vector2d((double(x) + 0.5) / double(n), (double(y) + 0.5) / double(n));
  });
}

static bool dummy = SamplerFactory::self().registerClass<RegularSampler>("RegularSampler");
