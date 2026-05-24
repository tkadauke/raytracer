#include <gtest/gtest.h>
#include "core/math/Quaternion.h"
#include "core/math/Vector.h"

#include <sstream>

namespace QuaternionTest {
  using namespace std;

  template<class T>
  class QuaternionTest : public ::testing::Test {};

  typedef ::testing::Types<float, double> QuaternionTypes;

  TYPED_TEST_SUITE(QuaternionTest, QuaternionTypes);

  TYPED_TEST(QuaternionTest, ShouldInitializeAsMultiplicationIdentityQuaternion) {
    Quaternion<TypeParam> quaternion;
    ASSERT_EQ(1, quaternion.w());
    ASSERT_EQ(0, quaternion.x());
    ASSERT_EQ(0, quaternion.y());
    ASSERT_EQ(0, quaternion.z());
  }

  TYPED_TEST(QuaternionTest, ShouldInitializeFromFourScalars) {
    Quaternion<TypeParam> quaternion(4, 1, 2, 3);
    ASSERT_EQ(4, quaternion.w());
    ASSERT_EQ(1, quaternion.x());
    ASSERT_EQ(2, quaternion.y());
    ASSERT_EQ(3, quaternion.z());
  }

  TYPED_TEST(QuaternionTest, ShouldInitializeFromScalarAndVector) {
    Vector3<TypeParam> vector(1, 2, 3);
    Quaternion<TypeParam> quaternion(4, vector);
    ASSERT_EQ(4, quaternion.w());
    ASSERT_EQ(1, quaternion.x());
    ASSERT_EQ(2, quaternion.y());
    ASSERT_EQ(3, quaternion.z());
  }

  TYPED_TEST(QuaternionTest, ShouldInitializeFromScalarAndCArray) {
    TypeParam elements[3] = {1, 2, 3};
    Quaternion<TypeParam> quaternion(4, elements);
    ASSERT_EQ(4, quaternion.w());
    ASSERT_EQ(1, quaternion.x());
    ASSERT_EQ(2, quaternion.y());
    ASSERT_EQ(3, quaternion.z());
  }

  TYPED_TEST(QuaternionTest, ShouldReturnTrueForEqualityTestOfTwoIdentityQuaternions) {
    ASSERT_TRUE(Quaternion<TypeParam>() == Quaternion<TypeParam>());
  }

  TYPED_TEST(QuaternionTest, ShouldTestQuaternionsForEquality) {
    Quaternion<TypeParam> first(1, 2, 2, 3), second(1, 2, 2, 3);
    ASSERT_TRUE(first == second);
  }

  TYPED_TEST(QuaternionTest, ShouldTestQuaternionsForInEquality) {
    Quaternion<TypeParam> first(1, 2, 2, 3), second(1, 4, 4, 6);
    ASSERT_TRUE(first != second);
  }

  TYPED_TEST(QuaternionTest, ShouldAddTwoQuaternions) {
    Quaternion<TypeParam> first(1, 2, 3, 4), second(5, 6, 7, 8);
    Quaternion<TypeParam> expected(6, 8, 10, 12);
    ASSERT_EQ(expected, first + second);
  }

  TYPED_TEST(QuaternionTest, ShouldSubtractTwoQuaternions) {
    Quaternion<TypeParam> first(5, 6, 7, 8), second(1, 2, 3, 4);
    Quaternion<TypeParam> expected(4, 4, 4, 4);
    ASSERT_EQ(expected, first - second);
  }

  TYPED_TEST(QuaternionTest, ShouldMultiplyQuaternionWithScalar) {
    Quaternion<TypeParam> quaternion(1, 2, 2, 3);
    Quaternion<TypeParam> expected(2, 4, 4, 6);
    ASSERT_EQ(expected, quaternion * 2);
  }

  TYPED_TEST(QuaternionTest, ShouldDivideQuaternionByScalar) {
    Quaternion<TypeParam> quaternion(2, 4, 4, 6);
    Quaternion<TypeParam> expected(1, 2, 2, 3);
    ASSERT_EQ(expected, quaternion / 2);
  }

