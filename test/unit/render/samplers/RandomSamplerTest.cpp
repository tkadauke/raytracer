#include <gtest/gtest.h>
#include "render/samplers/RandomSampler.h"

#include <cstdlib>

namespace RandomSamplerTest {
  using namespace ::testing;
  using namespace render;
  using namespace render;

  TEST(RandomSampler, ShouldConstructWithParameters) {
    render::RandomSampler sampler;
    sampler.setup(4, 1);
    ASSERT_EQ(4, sampler.numSamples());
  }

  // RandomSampler routes through std::rand(), so seeding std::srand with
  // the same value before two independent setups produces identical sets.
  TEST(RandomSampler, SameSeedProducesIdenticalSets) {
    std::srand(42);
    RandomSampler s1;
    s1.setup(16, 10);

    std::srand(42);
    RandomSampler s2;
    s2.setup(16, 10);

    for (int i = 0; i < s1.numSets(); ++i) {
      EXPECT_EQ(s1.setAt(i), s2.setAt(i))
        << "set " << i << " differs between identically-seeded RandomSamplers";
    }
  }

  // Different seeds must drive the sampler down a different path. With 16
  // doubles per set across 10 sets the chance of accidental equality is
  // negligible.
  TEST(RandomSampler, DifferentSeedsProduceDifferentSets) {
    std::srand(1);
    RandomSampler s1;
    s1.setup(16, 10);

    std::srand(99999);
    RandomSampler s2;
    s2.setup(16, 10);

    bool anyDifference = false;
    for (int i = 0; i < s1.numSets() && !anyDifference; ++i)
      anyDifference = (s1.setAt(i) != s2.setAt(i));

    EXPECT_TRUE(anyDifference) << "RandomSampler produced identical sets for seeds 1 and 99999";
  }
}
