#include <gtest/gtest.h>

#include "world/objects/Surface.h"
#include "world/objects/Sphere.h"
#include "world/objects/Box.h"
#include "world/objects/Cylinder.h"
#include "world/objects/Ring.h"
#include "world/objects/MatteMaterial.h"
#include "world/objects/PointLight.h"
#include "world/objects/Element.h"

#include "render/primitives/Primitive.h"
#include "render/primitives/Scene.h"

#include <limits>

namespace SurfaceTest {
  // ---------- Surface (abstract base) ---------------------------------------

  TEST(Surface, ShouldDefaultToVisibleAndUnmaterialised) {
    Sphere s;
    EXPECT_TRUE(s.visible());
    EXPECT_EQ(nullptr, s.material());
  }

  TEST(Surface, ShouldSetAndGetVisibleAndMaterial) {
    Sphere s;
    MatteMaterial m;
    s.setVisible(false);
    s.setMaterial(&m);
    EXPECT_FALSE(s.visible());
    EXPECT_EQ(&m, s.material());
  }

  TEST(Surface, ShouldHideAfterShow) {
    Sphere s;
    s.show();
    s.hide();
    EXPECT_FALSE(s.visible());
  }

  TEST(Surface, ShouldShowAfterHide) {
    Sphere s;
    s.hide();
    s.show();
    EXPECT_TRUE(s.visible());
  }

  TEST(Surface, ShouldAllowSurfaceAndLightChildren) {
    Sphere s;
    Sphere child;
    PointLight light;
    EXPECT_TRUE(s.canHaveChild(&child));
    EXPECT_TRUE(s.canHaveChild(&light));
  }

  TEST(Surface, ShouldRejectNonSurfaceNonLightChildren) {
    Sphere s;
    Element other;
    EXPECT_FALSE(s.canHaveChild(&other));
  }

  TEST(Surface, ShouldProduceRaytracerPrimitive) {
    Sphere s;
    render::Scene scene;
    EXPECT_NE(nullptr, s.toRaytracer(&scene));
  }

  // ---------- Sphere --------------------------------------------------------

  TEST(Sphere, ShouldDefaultToUnitRadius) {
    Sphere s;
    EXPECT_DOUBLE_EQ(1.0, s.radius());
  }

  TEST(Sphere, ShouldSetAndGetRadius) {
    Sphere s;
    s.setRadius(3.0);
    EXPECT_DOUBLE_EQ(3.0, s.radius());
  }

  TEST(Sphere, ShouldTakeAbsoluteValueOfNegativeRadius) {
    Sphere s;
    s.setRadius(-2.0);
    EXPECT_DOUBLE_EQ(2.0, s.radius());
  }

  TEST(Sphere, ShouldClampZeroRadiusToEpsilon) {
    // A truly-zero radius would make render::Sphere::intersect divide
    // by zero (radius² is the discriminant denominator). The setter
    // floors at numeric_limits epsilon to keep the math defined while
    // still letting the user "shrink to invisibility".
    Sphere s;
    s.setRadius(0.0);
    EXPECT_DOUBLE_EQ(std::numeric_limits<double>::epsilon(), s.radius());
  }

  // ---------- Box -----------------------------------------------------------

  TEST(Box, ShouldDefaultToUnitSize) {
    Box b;
    EXPECT_EQ(Vector3d(1, 1, 1), b.size());
  }

  TEST(Box, ShouldDefaultToZeroBevelRadius) {
    Box b;
    EXPECT_DOUBLE_EQ(0.0, b.bevelRadius());
  }

  TEST(Box, ShouldClampNearZeroSizeToOneMicron) {
    Box b;
    b.setSize(Vector3d(0, 0, 0));
    EXPECT_DOUBLE_EQ(1e-6, b.size().x());
    EXPECT_DOUBLE_EQ(1e-6, b.size().y());
    EXPECT_DOUBLE_EQ(1e-6, b.size().z());
  }

