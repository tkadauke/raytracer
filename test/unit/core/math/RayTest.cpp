#include "gtest.h"
#include "core/math/Ray.h"
#include "test/helpers/VectorTestHelper.h"

namespace RayTest {
  using namespace ::testing;
  
  template<class T>
  class RayTest : public ::testing::Test {
  };

  typedef ::testing::Types<float, double> RayTypes;

  TYPED_TEST_CASE(RayTest, RayTypes);

  TYPED_TEST(RayTest, ShouldInitializeWithValues) {
    Ray<TypeParam> ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    ASSERT_EQ(Vector3d(0, 1, 0), ray.origin());
    ASSERT_EQ(Vector3d(1, 0, 0), ray.direction());
  }
  
  TYPED_TEST(RayTest, ShouldReturnEpsilonShiftedRay) {
    Ray<TypeParam> ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    Ray<TypeParam> shifted = ray.epsilonShifted();
    ASSERT_VECTOR_NEAR(ray.origin(), shifted.origin(), Ray<TypeParam>::epsilon);
    ASSERT_EQ(ray.direction(), shifted.direction());
  }
  
  TYPED_TEST(RayTest, ShouldReturnNewRayWithDifferentOrigin) {
    Ray<TypeParam> ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    Ray<TypeParam> shifted = ray.from(Vector3d(2, 0, 0));
    ASSERT_EQ(Vector3d(2, 0, 0), shifted.origin());
    ASSERT_EQ(ray.direction(), shifted.direction());
  }
  
  TYPED_TEST(RayTest, ShouldReturnNewRayWithDifferentDirection) {
    Ray<TypeParam> ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    Ray<TypeParam> rotated = ray.to(Vector3d(0, 0, 1));
    ASSERT_EQ(Vector3d(0, 0, 1), rotated.direction());
    ASSERT_EQ(ray.origin(), rotated.origin());
  }
  
  TYPED_TEST(RayTest, ShouldReturnPointAtDistance) {
    Ray<TypeParam> ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    ASSERT_EQ(Vector3d(1, 1, 0), ray.at(1));
  }
}
