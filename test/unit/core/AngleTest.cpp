#include "gtest.h"
#include "core/math/Angle.h"

namespace AngleTest {
  template<class T>
  class AngleTest : public ::testing::Test {
  };

  typedef ::testing::Types<Anglef, Angled> AngleTypes;

  TYPED_TEST_CASE(AngleTest, AngleTypes);

  TYPED_TEST(AngleTest, ShouldCreateAngleFromRadians) {
    auto angle = TypeParam::fromRadians(5);
    ASSERT_NEAR(5, angle.radians(), 0.0001);
  }

  TYPED_TEST(AngleTest, ShouldCreateAngleFromDegrees) {
    auto angle = TypeParam::fromDegrees(3);
    ASSERT_NEAR(3, angle.degrees(), 0.0001);
  }

  TYPED_TEST(AngleTest, ShouldCreateAngleFromTurns) {
    auto angle = TypeParam::fromTurns(8);
    ASSERT_NEAR(8, angle.turns(), 0.0001);
  }
  
  TYPED_TEST(AngleTest, ShouldConvertBetweenUnits) {
    auto angle = TypeParam::fromDegrees(90);
    ASSERT_NEAR(1.5707963267949, angle.radians(), 0.0001);
    ASSERT_NEAR(0.25, angle.turns(), 0.0001);
  }
  
  TYPED_TEST(AngleTest, ShouldAddTwoAngles) {
    auto angle1 = TypeParam::fromDegrees(90);
    auto angle2 = TypeParam::fromDegrees(45);
    ASSERT_NEAR(135, (angle1 + angle2).degrees(), 0.0001);
  }
  
  TYPED_TEST(AngleTest, ShouldSubtractTwoAngles) {
    auto angle1 = TypeParam::fromDegrees(90);
    auto angle2 = TypeParam::fromDegrees(45);
    ASSERT_NEAR(45, (angle1 - angle2).degrees(), 0.0001);
  }
  
  TYPED_TEST(AngleTest, ShouldNegateAngle) {
    auto angle = TypeParam::fromDegrees(90);
    ASSERT_NEAR(-90, -angle.degrees(), 0.0001);
  }
}
