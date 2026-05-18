#include <gtest/gtest.h>

#include "world/objects/Disk.h"

#include "render/primitives/Scene.h"

#include <limits>

namespace DiskTest {

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
    render::Scene scene;
    EXPECT_NE(nullptr, d.toRaytracer(&scene));
  }

}
