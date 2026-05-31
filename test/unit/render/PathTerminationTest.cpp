#include <gtest/gtest.h>

#include "render/PathTermination.h"

#include "test/helpers/ColorTestHelper.h"

namespace PathTerminationTest {
  using namespace render;

  TEST(PathTermination, ComputesScalarContinuationProbability) {
    EXPECT_DOUBLE_EQ(0.0, continuationProbability(0.0));
    EXPECT_DOUBLE_EQ(0.0, continuationProbability(-0.25));
    EXPECT_DOUBLE_EQ(0.05, continuationProbability(0.01));
    EXPECT_DOUBLE_EQ(0.25, continuationProbability(0.25));
    EXPECT_DOUBLE_EQ(1.0, continuationProbability(1.0));
    EXPECT_DOUBLE_EQ(1.0, continuationProbability(4.0));
  }

  TEST(PathTermination, ClampsCustomMinimumContinuationProbability) {
    EXPECT_DOUBLE_EQ(0.01, continuationProbability(0.01, -1.0));
    EXPECT_DOUBLE_EQ(1.0, continuationProbability(0.01, 2.0));
  }

  TEST(PathTermination, ComputesColorContinuationProbabilityFromMaximumChannel) {
    EXPECT_DOUBLE_EQ(0.4, continuationProbability(Colord(0.1, 0.4, 0.2)));
    EXPECT_DOUBLE_EQ(0.05, continuationProbability(Colord(0.01, 0.02, 0.03)));
    EXPECT_DOUBLE_EQ(1.0, continuationProbability(Colord(0.1, 1.5, 0.2)));
  }

  TEST(PathTermination, ContinuesWhenSampleIsBelowProbability) {
    const PathContinuation continuation = pathContinuation(0.25, 0.249);

    EXPECT_TRUE(continuation.continues);
    EXPECT_DOUBLE_EQ(0.25, continuation.probability);
    EXPECT_DOUBLE_EQ(4.0, continuation.weightScale);
    EXPECT_DOUBLE_EQ(2.0, continuedThroughput(0.5, continuation));
  }

  TEST(PathTermination, TerminatesWhenSampleReachesProbability) {
    const PathContinuation continuation = pathContinuation(0.25, 0.25);

    EXPECT_FALSE(continuation.continues);
    EXPECT_DOUBLE_EQ(0.25, continuation.probability);
    EXPECT_DOUBLE_EQ(0.0, continuation.weightScale);
    EXPECT_DOUBLE_EQ(0.0, continuedThroughput(0.5, continuation));
  }

  TEST(PathTermination, PreservesExpectedScalarThroughput) {
    const double throughput = 0.2;
    const PathContinuation continuation = pathContinuation(throughput, 0.0);

    EXPECT_DOUBLE_EQ(throughput,
                     continuation.probability * continuedThroughput(throughput, continuation));
  }

  TEST(PathTermination, PreservesExpectedScalarThroughputWithMinimumProbabilityClamp) {
    const double throughput = 0.01;
    const PathContinuation continuation = pathContinuation(throughput, 0.0);

    EXPECT_DOUBLE_EQ(0.05, continuation.probability);
    EXPECT_DOUBLE_EQ(throughput,
                     continuation.probability * continuedThroughput(throughput, continuation));
  }

  TEST(PathTermination, PreservesExpectedColorThroughput) {
    const Colord throughput(0.1, 0.2, 0.4);
    const PathContinuation continuation = pathContinuation(throughput, 0.0);
    const Colord weighted = continuedThroughput(throughput, continuation);

    EXPECT_DOUBLE_EQ(0.4, continuation.probability);
    ASSERT_COLOR_NEAR(throughput, weighted * continuation.probability, 1e-12);
  }

  TEST(PathTermination, TerminatedColorThroughputIsBlack) {
    const PathContinuation continuation = pathContinuation(Colord(0.1, 0.2, 0.4), 0.5);

    EXPECT_FALSE(continuation.continues);
    ASSERT_COLOR_NEAR(Colord::black(), continuedThroughput(Colord(0.1, 0.2, 0.4), continuation),
                      1e-12);
  }
}
