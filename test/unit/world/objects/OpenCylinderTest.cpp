#include <gtest/gtest.h>

#include "world/objects/OpenCylinder.h"

#include "render/primitives/Scene.h"

#include <limits>

namespace OpenCylinderTest {

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
    render::Scene scene;
    EXPECT_NE(nullptr, c.toRaytracer(&scene));
  }

}
