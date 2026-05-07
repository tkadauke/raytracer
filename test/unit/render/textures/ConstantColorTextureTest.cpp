#include <gtest/gtest.h>
#include "render/textures/ConstantColorTexture.h"

#include "core/math/HitPoint.h"
#include "core/math/Ray.h"

namespace ConstantColorTextureTest {
  using namespace raytracer;
using namespace render;
using namespace render;

  TEST(ConstantColorTexture, ShouldInitialize) {
    ConstantColorTexture texture;
    ASSERT_EQ(Colord::black(), texture.color());
  }

  TEST(ConstantColorTexture, ShouldInitializeWithValues) {
    ConstantColorTexture texture(Colord(1, 0, 0));
    ASSERT_EQ(Colord(1, 0, 0), texture.color());
  }
  
  TEST(ConstantColorTexture, ShouldBeIndependentOfPointOrRayDirection) {
    ConstantColorTexture texture(Colord(1, 0, 0));
    ASSERT_EQ(Colord(1, 0, 0), texture.evaluate(Rayd::undefined(), HitPoint::undefined()));
  }
}
