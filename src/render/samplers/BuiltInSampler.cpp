#include "render/samplers/BuiltInSampler.h"

using namespace render;

std::shared_ptr<SampleStream> BuiltInSampler::sharedStream(int sampleIndex,
                                                            uint64_t pixelHash) const {
  return sharedSamplerBackedStream(sampleIndex, pixelHash);
}

SampleStream* BuiltInSampler::appendStream(SampleStreamStorage& storage, int sampleIndex,
                                           uint64_t pixelHash) const {
  return appendSamplerBackedStream(storage, sampleIndex, pixelHash);
}

Vector2d BuiltInSampler::scrambledPathAwareSample(int sampleIndex, uint64_t pixelHash,
                                                  uint64_t dimension) const {
  if (!isPathTracingDimension(dimension)) {
    return Sampler::sampleForDimension(sampleIndex, pixelHash, dimension);
  }

  return offsetScrambledPathDimensionSample(sampleIndex, pixelHash, dimension);
}
