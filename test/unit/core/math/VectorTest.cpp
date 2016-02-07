#include "gtest.h"
#include "core/math/Vector.h"
#include "test/helpers/TypeTestHelper.h"

#include <limits>

using namespace std;

namespace VectorTest {
  TEST(Vector, ShouldInitializeCoordinatesWithZeros) {
    Vector<3, float> vector;
    ASSERT_EQ(0, vector[0]);
    ASSERT_EQ(0, vector[1]);
    ASSERT_EQ(0, vector[2]);
  }

  TEST(Vector, ShouldGetAndSetCoordinate) {
    Vector<3, float> vector;
    vector.setCoordinate(0, 10.0);
    ASSERT_EQ(10.0, vector.coordinate(0));
  }

  TEST(Vector, ShouldGetAndSetCoordinateWithIndexOperator) {
    Vector<3, float> vector;
    vector[1] = 10.0;
    ASSERT_EQ(10.0, vector[1]);
  }

  TEST(Vector, ShouldCopyVector) {
    Vector<3, float> vector;
    vector[0] = 10.0;

    Vector<3, float> copy = vector;
    ASSERT_EQ(10.0, copy[0]);
  }

  TEST(Vector, ShouldCopyFromSmallerVector) {
    Vector<3, float> source;
    source[0] = 10.0;

    Vector<4, float> dest = source;
    ASSERT_EQ(10.0, dest[0]);
  }

  TEST(Vector, ShouldCopyFromLargerVector) {
    Vector<3, float> source;
    source[0] = 10.0;

    Vector<2, float> dest = source;
    ASSERT_EQ(10.0, dest[0]);
  }

  TEST(Vector, ShouldInitializeVectorFromCArray) {
    float elements[3] = { 1, 2, 3 };
    Vector<3, float> vector(elements);
    ASSERT_EQ(1, vector[0]);
    ASSERT_EQ(2, vector[1]);
    ASSERT_EQ(3, vector[2]);
  }

  TEST(Vector, ShouldAdd) {
    Vector<3, float> vector1, vector2;
    vector1[0] = 10.0;
    vector1[1] = 20.0;
    vector2[1] = 10.0;
    vector2[2] = 20.0;

    Vector<3, float> vector3 = vector1 + vector2;
    ASSERT_EQ(10.0, vector3[0]);
    ASSERT_EQ(30.0, vector3[1]);
    ASSERT_EQ(20.0, vector3[2]);
  }
  
  TEST(Vector, ShouldAddInPlace) {
    Vector<3, float> vector1, vector2;
    vector1[0] = 10.0;
    vector1[1] = 20.0;
    vector2[1] = 10.0;
    vector2[2] = 20.0;

    vector1 += vector2;
    ASSERT_EQ(10.0, vector1[0]);
    ASSERT_EQ(30.0, vector1[1]);
    ASSERT_EQ(20.0, vector1[2]);
  }

  TEST(Vector, ShouldSubtract) {
    Vector<3, float> vector1, vector2;
    vector1[0] = 10.0;
    vector1[1] = 20.0;
    vector2[1] = 10.0;
    vector2[2] = 20.0;

    Vector<3, float> vector3 = vector1 - vector2;
    ASSERT_EQ( 10.0, vector3[0]);
    ASSERT_EQ( 10.0, vector3[1]);
    ASSERT_EQ(-20.0, vector3[2]);
  }

  TEST(Vector, ShouldSubtractInPlace) {
    Vector<3, float> vector1, vector2;
    vector1[0] = 10.0;
    vector1[1] = 20.0;
    vector2[1] = 10.0;
    vector2[2] = 20.0;

    vector1 -= vector2;
    ASSERT_EQ( 10.0, vector1[0]);
    ASSERT_EQ( 10.0, vector1[1]);
    ASSERT_EQ(-20.0, vector1[2]);
  }

  TEST(Vector, ShouldNegate) {
    Vector<3, float> vector;
    vector[0] = 10.0;

    Vector<3, float> negated = -vector;
    ASSERT_EQ(-10.0, negated[0]);
  }

  TEST(Vector, ShouldCalculateLength) {
    Vector<3, float> vector;
    vector[0] = 10.0;

    ASSERT_EQ(10.0, vector.length());
  }
  
