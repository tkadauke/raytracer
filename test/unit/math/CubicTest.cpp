#include "gtest.h"
#include "math/Cubic.h"

namespace CubicTest {
  template<class T>
  class CubicTest : public ::testing::Test {
  };

  typedef ::testing::Types<Cubic<float>, Cubic<double> > CubicTypes;

  TYPED_TEST_CASE(CubicTest, CubicTypes);

  TYPED_TEST(CubicTest, ShouldInitializeResult) {
    TypeParam Cubic(0, 0, 0, 0);
    ASSERT_TRUE(std::isnan(Cubic.result()[0]));
    ASSERT_TRUE(std::isnan(Cubic.result()[1]));
    ASSERT_TRUE(std::isnan(Cubic.result()[2]));
    ASSERT_TRUE(std::isnan(Cubic.result()[3]));
  }

  TYPED_TEST(CubicTest, ShouldSolveCubicWithOneResult) {
    TypeParam Cubic(1, 0, 0, 0);
    ASSERT_EQ(1, Cubic.solve());
    ASSERT_NEAR(0, Cubic.result()[0], 0.0001);
  }

  TYPED_TEST(CubicTest, ShouldSolveCubicWithTwoResults) {
    TypeParam Cubic(1, 1, 0, 0);
    ASSERT_EQ(2, Cubic.solve());
    ASSERT_NEAR(-1, Cubic.result()[0], 0.0001);
    ASSERT_NEAR(0, Cubic.result()[1], 0.0001);
  }

  TYPED_TEST(CubicTest, ShouldSolveCubicWithThreeResults) {
    TypeParam Cubic(1, 0, -1, 0);
    ASSERT_EQ(3, Cubic.solve());
    ASSERT_NEAR(1, Cubic.result()[0], 0.0001);
    ASSERT_NEAR(0, Cubic.result()[1], 0.0001);
    ASSERT_NEAR(-1, Cubic.result()[2], 0.0001);
  }
}
