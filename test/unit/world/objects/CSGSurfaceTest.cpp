#include <gtest/gtest.h>

#include "world/objects/CSGSurface.h"
#include "world/objects/Difference.h"
#include "world/objects/Union.h"
#include "world/objects/Intersection.h"
#include "world/objects/MinkowskiSum.h"
#include "world/objects/ConvexHull.h"
#include "world/objects/Sphere.h"

#include "raytracer/primitives/Difference.h"
#include "raytracer/primitives/Union.h"
#include "raytracer/primitives/Intersection.h"
#include "raytracer/primitives/MinkowskiSum.h"
#include "raytracer/primitives/ConvexHull.h"
#include "raytracer/primitives/Composite.h"
#include "raytracer/primitives/Scene.h"

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
  //   - inactive → returns a raytracer::Composite (children pass through
  //     un-CSG'd) regardless of child count
  //
  // Pin all three for each operation so a renaming or behaviour swap is
  // a deliberate failure.

  // ---------- Difference ----------------------------------------------------

  TEST(Difference, ShouldReturnNullPrimitiveWhenEmptyAndActive) {
    Difference d;
    raytracer::Scene scene;
    EXPECT_EQ(nullptr, d.toRaytracer(&scene));
  }

  TEST(Difference, ShouldReturnCompositeWhenInactive) {
    Difference d;
    d.setActive(false);
    raytracer::Scene scene;
    auto rt = d.toRaytracer(&scene);
    ASSERT_NE(nullptr, rt);
  }

  TEST(Difference, ShouldReturnDifferencePrimitiveWhenNonEmptyAndActive) {
    Difference d;
    d.addChild(new Sphere);
    raytracer::Scene scene;
    EXPECT_NE(nullptr, d.toRaytracer(&scene));
  }

  // ---------- Union ---------------------------------------------------------

  TEST(Union, ShouldReturnNullPrimitiveWhenEmptyAndActive) {
    Union u;
    raytracer::Scene scene;
    EXPECT_EQ(nullptr, u.toRaytracer(&scene));
  }

  TEST(Union, ShouldReturnCompositeWhenInactive) {
    Union u;
    u.setActive(false);
    raytracer::Scene scene;
    EXPECT_NE(nullptr, u.toRaytracer(&scene));
  }

  TEST(Union, ShouldReturnUnionPrimitiveWhenNonEmptyAndActive) {
    Union u;
    u.addChild(new Sphere);
    raytracer::Scene scene;
    EXPECT_NE(nullptr, u.toRaytracer(&scene));
  }

  // ---------- Intersection --------------------------------------------------

  TEST(Intersection, ShouldReturnNullPrimitiveWhenEmptyAndActive) {
    Intersection i;
    raytracer::Scene scene;
    EXPECT_EQ(nullptr, i.toRaytracer(&scene));
  }

  TEST(Intersection, ShouldReturnCompositeWhenInactive) {
    Intersection i;
    i.setActive(false);
    raytracer::Scene scene;
    EXPECT_NE(nullptr, i.toRaytracer(&scene));
  }

  TEST(Intersection, ShouldReturnIntersectionPrimitiveWhenNonEmptyAndActive) {
    Intersection i;
    i.addChild(new Sphere);
    raytracer::Scene scene;
    EXPECT_NE(nullptr, i.toRaytracer(&scene));
  }

  // ---------- MinkowskiSum --------------------------------------------------

  TEST(MinkowskiSum, ShouldReturnNullPrimitiveWhenEmptyAndActive) {
    MinkowskiSum m;
    raytracer::Scene scene;
    EXPECT_EQ(nullptr, m.toRaytracer(&scene));
  }

  TEST(MinkowskiSum, ShouldReturnCompositeWhenInactive) {
    MinkowskiSum m;
    m.setActive(false);
    raytracer::Scene scene;
    EXPECT_NE(nullptr, m.toRaytracer(&scene));
  }

  TEST(MinkowskiSum, ShouldReturnMinkowskiSumPrimitiveWhenNonEmptyAndActive) {
    MinkowskiSum m;
    m.addChild(new Sphere);
    raytracer::Scene scene;
    EXPECT_NE(nullptr, m.toRaytracer(&scene));
  }

  // ---------- ConvexHull ----------------------------------------------------

  TEST(ConvexHull, ShouldReturnNullPrimitiveWhenEmptyAndActive) {
    ConvexHull c;
    raytracer::Scene scene;
    EXPECT_EQ(nullptr, c.toRaytracer(&scene));
  }

  TEST(ConvexHull, ShouldReturnCompositeWhenInactive) {
    ConvexHull c;
    c.setActive(false);
    raytracer::Scene scene;
    EXPECT_NE(nullptr, c.toRaytracer(&scene));
  }

  TEST(ConvexHull, ShouldReturnConvexHullPrimitiveWhenNonEmptyAndActive) {
    ConvexHull c;
    c.addChild(new Sphere);
    raytracer::Scene scene;
    EXPECT_NE(nullptr, c.toRaytracer(&scene));
  }
}
