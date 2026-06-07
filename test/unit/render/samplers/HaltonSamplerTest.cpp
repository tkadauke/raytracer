#include <gtest/gtest.h>

#include "render/samplers/HaltonSampler.h"
#include "render/samplers/SamplerFactory.h"

namespace HaltonSamplerTest {
  using namespace render;

  TEST(HaltonSampler, ShouldConstructWithRequestedSampleCount) {
    HaltonSampler sampler;
    sampler.setup(5, 3);

    EXPECT_EQ(5, sampler.numSamples());
    EXPECT_EQ(3, sampler.numSets());
    EXPECT_EQ(5u, sampler.setAt(0).size());
  }

  TEST(HaltonSampler, GeneratesBaseTwoThreeSetForLegacyApi) {
    HaltonSampler sampler;
    sampler.setup(4, 1);

    const auto& set = sampler.setAt(0);

    ASSERT_EQ(4u, set.size());
    EXPECT_DOUBLE_EQ(0.5, set[0].x());
    EXPECT_DOUBLE_EQ(1.0 / 3.0, set[0].y());
    EXPECT_DOUBLE_EQ(0.25, set[1].x());
    EXPECT_DOUBLE_EQ(2.0 / 3.0, set[1].y());
    EXPECT_DOUBLE_EQ(0.75, set[2].x());
    EXPECT_DOUBLE_EQ(1.0 / 9.0, set[2].y());
    EXPECT_DOUBLE_EQ(0.125, set[3].x());
    EXPECT_DOUBLE_EQ(4.0 / 9.0, set[3].y());
  }

  TEST(HaltonSampler, StreamSamplesAreDeterministicForSameInputs) {
    HaltonSampler sampler;
    sampler.setup(16, 1);

    const auto first = sampler.sampleForDimension(/*sampleIndex=*/7, /*pixelHash=*/42,
                                                  /*dimension=*/3);
    const auto second = sampler.sampleForDimension(/*sampleIndex=*/7, /*pixelHash=*/42,
                                                   /*dimension=*/3);

    EXPECT_EQ(first, second);
  }

  TEST(HaltonSampler, StreamSamplesVaryByPixelHash) {
    HaltonSampler sampler;
    sampler.setup(16, 1);

    const auto first = sampler.sampleForDimension(/*sampleIndex=*/7, /*pixelHash=*/42,
                                                  /*dimension=*/3);
    const auto second = sampler.sampleForDimension(/*sampleIndex=*/7, /*pixelHash=*/43,
                                                   /*dimension=*/3);

    EXPECT_NE(first, second);
  }

  TEST(HaltonSampler, StreamSamplesVaryByDimension) {
    HaltonSampler sampler;
    sampler.setup(16, 1);

    const auto bsdf = sampler.sampleForDimension(/*sampleIndex=*/7, /*pixelHash=*/42,
                                                 /*dimension=*/3);
    const auto light = sampler.sampleForDimension(/*sampleIndex=*/7, /*pixelHash=*/42,
                                                  /*dimension=*/4);

    EXPECT_NE(bsdf, light);
  }

  TEST(HaltonSampler, SharedAndRetainedStreamsMatch) {
    HaltonSampler sampler;
    sampler.setup(16, 1);
    SampleStreamStorage storage;

    auto shared = sampler.sharedStream(/*sampleIndex=*/5, /*pixelHash=*/9);
    SampleStream* retained = sampler.appendStream(storage, /*sampleIndex=*/5,
                                                  /*pixelHash=*/9);

    ASSERT_NE(nullptr, shared);
    ASSERT_NE(nullptr, retained);
    EXPECT_EQ(shared->next2D(), retained->next2D());
    EXPECT_DOUBLE_EQ(shared->next1D(), retained->next1D());
    EXPECT_EQ(shared->sample2D(SampleDimension::BSDF, 2),
              retained->sample2D(SampleDimension::BSDF, 2));
  }

  TEST(HaltonSampler, RegistersWithSamplerFactory) {
    auto sampler = SamplerFactory::self().create("HaltonSampler");

    ASSERT_NE(nullptr, sampler);
    EXPECT_NE(nullptr, dynamic_cast<HaltonSampler*>(sampler.get()));
  }
}
