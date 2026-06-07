#include "render/samplers/HaltonSampler.h"
#include "render/SamplingSeed.h"
#include "render/samplers/SamplerFactory.h"

#include <array>
#include <cmath>
#include <cstddef>

using namespace render;
using namespace std;

Vector2d HaltonSampler::sampleForDimension(int sampleIndex, uint64_t pixelHash,
                                           uint64_t dimension) const {
  const uint64_t sequenceIndex = static_cast<uint64_t>(sampleIndex) + 1u;
  const double x = radicalInverse(sequenceIndex, baseForDimension(dimension, /*axis=*/0));
  const double y = radicalInverse(sequenceIndex, baseForDimension(dimension, /*axis=*/1));

  return Vector2d(wrapUnitInterval(x + rotationOffset(pixelHash, dimension, /*axis=*/0)),
                  wrapUnitInterval(y + rotationOffset(pixelHash, dimension, /*axis=*/1)));
}

std::shared_ptr<SampleStream> HaltonSampler::sharedStream(int sampleIndex,
                                                          uint64_t pixelHash) const {
  return sharedSamplerBackedStream(sampleIndex, pixelHash);
}

SampleStream* HaltonSampler::appendStream(SampleStreamStorage& storage, int sampleIndex,
                                          uint64_t pixelHash) const {
  return appendSamplerBackedStream(storage, sampleIndex, pixelHash);
}

std::vector<Vector2d> HaltonSampler::generateSet() {
  vector<Vector2d> result;
  result.reserve(static_cast<size_t>(numSamples()));

  for (int i = 0; i != numSamples(); ++i) {
    const uint64_t sequenceIndex = static_cast<uint64_t>(i) + 1u;
    result.push_back(
      Vector2d(radicalInverse(sequenceIndex, 2u), radicalInverse(sequenceIndex, 3u)));
  }

  return result;
}

uint32_t HaltonSampler::baseForDimension(uint64_t dimension, uint64_t axis) const {
  constexpr array<uint32_t, 64> primeBases{
    2u,   3u,   5u,   7u,   11u,  13u,  17u,  19u,  23u,  29u,  31u,  37u,  41u,  43u,  47u,  53u,
    59u,  61u,  67u,  71u,  73u,  79u,  83u,  89u,  97u,  101u, 103u, 107u, 109u, 113u, 127u, 131u,
    137u, 139u, 149u, 151u, 157u, 163u, 167u, 173u, 179u, 181u, 191u, 193u, 197u, 199u, 211u, 223u,
    227u, 229u, 233u, 239u, 241u, 251u, 257u, 263u, 269u, 271u, 277u, 281u, 283u, 293u, 307u, 311u};

  const size_t index = static_cast<size_t>((dimension * 2u + axis) % primeBases.size());
  return primeBases[index];
}

double HaltonSampler::radicalInverse(uint64_t index, uint32_t base) const {
  double reversed = 0.0;
  double placeValue = 1.0 / static_cast<double>(base);

  while (index > 0u) {
    const uint64_t digit = index % base;
    reversed += static_cast<double>(digit) * placeValue;
    index /= base;
    placeValue /= static_cast<double>(base);
  }

  return reversed;
}

double HaltonSampler::rotationOffset(uint64_t pixelHash, uint64_t dimension, uint64_t axis) const {
  uint64_t bits = SamplingSeed::mix(pixelHash);
  bits = SamplingSeed::mix(bits ^ SamplingSeed::mix(dimension));
  bits = SamplingSeed::mix(bits ^ SamplingSeed::mix(axis));
  constexpr double denominator = static_cast<double>(1ull << 53u);
  return static_cast<double>(bits >> 11u) / denominator;
}

double HaltonSampler::wrapUnitInterval(double value) const {
  value -= floor(value);
  if (value >= 1.0) {
    return nextafter(1.0, 0.0);
  }
  return value;
}

static bool dummy = SamplerFactory::self().registerClass<HaltonSampler>("HaltonSampler");
