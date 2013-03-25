#include "gtest.h"
#include "raytracer/Light.h"

namespace LightTest {
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
