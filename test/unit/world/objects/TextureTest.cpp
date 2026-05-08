#include <gtest/gtest.h>

#include "world/objects/Texture.h"
#include "world/objects/ConstantColorTexture.h"
#include "world/objects/CheckerBoardTexture.h"
#include "world/objects/UVColorTexture.h"
#include "core/math/HitPoint.h"
#include "render/textures/Texture.h"
#include "render/textures/ConstantColorTexture.h"
#include "render/textures/CheckerBoardTexture.h"
#include "render/textures/UVColorTexture.h"

#include <QString>
#include <QJsonObject>

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
    auto rt = std::dynamic_pointer_cast<render::ConstantColorTexture>(
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

  TEST(CheckerBoardTexture, ShouldDefaultToPlanarMappingWithUnitScale) {
    CheckerBoardTexture texture;
    EXPECT_EQ(QString("planar"), texture.mapping());
    EXPECT_DOUBLE_EQ(1.0, texture.uScale());
    EXPECT_DOUBLE_EQ(1.0, texture.vScale());
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

  TEST(CheckerBoardTexture, ShouldSetAndGetUVMapping) {
    CheckerBoardTexture texture;
    texture.setMapping("uv");
    texture.setUScale(4.0);
    texture.setVScale(8.0);
    EXPECT_EQ(QString("uv"), texture.mapping());
    EXPECT_DOUBLE_EQ(4.0, texture.uScale());
    EXPECT_DOUBLE_EQ(8.0, texture.vScale());
  }

  TEST(CheckerBoardTexture, ShouldApplyUVMappingPropertiesReadFromJson) {
    ConstantColorTexture bright;
    bright.setColor(Colord::white());
    ConstantColorTexture dark;
    dark.setColor(Colord::black());

    CheckerBoardTexture texture;
    texture.setBrightTexture(&bright);
    texture.setDarkTexture(&dark);

    QJsonObject json;
    json["mapping"] = "uv";
    json["uScale"] = 4.0;
    json["vScale"] = 1.0;

    texture.read(json);

    auto rt = texture.toRaytracerTexture();
    HitPoint hp(nullptr, 0.0, Vector4d(0, 0, 0), Vector3d(0, 1, 0), Vector2d(0.3, 0.0));
    EXPECT_EQ(Colord::black(), rt->evaluate(Rayd::undefined(), hp));
  }

  TEST(CheckerBoardTexture, ShouldTreatUnknownMappingAsPlanar) {
    CheckerBoardTexture texture;
    texture.setMapping("unknown");
    EXPECT_EQ(QString("planar"), texture.mapping());
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
    auto rt = std::dynamic_pointer_cast<render::CheckerBoardTexture>(
      texture.toRaytracerTexture());
    ASSERT_NE(nullptr, rt);
    // Both sub-textures fall through textureOrDefault → the default
    // ConstantColorTexture, so brightTexture/darkTexture on the raytracer
    // side resolve to non-null.
    EXPECT_NE(nullptr, rt->brightTexture());
    EXPECT_NE(nullptr, rt->darkTexture());
  }

  TEST(CheckerBoardTexture, ShouldProduceUVMappedRaytracerCheckerBoardTexture) {
    ConstantColorTexture bright;
    bright.setColor(Colord::white());
    ConstantColorTexture dark;
    dark.setColor(Colord::black());
    CheckerBoardTexture texture;
    texture.setBrightTexture(&bright);
    texture.setDarkTexture(&dark);
    texture.setMapping("uv");
    texture.setUScale(4.0);
    texture.setVScale(1.0);

    auto rt = texture.toRaytracerTexture();

    EXPECT_EQ(
      Colord::black(),
      rt->evaluate(
        Rayd::undefined(),
        HitPoint(nullptr, 0, Vector3d::null(), Vector3d::up(), Vector2d(0.3, 0.0))));
  }

  // ---------- UVColorTexture -----------------------------------------------

  TEST(UVColorTexture, ShouldProduceRaytracerUVColorTexture) {
    UVColorTexture texture;
    auto rt = std::dynamic_pointer_cast<render::UVColorTexture>(
      texture.toRaytracerTexture());
    ASSERT_NE(nullptr, rt);
    EXPECT_EQ(
      Colord(0.25, 0.75, 0.0),
      rt->evaluate(
        Rayd::undefined(),
        HitPoint(nullptr, 0, Vector3d::null(), Vector3d::up(), Vector2d(0.25, 0.75))));
  }
}
