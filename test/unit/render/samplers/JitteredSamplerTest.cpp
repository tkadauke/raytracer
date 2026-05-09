#include <gtest/gtest.h>
#include "render/samplers/JitteredSampler.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace JitteredSamplerTest {
  using namespace ::testing;
  using namespace render;
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
}
