#include <gtest/gtest.h>
#include "core/math/Matrix.h"
#include "core/math/Vector.h"
#include "core/math/Quaternion.h"
#include "test/helpers/VectorTestHelper.h"
#include "test/helpers/MatrixTestHelper.h"
#include "test/helpers/TypeTestHelper.h"

#include <sstream>

using namespace std;

namespace MatrixTest {
  using namespace ::testing;

  template<class T>
  class MatrixTest : public ::testing::Test {
  };

  typedef ::testing::Types<float, double, long double> SpecializedMatrixTypes;
  
  TYPED_TEST_SUITE(MatrixTest, SpecializedMatrixTypes);
  
  TYPED_TEST(MatrixTest, ShouldInitializeMatrixAsIdentity) {
    Matrix<3, TypeParam> matrix;
    ASSERT_EQ(1, matrix[0][0]);
    ASSERT_EQ(0, matrix[1][0]);
  }

  TYPED_TEST(MatrixTest, ShouldGetAndSetCell) {
    Matrix<3, TypeParam> matrix;
    matrix.setCell(0, 2, 10);
    ASSERT_EQ(10, matrix.cell(0, 2));
  }

  TYPED_TEST(MatrixTest, ShouldGetAndSetCellWithIndexOperator) {
    Matrix<3, TypeParam> matrix;
    matrix[0][2] = 10;
    ASSERT_EQ(10, matrix[0][2]);
  }

  TYPED_TEST(MatrixTest, ShouldConstructFromNestedCArray) {
    Matrix<3, TypeParam> matrix({
      {1, 2, 1},
      {2, 0, 1},
      {1, 1, 1}
    });
    ASSERT_EQ(1, matrix[0][0]);
    ASSERT_EQ(2, matrix[1][0]);
    ASSERT_EQ(1, matrix[2][0]);
  }
  
  TYPED_TEST(MatrixTest, ShouldCopyFromSmallerMatrix) {
    Matrix<3, TypeParam> source({
      {1, 2, 1},
      {2, 0, 1},
      {1, 1, 1}
    });
    
    Matrix<4, TypeParam> expected({
      {1, 2, 1, 0},
      {2, 0, 1, 0},
      {1, 1, 1, 0},
      {0, 0, 0, 1}
    });

    Matrix<4, TypeParam> dest = source;
    ASSERT_EQ(expected, dest);
  }

  TYPED_TEST(MatrixTest, ShouldCopyFromLargerMatrix) {
    Matrix<3, TypeParam> source({
      {1, 2, 1},
      {2, 0, 1},
      {1, 1, 1}
    });
    
    Matrix<2, TypeParam> expected({
      {1, 2},
      {2, 0}
    });

    Matrix<2, TypeParam> dest = source;
    ASSERT_EQ(expected, dest);
  }

  TYPED_TEST(MatrixTest, ShouldMultiplyWithOtherSameSizedMatrix) {
    Matrix<3, TypeParam> first({
      {1, 2, 1},
      {2, 0, 1},
      {1, 1, 1}
    });

    Matrix<3, TypeParam> second({
      {1, 1, 1},
      {2, 3, 1},
      {1, 2, 1}
    });

    Matrix<3, TypeParam> expected({
      {6, 9, 4},
      {3, 4, 3},
      {4, 6, 3}
    });

    ASSERT_EQ(expected, first * second);
  }

  TYPED_TEST(MatrixTest, ShouldMultiplyInPlaceWithOtherSameSizedMatrix) {
    Matrix<3, TypeParam> first({
      {1, 2, 1},
      {2, 0, 1},
      {1, 1, 1}
    });

    Matrix<3, TypeParam> second({
      {1, 1, 1},
      {2, 3, 1},
      {1, 2, 1}
    });

    Matrix<3, TypeParam> expected({
      {6, 9, 4},
      {3, 4, 3},
      {4, 6, 3}
    });
    
    first *= second;

    ASSERT_EQ(expected, first);
  }

  TYPED_TEST(MatrixTest, ShouldReturnSameMatrixWhenMultipliedWithIdentity) {
    Matrix<3, TypeParam> first({
      {1, 2, 1},
      {2, 0, 1},
      {1, 1, 1}
    });
    Matrix<3, TypeParam> second;

    ASSERT_EQ(first, first * second);
  }

  TYPED_TEST(MatrixTest, ShouldDetermineIfTwoIdentityMatricesAreEqual) {
    Matrix<3, TypeParam> first, second;
    ASSERT_TRUE(first == second);
  }

  TYPED_TEST(MatrixTest, ShouldDetermineIfTwoMatricesAreEqual) {
    TypeParam elements[3][3] = {
      {1, 2, 1},
      {2, 0, 1},
      {1, 1, 1}
    };
    Matrix<3, TypeParam> first(elements), second(elements);
    ASSERT_TRUE(first == second);
  }

  TYPED_TEST(MatrixTest, ShouldDetermineIfTwoMatricesAreNotEqual) {
    TypeParam elements[3][3] = {
      {1, 2, 1},
      {2, 0, 1},
      {1, 1, 1}
    };
    Matrix<3, TypeParam> first(elements), second;
    ASSERT_TRUE(first != second);
  }
  
  TYPED_TEST(MatrixTest, ShouldReturnRow) {
    Matrix<3, TypeParam> matrix({
      {1, 2, 1},
      {2, 0, 1},
      {1, 1, 1}
    });
    
    Vector<3, TypeParam> expected({2, 0, 1});
    
    ASSERT_EQ(expected, matrix.row(1));
  }

  TYPED_TEST(MatrixTest, ShouldReturnCol) {
    Matrix<3, TypeParam> matrix({
      {1, 2, 1},
      {2, 0, 1},
      {1, 1, 1}
    });
    
    Vector<3, TypeParam> expected({1, 1, 1});
    
    ASSERT_EQ(expected, matrix.col(2));
  }

  TYPED_TEST(MatrixTest, ShouldCalculateRowSum) {
    Matrix<3, TypeParam> matrix({
      {1, 2, 1},
      {2, 0, 1},
      {1, 1, 1}
    });
    ASSERT_EQ(4, matrix.rowSum(0));
  }

  TYPED_TEST(MatrixTest, ShouldCalculateColSum) {
    Matrix<3, TypeParam> matrix({
      {1, 2, 1},
      {2, 0, 1},
      {1, 1, 1}
    });
    ASSERT_EQ(3, matrix.colSum(1));
  }

