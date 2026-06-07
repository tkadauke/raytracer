#include <gtest/gtest.h>
#include "render/lights/DirectionalLight.h"

#include <cmath>

namespace DirectionalLightTest {
  using namespace render;
  using namespace render;

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
    ASSERT_EQ(dir, light.direction(Vector3d::undefined));
  }

  TEST(DirectionalLight, ShouldReturnRadiance) {
    DirectionalLight light(Vector3d(-0.5, -1, -0.5), Colord::white());
    ASSERT_EQ(Colord::white(), light.radiance());
  }

  TEST(DirectionalLight, ShouldReturnDeltaSampleAlongConstantDirection) {
    const Vector3d dir = Vector3d(-0.5, -1, -0.5).normalized();
    DirectionalLight light(dir, Colord(0.25, 0.5, 0.75));

    const LightSample sample = light.sample(Vector3d(2, 3, 4));

    EXPECT_EQ(dir, sample.direction);
    EXPECT_EQ(light.radiance(), sample.radiance);
    EXPECT_TRUE(std::isinf(sample.distance));
    EXPECT_DOUBLE_EQ(1.0, sample.pdf);
    EXPECT_TRUE(sample.delta);
  }

  TEST(DirectionalLight, ShouldIgnoreCallerSampleForDeltaSample) {
    const Vector3d dir = Vector3d(-0.5, -1, -0.5).normalized();
    DirectionalLight light(dir, Colord(0.25, 0.5, 0.75));

    const LightSample implicitSample = light.sample(Vector3d(2, 3, 4));
    const LightSample explicitSample = light.sample(Vector3d(2, 3, 4), Vector2d(0.125, 0.875));

    EXPECT_EQ(implicitSample.direction, explicitSample.direction);
    EXPECT_EQ(implicitSample.radiance, explicitSample.radiance);
    EXPECT_DOUBLE_EQ(implicitSample.distance, explicitSample.distance);
    EXPECT_DOUBLE_EQ(implicitSample.pdf, explicitSample.pdf);
    EXPECT_EQ(implicitSample.delta, explicitSample.delta);
  }

  TEST(DirectionalLight, ShouldExposeDeltaPdfBehavior) {
    const Vector3d dir = Vector3d(-0.5, -1, -0.5).normalized();
    DirectionalLight light(dir, Colord::white());

    EXPECT_TRUE(light.isDelta());
    EXPECT_DOUBLE_EQ(0.0, light.pdf(Vector3d::null, dir));
  }

  TEST(DirectionalLight, ShouldExposeEmissionAndUnboundedPowerMetadata) {
    const Colord color(0.25, 0.5, 0.75);
    DirectionalLight light(Vector3d(-0.5, -1, -0.5), color);

    EXPECT_EQ(color, light.emission());
    EXPECT_FALSE(light.power().has_value());
  }

  TEST(DirectionalLight, ShouldProvideDirectionalShadowMapDirection) {
    const Vector3d dir = Vector3d(-0.5, -1, -0.5).normalized();
    DirectionalLight light(dir, Colord::white());

    ASSERT_TRUE(light.directionalShadowMapDirection().has_value());
    ASSERT_EQ(dir, *light.directionalShadowMapDirection());
  }

  TEST(DirectionalLight, ShouldNotProvidePositionalLightPosition) {
    DirectionalLight light(Vector3d(-0.5, -1, -0.5), Colord::white());

    ASSERT_FALSE(light.positionalLightPosition().has_value());
  }
}
