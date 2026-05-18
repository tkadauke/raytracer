#include <gtest/gtest.h>

#include "world/objects/Triangle.h"

#include "render/primitives/Scene.h"

namespace TriangleTest {

  TEST(Triangle, ShouldDefaultToCannedVertices) {
    // Default triangle: (1,0,0), (-1,0,0), (0,-1,0) -- a right triangle
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
    render::Scene scene;
    EXPECT_NE(nullptr, t.toRaytracer(&scene));
  }

}
