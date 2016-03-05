#include "gtest.h"
#include "raytracer/lights/PointLight.h"

namespace PointLightTest {
  using namespace raytracer;

  TEST(PointLight, ShouldInitializeWithValues) {
    PointLight light(Vector3d(1, 0, 0), Colord::white());
  }
  
  TEST(PointLight, ShouldReturnPosition) {
    PointLight light(Vector3d(1, 0, 0), Colord::white());
    ASSERT_EQ(Vector3d(1, 0, 0), light.position());
  }
  
  TEST(PointLight, ShouldReturnColor) {
    PointLight light(Vector3d(1, 0, 0), Colord::white());
    ASSERT_EQ(Colord::white(), light.color());
  }
  
  TEST(PointLight, ShouldReturnDirection) {
    PointLight light(Vector3d(1, 0, 0), Colord::white());
    Vector3d point(2, 5, 3);
    Vector3d expected = (light.position() - point).normalized();
    ASSERT_EQ(expected, light.direction(point));
  }
  
  TEST(PointLight, ShouldReturnRadiance) {
    PointLight light(Vector3d(1, 0, 0), Colord::white());
    ASSERT_EQ(Colord::white(), light.radiance());
  }
}