  TYPED_TEST(MatrixTest, ShouldReturnOriginalVectorWhenVectorIsMultipliedWithIdentityMatrix) {
    Vector<3, TypeParam> vector({1, 2, 3});
    Matrix<3, TypeParam> matrix;

    Vector<3, TypeParam> result = matrix * vector;

    ASSERT_EQ(1, result[0]);
    ASSERT_EQ(2, result[1]);
    ASSERT_EQ(3, result[2]);
  }
  
  TYPED_TEST(MatrixTest, ShouldReturnXDirectionVectorWhenMultipliedWithXUnitVector) {
    Matrix<3, TypeParam> matrix({
      {1, 2, 1},
      {2, 0, 1},
      {1, 1, 1}
    });
    
    Vector<3, TypeParam> vector({1, 0, 0});
    Vector<3, TypeParam> expected({1, 2, 1});
    ASSERT_EQ(expected, matrix * vector);
  }

  TYPED_TEST(MatrixTest, ShouldReturnYDirectionVectorWhenMultipliedWithYUnitVector) {
    Matrix<3, TypeParam> matrix({
      {1, 2, 1},
      {2, 0, 1},
      {1, 1, 1}
    });
    
    Vector<3, TypeParam> vector({0, 1, 0});
    Vector<3, TypeParam> expected({2, 0, 1});
    ASSERT_EQ(expected, matrix * vector);
  }

  TYPED_TEST(MatrixTest, ShouldReturnZDirectionVectorWhenMultipliedWithZUnitVector) {
    Matrix<3, TypeParam> matrix({
      {1, 2, 1},
      {2, 0, 1},
      {1, 1, 1}
    });
    
    Vector<3, TypeParam> vector({0, 0, 1});
    Vector<3, TypeParam> expected({1, 1, 1});
    ASSERT_EQ(expected, matrix * vector);
  }

  TYPED_TEST(MatrixTest, ShouldReturnTransformedVectorWhenMultipliedWithMatrix) {
    Matrix<3, TypeParam> matrix({
      {1, 0, 0},
      {0, 0, 1},
      {0, 1, 0}
    });

    Vector<3, TypeParam> vector({1, 2, 3});
    Vector<3, TypeParam> expected({1, 3, 2});
    ASSERT_EQ(expected, matrix * vector);
  }
  
  TYPED_TEST(MatrixTest, ShouldMultiplyIdentityWithScalar) {
    Matrix<3, TypeParam> matrix;

    Matrix<3, TypeParam> expected({
      {3, 0, 0},
      {0, 3, 0},
      {0, 0, 3}
    });

    ASSERT_EQ(expected, matrix * 3);
  }

  TYPED_TEST(MatrixTest, ShouldMultiplyWithScalar) {
    Matrix<3, TypeParam> matrix({
      {1, 2, 1},
      {2, 0, 1},
      {1, 1, 1}
    });
  
    Matrix<3, TypeParam> expected({
      {3, 6, 3},
      {6, 0, 3},
      {3, 3, 3}
    });
  
    ASSERT_EQ(expected, matrix * 3);
  }
  
  TYPED_TEST(MatrixTest, ShouldMultiplyInPlaceWithScalar) {
    Matrix<3, TypeParam> matrix({
      {1, 2, 1},
      {2, 0, 1},
      {1, 1, 1}
    });
  
    Matrix<3, TypeParam> expected({
      {3, 6, 3},
      {6, 0, 3},
      {3, 3, 3}
    });
    
    matrix *= 3;
  
    ASSERT_EQ(expected, matrix);
  }
  
  TYPED_TEST(MatrixTest, ShouldAddTwoMatrices) {
    Matrix<3, TypeParam> first({
      {1, 0, 0},
      {0, 0, 1},
      {0, 1, 0}
    });

    Matrix<3, TypeParam> second({
      {0, 1, 0},
      {0, 1, 0},
      {1, 0, 0}
    });

    Matrix<3, TypeParam> expected({
      {1, 1, 0},
      {0, 1, 1},
      {1, 1, 0}
    });
    
    ASSERT_EQ(expected, first + second);
  }

  TYPED_TEST(MatrixTest, ShouldDivideIdentityByScalar) {
    Matrix<3, TypeParam> matrix;

    Matrix<3, TypeParam> expected({
      {0.5, 0,   0},
      {0,   0.5, 0},
      {0,   0,   0.5}
    });

    ASSERT_EQ(expected, matrix / 2);
  }

  TYPED_TEST(MatrixTest, ShouldDivideByScalar) {
    Matrix<3, TypeParam> matrix({
      {1, 2, 1},
      {2, 0, 1},
      {1, 1, 1}
    });
  
    Matrix<3, TypeParam> expected({
      {0.5, 1,   0.5},
      {1,   0,   0.5},
      {0.5, 0.5, 0.5}
    });
  
    ASSERT_EQ(expected, matrix / 2);
  }

  TYPED_TEST(MatrixTest, ShouldNotAllowDivisionByZero) {
    Matrix<3, TypeParam> matrix;
    ASSERT_THROW(matrix / 0, DivisionByZeroException);
  }
  
  TYPED_TEST(MatrixTest, ShouldReturnTransposedMatrix) {
    Matrix<3, TypeParam> matrix({
      {1, 2, 1},
      {3, 0, 2},
      {4, 1, 1}
    });
    
    Matrix<3, TypeParam> expected({
      {1, 3, 4},
      {2, 0, 1},
      {1, 2, 1}
    });
    ASSERT_EQ(expected, matrix.transposed());
  }
  
  TYPED_TEST(MatrixTest, ShouldReturnIdentityWhenIdentityIsTransposed) {
    Matrix<3, TypeParam> matrix;
    ASSERT_EQ(matrix, matrix.transposed());
  }
  
  TYPED_TEST(MatrixTest, ShouldReturnOriginalMatrixIfTransposedTwice) {
    Matrix<3, TypeParam> matrix({
      {1, 2, 1},
      {3, 0, 2},
      {4, 1, 1}
    });
    ASSERT_EQ(matrix, matrix.transposed().transposed());
  }
  
  TYPED_TEST(MatrixTest, ShouldStreamMatrixToString) {
    Matrix<3, TypeParam> matrix({
      {1, 2, 1},
      {3, 0, 2},
      {4, 1, 1}
    });
    ostringstream str;
    str << matrix;
    ASSERT_EQ("1 2 1 \n3 0 2 \n4 1 1 \n", str.str());
  }
}

