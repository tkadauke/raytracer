#include <gtest/gtest.h>

#include "world/objects/Disk.h"
#include "world/objects/OpenCylinder.h"
#include "world/objects/Rectangle.h"
#include "world/objects/Torus.h"
#include "world/objects/Triangle.h"
#include "world/objects/ScriptedSurface.h"

#include "raytracer/primitives/Scene.h"

#include <limits>

namespace PrimitiveShapeTest {
  // ---------- Disk ----------------------------------------------------------

  TEST(Disk, ShouldDefaultToUnitRadius) {
    Disk d;
    EXPECT_DOUBLE_EQ(1.0, d.radius());
  }

  TEST(Disk, ShouldSetAndGetRadius) {
    Disk d;
    d.setRadius(3.5);
    EXPECT_DOUBLE_EQ(3.5, d.radius());
  }

  TEST(Disk, ShouldTakeAbsoluteValueOfNegativeRadius) {
    Disk d;
    d.setRadius(-2.0);
    EXPECT_DOUBLE_EQ(2.0, d.radius());
  }

  TEST(Disk, ShouldClampZeroRadiusToEpsilon) {
    Disk d;
    d.setRadius(0.0);
    EXPECT_DOUBLE_EQ(std::numeric_limits<double>::epsilon(), d.radius());
  }

  TEST(Disk, ShouldProduceRaytracerPrimitive) {
    Disk d;
    raytracer::Scene scene;
    EXPECT_NE(nullptr, d.toRaytracer(&scene));
  }

  // ---------- OpenCylinder --------------------------------------------------

  TEST(OpenCylinder, ShouldDefaultToUnitRadiusAndHeight2) {
    OpenCylinder c;
    EXPECT_DOUBLE_EQ(1.0, c.radius());
    EXPECT_DOUBLE_EQ(2.0, c.height());
  }

  TEST(OpenCylinder, ShouldSetAndGetRadius) {
    OpenCylinder c;
    c.setRadius(4.0);
    EXPECT_DOUBLE_EQ(4.0, c.radius());
  }

  TEST(OpenCylinder, ShouldTakeAbsoluteValueOfNegativeRadius) {
    OpenCylinder c;
    c.setRadius(-3.0);
    EXPECT_DOUBLE_EQ(3.0, c.radius());
  }

  TEST(OpenCylinder, ShouldClampZeroRadiusToEpsilon) {
    OpenCylinder c;
    c.setRadius(0.0);
    EXPECT_DOUBLE_EQ(std::numeric_limits<double>::epsilon(), c.radius());
  }

  TEST(OpenCylinder, ShouldSetAndGetHeight) {
    OpenCylinder c;
    c.setHeight(5.0);
    EXPECT_DOUBLE_EQ(5.0, c.height());
  }

  TEST(OpenCylinder, ShouldTakeAbsoluteValueOfNegativeHeight) {
    OpenCylinder c;
    c.setHeight(-1.5);
    EXPECT_DOUBLE_EQ(1.5, c.height());
  }

  TEST(OpenCylinder, ShouldClampZeroHeightToEpsilon) {
    OpenCylinder c;
    c.setHeight(0.0);
    EXPECT_DOUBLE_EQ(std::numeric_limits<double>::epsilon(), c.height());
  }

  TEST(OpenCylinder, ShouldProduceRaytracerPrimitive) {
    OpenCylinder c;
    raytracer::Scene scene;
    EXPECT_NE(nullptr, c.toRaytracer(&scene));
  }

  // ---------- Rectangle -----------------------------------------------------

  TEST(Rectangle, ShouldDefaultToUnitXLeg1) {
    Rectangle r;
    EXPECT_EQ(Vector3d(1, 0, 0), r.leg1());
  }

  TEST(Rectangle, ShouldDefaultToUnitZLeg2) {
    Rectangle r;
    EXPECT_EQ(Vector3d(0, 0, 1), r.leg2());
  }

  TEST(Rectangle, ShouldSetAndGetLeg1) {
    Rectangle r;
    r.setLeg1(Vector3d(2, 0, 0));
    EXPECT_EQ(Vector3d(2, 0, 0), r.leg1());
  }

  TEST(Rectangle, ShouldSetAndGetLeg2) {
    Rectangle r;
    r.setLeg2(Vector3d(0, 0, 3));
    EXPECT_EQ(Vector3d(0, 0, 3), r.leg2());
  }

