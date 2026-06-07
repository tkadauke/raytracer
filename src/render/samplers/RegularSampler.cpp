#include "render/samplers/RegularSampler.h"
#include "render/SamplingSeed.h"
#include "render/samplers/SamplerFactory.h"

#include <cmath>

using namespace render;
using namespace std;

Vector2d RegularSampler::sampleForDimension(int sampleIndex, uint64_t pixelHash,
                                            uint64_t dimension) const {
  if (!shouldScrambleDimension(dimension)) {
    return Sampler::sampleForDimension(sampleIndex, pixelHash, dimension);
  }

  return scrambledPathDimensionSample(sampleIndex, pixelHash, dimension);
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

bool RegularSampler::shouldScrambleDimension(uint64_t dimension) const {
  return dimension >= sampleDimensionIndex(SampleDimension::BSDF);
}

Vector2d RegularSampler::scrambledPathDimensionSample(int sampleIndex, uint64_t pixelHash,
                                                      uint64_t dimension) const {
  const Vector2d base = Sampler::sampleForDimension(sampleIndex, pixelHash, dimension);
  return Vector2d(wrapUnitInterval(base.x() + scrambledOffset(sampleIndex, pixelHash, dimension,
                                                              /*axis=*/0)),
                  wrapUnitInterval(base.y() + scrambledOffset(sampleIndex, pixelHash, dimension,
                                                              /*axis=*/1)));
}

double RegularSampler::scrambledOffset(int sampleIndex, uint64_t pixelHash, uint64_t dimension,
                                       uint64_t axis) const {
  std::uint64_t bits = SamplingSeed::mix(pixelHash);
  bits = SamplingSeed::mix(bits ^ SamplingSeed::mix(static_cast<std::uint64_t>(sampleIndex)));
  bits = SamplingSeed::mix(bits ^ SamplingSeed::mix(dimension));
  bits = SamplingSeed::mix(bits ^ SamplingSeed::mix(axis));
  constexpr double denominator = static_cast<double>(1ull << 53u);
  return static_cast<double>(bits >> 11u) / denominator;
}

double RegularSampler::wrapUnitInterval(double value) const {
  value -= std::floor(value);
  if (value >= 1.0) {
    return std::nextafter(1.0, 0.0);
  }
  return value;
}

static bool dummy = SamplerFactory::self().registerClass<RegularSampler>("RegularSampler");
