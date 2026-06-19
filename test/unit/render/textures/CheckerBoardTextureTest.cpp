#include <gtest/gtest.h>
#include "render/textures/CheckerBoardTexture.h"
#include "render/textures/ConstantColorTexture.h"
#include "render/textures/mappings/PlanarMapping2D.h"

#include "core/math/HitPoint.h"
#include "core/math/Ray.h"

namespace CheckerBoardTextureTest {
  using namespace render;

  TEST(CheckerBoardTexture, ShouldInitializeWithValues) {
    auto mapping = new PlanarMapping2D;
    auto bright = std::make_shared<ConstantColorTexture>(Colord::white());
    auto dark = std::make_shared<ConstantColorTexture>(Colord::black());
    CheckerBoardTexture texture(mapping, bright, dark);
    ASSERT_EQ(mapping, texture.mapping());
    ASSERT_EQ(bright, texture.brightTexture());
    ASSERT_EQ(dark, texture.darkTexture());
  }

  TEST(CheckerBoardTexture, ReportsRuntimeTypeName) {
    CheckerBoardTexture texture(new PlanarMapping2D,
                                std::make_shared<ConstantColorTexture>(Colord::white()),
                                std::make_shared<ConstantColorTexture>(Colord::black()));

    EXPECT_STREQ("CheckerBoardTexture", texture.typeName());
  }

  TEST(CheckerBoardTexture, ShouldChooseSubtextureBasedOnPosition) {
    CheckerBoardTexture texture(new PlanarMapping2D,
                                std::make_shared<ConstantColorTexture>(Colord::white()),
                                std::make_shared<ConstantColorTexture>(Colord::black()));

    ASSERT_EQ(Colord::white(),
              texture.evaluate(Rayd::undefined,
                               HitPoint(nullptr, 0, Vector4d(0.5, 0, 0.5), Vector3d::null)));
    ASSERT_EQ(Colord::black(),
              texture.evaluate(Rayd::undefined,
                               HitPoint(nullptr, 0, Vector4d(1.5, 0, 0.5), Vector3d::null)));
  }
}
