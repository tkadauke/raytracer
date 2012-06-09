#include "gtest.h"
#include "math/Matrix.h"
#include "math/Vector.h"
#include "test/helpers/VectorTestHelper.h"
#include "test/helpers/MatrixTestHelper.h"
#include "test/helpers/TypeTestHelper.h"

namespace MatrixTest {
  using namespace ::testing;
  
  TEST(Matrix, ShouldInitializeMatrixAsIdentity) {
    Matrix<3, float> matrix;
    ASSERT_EQ(1, matrix[0][0]);
    ASSERT_EQ(0, matrix[1][0]);
  }

  TEST(Matrix, ShouldGetAndSetCell) {
    Matrix<3, float> matrix;
    matrix.setCell(0, 2, 10);
    ASSERT_EQ(10, matrix.cell(0, 2));
  }

  TEST(Matrix, ShouldGetAndSetCellWithIndexOperator) {
    Matrix<3, float> matrix;
    matrix[0][2] = 10;
    ASSERT_EQ(10, matrix[0][2]);
  }

  TEST(Matrix, ShouldConstructFromNestedCArray) {
    float elements[3][3] = { {1, 2, 1}, {2, 0, 1}, {1, 1, 1} };
    Matrix<3, float> matrix(elements);
    ASSERT_EQ(1, matrix[0][0]);
    ASSERT_EQ(2, matrix[1][0]);
    ASSERT_EQ(1, matrix[2][0]);
  }
  
  TEST(Matrix, ShouldCopyFromSmallerMatrix) {
    float elements[3][3] = { {1, 2, 1}, {2, 0, 1}, {1, 1, 1} };
    Matrix<3, float> source(elements);
    
    float expected_elements[4][4] = { {1, 2, 1, 0}, {2, 0, 1, 0}, {1, 1, 1, 0}, {0, 0, 0, 1} };
    Matrix<4, float> expected(expected_elements);

    Matrix<4, float> dest = source;
    ASSERT_EQ(expected, dest);
  }

  TEST(Matrix, ShouldCopyFromLargerMatrix) {
    float elements[3][3] = { {1, 2, 1}, {2, 0, 1}, {1, 1, 1} };
    Matrix<3, float> source(elements);
    
    float expected_elements[2][2] = { {1, 2}, {2, 0} };
    Matrix<2, float> expected(expected_elements);

    Matrix<2, float> dest = source;
    ASSERT_EQ(expected, dest);
  }

  TEST(Matrix, ShouldMultiplyWithOtherSameSizedMatrix) {
    float first_elements[3][3] = { {1, 2, 1}, {2, 0, 1}, {1, 1, 1} };
    Matrix<3, float> first(first_elements);

    float second_elements[3][3] = { {1, 1, 1}, {2, 3, 1}, {1, 2, 1} };
    Matrix<3, float> second(second_elements);

    float expected_elements[3][3] = { {6, 9, 4}, {3, 4, 3}, {4, 6, 3} };
    Matrix<3, float> expected(expected_elements);

    ASSERT_EQ(expected, first * second);
  }

  TEST(Matrix, ShouldReturnSameMatrixWhenMultipliedWithIdentity) {
    float elements[3][3] = { {1, 2, 1}, {2, 0, 1}, {1, 1, 1} };
    Matrix<3, float> first(elements);
    Matrix<3, float> second;

    ASSERT_EQ(first, first * second);
  }

  TEST(Matrix, ShouldDetermineIfTwoIdentityMatricesAreEqual) {
    Matrix<3, float> first, second;
    ASSERT_TRUE(first == second);
  }

  TEST(Matrix, ShouldDetermineIfTwoMatricesAreEqual) {
    float elements[3][3] = { {1, 2, 1}, {2, 0, 1}, {1, 1, 1} };
    Matrix<3, float> first(elements), second(elements);
    ASSERT_TRUE(first == second);
  }

  TEST(Matrix, ShouldDetermineIfTwoMatricesAreNotEqual) {
    float elements[3][3] = { {1, 2, 1}, {2, 0, 1}, {1, 1, 1} };
    Matrix<3, float> first(elements), second;
    ASSERT_TRUE(first != second);
  }

  TEST(Matrix, ShouldCalculateRowSum) {
    float elements[3][3] = { {1, 2, 1}, {2, 0, 1}, {1, 1, 1} };
    Matrix<3, float> matrix(elements);
    ASSERT_EQ(4, matrix.rowSum(0));
  }

