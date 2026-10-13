#include <gtest/gtest.h>
#include <type_traits>
#include <utility>

#include "core/math/Geometry.h"

namespace GeometryTest {
  template<class A, class B, class = void>
  struct CanAdd : std::false_type {};
  template<class A, class B>
  struct CanAdd<A, B, std::void_t<decltype(std::declval<A>() + std::declval<B>())>>
      : std::true_type {};

  template<class A, class B, class = void>
  struct CanSub : std::false_type {};
  template<class A, class B>
  struct CanSub<A, B, std::void_t<decltype(std::declval<A>() - std::declval<B>())>>
      : std::true_type {};

  template<class A, class B, class = void>
  struct CanMul : std::false_type {};
  template<class A, class B>
  struct CanMul<A, B, std::void_t<decltype(std::declval<A>() * std::declval<B>())>>
      : std::true_type {};

  template<class A, class B, class = void>
  struct CanConvert : std::false_type {};
  template<class A, class B>
  struct CanConvert<A, B, std::void_t<decltype(B(std::declval<A>()))>> : std::true_type {};

  // Legal arithmetic and its result types.
  static_assert(std::is_same_v<decltype(Point3f() + Vector3f()), Point3f>);
  static_assert(std::is_same_v<decltype(Vector3f() + Point3f()), Point3f>);
  static_assert(std::is_same_v<decltype(Point3f() - Vector3f()), Point3f>);
  static_assert(std::is_same_v<decltype(Point3f() - Point3f()), Vector3f>);
  static_assert(std::is_same_v<decltype(Direction3f::fromComponents(1, 0, 0) * 2.0f), Vector3f>);
  static_assert(std::is_same_v<decltype(Normal3f::fromComponents(1, 0, 0) + Normal3f::fromComponents(0, 1, 0)), Vector3f>);
  static_assert(std::is_same_v<decltype(Matrix4f() * Point3f()), Point3f>);
  static_assert(std::is_same_v<decltype(Matrix4f() * Direction3f::fromComponents(1, 0, 0)), Direction3f>);
  static_assert(std::is_same_v<decltype(NormalMatrix3f(Matrix3f()) * Normal3f::fromComponents(1, 0, 0)), Normal3f>);

  // Illegal arithmetic.
  static_assert(!CanAdd<Point3f, Point3f>::value);
  static_assert(!CanMul<Point3f, float>::value);
  static_assert(!CanMul<float, Point3f>::value);
  static_assert(!CanAdd<Point3f, Direction3f>::value);
  static_assert(!CanAdd<Point3f, Normal3f>::value);
  static_assert(!CanMul<Matrix4f, Normal3f>::value);
  static_assert(!CanMul<Matrix3f, Normal3f>::value);
  static_assert(!CanMul<NormalMatrix3f, Direction3f>::value);
  static_assert(!CanMul<NormalMatrix3f, Vector3f>::value);
  static_assert(!CanMul<NormalMatrix3f, Point3f>::value);
  static_assert(!CanMul<Matrix4f, Normal3d>::value);

  // Conversions: widening is implicit, narrowing and Direction <-> Normal are not.
  static_assert(std::is_convertible_v<Direction3f, Vector3f>);
  static_assert(std::is_convertible_v<Normal3f, Vector3f>);
  static_assert(!std::is_convertible_v<Vector3f, Direction3f>);
  static_assert(!std::is_convertible_v<Vector3f, Normal3f>);
  static_assert(!std::is_convertible_v<Direction3f, Normal3f>);
  static_assert(!std::is_convertible_v<Normal3f, Direction3f>);
  static_assert(!std::is_convertible_v<Vector3f, Point3f>);
  static_assert(!std::is_convertible_v<NormalMatrix3f, Matrix3f>);

  template<class T>
  class GeometryTest : public ::testing::Test {};
  typedef ::testing::Types<float, double> Types;
  TYPED_TEST_SUITE(GeometryTest, Types);

  TYPED_TEST(GeometryTest, PointHasUnitW) {
    Point3<TypeParam> p(1, 2, 3);
    EXPECT_EQ(TypeParam(1), p.w());
    EXPECT_EQ(Vector3<TypeParam>(1, 2, 3), p.toVector());
  }

