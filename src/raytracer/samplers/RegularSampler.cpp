#include "raytracer/samplers/RegularSampler.h"

using namespace raytracer;
using namespace std;

vector<Vector2d> RegularSampler::generateSet() const {
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