  TEST(Vector, ShouldCalculateSquaredLength) {
    Vector<3, float> vector;
    vector[0] = 2.0;

    ASSERT_EQ(4.0, vector.squaredLength());
  }
  
  TEST(Vector, ShouldCalculateDistanceToOtherVector) {
    Vector<3, float> vector1, vector2;
    vector1[0] = 10.0;
    vector2[0] = 5.0;
    
    ASSERT_EQ(5.0, vector1.distanceTo(vector2));
  }
  
  TEST(Vector, ShouldReturnSameValueForReversedDistance) {
    Vector<3, float> vector1, vector2, foo;
    vector1[0] = 10.0;
    vector2[0] = 5.0;
    
    ASSERT_EQ(vector1.distanceTo(vector2), vector2.distanceTo(vector1));
  }

  TEST(Vector, ShouldCalculateSquaredDistanceToOtherVector) {
    Vector<3, float> vector1, vector2;
    vector1[0] = 10.0;
    vector2[0] = 5.0;
    
    ASSERT_EQ(25.0, vector1.squaredDistanceTo(vector2));
  }
  
  TEST(Vector, ShouldReturnSameValueForReversedSquaredDistance) {
    Vector<3, float> vector1, vector2, foo;
    vector1[0] = 10.0;
    vector2[0] = 5.0;
    
    ASSERT_EQ(vector1.squaredDistanceTo(vector2), vector2.squaredDistanceTo(vector1));
  }

  TEST(Vector, ShouldDivideVectorByScalar) {
    Vector<3, float> vector;
    vector[0] = 10.0;

    Vector<3, float> shrunk = vector / 2;

    ASSERT_EQ(5.0, shrunk[0]);
  }

  TEST(Vector, ShouldDivideVectorByScalarInPlace) {
    Vector<3, float> vector;
    vector[0] = 10.0;

    vector /= 2;

    ASSERT_EQ(5.0, vector[0]);
  }

  TEST(Vector, ShouldNotAllowDivisionByZero) {
    Vector<3, float> vector;
    ASSERT_THROW(vector / 0, DivisionByZeroException);
  }
  
  TEST(Vector, ShouldNotAllowInPlaceDivisionByZero) {
    Vector<3, float> vector;
    ASSERT_THROW(vector /= 0, DivisionByZeroException);
  }
  
  TEST(Vector, ShouldCalculateDotProductOfNullVectors) {
    Vector<3, float> first, second;
    ASSERT_EQ(0, first * second);
  }
  
  TEST(Vector, ShouldCalculateDotProductOfUnitVectors) {
    float elements[3] = { 1, 0, 0 };
    Vector<3, float> vector(elements);
    
    ASSERT_EQ(1, vector * vector);
  }
  
  TEST(Vector, ShouldCalculateDotProduct) {
    float first_elements[3] = { 1, 2, 2 };
    Vector<3, float> first(first_elements);

    float second_elements[3] = { 3, 4, 2 };
    Vector<3, float> second(second_elements);
    
    ASSERT_EQ(15, first * second);
  }
  
  TEST(Vector, ShouldFollowDotProductRules) {
    float first_elements[3] = { 1, 2, 2 };
    Vector<3, float> first(first_elements);

    float second_elements[3] = { 3, 4, 2 };
    Vector<3, float> second(second_elements);
    
    ASSERT_EQ(-(first * second), (-first * second));
    ASSERT_EQ(-(first * second), (first * -second));
    ASSERT_EQ(first * second, second * first);
  }

  TEST(Vector, ShouldMultiplyVectorByScalar) {
    Vector<3, float> vector;
    vector[0] = 10.0;

    Vector<3, float> enlarged = vector * 2;

    ASSERT_EQ(20.0, enlarged[0]);
  }

  TEST(Vector, ShouldMultiplyVectorByScalarInPlace) {
    Vector<3, float> vector;
    vector[0] = 10.0;

    vector *= 2;

    ASSERT_EQ(20.0, vector[0]);
  }

  TEST(Vector, ShouldGetNormalizedVector) {
    Vector<3, float> vector;
    vector[0] = 10;

    Vector<3, float> normalized = vector.normalized();
    ASSERT_EQ(1, normalized[0]);
    ASSERT_EQ(0, normalized[1]);
    ASSERT_EQ(0, normalized[2]);
  }
  