  TEST(Matrix, ShouldCalculateColSum) {
    float elements[3][3] = { {1, 2, 1}, {2, 0, 1}, {1, 1, 1} };
    Matrix<3, float> matrix(elements);
    ASSERT_EQ(3, matrix.rowSum(1));
  }

  TEST(Matrix, ShouldReturnOriginalVectorWhenVectorIsMultipliedWithIdentityMatrix) {
    float elements[3] = { 1, 2, 3 };
    Vector<3, float> vector(elements);
    Matrix<3, float> matrix;

    Vector<3, float> result = matrix * vector;

    ASSERT_EQ(1, result[0]);
    ASSERT_EQ(2, result[1]);
    ASSERT_EQ(3, result[2]);
  }
  
  TEST(Matrix, ShouldReturnXDirectionVectorWhenMultipliedWithXUnitVector) {
    float elements[3][3] = { {1, 2, 1}, {2, 0, 1}, {1, 1, 1} };
    Matrix<3, float> matrix(elements);
    
    float vector_elements[3] = {1, 0, 0};
    Vector<3, float> vector(vector_elements);
    
    float expected_elements[3] = {1, 2, 1};
    Vector<3, float> expected(expected_elements);
    ASSERT_EQ(expected, matrix * vector);
  }

  TEST(Matrix, ShouldReturnYDirectionVectorWhenMultipliedWithYUnitVector) {
    float elements[3][3] = { {1, 2, 1}, {2, 0, 1}, {1, 1, 1} };
    Matrix<3, float> matrix(elements);
    
    float vector_elements[3] = {0, 1, 0};
    Vector<3, float> vector(vector_elements);
    
    float expected_elements[3] = {2, 0, 1};
    Vector<3, float> expected(expected_elements);
    ASSERT_EQ(expected, matrix * vector);
  }

  TEST(Matrix, ShouldReturnZDirectionVectorWhenMultipliedWithZUnitVector) {
    float elements[3][3] = { {1, 2, 1}, {2, 0, 1}, {1, 1, 1} };
    Matrix<3, float> matrix(elements);
    
    float vector_elements[3] = {0, 0, 1};
    Vector<3, float> vector(vector_elements);
    
    float expected_elements[3] = {1, 1, 1};
    Vector<3, float> expected(expected_elements);
    ASSERT_EQ(expected, matrix * vector);
  }

  TEST(Matrix, ShouldReturnTransformedVectorWhenMultipliedWithMatrix) {
    float matrix_elements[3][3] = { {1, 0, 0}, {0, 0, 1}, {0, 1, 0} };
    Matrix<3, float> matrix(matrix_elements);

    float vector_elements[3] = { 1, 2, 3 };
    Vector<3, float> vector(vector_elements);

    float expected_elements[3] = { 1, 3, 2 };
    Vector<3, float> expected(expected_elements);

    ASSERT_EQ(expected, matrix * vector);
  }
  
  TEST(Matrix, ShouldMultiplyIdentityWithScalar) {
    Matrix<3, float> matrix;

    float expected_elements[3][3] = { {3, 0, 0}, {0, 3, 0}, {0, 0, 3} };
    Matrix<3, float> expected(expected_elements);

    ASSERT_EQ(expected, matrix * 3);
  }

  TEST(Matrix, ShouldMultiplyWithScalar) {
    float elements[3][3] = { {1, 2, 1}, {2, 0, 1}, {1, 1, 1} };
    Matrix<3, float> matrix(elements);
  
    float expected_elements[3][3] = { {3, 6, 3}, {6, 0, 3}, {3, 3, 3} };
    Matrix<3, float> expected(expected_elements);
  
    ASSERT_EQ(expected, matrix * 3);
  }

  TEST(Matrix, ShouldDivideIdentityByScalar) {
    Matrix<3, float> matrix;

    float expected_elements[3][3] = { {0.5, 0, 0}, {0, 0.5, 0}, {0, 0, 0.5} };
    Matrix<3, float> expected(expected_elements);

    ASSERT_EQ(expected, matrix / 2);
  }

  TEST(Matrix, ShouldDivideByScalar) {
    float elements[3][3] = { {1, 2, 1}, {2, 0, 1}, {1, 1, 1} };
    Matrix<3, float> matrix(elements);
  
    float expected_elements[3][3] = { {0.5, 1, 0.5}, {1, 0, 0.5}, {0.5, 0.5, 0.5} };
    Matrix<3, float> expected(expected_elements);
  
    ASSERT_EQ(expected, matrix / 2);
  }

