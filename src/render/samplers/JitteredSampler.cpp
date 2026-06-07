#include "render/samplers/JitteredSampler.h"
#include "render/samplers/SamplerFactory.h"

#include "core/math/Range.h"

using namespace render;
using namespace std;

Vector2d JitteredSampler::sampleForDimension(int sampleIndex, uint64_t pixelHash,
                                             uint64_t dimension) const {
  if (!isPathTracingDimension(dimension)) {
    return Sampler::sampleForDimension(sampleIndex, pixelHash, dimension);
  }

  return offsetScrambledPathDimensionSample(sampleIndex, pixelHash, dimension);
}

std::shared_ptr<SampleStream> JitteredSampler::sharedStream(int sampleIndex,
                                                            uint64_t pixelHash) const {
  return sharedSamplerBackedStream(sampleIndex, pixelHash);
}

SampleStream* JitteredSampler::appendStream(SampleStreamStorage& storage, int sampleIndex,
                                            uint64_t pixelHash) const {
  return appendSamplerBackedStream(storage, sampleIndex, pixelHash);
}

std::vector<Vector2d> JitteredSampler::generateSet() {
  vector<Vector2d> result;

  int n = sqrt(numSamples());

  Ranged range(0, 1);

  for (int x = 0; x != n; ++x) {
    for (int y = 0; y != n; ++y) {
      Vector2d sample((double(x) + range.random()) / double(n),
                      (double(y) + range.random()) / double(n));
      result.push_back(sample);
    }
  }

  return result;
}

static bool dummy = SamplerFactory::self().registerClass<JitteredSampler>("JitteredSampler");