namespace DerivedMatrixTest {
  using namespace ::testing;

  template<class T>
  class DerivedMatrixTest : public ::testing::Test {
  };

  typedef ::testing::Types<
    Matrix2<float>, Matrix3<float>, Matrix4<float>,
    Matrix2<double>, Matrix3<double>, Matrix4<double>,
    Matrix2<long double>, Matrix3<long double>, Matrix4<long double>
  > SpecializedMatrixTypes;
  
  TYPED_TEST_SUITE(DerivedMatrixTest, SpecializedMatrixTypes);
  
  TYPED_TEST(DerivedMatrixTest, ShouldReturnCorrectTypeForMultiplicationWithMatrix) {
    TypeParam matrix;
    ASSERT_TYPES_EQ(matrix, matrix * matrix);
  }
  
  TYPED_TEST(DerivedMatrixTest, ShouldReturnCorrectTypeForMultiplicationWithVector) {
    TypeParam matrix;
    typename TypeParam::Vector vector;
    ASSERT_TYPES_EQ(vector, matrix * vector);
  }
  
  TYPED_TEST(DerivedMatrixTest, ShouldReturnCorrectTypeForMultiplicationWithScalar) {
    TypeParam matrix;
    ASSERT_TYPES_EQ(matrix, matrix * 2);
  }
  
  TYPED_TEST(DerivedMatrixTest, ShouldReturnCorrectTypeForDivisionByScalar) {
    TypeParam matrix;
    ASSERT_TYPES_EQ(matrix, matrix / 2);
  }

  TYPED_TEST(DerivedMatrixTest, ShouldReturnCorrectTypeForMatrixAddition) {
    TypeParam matrix;
    ASSERT_TYPES_EQ(matrix, matrix + matrix);
  }
  
  TYPED_TEST(DerivedMatrixTest, ShouldReturnCorrectTypeForTransposedMatrix) {
    TypeParam matrix;
    ASSERT_TYPES_EQ(matrix, matrix.transposed());
  }
  
  TYPED_TEST(DerivedMatrixTest, ShouldEvaluateEquality) {
    TypeParam matrix1, matrix2;
    ASSERT_TRUE(matrix1 == matrix2);
  }
}

namespace Matrix2Test {
  using namespace ::testing;

  template<class T>
  class Matrix2Test : public ::testing::Test {
  };

  typedef ::testing::Types<float, double, long double> SpecializedMatrixTypes;
  
  TYPED_TEST_SUITE(Matrix2Test, SpecializedMatrixTypes);

  TYPED_TEST(Matrix2Test, ShouldInitializeIdentity) {
    Matrix2<TypeParam> matrix;
    ASSERT_EQ(1, matrix[0][0]);
  }

  TYPED_TEST(Matrix2Test, ShouldInitializeWithCells) {
    Matrix2<TypeParam> matrix(
      1, 1,
      0, 1
    );
    ASSERT_EQ(1, matrix[0][0]);
    ASSERT_EQ(1, matrix[0][1]);
    ASSERT_EQ(0, matrix[1][0]);
    ASSERT_EQ(1, matrix[1][1]);
  }
  
  TYPED_TEST(Matrix2Test, ShouldCopyFromMatrix3) {
    Matrix3<TypeParam> matrix(
      2, 1, 2,
      3, 3, 2,
      4, 1, 2
    );
    Matrix2<TypeParam> copy(matrix);
    Matrix2<TypeParam> expected(
      2, 1,
      3, 3
    );
    
    ASSERT_EQ(expected, copy);
  }
  
  TYPED_TEST(Matrix2Test, ShouldHaveNonZeroDeterminantForIdentityMatrix) {
    Matrix2<TypeParam> matrix;
    ASSERT_TRUE(matrix.determinant() != 0);
  }
  
  TYPED_TEST(Matrix2Test, ShouldHaveZeroDeterminantForNonInvertibleMatrix) {
    Matrix2<TypeParam> matrix(
      0, 0,
      1, 0
    );
    ASSERT_EQ(0, matrix.determinant());
  }
  
  TYPED_TEST(Matrix2Test, ShouldReturnIdentityWhenIdentityIsInverted) {
    Matrix2<TypeParam> matrix;
    ASSERT_EQ(matrix, matrix.inverted());
  }
  
  TYPED_TEST(Matrix2Test, ShouldReturnInvertedMatrix) {
    Matrix2<TypeParam> matrix(
      0, -1,
      1, 0
    );
    Matrix2<TypeParam> expected(
      0, 1,
      -1, 0
    );
    
    ASSERT_EQ(expected, matrix.inverted());
  }
  
  TYPED_TEST(Matrix2Test, ShouldGenerate90DegreeRotationMatrix) {
    auto angle = Angle<TypeParam>::fromDegrees(90);
    auto matrix = Matrix2<TypeParam>::rotate(angle);
    auto expected = Matrix2<TypeParam>(
      0, -1,
      1, 0
    );
    ASSERT_MATRIX_NEAR(expected, matrix, 0.0001);
  }
  
  TYPED_TEST(Matrix2Test, ShouldRotateCounterclockwiseByDefault) {
    auto angle = Angle<TypeParam>::fromRadians(0.354);
    auto expected = Matrix2<TypeParam>::rotate(angle),
         actual = Matrix2<TypeParam>::counterclockwise(angle);
    ASSERT_EQ(expected, actual);
  }
  
  TYPED_TEST(Matrix2Test, ShouldRotateClockwiseByUsingNegativeAngle) {
    auto angle = Angle<TypeParam>::fromRadians(0.354);

    auto expected = Matrix2<TypeParam>::rotate(-angle),
         actual = Matrix2<TypeParam>::clockwise(angle);
    ASSERT_EQ(expected, actual);
  }
  
  TYPED_TEST(Matrix2Test, ShouldRotateClockwise) {
    auto angle = Angle<TypeParam>::fromDegrees(90);
    auto matrix = Matrix2<TypeParam>::clockwise(angle);
    auto expected = Matrix2<TypeParam>(
      0, 1,
      -1, 0
    );
    ASSERT_MATRIX_NEAR(expected, matrix, 0.0001);
  }
  
