#include <gtest/gtest.h>

#include "world/objects/Light.h"
#include "world/objects/PointLight.h"
#include "world/objects/DirectionalLight.h"
#include "world/objects/ElementFactory.h"
#include "world/objects/RectangularAreaLight.h"
#include "render/lights/Light.h"
#include "render/lights/PointLight.h"
#include "render/lights/DirectionalLight.h"
#include "render/lights/RectangularAreaLight.h"

#include "test/helpers/VectorTestHelper.h"

namespace LightTest {
  // ---------- Light (abstract base) -----------------------------------------

  TEST(Light, ShouldDefaultToVisible) {
    PointLight light;
    EXPECT_TRUE(light.visible());
  }

  TEST(Light, ShouldDefaultToWhiteColor) {
    PointLight light;
    EXPECT_EQ(Colord::white(), light.color());
  }

  TEST(Light, ShouldDefaultToHalfIntensity) {
    PointLight light;
    EXPECT_DOUBLE_EQ(0.5, light.intensity());
  }

  TEST(Light, ShouldSetAndGetVisible) {
    PointLight light;
    light.setVisible(false);
    EXPECT_FALSE(light.visible());
  }

  TEST(Light, ShouldSetAndGetColor) {
    PointLight light;
    light.setColor(Colord(0.1, 0.2, 0.3));
    EXPECT_EQ(Colord(0.1, 0.2, 0.3), light.color());
  }

  TEST(Light, ShouldSetAndGetIntensity) {
    PointLight light;
    light.setIntensity(0.75);
    EXPECT_DOUBLE_EQ(0.75, light.intensity());
  }

  TEST(Light, ShouldHideAfterShow) {
    PointLight light;
    light.show();
    light.hide();
    EXPECT_FALSE(light.visible());
  }

  TEST(Light, ShouldShowAfterHide) {
    PointLight light;
    light.hide();
    light.show();
    EXPECT_TRUE(light.visible());
  }

  // ---------- PointLight ----------------------------------------------------

  TEST(PointLight, ShouldProduceRaytracerPointLight) {
    PointLight light;
    auto rt = light.toRaytracer();
    ASSERT_NE(nullptr, rt);
    EXPECT_NE(nullptr, std::dynamic_pointer_cast<render::PointLight>(rt));
  }

  TEST(PointLight, ShouldDefaultRaytracerPositionToOrigin) {
    PointLight light;
    auto rt = std::dynamic_pointer_cast<render::PointLight>(light.toRaytracer());
    ASSERT_NE(nullptr, rt);
    ASSERT_VECTOR_NEAR(Vector3d(0, 0, 0), rt->position(), 1e-9);
  }

  TEST(PointLight, ShouldApplyPositionToRaytracerPosition) {
    PointLight light;
    light.setPosition(Vector3d(1, 2, 3));
    auto rt = std::dynamic_pointer_cast<render::PointLight>(light.toRaytracer());
    ASSERT_NE(nullptr, rt);
    ASSERT_VECTOR_NEAR(Vector3d(1, 2, 3), rt->position(), 1e-9);
  }

  TEST(PointLight, ShouldScaleColorByIntensity) {
    // toRaytracer multiplies the editable color by the intensity scalar so
    // a (1,1,1) light at intensity=0.5 lands as a (0.5,0.5,0.5) raytracer
    // PointLight — i.e. radiance is pre-baked into the colour rather than
    // tracked separately. Documenting that here so a future change that
    // splits radiance off again has a failing test to update.
    PointLight light;
    light.setColor(Colord(1.0, 0.8, 0.6));
    light.setIntensity(0.5);
    auto rt = std::dynamic_pointer_cast<render::PointLight>(light.toRaytracer());
    ASSERT_NE(nullptr, rt);
    EXPECT_DOUBLE_EQ(0.5, rt->color().r());
    EXPECT_DOUBLE_EQ(0.4, rt->color().g());
    EXPECT_DOUBLE_EQ(0.3, rt->color().b());
  }

  // ---------- DirectionalLight ----------------------------------------------

  TEST(DirectionalLight, ShouldDefaultToCannedDirection) {
    DirectionalLight light;
    EXPECT_EQ(Vector3d(-0.5, -1, -0.5), light.direction());
  }

  TEST(DirectionalLight, ShouldSetAndGetDirection) {
    DirectionalLight light;
    light.setDirection(Vector3d(1, 0, 0));
    EXPECT_EQ(Vector3d(1, 0, 0), light.direction());
  }

  TEST(DirectionalLight, ShouldProduceRaytracerDirectionalLight) {
    DirectionalLight light;
    auto rt = light.toRaytracer();
    ASSERT_NE(nullptr, rt);
    EXPECT_NE(nullptr, std::dynamic_pointer_cast<render::DirectionalLight>(rt));
  }