  TEST(Matrix, ShouldNotAllowDivisionByZero) {
    Matrix<3, float> matrix;
    ASSERT_THROW(matrix / 0, DivisionByZeroException);
  }
  
  TEST(Matrix, ShouldReturnTransposedMatrix) {
    float elements[3][3] = { {1, 2, 1},
                             {3, 0, 2},
                             {4, 1, 1} };
    Matrix<3, float> matrix(elements);
    
    float expected_elements[3][3] = { {1, 3, 4},
                                      {2, 0, 1},
                                      {1, 2, 1} };
    Matrix<3, float> expected(expected_elements);
    ASSERT_EQ(expected, matrix.transposed());
  }
  
  TEST(Matrix, ShouldReturnIdentityWhenIdentityIsTransposed) {
    Matrix<3, float> matrix;
    ASSERT_EQ(matrix, matrix.transposed());
  }
  
  TEST(Matrix, ShouldReturnOriginalMatrixIfTransposedTwice) {
    float elements[3][3] = { {1, 2, 1},
                             {3, 0, 2},
                             {4, 1, 1} };
    Matrix<3, float> matrix(elements);
    ASSERT_EQ(matrix, matrix.transposed().transposed());
  }
}

namespace SpecializedMatrixTest {
  using namespace ::testing;

  template<class T>
  class SpecializedMatrixTest : public ::testing::Test {
  };

  typedef ::testing::Types<Matrix2<float>, Matrix3<float>, Matrix4<float>,
                           Matrix2<double>, Matrix3<double>, Matrix4<double> > SpecializedMatrixTypes;
  TYPED_TEST_CASE(SpecializedMatrixTest, SpecializedMatrixTypes);
  
  TYPED_TEST(SpecializedMatrixTest, ShouldReturnCorrectTypeForMultiplicationWithMatrix) {
    TypeParam matrix;
    ASSERT_TYPES_EQ(matrix, matrix * matrix);
  }
  
  TYPED_TEST(SpecializedMatrixTest, ShouldReturnCorrectTypeForMultiplicationWithVector) {
    TypeParam matrix;
    typename TypeParam::Vector vector;
    ASSERT_TYPES_EQ(vector, matrix * vector);
  }
  
  TYPED_TEST(SpecializedMatrixTest, ShouldReturnCorrectTypeForMultiplicationWithScalar) {
    TypeParam matrix;
    ASSERT_TYPES_EQ(matrix, matrix * 2);
  }
  
  TYPED_TEST(SpecializedMatrixTest, ShouldReturnCorrectTypeForDivisionByScalar) {
    TypeParam matrix;
    ASSERT_TYPES_EQ(matrix, matrix / 2);
  }
  
  TYPED_TEST(SpecializedMatrixTest, ShouldReturnCorrectTypeForTransposedMatrix) {
    TypeParam matrix;
    ASSERT_TYPES_EQ(matrix, matrix.transposed());
  }
}

namespace Matrix2Test {
  using namespace ::testing;

  TEST(Matrix2, ShouldInitializeIdentity) {
    Matrix2<float> matrix;
    ASSERT_EQ(1, matrix[0][0]);
  }

  TEST(Matrix2, ShouldInitializeWithCells) {
    Matrix2<float> matrix(1, 1, 0, 1);
    ASSERT_EQ(1, matrix[0][0]);
    ASSERT_EQ(1, matrix[0][1]);
    ASSERT_EQ(0, matrix[1][0]);
    ASSERT_EQ(1, matrix[1][1]);
  }
  
  TEST(Matrix2, ShouldCopyFromMatrix3) {
    Matrix3<float> matrix(2, 1, 2, 3, 3, 2, 4, 1, 2);
    Matrix2<float> copy(matrix), expected(2, 1, 3, 3);
    
    ASSERT_EQ(expected, copy);
  }
  
  TEST(Matrix2, ShouldHaveNonZeroDeterminantForIdentityMatrix) {
    Matrix2<float> matrix;
    ASSERT_TRUE(matrix.determinant() != 0);
  }
  
  TEST(Matrix2, ShouldHaveZeroDeterminantForNonInvertibleMatrix) {
    Matrix2<float> matrix(0, 0, 1, 0);
    ASSERT_EQ(0, matrix.determinant());
  }
  
