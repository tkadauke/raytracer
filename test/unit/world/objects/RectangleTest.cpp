#include <gtest/gtest.h>

#include "world/objects/Rectangle.h"

#include "render/primitives/Scene.h"

namespace RectangleTest {

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
    render::Scene scene;
    EXPECT_NE(nullptr, r.toRaytracer(&scene));
  }

}
