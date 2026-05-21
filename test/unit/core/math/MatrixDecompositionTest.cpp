#include <gtest/gtest.h>

#include "core/math/Matrix.h"
#include "test/helpers/MatrixTestHelper.h"

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

    const auto decomposition = matrix.luDecomposition();

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

    const auto decomposition = matrix.luDecomposition();

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

    const auto decomposition = matrix.qrDecomposition();

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

    const auto decomposition = matrix.svdDecomposition();

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

    const auto decomposition = matrix.svdDecomposition();

    Matrix4<TypeParam> identity;
    ASSERT_MATRIX_NEAR(identity, decomposition.u.transposed() * decomposition.u, TypeParam(0.0001));
    ASSERT_MATRIX_NEAR(identity, decomposition.v.transposed() * decomposition.v, TypeParam(0.0001));
    ASSERT_NEAR(TypeParam(4), decomposition.singularValues[0], TypeParam(0.0001));
    ASSERT_NEAR(TypeParam(3), decomposition.singularValues[1], TypeParam(0.0001));
    ASSERT_NEAR(TypeParam(2), decomposition.singularValues[2], TypeParam(0.0001));
    ASSERT_NEAR(TypeParam(1), decomposition.singularValues[3], TypeParam(0.0001));
    ASSERT_MATRIX_NEAR(matrix, decomposition.u * decomposition.sigma() * decomposition.v.transposed(), TypeParam(0.0001));
  }

  TYPED_TEST(MatrixDecompositionTest, SVDCompletesOrthonormalBasisForRankDeficientMatrix4) {
    Matrix4<TypeParam> matrix(
      TypeParam(1), TypeParam(2), TypeParam(0), TypeParam(0),
      TypeParam(2), TypeParam(4), TypeParam(0), TypeParam(0),
      TypeParam(0), TypeParam(0), TypeParam(0), TypeParam(0),
      TypeParam(0), TypeParam(0), TypeParam(0), TypeParam(0)
    );

    const auto decomposition = matrix.svdDecomposition();

    Matrix4<TypeParam> identity;
    ASSERT_MATRIX_NEAR(identity, decomposition.u.transposed() * decomposition.u, TypeParam(0.0001));
    ASSERT_MATRIX_NEAR(identity, decomposition.v.transposed() * decomposition.v, TypeParam(0.0001));
    ASSERT_MATRIX_NEAR(matrix, decomposition.u * decomposition.sigma() * decomposition.v.transposed(), TypeParam(0.0001));
    ASSERT_NEAR(TypeParam(5), decomposition.singularValues[0], TypeParam(0.0001));
    ASSERT_NEAR(TypeParam(0), decomposition.singularValues[1], TypeParam(0.0001));
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

}
