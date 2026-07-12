#include <gtest/gtest.h>
#include "render/samplers/JitteredSampler.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace JitteredSamplerTest {
  using namespace ::testing;
  using namespace render;

  TEST(JitteredSampler, ShouldConstructWithParameters) {
    render::JitteredSampler sampler;
    sampler.setup(4, 1);
    ASSERT_EQ(4, sampler.numSamples());
  }

  TEST(JitteredSampler, ShouldBeRegularlySpacedHorizontally) {
    render::JitteredSampler sampler;
    sampler.setup(4, 1);
    auto set = sampler.sampleSet();
    ASSERT_TRUE(set[0].x() <= 0.5);
    ASSERT_TRUE(set[2].x() >= 0.5);
  }

  TEST(JitteredSampler, ShouldBeRegularlySpacedVertically) {
    render::JitteredSampler sampler;
    sampler.setup(4, 1);
    auto set = sampler.sampleSet();
    ASSERT_TRUE(set[0].y() <= 0.5);
    ASSERT_TRUE(set[1].y() >= 0.5);
  }

  // Each n×n stratum [x/n, (x+1)/n] × [y/n, (y+1)/n] is visited by
  // exactly one sample per set — the jitter stays inside its cell. Across
  // many sets the per-stratum count is exactly numSets, not statistically
  // close to it.
  TEST(JitteredSampler, EachStratumGetsExactlyOneSamplePerSet) {
    const int n = 5;
    const int numSets = 200;
    JitteredSampler sampler;
    sampler.setup(n * n, numSets);

    std::vector<int> buckets(n * n, 0);
    for (int s = 0; s < numSets; ++s) {
      for (const auto& sample : sampler.setAt(s)) {
        int bx = std::min(static_cast<int>(std::floor(sample.x() * n)), n - 1);
        int by = std::min(static_cast<int>(std::floor(sample.y() * n)), n - 1);
        ++buckets[bx * n + by];
      }
    }

    for (int i = 0; i < n * n; ++i) {
      EXPECT_EQ(numSets, buckets[i])
        << "stratum " << i << " has " << buckets[i] << " samples (expected " << numSets << ")";
    }

    EXPECT_NE(sampler.setAt(0), sampler.setAt(1))
      << "sets must differ — jitter must vary across sets";
  }

  TEST(JitteredSampler, CameraDimensionsUseUnscrambledJitteredSets) {
    JitteredSampler sampler;
    sampler.setup(4, 8, 12345);

    auto stream = sampler.stream(/*sampleIndex=*/0, /*pixelHash=*/0);

    EXPECT_EQ(sampler.setAt(0)[0], stream->sample2D(SampleDimension::Pixel));
    EXPECT_DOUBLE_EQ(sampler.setAt(1)[0].x(), stream->sample1D(SampleDimension::Time));
    EXPECT_EQ(sampler.setAt(2)[0], stream->sample2D(SampleDimension::Lens));
  }

  TEST(JitteredSampler, PathTracingDimensionsAreScrambled) {
    JitteredSampler sampler;
    sampler.setup(16, 32, 12345);

    auto stream = sampler.stream(/*sampleIndex=*/2, /*pixelHash=*/0);

    EXPECT_NE(sampler.setAt(3)[2], stream->sample2D(SampleDimension::BSDF, 0));
    EXPECT_NE(sampler.setAt(4)[2], stream->sample2D(SampleDimension::Light, 0));
    EXPECT_NE(sampler.setAt(5)[2].x(), stream->sample1D(SampleDimension::Continuation, 0));
  }

  TEST(JitteredSampler, PathTracingDimensionScrambleIsStable) {
    JitteredSampler sampler;
    sampler.setup(16, 32, 12345);

    auto first = sampler.stream(/*sampleIndex=*/2, /*pixelHash=*/17);
    auto second = sampler.stream(/*sampleIndex=*/2, /*pixelHash=*/17);

    EXPECT_EQ(first->sample2D(SampleDimension::BSDF, 2),
              second->sample2D(SampleDimension::BSDF, 2));
    EXPECT_DOUBLE_EQ(first->sample1D(SampleDimension::LightSelection, 3),
                     second->sample1D(SampleDimension::LightSelection, 3));
  }

  TEST(JitteredSampler, PathTracingDimensionsDoNotLockToMatchingStrata) {
    const int n = 8;
    JitteredSampler sampler;
    sampler.setup(n * n, 83, 12345);

    auto stratum = [](const Vector2d& sample) {
      const int bx = std::min(static_cast<int>(std::floor(sample.x() * n)), n - 1);
      const int by = std::min(static_cast<int>(std::floor(sample.y() * n)), n - 1);
      return bx * n + by;
    };

    int sameStrata = 0;
    for (int sampleIndex = 0; sampleIndex != sampler.numSamples(); ++sampleIndex) {
      auto stream = sampler.stream(sampleIndex, /*pixelHash=*/19);
      const int bsdfStratum = stratum(stream->sample2D(SampleDimension::BSDF, 0));
      const int lightStratum = stratum(stream->sample2D(SampleDimension::Light, 0));
      if (bsdfStratum == lightStratum) {
        ++sameStrata;
      }
    }

    EXPECT_LT(sameStrata, sampler.numSamples() / 2);
  }
}
