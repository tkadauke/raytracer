#include "render/samplers/Sampler.h"

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
    const auto& set = sampleSetForDimension(m_dim);
    ++m_dim;
    return set[m_sampleIndex];
  }

  double SamplerSampleStream::next1D() {
    const auto& set = sampleSetForDimension(m_dim);
    ++m_dim;
    return set[m_sampleIndex].x();
  }

  SampleStream::PrimarySample SamplerSampleStream::primarySample() {
    const auto& pixelSet = sampleSetForDimension(m_dim);
    const auto& timeSet = sampleSetForDimension(m_dim + 1);
    m_dim += 2;
    return SampleStream::PrimarySample{pixelSet[m_sampleIndex], timeSet[m_sampleIndex].x()};
  }

  Vector2d SamplerSampleStream::sample2D(SampleDimension dimension, std::uint64_t index) {
    return sampleSetForDimension(sampleDimensionIndex(dimension, index))[m_sampleIndex];
  }

  double SamplerSampleStream::sample1D(SampleDimension dimension, std::uint64_t index) {
    return sampleSetForDimension(sampleDimensionIndex(dimension, index))[m_sampleIndex].x();
  }

  const std::vector<Vector2d>&
  SamplerSampleStream::sampleSetForDimension(std::uint64_t dimension) const {
    return m_sampler->setAt((m_pixelHash + dimension) % m_sampler->numSets());
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
}
