#include "test/functional/support/EngineFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "engine/raster/Rasterizer.h"
#include "render/cameras/PinholeCamera.h"
#include "render/lights/DirectionalLight.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/Box.h"
#include "render/primitives/Rectangle.h"
#include "render/primitives/Scene.h"
#include "render/textures/ConstantColorTexture.h"

#include <cmath>
#include <memory>
#include <string>

using namespace render;
using namespace testing;

namespace {
  class RasterizerFeatureTest : public EngineFeatureTest {
  public:
    std::shared_ptr<render::RenderEngine> createEngine() override {
      return createRasterizer(false);
    }

    void setLod(int lod) {
      m_lod = lod;
    }

    void setShadowMapSize(int size) {
      m_shadowMapSize = size;
    }

    void setShadowBias(double bias) {
      m_shadowBias = bias;
    }

    void setShadowFilterRadius(int radius) {
      m_shadowFilterRadius = radius;
    }

    int shadowFilterRadius() const {
      return m_shadowFilterRadius;
    }

    void renderColorSnapshot(bool shadowMapsEnabled, Buffer<Colord>& result) {
      createRasterizer(shadowMapsEnabled)->render(result);
    }

  private:
    std::shared_ptr<engine::raster::Rasterizer> createRasterizer(bool shadowMapsEnabled) {
      auto rasterizer = std::make_shared<engine::raster::Rasterizer>(camera(), m_scene);
      rasterizer->setLod(m_lod);
      rasterizer->setShadowMapsEnabled(shadowMapsEnabled);
      rasterizer->setShadowMapSize(m_shadowMapSize);
      rasterizer->setShadowBias(m_shadowBias);
      rasterizer->setShadowFilterRadius(m_shadowFilterRadius);
      return rasterizer;
    }

    int m_lod{3};
    int m_shadowMapSize{256};
    double m_shadowBias{0.1};
    int m_shadowFilterRadius{0};
  };

  std::shared_ptr<MatteMaterial> matte(const Colord& color) {
    return std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(color));
  }

  RasterizerFeatureTest* rasterizerTest(EngineFeatureTest* test) {
    auto* rasterizer = dynamic_cast<RasterizerFeatureTest*>(test);
    EXPECT_NE(nullptr, rasterizer)
      << "This step is only meaningful for RasterizerFeatureTest scenarios.";
    return rasterizer;
  }

  Colord colorAtWorldPoint(EngineFeatureTest* test, const Buffer<Colord>& buffer,
                           const Vector3d& point) {
    const Vector2d screen = test->camera()->projectPoint(point);
    EXPECT_TRUE(screen.isDefined()) << "world point should project into the camera view";
    if (screen.isUndefined())
      return Colord::black();

    const int x = static_cast<int>(std::lround(screen.x()));
    const int y = static_cast<int>(std::lround(screen.y()));
    EXPECT_GE(x, 0);
    EXPECT_LT(x, buffer.width());
    EXPECT_GE(y, 0);
    EXPECT_LT(y, buffer.height());
    if (x < 0 || x >= buffer.width() || y < 0 || y >= buffer.height())
      return Colord::black();

    return buffer[y][x];
  }

  int countPixelsBrightenedByFiltering(const Buffer<Colord>& hardShadow,
                                       const Buffer<Colord>& filteredShadow) {
    int count = 0;
    for (int y = 0; y < hardShadow.height(); ++y) {
      for (int x = 0; x < hardShadow.width(); ++x) {
        if (filteredShadow[y][x].r() > hardShadow[y][x].r() + 0.03)
          ++count;
      }
    }
    return count;
  }

  int countPixelsDarkenedByFiltering(const Buffer<Colord>& hardShadow,
                                     const Buffer<Colord>& filteredShadow) {
    int count = 0;
    for (int y = 0; y < hardShadow.height(); ++y) {
      for (int x = 0; x < hardShadow.width(); ++x) {
        if (hardShadow[y][x].r() > filteredShadow[y][x].r() + 0.03)
          ++count;
      }
    }
    return count;
  }
}

GIVEN(EngineFeatureTest, "a rasterizer directional shadow scene") {
  ASSERT_NE(nullptr, rasterizerTest(test));

  test->scene()->setAmbient(Colord(0.1, 0.1, 0.1));
  test->scene()->setBackground(Colord::black());

  auto wall = std::make_shared<Rectangle>(Vector3d(-2.0, -2.0, 1.0), Vector3d(0.0, 4.0, 0.0),
                                          Vector3d(4.0, 0.0, 0.0));
  wall->setMaterial(matte(Colord::white()));
  test->add(wall);

  auto caster = std::make_shared<Box>(Vector3d(0.0, 0.0, 0.0), Vector3d(0.35, 0.35, 0.35));
  caster->setMaterial(matte(Colord::white()));
  test->add(caster);

  test->scene()->addLight(
    std::make_shared<DirectionalLight>(Vector3d(-0.5, 0.2, -1.0), Colord::white()));
}

