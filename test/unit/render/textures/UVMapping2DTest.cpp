#include <gtest/gtest.h>

#include "core/math/HitPoint.h"
#include "render/textures/mappings/UVMapping2D.h"

namespace UVMapping2DTest {
  using namespace render;

  TEST(UVMapping2D, ShouldMapHitPointUVToTextureCoordinates) {
    UVMapping2D mapping;
    HitPoint hitPoint(nullptr, 0, Vector3d::null(), Vector3d::up(), Vector2d(1.25, 0.75));
    double s = 0;
    double t = 0;

    mapping.map(hitPoint, s, t);

    EXPECT_DOUBLE_EQ(1.25, s);
    EXPECT_DOUBLE_EQ(0.75, t);
  }
}
