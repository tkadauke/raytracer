#include <gtest/gtest.h>

#include "core/math/HitPoint.h"
#include "render/textures/ImageTexture.h"
#include "render/textures/mappings/UVMapping2D.h"

namespace ImageTextureTest {
  using namespace render;

  void expectColorNear(const Colord& expected, const Colord& actual) {
    EXPECT_NEAR(expected.r(), actual.r(), 1e-9);
    EXPECT_NEAR(expected.g(), actual.g(), 1e-9);
    EXPECT_NEAR(expected.b(), actual.b(), 1e-9);
  }

  std::vector<Colord> quadPixels() {
    return {Colord::red(), Colord::green(), Colord::blue(), Colord::white()};
  }

  TEST(ImageTexture, NearestSamplesContainingTexel) {
    ImageTexture texture(new UVMapping2D, 2, 2, quadPixels(), ImageTextureFilter::Nearest);

    EXPECT_EQ(Colord::red(), texture.sample(0.1, 0.1));
    EXPECT_EQ(Colord::white(), texture.sample(0.75, 0.75));
  }

  TEST(ImageTexture, BilinearSamplesPixelCentersExactlyAndBlendsBetweenThem) {
    ImageTexture texture(new UVMapping2D, 2, 2, quadPixels(), ImageTextureFilter::Bilinear);

    EXPECT_EQ(Colord::red(), texture.sample(0.25, 0.25));
    expectColorNear(Colord(0.5, 0.5, 0.5), texture.sample(0.5, 0.5));
  }

  TEST(ImageTexture, RepeatWrapsCoordinatesOutsideTheUnitSquare) {
    ImageTexture texture(new UVMapping2D, 2, 2, quadPixels(), ImageTextureFilter::Nearest,
                         ImageTextureWrap::Repeat);

    EXPECT_EQ(Colord::red(), texture.sample(1.1, 0.1));
    EXPECT_EQ(Colord::blue(), texture.sample(-0.9, 0.75));
  }

  TEST(ImageTexture, ClampPinsCoordinatesToTheNearestEdgeTexel) {
    ImageTexture texture(new UVMapping2D, 2, 2, quadPixels(), ImageTextureFilter::Nearest,
                         ImageTextureWrap::Clamp);

    EXPECT_EQ(Colord::green(), texture.sample(1.5, 0.1));
    EXPECT_EQ(Colord::blue(), texture.sample(-0.5, 0.75));
  }

  TEST(ImageTexture, MipLevelSelectionUsesLargestScreenSpaceTexelFootprint) {
    ImageTexture texture(new UVMapping2D, 4, 4, std::vector<Colord>(16, Colord::white()),
                         ImageTextureFilter::Mipmap);

    EXPECT_DOUBLE_EQ(0.0, texture.mipLevelForDerivatives(Vector2d(0.0, 0.0), Vector2d(0.0, 0.0)));
    EXPECT_DOUBLE_EQ(1.0, texture.mipLevelForDerivatives(Vector2d(0.5, 0.0), Vector2d(0.0, 0.0)));
    EXPECT_DOUBLE_EQ(2.0, texture.mipLevelForDerivatives(Vector2d(1.0, 0.0), Vector2d(0.0, 0.0)));
  }

  TEST(ImageTexture, MipmapSamplingBlendsGeneratedLevels) {
    std::vector<Colord> pixels;
    for (int y = 0; y != 4; ++y) {
      for (int x = 0; x != 4; ++x) {
        pixels.push_back(((x + y) % 2 == 0) ? Colord::black() : Colord::white());
      }
    }
    ImageTexture texture(new UVMapping2D, 4, 4, pixels, ImageTextureFilter::Mipmap);

    ASSERT_EQ(3, texture.mipLevelCount());
    expectColorNear(Colord(0.5, 0.5, 0.5),
                    texture.sample(0.3, 0.7, Vector2d(1.0, 0.0), Vector2d(0.0, 0.0)));
  }

  TEST(ImageTexture, EvaluateUsesConfiguredMapping) {
    ImageTexture texture(new UVMapping2D(2.0, 1.0), 2, 2, quadPixels(), ImageTextureFilter::Nearest,
                         ImageTextureWrap::Repeat);
    const HitPoint hit(nullptr, 0.0, Vector4d::null, Vector3d::null, Vector2d(0.6, 0.1));

    EXPECT_EQ(Colord::red(), texture.evaluate(Rayd::undefined, hit));
  }
}
