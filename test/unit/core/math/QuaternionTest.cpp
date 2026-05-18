#include <gtest/gtest.h>
#include "core/math/Quaternion.h"
#include "core/math/Vector.h"

#include <functional>
#include <sstream>
#include <unordered_set>

#if __cplusplus >= 202002L && __has_include(<format>)
#include <format>
#endif

namespace QuaternionTest {
  using namespace std;

  template<class T>
  class QuaternionTest : public ::testing::Test {
  };

  typedef ::testing::Types<float, double> QuaternionTypes;

  TYPED_TEST_SUITE(QuaternionTest, QuaternionTypes);
  
  TYPED_TEST(QuaternionTest, ShouldInitializeAsMultiplicationIdentityQuaternion) {
    Quaternion<TypeParam> quaternion;
    ASSERT_EQ(1, quaternion.w());
    ASSERT_EQ(0, quaternion.x());
    ASSERT_EQ(0, quaternion.y());
    ASSERT_EQ(0, quaternion.z());
  }
  
  TYPED_TEST(QuaternionTest, ShouldInitializeFromFourScalars) {
    Quaternion<TypeParam> quaternion(4, 1, 2, 3);
    ASSERT_EQ(4, quaternion.w());
    ASSERT_EQ(1, quaternion.x());
    ASSERT_EQ(2, quaternion.y());
    ASSERT_EQ(3, quaternion.z());
  }
  
  TYPED_TEST(QuaternionTest, ShouldInitializeFromScalarAndVector) {
    Vector3<TypeParam> vector(1, 2, 3);
    Quaternion<TypeParam> quaternion(4, vector);
    ASSERT_EQ(4, quaternion.w());
    ASSERT_EQ(1, quaternion.x());
    ASSERT_EQ(2, quaternion.y());
    ASSERT_EQ(3, quaternion.z());
  }
  
  TYPED_TEST(QuaternionTest, ShouldInitializeFromScalarAndCArray) {
    TypeParam elements[3] = { 1, 2, 3 };
    Quaternion<TypeParam> quaternion(4, elements);
    ASSERT_EQ(4, quaternion.w());
    ASSERT_EQ(1, quaternion.x());
    ASSERT_EQ(2, quaternion.y());
    ASSERT_EQ(3, quaternion.z());
  }
  
  TYPED_TEST(QuaternionTest, ShouldReturnTrueForEqualityTestOfTwoIdentityQuaternions) {
    ASSERT_TRUE(Quaternion<TypeParam>() == Quaternion<TypeParam>());
  }
  
  TYPED_TEST(QuaternionTest, ShouldTestQuaternionsForEquality) {
    Quaternion<TypeParam> first(1, 2, 2, 3), second(1, 2, 2, 3);
    ASSERT_TRUE(first == second);
  }
  
  TYPED_TEST(QuaternionTest, ShouldTestQuaternionsForInEquality) {
    Quaternion<TypeParam> first(1, 2, 2, 3), second(1, 4, 4, 6);
    ASSERT_TRUE(first != second);
  }
  
  TYPED_TEST(QuaternionTest, ShouldMultiplyQuaternionWithScalar) {
    Quaternion<TypeParam> quaternion(1, 2, 2, 3);
    Quaternion<TypeParam> expected(2, 4, 4, 6);
    ASSERT_EQ(expected, quaternion * 2);
  }
  
  TYPED_TEST(QuaternionTest, ShouldDivideQuaternionByScalar) {
    Quaternion<TypeParam> quaternion(2, 4, 4, 6);
    Quaternion<TypeParam> expected(1, 2, 2, 3);
    ASSERT_EQ(expected, quaternion / 2);
  }
  
  TYPED_TEST(QuaternionTest, ShouldCalculateLengthOfUnitQuaternion) {
    Quaternion<TypeParam> quaternion;
    ASSERT_EQ(1, quaternion.length());
  }
  
  TYPED_TEST(QuaternionTest, ShouldCalculateLengthOfQuaternion) {
    Quaternion<TypeParam> quaternion(2, 2, 2, 2);
    ASSERT_EQ(4, quaternion.length());
  }
  
  TYPED_TEST(QuaternionTest, ShouldReturnNormalizedQuaternion) {
    Quaternion<TypeParam> quaternion(2, 5, 4, 2);
    ASSERT_NEAR(1, quaternion.normalized().length(), 0.001);
  }
  
  TYPED_TEST(QuaternionTest, ShouldReturnOriginalQuaternionWhenMultipliedWithIdentityQuaternion) {
    Quaternion<TypeParam> quaternion(4, 1, 2, 3);
    Quaternion<TypeParam> identity;
    
    ASSERT_EQ(quaternion, quaternion * identity);
  }
  
  TYPED_TEST(QuaternionTest, ShouldMultiplyTwoQuaternions) {
    Quaternion<TypeParam> first(1, 2, 2, 3), second(1, 2, 2, 3);
    Quaternion<TypeParam> expected(-16, 4, 4, 6);
    ASSERT_EQ(expected, first * second);
  }

  TYPED_TEST(QuaternionTest, ShouldStreamQuaternionToString) {
    Quaternion<TypeParam> quaternion(4, 1, 2, 3);

    ostringstream str;
    str << quaternion;

    ASSERT_EQ("[4, 1 2 3]", str.str());
  }

  TYPED_TEST(QuaternionTest, HashOfEqualQuaternionsIsEqual) {
    hash<Quaternion<TypeParam>> h;
    EXPECT_EQ(h(Quaternion<TypeParam>(1, 2, 3, 4)), h(Quaternion<TypeParam>(1, 2, 3, 4)));
  }

  TYPED_TEST(QuaternionTest, HashOfDistinctQuaternionsDiffers) {
    hash<Quaternion<TypeParam>> h;
    EXPECT_NE(h(Quaternion<TypeParam>(1, 0, 0, 0)), h(Quaternion<TypeParam>(0, 1, 0, 0)));
    EXPECT_NE(h(Quaternion<TypeParam>(1, 0, 0, 0)), h(Quaternion<TypeParam>(0, 0, 1, 0)));
    EXPECT_NE(h(Quaternion<TypeParam>(1, 0, 0, 0)), h(Quaternion<TypeParam>(0, 0, 0, 1)));
  }

  TYPED_TEST(QuaternionTest, WorksAsUnorderedSetElement) {
    unordered_set<Quaternion<TypeParam>> set;
    set.insert(Quaternion<TypeParam>(1, 0, 0, 0));
    set.insert(Quaternion<TypeParam>(0, 1, 0, 0));
    set.insert(Quaternion<TypeParam>(0, 0, 1, 0));
    set.insert(Quaternion<TypeParam>(1, 0, 0, 0));
    EXPECT_EQ(set.size(), 3u);
  }

#ifdef __cpp_lib_format
  TYPED_TEST(QuaternionTest, FormatsIdentityQuaternion) {
    Quaternion<TypeParam> q;
    EXPECT_EQ(format("{}", q), "[1, 0 0 0]");
  }

  TYPED_TEST(QuaternionTest, FormatsArbitraryQuaternion) {
    Quaternion<TypeParam> q(4, 1, 2, 3);
    EXPECT_EQ(format("{}", q), "[4, 1 2 3]");
  }
#endif
}