  TEST(Matrix2, ShouldReturnIdentityWhenIdentityIsInverted) {
    Matrix2<float> matrix;
    ASSERT_EQ(matrix, matrix.inverted());
  }
  
  TEST(Matrix2, ShouldReturnInvertedMatrix) {
    Matrix2<float> matrix(0, -1, 1, 0);
    Matrix2<float> expected(0, 1, -1, 0);
    
    ASSERT_EQ(expected, matrix.inverted());
  }
  
  TEST(Matrix2, ShouldGenerate90DegreeRotationMatrix) {
    Matrix2<float> matrix = Matrix2<float>::rotate(3.1415926535897 / 2.0),
                   expected = Matrix2<float>(0, -1, 1, 0);
    ASSERT_MATRIX_NEAR(expected, matrix, 0.0001);
  }
  
  TEST(Matrix2, ShouldRotateCounterclockwiseByDefault) {
    Matrix2<float> expected = Matrix2<float>::rotate(0.354),
                   actual = Matrix2<float>::counterclockwise(0.354);
    ASSERT_EQ(expected, actual);
  }
  
  TEST(Matrix2, ShouldRotateClockwiseByUsingNegativeAngle) {
    Matrix2<float> expected = Matrix2<float>::rotate(-0.354),
                   actual = Matrix2<float>::clockwise(0.354);
    ASSERT_EQ(expected, actual);
  }
  
  TEST(Matrix2, ShouldRotateClockwise) {
    Matrix2<float> matrix = Matrix2<float>::clockwise(3.1415926535897 / 2.0),
                   expected = Matrix2<float>(0, 1, -1, 0);
    ASSERT_MATRIX_NEAR(expected, matrix, 0.0001);
  }
  
  TEST(Matrix2, ShouldRotateCounterClockwise) {
    Matrix2<float> matrix = Matrix2<float>::counterclockwise(3.1415926535897 / 2.0),
                   expected(0, -1, 1, 0);
    ASSERT_MATRIX_NEAR(expected, matrix, 0.0001);
  }

  TEST(Matrix2, ShouldGenerateScalingMatrix) {
    Matrix2<float> matrix = Matrix2<float>::scale(2),
                   expected(2, 0, 0, 2);
    ASSERT_EQ(expected, matrix);
  }
  
  TEST(Matrix2, ShouldScaleVector) {
    Matrix2<float> matrix = Matrix2<float>::scale(2);
    Vector2<float> vector(1, 1), expected(2, 2);
    ASSERT_EQ(expected, matrix * vector);
  }
  
  TEST(Matrix2, ShouldGenerateScalingMatrixWithTwoIndependentFactors) {
    Matrix2<float> matrix = Matrix2<float>::scale(2, 3),
                   expected(2, 0, 0, 3);
    ASSERT_EQ(expected, matrix);
  }
  
  TEST(Matrix2, ShouldScaleVectorWithTwoIndependentFactors) {
    Matrix2<float> matrix = Matrix2<float>::scale(2, 3);
    Vector2<float> vector(1, 1), expected(2, 3);
    ASSERT_EQ(expected, matrix * vector);
  }
  
  TEST(Matrix2, ShouldGenerateShearingMatrix) {
    Matrix2<float> matrix = Matrix2<float>::shear(1, 2),
                   expected(1, 1, 2, 1);
    ASSERT_EQ(expected, matrix);
  }
  
  TEST(Matrix2, ShouldShearVector) {
    Matrix2<float> matrix = Matrix2<float>::shear(1, 2);
    Vector2<float> vector(1, 1), expected(2, 3);
    ASSERT_EQ(expected, matrix * vector);
  }
  
  TEST(Matrix2, ShouldGenerateReflectionMatrix) {
    Matrix2<float> matrix = Matrix2<float>::reflect(1, 1),
                   expected(0, 1, 1, 0);
    ASSERT_MATRIX_NEAR(expected, matrix, 0.0001);
  }

  TEST(Matrix2, ShouldGenerateReflectionMatrixFromVector) {
    Matrix2<float> matrix = Matrix2<float>::reflect(Vector2<float>(1, 1)),
                   expected(0, 1, 1, 0);
    ASSERT_MATRIX_NEAR(expected, matrix, 0.0001);
  }
  
  TEST(Matrix2, ShouldReflectVector) {
    Matrix2<float> matrix = Matrix2<float>::reflect(1, 1);
    Vector2<float> vector(-1, 1), expected(1, -1);
    
    ASSERT_VECTOR_NEAR(expected, matrix * vector, 0.0001);
  }

