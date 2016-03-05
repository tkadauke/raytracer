#include "gtest.h"
#include "core/math/Quadric.h"

#include "test/helpers/PolynomialTestHelper.h"

namespace QuadricTest {
  template<class T>
  class QuadricTest : public ::testing::Test {
  };

  typedef ::testing::Types<float, double> QuadricTypes;

  TYPED_TEST_CASE(QuadricTest, QuadricTypes);

  TYPED_TEST(QuadricTest, ShouldInitializeResult) {
    Quadric<TypeParam> quadric(0, 0, 0);
    ASSERT_TRUE(std::isnan(quadric.result()[0]));
    ASSERT_TRUE(std::isnan(quadric.result()[1]));
  }

  TYPED_TEST(QuadricTest, ShouldSolveQuadricWithNoResult) {
    Quadric<TypeParam> quadric(1, 0, 1);
    ASSERT_EQ(0, quadric.solve());
  }

  TYPED_TEST(QuadricTest, ShouldSolveQuadricWithOneResult) {
    Quadric<TypeParam> quadric(1, 0, 0);
    ASSERT_CONTAINERS_NEAR(testing::makeStdVector<TypeParam>(0), quadric.sortedResult(), 0.01);
  }

  TYPED_TEST(QuadricTest, ShouldSolveQuadricWithTwoResults) {
    Quadric<TypeParam> quadric(1, 0, -1);
    ASSERT_EQ(2, quadric.solve());
    ASSERT_CONTAINERS_NEAR(testing::makeStdVector<TypeParam>(-1, 1), quadric.sortedResult(), 0.01);
  }
}
