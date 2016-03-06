#include "gtest.h"
#include "raytracer/lights/DirectionalLight.h"

namespace DirectionalLightTest {
  using namespace raytracer;

  TEST(DirectionalLight, ShouldInitializeWithValues) {
    DirectionalLight light(Vector3d(-0.5, -1, -0.5), Colord::white());
  }
  
  TEST(DirectionalLight, ShouldReturnDirection) {
    Vector3d dir = Vector3d(-0.5, -1, -0.5).normalized();
    
    DirectionalLight light(dir, Colord::white());
    ASSERT_EQ(dir, light.direction());
  }
  
  TEST(DirectionalLight, ShouldReturnColor) {
    DirectionalLight light(Vector3d(-0.5, -1, -0.5), Colord::white());
    ASSERT_EQ(Colord::white(), light.color());
  }
  
  TEST(DirectionalLight, ShouldReturnConstantDirection) {
    Vector3d dir = Vector3d(-0.5, -1, -0.5).normalized();

    DirectionalLight light(dir, Colord::white());
    ASSERT_EQ(dir, light.direction(Vector3d::undefined()));
  }
  
  TEST(DirectionalLight, ShouldReturnRadiance) {
    DirectionalLight light(Vector3d(-0.5, -1, -0.5), Colord::white());
    ASSERT_EQ(Colord::white(), light.radiance());
  }
}