  TEST(Matrix2, ShouldGenerateProjectionMatrix) {
    Matrix2<float> matrix = Matrix2<float>::project(1, 1),
                   expected(0.5, 0.5, 0.5, 0.5);
    ASSERT_MATRIX_NEAR(expected, matrix, 0.0001);
  }

  TEST(Matrix2, ShouldGenerateProjectionMatrixFromVector) {
    Matrix2<float> matrix = Matrix2<float>::project(Vector2<float>(1, 1)),
                   expected(0.5, 0.5, 0.5, 0.5);
    ASSERT_MATRIX_NEAR(expected, matrix, 0.0001);
  }
  
  TEST(Matrix2, ShouldProjectVector) {
    Matrix2<float> matrix = Matrix2<float>::project(1, 1);
    Vector2<float> vector(-1, 1), expected(0, 0);
    
    ASSERT_EQ(expected, matrix * vector);
  }
  
  TEST(Matrix2, ShouldHaveIdentityConstant) {
    Matrix2<float> expected(1, 0, 0, 1);
    ASSERT_EQ(expected, Matrix2<float>::identity());
  }
  
  TEST(Matrix2, ShouldHave90DegreeRotationConstant) {
    Matrix2<float> expected(0, -1, 1, 0);
    ASSERT_EQ(expected, Matrix2<float>::rotate90());
  }
  
  TEST(Matrix2, ShouldHave180DegreeRotationConstant) {
    Matrix2<float> expected(-1, 0, 0, -1);
    ASSERT_EQ(expected, Matrix2<float>::rotate180());
  }
  
  TEST(Matrix2, ShouldHave270DegreeRotationConstant) {
    Matrix2<float> expected(0, 1, -1, 0);
    ASSERT_EQ(expected, Matrix2<float>::rotate270());
  }
  
  TEST(Matrix2, ShouldHaveXReflectionConstant) {
    Matrix2<float> expected(-1, 0, 0, 1);
    ASSERT_EQ(expected, Matrix2<float>::reflectX());
  }
  
  TEST(Matrix2, ShouldHaveYReflectionConstant) {
    Matrix2<float> expected(1, 0, 0, -1);
    ASSERT_EQ(expected, Matrix2<float>::reflectY());
  }
  
  TEST(Matrix2, ShouldHaveXUnitVectorConstant) {
    Vector2<float> expected(1, 0);
    ASSERT_EQ(expected, Matrix2<float>::xUnit());
  }
  
  TEST(Matrix2, ShouldHaveYUnitVectorConstant) {
    Vector2<float> expected(0, 1);
    ASSERT_EQ(expected, Matrix2<float>::yUnit());
  }
}

namespace Matrix3Test {
  using namespace ::testing;

  TEST(Matrix3, ShouldInitializeIdentity) {
    Matrix3<float> matrix;
    ASSERT_EQ(1, matrix[0][0]);
  }

  TEST(Matrix3, ShouldInitializeWithCells) {
    Matrix3<float> matrix(1, 1, 2, 0, 1, 0, 2, 1, 1);
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
  
  TEST(Matrix3, ShouldCopyFromMatrix2) {
    Matrix2<float> matrix(2, 1, 1, 2);
    Matrix3<float> copy(matrix), expected(2, 1, 0, 1, 2, 0, 0, 0, 1);
    
    ASSERT_EQ(expected, copy);
  }
  
  TEST(Matrix3, ShouldCopyFromMatrix4) {
    Matrix4<float> matrix(2, 1, 3, 0, 1, 2, 1, 0, 4, 3, 3, 0, 0, 0, 0, 1);
    Matrix3<float> copy(matrix), expected(2, 1, 3, 1, 2, 1, 4, 3, 3);
    
    ASSERT_EQ(expected, copy);
  }
  
  TEST(Matrix3, ShouldRotateAroundXAxis) {
    Matrix3<float> matrix = Matrix3<float>::rotateX(3.1415926535897 / 2.0);
    Matrix3<float> expected(1, 0, 0, 0, 0, -1, 0, 1, 0);
    ASSERT_MATRIX_NEAR(expected, matrix, 0.0001);
  }
  
  TEST(Matrix3, ShouldRotateAroundYAxis) {
    Matrix3<float> matrix = Matrix3<float>::rotateY(3.1415926535897 / 2.0);
    Matrix3<float> expected(0, 0, 1, 0, 1, 0, -1, 0, 0);
    ASSERT_MATRIX_NEAR(expected, matrix, 0.0001);
  }
  
  TEST(Matrix3, ShouldRotateAroundZAxis) {
    Matrix3<float> matrix = Matrix3<float>::rotateZ(3.1415926535897 / 2.0);
    Matrix3<float> expected(0, -1, 0, 1, 0, 0, 0, 0, 1);
    ASSERT_MATRIX_NEAR(expected, matrix, 0.0001);
  }
  
  TEST(Matrix3, ShouldScaleUniformly) {
    Matrix3<float> matrix = Matrix3<float>::scale(2.0);
    Matrix3<float> expected(2, 0, 0, 0, 2, 0, 0, 0, 2);
    ASSERT_EQ(expected, matrix);
  }
  
  TEST(Matrix3, ShouldScaleDifferentlyForEachAxis) {
    Matrix3<float> matrix = Matrix3<float>::scale(2, 3, 4);
    Matrix3<float> expected(2, 0, 0, 0, 3, 0, 0, 0, 4);
    ASSERT_EQ(expected, matrix);
  }
}

namespace Matrix4Test {
  using namespace ::testing;