  TEST(Vector, ShouldNormalizeInPlace) {
    Vector<3, float> vector;
    vector[0] = 10;

    vector.normalize();
    ASSERT_EQ(1, vector[0]);
    ASSERT_EQ(0, vector[1]);
    ASSERT_EQ(0, vector[2]);
  }

  TEST(Vector, ShouldFindOutIfItIsNormalized) {
    Vector<3, float> normal, vector;
    normal[0] = 1;
    vector[0] = 10;

    ASSERT_TRUE(normal.isNormalized());
    ASSERT_FALSE(vector.isNormalized());
  }

  TEST(Vector, ShouldReturnTrueWhenComparingTwoDefaultVectors) {
    Vector<3, float> first, second;
    ASSERT_TRUE(first == second);
  }

  TEST(Vector, ShouldCompareSameVectorForEquality) {
    Vector<3, float> vector;
    ASSERT_TRUE(vector == vector);
  }

  TEST(Vector, ShouldCompareVectorsForEquality) {
    float elements[3] = { 1, 2, 3 };
    Vector<3, float> first(elements), second(elements);
    ASSERT_TRUE(first == second);
  }

  TEST(Vector, ShouldCompareVectorsForInequality) {
    float first_elements[3] = { 1, 2, 3 };
    Vector<3, float> first(first_elements);

    float second_elements[3] = { 4, 5, 6 };
    Vector<3, float> second(second_elements);

    ASSERT_TRUE(first != second);
  }
  
  TEST(Vector, ShouldReturnTrueForUndefinedIfVectorIsUndefined) {
    float elements[3] = { numeric_limits<float>::quiet_NaN(), 0, 0 };
    Vector<3, float> vector(elements);
    ASSERT_TRUE(vector.isUndefined());
  }
  
  TEST(Vector, ShouldReturnFalseForUndefinedIfVectorIsDefined) {
    float elements[3] = { 0, 0, 0 };
    Vector<3, float> vector(elements);
    ASSERT_FALSE(vector.isUndefined());
  }

  TEST(Vector, ShouldReturnTrueForDefinedIfVectorIsDefined) {
    float elements[3] = { 0, 0, 0 };
    Vector<3, float> vector(elements);
    ASSERT_TRUE(vector.isDefined());
  }
  
  TEST(Vector, ShouldReturnFalseForDefinedIfVectorIsUndefined) {
    float elements[3] = { numeric_limits<float>::quiet_NaN(), 0, 0 };
    Vector<3, float> vector(elements);
    ASSERT_FALSE(vector.isDefined());
  }
}

namespace SpecializedVectorTest {
  template<class T>
  class SpecializedVectorTest : public ::testing::Test {
  };

  typedef ::testing::Types<Vector2<float>, Vector3<float>, Vector4<float>,
                           Vector2<double>, Vector3<double>, Vector4<double>> SpecializedVectorTypes;
  TYPED_TEST_CASE(SpecializedVectorTest, SpecializedVectorTypes);

  TYPED_TEST(SpecializedVectorTest, ShouldReturnCorrectTypeForDotProduct) {
    TypeParam vector;
    ASSERT_TYPES_EQ(typename TypeParam::Coordinate(0), vector * vector);
  }

  TYPED_TEST(SpecializedVectorTest, ShouldReturnCorrectTypeForMultiplicationWithScalar) {
    TypeParam vector;
    ASSERT_TYPES_EQ(vector, vector * 2);
  }

  TYPED_TEST(SpecializedVectorTest, ShouldReturnCorrectTypeForDivisionByScalar) {
    TypeParam vector;
    ASSERT_TYPES_EQ(vector, vector / 2);
  }

  TYPED_TEST(SpecializedVectorTest, ShouldReturnCorrectTypeForInPlaceAddition) {
    TypeParam vector;
    ASSERT_TYPES_EQ(vector, vector += vector);
  }

  TYPED_TEST(SpecializedVectorTest, ShouldReturnCorrectTypeForInPlaceSubtraction) {
    TypeParam vector;
    ASSERT_TYPES_EQ(vector, vector -= vector);
  }

  TYPED_TEST(SpecializedVectorTest, ShouldReturnCorrectTypeForInPlaceMultiplicationWithScalar) {
    TypeParam vector;
    ASSERT_TYPES_EQ(vector, vector *= 2);
  }

