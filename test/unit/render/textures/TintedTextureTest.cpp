#include <gtest/gtest.h>

#include "core/math/HitPoint.h"
#include "render/textures/ConstantColorTexture.h"
#include "render/textures/TintedTexture.h"

namespace TintedTextureTest {
  using namespace render;

  TEST(TintedTexture, ShouldInitializeWithValues) {
    auto base = std::make_shared<ConstantColorTexture>(Colord::white());
    TintedTexture texture(base, Colord(0.25, 0.5, 0.75));

    EXPECT_EQ(base, texture.texture());
    EXPECT_EQ(Colord(0.25, 0.5, 0.75), texture.tint());
  }

  TEST(TintedTexture, ReportsRuntimeTypeName) {
    TintedTexture texture(std::make_shared<ConstantColorTexture>(Colord::white()),
                          Colord(0.25, 0.5, 0.75));

    EXPECT_STREQ("TintedTexture", texture.typeName());
  }

  TEST(TintedTexture, MultipliesChildTextureByTint) {
    TintedTexture texture(std::make_shared<ConstantColorTexture>(Colord(0.5, 0.25, 0.75)),
                          Colord(0.5, 0.25, 0.25));

    EXPECT_EQ(Colord(0.25, 0.0625, 0.1875),
              texture.evaluate(Rayd::undefined, HitPoint::undefined()));
  }
}