  TEST(Matrix4, ShouldInitializeIdentity) {
    Matrix4<float> matrix;
    ASSERT_EQ(1, matrix[0][0]);
  }

  TEST(Matrix4, ShouldInitializeWithCells) {
    Matrix4<float> matrix(1, 1, 2, 0, 0, 1, 0, 0, 2, 1, 1, 0, 0, 0, 0, 1);
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
  
  TEST(Matrix4, ShouldInitializeFromThree3DVectors) {
    Matrix4<float> matrix(Vector3f(1, 1, 2), Vector3f(0, 1, 0), Vector3f(2, 1, 1));
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
  
  TEST(Matrix4, ShouldCopyFromMatrix3) {
    Matrix3<float> matrix(2, 1, 3, 1, 2, 1, 4, 3, 3);
    Matrix4<float> copy(matrix), expected(2, 1, 3, 0, 1, 2, 1, 0, 4, 3, 3, 0, 0, 0, 0, 1);
    
    ASSERT_EQ(expected, copy);
  }

  TEST(Matrix4, ShouldReturnVector4WhenMultipliedWithVector) {
    Matrix4<float> matrix;
    Vector4<float> vector(1, 2, 3);
    Vector4<float> result = matrix * vector;
    ASSERT_EQ(vector, result);
  }
  
  TEST(Matrix4, ShouldHaveNonZeroDeterminantForIdentityMatrix) {
    Matrix4<float> matrix;
    ASSERT_TRUE(matrix.determinant() != 0);
  }
  
  TEST(Matrix4, ShouldHaveZeroDeterminantForNonInvertibleMatrix) {
    Matrix4<float> matrix(0, 0, 0, 0,
                          0, 1, 0, 0,
                          0, 0, 1, 0,
                          0, 0, 0, 1);
    ASSERT_EQ(0, matrix.determinant());
  }
  
  TEST(Matrix4, ShouldReturnIdentityWhenIdentityIsInverted) {
    Matrix4<float> matrix;
    ASSERT_EQ(matrix, matrix.inverted());
  }
  
  TEST(Matrix4, ShouldReturnOriginalMatrixWhenInvertedTwice) {
    Matrix4<float> matrix(1, 0, 0, 0,
                          0, 1, 0, 0,
                          0, 1, 1, 0,
                          0, 0, 0, 1);
    ASSERT_EQ(matrix, matrix.inverted().inverted());
  }
  
  TEST(Matrix4, ShouldReturnInvertedMatrix) {
    Matrix4<float> matrix(1, 0, 0, 0,
                          0, 1, 0, 0,
                          0, 1, 1, 0,
                          0, 0, 0, 1);
    Matrix4<float> expected(1, 0, 0, 0,
                            0, 1, 0, 0,
                            0, -1, 1, 0,
                            0, 0, 0, 1);
    
    ASSERT_EQ(expected, matrix.inverted());
  }
  
  TEST(Matrix4, ShouldReturnTranslationMatrix) {
    Matrix4<float> expected(1, 0, 0, 1,
                            0, 1, 0, 2,
                            0, 0, 1, 3,
                            0, 0, 0, 1);
    ASSERT_EQ(expected, Matrix4<float>::translate(Vector3f(1, 2, 3)));
  }
}
