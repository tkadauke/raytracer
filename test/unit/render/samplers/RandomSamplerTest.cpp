#include <gtest/gtest.h>
#include "render/samplers/RandomSampler.h"
#include "core/math/Number.h"

namespace RandomSamplerTest {
  using namespace ::testing;
  using namespace render;
  using namespace render;

  TEST(RandomSampler, ShouldConstructWithParameters) {
    render::RandomSampler sampler;
    sampler.setup(4, 1);
    ASSERT_EQ(4, sampler.numSamples());
  }

  // RandomSampler routes through the thread-local PCG32 PRNG in Number.h,
  // so seeding with seed() before two independent setups produces identical sets.
  TEST(RandomSampler, SameSeedProducesIdenticalSets) {
    seed(42);
    RandomSampler s1;
    s1.setup(16, 10);

    seed(42);
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
    seed(1);
    RandomSampler s1;
    s1.setup(16, 10);

    seed(99999);
    RandomSampler s2;
    s2.setup(16, 10);

    bool anyDifference = false;
    for (int i = 0; i < s1.numSets() && !anyDifference; ++i)
      anyDifference = (s1.setAt(i) != s2.setAt(i));

    EXPECT_TRUE(anyDifference) << "RandomSampler produced identical sets for seeds 1 and 99999";
  }

  TEST(RandomSampler, ExplicitSetupSeedProducesIdenticalSets) {
    RandomSampler s1;
    s1.setup(16, 10, 42);

    RandomSampler s2;
    s2.setup(16, 10, 42);

    for (int i = 0; i < s1.numSets(); ++i)
      EXPECT_EQ(s1.setAt(i), s2.setAt(i));
  }

  TEST(RandomSampler, ExplicitSetupSeedDoesNotPerturbCallerRng) {
    seed(777);
    const int first = random(1000000);

    RandomSampler sampler;
    sampler.setup(16, 10, 123);

    const int second = random(1000000);

    seed(777);
    EXPECT_EQ(first, random(1000000));
    EXPECT_EQ(second, random(1000000));
  }
}
