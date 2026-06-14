#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "core/Buffer.h"
#include "core/Color.h"
#include "test/helpers/TracingImageComparison.h"

namespace TracingImageComparisonTest {

  TEST(TracingImageComparison, ReportsZeroDeltaForIdenticalHdrImages) {
    Buffer<Colord> expected(2, 2);
    expected.clear(Colord(0.1, 0.2, 0.3));

    Buffer<Colord> actual(2, 2);
    actual.clear(Colord(0.1, 0.2, 0.3));

    const auto stats = test::helpers::compareTracingImages(expected, actual);

    EXPECT_TRUE(stats.dimensionsMatch);
    EXPECT_EQ(2, stats.width);
    EXPECT_EQ(2, stats.height);
    EXPECT_EQ(0u, stats.differingPixels);
    EXPECT_DOUBLE_EQ(0.0, stats.normalizedRmsDelta);
    EXPECT_DOUBLE_EQ(0.0, stats.maxNormalizedChannelDelta);
    EXPECT_TRACING_IMAGE_NEAR(expected, actual, 0.0);
  }

  TEST(TracingImageComparison, AllowsHdrImageWithinNormalizedRmsThreshold) {
    Buffer<Colord> expected(2, 1);
    expected.clear(Colord::black());

    Buffer<Colord> actual(2, 1);
    actual.clear(Colord::black());
    actual[0][0] = Colord(0.3, 0.0, 0.0);

    const auto stats = test::helpers::compareTracingImages(expected, actual);

    EXPECT_EQ(1u, stats.differingPixels);
    EXPECT_NEAR(0.1224744871, stats.normalizedRmsDelta, 1e-10);
    EXPECT_DOUBLE_EQ(0.3, stats.maxNormalizedChannelDelta);
    EXPECT_TRACING_IMAGE_NEAR(expected, actual, 0.13);
  }

  TEST(TracingImageComparison, FailsHdrImageAboveNormalizedRmsThreshold) {
    Buffer<Colord> expected(2, 1);
    expected.clear(Colord::black());

    Buffer<Colord> actual(2, 1);
    actual.clear(Colord::black());
    actual[0][0] = Colord(0.3, 0.0, 0.0);

    const auto assertion = test::helpers::tracingImagesWithinNormalizedRms(
      "expected", "actual", "threshold", expected, actual, 0.12);

    EXPECT_FALSE(assertion);
    EXPECT_THAT(assertion.message(), ::testing::HasSubstr("normalized RMS delta"));
    EXPECT_THAT(assertion.message(), ::testing::HasSubstr("exceeding threshold"));
    EXPECT_THAT(assertion.message(), ::testing::HasSubstr("1 differing pixels"));
  }

  TEST(TracingImageComparison, ComparesRgbImagesWithNormalizedChannelDeltas) {
    Buffer<unsigned int> expected(1, 1);
    expected.clear(0x000000u);

    Buffer<unsigned int> actual(1, 1);
    actual.clear(0xff0000u);

    const auto stats = test::helpers::compareTracingImages(expected, actual);

    EXPECT_EQ(1u, stats.differingPixels);
    EXPECT_NEAR(0.5773502692, stats.normalizedRmsDelta, 1e-10);
    EXPECT_DOUBLE_EQ(1.0, stats.maxNormalizedChannelDelta);
    EXPECT_TRACING_IMAGE_NEAR(expected, actual, 0.58);
  }

  TEST(TracingImageComparison, FailsImagesWithDifferentDimensions) {
    Buffer<Colord> expected(2, 1);
    expected.clear(Colord::black());

    Buffer<Colord> actual(1, 2);
    actual.clear(Colord::black());

    const auto stats = test::helpers::compareTracingImages(expected, actual);
    const auto assertion = test::helpers::tracingImagesWithinNormalizedRms(
      "expected", "actual", "threshold", expected, actual, 1.0);

    EXPECT_FALSE(stats.dimensionsMatch);
    EXPECT_EQ(2u, stats.differingPixels);
    EXPECT_FALSE(assertion);
    EXPECT_THAT(assertion.message(), ::testing::HasSubstr("different dimensions"));
    EXPECT_THAT(assertion.message(), ::testing::HasSubstr("2x1 vs 1x2"));
  }
}
