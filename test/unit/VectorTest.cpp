#include "gtest.h"
#include "Vector.h"
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

  TEST(Vector, ShouldDivideVectorByScalar) {
    Vector<3, float> vector;
    vector[0] = 10.0;

    Vector<3, float> shrunk = vector / 2;

    ASSERT_EQ(5.0, shrunk[0]);
  }

  TEST(Vector, ShouldNotAllowDivisionByZero) {
    Vector<3, float> vector;
    ASSERT_THROW(vector / 0, DivisionByZeroException);
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

  TEST(Vector, ShouldGetNormalizedVector) {
    Vector<3, float> vector;
    vector[0] = 10;

    Vector<3, float> normalized = vector.normalized();
    ASSERT_EQ(1, normalized[0]);
    ASSERT_EQ(0, normalized[1]);
    ASSERT_EQ(0, normalized[2]);
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
  
  TEST(Vector, ShouldReturnTrueIfVectorIsUndefined) {
    float elements[3] = { numeric_limits<float>::quiet_NaN(), 0, 0 };
    Vector<3, float> vector(elements);
    ASSERT_TRUE(vector.isUndefined());
  }
  
  TEST(Vector, ShouldReturnFalseIfVectorIsDefined) {
    float elements[3] = { 0, 0, 0 };
    Vector<3, float> vector(elements);
    ASSERT_FALSE(vector.isUndefined());
  }
}

namespace SpecializedVectorTest {
  template<class T>
  class SpecializedVectorTest : public ::testing::Test {
  };

  typedef ::testing::Types<Vector2<float>, Vector3<float>, Vector4<float>,
                           Vector2<double>, Vector3<double>, Vector4<double> > SpecializedVectorTypes;
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
    TypeParam vector = TypeParam::undefined;
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
    ASSERT_EQ(expected, Vector2<float>::null);
  }
}

namespace Vector3Test {
  TEST(Vector3, ShouldInitializeNullVector) {
    Vector3<float> vector;
    ASSERT_EQ(0, vector.x());
    ASSERT_EQ(0, vector.y());
    ASSERT_EQ(0, vector.z());
  }

  TEST(Vector3, ShouldInitializeCoordinates) {
    Vector3<float> vector(1, 2, 3);
    ASSERT_EQ(1, vector.x());
    ASSERT_EQ(2, vector.y());
    ASSERT_EQ(3, vector.z());
  }

  TEST(Vector3, ShouldInitializeZCoordinateAsZeroByDefault) {
    Vector3<float> vector(1, 2);
    ASSERT_EQ(0, vector.z());
  }

  TEST(Vector3, ShouldCopyFromVector2) {
    Vector2<float> source(1, 2);
    Vector3<float> dest = source;
    ASSERT_EQ(1, dest.x());
    ASSERT_EQ(2, dest.y());
    ASSERT_EQ(0, dest.z());
  }

  TEST(Vector3, ShouldCopyFromVector4) {
    Vector4<float> source(1, 2, 3);
    Vector3<float> dest = source;
    ASSERT_EQ(1, dest.x());
    ASSERT_EQ(2, dest.y());
    ASSERT_EQ(3, dest.z());
  }

  TEST(Vector3, ShouldDefineNullVector) {
    Vector3<float> expected(0, 0, 0);
    ASSERT_EQ(expected, Vector3<float>::null);
  }
  
  TEST(Vector3, ShouldCalculateCrossProductOfUnitVectors) {
    Vector3<float> x(1, 0, 0), y(0, 1, 0);
    Vector3<float> expected(0, 0, 1);
    ASSERT_EQ(expected, x ^ y);
  }
}

namespace Vector4Test {
  TEST(Vector4, ShouldInitializeNullVector) {
    Vector4<float> vector;
    ASSERT_EQ(0, vector.x());
    ASSERT_EQ(0, vector.y());
    ASSERT_EQ(0, vector.z());
    ASSERT_EQ(1, vector.w());
  }

  TEST(Vector4, ShouldInitializeCoordinates) {
    Vector4<float> vector(1, 2, 3, 4);
    ASSERT_EQ(1, vector.x());
    ASSERT_EQ(2, vector.y());
    ASSERT_EQ(3, vector.z());
    ASSERT_EQ(4, vector.w());
  }

  TEST(Vector4, ShouldInitializeWCoordinateAsOneByDefault) {
    Vector4<float> vector(1, 2, 3);
    ASSERT_EQ(1, vector.w());
  }

  TEST(Vector4, ShouldInitializeWCoordinateAsOneByDefaultInDefaultConstructor) {
    Vector4<float> vector;
    ASSERT_EQ(1, vector.w());
  }

  TEST(Vector4, ShouldCopyFromVector3) {
    Vector3<float> source(1, 2, 3);
    Vector4<float> dest = source;
    ASSERT_EQ(1, dest.x());
    ASSERT_EQ(2, dest.y());
    ASSERT_EQ(3, dest.z());
    ASSERT_EQ(1, dest.w());
  }

  TEST(Vector4, ShouldPreserveWCoordinateWhenCopyingFromVector4) {
    Vector4<float> source(1, 2, 3, 5);
    Vector4<float> dest = source;
    ASSERT_EQ(1, dest.x());
    ASSERT_EQ(2, dest.y());
    ASSERT_EQ(3, dest.z());
    ASSERT_EQ(5, dest.w());
  }

  TEST(Vector4, ShouldDefineNullVector) {
    Vector4<float> expected(0, 0, 0, 1);
    ASSERT_EQ(expected, Vector4<float>::null);
  }
  
  TEST(Vector4, ShouldReturnHomogenizedVector) {
    Vector4<float> vector(2, 4, 6, 2);
    Vector3<float> expected(1, 2, 3);
    
    ASSERT_EQ(expected, vector.homogenized());
  }
}
