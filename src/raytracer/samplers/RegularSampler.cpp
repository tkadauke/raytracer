#include "raytracer/samplers/RegularSampler.h"

using namespace raytracer;
using namespace std;

RegularSampler::RegularSampler(int numSamples)
  : Sampler(numSamples, 1)
{
  vector<Vector2d> result;
  
  int n = sqrt(numSamples);
  
  for (int x = 0; x != n; ++x) {
    for (int y = 0; y != n; ++y) {
      Vector2d sample(
        (double(x) + 0.5) / double(n),
        (double(y) + 0.5) / double(n)
      );
      result.push_back(sample);
    }
  }
  
  addSet(result);
}
