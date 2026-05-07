#include <gtest/gtest.h>
#include "render/samplers/JitteredSampler.h"

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
}
