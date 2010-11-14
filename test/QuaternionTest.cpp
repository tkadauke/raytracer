#include "gtest.h"
#include "Quaternion.h"
#include "Vector.h"

namespace QuaternionTest {
  TEST(Quaternion, ShouldInitializeAsMultiplicationIdentityQuaternion) {
    Quaternion<float> quaternion;
    ASSERT_EQ(1, quaternion.w());
    ASSERT_EQ(0, quaternion.x());
    ASSERT_EQ(0, quaternion.y());
    ASSERT_EQ(0, quaternion.z());
  }
  
  TEST(Quaternion, ShouldInitializeFromFourScalars) {
    Quaternion<float> quaternion(4, 1, 2, 3);
    ASSERT_EQ(4, quaternion.w());
    ASSERT_EQ(1, quaternion.x());
    ASSERT_EQ(2, quaternion.y());
    ASSERT_EQ(3, quaternion.z());
  }
  
  TEST(Quaternion, ShouldInitializeFromScalarAndVector) {
    Vector3<float> vector(1, 2, 3);
    Quaternion<float> quaternion(4, vector);
    ASSERT_EQ(4, quaternion.w());
    ASSERT_EQ(1, quaternion.x());
    ASSERT_EQ(2, quaternion.y());
    ASSERT_EQ(3, quaternion.z());
  }
  
  TEST(Quaternion, ShouldInitializeFromScalarAndCArray) {
    float elements[3] = { 1, 2, 3 };
    Quaternion<float> quaternion(4, elements);
    ASSERT_EQ(4, quaternion.w());
    ASSERT_EQ(1, quaternion.x());
    ASSERT_EQ(2, quaternion.y());
    ASSERT_EQ(3, quaternion.z());
  }
  
  TEST(Quaternion, ShouldReturnTrueForEqualityTestOfTwoIdentityQuaternions) {
    ASSERT_TRUE(Quaternion<float>() == Quaternion<float>());
  }
  
  TEST(Quaternion, ShouldTestQuaternionsForEquality) {
    Quaternion<float> first(1, 2, 2, 3), second(1, 2, 2, 3);
    ASSERT_TRUE(first == second);
  }
  
  TEST(Quaternion, ShouldTestQuaternionsForInEquality) {
    Quaternion<float> first(1, 2, 2, 3), second(1, 4, 4, 6);
    ASSERT_TRUE(first != second);
  }
  
  TEST(Quaternion, ShouldMultiplyQuaternionWithScalar) {
    Quaternion<float> quaternion(1, 2, 2, 3);
    Quaternion<float> expected(2, 4, 4, 6);
    ASSERT_EQ(expected, quaternion * 2);
  }
  
  TEST(Quaternion, ShouldDivideQuaternionByScalar) {
    Quaternion<float> quaternion(2, 4, 4, 6);
    Quaternion<float> expected(1, 2, 2, 3);
    ASSERT_EQ(expected, quaternion / 2);
  }
  
  TEST(Quaternion, ShouldCalculateLengthOfUnitQuaternion) {
    Quaternion<float> quaternion;
    ASSERT_EQ(1, quaternion.length());
  }
  
  TEST(Quaternion, ShouldCalculateLengthOfQuaternion) {
    Quaternion<float> quaternion(2, 2, 2, 2);
    ASSERT_EQ(4, quaternion.length());
  }
  
  TEST(Quaternion, ShouldReturnNormalizedQuaternion) {
    Quaternion<float> quaternion(2, 5, 4, 2);
    ASSERT_EQ(1, quaternion.normalized().length());
  }
  
  TEST(Quaternion, ShouldReturnOriginalQuaternionWhenMultipliedWithIdentityQuaternion) {
    Quaternion<float> quaternion(4, 1, 2, 3);
    Quaternion<float> identity;
    
    ASSERT_EQ(quaternion, quaternion * identity);
  }
  
  TEST(Quaternion, ShouldMultiplyTwoQuaternions) {
    
  }
}
