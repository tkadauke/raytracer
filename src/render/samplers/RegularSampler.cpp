#include "render/samplers/RegularSampler.h"
#include "render/samplers/SamplerFactory.h"

#include <cmath>

using namespace render;
using namespace std;

Vector2d RegularSampler::sampleForDimension(int sampleIndex, uint64_t pixelHash,
                                            uint64_t dimension) const {
  if (!isPathTracingDimension(dimension)) {
    return Sampler::sampleForDimension(sampleIndex, pixelHash, dimension);
  }

  return offsetScrambledPathDimensionSample(sampleIndex, pixelHash, dimension);
}

std::shared_ptr<SampleStream> RegularSampler::sharedStream(int sampleIndex,
                                                           uint64_t pixelHash) const {
  return sharedSamplerBackedStream(sampleIndex, pixelHash);
}

SampleStream* RegularSampler::appendStream(SampleStreamStorage& storage, int sampleIndex,
                                           uint64_t pixelHash) const {
  return appendSamplerBackedStream(storage, sampleIndex, pixelHash);
}

std::vector<Vector2d> RegularSampler::generateSet() {
  vector<Vector2d> result;

  int n = sqrt(numSamples());

  for (int x = 0; x != n; ++x) {
    for (int y = 0; y != n; ++y) {
      Vector2d sample((double(x) + 0.5) / double(n), (double(y) + 0.5) / double(n));
      result.push_back(sample);
    }
  }

  return result;
}

static bool dummy = SamplerFactory::self().registerClass<RegularSampler>("RegularSampler");
