#include "gtest.h"
#include "core/math/Vector.h"
#include "test/helpers/TypeTestHelper.h"

#include <limits>

using namespace std;

namespace VectorTest {
  template<class T>
  class VectorTest : public ::testing::Test {
  };

  typedef ::testing::Types<float, double, long double> VectorTypes;
  TYPED_TEST_CASE(VectorTest, VectorTypes);

  TYPED_TEST(VectorTest, ShouldInitializeCoordinatesWithZeros) {
    Vector<3, TypeParam> vector;
    ASSERT_EQ(0, vector[0]);
    ASSERT_EQ(0, vector[1]);
    ASSERT_EQ(0, vector[2]);
  }

  TYPED_TEST(VectorTest, ShouldGetAndSetCoordinate) {
    Vector<3, TypeParam> vector;
    vector.setCoordinate(0, 10.0);
    ASSERT_EQ(10.0, vector.coordinate(0));
  }

  TYPED_TEST(VectorTest, ShouldGetAndSetCoordinateWithIndexOperator) {
    Vector<3, TypeParam> vector;
    vector[1] = 10.0;
    ASSERT_EQ(10.0, vector[1]);
  }

  TYPED_TEST(VectorTest, ShouldCopyVector) {
    Vector<3, TypeParam> vector;
    vector[0] = 10.0;

    Vector<3, TypeParam> copy = vector;
    ASSERT_EQ(10.0, copy[0]);
  }

  TYPED_TEST(VectorTest, ShouldCopyFromSmallerVector) {
    Vector<3, TypeParam> source;
    source[0] = 10.0;

    Vector<4, TypeParam> dest = source;
    ASSERT_EQ(10.0, dest[0]);
  }

  TYPED_TEST(VectorTest, ShouldCopyFromLargerVector) {
    Vector<3, TypeParam> source;
    source[0] = 10.0;

    Vector<2, TypeParam> dest = source;
    ASSERT_EQ(10.0, dest[0]);
  }

  TYPED_TEST(VectorTest, ShouldInitializeVectorFromCArray) {
    Vector<3, TypeParam> vector({ 1, 2, 3 });
    ASSERT_EQ(1, vector[0]);
    ASSERT_EQ(2, vector[1]);
    ASSERT_EQ(3, vector[2]);
  }

  TYPED_TEST(VectorTest, ShouldAdd) {
    Vector<3, TypeParam> vector1, vector2;
    vector1[0] = 10.0;
    vector1[1] = 20.0;
    vector2[1] = 10.0;
    vector2[2] = 20.0;

    Vector<3, TypeParam> vector3 = vector1 + vector2;
    ASSERT_EQ(10.0, vector3[0]);
    ASSERT_EQ(30.0, vector3[1]);
    ASSERT_EQ(20.0, vector3[2]);
  }
  
  TYPED_TEST(VectorTest, ShouldAddInPlace) {
    Vector<3, TypeParam> vector1, vector2;
    vector1[0] = 10.0;
    vector1[1] = 20.0;
    vector2[1] = 10.0;
    vector2[2] = 20.0;

    vector1 += vector2;
    ASSERT_EQ(10.0, vector1[0]);
    ASSERT_EQ(30.0, vector1[1]);
    ASSERT_EQ(20.0, vector1[2]);
  }

  TYPED_TEST(VectorTest, ShouldSubtract) {
    Vector<3, TypeParam> vector1, vector2;
    vector1[0] = 10.0;
    vector1[1] = 20.0;
    vector2[1] = 10.0;
    vector2[2] = 20.0;

    Vector<3, TypeParam> vector3 = vector1 - vector2;
    ASSERT_EQ( 10.0, vector3[0]);
    ASSERT_EQ( 10.0, vector3[1]);
    ASSERT_EQ(-20.0, vector3[2]);
  }

  TYPED_TEST(VectorTest, ShouldSubtractInPlace) {
    Vector<3, TypeParam> vector1, vector2;
    vector1[0] = 10.0;
    vector1[1] = 20.0;
    vector2[1] = 10.0;
    vector2[2] = 20.0;

    vector1 -= vector2;
    ASSERT_EQ( 10.0, vector1[0]);
    ASSERT_EQ( 10.0, vector1[1]);
    ASSERT_EQ(-20.0, vector1[2]);
  }

  TYPED_TEST(VectorTest, ShouldNegate) {
    Vector<3, TypeParam> vector;
    vector[0] = 10.0;

    Vector<3, TypeParam> negated = -vector;
    ASSERT_EQ(-10.0, negated[0]);
  }

  TYPED_TEST(VectorTest, ShouldCalculateLength) {
    Vector<3, TypeParam> vector;
    vector[0] = 10.0;

    ASSERT_EQ(10.0, vector.length());
  }
  
  TYPED_TEST(VectorTest, ShouldCalculateSquaredLength) {
    Vector<3, TypeParam> vector;
    vector[0] = 2.0;

    ASSERT_EQ(4.0, vector.squaredLength());
  }
  
  TYPED_TEST(VectorTest, ShouldCalculateDistanceToOtherVector) {
    Vector<3, TypeParam> vector1, vector2;
    vector1[0] = 10.0;
    vector2[0] = 5.0;
    
    ASSERT_EQ(5.0, vector1.distanceTo(vector2));
  }
  
  TYPED_TEST(VectorTest, ShouldReturnSameValueForReversedDistance) {
    Vector<3, TypeParam> vector1, vector2, foo;
    vector1[0] = 10.0;
    vector2[0] = 5.0;
    
    ASSERT_EQ(vector1.distanceTo(vector2), vector2.distanceTo(vector1));
  }

