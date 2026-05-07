#include <gtest/gtest.h>
#include "render/samplers/RandomSampler.h"

namespace RandomSamplerTest {
  using namespace ::testing;
  using namespace render;
  
  TEST(RandomSampler, ShouldConstructWithParameters) {
    render::RandomSampler sampler;
    sampler.setup(4, 1);
    ASSERT_EQ(4, sampler.numSamples());
  }
}
