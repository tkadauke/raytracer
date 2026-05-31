#include <gtest/gtest.h>
#include "render/MIS.h"

#include "test/helpers/ColorTestHelper.h"

namespace MISTest {
  TEST(MIS, BalanceHeuristicWeightsPdfProportionally) {
    EXPECT_DOUBLE_EQ(0.25, render::mis::balanceHeuristic(0.25, 0.75));
    EXPECT_DOUBLE_EQ(0.75, render::mis::balanceHeuristic(0.75, 0.25));
  }

  TEST(MIS, BalanceHeuristicAccountsForSampleCounts) {
    EXPECT_DOUBLE_EQ(0.5, render::mis::balanceHeuristic(3, 0.25, 1, 0.75));
  }

  TEST(MIS, PowerHeuristicSquaresPdfProducts) {
    EXPECT_DOUBLE_EQ(0.2, render::mis::powerHeuristic(0.25, 0.5));
    EXPECT_DOUBLE_EQ(0.8, render::mis::powerHeuristic(0.5, 0.25));
  }

  TEST(MIS, HeuristicsIgnoreNonPositivePdfValues) {
    EXPECT_DOUBLE_EQ(0.0, render::mis::balanceHeuristic(-1.0, 0.0));
    EXPECT_DOUBLE_EQ(1.0, render::mis::powerHeuristic(0.5, -2.0));
  }

  TEST(MIS, LightAndBsdfSamplesSplitEvenlyWhenPdfsMatch) {
    const Colord bsdf(0.8, 0.4, 0.2);
    const Colord radiance(10.0, 10.0, 10.0);
    const double cosine = 0.5;
    const double pdf = 0.25;

    const Colord lightEstimate =
      render::mis::estimateDirectLightingFromLightSample(bsdf, radiance, cosine, pdf, pdf);
    const Colord bsdfEstimate =
      render::mis::estimateDirectLightingFromBsdfSample(bsdf, radiance, cosine, pdf, pdf);
    const Colord combined = lightEstimate + bsdfEstimate;

    ASSERT_COLOR_NEAR(Colord(8.0, 4.0, 2.0), lightEstimate, 1e-12);
    ASSERT_COLOR_NEAR(lightEstimate, bsdfEstimate, 1e-12);
    ASSERT_COLOR_NEAR(Colord(16.0, 8.0, 4.0), combined, 1e-12);
  }

  TEST(MIS, DirectLightingEstimatorCombinesBsdfAndLightPdfsWithPowerHeuristic) {
    const Colord bsdf(0.25, 0.5, 1.0);
    const Colord radiance(4.0, 2.0, 1.0);
    const double cosine = 0.75;

    const Colord lightEstimate =
      render::mis::estimateDirectLightingFromLightSample(bsdf, radiance, cosine, 0.5, 0.25);
    const Colord bsdfEstimate =
      render::mis::estimateDirectLightingFromBsdfSample(bsdf, radiance, cosine, 0.25, 0.5);

    ASSERT_COLOR_NEAR(Colord(1.2, 1.2, 1.2), lightEstimate, 1e-12);
    ASSERT_COLOR_NEAR(Colord(0.6, 0.6, 0.6), bsdfEstimate, 1e-12);
  }

  TEST(MIS, DirectLightingEstimatorSupportsBalanceHeuristic) {
    const Colord estimate = render::mis::estimateDirectLightingFromLightSample(
      Colord(0.25, 0.5, 1.0), Colord(4.0, 2.0, 1.0), 0.75, 0.5, 0.25, false,
      render::mis::Heuristic::Balance);

    ASSERT_COLOR_NEAR(Colord(1.0, 1.0, 1.0), estimate, 1e-12);
  }

  TEST(MIS, DirectLightingEstimatorDoesNotMisWeightDeltaSamples) {
    const Colord estimate = render::mis::estimateDirectLightingFromLightSample(
      Colord(0.25, 0.5, 1.0), Colord(4.0, 2.0, 1.0), 0.75, 1.0, 0.5, true);

    ASSERT_COLOR_NEAR(Colord(0.75, 0.75, 0.75), estimate, 1e-12);
  }

  TEST(MIS, DirectLightingEstimatorRejectsInvalidSamples) {
    const Colord bsdf(1.0, 1.0, 1.0);
    const Colord radiance(2.0, 2.0, 2.0);

    ASSERT_EQ(Colord::black(),
              render::mis::estimateDirectLightingFromLightSample(bsdf, radiance, 0.0, 0.5, 0.5));
    ASSERT_EQ(Colord::black(),
              render::mis::estimateDirectLightingFromLightSample(bsdf, radiance, 0.5, 0.0, 0.5));
  }
}
