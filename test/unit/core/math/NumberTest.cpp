#include <gtest/gtest.h>
#include <array>
#include <thread>

#include "core/math/Number.h"

namespace NumberTest {
  template<class T>
  class NumberTest : public ::testing::Test {};

  typedef ::testing::Types<float, double> NumberTypes;

  TYPED_TEST_SUITE(NumberTest, NumberTypes);

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

  TEST(NumberTest, ShouldReturnRandomDoubleWithinRange) {
    double number = random(4.5, 13.1);
    ASSERT_TRUE(number >= 4.5);
    ASSERT_TRUE(number < 13.1);
  }

  TEST(NumberTest, ShouldReturnRandomDoubleWithUpperLimit) {
    double number = random(13.1);
    ASSERT_TRUE(number >= 0);
    ASSERT_TRUE(number < 13.1);
  }

  TEST(NumberTest, ShouldReturnRandomIntegerWithUpperLimit) {
    int number = random(1337);
    ASSERT_TRUE(number >= 0);
    ASSERT_TRUE(number < 1337);
  }

  TEST(NumberTest, SameSeedProducesSameIntegerSequence) {
    seed(12345);
    std::array<int, 8> first{};
    for (int& value : first)
      value = random(1000000);

    seed(12345);
    std::array<int, 8> second{};
    for (int& value : second)
      value = random(1000000);

    EXPECT_EQ(first, second);
  }

  TEST(NumberTest, SameSeedProducesSameFloatingPointSequence) {
    seed(98765);
    std::array<double, 8> first{};
    for (double& value : first)
      value = random(-10.0, 10.0);

    seed(98765);
    std::array<double, 8> second{};
    for (double& value : second)
      value = random(-10.0, 10.0);

    EXPECT_EQ(first, second);
  }

  TYPED_TEST(NumberTest, PositiveExtentOfPositiveNumberIsUnchanged) {
    TypeParam value = 4.5;
    ASSERT_EQ(positiveExtent(value), value);
  }

  TYPED_TEST(NumberTest, PositiveExtentOfNegativeNumberReturnsAbsoluteValue) {
    TypeParam value = -3.0;
    ASSERT_EQ(positiveExtent(value), TypeParam(3.0));
  }

  TYPED_TEST(NumberTest, PositiveExtentOfZeroReturnsEpsilon) {
    TypeParam value = 0;
    ASSERT_EQ(positiveExtent(value), std::numeric_limits<TypeParam>::epsilon());
  }

  TEST(NumberTest, SeedingOneThreadDoesNotPerturbAnotherThread) {
    seed(123);
    int first = random(1000000);

    std::array<int, 4> workerValues{};
    std::thread worker([&workerValues]() {
      seed(456);
      for (int& value : workerValues)
        value = random(1000000);
    });
    worker.join();

    int second = random(1000000);

    seed(123);
    EXPECT_EQ(first, random(1000000));
    EXPECT_EQ(second, random(1000000));

    std::array<int, 4> repeatedWorkerValues{};
    std::thread repeatedWorker([&repeatedWorkerValues]() {
      seed(456);
      for (int& value : repeatedWorkerValues)
        value = random(1000000);
    });
    repeatedWorker.join();

    EXPECT_EQ(workerValues, repeatedWorkerValues);
  }
}
