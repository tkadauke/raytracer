#include <gtest/gtest.h>

#include "core/math/MatrixDecomposition.h"
#include "test/helpers/MatrixTestHelper.h"

#include <chrono>

namespace MatrixDecompositionTest {
  using namespace ::testing;

  template<class T>
  class MatrixDecompositionTest : public ::testing::Test {
  };

  using MatrixTypes = ::testing::Types<float, double, long double>;

  TYPED_TEST_SUITE(MatrixDecompositionTest, MatrixTypes);

  TYPED_TEST(MatrixDecompositionTest, LUReconstructsPivotedMatrix3) {
    Matrix3<TypeParam> matrix(
      TypeParam(2), TypeParam(1), TypeParam(1),
      TypeParam(4), TypeParam(-6), TypeParam(0),
      TypeParam(-2), TypeParam(7), TypeParam(2)
    );

    const auto decomposition = luDecomposition<3, TypeParam, Vector3<TypeParam>, Matrix3<TypeParam>>(matrix);

    ASSERT_FALSE(decomposition.singular);
    ASSERT_MATRIX_NEAR(
      decomposition.permutation() * matrix,
      decomposition.lower() * decomposition.upper(),
      TypeParam(0.0001)
    );
    ASSERT_NEAR(TypeParam(-16), decomposition.determinant(), TypeParam(0.0001));
  }

  TYPED_TEST(MatrixDecompositionTest, LUSolvesCanonicalSystem4) {
    Matrix4<TypeParam> matrix(
      TypeParam(3), TypeParam(0), TypeParam(2), TypeParam(-1),
      TypeParam(1), TypeParam(2), TypeParam(0), TypeParam(-2),
      TypeParam(4), TypeParam(0), TypeParam(6), TypeParam(-3),
      TypeParam(5), TypeParam(0), TypeParam(2), TypeParam(0)
    );
    Vector4<TypeParam> expected(TypeParam(1), TypeParam(2), TypeParam(-1), TypeParam(3));
    Vector4<TypeParam> rhs = matrix * expected;

    const auto decomposition = luDecomposition<4, TypeParam, Vector4<TypeParam>, Matrix4<TypeParam>>(matrix);

    ASSERT_FALSE(decomposition.singular);
    const Vector4<TypeParam> actual = decomposition.solve(rhs);
    for (int i = 0; i != 4; ++i) {
      ASSERT_NEAR(expected[i], actual[i], TypeParam(0.0001));
    }
  }

  TYPED_TEST(MatrixDecompositionTest, QRReconstructsCanonicalMatrix3WithOrthonormalQ) {
    Matrix3<TypeParam> matrix(
      TypeParam(12), TypeParam(-51), TypeParam(4),
      TypeParam(6), TypeParam(167), TypeParam(-68),
      TypeParam(-4), TypeParam(24), TypeParam(-41)
    );

    const auto decomposition = qrDecomposition<3, TypeParam, Vector3<TypeParam>, Matrix3<TypeParam>>(matrix);

    Matrix3<TypeParam> identity;
    ASSERT_MATRIX_NEAR(identity, decomposition.q.transposed() * decomposition.q, TypeParam(0.0001));
    ASSERT_MATRIX_NEAR(matrix, decomposition.q * decomposition.r, TypeParam(0.001));
    ASSERT_NEAR(TypeParam(14), std::abs(decomposition.r[0][0]), TypeParam(0.0001));
    ASSERT_NEAR(TypeParam(175), std::abs(decomposition.r[1][1]), TypeParam(0.0001));
    ASSERT_NEAR(TypeParam(35), std::abs(decomposition.r[2][2]), TypeParam(0.0001));
  }

  TYPED_TEST(MatrixDecompositionTest, SVDReconstructsDiagonalMatrix3) {
    Matrix3<TypeParam> matrix(
      TypeParam(3), TypeParam(0), TypeParam(0),
      TypeParam(0), TypeParam(2), TypeParam(0),
      TypeParam(0), TypeParam(0), TypeParam(1)
    );

    const auto decomposition = svdDecomposition<3, TypeParam, Vector3<TypeParam>, Matrix3<TypeParam>>(matrix);

    Matrix3<TypeParam> identity;
    ASSERT_MATRIX_NEAR(identity, decomposition.u.transposed() * decomposition.u, TypeParam(0.0001));
    ASSERT_MATRIX_NEAR(identity, decomposition.v.transposed() * decomposition.v, TypeParam(0.0001));
    ASSERT_NEAR(TypeParam(3), decomposition.singularValues[0], TypeParam(0.0001));
    ASSERT_NEAR(TypeParam(2), decomposition.singularValues[1], TypeParam(0.0001));
    ASSERT_NEAR(TypeParam(1), decomposition.singularValues[2], TypeParam(0.0001));
    ASSERT_MATRIX_NEAR(matrix, decomposition.u * decomposition.sigma() * decomposition.v.transposed(), TypeParam(0.0001));
  }