  TYPED_TEST(Matrix2Test, ShouldRotateCounterClockwise) {
    auto angle = Angle<TypeParam>::fromDegrees(90);
    auto matrix = Matrix2<TypeParam>::counterclockwise(angle);
    auto expected = Matrix2<TypeParam>(
      0, -1,
      1, 0
    );
    ASSERT_MATRIX_NEAR(expected, matrix, 0.0001);
  }

  TYPED_TEST(Matrix2Test, ShouldGenerateScalingMatrix) {
    Matrix2<TypeParam> matrix = Matrix2<TypeParam>::scale(2);
    Matrix2<TypeParam> expected(
      2, 0,
      0, 2
    );
    ASSERT_EQ(expected, matrix);
  }
  
  TYPED_TEST(Matrix2Test, ShouldScaleVector) {
    Matrix2<TypeParam> matrix = Matrix2<TypeParam>::scale(2);
    Vector2<TypeParam> vector(1, 1), expected(2, 2);
    ASSERT_EQ(expected, matrix * vector);
  }
  
  TYPED_TEST(Matrix2Test, ShouldGenerateScalingMatrixWithTwoIndependentFactors) {
    Matrix2<TypeParam> matrix = Matrix2<TypeParam>::scale(2, 3);
    Matrix2<TypeParam> expected(
      2, 0,
      0, 3
    );
    ASSERT_EQ(expected, matrix);
  }
  
  TYPED_TEST(Matrix2Test, ShouldScaleVectorWithTwoIndependentFactors) {
    Matrix2<TypeParam> matrix = Matrix2<TypeParam>::scale(2, 3);
    Vector2<TypeParam> vector(1, 1), expected(2, 3);
    ASSERT_EQ(expected, matrix * vector);
  }
  
  TYPED_TEST(Matrix2Test, ShouldGenerateShearingMatrix) {
    Matrix2<TypeParam> matrix = Matrix2<TypeParam>::shear(1, 2),
                   expected(1, 1, 2, 1);
    ASSERT_EQ(expected, matrix);
  }
  
  TYPED_TEST(Matrix2Test, ShouldShearVector) {
    Matrix2<TypeParam> matrix = Matrix2<TypeParam>::shear(1, 2);
    Vector2<TypeParam> vector(1, 1), expected(2, 3);
    ASSERT_EQ(expected, matrix * vector);
  }
  
  TYPED_TEST(Matrix2Test, ShouldGenerateReflectionMatrix) {
    Matrix2<TypeParam> matrix = Matrix2<TypeParam>::reflect(1, 1);
    Matrix2<TypeParam> expected(
      0, 1,
      1, 0
    );
    ASSERT_MATRIX_NEAR(expected, matrix, 0.0001);
  }

  TYPED_TEST(Matrix2Test, ShouldGenerateReflectionMatrixFromVector) {
    Matrix2<TypeParam> matrix = Matrix2<TypeParam>::reflect(Vector2<TypeParam>(1, 1));
    Matrix2<TypeParam> expected(
      0, 1,
      1, 0
    );
    ASSERT_MATRIX_NEAR(expected, matrix, 0.0001);
  }
  
  TYPED_TEST(Matrix2Test, ShouldReflectVector) {
    Matrix2<TypeParam> matrix = Matrix2<TypeParam>::reflect(1, 1);
    Vector2<TypeParam> vector(-1, 1), expected(1, -1);
    
    ASSERT_VECTOR_NEAR(expected, matrix * vector, 0.0001);
  }

  TYPED_TEST(Matrix2Test, ShouldGenerateProjectionMatrix) {
    Matrix2<TypeParam> matrix = Matrix2<TypeParam>::project(1, 1);
    Matrix2<TypeParam> expected(
      0.5, 0.5,
      0.5, 0.5
    );
    ASSERT_MATRIX_NEAR(expected, matrix, 0.0001);
  }

  TYPED_TEST(Matrix2Test, ShouldGenerateProjectionMatrixFromVector) {
    Matrix2<TypeParam> matrix = Matrix2<TypeParam>::project(Vector2<TypeParam>(1, 1));
    Matrix2<TypeParam> expected(
      0.5, 0.5,
      0.5, 0.5
    );
    ASSERT_MATRIX_NEAR(expected, matrix, 0.0001);
  }
  
  TYPED_TEST(Matrix2Test, ShouldProjectVector) {
    Matrix2<TypeParam> matrix = Matrix2<TypeParam>::project(1, 1);
    Vector2<TypeParam> vector(-1, 1), expected(0, 0);
    
    ASSERT_EQ(expected, matrix * vector);
  }
  
  TYPED_TEST(Matrix2Test, ShouldHaveIdentityConstant) {
    Matrix2<TypeParam> expected(
      1, 0,
      0, 1
    );
    ASSERT_EQ(expected, Matrix2<TypeParam>::identity());
  }
  
  TYPED_TEST(Matrix2Test, ShouldHave90DegreeRotationConstant) {
    Matrix2<TypeParam> expected(
      0, -1,
      1, 0
    );
    ASSERT_EQ(expected, Matrix2<TypeParam>::rotate90());
  }
  
  TYPED_TEST(Matrix2Test, ShouldHave180DegreeRotationConstant) {
    Matrix2<TypeParam> expected(
      -1, 0,
      0, -1
    );
    ASSERT_EQ(expected, Matrix2<TypeParam>::rotate180());
  }
  
  TYPED_TEST(Matrix2Test, ShouldHave270DegreeRotationConstant) {
    Matrix2<TypeParam> expected(
      0, 1,
      -1, 0
    );
    ASSERT_EQ(expected, Matrix2<TypeParam>::rotate270());
  }
  
  TYPED_TEST(Matrix2Test, ShouldHaveXReflectionConstant) {
    Matrix2<TypeParam> expected(
      -1, 0,
      0, 1
    );
    ASSERT_EQ(expected, Matrix2<TypeParam>::reflectX());
  }
  
  TYPED_TEST(Matrix2Test, ShouldHaveYReflectionConstant) {
    Matrix2<TypeParam> expected(
      1, 0,
      0, -1
    );
    ASSERT_EQ(expected, Matrix2<TypeParam>::reflectY());
  }
  
  TYPED_TEST(Matrix2Test, ShouldHaveXUnitVectorConstant) {
    Vector2<TypeParam> expected(1, 0);
    ASSERT_EQ(expected, Matrix2<TypeParam>::xUnit());
  }
  