  TEST(Rectangle, ShouldProduceRaytracerPrimitive) {
    Rectangle r;
    raytracer::Scene scene;
    EXPECT_NE(nullptr, r.toRaytracer(&scene));
  }

  // ---------- Torus ---------------------------------------------------------

  TEST(Torus, ShouldDefaultToSweptRadius2AndTubeRadius1) {
    Torus t;
    EXPECT_DOUBLE_EQ(2.0, t.sweptRadius());
    EXPECT_DOUBLE_EQ(1.0, t.tubeRadius());
  }

  TEST(Torus, ShouldSetAndGetSweptRadius) {
    Torus t;
    t.setSweptRadius(3.0);
    EXPECT_DOUBLE_EQ(3.0, t.sweptRadius());
  }

  TEST(Torus, ShouldTakeAbsoluteValueOfNegativeSweptRadius) {
    Torus t;
    t.setSweptRadius(-2.5);
    EXPECT_DOUBLE_EQ(2.5, t.sweptRadius());
  }

  TEST(Torus, ShouldClampZeroSweptRadiusToEpsilon) {
    Torus t;
    t.setSweptRadius(0.0);
    EXPECT_DOUBLE_EQ(std::numeric_limits<double>::epsilon(), t.sweptRadius());
  }

  TEST(Torus, ShouldSetAndGetTubeRadius) {
    Torus t;
    t.setTubeRadius(0.5);
    EXPECT_DOUBLE_EQ(0.5, t.tubeRadius());
  }

  TEST(Torus, ShouldTakeAbsoluteValueOfNegativeTubeRadius) {
    Torus t;
    t.setTubeRadius(-0.8);
    EXPECT_DOUBLE_EQ(0.8, t.tubeRadius());
  }

  TEST(Torus, ShouldClampZeroTubeRadiusToEpsilon) {
    Torus t;
    t.setTubeRadius(0.0);
    EXPECT_DOUBLE_EQ(std::numeric_limits<double>::epsilon(), t.tubeRadius());
  }

  TEST(Torus, ShouldProduceRaytracerPrimitive) {
    Torus t;
    raytracer::Scene scene;
    EXPECT_NE(nullptr, t.toRaytracer(&scene));
  }

  // ---------- Triangle ------------------------------------------------------

  TEST(Triangle, ShouldDefaultToCannedVertices) {
    // Default triangle: (1,0,0), (-1,0,0), (0,-1,0) — a right triangle
    // in the XY plane that sits near the origin. Pin the layout so a
    // "more symmetric default" change is a deliberate test update.
    Triangle t;
    EXPECT_EQ(Vector3d(1, 0, 0), t.vertexA());
    EXPECT_EQ(Vector3d(-1, 0, 0), t.vertexB());
    EXPECT_EQ(Vector3d(0, -1, 0), t.vertexC());
  }

  TEST(Triangle, ShouldSetAndGetVertexA) {
    Triangle t;
    t.setVertexA(Vector3d(2, 3, 4));
    EXPECT_EQ(Vector3d(2, 3, 4), t.vertexA());
  }

  TEST(Triangle, ShouldSetAndGetVertexB) {
    Triangle t;
    t.setVertexB(Vector3d(-2, 0, 1));
    EXPECT_EQ(Vector3d(-2, 0, 1), t.vertexB());
  }

  TEST(Triangle, ShouldSetAndGetVertexC) {
    Triangle t;
    t.setVertexC(Vector3d(0, 5, -3));
    EXPECT_EQ(Vector3d(0, 5, -3), t.vertexC());
  }

  TEST(Triangle, ShouldProduceRaytracerPrimitive) {
    Triangle t;
    raytracer::Scene scene;
    EXPECT_NE(nullptr, t.toRaytracer(&scene));
  }

  // ---------- ScriptedSurface -----------------------------------------------

  TEST(ScriptedSurface, ShouldDefaultToEmptyScriptName) {
    ScriptedSurface s;
    EXPECT_TRUE(s.scriptName().isEmpty());
  }

  TEST(ScriptedSurface, ShouldDefaultToNotGeneratedAndVisible) {
    // Inherits Surface defaults: visible=true, material=nullptr.
    ScriptedSurface s;
    EXPECT_TRUE(s.visible());
    EXPECT_EQ(nullptr, s.material());
  }
}