  TYPED_TEST(VectorTest, ShouldCalculateSquaredDistanceToOtherVector) {
    Vector<3, TypeParam> vector1, vector2;
    vector1[0] = 10.0;
    vector2[0] = 5.0;
    
    ASSERT_EQ(25.0, vector1.squaredDistanceTo(vector2));
  }
  
  TYPED_TEST(VectorTest, ShouldReturnSameValueForReversedSquaredDistance) {
    Vector<3, TypeParam> vector1, vector2, foo;
    vector1[0] = 10.0;
    vector2[0] = 5.0;
    
    ASSERT_EQ(vector1.squaredDistanceTo(vector2), vector2.squaredDistanceTo(vector1));
  }

  TYPED_TEST(VectorTest, ShouldDivideVectorByScalar) {
    Vector<3, TypeParam> vector;
    vector[0] = 10.0;

    Vector<3, TypeParam> shrunk = vector / 2;

    ASSERT_EQ(5.0, shrunk[0]);
  }

  TYPED_TEST(VectorTest, ShouldDivideVectorByScalarInPlace) {
    Vector<3, TypeParam> vector;
    vector[0] = 10.0;

    vector /= 2;

    ASSERT_EQ(5.0, vector[0]);
  }

  TYPED_TEST(VectorTest, ShouldNotAllowDivisionByZero) {
    Vector<3, TypeParam> vector;
    ASSERT_THROW(vector / 0, DivisionByZeroException);
  }
  
  TYPED_TEST(VectorTest, ShouldNotAllowInPlaceDivisionByZero) {
    Vector<3, TypeParam> vector;
    ASSERT_THROW(vector /= 0, DivisionByZeroException);
  }
  
  TYPED_TEST(VectorTest, ShouldCalculateDotProduct) {
    Vector<3, TypeParam> vector({ 1, 0, 0 });
    
    ASSERT_EQ(1, vector.dotProduct(vector));
  }
  
  TYPED_TEST(VectorTest, ShouldCalculateDotProductOfNullVectors) {
    Vector<3, TypeParam> first, second;
    ASSERT_EQ(0, first * second);
  }
  
  TYPED_TEST(VectorTest, ShouldCalculateDotProductOfUnitVectors) {
    Vector<3, TypeParam> vector({ 1, 0, 0 });
    
    ASSERT_EQ(1, vector * vector);
  }
  
  TYPED_TEST(VectorTest, ShouldCalculateDotProductWithOperator) {
    Vector<3, TypeParam> first({ 1, 2, 2 });
    Vector<3, TypeParam> second({ 3, 4, 2 });
    
    ASSERT_EQ(15, first * second);
  }
  
  TYPED_TEST(VectorTest, ShouldFollowDotProductRules) {
    Vector<3, TypeParam> first({ 1, 2, 2 });
    Vector<3, TypeParam> second({ 3, 4, 2 });
    
    ASSERT_EQ(-(first * second), (-first * second));
    ASSERT_EQ(-(first * second), (first * -second));
    ASSERT_EQ(first * second, second * first);
  }

  TYPED_TEST(VectorTest, ShouldMultiplyVectorByScalar) {
    Vector<3, TypeParam> vector;
    vector[0] = 10.0;

    Vector<3, TypeParam> enlarged = vector * 2;

    ASSERT_EQ(20.0, enlarged[0]);
  }

  TYPED_TEST(VectorTest, ShouldMultiplyVectorByScalarInPlace) {
    Vector<3, TypeParam> vector;
    vector[0] = 10.0;

    vector *= 2;

    ASSERT_EQ(20.0, vector[0]);
  }
  
  TYPED_TEST(VectorTest, ShouldGetReversedVector) {
    Vector<3, TypeParam> vector({0, 5, 4});

    Vector<3, TypeParam> reversed = vector.reversed();
    Vector<3, TypeParam> expected({0, -5, -4});
    ASSERT_EQ(expected, reversed);
  }
  
  TYPED_TEST(VectorTest, ShouldReverseInPlace) {
    Vector<3, TypeParam> vector({0, 5, 4});

    vector.reverse();
    Vector<3, TypeParam> expected({0, -5, -4});
    ASSERT_EQ(expected, vector);
  }

  TYPED_TEST(VectorTest, ShouldGetNormalizedVector) {
    Vector<3, TypeParam> vector;
    vector[0] = 10;

    Vector<3, TypeParam> normalized = vector.normalized();
    ASSERT_EQ(1, normalized[0]);
    ASSERT_EQ(0, normalized[1]);
    ASSERT_EQ(0, normalized[2]);
  }
  
  TYPED_TEST(VectorTest, ShouldNormalizeInPlace) {
    Vector<3, TypeParam> vector;
    vector[0] = 10;

    vector.normalize();
    ASSERT_EQ(1, vector[0]);
    ASSERT_EQ(0, vector[1]);
    ASSERT_EQ(0, vector[2]);
  }

  TYPED_TEST(VectorTest, ShouldFindOutIfItIsNormalized) {
    Vector<3, TypeParam> normal, vector;
    normal[0] = 1;
    vector[0] = 10;

    ASSERT_TRUE(normal.isNormalized());
    ASSERT_FALSE(vector.isNormalized());
  }

  TYPED_TEST(VectorTest, ShouldReturnTrueWhenComparingTwoDefaultVectors) {
    Vector<3, TypeParam> first, second;
    ASSERT_TRUE(first == second);
  }

  TYPED_TEST(VectorTest, ShouldCompareSameVectorForEquality) {
    Vector<3, TypeParam> vector;
    ASSERT_TRUE(vector == vector);
  }