  TYPED_TEST(MatrixDecompositionTest, SVDReconstructsGeneralMatrix4) {
    Matrix4<TypeParam> matrix(
      TypeParam(1), TypeParam(0), TypeParam(0), TypeParam(0),
      TypeParam(0), TypeParam(0), TypeParam(2), TypeParam(0),
      TypeParam(0), TypeParam(3), TypeParam(0), TypeParam(0),
      TypeParam(0), TypeParam(0), TypeParam(0), TypeParam(-4)
    );

    const auto decomposition = svdDecomposition<4, TypeParam, Vector4<TypeParam>, Matrix4<TypeParam>>(matrix);

    Matrix4<TypeParam> identity;
    ASSERT_MATRIX_NEAR(identity, decomposition.u.transposed() * decomposition.u, TypeParam(0.0001));
    ASSERT_MATRIX_NEAR(identity, decomposition.v.transposed() * decomposition.v, TypeParam(0.0001));
    ASSERT_NEAR(TypeParam(4), decomposition.singularValues[0], TypeParam(0.0001));
    ASSERT_NEAR(TypeParam(3), decomposition.singularValues[1], TypeParam(0.0001));
    ASSERT_NEAR(TypeParam(2), decomposition.singularValues[2], TypeParam(0.0001));
    ASSERT_NEAR(TypeParam(1), decomposition.singularValues[3], TypeParam(0.0001));
    ASSERT_MATRIX_NEAR(matrix, decomposition.u * decomposition.sigma() * decomposition.v.transposed(), TypeParam(0.0001));
  }

  TYPED_TEST(MatrixDecompositionTest, StableInverseMatchesBlockInverseForWellConditionedMatrix4) {
    Matrix4<TypeParam> matrix(
      TypeParam(0.866), TypeParam(-0.5), TypeParam(0), TypeParam(1),
      TypeParam(0.5), TypeParam(0.866), TypeParam(0), TypeParam(2),
      TypeParam(0), TypeParam(0), TypeParam(1), TypeParam(3),
      TypeParam(0), TypeParam(0), TypeParam(0), TypeParam(1)
    );

    ASSERT_MATRIX_NEAR(matrix.inverted(), matrix.stableInverse(), TypeParam(0.0001));
  }

  TYPED_TEST(MatrixDecompositionTest, StableInverseKeepsSmallResidualForIllConditionedMatrix4) {
    Matrix4<TypeParam> matrix(
      TypeParam(1), TypeParam(0.999999), TypeParam(0), TypeParam(0),
      TypeParam(0.999999), TypeParam(1), TypeParam(0), TypeParam(0),
      TypeParam(0), TypeParam(0), TypeParam(1), TypeParam(0),
      TypeParam(0), TypeParam(0), TypeParam(0), TypeParam(1)
    );

    Matrix4<TypeParam> identity;
    ASSERT_MATRIX_NEAR(identity, matrix * matrix.stableInverse(), TypeParam(0.01));
  }

  TEST(Matrix4StableInversePerformance, WellConditionedPathStaysWithin2xBlockInverse) {
    Matrix4d matrix(
      0.866, -0.5, 0.0, 1.0,
      0.5, 0.866, 0.0, 2.0,
      0.0, 0.0, 1.0, 3.0,
      0.0, 0.0, 0.0, 1.0
    );
    constexpr int kIterations = 50000;

    auto time = [&](auto inverse) {
      Matrix4d last;
      const auto start = std::chrono::steady_clock::now();
      for (int i = 0; i != kIterations; ++i) {
        last = inverse(matrix);
      }
      const auto elapsed = std::chrono::steady_clock::now() - start;
      EXPECT_NEAR(matrix.inverted()[0][0], last[0][0], 0.0001);
      return elapsed;
    };

    const auto block = time([](const Matrix4d& m) { return m.inverted(); });
    const auto stable = time([](const Matrix4d& m) { return m.stableInverse(); });
    const double ratio = static_cast<double>(stable.count()) / static_cast<double>(block.count());

    EXPECT_LE(ratio, 2.0)
      << "stableInverse was " << ratio << "x slower than block inverse "
      << "(stable=" << stable.count() << "ns, block=" << block.count() << "ns)";
  }
}
