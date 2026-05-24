#include "render/samplers/Sampler.h"

using namespace render;

namespace {
  // Default `SampleStream` over a Sampler's pre-baked sample sets.
  // Stratification across the per-pixel sample loop comes from
  // pinning a single set per dimension; decorrelation across pixels
  // comes from offsetting the chosen set by `pixelHash`.
  //
  // For dimension `dim` we look up
  //   set = sampler->setAt((pixelHash + dim) mod numSets)
  // and read `set[sampleIndex]`. That guarantees:
  //   - within one dimension we walk the SAME stratified set across
  //     all `numSamples` samples for this pixel — the stratification
  //     each concrete sampler builds into its sets is preserved.
  //   - across dimensions we walk DIFFERENT sets — independent jitter.
  //   - across pixels we walk different sets — no banded patterns.
  //
  // `next1D` reads the x-coordinate of a 2D sample. The y is wasted,
  // which is wasteful but defensible for the default fallback;
  // future low-discrepancy samplers (Sobol, Halton) will override
  // `Sampler::stream` to return a sequence-aware stream that doesn't
  // discard half of every 2D point.
  class DefaultSampleStream : public SampleStream {
  public:
    DefaultSampleStream(const Sampler* sampler, int sampleIndex, std::uint64_t pixelHash)
        : m_sampler(sampler),
          m_sampleIndex(sampleIndex),
          m_pixelHash(pixelHash),
          m_dim(0) {
    }

    Vector2d next2D() override {
      const auto& set = m_sampler->setAt((m_pixelHash + m_dim) % m_sampler->numSets());
      ++m_dim;
      return set[m_sampleIndex];
    }

    double next1D() override {
      const auto& set = m_sampler->setAt((m_pixelHash + m_dim) % m_sampler->numSets());
      ++m_dim;
      return set[m_sampleIndex].x();
    }

  private:
    const Sampler* m_sampler;
    int m_sampleIndex;
    std::uint64_t m_pixelHash;
    std::uint64_t m_dim;
  };
}

void Sampler::setup(int numSamples, int numSets) {
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

std::unique_ptr<SampleStream> Sampler::stream(int sampleIndex, std::uint64_t pixelHash) const {
  return std::make_unique<DefaultSampleStream>(this, sampleIndex, pixelHash);
}
