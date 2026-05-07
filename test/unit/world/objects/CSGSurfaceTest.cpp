#include <gtest/gtest.h>

#include "world/objects/CSGSurface.h"
#include "world/objects/Difference.h"
#include "world/objects/Union.h"
#include "world/objects/Intersection.h"
#include "world/objects/MinkowskiSum.h"
#include "world/objects/ConvexHull.h"
#include "world/objects/Sphere.h"

#include "render/primitives/Difference.h"
#include "render/primitives/Union.h"
#include "render/primitives/Intersection.h"
#include "render/primitives/MinkowskiSum.h"
#include "render/primitives/ConvexHull.h"
#include "render/primitives/Composite.h"
#include "render/primitives/Scene.h"

namespace CSGSurfaceTest {
  // ---------- CSGSurface (abstract base) ------------------------------------

  TEST(CSGSurface, ShouldDefaultToActive) {
    Difference csg;
    EXPECT_TRUE(csg.active());
  }

  TEST(CSGSurface, ShouldSetAndGetActive) {
    Difference csg;
    csg.setActive(false);
    EXPECT_FALSE(csg.active());
  }

  // The remaining tests for each concrete CSG share a pattern:
  //
  //   - empty + active → toRaytracerPrimitive returns nullptr (no children
  //     means the operation is undefined; the renderer drops it)
  //   - non-empty + active → returns the CSG type itself
  //   - inactive → returns a render::Composite (children pass through
  //     un-CSG'd) regardless of child count
  //
  // Pin all three for each operation so a renaming or behaviour swap is
  // a deliberate failure.

  // ---------- Difference ----------------------------------------------------

  TEST(Difference, ShouldReturnNullPrimitiveWhenEmptyAndActive) {
    Difference d;
    render::Scene scene;
    EXPECT_EQ(nullptr, d.toRaytracer(&scene));
  }

  TEST(Difference, ShouldReturnCompositeWhenInactive) {
    Difference d;
    d.setActive(false);
    render::Scene scene;
    auto rt = d.toRaytracer(&scene);
    ASSERT_NE(nullptr, rt);
  }

  TEST(Difference, ShouldReturnDifferencePrimitiveWhenNonEmptyAndActive) {
    Difference d;
    d.addChild(new Sphere);
    render::Scene scene;
    EXPECT_NE(nullptr, d.toRaytracer(&scene));
  }

  // ---------- Union ---------------------------------------------------------

  TEST(Union, ShouldReturnNullPrimitiveWhenEmptyAndActive) {
    Union u;
    render::Scene scene;
    EXPECT_EQ(nullptr, u.toRaytracer(&scene));
  }

  TEST(Union, ShouldReturnCompositeWhenInactive) {
    Union u;
    u.setActive(false);
    render::Scene scene;
    EXPECT_NE(nullptr, u.toRaytracer(&scene));
  }

  TEST(Union, ShouldReturnUnionPrimitiveWhenNonEmptyAndActive) {
    Union u;
    u.addChild(new Sphere);
    render::Scene scene;
    EXPECT_NE(nullptr, u.toRaytracer(&scene));
  }

  // ---------- Intersection --------------------------------------------------

  TEST(Intersection, ShouldReturnNullPrimitiveWhenEmptyAndActive) {
    Intersection i;
    render::Scene scene;
    EXPECT_EQ(nullptr, i.toRaytracer(&scene));
  }

  TEST(Intersection, ShouldReturnCompositeWhenInactive) {
    Intersection i;
    i.setActive(false);
    render::Scene scene;
    EXPECT_NE(nullptr, i.toRaytracer(&scene));
  }

  TEST(Intersection, ShouldReturnIntersectionPrimitiveWhenNonEmptyAndActive) {
    Intersection i;
    i.addChild(new Sphere);
    render::Scene scene;
    EXPECT_NE(nullptr, i.toRaytracer(&scene));
  }

  // ---------- MinkowskiSum --------------------------------------------------

  TEST(MinkowskiSum, ShouldReturnNullPrimitiveWhenEmptyAndActive) {
    MinkowskiSum m;
    render::Scene scene;
    EXPECT_EQ(nullptr, m.toRaytracer(&scene));
  }

  TEST(MinkowskiSum, ShouldReturnCompositeWhenInactive) {
    MinkowskiSum m;
    m.setActive(false);
    render::Scene scene;
    EXPECT_NE(nullptr, m.toRaytracer(&scene));
  }

  TEST(MinkowskiSum, ShouldReturnMinkowskiSumPrimitiveWhenNonEmptyAndActive) {
    MinkowskiSum m;
    m.addChild(new Sphere);
    render::Scene scene;
    EXPECT_NE(nullptr, m.toRaytracer(&scene));
  }

  // ---------- ConvexHull ----------------------------------------------------

  TEST(ConvexHull, ShouldReturnNullPrimitiveWhenEmptyAndActive) {
    ConvexHull c;
    render::Scene scene;
    EXPECT_EQ(nullptr, c.toRaytracer(&scene));
  }

  TEST(ConvexHull, ShouldReturnCompositeWhenInactive) {
    ConvexHull c;
    c.setActive(false);
    render::Scene scene;
    EXPECT_NE(nullptr, c.toRaytracer(&scene));
  }

  TEST(ConvexHull, ShouldReturnConvexHullPrimitiveWhenNonEmptyAndActive) {
    ConvexHull c;
    c.addChild(new Sphere);
    render::Scene scene;
    EXPECT_NE(nullptr, c.toRaytracer(&scene));
  }
}
