#include "gtest.h"
#include "core/math/Angle.h"

#include <sstream>

using namespace std;

namespace AngleTest {
  template<class T>
  class AngleTest : public ::testing::Test {
  };

  typedef ::testing::Types<float, double> AngleTypes;

  TYPED_TEST_CASE(AngleTest, AngleTypes);

  TYPED_TEST(AngleTest, ShouldCreateDefaultAngle) {
    auto angle = Angle<TypeParam>();
    ASSERT_EQ(0, angle.radians());
  }
  
  TYPED_TEST(AngleTest, ShouldCopyAngle) {
    auto angle = Angle<TypeParam>::fromDegrees(90);
    Angle<TypeParam> copy(angle);
    ASSERT_NEAR(90, copy.degrees(), 0.0001);
  }
  
  TYPED_TEST(AngleTest, ShouldCreateAngleFromRadians) {
    auto angle = Angle<TypeParam>::fromRadians(5);
    ASSERT_NEAR(5, angle.radians(), 0.0001);
  }

  TYPED_TEST(AngleTest, ShouldCreateAngleFromDegrees) {
    auto angle = Angle<TypeParam>::fromDegrees(3);
    ASSERT_NEAR(3, angle.degrees(), 0.0001);
  }

  TYPED_TEST(AngleTest, ShouldCreateAngleFromTurns) {
    auto angle = Angle<TypeParam>::fromTurns(8);
    ASSERT_NEAR(8, angle.turns(), 0.0001);
  }

  TYPED_TEST(AngleTest, ShouldCreateAngleFromClock) {
    auto angle = Angle<TypeParam>::fromClock(3);
    ASSERT_NEAR(3, angle.oclock(), 0.0001);
  }
  
  TYPED_TEST(AngleTest, ShouldConvertBetweenUnits) {
    auto angle = Angle<TypeParam>::fromDegrees(90);
    ASSERT_NEAR(1.5707963267949, angle.radians(), 0.0001);
    ASSERT_NEAR(0.25, angle.turns(), 0.0001);
  }
  
  TYPED_TEST(AngleTest, ShouldAddTwoAngles) {
    auto angle1 = Angle<TypeParam>::fromDegrees(90);
    auto angle2 = Angle<TypeParam>::fromDegrees(45);
    ASSERT_NEAR(135, (angle1 + angle2).degrees(), 0.0001);
  }
  
  TYPED_TEST(AngleTest, ShouldSubtractTwoAngles) {
    auto angle1 = Angle<TypeParam>::fromDegrees(90);
    auto angle2 = Angle<TypeParam>::fromDegrees(45);
    ASSERT_NEAR(45, (angle1 - angle2).degrees(), 0.0001);
  }
  
  TYPED_TEST(AngleTest, ShouldNegateAngle) {
    auto angle = Angle<TypeParam>::fromDegrees(90);
    ASSERT_NEAR(-90, -angle.degrees(), 0.0001);
  }
  
  TYPED_TEST(AngleTest, ShouldMultiplyAngleByScalar) {
    auto angle = Angle<TypeParam>::fromDegrees(90);
    ASSERT_NEAR(180, (angle * 2).degrees(), 0.0001);
  }
  
  TYPED_TEST(AngleTest, ShouldMultiplyScalarByAngle) {
    auto angle = Angle<TypeParam>::fromDegrees(90);
    ASSERT_NEAR(180, (2 * angle).degrees(), 0.0001);
  }
  
  TYPED_TEST(AngleTest, ShouldCompareAngles) {
    auto angle1 = Angle<TypeParam>::fromDegrees(90),
         angle2 = Angle<TypeParam>::fromDegrees(90);
    ASSERT_TRUE(angle1 == angle2);
  }
  
  TYPED_TEST(AngleTest, ShouldCompareAnglesForInequality) {
    auto angle1 = Angle<TypeParam>::fromDegrees(90),
         angle2 = Angle<TypeParam>::fromDegrees(180);
    ASSERT_TRUE(angle1 != angle2);
  }
  
  TYPED_TEST(AngleTest, ShouldStreamToString) {
    Angle<TypeParam> angle = Angle<TypeParam>::fromRadians(1.5);
    ostringstream str;
    str << angle;
    ASSERT_EQ("1.5", str.str());
  }
  
  TEST(AngleTest, ShouldHaveDegreeShortCuts) {
    auto anglef = 5 * Degreef;
    ASSERT_NEAR(5, anglef.degrees(), 0.0001);

    auto angled = 5 * Degreed;
    ASSERT_NEAR(5, angled.degrees(), 0.0001);
  }
  
  TEST(AngleTest, ShouldHaveRadianShortCuts) {
    auto anglef = 5 * Radianf;
    ASSERT_NEAR(5, anglef.radians(), 0.0001);

    auto angled = 5 * Radiand;
    ASSERT_NEAR(5, angled.radians(), 0.0001);
  }
  
  TEST(AngleTest, ShouldHaveTurnShortCuts) {
    auto anglef = 5 * Turnf;
    ASSERT_NEAR(5, anglef.turns(), 0.0001);

    auto angled = 5 * Turnd;
    ASSERT_NEAR(5, angled.turns(), 0.0001);
  }
  
  TEST(AngleTest, ShouldHaveClockShortCuts) {
    auto anglef = 5 * oClockf;
    ASSERT_NEAR(5, anglef.oclock(), 0.0001);

    auto angled = 5 * oClockd;
    ASSERT_NEAR(5, angled.oclock(), 0.0001);
  }
  
  TEST(AngleTest, ShouldHaveDegreesSuffixOperator) {
    auto anglei = 5_degrees;
    ASSERT_NEAR(5, anglei.degrees(), 0.0001);

    auto angled = 7.1_degrees;
    ASSERT_NEAR(7.1, angled.degrees(), 0.0001);
  }
  
  TEST(AngleTest, ShouldHaveRadiansSuffixOperator) {
    auto anglei = 5_radians;
    ASSERT_NEAR(5, anglei.radians(), 0.0001);

    auto angled = 7.1_radians;
    ASSERT_NEAR(7.1, angled.radians(), 0.0001);
  }
  
  TEST(AngleTest, ShouldHaveTurnsSuffixOperator) {
    auto anglei = 5_turns;
    ASSERT_NEAR(5, anglei.turns(), 0.0001);

    auto angled = 7.1_turns;
    ASSERT_NEAR(7.1, angled.turns(), 0.0001);
  }
  
  TEST(AngleTest, ShouldHaveOClockSuffixOperator) {
    auto anglei = 5_oclock;
    ASSERT_NEAR(5, anglei.oclock(), 0.0001);

    auto angled = 7.1_oclock;
    ASSERT_NEAR(7.1, angled.oclock(), 0.0001);
  }
}
