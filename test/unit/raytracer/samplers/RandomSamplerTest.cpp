#include "gtest.h"
#include "raytracer/samplers/RandomSampler.h"

namespace RandomSamplerTest {
  using namespace ::testing;
  using namespace raytracer;
  
  TEST(RandomSampler, ShouldConstructWithParameters) {
    RandomSampler sampler;
    sampler.setup(4, 1);
    ASSERT_EQ(4, sampler.numSamples());
  }
}
