#include <gtest/gtest.h>

#include "world/objects/Torus.h"

#include "render/primitives/Scene.h"

#include <limits>

namespace TorusTest {

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
    render::Scene scene;
    EXPECT_NE(nullptr, t.toRaytracer(&scene));
  }

}
