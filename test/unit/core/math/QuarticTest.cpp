#include <gtest/gtest.h>
#include "core/math/Quartic.h"

#include "test/helpers/PolynomialTestHelper.h"

namespace QuarticTest {
  template<class T>
  class QuarticTest : public ::testing::Test {
  };

  typedef ::testing::Types<float, double> QuarticTypes;

  TYPED_TEST_SUITE(QuarticTest, QuarticTypes);

  TYPED_TEST(QuarticTest, ShouldInitializeResult) {
    Quartic<TypeParam> quartic(0, 0, 0, 0, 0);
    ASSERT_TRUE(std::isnan(quartic.result()[0]));
    ASSERT_TRUE(std::isnan(quartic.result()[1]));
    ASSERT_TRUE(std::isnan(quartic.result()[2]));
    ASSERT_TRUE(std::isnan(quartic.result()[3]));
  }

  TYPED_TEST(QuarticTest, ShouldSolveQuarticWithNoResult) {
    Quartic<TypeParam> quartic(1, 0, 0, 0, 1);
    ASSERT_EQ(0, quartic.solve());
  }

  TYPED_TEST(QuarticTest, ShouldSolveQuarticWithOneResult) {
    Quartic<TypeParam> quartic(1, 0, 0, 0, 0);
    ASSERT_CONTAINERS_NEAR(testing::makeStdVector<TypeParam>(0), quartic.sortedResult(), 0.01);
  }

  TYPED_TEST(QuarticTest, ShouldSolveQuarticWithTwoResults) {
    Quartic<TypeParam> quartic(1, 0, 0, 0, -1);
    ASSERT_CONTAINERS_NEAR(testing::makeStdVector<TypeParam>(-1, 1), quartic.sortedResult(), 0.01);
  }

  TYPED_TEST(QuarticTest, ShouldSolveAnotherQuarticWithTwoResults) {
    Quartic<TypeParam> quartic(1, 0, -2, 0, 1);
    ASSERT_CONTAINERS_NEAR(testing::makeStdVector<TypeParam>(-1, 1), quartic.sortedResult(), 0.01);
  }

  TYPED_TEST(QuarticTest, ShouldSolveQuarticWithThreeResults) {
    Quartic<TypeParam> quartic(1, 0, -1, 0, 0);
    ASSERT_CONTAINERS_NEAR(testing::makeStdVector<TypeParam>(-1, 0, 1), quartic.sortedResult(), 0.01);
  }

  TYPED_TEST(QuarticTest, ShouldSolveAnotherQuarticWithThreeResults) {
    Quartic<TypeParam> quartic(1, 1, -3, -1, 2);
    ASSERT_CONTAINERS_NEAR(testing::makeStdVector<TypeParam>(-2, -1, 1), quartic.sortedResult(), 0.01);
  }

  TYPED_TEST(QuarticTest, ShouldSolveQuarticWithFourResults) {
    Quartic<TypeParam> quartic(4, 0, -5, 0, 1);
    ASSERT_CONTAINERS_NEAR(testing::makeStdVector<TypeParam>(-1, -0.5, 0.5, 1), quartic.sortedResult(), 0.01);
  }

  TYPED_TEST(QuarticTest, ShouldSolveAnotherQuarticWithFourResults) {
    Quartic<TypeParam> quartic(1, -16, 86, -176, 105);
    ASSERT_CONTAINERS_NEAR(testing::makeStdVector<TypeParam>(1, 3, 5, 7), quartic.sortedResult(), 0.01);
  }

  TYPED_TEST(QuarticTest, ShouldSolveIllConditionedGrazingQuartic) {
    // This is the PolynomialBenchmark grazing-incidence case. Ferrari's
    // closed-form path used to report 0 roots for float and a spurious
    // -0.298... root for double; the stable fallback finds the two real roots.
    Quartic<TypeParam> quartic(TypeParam(1e-6), TypeParam(1), TypeParam(-2), TypeParam(1), TypeParam(-1));
    ASSERT_CONTAINERS_NEAR(
      testing::makeStdVector<TypeParam>(TypeParam(-1000002), TypeParam(1.7548747)),
      quartic.stableSortedResult(),
      TypeParam(2));
  }

  TYPED_TEST(QuarticTest, ShouldSolveQuarticWithWideCoefficientScale) {
    // Wide coefficient ranges are common in grazing geometry after expansion.
    // Keeping the real-axis isolation fallback on this path preserves the
    // root count while avoiding closed-form cancellation.
    Quartic<TypeParam> quartic(
      TypeParam(1),
      TypeParam(-10000),
      TypeParam(-7),
      TypeParam(70006),
      TypeParam(-60000));

    ASSERT_CONTAINERS_NEAR(
      testing::makeStdVector<TypeParam>(
        TypeParam(-3), TypeParam(1), TypeParam(2), TypeParam(10000)),
      quartic.stableSortedResult(),
      TypeParam(0.01));
  }
}