  TEST(Box, ShouldTakeAbsoluteValueOfNegativeSize) {
    Box b;
    b.setSize(Vector3d(-2, -3, -4));
    EXPECT_EQ(Vector3d(2, 3, 4), b.size());
  }

  TEST(Box, ShouldClampBevelRadiusToHalfSizeMin) {
    // bevelRadius >= size.min() would generate a sphere instead of a
    // rounded box (see Box::toRaytracerPrimitive's r==s.min() branch);
    // setBevelRadius caps at size.min() so the user gets exactly that
    // sphere-degenerate case rather than a malformed primitive.
    Box b;
    b.setSize(Vector3d(2, 4, 6));
    b.setBevelRadius(10.0);
    EXPECT_DOUBLE_EQ(2.0, b.bevelRadius());
  }

  TEST(Box, ShouldRecomputeBevelRadiusWhenSizeShrinks) {
    Box b;
    b.setSize(Vector3d(10, 10, 10));
    b.setBevelRadius(3.0);
    b.setSize(Vector3d(2, 2, 2));
    // setSize calls setBevelRadius again with the previous radius —
    // which then clamps to the new (smaller) size.min(). Pin so a future
    // refactor that drops the recompute is loud.
    EXPECT_DOUBLE_EQ(2.0, b.bevelRadius());
  }

  TEST(Box, ShouldProduceRaytracerPrimitiveForSharpBox) {
    Box b;
    render::Scene scene;
    EXPECT_NE(nullptr, b.toRaytracer(&scene));
  }

  TEST(Box, ShouldProduceRaytracerPrimitiveForBevelledBox) {
    Box b;
    b.setSize(Vector3d(2, 2, 2));
    b.setBevelRadius(0.5);
    render::Scene scene;
    EXPECT_NE(nullptr, b.toRaytracer(&scene));
  }

  // ---------- Cylinder ------------------------------------------------------

  TEST(Cylinder, ShouldDefaultToUnitRadiusAndHeight2) {
    Cylinder c;
    EXPECT_DOUBLE_EQ(1.0, c.radius());
    EXPECT_DOUBLE_EQ(2.0, c.height());
    EXPECT_DOUBLE_EQ(0.0, c.bevelRadius());
  }

  TEST(Cylinder, ShouldClampNegativeRadiusToAbsoluteValue) {
    Cylinder c;
    c.setRadius(-2);
    EXPECT_DOUBLE_EQ(2.0, c.radius());
  }

  TEST(Cylinder, ShouldClampZeroRadiusToEpsilon) {
    Cylinder c;
    c.setRadius(0.0);
    EXPECT_DOUBLE_EQ(std::numeric_limits<double>::epsilon(), c.radius());
  }

  TEST(Cylinder, ShouldProduceRaytracerPrimitive) {
    Cylinder c;
    render::Scene scene;
    EXPECT_NE(nullptr, c.toRaytracer(&scene));
  }

  // ---------- Ring ----------------------------------------------------------

  TEST(Ring, ShouldDefaultToCannedDimensions) {
    Ring r;
    EXPECT_DOUBLE_EQ(0.5, r.innerRadius());
    EXPECT_DOUBLE_EQ(1.0, r.outerRadius());
    EXPECT_DOUBLE_EQ(2.0, r.height());
    EXPECT_DOUBLE_EQ(0.0, r.bevelRadius());
  }

  TEST(Ring, ShouldKeepOuterRadiusGreaterThanInnerRadius) {
    // Setting outerRadius below innerRadius would invert the ring (the
    // CSG difference would be empty). The setter floors outerRadius at
    // innerRadius + epsilon. Pin so future "validation moves to the
    // model layer" sweeps don't silently change the contract.
    Ring r;
    r.setInnerRadius(1.0);
    r.setOuterRadius(0.5);
    EXPECT_GT(r.outerRadius(), r.innerRadius());
  }

  TEST(Ring, ShouldProduceRaytracerPrimitive) {
    Ring r;
    render::Scene scene;
    EXPECT_NE(nullptr, r.toRaytracer(&scene));
  }
}
