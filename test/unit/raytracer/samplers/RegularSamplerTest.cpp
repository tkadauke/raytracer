#include "gtest.h"
#include "raytracer/samplers/RegularSampler.h"

namespace RegularSamplerTest {
  using namespace ::testing;
  using namespace raytracer;
  
  TEST(RegularSampler, ShouldConstructWithParameters) {
    RegularSampler sampler(4);
    ASSERT_EQ(4, sampler.numSamples());
  }
  
  TEST(RegularSampler, ShouldBeEquallySpacedHorizontally) {
    RegularSampler sampler(4);
    auto set = sampler.sampleSet();
    ASSERT_EQ(set[1] - set[0], set[3] - set[2]);
  }
  
  TEST(RegularSampler, ShouldBeEquallySpacedVertically) {
    RegularSampler sampler(4);
    auto set = sampler.sampleSet();
    ASSERT_EQ(set[2] - set[0], set[3] - set[1]);
  }
}
