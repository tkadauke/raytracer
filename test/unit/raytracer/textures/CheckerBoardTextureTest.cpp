#include "gtest.h"
#include "raytracer/textures/CheckerBoardTexture.h"
#include "raytracer/textures/ConstantColorTexture.h"
#include "raytracer/textures/mappings/PlanarMapping2D.h"

#include "core/math/HitPoint.h"
#include "core/math/Ray.h"

namespace CheckerBoardTextureTest {
  using namespace raytracer;

  TEST(CheckerBoardTexture, ShouldInitializeWithValues) {
    auto bright = std::make_shared<ConstantColorTexture>(Colord::white());
    auto dark = std::make_shared<ConstantColorTexture>(Colord::black());
    CheckerBoardTexture texture(new PlanarMapping2D, bright, dark);
    ASSERT_EQ(bright, texture.brightTexture());
    ASSERT_EQ(dark, texture.darkTexture());
  }
  
  TEST(CheckerBoardTexture, ShouldChooseSubtextureBasedOnPosition) {
    CheckerBoardTexture texture(
      new PlanarMapping2D,
      std::make_shared<ConstantColorTexture>(Colord::white()),
      std::make_shared<ConstantColorTexture>(Colord::black())
    );
    
    ASSERT_EQ(Colord::white(), texture.evaluate(
      Rayd::undefined(), HitPoint(nullptr, 0, Vector4d(0.5, 0, 0.5), Vector3d::null()))
    );
    ASSERT_EQ(Colord::black(), texture.evaluate(
      Rayd::undefined(), HitPoint(nullptr, 0, Vector4d(1.5, 0, 0.5), Vector3d::null()))
    );
  }
}