  TYPED_TEST(Matrix2Test, ShouldHaveYUnitVectorConstant) {
    Vector2<TypeParam> expected(0, 1);
    ASSERT_EQ(expected, Matrix2<TypeParam>::yUnit());
  }
}

namespace Matrix3Test {
  using namespace ::testing;

  template<class T>
  class Matrix3Test : public ::testing::Test {
  };

  typedef ::testing::Types<float, double, long double> SpecializedMatrixTypes;
  
  TYPED_TEST_SUITE(Matrix3Test, SpecializedMatrixTypes);

  TYPED_TEST(Matrix3Test, ShouldInitializeIdentity) {
    Matrix3<TypeParam> matrix;
    ASSERT_EQ(1, matrix[0][0]);
  }

  TYPED_TEST(Matrix3Test, ShouldInitializeWithCells) {
    Matrix3<TypeParam> matrix(
      1, 1, 2,
      0, 1, 0,
      2, 1, 1
    );
    ASSERT_EQ(1, matrix[0][0]);
    ASSERT_EQ(1, matrix[0][1]);
    ASSERT_EQ(2, matrix[0][2]);
    ASSERT_EQ(0, matrix[1][0]);
    ASSERT_EQ(1, matrix[1][1]);
    ASSERT_EQ(0, matrix[1][2]);
    ASSERT_EQ(2, matrix[2][0]);
    ASSERT_EQ(1, matrix[2][1]);
    ASSERT_EQ(1, matrix[2][2]);
  }
  
  TYPED_TEST(Matrix3Test, ShouldCopyFromMatrix2) {
    Matrix2<TypeParam> matrix(
      2, 1,
      1, 2
    );
    Matrix3<TypeParam> copy(matrix), expected(
      2, 1, 0,
      1, 2, 0,
      0, 0, 1
    );
    
    ASSERT_EQ(expected, copy);
  }
  
  TYPED_TEST(Matrix3Test, ShouldCopyFromMatrix4) {
    Matrix4<TypeParam> matrix(
      2, 1, 3, 0,
      1, 2, 1, 0,
      4, 3, 3, 0,
      0, 0, 0, 1
    );
    Matrix3<TypeParam> copy(matrix), expected(
      2, 1, 3,
      1, 2, 1,
      4, 3, 3
    );
    
    ASSERT_EQ(expected, copy);
  }
  
  TYPED_TEST(Matrix3Test, ShouldRotateAroundXAxis) {
    Matrix3<TypeParam> matrix = Matrix3<TypeParam>::rotateX(Angle<TypeParam>::fromDegrees(90));
    Matrix3<TypeParam> expected(
      1, 0, 0,
      0, 0, -1,
      0, 1, 0
    );
    ASSERT_MATRIX_NEAR(expected, matrix, 0.0001);
  }
  
  TYPED_TEST(Matrix3Test, ShouldRotateAroundYAxis) {
    Matrix3<TypeParam> matrix = Matrix3<TypeParam>::rotateY(Angle<TypeParam>::fromDegrees(90));
    Matrix3<TypeParam> expected(
      0, 0, 1,
      0, 1, 0,
      -1, 0, 0
    );
    ASSERT_MATRIX_NEAR(expected, matrix, 0.0001);
  }
  
  TYPED_TEST(Matrix3Test, ShouldRotateAroundZAxis) {
    Matrix3<TypeParam> matrix = Matrix3<TypeParam>::rotateZ(Angle<TypeParam>::fromDegrees(90));
    Matrix3<TypeParam> expected(
      0, -1, 0,
      1, 0, 0,
      0, 0, 1
    );
    ASSERT_MATRIX_NEAR(expected, matrix, 0.0001);
  }

  TYPED_TEST(Matrix3Test, ShouldRotateAroundAllAxes) {
    auto angle = Angle<TypeParam>::fromDegrees(45);
    auto matrix = Matrix3<TypeParam>::rotate(angle, angle, angle);
    
    auto expected = Matrix3<TypeParam>::rotateZ(angle) *
                    Matrix3<TypeParam>::rotateY(angle) *
                    Matrix3<TypeParam>::rotateX(angle);
    
    ASSERT_MATRIX_NEAR(expected, matrix, 0.0001);
  }

  TYPED_TEST(Matrix3Test, ShouldRotateAroundAllAxesFromVector) {
    auto matrix = Matrix3<TypeParam>::rotate(Vector3d(1.1, 1.1, 1.1));
    
    auto angle = Angle<TypeParam>::fromRadians(1.1);
    auto expected = Matrix3<TypeParam>::rotateZ(angle) *
                    Matrix3<TypeParam>::rotateY(angle) *
                    Matrix3<TypeParam>::rotateX(angle);
    
    ASSERT_MATRIX_NEAR(expected, matrix, 0.0001);
  }
  
  TYPED_TEST(Matrix3Test, ShouldScaleUniformly) {
    Matrix3<TypeParam> matrix = Matrix3<TypeParam>::scale(2.0);
    Matrix3<TypeParam> expected(
      2, 0, 0,
      0, 2, 0,
      0, 0, 2
    );
    ASSERT_EQ(expected, matrix);
  }
  
  TYPED_TEST(Matrix3Test, ShouldScaleDifferentlyForEachAxis) {
    Matrix3<TypeParam> matrix = Matrix3<TypeParam>::scale(2, 3, 4);
    Matrix3<TypeParam> expected(
      2, 0, 0,
      0, 3, 0,
      0, 0, 4
    );
    ASSERT_EQ(expected, matrix);
  }
  
  TYPED_TEST(Matrix3Test, ShouldReturnScaleVector) {
    Matrix3<TypeParam> matrix =
      Matrix3<TypeParam>::rotate(Vector3<TypeParam>(2, 2, 1)) *
      Matrix3<TypeParam>::scale(1, 4, 3);
    
    ASSERT_VECTOR_NEAR(Vector3<TypeParam>(1, 4, 3), matrix.scaleVector(), 0.001);
  }
  
  TYPED_TEST(Matrix3Test, ShouldReturnRotationQuaternion) {
    Matrix3<TypeParam> matrix = Matrix3<TypeParam>::rotateX(
      Angle<TypeParam>::fromDegrees(90)
    );
    Quaternion<TypeParam> quat = matrix.rotationQuaternion();
    ASSERT_EQ(1, quat.length());
  }
  
