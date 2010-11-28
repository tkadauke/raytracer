#include "gtest.h"
#include "Ray.h"
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
  
  TEST(Ray, ShouldReturnPointAtDistance) {
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    ASSERT_EQ(Vector3d(1, 1, 0), ray.at(1));
  }
}