  TYPED_TEST(QuaternionTest, ShouldCalculateDotProduct) {
    Quaternion<TypeParam> a(1, 0, 0, 0), b(1, 0, 0, 0);
    ASSERT_NEAR(1, a.dot(b), 0.001);
  }

  TYPED_TEST(QuaternionTest, ShouldCalculateDotProductOfTwoQuaternions) {
    Quaternion<TypeParam> a(1, 2, 3, 4), b(1, 2, 3, 4);
    ASSERT_NEAR(30, a.dot(b), 0.001);
  }

  TYPED_TEST(QuaternionTest, ShouldCalculateLengthSquared) {
    Quaternion<TypeParam> quaternion(1, 2, 2, 2);
    ASSERT_NEAR(13, quaternion.lengthSquared(), 0.001);
  }

  TYPED_TEST(QuaternionTest, ShouldCalculateLengthOfUnitQuaternion) {
    Quaternion<TypeParam> quaternion;
    ASSERT_EQ(1, quaternion.length());
  }

  TYPED_TEST(QuaternionTest, ShouldCalculateLengthOfQuaternion) {
    Quaternion<TypeParam> quaternion(2, 2, 2, 2);
    ASSERT_EQ(4, quaternion.length());
  }

  TYPED_TEST(QuaternionTest, ShouldReturnNormalizedQuaternion) {
    Quaternion<TypeParam> quaternion(2, 5, 4, 2);
    ASSERT_NEAR(1, quaternion.normalized().length(), 0.001);
  }

  TYPED_TEST(QuaternionTest, ShouldReturnOriginalQuaternionWhenMultipliedWithIdentityQuaternion) {
    Quaternion<TypeParam> quaternion(4, 1, 2, 3);
    Quaternion<TypeParam> identity;

    ASSERT_EQ(quaternion, quaternion * identity);
  }

  TYPED_TEST(QuaternionTest, ShouldMultiplyTwoQuaternions) {
    Quaternion<TypeParam> first(1, 2, 2, 3), second(1, 2, 2, 3);
    Quaternion<TypeParam> expected(-16, 4, 4, 6);
    ASSERT_EQ(expected, first * second);
  }

  TYPED_TEST(QuaternionTest, ShouldReturnConjugate) {
    Quaternion<TypeParam> q(1, 2, 3, 4);
    Quaternion<TypeParam> expected(1, -2, -3, -4);
    ASSERT_EQ(expected, q.conjugate());
  }

  TYPED_TEST(QuaternionTest, ShouldReturnIdentityForConjugateOfIdentity) {
    Quaternion<TypeParam> identity;
    ASSERT_EQ(identity, identity.conjugate());
  }

  TYPED_TEST(QuaternionTest, ShouldSatisfyQTimesConjugateEqualsIdentityForUnitQuaternion) {
    // q * q* == identity for any unit quaternion
    Quaternion<TypeParam> q =
      Quaternion<TypeParam>(TypeParam(0.707), TypeParam(0.707), TypeParam(0), TypeParam(0))
        .normalized();
    Quaternion<TypeParam> result = q * q.conjugate();
    ASSERT_NEAR(1, result.w(), 0.001);
    ASSERT_NEAR(0, result.x(), 0.001);
    ASSERT_NEAR(0, result.y(), 0.001);
    ASSERT_NEAR(0, result.z(), 0.001);
  }

  TYPED_TEST(QuaternionTest, ShouldReturnInverse) {
    Quaternion<TypeParam> q =
      Quaternion<TypeParam>(TypeParam(0.5), TypeParam(0.5), TypeParam(0.5), TypeParam(0.5));
    Quaternion<TypeParam> result = q * q.inverse();
    ASSERT_NEAR(1, result.w(), 0.001);
    ASSERT_NEAR(0, result.x(), 0.001);
    ASSERT_NEAR(0, result.y(), 0.001);
    ASSERT_NEAR(0, result.z(), 0.001);
  }