  TYPED_TEST(Matrix3Test, ShouldReturnRotationVector) {
    Angle<TypeParam> x = Angle<TypeParam>::fromDegrees(5),
                     y = Angle<TypeParam>::fromDegrees(15),
                     z = Angle<TypeParam>::fromDegrees(8);
    Matrix3<TypeParam> matrix =
      Matrix3<TypeParam>::rotateZ(z) *
      Matrix3<TypeParam>::rotateY(y) *
      Matrix3<TypeParam>::rotateX(x);
    Vector3<TypeParam> rot = matrix.rotationVector();
    
    ASSERT_NEAR(x.radians(), rot.x(), 0.001);
    ASSERT_NEAR(y.radians(), rot.y(), 0.001);
    ASSERT_NEAR(z.radians(), rot.z(), 0.001);
  }
}

namespace Matrix4Test {
  using namespace ::testing;

  template<class T>
  class Matrix4Test : public ::testing::Test {
  };

  typedef ::testing::Types<float, double, long double> SpecializedMatrixTypes;

  TYPED_TEST_SUITE(Matrix4Test, SpecializedMatrixTypes);

  TYPED_TEST(Matrix4Test, ShouldInitializeIdentity) {
    Matrix4<TypeParam> matrix;
    ASSERT_EQ(1, matrix[0][0]);
  }

  TYPED_TEST(Matrix4Test, ShouldInitializeWithCells) {
    Matrix4<TypeParam> matrix(
      1, 1, 2, 0,
      0, 1, 0, 0,
      2, 1, 1, 0,
      0, 0, 0, 1
    );
    ASSERT_EQ(1, matrix[0][0]);
    ASSERT_EQ(1, matrix[0][1]);
    ASSERT_EQ(2, matrix[0][2]);
    ASSERT_EQ(0, matrix[0][3]);
    ASSERT_EQ(0, matrix[1][0]);
    ASSERT_EQ(1, matrix[1][1]);
    ASSERT_EQ(0, matrix[1][2]);
    ASSERT_EQ(0, matrix[1][3]);
    ASSERT_EQ(2, matrix[2][0]);
    ASSERT_EQ(1, matrix[2][1]);
    ASSERT_EQ(1, matrix[2][2]);
    ASSERT_EQ(0, matrix[2][3]);
    ASSERT_EQ(0, matrix[3][0]);
    ASSERT_EQ(0, matrix[3][1]);
    ASSERT_EQ(0, matrix[3][2]);
    ASSERT_EQ(1, matrix[3][3]);
  }
  
  TYPED_TEST(Matrix4Test, ShouldInitializeFromThree3DVectors) {
    Matrix4<TypeParam> matrix(
      Vector3f(1, 1, 2),
      Vector3f(0, 1, 0),
      Vector3f(2, 1, 1)
    );
    ASSERT_EQ(1, matrix[0][0]);
    ASSERT_EQ(1, matrix[0][1]);
    ASSERT_EQ(2, matrix[0][2]);
    ASSERT_EQ(0, matrix[1][0]);
    ASSERT_EQ(1, matrix[1][1]);
    ASSERT_EQ(0, matrix[1][2]);
    ASSERT_EQ(2, matrix[2][0]);
    ASSERT_EQ(1, matrix[2][1]);
    ASSERT_EQ(1, matrix[2][2]);
  }
  
  TYPED_TEST(Matrix4Test, ShouldCopyFromMatrix3) {
    Matrix3<TypeParam> matrix(
      2, 1, 3,
      1, 2, 1,
      4, 3, 3
    );
    Matrix4<TypeParam> copy(matrix), expected(
      2, 1, 3, 0,
      1, 2, 1, 0,
      4, 3, 3, 0,
      0, 0, 0, 1
    );
    
    ASSERT_EQ(expected, copy);
  }

  TYPED_TEST(Matrix4Test, ShouldReturnVector4WhenMultipliedWithVector) {
    Matrix4<TypeParam> matrix;
    Vector4<TypeParam> vector(1, 2, 3);
    Vector4<TypeParam> result = matrix * vector;
    ASSERT_EQ(vector, result);
  }
  
  TYPED_TEST(Matrix4Test, ShouldHaveNonZeroDeterminantForIdentityMatrix) {
    Matrix4<TypeParam> matrix;
    ASSERT_TRUE(matrix.determinant() != 0);
  }
  
  TYPED_TEST(Matrix4Test, ShouldHaveZeroDeterminantForNonInvertibleMatrix) {
    Matrix4<TypeParam> matrix(
      0, 0, 0, 0,
      0, 1, 0, 0,
      0, 0, 1, 0,
      0, 0, 0, 1
    );
    ASSERT_EQ(0, matrix.determinant());
  }
  
  TYPED_TEST(Matrix4Test, ShouldReturnIdentityWhenIdentityIsInverted) {
    Matrix4<TypeParam> matrix;
    ASSERT_EQ(matrix, matrix.inverted());
  }
  
  TYPED_TEST(Matrix4Test, ShouldReturnOriginalMatrixWhenInvertedTwice) {
    Matrix4<TypeParam> matrix(
      1, 0, 0, 0,
      0, 1, 0, 0,
      0, 1, 1, 0,
      0, 0, 0, 1
    );
    ASSERT_EQ(matrix, matrix.inverted().inverted());
  }
  
  TYPED_TEST(Matrix4Test, ShouldReturnInvertedMatrix) {
    Matrix4<TypeParam> matrix(
      1, 0, 0, 0,
      0, 1, 0, 0,
      0, 1, 1, 0,
      0, 0, 0, 1
    );
    Matrix4<TypeParam> expected(
      1, 0, 0, 0,
      0, 1, 0, 0,
      0, -1, 1, 0,
      0, 0, 0, 1
    );
    
    ASSERT_EQ(expected, matrix.inverted());
  }
  
  // Numerical-stability battery: verify ||M·M^{-1} - I|| stays tight.
  // These exercise the block-inverse path against well-conditioned and
  // near-singular inputs.

  TYPED_TEST(Matrix4Test, ShouldHaveSmallResidualForTRSMatrix) {
    // Rotation 30° + translation, condition number ≈ 1.
    Matrix4<TypeParam> m(
      TypeParam(0.866), TypeParam(-0.5),  TypeParam(0), TypeParam(1),
      TypeParam(0.5),   TypeParam(0.866), TypeParam(0), TypeParam(2),
      TypeParam(0),     TypeParam(0),     TypeParam(1), TypeParam(3),
      TypeParam(0),     TypeParam(0),     TypeParam(0), TypeParam(1)
    );
    Matrix4<TypeParam> identity;
    ASSERT_MATRIX_NEAR(identity, m * m.inverted(), 1e-4);
  }