GIVEN(EngineFeatureTest, "a rasterizer lod of ([0-9]+)") {
  if (auto* rasterizer = rasterizerTest(test)) {
    rasterizer->setLod(std::stoi(match[1]));
  }
}

GIVEN(EngineFeatureTest, "a rasterizer shadow map size of ([0-9]+)") {
  if (auto* rasterizer = rasterizerTest(test)) {
    rasterizer->setShadowMapSize(std::stoi(match[1]));
  }
}

GIVEN(EngineFeatureTest, "a rasterizer shadow bias of ([\\d.]+)") {
  if (auto* rasterizer = rasterizerTest(test)) {
    rasterizer->setShadowBias(std::stod(match[1]));
  }
}

GIVEN(EngineFeatureTest, "a rasterizer shadow filter radius of ([0-9]+)") {
  if (auto* rasterizer = rasterizerTest(test)) {
    rasterizer->setShadowFilterRadius(std::stoi(match[1]));
  }
}

WHEN(EngineFeatureTest, "i look at the rasterizer shadow receiver") {
  test->setCamera(
    std::make_shared<PinholeCamera>(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.5)));
}

THEN(EngineFeatureTest, "the rasterizer shadow map darkens the occluded receiver") {
  auto* rasterizer = rasterizerTest(test);
  ASSERT_NE(nullptr, rasterizer);

  Buffer<Colord> direct(test->buffer().width(), test->buffer().height());
  Buffer<Colord> shadowed(test->buffer().width(), test->buffer().height());
  rasterizer->renderColorSnapshot(false, direct);
  rasterizer->renderColorSnapshot(true, shadowed);

  const Colord directOccluded = colorAtWorldPoint(test, direct, Vector3d(0.6, 0.0, 1.0));
  const Colord shadowedOccluded = colorAtWorldPoint(test, shadowed, Vector3d(0.6, 0.0, 1.0));
  const Colord shadowedLit = colorAtWorldPoint(test, shadowed, Vector3d(-1.2, 0.0, 1.0));

  EXPECT_GT(directOccluded.r(), shadowedOccluded.r() + 0.4);
  EXPECT_GT(shadowedLit.r(), shadowedOccluded.r() + 0.4);
  EXPECT_LT(shadowedOccluded.r(), 0.35);
}

THEN(EngineFeatureTest, "the rasterizer shadow edge is percentage closer filtered") {
  auto* rasterizer = rasterizerTest(test);
  ASSERT_NE(nullptr, rasterizer);

  Buffer<Colord> hard(test->buffer().width(), test->buffer().height());
  Buffer<Colord> filtered(test->buffer().width(), test->buffer().height());
  const int filterRadius = rasterizer->shadowFilterRadius();
  ASSERT_GT(filterRadius, 0);

  rasterizer->setShadowFilterRadius(0);
  rasterizer->renderColorSnapshot(true, hard);
  rasterizer->setShadowFilterRadius(filterRadius);
  rasterizer->renderColorSnapshot(true, filtered);

  EXPECT_GT(countPixelsBrightenedByFiltering(hard, filtered), 0);
  EXPECT_GT(countPixelsDarkenedByFiltering(hard, filtered), 0);
}

namespace RasterizerFunctionalTest {
  struct RasterizerTest : public RasterizerFeatureTest {};

  TEST_F(RasterizerTest, DirectionalShadowMapsDarkenOccludedDiffuseLight) {
    given("a rasterizer directional shadow scene");
    given("a rasterizer lod of 3");
    given("a rasterizer shadow map size of 256");
    given("a rasterizer shadow bias of 0.1");
    when("i look at the rasterizer shadow receiver");
    then("the rasterizer shadow map darkens the occluded receiver");
  }

  TEST_F(RasterizerTest, DirectionalShadowMapsSupportPercentageCloserFiltering) {
    given("a rasterizer directional shadow scene");
    given("a rasterizer lod of 3");
    given("a rasterizer shadow map size of 64");
    given("a rasterizer shadow bias of 0.1");
    given("a rasterizer shadow filter radius of 2");
    when("i look at the rasterizer shadow receiver");
    then("the rasterizer shadow edge is percentage closer filtered");
  }

}