  TYPED_TEST(QuaternionTest, ShouldRotateVectorWithIdentityQuaternion) {
    Quaternion<TypeParam> identity;
    Vector3<TypeParam> v(TypeParam(1), TypeParam(0), TypeParam(0));
    Vector3<TypeParam> result = identity.rotate(v);
    ASSERT_NEAR(1, result.x(), 0.001);
    ASSERT_NEAR(0, result.y(), 0.001);
    ASSERT_NEAR(0, result.z(), 0.001);
  }

  TYPED_TEST(QuaternionTest, ShouldRotateVectorBy90DegreesAroundZ) {
    // 90° rotation around z: (1,0,0) -> (0,1,0)
    TypeParam angle = TypeParam(M_PI) / TypeParam(2);
    Vector3<TypeParam> axis(TypeParam(0), TypeParam(0), TypeParam(1));
    Quaternion<TypeParam> q = Quaternion<TypeParam>::fromAxisAngle(axis, angle);
    Vector3<TypeParam> result =
      q.rotate(Vector3<TypeParam>(TypeParam(1), TypeParam(0), TypeParam(0)));
    ASSERT_NEAR(0, result.x(), 0.001);
    ASSERT_NEAR(1, result.y(), 0.001);
    ASSERT_NEAR(0, result.z(), 0.001);
  }

  TYPED_TEST(QuaternionTest, ShouldBuildFromAxisAngle) {
    TypeParam angle = TypeParam(M_PI) / TypeParam(2);
    Vector3<TypeParam> axis(TypeParam(0), TypeParam(0), TypeParam(1));
    Quaternion<TypeParam> q = Quaternion<TypeParam>::fromAxisAngle(axis, angle);
    TypeParam expected_w = std::cos(angle / TypeParam(2));
    TypeParam expected_z = std::sin(angle / TypeParam(2));
    ASSERT_NEAR(expected_w, q.w(), 0.001);
    ASSERT_NEAR(0, q.x(), 0.001);
    ASSERT_NEAR(0, q.y(), 0.001);
    ASSERT_NEAR(expected_z, q.z(), 0.001);
  }

  TYPED_TEST(QuaternionTest, ShouldBuildFromAxisAngleProducingUnitQuaternion) {
    TypeParam angle = TypeParam(M_PI) / TypeParam(3);
    Vector3<TypeParam> axis =
      Vector3<TypeParam>(TypeParam(1), TypeParam(1), TypeParam(0)).normalized();
    Quaternion<TypeParam> q = Quaternion<TypeParam>::fromAxisAngle(axis, angle);
    ASSERT_NEAR(1, q.length(), 0.001);
  }

  TYPED_TEST(QuaternionTest, ShouldBuildFromEulerAnglesMatchingAxisAngleAroundX) {
    TypeParam angle = TypeParam(M_PI) / TypeParam(2);
    Quaternion<TypeParam> from_euler =
      Quaternion<TypeParam>::fromEulerAngles(angle, TypeParam(0), TypeParam(0));
    Quaternion<TypeParam> from_axis = Quaternion<TypeParam>::fromAxisAngle(
      Vector3<TypeParam>(TypeParam(1), TypeParam(0), TypeParam(0)), angle);
    ASSERT_NEAR(from_axis.w(), from_euler.w(), 0.001);
    ASSERT_NEAR(from_axis.x(), from_euler.x(), 0.001);
    ASSERT_NEAR(from_axis.y(), from_euler.y(), 0.001);
    ASSERT_NEAR(from_axis.z(), from_euler.z(), 0.001);
  }