  TYPED_TEST(Matrix4Test, ShouldHaveSmallResidualForNearSingularMatrix) {
    // Rows 0 and 1 are nearly parallel: det(top-left 2×2) ≈ 0.01.
    // Condition number ≈ 200 — a genuine stress test for numerical stability.
    Matrix4<TypeParam> m(
      TypeParam(1),     TypeParam(0.995), TypeParam(0), TypeParam(0),
      TypeParam(0.995), TypeParam(1),     TypeParam(0), TypeParam(0),
      TypeParam(0),     TypeParam(0),     TypeParam(1), TypeParam(0),
      TypeParam(0),     TypeParam(0),     TypeParam(0), TypeParam(1)
    );
    Matrix4<TypeParam> identity;
    ASSERT_MATRIX_NEAR(identity, m * m.inverted(), 1e-3);
  }

  TYPED_TEST(Matrix4Test, ShouldHaveSmallResidualForLargeTranslationMatrix) {
    // Large translation values stress the off-diagonal blocks.
    Matrix4<TypeParam> m(
      TypeParam(1), TypeParam(0), TypeParam(0), TypeParam(1000),
      TypeParam(0), TypeParam(1), TypeParam(0), TypeParam(1000),
      TypeParam(0), TypeParam(0), TypeParam(1), TypeParam(1000),
      TypeParam(0), TypeParam(0), TypeParam(0), TypeParam(1)
    );
    Matrix4<TypeParam> identity;
    ASSERT_MATRIX_NEAR(identity, m * m.inverted(), 1e-4);
  }

  TYPED_TEST(Matrix4Test, ShouldReturnTranslationMatrixFromVector) {
    Matrix4<TypeParam> expected(
      1, 0, 0, 1,
      0, 1, 0, 2,
      0, 0, 1, 3,
      0, 0, 0, 1
    );
    ASSERT_EQ(expected, Matrix4<TypeParam>::translate(Vector3f(1, 2, 3)));
  }
  
  TYPED_TEST(Matrix4Test, ShouldReturnTranslationMatrixFromCoordinates) {
    Matrix4<TypeParam> expected(
      1, 0, 0, 1,
      0, 1, 0, 2,
      0, 0, 1, 3,
      0, 0, 0, 1
    );
    ASSERT_EQ(expected, Matrix4<TypeParam>::translate(1, 2, 3));
  }
  
  TYPED_TEST(Matrix4Test, ShouldExtractTranslationFromMatrix) {
    Vector3f expected(1, 2, 3);
    ASSERT_EQ(expected, Matrix4<TypeParam>::translate(expected).translationVector());
  }

  TYPED_TEST(Matrix4Test, LookAtShouldPlaceEyeAtTranslationColumn) {
    Vector3<TypeParam> eye(0, 0, -1), target(0, 0, 0), up(0, 1, 0);
    auto m = Matrix4<TypeParam>::lookAt(eye, target, up);
    ASSERT_NEAR(TypeParam(0),  m.cell(0, 3), TypeParam(0.0001));
    ASSERT_NEAR(TypeParam(0),  m.cell(1, 3), TypeParam(0.0001));
    ASSERT_NEAR(TypeParam(-1), m.cell(2, 3), TypeParam(0.0001));
    ASSERT_NEAR(TypeParam(1),  m.cell(3, 3), TypeParam(0.0001));
  }

  TYPED_TEST(Matrix4Test, LookAtShouldAlignForwardColumnWithTargetDirection) {
    // Camera at origin looking along +X toward (1,0,0)
    Vector3<TypeParam> eye(0, 0, 0), target(1, 0, 0), up(0, 1, 0);
    auto m = Matrix4<TypeParam>::lookAt(eye, target, up);
    // col 2 is the forward (Z) axis in world space
    ASSERT_NEAR(TypeParam(1), m.cell(0, 2), TypeParam(0.0001));
    ASSERT_NEAR(TypeParam(0), m.cell(1, 2), TypeParam(0.0001));
    ASSERT_NEAR(TypeParam(0), m.cell(2, 2), TypeParam(0.0001));
  }

  TYPED_TEST(Matrix4Test, LookAtShouldProduceOrthonormalBasis) {
    Vector3<TypeParam> eye(3, 2, 1), target(0, 0, 0), up(0, 1, 0);
    auto m = Matrix4<TypeParam>::lookAt(eye, target, up);
    // Columns 0, 1, 2 are the right, up, forward axes — each should be unit
    for (int col = 0; col < 3; ++col) {
      TypeParam len2 = m.cell(0, col) * m.cell(0, col)
                     + m.cell(1, col) * m.cell(1, col)
                     + m.cell(2, col) * m.cell(2, col);
      ASSERT_NEAR(TypeParam(1), len2, TypeParam(0.0001));
    }
    // Columns should be pairwise orthogonal
    auto dot01 = m.cell(0,0)*m.cell(0,1) + m.cell(1,0)*m.cell(1,1) + m.cell(2,0)*m.cell(2,1);
    auto dot02 = m.cell(0,0)*m.cell(0,2) + m.cell(1,0)*m.cell(1,2) + m.cell(2,0)*m.cell(2,2);
    auto dot12 = m.cell(0,1)*m.cell(0,2) + m.cell(1,1)*m.cell(1,2) + m.cell(2,1)*m.cell(2,2);
    ASSERT_NEAR(TypeParam(0), dot01, TypeParam(0.0001));
    ASSERT_NEAR(TypeParam(0), dot02, TypeParam(0.0001));
    ASSERT_NEAR(TypeParam(0), dot12, TypeParam(0.0001));
  }

  TYPED_TEST(Matrix4Test, LookAtShouldMatchKnownCameraMatrix) {
    // Camera at (0,0,-1) looking at origin — matches existing CameraTest expectation
    Vector3<TypeParam> eye(0, 0, -1), target(0, 0, 0), up(0, 1, 0);
    Matrix4<TypeParam> expected(
      1, 0, 0,  0,
      0, 1, 0,  0,
      0, 0, 1, -1,
      0, 0, 0,  1
    );
    ASSERT_MATRIX_NEAR(expected, Matrix4<TypeParam>::lookAt(eye, target, up), TypeParam(0.0001));
  }

