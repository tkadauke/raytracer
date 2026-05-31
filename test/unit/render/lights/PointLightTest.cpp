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

  TEST(PointLight, ShouldReturnDeltaSampleTowardPosition) {
    PointLight light(Vector3d(1, 2, 3), Colord(0.25, 0.5, 0.75));
    const Vector3d point(4, 6, 3);

    const LightSample sample = light.sample(point);

    EXPECT_EQ((light.position() - point).normalized(), sample.direction);
    EXPECT_EQ(light.radiance(), sample.radiance);
    EXPECT_DOUBLE_EQ(5.0, sample.distance);
    EXPECT_DOUBLE_EQ(1.0, sample.pdf);
    EXPECT_TRUE(sample.delta);
  }

  TEST(PointLight, ShouldExposeDeltaPdfBehavior) {
    PointLight light(Vector3d(1, 2, 3), Colord::white());

    EXPECT_TRUE(light.isDelta());
    EXPECT_DOUBLE_EQ(0.0, light.pdf(Vector3d::null, light.direction(Vector3d::null)));
  }

  TEST(PointLight, ShouldExposeEmissionAndBoundedPowerMetadata) {
    const Colord color(0.25, 0.5, 0.75);
    PointLight light(Vector3d(1, 2, 3), color);

    EXPECT_EQ(color, light.emission());
    ASSERT_TRUE(light.power().has_value());
    EXPECT_EQ(color, *light.power());
  }

  TEST(PointLight, ShouldNotProvideDirectionalShadowMapDirection) {
    PointLight light(Vector3d(1, 0, 0), Colord::white());
    ASSERT_FALSE(light.directionalShadowMapDirection().has_value());
  }

  TEST(PointLight, ShouldProvidePositionalLightPosition) {
    PointLight light(Vector3d(1, 2, 3), Colord::white());

    ASSERT_TRUE(light.positionalLightPosition().has_value());
    EXPECT_EQ(Vector3d(1, 2, 3), *light.positionalLightPosition());
  }
}