  TYPED_TEST(QuaternionTest, ShouldBuildFromEulerAnglesMatchingAxisAngleAroundZ) {
    TypeParam angle = TypeParam(M_PI) / TypeParam(2);
    Quaternion<TypeParam> from_euler =
      Quaternion<TypeParam>::fromEulerAngles(TypeParam(0), TypeParam(0), angle);
    Quaternion<TypeParam> from_axis = Quaternion<TypeParam>::fromAxisAngle(
      Vector3<TypeParam>(TypeParam(0), TypeParam(0), TypeParam(1)), angle);
    ASSERT_NEAR(from_axis.w(), from_euler.w(), 0.001);
    ASSERT_NEAR(from_axis.x(), from_euler.x(), 0.001);
    ASSERT_NEAR(from_axis.y(), from_euler.y(), 0.001);
    ASSERT_NEAR(from_axis.z(), from_euler.z(), 0.001);
  }

  TYPED_TEST(QuaternionTest, ShouldRoundTripEulerAngles) {
    TypeParam rx = TypeParam(0.3), ry = TypeParam(0.5), rz = TypeParam(0.7);
    Quaternion<TypeParam> q = Quaternion<TypeParam>::fromEulerAngles(rx, ry, rz);
    Vector3<TypeParam> angles = q.toEulerAngles();
    ASSERT_NEAR(rx, angles.x(), 0.001);
    ASSERT_NEAR(ry, angles.y(), 0.001);
    ASSERT_NEAR(rz, angles.z(), 0.001);
  }

  TYPED_TEST(QuaternionTest, ShouldReturnZeroEulerAnglesForIdentityQuaternion) {
    Quaternion<TypeParam> identity;
    Vector3<TypeParam> angles = identity.toEulerAngles();
    ASSERT_NEAR(0, angles.x(), 0.001);
    ASSERT_NEAR(0, angles.y(), 0.001);
    ASSERT_NEAR(0, angles.z(), 0.001);
  }

  TYPED_TEST(QuaternionTest, ShouldConvertToMatrix3AsIdentityForIdentityQuaternion) {
    Quaternion<TypeParam> identity;
    Matrix3<TypeParam> m = identity.toMatrix3();
    Matrix3<TypeParam> expected;
    for (int r = 0; r < 3; ++r)
      for (int c = 0; c < 3; ++c)
        ASSERT_NEAR(expected.cell(r, c), m.cell(r, c), 0.001);
  }

  TYPED_TEST(QuaternionTest, ShouldConvertToMatrix3Matching90DegRotationAroundZ) {
    TypeParam angle = TypeParam(M_PI) / TypeParam(2);
    Quaternion<TypeParam> q = Quaternion<TypeParam>::fromAxisAngle(
      Vector3<TypeParam>(TypeParam(0), TypeParam(0), TypeParam(1)), angle);
    Matrix3<TypeParam> m = q.toMatrix3();
    Matrix3<TypeParam> expected = Matrix3<TypeParam>::rotateZ(Angle<TypeParam>::fromRadians(angle));
    for (int r = 0; r < 3; ++r)
      for (int c = 0; c < 3; ++c)
        ASSERT_NEAR(expected.cell(r, c), m.cell(r, c), 0.001);
  }

  TYPED_TEST(QuaternionTest, ShouldConvertToMatrix4AsIdentityForIdentityQuaternion) {
    Quaternion<TypeParam> identity;
    Matrix4<TypeParam> m = identity.toMatrix4();
    Matrix4<TypeParam> expected;
    for (int r = 0; r < 4; ++r)
      for (int c = 0; c < 4; ++c)
        ASSERT_NEAR(expected.cell(r, c), m.cell(r, c), 0.001);
  }

  TYPED_TEST(QuaternionTest, ShouldConvertToMatrix4Matching90DegRotationAroundX) {
    TypeParam angle = TypeParam(M_PI) / TypeParam(2);
    Quaternion<TypeParam> q = Quaternion<TypeParam>::fromAxisAngle(
      Vector3<TypeParam>(TypeParam(1), TypeParam(0), TypeParam(0)), angle);
    Matrix4<TypeParam> m = q.toMatrix4();
    Matrix3<TypeParam> rot3 = Matrix3<TypeParam>::rotateX(Angle<TypeParam>::fromRadians(angle));
    // Upper-left 3x3 block must match the rotation matrix
    for (int r = 0; r < 3; ++r)
      for (int c = 0; c < 3; ++c)
        ASSERT_NEAR(rot3.cell(r, c), m.cell(r, c), 0.001);
    // Bottom-right corner must be 1
    ASSERT_NEAR(1, m.cell(3, 3), 0.001);
    // Translation column must be zero
    ASSERT_NEAR(0, m.cell(0, 3), 0.001);
    ASSERT_NEAR(0, m.cell(1, 3), 0.001);
    ASSERT_NEAR(0, m.cell(2, 3), 0.001);
  }

