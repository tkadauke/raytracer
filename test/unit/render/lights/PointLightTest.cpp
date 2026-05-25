#include <gtest/gtest.h>
#include "render/lights/PointLight.h"

namespace PointLightTest {
  using namespace render;
  using namespace render;

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

  TEST(PointLight, ShouldNotProvideDirectionalShadowMapDirection) {
    PointLight light(Vector3d(1, 0, 0), Colord::white());
    ASSERT_FALSE(light.directionalShadowMapDirection().has_value());
  }
}
