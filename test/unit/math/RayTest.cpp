#include "gtest.h"
#include "math/Ray.h"
#include "test/helpers/VectorTestHelper.h"

namespace RayTest {
  TEST(Ray, ShouldInitializeWithValues) {
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    ASSERT_EQ(Vector3d(0, 1, 0), ray.origin());
    ASSERT_EQ(Vector3d(1, 0, 0), ray.direction());
  }
  
  TEST(Ray, ShouldReturnEpsilonShiftedRay) {
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    Ray shifted = ray.epsilonShifted();
    ASSERT_VECTOR_NEAR(ray.origin(), shifted.origin(), Ray::epsilon);
    ASSERT_EQ(ray.direction(), shifted.direction());
  }
  
  TEST(Ray, ShouldReturnNewRayWithDifferentOrigin) {
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    Ray shifted = ray.from(Vector3d(2, 0, 0));
    ASSERT_EQ(Vector3d(2, 0, 0), shifted.origin());
    ASSERT_EQ(ray.direction(), shifted.direction());
  }
  
  TEST(Ray, ShouldReturnNewRayWithDifferentDirection) {
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    Ray rotated = ray.to(Vector3d(0, 0, 1));
    ASSERT_EQ(Vector3d(0, 0, 1), rotated.direction());
    ASSERT_EQ(ray.origin(), rotated.origin());
  }
  
  TEST(Ray, ShouldReturnPointAtDistance) {
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    ASSERT_EQ(Vector3d(1, 1, 0), ray.at(1));
  }
}
