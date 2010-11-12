#include "gtest.h"
#include "Matrix.h"
#include "Vector.h"

namespace MatrixTest {
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
    Matrix<3, float> first;
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

  TEST(Matrix, ShouldReturnTransformedVectorWhenMultipliedWithMatrix) {
    float matrix_elements[3][3] = { {1, 0, 0}, {0, 0, 1}, {0, 1, 0} };
    Matrix<3, float> matrix(matrix_elements);

    float vector_elements[3] = { 1, 2, 3 };
    Vector<3, float> vector(vector_elements);

    float expected_elements[3] = { 1, 3, 2 };
    Vector<3, float> expected(expected_elements);

    ASSERT_EQ(expected, matrix * vector);
  }
}

namespace Matrix2Test {
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
}

namespace Matrix3Test {
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
}

namespace Matrix4Test {
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

  TEST(Matrix4, ShouldReturnVector4WhenMultipliedWithVector) {
    Matrix4<float> matrix;
    Vector4<float> vector(1, 2, 3);
    Vector4<float> result = matrix * vector;
    ASSERT_EQ(vector, result);
  }
}
