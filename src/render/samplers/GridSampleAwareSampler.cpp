#include "render/samplers/GridSampleAwareSampler.h"

#include <cmath>

using namespace render;
using namespace std;

Vector2d GridSampleAwareSampler::sampleForDimension(int sampleIndex, uint64_t pixelHash,
                                                    uint64_t dimension) const {
  return scrambledPathAwareSample(sampleIndex, pixelHash, dimension);
}

std::vector<Vector2d> GridSampleAwareSampler::generateGridSet(
    const std::function<Vector2d(int x, int y, int n)>& cellSample) const {
  vector<Vector2d> result;

  int n = sqrt(numSamples());

  for (int x = 0; x != n; ++x) {
    for (int y = 0; y != n; ++y) {
      result.push_back(cellSample(x, y, n));
    }
  }

  return result;
}