  TYPED_TEST(GeometryTest, PointVectorAlgebra) {
    Point3<TypeParam> a(1, 2, 3), b(4, 6, 8);
    EXPECT_EQ(Vector3<TypeParam>(3, 4, 5), b - a);
    EXPECT_EQ(b, a + (b - a));
    EXPECT_EQ(a, b - (b - a));
  }

  TYPED_TEST(GeometryTest, ToDirectionAndToNormalNormalize) {
    Vector3<TypeParam> v(0, 3, 4);
    EXPECT_NEAR(1.0, toDirection(v).toVector().length(), 1e-6);
    EXPECT_NEAR(0.6, toNormal(v).y(), 1e-6);
    Normal3<TypeParam> n = toNormal(toDirection(v));
    EXPECT_NEAR(0.8, n.z(), 1e-6);
  }

  TYPED_TEST(GeometryTest, DotProducts) {
    auto d = Direction3<TypeParam>::fromComponents(0, 1, 0);
    auto n = Normal3<TypeParam>::fromComponents(0, 1, 1);
    EXPECT_NEAR(1.0, dot(d, d), 1e-6);
    EXPECT_NEAR(std::sqrt(0.5), dot(n, d), 1e-6);
    EXPECT_NEAR(dot(n, d), dot(d, n), 1e-6);
  }

  TYPED_TEST(GeometryTest, MatrixTranslatesPointsButNotDirections) {
    Matrix4<TypeParam> m = Matrix4<TypeParam>::translate(Vector3<TypeParam>(5, 6, 7));
    Point3<TypeParam> p = m * Point3<TypeParam>(1, 1, 1);
    EXPECT_EQ(Vector3<TypeParam>(6, 7, 8), p.toVector());
    auto d = Direction3<TypeParam>::fromComponents(0, 0, 1);
    EXPECT_EQ(d, m * d);
  }

  TYPED_TEST(GeometryTest, MatrixRenormalizesDirections) {
    Matrix3<TypeParam> m(2, 0, 0, 0, 3, 0, 0, 0, 4);
    auto d = m * Direction3<TypeParam>::fromComponents(1, 0, 0);
    EXPECT_NEAR(1.0, d.toVector().length(), 1e-6);
  }

  TYPED_TEST(GeometryTest, NormalMatrixUsesInverseTranspose) {
    // Non-uniform scale (1, 1, 4) applied to a surface with tangent (1, 0, 1)
    // and normal (1, 0, -1)/sqrt(2). The transformed normal must stay
    // perpendicular to the transformed tangent.
    Matrix4<TypeParam> m = Matrix4<TypeParam>::scale(1, 1, 4);
    Vector3<TypeParam> tangent(1, 0, 1);
    auto n = Normal3<TypeParam>::fromComponents(1, 0, -1);

    Vector3<TypeParam> tangentT = Matrix3<TypeParam>(m) * tangent;
    Normal3<TypeParam> nT = normalMatrix(m) * n;

    EXPECT_NEAR(0.0, tangentT.dotProduct(nT.toVector()), 1e-5);
    // Forward-transforming the normal would not preserve perpendicularity.
    Vector3<TypeParam> wrong = Matrix3<TypeParam>(m) * n.toVector();
    EXPECT_GT(std::abs(tangentT.dotProduct(wrong.normalized())), 0.1);
  }

  TYPED_TEST(GeometryTest, NormalMatrixComposes) {
    Matrix3<TypeParam> a(2, 0, 0, 0, 1, 0, 0, 0, 1);
    Matrix3<TypeParam> b(1, 0, 0, 0, 3, 0, 0, 0, 1);
    auto n = Normal3<TypeParam>::fromComponents(1, 1, 1);
    Normal3<TypeParam> composed = (normalMatrix(a) * normalMatrix(b)) * n;
    Normal3<TypeParam> direct = normalMatrix(a * b) * n;
    EXPECT_NEAR(composed.x(), direct.x(), 1e-5);
    EXPECT_NEAR(composed.y(), direct.y(), 1e-5);
    EXPECT_NEAR(composed.z(), direct.z(), 1e-5);
  }

#ifndef NDEBUG
  TEST(GeometryDeathTest, InvariantsTripInDebugBuilds) {
    EXPECT_DEATH(Point3f(1, 2, 3, 0.0f), "");
    EXPECT_DEATH(Direction3f(Vector3f(1, 1, 0)), "");
    EXPECT_DEATH(Normal3f(Vector3f(0, 0, 2)), "");
  }
#endif
}
