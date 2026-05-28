#include <gtest/gtest.h>

#include "core/math/HitPoint.h"
#include "core/math/Matrix.h"
#include "render/State.h"
#include "render/materials/PortalMaterial.h"
#include "render/primitives/Scene.h"

#include "test/helpers/ColorTestHelper.h"
#include "test/helpers/RecordingRayCaster.h"

namespace PortalMaterialTest {
  using namespace render;
  using test::helpers::RecordingRayCaster;

  TEST(PortalMaterial, ShouldRecurseThroughRayCasterAndFilterColor) {
    Scene scene;
    State state;
    RecordingRayCaster raycaster;
    raycaster.pushColor(Colord(0.8, 0.6, 0.4));
    PortalMaterial material(Matrix4d(), Colord(0.5, 0.25, 1.0));
    const HitPoint hitPoint(nullptr, 1.0, Vector4d(1, 2, 3, 1), Vector3d(0, 1, 0));
    const Rayd ray(Vector3d(9, 9, 9), Vector3d(0, 0, 1));

    const Colord color = material.shade(&raycaster, scene, ray, hitPoint, state);

    ASSERT_COLOR_NEAR(Colord(0.4, 0.15, 0.4), color, 0.001);
    ASSERT_EQ(1u, raycaster.rays.size());
    EXPECT_NEAR(1.0, raycaster.rays.front().origin().x(), 0.001);
    EXPECT_NEAR(2.0, raycaster.rays.front().origin().y(), 0.001);
    EXPECT_NEAR(3.0 + Rayd::epsilon, raycaster.rays.front().origin().z(), 0.001);
    EXPECT_NEAR(0.0, raycaster.rays.front().direction().x(), 0.001);
    EXPECT_NEAR(0.0, raycaster.rays.front().direction().y(), 0.001);
    EXPECT_NEAR(1.0, raycaster.rays.front().direction().z(), 0.001);
  }

  TEST(PortalMaterial, ShouldTransformRecursiveRayBeforeRayCasterCallback) {
    Scene scene;
    State state;
    RecordingRayCaster raycaster(Colord::white());
    PortalMaterial material(Matrix4d::translate(10, 0, 0), Colord::white());
    const HitPoint hitPoint(nullptr, 1.0, Vector4d(12, 0, 0, 1), Vector3d(0, 1, 0));
    const Rayd ray(Vector3d(0, 0, 0), Vector3d(0, 1, 0));

    material.shade(&raycaster, scene, ray, hitPoint, state);

    ASSERT_EQ(1u, raycaster.rays.size());
    EXPECT_NEAR(2.0, raycaster.rays.front().origin().x(), 0.001);
    EXPECT_NEAR(Rayd::epsilon, raycaster.rays.front().origin().y(), 0.001);
    EXPECT_NEAR(0.0, raycaster.rays.front().origin().z(), 0.001);
    EXPECT_NEAR(0.0, raycaster.rays.front().direction().x(), 0.001);
    EXPECT_NEAR(1.0, raycaster.rays.front().direction().y(), 0.001);
    EXPECT_NEAR(0.0, raycaster.rays.front().direction().z(), 0.001);
  }
}