  TYPED_TEST(VectorTest, ShouldCompareVectorsForEquality) {
    TypeParam elements[3] = { 1, 2, 3 };
    Vector<3, TypeParam> first(elements), second(elements);
    ASSERT_TRUE(first == second);
  }

  TYPED_TEST(VectorTest, ShouldCompareVectorsForInequality) {
    Vector<3, TypeParam> first({ 1, 2, 3 });
    Vector<3, TypeParam> second({ 4, 5, 6 });

    ASSERT_TRUE(first != second);
  }
  
  TYPED_TEST(VectorTest, ShouldReturnTrueForUndefinedIfVectorIsUndefined) {
    Vector<3, TypeParam> vector({ numeric_limits<TypeParam>::quiet_NaN(), 0, 0 });
    ASSERT_TRUE(vector.isUndefined());
  }
  
  TYPED_TEST(VectorTest, ShouldReturnFalseForUndefinedIfVectorIsDefined) {
    Vector<3, TypeParam> vector({ 0, 0, 0 });
    ASSERT_FALSE(vector.isUndefined());
  }

  TYPED_TEST(VectorTest, ShouldReturnTrueForDefinedIfVectorIsDefined) {
    Vector<3, TypeParam> vector({ 0, 0, 0 });
    ASSERT_TRUE(vector.isDefined());
  }
  
  TYPED_TEST(VectorTest, ShouldReturnFalseForDefinedIfVectorIsUndefined) {
    Vector<3, TypeParam> vector({ numeric_limits<TypeParam>::quiet_NaN(), 0, 0 });
    ASSERT_FALSE(vector.isDefined());
  }
  
  TYPED_TEST(VectorTest, ShouldReturnTrueForInfiniteIfVectorIsInfinite) {
    Vector<3, TypeParam> vector({ numeric_limits<TypeParam>::infinity(), 0, 0 });
    ASSERT_TRUE(vector.isInfinite());
  }
  
  TYPED_TEST(VectorTest, ShouldReturnFalseForInfiniteIfVectorIsFinite) {
    Vector<3, TypeParam> vector({ 0, 0, 0 });
    ASSERT_FALSE(vector.isInfinite());
  }

  TYPED_TEST(VectorTest, ShouldReturnTrueForIsNullIfVectorIsNullVector) {
    Vector<3, TypeParam> vector;
    ASSERT_TRUE(vector.isNull());
  }
  
  TYPED_TEST(VectorTest, ShouldReturnFalseForIsNullOtherwise) {
    Vector<3, TypeParam> vector({2, 5, 5});
    ASSERT_FALSE(vector.isNull());
  }
  
  TYPED_TEST(VectorTest, ShouldReturnMinComponent) {
    Vector<3, TypeParam> vec({ 1, 2, -2 });
    ASSERT_EQ(-2, vec.min());
  }
  
  TYPED_TEST(VectorTest, ShouldReturnMaxComponent) {
    Vector<3, TypeParam> vec({ 1, 2, -2 });
    ASSERT_EQ(2, vec.max());
  }
  
  TYPED_TEST(VectorTest, ShouldReturnAbsVectir) {
    Vector<3, TypeParam> vec({ 1, 2, -2 });
    Vector<3, TypeParam> expected({ 1, 2, 2});
    ASSERT_EQ(expected, vec.abs());
  }
  
  TYPED_TEST(VectorTest, ShouldMultiplyFromLeft) {
    Vector<3, TypeParam> vec({ 1, 2, -2 });
    Vector<3, TypeParam> expected({ 2, 4, -4});
    ASSERT_EQ(expected, TypeParam(2) * vec);
  }
}

namespace DerivedVectorTest {
  template<class T>
  class DerivedVectorTest : public ::testing::Test {
  };

  typedef ::testing::Types<
    Vector2<float>, Vector3<float>, Vector4<float>,
    Vector2<double>, Vector3<double>, Vector4<double>,
    Vector2<long double>, Vector3<long double>, Vector4<long double>
  > DerivedVectorTypes;
  TYPED_TEST_CASE(DerivedVectorTest, DerivedVectorTypes);

  TYPED_TEST(DerivedVectorTest, ShouldReturnCorrectTypeForDotProduct) {
    TypeParam vector;
    ASSERT_TYPES_EQ(typename TypeParam::Coordinate(0), vector * vector);
  }

  TYPED_TEST(DerivedVectorTest, ShouldReturnCorrectTypeForMultiplicationWithScalar) {
    TypeParam vector;
    ASSERT_TYPES_EQ(vector, vector * 2);
  }

  TYPED_TEST(DerivedVectorTest, ShouldReturnCorrectTypeForDivisionByScalar) {
    TypeParam vector;
    ASSERT_TYPES_EQ(vector, vector / 2);
  }

  TYPED_TEST(DerivedVectorTest, ShouldReturnCorrectTypeForInPlaceAddition) {
    TypeParam vector;
    ASSERT_TYPES_EQ(vector, vector += vector);
  }

  TYPED_TEST(DerivedVectorTest, ShouldReturnCorrectTypeForInPlaceSubtraction) {
    TypeParam vector;
    ASSERT_TYPES_EQ(vector, vector -= vector);
  }

  TYPED_TEST(DerivedVectorTest, ShouldReturnCorrectTypeForInPlaceMultiplicationWithScalar) {
    TypeParam vector;
    ASSERT_TYPES_EQ(vector, vector *= 2);
  }

  TYPED_TEST(DerivedVectorTest, ShouldReturnCorrectTypeForInPlaceDivisionByScalar) {
    TypeParam vector;
    ASSERT_TYPES_EQ(vector, vector /= 2);
  }

