#include <gtest/gtest.h>

#include "world/objects/Material.h"
#include "world/objects/PortalMaterial.h"

#include "render/materials/PortalMaterial.h"

namespace PortalMaterialTest {
  TEST(PortalMaterial, ShouldDefaultToIdentityTransform) {
    PortalMaterial material;

    EXPECT_EQ(Vector3d::null, material.position());
    EXPECT_EQ(Vector3d::null, material.rotation());
    EXPECT_EQ(Vector3d::one, material.scale());
  }

  TEST(PortalMaterial, ShouldDefaultToWhiteFilter) {
    PortalMaterial material;

    EXPECT_EQ(Colord::white(), material.filterColor());
  }

  TEST(PortalMaterial, ShouldSetAndGetTransformProperties) {
    PortalMaterial material;

    material.setPosition(Vector3d(1, 2, 3));
    material.setRotation(Vector3d(0.1, 0.2, 0.3));
    material.setScale(Vector3d(2, 3, 4));

    EXPECT_EQ(Vector3d(1, 2, 3), material.position());
    EXPECT_EQ(Vector3d(0.1, 0.2, 0.3), material.rotation());
    EXPECT_EQ(Vector3d(2, 3, 4), material.scale());
  }

  TEST(PortalMaterial, ShouldKeepScaleInvertible) {
    PortalMaterial material;

    material.setScale(Vector3d(-2, 0, 4));

    EXPECT_EQ(Vector3d(2, 0.000001, 4), material.scale());
  }

  TEST(PortalMaterial, ShouldSetAndGetFilterColor) {
    PortalMaterial material;

    material.setFilterColor(Colord(0.25, 0.5, 0.75));

    EXPECT_EQ(Colord(0.25, 0.5, 0.75), material.filterColor());
  }

  TEST(PortalMaterial, ShouldProduceRaytracerPortalMaterial) {
    PortalMaterial material;
    Material* base = &material;

    auto rt = std::dynamic_pointer_cast<render::PortalMaterial>(base->toRaytracerMaterial());

    EXPECT_NE(nullptr, rt);
  }
}
