#include "gtest.h"
#include "math/Quartic.h"

#include "test/helpers/PolynomialTestHelper.h"

namespace QuarticTest {
  template<class T>
  class QuarticTest : public ::testing::Test {
  };

  typedef ::testing::Types<Quartic<float>, Quartic<double> > QuarticTypes;

  TYPED_TEST_CASE(QuarticTest, QuarticTypes);

  TYPED_TEST(QuarticTest, ShouldInitializeResult) {
    TypeParam quartic(0, 0, 0, 0, 0);
    ASSERT_TRUE(std::isnan(quartic.result()[0]));
    ASSERT_TRUE(std::isnan(quartic.result()[1]));
    ASSERT_TRUE(std::isnan(quartic.result()[2]));
    ASSERT_TRUE(std::isnan(quartic.result()[3]));
  }

  TYPED_TEST(QuarticTest, ShouldSolveQuarticWithNoResult) {
    TypeParam quartic(1, 0, 0, 0, 1);
    ASSERT_EQ(0, quartic.solve());
  }

  TYPED_TEST(QuarticTest, ShouldSolveQuarticWithOneResult) {
    TypeParam quartic(1, 0, 0, 0, 0);
    ASSERT_CONTAINERS_NEAR(testing::makeStdVector<typename TypeParam::Coefficient>(0), quartic.sortedResult(), 0.01);
  }

  TYPED_TEST(QuarticTest, ShouldSolveQuarticWithTwoResults) {
    TypeParam quartic(1, 0, 0, 0, -1);
    ASSERT_CONTAINERS_NEAR(testing::makeStdVector<typename TypeParam::Coefficient>(-1, 1), quartic.sortedResult(), 0.01);
  }

  TYPED_TEST(QuarticTest, ShouldSolveAnotherQuarticWithTwoResults) {
    TypeParam quartic(1, 0, -2, 0, 1);
    ASSERT_CONTAINERS_NEAR(testing::makeStdVector<typename TypeParam::Coefficient>(-1, 1), quartic.sortedResult(), 0.01);
  }

  TYPED_TEST(QuarticTest, ShouldSolveQuarticWithThreeResults) {
    TypeParam quartic(1, 0, -1, 0, 0);
    ASSERT_CONTAINERS_NEAR(testing::makeStdVector<typename TypeParam::Coefficient>(-1, 0, 1), quartic.sortedResult(), 0.01);
  }

  TYPED_TEST(QuarticTest, ShouldSolveAnotherQuarticWithThreeResults) {
    TypeParam quartic(1, 1, -3, -1, 2);
    ASSERT_CONTAINERS_NEAR(testing::makeStdVector<typename TypeParam::Coefficient>(-2, -1, 1), quartic.sortedResult(), 0.01);
  }

  TYPED_TEST(QuarticTest, ShouldSolveQuarticWithFourResults) {
    TypeParam quartic(4, 0, -5, 0, 1);
    ASSERT_CONTAINERS_NEAR(testing::makeStdVector<typename TypeParam::Coefficient>(-1, -0.5, 0.5, 1), quartic.sortedResult(), 0.01);
  }
}