  TYPED_TEST(SpecializedVectorTest, ShouldReturnCorrectTypeForInPlaceDivisionByScalar) {
    TypeParam vector;
    ASSERT_TYPES_EQ(vector, vector /= 2);
  }

  TYPED_TEST(SpecializedVectorTest, ShouldReturnCorrectTypeForAddition) {
    TypeParam vector;
    ASSERT_TYPES_EQ(vector, vector + vector);
  }

  TYPED_TEST(SpecializedVectorTest, ShouldReturnCorrectTypeForSubtraction) {
    TypeParam vector;
    ASSERT_TYPES_EQ(vector, vector - vector);
  }

  TYPED_TEST(SpecializedVectorTest, ShouldReturnCorrectTypeForNegation) {
    TypeParam vector;
    ASSERT_TYPES_EQ(vector, - vector);
  }

  TYPED_TEST(SpecializedVectorTest, ShouldReturnCorrectTypeForNormalization) {
    TypeParam vector;
    vector[0] = 1;
    ASSERT_TYPES_EQ(vector, vector.normalized());
  }
  
  TYPED_TEST(SpecializedVectorTest, ShouldProvideUndefinedVector) {
    TypeParam vector = TypeParam::undefined();
    ASSERT_TRUE(vector.isUndefined());
  }
}

namespace Vector2Test {
  TEST(Vector2, ShouldInitializeNullVector) {
    Vector2<float> vector;
    ASSERT_EQ(0, vector.x());
    ASSERT_EQ(0, vector.y());
  }

  TEST(Vector2, ShouldInitializeCoordinates) {
    Vector2<float> vector(1, 2);
    ASSERT_EQ(1, vector.x());
    ASSERT_EQ(2, vector.y());
  }

  TEST(Vector2, ShouldCopyFromVector3) {
    Vector3<float> source(1, 2, 3);
    Vector2<float> dest = source;
    ASSERT_EQ(1, dest.x());
    ASSERT_EQ(2, dest.y());
  }
  
  TEST(Vector2, ShouldDefineNullVector) {
    Vector2<float> expected(0, 0);
    ASSERT_EQ(expected, Vector2<float>::null());
  }
}

namespace Vector3Test {
  template<class T>
  class Vector3Test : public ::testing::Test {
  };

  typedef ::testing::Types<Vector3<float>, Vector3<double>> Vector3Types;
  TYPED_TEST_CASE(Vector3Test, Vector3Types);
  
  TYPED_TEST(Vector3Test, ShouldInitializeNullVector) {
    TypeParam vector;
    ASSERT_EQ(0, vector.x());
    ASSERT_EQ(0, vector.y());
    ASSERT_EQ(0, vector.z());
  }

  TYPED_TEST(Vector3Test, ShouldInitializeCoordinates) {
    TypeParam vector(1, 2, 3);
    ASSERT_EQ(1, vector.x());
    ASSERT_EQ(2, vector.y());
    ASSERT_EQ(3, vector.z());
  }

  TYPED_TEST(Vector3Test, ShouldInitializeZCoordinateAsZeroByDefault) {
    TypeParam vector(1, 2);
    ASSERT_EQ(0, vector.z());
  }

  TYPED_TEST(Vector3Test, ShouldCopyFromVector2) {
    Vector2<typename TypeParam::Coordinate> source(1, 2);
    TypeParam dest = source;
    ASSERT_EQ(1, dest.x());
    ASSERT_EQ(2, dest.y());
    ASSERT_EQ(0, dest.z());
  }

  TYPED_TEST(Vector3Test, ShouldCopyFromVector4) {
    Vector4<typename TypeParam::Coordinate> source(1, 2, 3);
    TypeParam dest = source;
    ASSERT_EQ(1, dest.x());
    ASSERT_EQ(2, dest.y());
    ASSERT_EQ(3, dest.z());
  }

  TYPED_TEST(Vector3Test, ShouldDefineNullVector) {
    TypeParam expected(0, 0, 0);
    ASSERT_EQ(expected, TypeParam::null());
  }
  
  TYPED_TEST(Vector3Test, ShouldDefineEpsilonVector) {
    ASSERT_TRUE(TypeParam::epsilon().x() > 0.0);
    ASSERT_TRUE(TypeParam::epsilon().y() > 0.0);
    ASSERT_TRUE(TypeParam::epsilon().z() > 0.0);
  }
  
