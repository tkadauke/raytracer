#include "render/samplers/JitteredSampler.h"
#include "render/samplers/SamplerFactory.h"

#include "core/math/Range.h"

using namespace render;

std::vector<Vector2d> JitteredSampler::generateSet() {
  Ranged range(0, 1);

  return generateGridSet([&range](int x, int y, int n) {
    return Vector2d((double(x) + range.random()) / double(n),
                    (double(y) + range.random()) / double(n));
  });
}

static bool dummy = SamplerFactory::self().registerClass<JitteredSampler>("JitteredSampler");