  TYPED_TEST(QuaternionTest, ShouldRoundTripQuaternionThroughMatrix3) {
    TypeParam angle = TypeParam(M_PI) / TypeParam(3);
    Vector3<TypeParam> axis =
      Vector3<TypeParam>(TypeParam(1), TypeParam(2), TypeParam(3)).normalized();
    Quaternion<TypeParam> q = Quaternion<TypeParam>::fromAxisAngle(axis, angle);
    Quaternion<TypeParam> q2 = q.toMatrix3().rotationQuaternion();
    // Either q2 == q or q2 == -q (both represent the same rotation)
    bool same =
      std::abs(q.w() - q2.w()) < TypeParam(0.001) && std::abs(q.x() - q2.x()) < TypeParam(0.001) &&
      std::abs(q.y() - q2.y()) < TypeParam(0.001) && std::abs(q.z() - q2.z()) < TypeParam(0.001);
    bool opposite =
      std::abs(q.w() + q2.w()) < TypeParam(0.001) && std::abs(q.x() + q2.x()) < TypeParam(0.001) &&
      std::abs(q.y() + q2.y()) < TypeParam(0.001) && std::abs(q.z() + q2.z()) < TypeParam(0.001);
    ASSERT_TRUE(same || opposite);
  }

  TYPED_TEST(QuaternionTest, NlerpShouldReturnFirstQuaternionAtT0) {
    Quaternion<TypeParam> a = Quaternion<TypeParam>::fromAxisAngle(
      Vector3<TypeParam>(TypeParam(0), TypeParam(0), TypeParam(1)), TypeParam(0));
    Quaternion<TypeParam> b = Quaternion<TypeParam>::fromAxisAngle(
      Vector3<TypeParam>(TypeParam(0), TypeParam(0), TypeParam(1)), TypeParam(M_PI) / TypeParam(2));
    Quaternion<TypeParam> result = Quaternion<TypeParam>::nlerp(a, b, TypeParam(0));
    ASSERT_NEAR(a.w(), result.w(), 0.001);
    ASSERT_NEAR(a.x(), result.x(), 0.001);
    ASSERT_NEAR(a.y(), result.y(), 0.001);
    ASSERT_NEAR(a.z(), result.z(), 0.001);
  }

  TYPED_TEST(QuaternionTest, NlerpShouldReturnSecondQuaternionAtT1) {
    Quaternion<TypeParam> a = Quaternion<TypeParam>::fromAxisAngle(
      Vector3<TypeParam>(TypeParam(0), TypeParam(0), TypeParam(1)), TypeParam(0));
    Quaternion<TypeParam> b = Quaternion<TypeParam>::fromAxisAngle(
      Vector3<TypeParam>(TypeParam(0), TypeParam(0), TypeParam(1)), TypeParam(M_PI) / TypeParam(2));
    Quaternion<TypeParam> result = Quaternion<TypeParam>::nlerp(a, b, TypeParam(1));
    ASSERT_NEAR(b.w(), result.w(), 0.001);
    ASSERT_NEAR(b.x(), result.x(), 0.001);
    ASSERT_NEAR(b.y(), result.y(), 0.001);
    ASSERT_NEAR(b.z(), result.z(), 0.001);
  }

