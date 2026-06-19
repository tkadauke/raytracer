#include <gtest/gtest.h>

#include "core/math/HitPoint.h"
#include "render/textures/UVColorTexture.h"

namespace UVColorTextureTest {
  using namespace render;

  TEST(UVColorTexture, ShouldMapUVToRedAndGreenChannels) {
    UVColorTexture texture;
    HitPoint hitPoint(nullptr, 0, Vector3d::null, Vector3d::up(), Vector2d(0.25, 0.75));

    EXPECT_EQ(Colord(0.25, 0.75, 0.0), texture.evaluate(Rayd::undefined, hitPoint));
  }

  TEST(UVColorTexture, ReportsRuntimeTypeName) {
    UVColorTexture texture;

    EXPECT_STREQ("UVColorTexture", texture.typeName());
  }
}
