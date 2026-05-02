#include <gtest/gtest.h>

#include "world/objects/Texture.h"
#include "world/objects/ConstantColorTexture.h"
#include "world/objects/CheckerBoardTexture.h"
#include "raytracer/textures/Texture.h"
#include "raytracer/textures/ConstantColorTexture.h"
#include "raytracer/textures/CheckerBoardTexture.h"

namespace TextureTest {
  // ---------- Texture (abstract base) ---------------------------------------

  TEST(Texture, ShouldReturnSameDefaultTextureAcrossCalls) {
    // defaultTexture() is a Meyers-singleton-ish lazy init that returns the
    // same heap-allocated ConstantColorTexture on every call. Pin that so a
    // future change that hands out per-call instances is deliberate.
    EXPECT_EQ(Texture::defaultTexture(), Texture::defaultTexture());
  }

  TEST(Texture, ShouldReturnBlackConstantColorTextureAsDefault) {
    auto* def = dynamic_cast<ConstantColorTexture*>(Texture::defaultTexture());
    ASSERT_NE(nullptr, def);
    EXPECT_EQ(Colord::black(), def->color());
  }

  TEST(Texture, ShouldReturnDefaultWhenTextureOrDefaultGetsNull) {
    EXPECT_EQ(Texture::defaultTexture(), textureOrDefault(nullptr));
  }

  TEST(Texture, ShouldReturnSameTextureWhenTextureOrDefaultGetsNonNull) {
    ConstantColorTexture custom;
    EXPECT_EQ(&custom, textureOrDefault(&custom));
  }

  // ---------- ConstantColorTexture ------------------------------------------

  TEST(ConstantColorTexture, ShouldDefaultToBlack) {
    ConstantColorTexture texture;
    EXPECT_EQ(Colord::black(), texture.color());
  }

  TEST(ConstantColorTexture, ShouldSetAndGetColor) {
    ConstantColorTexture texture;
    texture.setColor(Colord(0.2, 0.4, 0.6));
    EXPECT_EQ(Colord(0.2, 0.4, 0.6), texture.color());
  }

  TEST(ConstantColorTexture, ShouldProduceRaytracerConstantColorTexture) {
    ConstantColorTexture texture;
    texture.setColor(Colord(0.1, 0.2, 0.3));
    auto rt = std::dynamic_pointer_cast<raytracer::ConstantColorTexture>(
      texture.toRaytracerTexture());
    ASSERT_NE(nullptr, rt);
    EXPECT_EQ(Colord(0.1, 0.2, 0.3), rt->color());
  }

  // ---------- CheckerBoardTexture -------------------------------------------

  TEST(CheckerBoardTexture, ShouldDefaultToNullSubTextures) {
    CheckerBoardTexture texture;
    EXPECT_EQ(nullptr, texture.brightTexture());
    EXPECT_EQ(nullptr, texture.darkTexture());
  }

  TEST(CheckerBoardTexture, ShouldSetAndGetBrightTexture) {
    CheckerBoardTexture texture;
    ConstantColorTexture bright;
    texture.setBrightTexture(&bright);
    EXPECT_EQ(&bright, texture.brightTexture());
  }

  TEST(CheckerBoardTexture, ShouldSetAndGetDarkTexture) {
    CheckerBoardTexture texture;
    ConstantColorTexture dark;
    texture.setDarkTexture(&dark);
    EXPECT_EQ(&dark, texture.darkTexture());
  }

  TEST(CheckerBoardTexture, ShouldRejectSelfAsBrightTexture) {
    // Self-reference would cause infinite recursion in toRaytracerTexture
    // (each call tries to convert its own bright/dark sub-tree). The
    // setter coerces self → nullptr instead, so toRaytracerTexture falls
    // back to the default texture.
    CheckerBoardTexture texture;
    texture.setBrightTexture(&texture);
    EXPECT_EQ(nullptr, texture.brightTexture());
  }

  TEST(CheckerBoardTexture, ShouldRejectSelfAsDarkTexture) {
    CheckerBoardTexture texture;
    texture.setDarkTexture(&texture);
    EXPECT_EQ(nullptr, texture.darkTexture());
  }

  TEST(CheckerBoardTexture, ShouldProduceRaytracerCheckerBoardTexture) {
    CheckerBoardTexture texture;
    auto rt = std::dynamic_pointer_cast<raytracer::CheckerBoardTexture>(
      texture.toRaytracerTexture());
    ASSERT_NE(nullptr, rt);
    // Both sub-textures fall through textureOrDefault → the default
    // ConstantColorTexture, so brightTexture/darkTexture on the raytracer
    // side resolve to non-null.
    EXPECT_NE(nullptr, rt->brightTexture());
    EXPECT_NE(nullptr, rt->darkTexture());
  }
}