  TYPED_TEST(DerivedVectorTest, ShouldReturnCorrectTypeForAddition) {
    TypeParam vector;
    ASSERT_TYPES_EQ(vector, vector + vector);
  }

  TYPED_TEST(DerivedVectorTest, ShouldReturnCorrectTypeForSubtraction) {
    TypeParam vector;
    ASSERT_TYPES_EQ(vector, vector - vector);
  }

  TYPED_TEST(DerivedVectorTest, ShouldReturnCorrectTypeForNegation) {
    TypeParam vector;
    ASSERT_TYPES_EQ(vector, - vector);
  }

  TYPED_TEST(DerivedVectorTest, ShouldReturnCorrectTypeForReversal) {
    TypeParam vector;
    vector[0] = 1;
    ASSERT_TYPES_EQ(vector, vector.reversed());
  }
  
  TYPED_TEST(DerivedVectorTest, ShouldReturnCorrectTypeForNormalization) {
    TypeParam vector;
    vector[0] = 1;
    ASSERT_TYPES_EQ(vector, vector.normalized());
  }
  
  TYPED_TEST(DerivedVectorTest, ShouldEvaluateEquality) {
    TypeParam vector1, vector2;
    ASSERT_TRUE(vector1 == vector2);
  }
  
  TYPED_TEST(DerivedVectorTest, ShouldEvaluateInequality) {
    TypeParam vector1, vector2;
    vector2[0] = 1;
    ASSERT_TRUE(vector1 != vector2);
  }
  
  TYPED_TEST(DerivedVectorTest, ShouldCalculateLength) {
    TypeParam vector;
    vector[TypeParam::Dim - 1] = 1;
    ASSERT_EQ(1, vector.length());
  }
  
  TYPED_TEST(DerivedVectorTest, ShouldCalculateSquaredLength) {
    TypeParam vector;
    vector[TypeParam::Dim - 1] = 1;
    ASSERT_EQ(1, vector.squaredLength());
  }

  TYPED_TEST(DerivedVectorTest, ShouldCalculateDistance) {
    TypeParam vector1, vector2;
    vector1[0] = 1;
    ASSERT_EQ(1, vector2.distanceTo(vector1));
  }
  
  TYPED_TEST(DerivedVectorTest, ShouldProvideUndefinedVector) {
    TypeParam vector = TypeParam::undefined();
    ASSERT_TRUE(vector.isUndefined());
  }
}

namespace Vector2Test {
  template<class T>
  class Vector2Test : public ::testing::Test {
  };

  typedef ::testing::Types<float, double, long double> Vector2Types;
  TYPED_TEST_CASE(Vector2Test, Vector2Types);
  
  TYPED_TEST(Vector2Test, ShouldHaveRightSize) {
    ASSERT_EQ(sizeof(Vector<2, TypeParam>), sizeof(Vector2<TypeParam>));
    ASSERT_EQ(2 * sizeof(TypeParam), sizeof(Vector2<TypeParam>));
  }

  TYPED_TEST(Vector2Test, ShouldInitializeNullVector) {
    Vector2<TypeParam> vector;
    ASSERT_EQ(0, vector.x());
    ASSERT_EQ(0, vector.y());
  }

  TYPED_TEST(Vector2Test, ShouldInitializeCoordinates) {
    Vector2<TypeParam> vector(1, 2);
    ASSERT_EQ(1, vector.x());
    ASSERT_EQ(2, vector.y());
  }

  TYPED_TEST(Vector2Test, ShouldCopyFromVector3) {
    Vector3<TypeParam> source(1, 2, 3);
    Vector2<TypeParam> dest = source;
    ASSERT_EQ(1, dest.x());
    ASSERT_EQ(2, dest.y());
  }
  
  TYPED_TEST(Vector2Test, ShouldDefineNullVector) {
    Vector2<TypeParam> expected(0, 0);
    ASSERT_EQ(expected, Vector2<TypeParam>::null());
    ASSERT_TRUE(Vector2<TypeParam>::null().isNull());
  }
  
  TYPED_TEST(Vector2Test, ShouldDefineRightVector) {
    Vector2<TypeParam> expected(1, 0);
    ASSERT_EQ(expected, Vector2<TypeParam>::right());
  }
  
  TYPED_TEST(Vector2Test, ShouldDefineUpVector) {
    Vector2<TypeParam> expected(0, 1);
    ASSERT_EQ(expected, Vector2<TypeParam>::up());
  }
  
  TYPED_TEST(Vector2Test, ShouldReturnX) {
    auto vec = Vector2<TypeParam>(3, 4);
    ASSERT_EQ(3, vec.x());
  }
  
  TYPED_TEST(Vector2Test, ShouldSetX) {
    auto vec = Vector2<TypeParam>(3, 4);
    vec.setX(5);
    ASSERT_EQ(Vector2<TypeParam>(5, 4), vec);
  }
  
  TYPED_TEST(Vector2Test, ShouldReturnY) {
    auto vec = Vector2<TypeParam>(3, 4);
    ASSERT_EQ(4, vec.y());
  }
  
  TYPED_TEST(Vector2Test, ShouldSetY) {
    auto vec = Vector2<TypeParam>(3, 4);
    vec.setY(5);
    ASSERT_EQ(Vector2<TypeParam>(3, 5), vec);
  }
}

namespace Vector3Test {
  template<class T>
  class Vector3Test : public ::testing::Test {
  };

  typedef ::testing::Types<float, double, long double> Vector3Types;
  TYPED_TEST_CASE(Vector3Test, Vector3Types);
  
