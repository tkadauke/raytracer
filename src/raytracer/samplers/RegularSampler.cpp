#include "raytracer/samplers/RegularSampler.h"
#include "raytracer/samplers/SamplerFactory.h"

using namespace raytracer;
using namespace std;

std::vector<Vector2d> RegularSampler::generateSet() {
  vector<Vector2d> result;
  
  int n = sqrt(numSamples());
  
  for (int x = 0; x != n; ++x) {
    for (int y = 0; y != n; ++y) {
      Vector2d sample(
        (double(x) + 0.5) / double(n),
        (double(y) + 0.5) / double(n)
      );
      result.push_back(sample);
    }
  }
  
  return result;
}

static bool dummy = SamplerFactory::self().registerClass<RegularSampler>("RegularSampler");
