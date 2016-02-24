#include "gtest.h"
#include "core/math/Number.h"

namespace NumberTest {
  template<class T>
  class NumberTest : public ::testing::Test {
  };

  typedef ::testing::Types<float, double> NumberTypes;

  TYPED_TEST_CASE(NumberTest, NumberTypes);

  TYPED_TEST(NumberTest, ShouldReturnTrueIfNumbersAreEqual) {
    TypeParam number = 1.5, value = 1.5;
    ASSERT_TRUE(isAlmost(number, value));
  }

  TYPED_TEST(NumberTest, ShouldReturnFalseIfNumbersAreNotEqual) {
    TypeParam number = 4, value = 37;
    ASSERT_FALSE(isAlmost(number, value));
  }

  TYPED_TEST(NumberTest, ShouldReturnTrueIfNumberIsZero) {
    TypeParam number = 0;
    ASSERT_TRUE(isAlmostZero(number));
  }

  TYPED_TEST(NumberTest, ShouldReturnFalseIfNumberIsNotZero) {
    TypeParam number = 4;
    ASSERT_FALSE(isAlmostZero(number));
  }

  TEST(NumberTest, ShouldReturnRandomIntegerWithinRange) {
    int number = randomInt(1337);
    ASSERT_TRUE(number >= 0);
    ASSERT_TRUE(number < 1337);
  }
}