  TYPED_TEST(Vector3Test, ShouldHaveRightSize) {
#if defined(__SSE__) || defined(__SSE3__)
    if (sizeof(TypeParam) == sizeof(float) || sizeof(TypeParam) == sizeof(double)) {
      ASSERT_EQ(sizeof(Vector<3, TypeParam>) + sizeof(TypeParam), sizeof(Vector3<TypeParam>));
      ASSERT_EQ(4 * sizeof(TypeParam), sizeof(Vector3<TypeParam>));
    } else {
      // long double has no SSE-enabled specializetion, so in this case, the
      // size is as specified below
      ASSERT_EQ(sizeof(Vector<3, TypeParam>), sizeof(Vector3<TypeParam>));
      ASSERT_EQ(3 * sizeof(TypeParam), sizeof(Vector3<TypeParam>));
    }
#else
    ASSERT_EQ(sizeof(Vector<3, TypeParam>), sizeof(Vector3<TypeParam>));
    ASSERT_EQ(3 * sizeof(TypeParam), sizeof(Vector3<TypeParam>));
#endif
  }
  
  TYPED_TEST(Vector3Test, ShouldInitializeNullVector) {
    Vector3<TypeParam> vector;
    ASSERT_EQ(0, vector.x());
    ASSERT_EQ(0, vector.y());
    ASSERT_EQ(0, vector.z());
    ASSERT_TRUE(vector.isNull());
  }

  TYPED_TEST(Vector3Test, ShouldInitializeCoordinates) {
    Vector3<TypeParam> vector(1, 2, 3);
    ASSERT_EQ(1, vector.x());
    ASSERT_EQ(2, vector.y());
    ASSERT_EQ(3, vector.z());
  }

  TYPED_TEST(Vector3Test, ShouldInitializeZCoordinateAsZeroByDefault) {
    Vector3<TypeParam> vector(1, 2);
    ASSERT_EQ(0, vector.z());
  }

  TYPED_TEST(Vector3Test, ShouldCopyFromVector2) {
    Vector2<TypeParam> source(1, 2);
    Vector3<TypeParam> dest = source;
    ASSERT_EQ(1, dest.x());
    ASSERT_EQ(2, dest.y());
    ASSERT_EQ(0, dest.z());
  }

  TYPED_TEST(Vector3Test, ShouldCopyFromVector4) {
    Vector4<TypeParam> source(1, 2, 3);
    Vector3<TypeParam> dest = source;
    ASSERT_EQ(1, dest.x());
    ASSERT_EQ(2, dest.y());
    ASSERT_EQ(3, dest.z());
  }

  TYPED_TEST(Vector3Test, ShouldDefineNullVector) {
    Vector3<TypeParam> expected(0, 0, 0);
    ASSERT_EQ(expected, Vector3<TypeParam>::null());
    ASSERT_TRUE(Vector3<TypeParam>::null().isNull());
  }
  
  TYPED_TEST(Vector3Test, ShouldDefineEpsilonVector) {
    ASSERT_TRUE(Vector3<TypeParam>::epsilon().x() > 0.0);
    ASSERT_TRUE(Vector3<TypeParam>::epsilon().y() > 0.0);
    ASSERT_TRUE(Vector3<TypeParam>::epsilon().z() > 0.0);
  }
  
  TYPED_TEST(Vector3Test, ShouldDefineUndefinedVector) {
    ASSERT_TRUE(Vector3<TypeParam>::undefined().isUndefined());
  }
  
  TYPED_TEST(Vector3Test, ShouldDefineMinusInfinityVector) {
    ASSERT_EQ(- numeric_limits<TypeParam>::infinity(), Vector3<TypeParam>::minusInfinity().x());
  }
  
  TYPED_TEST(Vector3Test, ShouldDefinePlusInfinityVector) {
    ASSERT_EQ(numeric_limits<TypeParam>::infinity(), Vector3<TypeParam>::plusInfinity().x());
  }
  
  TYPED_TEST(Vector3Test, ShouldDefineRightVector) {
    ASSERT_EQ(1, Vector3<TypeParam>::right().x());
  }
  
  TYPED_TEST(Vector3Test, ShouldDefineUpVector) {
    ASSERT_EQ(1, Vector3<TypeParam>::up().y());
  }
  
  TYPED_TEST(Vector3Test, ShouldDefineForwardVector) {
    ASSERT_EQ(1, Vector3<TypeParam>::forward().z());
  }
  
  TYPED_TEST(Vector3Test, ShouldCalculateCrossProductOfUnitVectors) {
    Vector3<TypeParam> x(1, 0, 0), y(0, 1, 0);
    Vector3<TypeParam> expected(0, 0, 1);
    ASSERT_EQ(expected, x.crossProduct(y));
  }
  
  TYPED_TEST(Vector3Test, ShouldCalculateCrossProductOfUnitVectorsWithOperator) {
    Vector3<TypeParam> x(1, 0, 0), y(0, 1, 0);
    Vector3<TypeParam> expected(0, 0, 1);
    ASSERT_EQ(expected, x ^ y);
  }
  
  TYPED_TEST(Vector3Test, ShouldAdd) {
    Vector3<TypeParam> v1(1, 2, 1), v2(0, 1, 0), expected(1, 3, 1);
    ASSERT_EQ(expected, v1 + v2);
  }
  
  TYPED_TEST(Vector3Test, ShouldSubtract) {
    Vector3<TypeParam> v1(1, 3, 1), v2(0, 1, 0), expected(1, 2, 1);
    ASSERT_EQ(expected, v1 - v2);
  }
  
