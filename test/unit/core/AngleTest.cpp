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
  
  TYPED_TEST(AngleTest, ShouldCreateAngleFromArcMinutes) {
    auto angle = Angle<TypeParam>::fromArcMinutes(5400);
    ASSERT_NEAR(5400, angle.arcMinutes(), 0.0001);
  }
  
  TYPED_TEST(AngleTest, ShouldCreateAngleFromArcSeconds) {
    auto angle = Angle<TypeParam>::fromArcSeconds(324000);
    ASSERT_NEAR(324000, angle.arcSeconds(), 0.0001);
  }
  
  TYPED_TEST(AngleTest, ShouldCreateAngleFromHexacontades) {
    auto angle = Angle<TypeParam>::fromHexacontades(15);
    ASSERT_NEAR(15, angle.hexacontades(), 0.0001);
  }
  
  TYPED_TEST(AngleTest, ShouldCreateAngleFromBinaryDegrees) {
    auto angle = Angle<TypeParam>::fromBinaryDegrees(64);
    ASSERT_NEAR(64, angle.binaryDegrees(), 0.0001);
  }
  
  TYPED_TEST(AngleTest, ShouldCreateAngleFromRadians) {
    auto angle = Angle<TypeParam>::fromRadians(5);
    ASSERT_NEAR(5, angle.radians(), 0.0001);
  }

  TYPED_TEST(AngleTest, ShouldCreateAngleFromGrads) {
    auto angle = Angle<TypeParam>::fromGrads(100);
    ASSERT_NEAR(100, angle.grads(), 0.0001);
  }

  TYPED_TEST(AngleTest, ShouldCreateAngleFromDegrees) {
    auto angle = Angle<TypeParam>::fromDegrees(3);
    ASSERT_NEAR(3, angle.degrees(), 0.0001);
  }

  TYPED_TEST(AngleTest, ShouldCreateAngleFromTurns) {
    auto angle = Angle<TypeParam>::fromTurns(8);
    ASSERT_NEAR(8, angle.turns(), 0.0001);
  }

  TYPED_TEST(AngleTest, ShouldCreateAngleFromQuadrants) {
    auto angle = Angle<TypeParam>::fromQuadrants(1);
    ASSERT_NEAR(1, angle.quadrants(), 0.0001);
  }

  TYPED_TEST(AngleTest, ShouldCreateAngleFromSextants) {
    auto angle = Angle<TypeParam>::fromSextants(2);
    ASSERT_NEAR(2, angle.sextants(), 0.0001);
  }

  TYPED_TEST(AngleTest, ShouldCreateAngleFromClock) {
    auto angle = Angle<TypeParam>::fromClock(3);
    ASSERT_NEAR(3, angle.oclock(), 0.0001);
  }
  
  TYPED_TEST(AngleTest, ShouldCreateAngleFromHours) {
    auto angle = Angle<TypeParam>::fromHours(6);
    ASSERT_NEAR(6, angle.hours(), 0.0001);
  }
  
  TYPED_TEST(AngleTest, ShouldCreateAngleFromCompassPoints) {
    auto angle = Angle<TypeParam>::fromCompassPoints(16);
    ASSERT_NEAR(16, angle.compassPoints(), 0.0001);
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
  
  TEST(AngleTest, ShouldHaveArcMinuteShortCuts) {
    auto anglef = 5 * ArcMinutef;
    ASSERT_NEAR(5, anglef.arcMinutes(), 0.0001);

    auto angled = 5 * ArcMinuted;
    ASSERT_NEAR(5, angled.arcMinutes(), 0.0001);
  }

  TEST(AngleTest, ShouldHaveArcSecondShortCuts) {
    auto anglef = 5 * ArcSecondf;
    ASSERT_NEAR(5, anglef.arcSeconds(), 0.0001);

    auto angled = 5 * ArcSecondd;
    ASSERT_NEAR(5, angled.arcSeconds(), 0.0001);
  }
  
  TEST(AngleTest, ShouldHaveHexacontadeShortCuts) {
    auto anglef = 5 * Hexacontadef;
    ASSERT_NEAR(5, anglef.hexacontades(), 0.0001);

    auto angled = 5 * Hexacontaded;
    ASSERT_NEAR(5, angled.hexacontades(), 0.0001);
  }
  
  TEST(AngleTest, ShouldHaveBinaryDegreeShortCuts) {
    auto anglef = 5 * BinaryDegreef;
    ASSERT_NEAR(5, anglef.binaryDegrees(), 0.0001);

    auto angled = 5 * BinaryDegreed;
    ASSERT_NEAR(5, angled.binaryDegrees(), 0.0001);
  }
  
  TEST(AngleTest, ShouldHaveRadianShortCuts) {
    auto anglef = 5 * Radianf;
    ASSERT_NEAR(5, anglef.radians(), 0.0001);

    auto angled = 5 * Radiand;
    ASSERT_NEAR(5, angled.radians(), 0.0001);
  }
  
  TEST(AngleTest, ShouldHaveGradShortCuts) {
    auto anglef = 5 * Gradf;
    ASSERT_NEAR(5, anglef.grads(), 0.0001);

    auto angled = 5 * Gradd;
    ASSERT_NEAR(5, angled.grads(), 0.0001);
  }
  
  TEST(AngleTest, ShouldHaveTurnShortCuts) {
    auto anglef = 5 * Turnf;
    ASSERT_NEAR(5, anglef.turns(), 0.0001);

    auto angled = 5 * Turnd;
    ASSERT_NEAR(5, angled.turns(), 0.0001);
  }
  
  TEST(AngleTest, ShouldHaveQuadrantShortCuts) {
    auto anglef = 5 * Quadrantf;
    ASSERT_NEAR(5, anglef.quadrants(), 0.0001);

    auto angled = 5 * Quadrantd;
    ASSERT_NEAR(5, angled.quadrants(), 0.0001);
  }
  
  TEST(AngleTest, ShouldHaveSextantShortCuts) {
    auto anglef = 5 * Sextantf;
    ASSERT_NEAR(5, anglef.sextants(), 0.0001);

    auto angled = 5 * Sextantd;
    ASSERT_NEAR(5, angled.sextants(), 0.0001);
  }
  
  TEST(AngleTest, ShouldHaveClockShortCuts) {
    auto anglef = 5 * oClockf;
    ASSERT_NEAR(5, anglef.oclock(), 0.0001);

    auto angled = 5 * oClockd;
    ASSERT_NEAR(5, angled.oclock(), 0.0001);
  }
  
  TEST(AngleTest, ShouldHaveHourShortCuts) {
    auto anglef = 5 * Hourf;
    ASSERT_NEAR(5, anglef.hours(), 0.0001);

    auto angled = 5 * Hourd;
    ASSERT_NEAR(5, angled.hours(), 0.0001);
  }
  
  TEST(AngleTest, ShouldHaveDegreesSuffixOperator) {
    auto anglei = 5_degrees;
    ASSERT_NEAR(5, anglei.degrees(), 0.0001);

    auto angled = 7.1_degrees;
    ASSERT_NEAR(7.1, angled.degrees(), 0.0001);
  }
  
  TEST(AngleTest, ShouldHaveArcMinutesSuffixOperator) {
    auto anglei = 5_arc_minutes;
    ASSERT_NEAR(5, anglei.arcMinutes(), 0.0001);

    auto angled = 7.1_arc_minutes;
    ASSERT_NEAR(7.1, angled.arcMinutes(), 0.0001);
  }
  
  TEST(AngleTest, ShouldHaveArcSecondsSuffixOperator) {
    auto anglei = 5_arc_seconds;
    ASSERT_NEAR(5, anglei.arcSeconds(), 0.0001);

    auto angled = 7.1_arc_seconds;
    ASSERT_NEAR(7.1, angled.arcSeconds(), 0.0001);
  }
  
  TEST(AngleTest, ShouldHaveRadiansSuffixOperator) {
    auto anglei = 5_radians;
    ASSERT_NEAR(5, anglei.radians(), 0.0001);

    auto angled = 7.1_radians;
    ASSERT_NEAR(7.1, angled.radians(), 0.0001);
  }
  
  TEST(AngleTest, ShouldHaveGradsSuffixOperator) {
    auto anglei = 5_grads;
    ASSERT_NEAR(5, anglei.grads(), 0.0001);

    auto angled = 7.1_grads;
    ASSERT_NEAR(7.1, angled.grads(), 0.0001);
  }
  
  TEST(AngleTest, ShouldHaveTurnsSuffixOperator) {
    auto anglei = 5_turns;
    ASSERT_NEAR(5, anglei.turns(), 0.0001);

    auto angled = 7.1_turns;
    ASSERT_NEAR(7.1, angled.turns(), 0.0001);
  }
  
  TEST(AngleTest, ShouldHaveQuadrantsSuffixOperator) {
    auto anglei = 5_quadrants;
    ASSERT_NEAR(5, anglei.quadrants(), 0.0001);

    auto angled = 7.1_quadrants;
    ASSERT_NEAR(7.1, angled.quadrants(), 0.0001);
  }
  
  TEST(AngleTest, ShouldHaveSextantsSuffixOperator) {
    auto anglei = 5_sextants;
    ASSERT_NEAR(5, anglei.sextants(), 0.0001);

    auto angled = 7.1_sextants;
    ASSERT_NEAR(7.1, angled.sextants(), 0.0001);
  }
  
  TEST(AngleTest, ShouldHaveOClockSuffixOperator) {
    auto anglei = 5_oclock;
    ASSERT_NEAR(5, anglei.oclock(), 0.0001);

    auto angled = 7.1_oclock;
    ASSERT_NEAR(7.1, angled.oclock(), 0.0001);
  }

  TEST(AngleTest, ShouldHaveHourSuffixOperator) {
    auto anglei = 5_hours;
    ASSERT_NEAR(5, anglei.hours(), 0.0001);

    auto angled = 7.1_hours;
    ASSERT_NEAR(7.1, angled.hours(), 0.0001);
  }
}