  TYPED_TEST(Matrix4Test, PerspectiveShouldMapNearPlaneToMinusOne) {
    auto fov = Angle<TypeParam>::fromDegrees(90);
    auto m = Matrix4<TypeParam>::perspective(fov, TypeParam(1), TypeParam(1), TypeParam(10));
    // z_ndc at z_eye = 1 (near): (z_clip) / w_clip = (M[2][2]*1 + M[2][3]) / 1
    TypeParam z_clip = m.cell(2, 2) * TypeParam(1) + m.cell(2, 3) * TypeParam(1);
    TypeParam w_clip = m.cell(3, 2) * TypeParam(1);
    ASSERT_NEAR(TypeParam(-1), z_clip / w_clip, TypeParam(0.0001));
  }

  TYPED_TEST(Matrix4Test, PerspectiveShouldMapFarPlaneToOne) {
    auto fov = Angle<TypeParam>::fromDegrees(90);
    auto m = Matrix4<TypeParam>::perspective(fov, TypeParam(1), TypeParam(1), TypeParam(10));
    TypeParam z_clip = m.cell(2, 2) * TypeParam(10) + m.cell(2, 3) * TypeParam(1);
    TypeParam w_clip = m.cell(3, 2) * TypeParam(10);
    ASSERT_NEAR(TypeParam(1), z_clip / w_clip, TypeParam(0.0001));
  }

  TYPED_TEST(Matrix4Test, PerspectiveShouldMapEdgeXToOne) {
    // 90-degree fovY, aspect 1: at z=near=1 the visible x range is [-1, 1]
    auto fov = Angle<TypeParam>::fromDegrees(90);
    auto m = Matrix4<TypeParam>::perspective(fov, TypeParam(1), TypeParam(1), TypeParam(10));
    // x_ndc = M[0][0]*x / z_eye; at x=1, z=1: x_ndc = M[0][0]*1/1
    TypeParam x_ndc = m.cell(0, 0) * TypeParam(1) / TypeParam(1);
    ASSERT_NEAR(TypeParam(1), x_ndc, TypeParam(0.0001));
  }

  TYPED_TEST(Matrix4Test, OrthographicShouldMapNearToMinusOne) {
    auto m = Matrix4<TypeParam>::orthographic(
      TypeParam(-2), TypeParam(2), TypeParam(-2), TypeParam(2),
      TypeParam(1), TypeParam(10)
    );
    TypeParam z_ndc = m.cell(2, 2) * TypeParam(1) + m.cell(2, 3);
    ASSERT_NEAR(TypeParam(-1), z_ndc, TypeParam(0.0001));
  }

  TYPED_TEST(Matrix4Test, OrthographicShouldMapFarToOne) {
    auto m = Matrix4<TypeParam>::orthographic(
      TypeParam(-2), TypeParam(2), TypeParam(-2), TypeParam(2),
      TypeParam(1), TypeParam(10)
    );
    TypeParam z_ndc = m.cell(2, 2) * TypeParam(10) + m.cell(2, 3);
    ASSERT_NEAR(TypeParam(1), z_ndc, TypeParam(0.0001));
  }

  TYPED_TEST(Matrix4Test, OrthographicShouldMapRightEdgeXToOne) {
    auto m = Matrix4<TypeParam>::orthographic(
      TypeParam(-2), TypeParam(2), TypeParam(-2), TypeParam(2),
      TypeParam(1), TypeParam(10)
    );
    TypeParam x_ndc = m.cell(0, 0) * TypeParam(2) + m.cell(0, 3);
    ASSERT_NEAR(TypeParam(1), x_ndc, TypeParam(0.0001));
  }

  TYPED_TEST(Matrix4Test, OrthographicShouldMapLeftEdgeXToMinusOne) {
    auto m = Matrix4<TypeParam>::orthographic(
      TypeParam(-2), TypeParam(2), TypeParam(-2), TypeParam(2),
      TypeParam(1), TypeParam(10)
    );
    TypeParam x_ndc = m.cell(0, 0) * TypeParam(-2) + m.cell(0, 3);
    ASSERT_NEAR(TypeParam(-1), x_ndc, TypeParam(0.0001));
  }

  TYPED_TEST(Matrix4Test, FrustumShouldMapNearPlaneToMinusOne) {
    auto m = Matrix4<TypeParam>::frustum(
      TypeParam(-1), TypeParam(1), TypeParam(-1), TypeParam(1),
      TypeParam(1), TypeParam(10)
    );
    TypeParam z_clip = m.cell(2, 2) * TypeParam(1) + m.cell(2, 3) * TypeParam(1);
    TypeParam w_clip = m.cell(3, 2) * TypeParam(1);
    ASSERT_NEAR(TypeParam(-1), z_clip / w_clip, TypeParam(0.0001));
  }

  TYPED_TEST(Matrix4Test, FrustumShouldMapFarPlaneToOne) {
    auto m = Matrix4<TypeParam>::frustum(
      TypeParam(-1), TypeParam(1), TypeParam(-1), TypeParam(1),
      TypeParam(1), TypeParam(10)
    );
    TypeParam z_clip = m.cell(2, 2) * TypeParam(10) + m.cell(2, 3) * TypeParam(1);
    TypeParam w_clip = m.cell(3, 2) * TypeParam(10);
    ASSERT_NEAR(TypeParam(1), z_clip / w_clip, TypeParam(0.0001));
  }

  TYPED_TEST(Matrix4Test, FrustumShouldMatchPerspectiveForSymmetricCase) {
    // frustum(-1,1,-1,1,1,10) == perspective(90°, 1, 1, 10) because
    // tan(45°) = 1, so half-width = near * tan(fovY/2) = 1 * 1 = 1
    auto fov = Angle<TypeParam>::fromDegrees(90);
    auto mp = Matrix4<TypeParam>::perspective(fov, TypeParam(1), TypeParam(1), TypeParam(10));
    auto mf = Matrix4<TypeParam>::frustum(
      TypeParam(-1), TypeParam(1), TypeParam(-1), TypeParam(1),
      TypeParam(1), TypeParam(10)
    );
    ASSERT_MATRIX_NEAR(mp, mf, TypeParam(0.0001));
  }
}