  TYPED_TEST(Vector3Test, ShouldNegate) {
    Vector3<TypeParam> v(1, 2, 3), expected(-1, -2, -3);
    ASSERT_EQ(expected, -v);
  }
  
  TYPED_TEST(Vector3Test, ShouldDivide) {
    Vector3<TypeParam> v(2, 4, 2), expected(1, 2, 1);
    ASSERT_EQ(expected, v / 2);
  }
  
  TYPED_TEST(Vector3Test, ShouldNotAllowDivisionByZero) {
    Vector3<TypeParam> vector;
    ASSERT_THROW(vector / 0, DivisionByZeroException);
  }
  
  TYPED_TEST(Vector3Test, ShouldCalculateDotProduct) {
    Vector3<TypeParam> v1(0, 2, 1), v2(1, 2, 1);
    ASSERT_EQ(5, v1 * v2);
  }
  
  TYPED_TEST(Vector3Test, ShouldMultiply) {
    Vector3<TypeParam> v(2, 1, 1), expected(4, 2, 2);
    ASSERT_EQ(expected, v * 2);
  }
  
  TYPED_TEST(Vector3Test, ShouldCompareSameVectorForEquality) {
    Vector3<TypeParam> vector;
    ASSERT_TRUE(vector == vector);
  }

  TYPED_TEST(Vector3Test, ShouldCompareVectorsForEquality) {
    Vector3<TypeParam> first(1, 2, 3), second(1, 2, 3);
    ASSERT_TRUE(first == second);
  }

  TYPED_TEST(Vector3Test, ShouldCompareVectorsForInequality) {
    Vector3<TypeParam> first(1, 2, 3);
    Vector3<TypeParam> second(4, 5, 6);

    ASSERT_TRUE(first != second);
  }
  
  TYPED_TEST(Vector3Test, ShouldAddInPlace) {
    Vector3<TypeParam> v1(1, 2, 1), v2(0, 1, 0), expected(1, 3, 1);
    v1 += v2;
    ASSERT_EQ(expected, v1);
  }
  
  TYPED_TEST(Vector3Test, ShouldSubtractInPlace) {
    Vector3<TypeParam> v1(1, 3, 1), v2(0, 1, 0), expected(1, 2, 1);
    v1 -= v2;
    ASSERT_EQ(expected, v1);
  }
  
  TYPED_TEST(Vector3Test, ShouldDivideInPlace) {
    Vector3<TypeParam> v(2, 4, 2), expected(1, 2, 1);
    v /= 2;
    ASSERT_EQ(expected, v);
  }
  
  TYPED_TEST(Vector3Test, ShouldNotAllowDivisionByZeroInPlace) {
    Vector3<TypeParam> vector;
    ASSERT_THROW(vector /= 0, DivisionByZeroException);
  }
  
  TYPED_TEST(Vector3Test, ShouldMultiplyInPlace) {
    Vector3<TypeParam> v(2, 1, 1), expected(4, 2, 2);
    v *= 2;
    ASSERT_EQ(expected, v);
  }
  
  TYPED_TEST(Vector3Test, ShouldCalculateLength) {
    ASSERT_EQ(2, Vector3<TypeParam>(2, 0, 0).length());
    ASSERT_EQ(2, Vector3<TypeParam>(0, 2, 0).length());
  }
  
  TYPED_TEST(Vector3Test, ShouldReverse) {
    Vector3<TypeParam> v(2, 0, 0), expected(-2, 0, 0);
    v.reverse();
    ASSERT_EQ(expected, v);
  }
  
  TYPED_TEST(Vector3Test, ShouldReturnReversedVector) {
    Vector3<TypeParam> v(2, 0, 0), expected(-2, 0, 0);
    ASSERT_EQ(expected, v.reversed());
  }
  
  TYPED_TEST(Vector3Test, ShouldNormalize) {
    Vector3<TypeParam> v(2, 0, 0), expected(1, 0, 0);
    v.normalize();
    ASSERT_EQ(expected, v);
  }
  
  TYPED_TEST(Vector3Test, ShouldReturnNormalizedVector) {
    Vector3<TypeParam> v(2, 0, 0), expected(1, 0, 0);
    ASSERT_EQ(expected, v.normalized());
  }
  
  TYPED_TEST(Vector3Test, ShouldReturnX) {
    auto vec = Vector3<TypeParam>(3, 4, 2);
    ASSERT_EQ(3, vec.x());
  }
  
  TYPED_TEST(Vector3Test, ShouldSetX) {
    auto vec = Vector3<TypeParam>(3, 4, 2);
    vec.setX(5);
    ASSERT_EQ(Vector3<TypeParam>(5, 4, 2), vec);
  }
  
  TYPED_TEST(Vector3Test, ShouldReturnY) {
    auto vec = Vector3<TypeParam>(3, 4, 2);
    ASSERT_EQ(4, vec.y());
  }
  
  TYPED_TEST(Vector3Test, ShouldSetY) {
    auto vec = Vector3<TypeParam>(3, 4, 2);
    vec.setY(5);
    ASSERT_EQ(Vector3<TypeParam>(3, 5, 2), vec);
  }
  
  TYPED_TEST(Vector3Test, ShouldReturnZ) {
    auto vec = Vector3<TypeParam>(3, 4, 2);
    ASSERT_EQ(2, vec.z());
  }
  
  TYPED_TEST(Vector3Test, ShouldSetZ) {
    auto vec = Vector3<TypeParam>(3, 4, 2);
    vec.setZ(5);
    ASSERT_EQ(Vector3<TypeParam>(3, 4, 5), vec);
  }
}

