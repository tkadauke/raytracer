#include "gtest.h"
#include "math/Quadric.h"

namespace QuadricTest {
  template<class T>
  class QuadricTest : public ::testing::Test {
  };

  typedef ::testing::Types<Quadric<float>, Quadric<double> > QuadricTypes;

  TYPED_TEST_CASE(QuadricTest, QuadricTypes);

  TYPED_TEST(QuadricTest, ShouldInitializeResult) {
    TypeParam quadric(0, 0, 0);
    ASSERT_TRUE(std::isnan(quadric.result()[0]));
    ASSERT_TRUE(std::isnan(quadric.result()[1]));
  }

  TYPED_TEST(QuadricTest, ShouldSolveQuadricWithNoResult) {
    TypeParam quadric(1, 0, 1);
    ASSERT_EQ(0, quadric.solve());
  }

  TYPED_TEST(QuadricTest, ShouldSolveQuadricWithOneResult) {
    TypeParam quadric(1, 0, 0);
    ASSERT_EQ(1, quadric.solve());
    ASSERT_EQ(0, quadric.result()[0]);
  }

  TYPED_TEST(QuadricTest, ShouldSolveQuadricWithTwoResults) {
    TypeParam quadric(1, 0, -1);
    ASSERT_EQ(2, quadric.solve());
    ASSERT_EQ(1, quadric.result()[0]);
    ASSERT_EQ(-1, quadric.result()[1]);
  }
}