  TYPED_TEST(Vector3Test, ShouldDefineUndefinedVector) {
    ASSERT_TRUE(TypeParam::undefined().isUndefined());
  }
  
  TYPED_TEST(Vector3Test, ShouldDefineMinusInfinityVector) {
    ASSERT_EQ(- numeric_limits<float>::infinity(), TypeParam::minusInfinity().x());
  }
  
  TYPED_TEST(Vector3Test, ShouldDefinePlusInfinityVector) {
    ASSERT_EQ(numeric_limits<float>::infinity(), TypeParam::plusInfinity().x());
  }
  
  TYPED_TEST(Vector3Test, ShouldDefineRightVector) {
    ASSERT_EQ(1, TypeParam::right().x());
  }
  
  TYPED_TEST(Vector3Test, ShouldDefineUpVector) {
    ASSERT_EQ(1, TypeParam::up().y());
  }
  
  TYPED_TEST(Vector3Test, ShouldDefineForwardVector) {
    ASSERT_EQ(1, TypeParam::forward().z());
  }
  
  TYPED_TEST(Vector3Test, ShouldCalculateCrossProductOfUnitVectors) {
    TypeParam x(1, 0, 0), y(0, 1, 0);
    TypeParam expected(0, 0, 1);
    ASSERT_EQ(expected, x ^ y);
  }
  
  TYPED_TEST(Vector3Test, ShouldAdd) {
    TypeParam v1(1, 2, 1), v2(0, 1, 0), expected(1, 3, 1);
    ASSERT_EQ(expected, v1 + v2);
  }
  
  TYPED_TEST(Vector3Test, ShouldSubtract) {
    TypeParam v1(1, 3, 1), v2(0, 1, 0), expected(1, 2, 1);
    ASSERT_EQ(expected, v1 - v2);
  }
  
  TYPED_TEST(Vector3Test, ShouldNegate) {
    TypeParam v(1, 2, 3), expected(-1, -2, -3);
    ASSERT_EQ(expected, -v);
  }
  
  TYPED_TEST(Vector3Test, ShouldDivide) {
    TypeParam v(2, 4, 2), expected(1, 2, 1);
    ASSERT_EQ(expected, v / 2);
  }
  
  TYPED_TEST(Vector3Test, ShouldNotAllowDivisionByZero) {
    TypeParam vector;
    ASSERT_THROW(vector / 0, DivisionByZeroException);
  }
  
  TYPED_TEST(Vector3Test, ShouldCalculateDotProduct) {
    TypeParam v1(0, 2, 1), v2(1, 2, 1);
    ASSERT_EQ(5, v1 * v2);
  }
  
  TYPED_TEST(Vector3Test, ShouldMultiply) {
    TypeParam v(2, 1, 1), expected(4, 2, 2);
    ASSERT_EQ(expected, v * 2);
  }
  
  TYPED_TEST(Vector3Test, ShouldCompareSameVectorForEquality) {
    TypeParam vector;
    ASSERT_TRUE(vector == vector);
  }

  TYPED_TEST(Vector3Test, ShouldCompareVectorsForEquality) {
    TypeParam first(1, 2, 3), second(1, 2, 3);
    ASSERT_TRUE(first == second);
  }

  TYPED_TEST(Vector3Test, ShouldCompareVectorsForInequality) {
    TypeParam first(1, 2, 3);
    TypeParam second(4, 5, 6);

    ASSERT_TRUE(first != second);
  }
  
  TYPED_TEST(Vector3Test, ShouldAddInPlace) {
    TypeParam v1(1, 2, 1), v2(0, 1, 0), expected(1, 3, 1);
    v1 += v2;
    ASSERT_EQ(expected, v1);
  }
  
  TYPED_TEST(Vector3Test, ShouldSubtractInPlace) {
    TypeParam v1(1, 3, 1), v2(0, 1, 0), expected(1, 2, 1);
    v1 -= v2;
    ASSERT_EQ(expected, v1);
  }
  
  TYPED_TEST(Vector3Test, ShouldDivideInPlace) {
    TypeParam v(2, 4, 2), expected(1, 2, 1);
    v /= 2;
    ASSERT_EQ(expected, v);
  }
  
  TYPED_TEST(Vector3Test, ShouldNotAllowDivisionByZeroInPlace) {
    TypeParam vector;
    ASSERT_THROW(vector /= 0, DivisionByZeroException);
  }
  
