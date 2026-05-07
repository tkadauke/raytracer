#include <gtest/gtest.h>

#include "core/Buffer.h"
#include "render/WireframeEngine.h"
#include "render/cameras/PinholeCamera.h"
#include "render/primitives/Box.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"

#include <memory>

namespace WireframeEngineTest {
  using namespace raytracer;
using namespace render;

  // Helper: count pixels that match a given colour. Background
  // counting / edge counting both go through this.
  static int countPixels(const Buffer<Colord>& buffer, const Colord& color) {
    int count = 0;
    for (int y = 0; y < buffer.height(); ++y)
      for (int x = 0; x < buffer.width(); ++x)
        if (buffer[y][x] == color) ++count;
    return count;
  }

  // Helper: build a scene with one centered axis-aligned box.
  static std::shared_ptr<render::Scene> sceneWithBox() {
    auto scene = std::make_shared<Scene>(Colord::black());
    scene->add(std::make_shared<Box>(Vector3d::null(), Vector3d(1, 1, 1)));
    return scene;
  }

  // Helper: a pinhole camera positioned to see the centred box from
  // outside it. Eye at (2, 2, -5) looking at origin; the box
  // straddles origin so it's safely in front of the camera.
  static std::shared_ptr<PinholeCamera> camera() {
    return std::make_shared<PinholeCamera>(Vector3d(2, 2, -5), Vector3d::null());
  }

  TEST(WireframeEngine, EmptySceneRendersBackground) {
    auto scene = std::make_shared<Scene>(Colord::black());
    WireframeEngine engine(camera(), scene);
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    // 64×64 = 4096 pixels, all background.
    EXPECT_EQ(64 * 64, countPixels(buffer, Colord::black()));
  }

  TEST(WireframeEngine, SceneWithBoxProducesEdgePixels) {
    WireframeEngine engine(camera(), sceneWithBox());
    Buffer<Colord> buffer(128, 128);

    engine.render(buffer);

    // Box at lod=0 produces 6 quads × 4 edges = 24 edges projecting
    // to non-zero pixel counts. The exact number depends on
    // projection geometry, so we just check that some edges were
    // drawn — meaning at least one pixel is white, and most are
    // still background.
    int white = countPixels(buffer, Colord::white());
    int black = countPixels(buffer, Colord::black());
    EXPECT_GT(white, 0);
    EXPECT_GT(black, white);  // most pixels still background
    EXPECT_EQ(128 * 128, white + black);  // every pixel is one or the other
  }

  TEST(WireframeEngine, BackgroundColorIsConfigurable) {
    auto scene = std::make_shared<Scene>(Colord::black());
    WireframeEngine engine(camera(), scene);
    engine.setBackgroundColor(Colord(0.2, 0.3, 0.4));
    Buffer<Colord> buffer(32, 32);

    engine.render(buffer);

    EXPECT_EQ(32 * 32, countPixels(buffer, Colord(0.2, 0.3, 0.4)));
  }

  TEST(WireframeEngine, EdgeColorIsConfigurable) {
    WireframeEngine engine(camera(), sceneWithBox());
    engine.setEdgeColor(Colord(1.0, 0.0, 0.5));
    Buffer<Colord> buffer(128, 128);

    engine.render(buffer);

    // Some pixels in the chosen edge colour, none white.
    EXPECT_GT(countPixels(buffer, Colord(1.0, 0.0, 0.5)), 0);
    EXPECT_EQ(0, countPixels(buffer, Colord::white()));
  }

  TEST(WireframeEngine, HigherLodProducesMoreEdges) {
    auto scene = std::make_shared<Scene>(Colord::black());
    // Use a Sphere primitive — UV sphere quad count scales 4× per LOD step.
    // (Box is LOD-invariant, so wouldn't show the difference.)
    auto sphere = std::make_shared<Sphere>(Vector3d::null(), 1.0);
    scene->add(sphere);

    WireframeEngine engineLow(camera(), scene);
    engineLow.setLod(0);
    Buffer<Colord> bufferLow(256, 256);
    engineLow.render(bufferLow);
    int edgeLow = countPixels(bufferLow, Colord::white());

    WireframeEngine engineHigh(camera(), scene);
    engineHigh.setLod(2);
    Buffer<Colord> bufferHigh(256, 256);
    engineHigh.render(bufferHigh);
    int edgeHigh = countPixels(bufferHigh, Colord::white());

    EXPECT_GT(edgeHigh, edgeLow);
  }

  TEST(WireframeEngine, HandlesNullSceneGracefully) {
    WireframeEngine engine(camera(), nullptr);
    engine.setBackgroundColor(Colord(0.1, 0.1, 0.1));
    Buffer<Colord> buffer(32, 32);

    engine.render(buffer);

    // Background still cleared even with no scene.
    EXPECT_EQ(32 * 32, countPixels(buffer, Colord(0.1, 0.1, 0.1)));
  }

  TEST(WireframeEngine, CancelStopsFurtherDrawing) {
    WireframeEngine engine(camera(), sceneWithBox());
    Buffer<Colord> buffer(64, 64);

    // Pre-cancel — the render loop checks the flag at face boundaries
    // and exits early. Background still gets cleared (the clear runs
    // before the draw loop), but no edges are drawn.
    engine.cancel();
    engine.render(buffer);

    EXPECT_EQ(0, countPixels(buffer, Colord::white()));
    EXPECT_EQ(64 * 64, countPixels(buffer, Colord::black()));
  }

  TEST(WireframeEngine, UncancelAllowsSubsequentRender) {
    WireframeEngine engine(camera(), sceneWithBox());
    Buffer<Colord> buffer(64, 64);

    engine.cancel();
    engine.uncancel();
    engine.render(buffer);

    EXPECT_GT(countPixels(buffer, Colord::white()), 0);
  }
}