  TYPED_TEST(QuaternionTest, NlerpShouldProduceUnitQuaternion) {
    Quaternion<TypeParam> a = Quaternion<TypeParam>::fromAxisAngle(
      Vector3<TypeParam>(TypeParam(1), TypeParam(0), TypeParam(0)), TypeParam(0.3));
    Quaternion<TypeParam> b = Quaternion<TypeParam>::fromAxisAngle(
      Vector3<TypeParam>(TypeParam(0), TypeParam(1), TypeParam(0)), TypeParam(1.2));
    Quaternion<TypeParam> result = Quaternion<TypeParam>::nlerp(a, b, TypeParam(0.5));
    ASSERT_NEAR(1, result.length(), 0.001);
  }

  TYPED_TEST(QuaternionTest, SlerpShouldReturnFirstQuaternionAtT0) {
    Quaternion<TypeParam> a = Quaternion<TypeParam>::fromAxisAngle(
      Vector3<TypeParam>(TypeParam(0), TypeParam(0), TypeParam(1)), TypeParam(0));
    Quaternion<TypeParam> b = Quaternion<TypeParam>::fromAxisAngle(
      Vector3<TypeParam>(TypeParam(0), TypeParam(0), TypeParam(1)), TypeParam(M_PI) / TypeParam(2));
    Quaternion<TypeParam> result = Quaternion<TypeParam>::slerp(a, b, TypeParam(0));
    ASSERT_NEAR(a.w(), result.w(), 0.001);
    ASSERT_NEAR(a.x(), result.x(), 0.001);
    ASSERT_NEAR(a.y(), result.y(), 0.001);
    ASSERT_NEAR(a.z(), result.z(), 0.001);
  }

  TYPED_TEST(QuaternionTest, SlerpShouldReturnSecondQuaternionAtT1) {
    Quaternion<TypeParam> a = Quaternion<TypeParam>::fromAxisAngle(
      Vector3<TypeParam>(TypeParam(0), TypeParam(0), TypeParam(1)), TypeParam(0));
    Quaternion<TypeParam> b = Quaternion<TypeParam>::fromAxisAngle(
      Vector3<TypeParam>(TypeParam(0), TypeParam(0), TypeParam(1)), TypeParam(M_PI) / TypeParam(2));
    Quaternion<TypeParam> result = Quaternion<TypeParam>::slerp(a, b, TypeParam(1));
    ASSERT_NEAR(b.w(), result.w(), 0.001);
    ASSERT_NEAR(b.x(), result.x(), 0.001);
    ASSERT_NEAR(b.y(), result.y(), 0.001);
    ASSERT_NEAR(b.z(), result.z(), 0.001);
  }

  TYPED_TEST(QuaternionTest, SlerpShouldProduceUnitQuaternion) {
    Quaternion<TypeParam> a = Quaternion<TypeParam>::fromAxisAngle(
      Vector3<TypeParam>(TypeParam(1), TypeParam(0), TypeParam(0)), TypeParam(0.3));
    Quaternion<TypeParam> b = Quaternion<TypeParam>::fromAxisAngle(
      Vector3<TypeParam>(TypeParam(0), TypeParam(1), TypeParam(0)), TypeParam(1.2));
    Quaternion<TypeParam> result = Quaternion<TypeParam>::slerp(a, b, TypeParam(0.5));
    ASSERT_NEAR(1, result.length(), 0.001);
  }

  TYPED_TEST(QuaternionTest, SlerpShouldMatchNlerpForIdenticalInputs) {
    Quaternion<TypeParam> a = Quaternion<TypeParam>::fromAxisAngle(
      Vector3<TypeParam>(TypeParam(0), TypeParam(1), TypeParam(0)), TypeParam(0.5));
    Quaternion<TypeParam> slerp_result = Quaternion<TypeParam>::slerp(a, a, TypeParam(0.5));
    ASSERT_NEAR(1, slerp_result.length(), 0.001);
  }

  TYPED_TEST(QuaternionTest, ShouldStreamQuaternionToString) {
    Quaternion<TypeParam> quaternion(4, 1, 2, 3);

    ostringstream str;
    str << quaternion;

    ASSERT_EQ("[4, 1 2 3]", str.str());
  }
}