  TEST(DirectionalLight, ShouldNormalizeDirectionInRaytracerOutput) {
    // render::DirectionalLight's ctor normalizes its direction argument
    // (so radiance falls off correctly per the rendering equation). This
    // test pins that contract by feeding a non-unit direction and reading
    // it back unit-length.
    DirectionalLight light;
    light.setDirection(Vector3d(2, 0, 0));
    auto rt = std::dynamic_pointer_cast<render::DirectionalLight>(light.toRaytracer());
    ASSERT_NE(nullptr, rt);
    ASSERT_VECTOR_NEAR(Vector3d(1, 0, 0), rt->direction(), 1e-9);
  }

  TEST(DirectionalLight, ShouldIgnorePositionInRaytracerOutput) {
    // DirectionalLight is positionless by design — only its rotation
    // affects the resulting world-space direction. Setting position
    // shouldn't change the raytracer-side direction (ignored entirely)
    // since toRaytracer multiplies *only* the rotation matrix into the
    // editable direction.
    DirectionalLight a;
    a.setDirection(Vector3d(1, 0, 0));

    DirectionalLight b;
    b.setDirection(Vector3d(1, 0, 0));
    b.setPosition(Vector3d(100, 200, 300));

    auto rtA = std::dynamic_pointer_cast<render::DirectionalLight>(a.toRaytracer());
    auto rtB = std::dynamic_pointer_cast<render::DirectionalLight>(b.toRaytracer());
    ASSERT_NE(nullptr, rtA);
    ASSERT_NE(nullptr, rtB);
    ASSERT_VECTOR_NEAR(rtA->direction(), rtB->direction(), 1e-9);
  }

  TEST(DirectionalLight, ShouldScaleColorByIntensity) {
    DirectionalLight light;
    light.setColor(Colord(1, 1, 1));
    light.setIntensity(0.25);
    auto rt = std::dynamic_pointer_cast<render::DirectionalLight>(light.toRaytracer());
    ASSERT_NE(nullptr, rt);
    EXPECT_DOUBLE_EQ(0.25, rt->color().r());
    EXPECT_DOUBLE_EQ(0.25, rt->color().g());
    EXPECT_DOUBLE_EQ(0.25, rt->color().b());
  }

  // ---------- RectangularAreaLight ------------------------------------------

  TEST(RectangularAreaLight, ShouldDefaultToTwoByTwo) {
    RectangularAreaLight light;

    EXPECT_DOUBLE_EQ(2.0, light.width());
    EXPECT_DOUBLE_EQ(2.0, light.height());
  }

  TEST(RectangularAreaLight, ShouldSetPositiveDimensions) {
    RectangularAreaLight light;

    light.setWidth(-3.0);
    light.setHeight(-4.0);

    EXPECT_DOUBLE_EQ(3.0, light.width());
    EXPECT_DOUBLE_EQ(4.0, light.height());
  }

  TEST(RectangularAreaLight, ShouldProduceRaytracerRectangularAreaLight) {
    RectangularAreaLight light;

    auto rt = light.toRaytracer();

    ASSERT_NE(nullptr, rt);
    EXPECT_NE(nullptr, std::dynamic_pointer_cast<render::RectangularAreaLight>(rt));
  }

  TEST(RectangularAreaLight, ShouldTransformCenterAndEdges) {
    RectangularAreaLight light;
    light.setPosition(Vector3d(1, 2, 3));
    light.setWidth(4.0);
    light.setHeight(6.0);
    light.setColor(Colord(1.0, 0.8, 0.6));
    light.setIntensity(0.5);

    auto rt = std::dynamic_pointer_cast<render::RectangularAreaLight>(light.toRaytracer());

    ASSERT_NE(nullptr, rt);
    ASSERT_VECTOR_NEAR(Vector3d(1, 2, 3), rt->center(), 1e-9);
    ASSERT_VECTOR_NEAR(Vector3d(4, 0, 0), rt->edgeU(), 1e-9);
    ASSERT_VECTOR_NEAR(Vector3d(0, 0, 6), rt->edgeV(), 1e-9);
    EXPECT_EQ(Colord(0.5, 0.4, 0.3), rt->color());
  }

  TEST(RectangularAreaLight, ShouldBeRegisteredWithElementFactory) {
    auto light = ElementFactory::self().create("RectangularAreaLight");

    ASSERT_NE(nullptr, light);
    EXPECT_NE(nullptr, dynamic_cast<RectangularAreaLight*>(light.get()));
  }
}