  TYPED_TEST(Vector3Test, ShouldMultiplyInPlace) {
    TypeParam v(2, 1, 1), expected(4, 2, 2);
    v *= 2;
    ASSERT_EQ(expected, v);
  }
  
  TYPED_TEST(Vector3Test, ShouldCalculateLength) {
    ASSERT_EQ(2, TypeParam(2, 0, 0).length());
    ASSERT_EQ(2, TypeParam(0, 2, 0).length());
  }
  
  TYPED_TEST(Vector3Test, ShouldNormalize) {
    TypeParam v(2, 0, 0), expected(1, 0, 0);
    v.normalize();
    ASSERT_EQ(expected, v);
  }
  
  TYPED_TEST(Vector3Test, ShouldReturnNormalizedVector) {
    TypeParam v(2, 0, 0), expected(1, 0, 0);
    ASSERT_EQ(expected, v.normalized());
  }
}

namespace Vector4Test {
  template<class T>
  class Vector4Test : public ::testing::Test {
  };

  typedef ::testing::Types<Vector4<float>, Vector4<double>> Vector4Types;
  TYPED_TEST_CASE(Vector4Test, Vector4Types);
  
  TYPED_TEST(Vector4Test, ShouldInitializeNullVector) {
    TypeParam vector;
    ASSERT_EQ(0, vector.x());
    ASSERT_EQ(0, vector.y());
    ASSERT_EQ(0, vector.z());
    ASSERT_EQ(1, vector.w());
  }

  TYPED_TEST(Vector4Test, ShouldInitializeCoordinates) {
    TypeParam vector(1, 2, 3, 4);
    ASSERT_EQ(1, vector.x());
    ASSERT_EQ(2, vector.y());
    ASSERT_EQ(3, vector.z());
    ASSERT_EQ(4, vector.w());
  }

  TYPED_TEST(Vector4Test, ShouldInitializeWCoordinateAsOneByDefault) {
    TypeParam vector(1, 2, 3);
    ASSERT_EQ(1, vector.w());
  }

  TYPED_TEST(Vector4Test, ShouldInitializeWCoordinateAsOneByDefaultInDefaultConstructor) {
    TypeParam vector;
    ASSERT_EQ(1, vector.w());
  }

  TYPED_TEST(Vector4Test, ShouldCopyFromVector3) {
    Vector3<typename TypeParam::Coordinate> source(1, 2, 3);
    TypeParam dest = source;
    ASSERT_EQ(1, dest.x());
    ASSERT_EQ(2, dest.y());
    ASSERT_EQ(3, dest.z());
    ASSERT_EQ(1, dest.w());
  }

  TYPED_TEST(Vector4Test, ShouldPreserveWCoordinateWhenCopyingFromVector4) {
    TypeParam source(1, 2, 3, 5);
    TypeParam dest = source;
    ASSERT_EQ(1, dest.x());
    ASSERT_EQ(2, dest.y());
    ASSERT_EQ(3, dest.z());
    ASSERT_EQ(5, dest.w());
  }

  TYPED_TEST(Vector4Test, ShouldDefineNullVector) {
    TypeParam expected(0, 0, 0, 1);
    ASSERT_EQ(expected, TypeParam::null());
  }

  TYPED_TEST(Vector4Test, ShouldDefineEpsilonVector) {
    ASSERT_TRUE(TypeParam::epsilon().x() > 0.0);
    ASSERT_TRUE(TypeParam::epsilon().y() > 0.0);
    ASSERT_TRUE(TypeParam::epsilon().z() > 0.0);
    ASSERT_TRUE(TypeParam::epsilon().w() > 0.0);
  }
  
  TYPED_TEST(Vector4Test, ShouldDefineUndefinedVector) {
    ASSERT_TRUE(TypeParam::undefined().isUndefined());
  }
  
  TYPED_TEST(Vector4Test, ShouldDefineMinusInfinityVector) {
    ASSERT_EQ(- numeric_limits<float>::infinity(), TypeParam::minusInfinity().x());
  }
  
  TYPED_TEST(Vector4Test, ShouldDefinePlusInfinityVector) {
    ASSERT_EQ(numeric_limits<float>::infinity(), TypeParam::plusInfinity().x());
  }
  
