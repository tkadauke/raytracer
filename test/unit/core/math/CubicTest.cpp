#include "gtest.h"
#include "core/math/Cubic.h"

#include "test/helpers/PolynomialTestHelper.h"

namespace CubicTest {
  template<class T>
  class CubicTest : public ::testing::Test {
  };

  typedef ::testing::Types<float, double> CubicTypes;

  TYPED_TEST_CASE(CubicTest, CubicTypes);

  TYPED_TEST(CubicTest, ShouldInitializeResult) {
    Cubic<TypeParam> cubic(0, 0, 0, 0);
    ASSERT_TRUE(std::isnan(cubic.result()[0]));
    ASSERT_TRUE(std::isnan(cubic.result()[1]));
    ASSERT_TRUE(std::isnan(cubic.result()[2]));
  }

  TYPED_TEST(CubicTest, ShouldSolveCubicWithOneResult) {
    Cubic<TypeParam> cubic(1, 0, 0, 0);
    ASSERT_CONTAINERS_NEAR(testing::makeStdVector<TypeParam>(0), cubic.sortedResult(), 0.01);
  }

  TYPED_TEST(CubicTest, ShouldSolveCubicWithTwoResults) {
    Cubic<TypeParam> cubic(1, 1, 0, 0);
    ASSERT_EQ(2, cubic.solve());
    ASSERT_CONTAINERS_NEAR(testing::makeStdVector<TypeParam>(-1, 0), cubic.sortedResult(), 0.01);
  }

  TYPED_TEST(CubicTest, ShouldSolveCubicWithThreeResults) {
    Cubic<TypeParam> cubic(1, 0, -1, 0);
    ASSERT_EQ(3, cubic.solve());
    ASSERT_CONTAINERS_NEAR(testing::makeStdVector<TypeParam>(-1, 0, 1), cubic.sortedResult(), 0.01);
  }
}