namespace Vector4Test {
  template<class T>
  class Vector4Test : public ::testing::Test {
  };

  typedef ::testing::Types<float, double, long double> Vector4Types;
  TYPED_TEST_CASE(Vector4Test, Vector4Types);
  
  TYPED_TEST(Vector4Test, ShouldHaveRightSize) {
    ASSERT_EQ(sizeof(Vector<4, TypeParam>), sizeof(Vector4<TypeParam>));
    ASSERT_EQ(4 * sizeof(TypeParam), sizeof(Vector4<TypeParam>));
  }

  TYPED_TEST(Vector4Test, ShouldInitializeNullVector) {
    Vector4<TypeParam> vector;
    ASSERT_EQ(0, vector.x());
    ASSERT_EQ(0, vector.y());
    ASSERT_EQ(0, vector.z());
    ASSERT_EQ(1, vector.w());
    // vector.w() is 1, so technically it's not a true null vector
    ASSERT_FALSE(vector.isNull());
  }

  TYPED_TEST(Vector4Test, ShouldInitializeCoordinates) {
    Vector4<TypeParam> vector(1, 2, 3, 4);
    ASSERT_EQ(1, vector.x());
    ASSERT_EQ(2, vector.y());
    ASSERT_EQ(3, vector.z());
    ASSERT_EQ(4, vector.w());
  }

  TYPED_TEST(Vector4Test, ShouldInitializeWCoordinateAsOneByDefault) {
    Vector4<TypeParam> vector(1, 2, 3);
    ASSERT_EQ(1, vector.w());
  }

  TYPED_TEST(Vector4Test, ShouldInitializeWCoordinateAsOneByDefaultInDefaultConstructor) {
    Vector4<TypeParam> vector;
    ASSERT_EQ(1, vector.w());
  }

  TYPED_TEST(Vector4Test, ShouldCopyFromVector3) {
    Vector3<TypeParam> source(1, 2, 3);
    Vector4<TypeParam> dest = source;
    ASSERT_EQ(1, dest.x());
    ASSERT_EQ(2, dest.y());
    ASSERT_EQ(3, dest.z());
    ASSERT_EQ(1, dest.w());
  }

  TYPED_TEST(Vector4Test, ShouldPreserveWCoordinateWhenCopyingFromVector4) {
    Vector4<TypeParam> source(1, 2, 3, 5);
    Vector4<TypeParam> dest = source;
    ASSERT_EQ(1, dest.x());
    ASSERT_EQ(2, dest.y());
    ASSERT_EQ(3, dest.z());
    ASSERT_EQ(5, dest.w());
  }

  TYPED_TEST(Vector4Test, ShouldDefineNullVector) {
    Vector4<TypeParam> expected(0, 0, 0, 1);
    ASSERT_EQ(expected, Vector4<TypeParam>::null());
    // The w component is 1, so it's not a true null vector
    ASSERT_FALSE(Vector4<TypeParam>::null().isNull());
  }

  TYPED_TEST(Vector4Test, ShouldDefineEpsilonVector) {
    ASSERT_TRUE(Vector4<TypeParam>::epsilon().x() > 0.0);
    ASSERT_TRUE(Vector4<TypeParam>::epsilon().y() > 0.0);
    ASSERT_TRUE(Vector4<TypeParam>::epsilon().z() > 0.0);
    ASSERT_TRUE(Vector4<TypeParam>::epsilon().w() > 0.0);
  }
  
  TYPED_TEST(Vector4Test, ShouldDefineUndefinedVector) {
    ASSERT_TRUE(Vector4<TypeParam>::undefined().isUndefined());
  }
  
  TYPED_TEST(Vector4Test, ShouldDefineMinusInfinityVector) {
    ASSERT_EQ(- numeric_limits<TypeParam>::infinity(), Vector4<TypeParam>::minusInfinity().x());
  }
  
  TYPED_TEST(Vector4Test, ShouldDefinePlusInfinityVector) {
    ASSERT_EQ(numeric_limits<TypeParam>::infinity(), Vector4<TypeParam>::plusInfinity().x());
  }
  
  TYPED_TEST(Vector4Test, ShouldReturnHomogenizedVector) {
    Vector4<TypeParam> vector(2, 4, 6, 2);
    Vector3<TypeParam> expected(1, 2, 3);
    
    ASSERT_EQ(expected, vector.homogenized());
  }
  
  TYPED_TEST(Vector4Test, ShouldAdd) {
    Vector4<TypeParam> v1(1, 2, 1, 1), v2(0, 1, 0, 0), expected(1, 3, 1, 1);
    ASSERT_EQ(expected, v1 + v2);
  }

  TYPED_TEST(Vector4Test, ShouldSubtract) {
    Vector4<TypeParam> v1(1, 3, 1, 2), v2(0, 1, 0, 1), expected(1, 2, 1, 1);
    ASSERT_EQ(expected, v1 - v2);
  }

  TYPED_TEST(Vector4Test, ShouldNegate) {
    Vector4<TypeParam> v(1, 2, 3, 1), expected(-1, -2, -3, -1);
    ASSERT_EQ(expected, -v);
  }

  TYPED_TEST(Vector4Test, ShouldDivide) {
    Vector4<TypeParam> v(2, 4, 2, 4), expected(1, 2, 1, 2);
    ASSERT_EQ(expected, v / 2);
  }

  TYPED_TEST(Vector4Test, ShouldNotAllowDivisionByZero) {
    Vector4<TypeParam> vector;
    ASSERT_THROW(vector / 0, DivisionByZeroException);
  }