  TYPED_TEST(Vector4Test, ShouldReturnHomogenizedVector) {
    TypeParam vector(2, 4, 6, 2);
    Vector3<typename TypeParam::Coordinate> expected(1, 2, 3);
    
    ASSERT_EQ(expected, vector.homogenized());
  }
  
  TYPED_TEST(Vector4Test, ShouldAdd) {
    TypeParam v1(1, 2, 1, 1), v2(0, 1, 0, 0), expected(1, 3, 1, 1);
    ASSERT_EQ(expected, v1 + v2);
  }

  TYPED_TEST(Vector4Test, ShouldSubtract) {
    TypeParam v1(1, 3, 1, 2), v2(0, 1, 0, 1), expected(1, 2, 1, 1);
    ASSERT_EQ(expected, v1 - v2);
  }

  TYPED_TEST(Vector4Test, ShouldNegate) {
    TypeParam v(1, 2, 3, 1), expected(-1, -2, -3, -1);
    ASSERT_EQ(expected, -v);
  }

  TYPED_TEST(Vector4Test, ShouldDivide) {
    TypeParam v(2, 4, 2, 4), expected(1, 2, 1, 2);
    ASSERT_EQ(expected, v / 2);
  }

  TYPED_TEST(Vector4Test, ShouldNotAllowDivisionByZero) {
    TypeParam vector;
    ASSERT_THROW(vector / 0, DivisionByZeroException);
  }

  TYPED_TEST(Vector4Test, ShouldCalculateDotProduct) {
    TypeParam v1(0, 2, 1, 1), v2(1, 2, 1, 1);
    ASSERT_EQ(6, v1 * v2);
  }

  TYPED_TEST(Vector4Test, ShouldMultiply) {
    TypeParam v(2, 1, 1, 2), expected(4, 2, 2, 4);
    ASSERT_EQ(expected, v * 2);
  }

  TYPED_TEST(Vector4Test, ShouldCompareSameVectorForEquality) {
    TypeParam vector;
    ASSERT_TRUE(vector == vector);
  }

  TYPED_TEST(Vector4Test, ShouldCompareVectorsForEquality) {
    TypeParam first(1, 2, 3, 1), second(1, 2, 3, 1);
    ASSERT_TRUE(first == second);
  }

  TYPED_TEST(Vector4Test, ShouldCompareVectorsForInequality) {
    TypeParam first(1, 2, 3, 4);
    TypeParam second(4, 5, 6, 7);

    ASSERT_TRUE(first != second);
  }

  TYPED_TEST(Vector4Test, ShouldAddInPlace) {
    TypeParam v1(1, 2, 1, 1), v2(0, 1, 0, 0), expected(1, 3, 1, 1);
    v1 += v2;
    ASSERT_EQ(expected, v1);
  }

  TYPED_TEST(Vector4Test, ShouldSubtractInPlace) {
    TypeParam v1(1, 3, 1, 2), v2(0, 1, 0, 1), expected(1, 2, 1, 1);
    v1 -= v2;
    ASSERT_EQ(expected, v1);
  }

  TYPED_TEST(Vector4Test, ShouldDivideInPlace) {
    TypeParam v(2, 4, 2, 4), expected(1, 2, 1, 2);
    v /= 2;
    ASSERT_EQ(expected, v);
  }

  TYPED_TEST(Vector4Test, ShouldNotAllowDivisionByZeroInPlace) {
    TypeParam vector;
    ASSERT_THROW(vector /= 0, DivisionByZeroException);
  }

  TYPED_TEST(Vector4Test, ShouldMultiplyInPlace) {
    TypeParam v(2, 1, 1, 2), expected(4, 2, 2, 4);
    v *= 2;
    ASSERT_EQ(expected, v);
  }
  
  TYPED_TEST(Vector4Test, ShouldCalculateLength) {
    ASSERT_EQ(2, TypeParam(2, 0, 0, 0).length());
    ASSERT_EQ(2, TypeParam(0, 2, 0, 0).length());
  }
  
  TYPED_TEST(Vector4Test, ShouldNormalize) {
    TypeParam v(2, 0, 0, 0), expected(1, 0, 0, 0);
    v.normalize();
    ASSERT_EQ(expected, v);
  }
  
  TYPED_TEST(Vector4Test, ShouldReturnNormalizedVector) {
    TypeParam v(2, 0, 0, 0), expected(1, 0, 0, 0);
    ASSERT_EQ(expected, v.normalized());
  }
}
