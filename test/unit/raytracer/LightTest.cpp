#include "gtest.h"
#include "raytracer/lights/Light.h"

namespace LightTest {
  using namespace raytracer;

  TEST(Light, ShouldInitializeWithValues) {
    Light light(Vector3d(1, 0, 0), Colord::white());
  }
  
  TEST(Light, ShouldReturnPosition) {
    Light light(Vector3d(1, 0, 0), Colord::white());
    ASSERT_EQ(Vector3d(1, 0, 0), light.position());
  }
  
  TEST(Light, ShouldReturnColor) {
    Light light(Vector3d(1, 0, 0), Colord::white());
    ASSERT_EQ(Colord::white(), light.color());
  }
}