  TYPED_TEST(Vector4Test, ShouldCalculateDotProduct) {
    Vector4<TypeParam> v1(0, 2, 1, 1), v2(1, 2, 1, 1);
    ASSERT_EQ(6, v1 * v2);
  }

  TYPED_TEST(Vector4Test, ShouldMultiply) {
    Vector4<TypeParam> v(2, 1, 1, 2), expected(4, 2, 2, 4);
    ASSERT_EQ(expected, v * 2);
  }

  TYPED_TEST(Vector4Test, ShouldCompareSameVectorForEquality) {
    Vector4<TypeParam> vector;
    ASSERT_TRUE(vector == vector);
  }

  TYPED_TEST(Vector4Test, ShouldCompareVectorsForEquality) {
    Vector4<TypeParam> first(1, 2, 3, 1), second(1, 2, 3, 1);
    ASSERT_TRUE(first == second);
  }

  TYPED_TEST(Vector4Test, ShouldCompareVectorsForInequality) {
    Vector4<TypeParam> first(1, 2, 3, 4);
    Vector4<TypeParam> second(4, 5, 6, 7);

    ASSERT_TRUE(first != second);
  }

  TYPED_TEST(Vector4Test, ShouldAddInPlace) {
    Vector4<TypeParam> v1(1, 2, 1, 1), v2(0, 1, 0, 0), expected(1, 3, 1, 1);
    v1 += v2;
    ASSERT_EQ(expected, v1);
  }

  TYPED_TEST(Vector4Test, ShouldSubtractInPlace) {
    Vector4<TypeParam> v1(1, 3, 1, 2), v2(0, 1, 0, 1), expected(1, 2, 1, 1);
    v1 -= v2;
    ASSERT_EQ(expected, v1);
  }

  TYPED_TEST(Vector4Test, ShouldDivideInPlace) {
    Vector4<TypeParam> v(2, 4, 2, 4), expected(1, 2, 1, 2);
    v /= 2;
    ASSERT_EQ(expected, v);
  }

  TYPED_TEST(Vector4Test, ShouldNotAllowDivisionByZeroInPlace) {
    Vector4<TypeParam> vector;
    ASSERT_THROW(vector /= 0, DivisionByZeroException);
  }

  TYPED_TEST(Vector4Test, ShouldMultiplyInPlace) {
    Vector4<TypeParam> v(2, 1, 1, 2), expected(4, 2, 2, 4);
    v *= 2;
    ASSERT_EQ(expected, v);
  }
  
  TYPED_TEST(Vector4Test, ShouldCalculateLength) {
    ASSERT_EQ(2, Vector4<TypeParam>(2, 0, 0, 0).length());
    ASSERT_EQ(2, Vector4<TypeParam>(0, 2, 0, 0).length());
  }
  
  TYPED_TEST(Vector4Test, ShouldReverse) {
    Vector4<TypeParam> v(2, 0, 0, 0), expected(-2, 0, 0, 0);
    v.reverse();
    ASSERT_EQ(expected, v);
  }
  
  TYPED_TEST(Vector4Test, ShouldReturnReversedVector) {
    Vector4<TypeParam> v(2, 0, 0, 0), expected(-2, 0, 0, 0);
    ASSERT_EQ(expected, v.reversed());
  }
  
  TYPED_TEST(Vector4Test, ShouldNormalize) {
    Vector4<TypeParam> v(2, 0, 0, 0), expected(1, 0, 0, 0);
    v.normalize();
    ASSERT_EQ(expected, v);
  }
  
  TYPED_TEST(Vector4Test, ShouldReturnNormalizedVector) {
    Vector4<TypeParam> v(2, 0, 0, 0), expected(1, 0, 0, 0);
    ASSERT_EQ(expected, v.normalized());
  }
  
  TYPED_TEST(Vector4Test, ShouldReturnX) {
    auto vec = Vector4<TypeParam>(3, 4, 2, 1);
    ASSERT_EQ(3, vec.x());
  }
  
  TYPED_TEST(Vector4Test, ShouldSetX) {
    auto vec = Vector4<TypeParam>(3, 4, 2, 1);
    vec.setX(5);
    ASSERT_EQ(Vector4<TypeParam>(5, 4, 2, 1), vec);
  }
  
  TYPED_TEST(Vector4Test, ShouldReturnY) {
    auto vec = Vector4<TypeParam>(3, 4, 2, 1);
    ASSERT_EQ(4, vec.y());
  }
  
  TYPED_TEST(Vector4Test, ShouldSetY) {
    auto vec = Vector4<TypeParam>(3, 4, 2, 1);
    vec.setY(5);
    ASSERT_EQ(Vector4<TypeParam>(3, 5, 2, 1), vec);
  }
  
  TYPED_TEST(Vector4Test, ShouldReturnZ) {
    auto vec = Vector4<TypeParam>(3, 4, 2, 1);
    ASSERT_EQ(2, vec.z());
  }
  
  TYPED_TEST(Vector4Test, ShouldSetZ) {
    auto vec = Vector4<TypeParam>(3, 4, 2, 1);
    vec.setZ(5);
    ASSERT_EQ(Vector4<TypeParam>(3, 4, 5, 1), vec);
  }
  
  TYPED_TEST(Vector4Test, ShouldReturnW) {
    auto vec = Vector4<TypeParam>(3, 4, 2, 1);
    ASSERT_EQ(1, vec.w());
  }
  
  TYPED_TEST(Vector4Test, ShouldSetW) {
    auto vec = Vector4<TypeParam>(3, 4, 2, 1);
    vec.setW(5);
    ASSERT_EQ(Vector4<TypeParam>(3, 4, 2, 5), vec);
  }
}
