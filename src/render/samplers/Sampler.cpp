#include "render/samplers/Sampler.h"

#include "render/SamplingSeed.h"

#include <cmath>
#include <utility>

namespace render {
  // Stratification across the per-pixel sample loop comes from pinning a single
  // set per dimension; decorrelation across pixels comes from offsetting the
  // chosen set by `pixelHash`.
  //
  // For dimension `dim` we look up
  //   set = sampler->setAt((pixelHash + dim) mod numSets)
  // and read `set[sampleIndex]`. That guarantees:
  //   - within one dimension we walk the SAME stratified set across all
  //     `numSamples` samples for this pixel.
  //   - across dimensions we walk DIFFERENT sets.
  //   - across pixels we walk different sets.
  //
  // `next1D` reads the x-coordinate of a 2D sample. The y is wasted, which is
  // acceptable for the default fallback; future low-discrepancy samplers can
  // override `Sampler::stream` to avoid discarding half of every 2D point.
  SamplerSampleStream::SamplerSampleStream(const Sampler& sampler, int sampleIndex,
                                           std::uint64_t pixelHash)
      : m_sampler(&sampler),
        m_sampleIndex(sampleIndex),
        m_pixelHash(pixelHash),
        m_dim(0) {
  }

  Vector2d SamplerSampleStream::next2D() {
    const Vector2d sample = sampleForDimension(m_dim);
    ++m_dim;
    return sample;
  }

  double SamplerSampleStream::next1D() {
    const Vector2d sample = sampleForDimension(m_dim);
    ++m_dim;
    return sample.x();
  }

  SampleStream::PrimarySample SamplerSampleStream::primarySample() {
    const Vector2d pixelSample = sampleForDimension(m_dim);
    const Vector2d timeSample = sampleForDimension(m_dim + 1);
    m_dim += 2;
    return SampleStream::PrimarySample{pixelSample, timeSample.x()};
  }

  Vector2d SamplerSampleStream::sample2D(SampleDimension dimension, std::uint64_t index) {
    return sampleForDimension(sampleDimensionIndex(dimension, index));
  }

  double SamplerSampleStream::sample1D(SampleDimension dimension, std::uint64_t index) {
    return sampleForDimension(sampleDimensionIndex(dimension, index)).x();
  }

  Vector2d SamplerSampleStream::sampleForDimension(std::uint64_t dimension) const {
    return m_sampler->sampleForDimension(m_sampleIndex, m_pixelHash, dimension);
  }

  void SampleStreamStorage::reserve(std::size_t count) {
    m_ownedStreams.reserve(count);
    if (m_samplerBackedStreams.empty())
      m_samplerBackedStreams.reserve(count);
  }

  SampleStream* SampleStreamStorage::appendOwned(std::shared_ptr<SampleStream> stream) {
    m_ownedStreams.push_back(std::move(stream));
    return m_ownedStreams.back().get();
  }

  SampleStream* SampleStreamStorage::appendSamplerBacked(const Sampler& sampler, int sampleIndex,
                                                         std::uint64_t pixelHash) {
    if (m_samplerBackedStreams.size() < m_samplerBackedStreams.capacity()) {
      m_samplerBackedStreams.emplace_back(sampler, sampleIndex, pixelHash);
      return &m_samplerBackedStreams.back();
    }

    m_samplerBackedOverflowStreams.emplace_back(sampler, sampleIndex, pixelHash);
    return &m_samplerBackedOverflowStreams.back();
  }

  void Sampler::setup(int numSamples, int numSets) {
    m_sampleSets.clear();
    m_numSamples = numSamples;
    m_numSets = numSets;

    for (int i = 0; i != m_numSets; ++i) {
      m_sampleSets.push_back(generateSet());
    }

    // Concrete samplers may produce a different sample count than
    // requested — `RegularSampler` / `JitteredSampler` floor `sqrt(N)`
    // to lay out an n×n grid, which silently downgrades 50 → 49,
    // 100 → 100 (unchanged), 1024 → 1024 (unchanged), 2048 → 2025. Sync
    // `m_numSamples` with what was actually produced so callers reading
    // `numSamples()` see the truthful count and the `Camera::plot`
    // averaging divides by the right denominator.
    if (!m_sampleSets.empty()) {
      m_numSamples = static_cast<int>(m_sampleSets[0].size());
    }
  }

  void Sampler::setup(int numSamples, int numSets, std::uint64_t seed) {
    RandomSeedScope scopedSeed(seed);
    setup(numSamples, numSets);
  }

  std::unique_ptr<SampleStream> Sampler::stream(int sampleIndex, std::uint64_t pixelHash) const {
    return std::make_unique<SamplerSampleStream>(*this, sampleIndex, pixelHash);
  }

  Vector2d Sampler::sampleForDimension(int sampleIndex, std::uint64_t pixelHash,
                                       std::uint64_t dimension) const {
    const auto& set = setAt((pixelHash + dimension) % numSets());
    return set[sampleIndex];
  }

  std::shared_ptr<SampleStream> Sampler::sharedStream(int sampleIndex,
                                                      std::uint64_t pixelHash) const {
    return std::shared_ptr<SampleStream>(stream(sampleIndex, pixelHash));
  }

  SampleStream* Sampler::appendStream(SampleStreamStorage& storage, int sampleIndex,
                                      std::uint64_t pixelHash) const {
    return storage.appendOwned(sharedStream(sampleIndex, pixelHash));
  }

  std::shared_ptr<SampleStream> Sampler::sharedSamplerBackedStream(int sampleIndex,
                                                                   std::uint64_t pixelHash) const {
    return std::make_shared<SamplerSampleStream>(*this, sampleIndex, pixelHash);
  }

  SampleStream* Sampler::appendSamplerBackedStream(SampleStreamStorage& storage, int sampleIndex,
                                                   std::uint64_t pixelHash) const {
    return storage.appendSamplerBacked(*this, sampleIndex, pixelHash);
  }

  bool Sampler::isPathTracingDimension(std::uint64_t dimension) const {
    return dimension >= sampleDimensionIndex(SampleDimension::BSDF);
  }

  Vector2d Sampler::offsetScrambledPathDimensionSample(int sampleIndex, std::uint64_t pixelHash,
                                                       std::uint64_t dimension) const {
    const Vector2d base = Sampler::sampleForDimension(sampleIndex, pixelHash, dimension);
    return Vector2d(wrapUnitInterval(base.x() + scrambledOffset(sampleIndex, pixelHash, dimension,
                                                                /*axis=*/0)),
                    wrapUnitInterval(base.y() + scrambledOffset(sampleIndex, pixelHash, dimension,
                                                                /*axis=*/1)));
  }

  double Sampler::scrambledOffset(int sampleIndex, std::uint64_t pixelHash, std::uint64_t dimension,
                                  std::uint64_t axis) const {
    std::uint64_t bits = SamplingSeed::mix(pixelHash);
    bits = SamplingSeed::mix(bits ^ SamplingSeed::mix(static_cast<std::uint64_t>(sampleIndex)));
    bits = SamplingSeed::mix(bits ^ SamplingSeed::mix(dimension));
    bits = SamplingSeed::mix(bits ^ SamplingSeed::mix(axis));
    constexpr double denominator = static_cast<double>(1ull << 53u);
    return static_cast<double>(bits >> 11u) / denominator;
  }

  double Sampler::wrapUnitInterval(double value) const {
    value -= std::floor(value);
    if (value >= 1.0) {
      return std::nextafter(1.0, 0.0);
    }
    return value;
  }
}
