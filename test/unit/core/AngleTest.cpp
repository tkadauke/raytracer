#include "gtest.h"
#include "core/math/Angle.h"

#include <sstream>

using namespace std;

namespace AngleTest {
  template<class T>
  class AngleTest : public ::testing::Test {
  };

  typedef ::testing::Types<Anglef, Angled> AngleTypes;

  TYPED_TEST_CASE(AngleTest, AngleTypes);

  TYPED_TEST(AngleTest, ShouldCreateDefaultAngle) {
    auto angle = TypeParam();
    ASSERT_EQ(0, angle.radians());
  }
  
  TYPED_TEST(AngleTest, ShouldCopyAngle) {
    auto angle = TypeParam::fromDegrees(90);
    TypeParam copy(angle);
    ASSERT_NEAR(90, copy.degrees(), 0.0001);
  }
  
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
  
  TYPED_TEST(AngleTest, ShouldMultiplyAngleByScalar) {
    auto angle = TypeParam::fromDegrees(90);
    ASSERT_NEAR(180, (angle * 2).degrees(), 0.0001);
  }
  
  TYPED_TEST(AngleTest, ShouldMultiplyScalarByAngle) {
    auto angle = TypeParam::fromDegrees(90);
    ASSERT_NEAR(180, (2 * angle).degrees(), 0.0001);
  }
  
  TYPED_TEST(AngleTest, ShouldCompareAngles) {
    auto angle1 = TypeParam::fromDegrees(90),
         angle2 = TypeParam::fromDegrees(90);
    ASSERT_TRUE(angle1 == angle2);
  }
  
  TYPED_TEST(AngleTest, ShouldCompareAnglesForInequality) {
    auto angle1 = TypeParam::fromDegrees(90),
         angle2 = TypeParam::fromDegrees(180);
    ASSERT_TRUE(angle1 != angle2);
  }
  
  TYPED_TEST(AngleTest, ShouldStreamToString) {
    TypeParam angle = TypeParam::fromRadians(1.5);
    ostringstream str;
    str << angle;
    ASSERT_EQ("1.5", str.str());
  }
}
