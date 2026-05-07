#include <gtest/gtest.h>

#include "core/Buffer.h"
#include "engine/raster/Rasterizer.h"
#include "render/cameras/PinholeCamera.h"
#include "render/primitives/Box.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"

#include <memory>

namespace RasterizerTest {
  using namespace ::testing;
  using namespace render;
  using namespace engine::raster;

  // Counter helper — total pixels in the buffer matching `color`.
  static int countPixels(const Buffer<Colord>& buffer, const Colord& color) {
    int count = 0;
    for (int y = 0; y < buffer.height(); ++y)
      for (int x = 0; x < buffer.width(); ++x)
        if (buffer[y][x] == color) ++count;
    return count;
  }

  // Counter helper — total pixels in the buffer NOT matching the
  // background colour. The V1 rasterizer paints each face a hash
  // colour; we don't predict the colour, only that something painted
  // *some* pixels.
  static int countNonBackground(const Buffer<Colord>& buffer, const Colord& bg) {
    int count = 0;
    for (int y = 0; y < buffer.height(); ++y)
      for (int x = 0; x < buffer.width(); ++x)
        if (!(buffer[y][x] == bg)) ++count;
    return count;
  }

  static std::shared_ptr<Scene> sceneWithBox() {
    auto scene = std::make_shared<Scene>(Colord::black());
    scene->add(std::make_shared<Box>(Vector3d::null(), Vector3d(1, 1, 1)));
    return scene;
  }

  static std::shared_ptr<Scene> sceneWithSphere() {
    auto scene = std::make_shared<Scene>(Colord::black());
    scene->add(std::make_shared<Sphere>(Vector3d::null(), 1));
    return scene;
  }

  static std::shared_ptr<PinholeCamera> camera() {
    return std::make_shared<PinholeCamera>(Vector3d(2, 2, -5), Vector3d::null());
  }

  TEST(Rasterizer, EmptySceneRendersBackgroundOnly) {
    auto scene = std::make_shared<Scene>(Colord::black());
    Rasterizer engine(camera(), scene);
    engine.setBackgroundColor(Colord(0.1, 0.2, 0.3));
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_EQ(64 * 64, countPixels(buffer, Colord(0.1, 0.2, 0.3)));
  }

  TEST(Rasterizer, SceneWithBoxFillsSomePixels) {
    Rasterizer engine(camera(), sceneWithBox());
    Buffer<Colord> buffer(128, 128);
    engine.render(buffer);

    const int filled = countNonBackground(buffer, Colord::black());
    EXPECT_GT(filled, 0);
    // Filled region should be substantially larger than the
    // wireframe-engine output for the same scene — a filled box
    // covers a hexagonal area, not just edges.
    EXPECT_GT(filled, 100);
  }

  TEST(Rasterizer, SceneWithSphereFillsManyMorePixelsThanWireframeOutline) {
    // The sphere's tessellated mesh has ~256 triangles at lod=0;
    // a filled rasterizer covers the silhouette disk while a
    // wireframe draws only edges. We don't compare against
    // Wireframe directly here (that would couple the test to two
    // engines) — just assert the filled count is "large", indicating
    // the interior is being filled.
    Rasterizer engine(camera(), sceneWithSphere());
    Buffer<Colord> buffer(128, 128);
    engine.render(buffer);

    const int filled = countNonBackground(buffer, Colord::black());
    // 128×128 framebuffer with a unit sphere viewed from (2,2,-5)
    // produces a small silhouette disk (≈π·8² = 200 pixels). The
    // claim is "the interior is filled" — much larger than what an
    // outline-only renderer would produce for the same projection.
    EXPECT_GT(filled, 150);
  }

  TEST(Rasterizer, BackgroundColorIsConfigurable) {
    auto scene = std::make_shared<Scene>(Colord::black());
    Rasterizer engine(camera(), scene);
    engine.setBackgroundColor(Colord(0.5, 0.0, 0.5));
    Buffer<Colord> buffer(32, 32);

    engine.render(buffer);

    EXPECT_EQ(32 * 32, countPixels(buffer, Colord(0.5, 0.0, 0.5)));
  }

  TEST(Rasterizer, HigherLodProducesMoreOrEqualFilledPixels) {
    // For a Sphere primitive, higher LOD means denser triangulation
    // — but the silhouette area is bounded by the sphere's actual
    // projected size, so the filled region grows toward but doesn't
    // exceed that bound. Looser invariant: high-LOD render fills at
    // least as many pixels as low-LOD (V1 has no z-buffer so this
    // is monotonic in practice; not a strict mathematical claim).
    auto scene = sceneWithSphere();

    Rasterizer engineLow(camera(), scene);
    engineLow.setLod(0);
    Buffer<Colord> bufferLow(256, 256);
    engineLow.render(bufferLow);
    const int filledLow = countNonBackground(bufferLow, Colord::black());

    Rasterizer engineHigh(camera(), scene);
    engineHigh.setLod(2);
    Buffer<Colord> bufferHigh(256, 256);
    engineHigh.render(bufferHigh);
    const int filledHigh = countNonBackground(bufferHigh, Colord::black());

    EXPECT_GE(filledHigh, filledLow);
  }

  TEST(Rasterizer, HandlesNullSceneGracefully) {
    Rasterizer engine(camera(), nullptr);
    engine.setBackgroundColor(Colord(0.1, 0.1, 0.1));
    Buffer<Colord> buffer(32, 32);

    engine.render(buffer);

    EXPECT_EQ(32 * 32, countPixels(buffer, Colord(0.1, 0.1, 0.1)));
  }

  TEST(Rasterizer, CancelStopsFurtherDrawing) {
    Rasterizer engine(camera(), sceneWithBox());
    Buffer<Colord> buffer(64, 64);

    engine.cancel();
    engine.render(buffer);

    // Background still cleared; no triangles drawn.
    EXPECT_EQ(64 * 64, countPixels(buffer, Colord::black()));
  }

  TEST(Rasterizer, UncancelAllowsSubsequentRender) {
    Rasterizer engine(camera(), sceneWithBox());
    Buffer<Colord> buffer(64, 64);

    engine.cancel();
    engine.uncancel();
    engine.render(buffer);

    EXPECT_GT(countNonBackground(buffer, Colord::black()), 0);
  }
}
