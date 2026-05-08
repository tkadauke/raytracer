#include <gtest/gtest.h>

#include "core/Buffer.h"
#include "engine/raster/Rasterizer.h"
#include "render/cameras/PinholeCamera.h"
#include "render/primitives/Box.h"
#include "render/primitives/Rectangle.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "render/primitives/Triangle.h"

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
  // background colour. The rasterizer shades from material albedo
  // when available and falls back to a face hash otherwise; these
  // tests only need to know that something painted *some* pixels.
  static int countNonBackground(const Buffer<Colord>& buffer, const Colord& bg) {
    int count = 0;
    for (int y = 0; y < buffer.height(); ++y)
      for (int x = 0; x < buffer.width(); ++x)
        if (!(buffer[y][x] == bg)) ++count;
    return count;
  }

  static void expectBuffersEqual(const Buffer<Colord>& expected, const Buffer<Colord>& actual) {
    ASSERT_EQ(expected.width(), actual.width());
    ASSERT_EQ(expected.height(), actual.height());
    for (int y = 0; y < expected.height(); ++y)
      for (int x = 0; x < expected.width(); ++x)
        EXPECT_EQ(expected[y][x], actual[y][x]) << "at (" << x << ", " << y << ")";
  }

  static std::shared_ptr<Scene> sceneWithBox() {
    auto scene = std::make_shared<Scene>(Colord::white());
    scene->add(std::make_shared<Box>(Vector3d::null(), Vector3d(1, 1, 1)));
    return scene;
  }

  static std::shared_ptr<Scene> sceneWithSphere() {
    auto scene = std::make_shared<Scene>(Colord::white());
    scene->add(std::make_shared<Sphere>(Vector3d::null(), 1));
    return scene;
  }

  static std::shared_ptr<Scene> sceneWithOversizedRectangle() {
    auto scene = std::make_shared<Scene>(Colord::white());
    scene->add(std::make_shared<Rectangle>(
      Vector3d(-100, -100, 0),
      Vector3d( 200,    0, 0),
      Vector3d(   0,  200, 0)));
    return scene;
  }

  static std::shared_ptr<Scene> sceneWithBackFacingTriangle() {
    auto scene = std::make_shared<Scene>(Colord::white());
    scene->add(std::make_shared<Triangle>(
      Vector3d(-1, -1, 0),
      Vector3d( 1, -1, 0),
      Vector3d( 0,  1, 0)));
    return scene;
  }

  static std::shared_ptr<Scene> sceneWithFrontFacingTriangle() {
    auto scene = std::make_shared<Scene>(Colord::white());
    scene->add(std::make_shared<Triangle>(
      Vector3d(-1, -1, 0),
      Vector3d( 0,  1, 0),
      Vector3d( 1, -1, 0)));
    return scene;
  }

  static std::shared_ptr<PinholeCamera> camera() {
    return std::make_shared<PinholeCamera>(Vector3d(2, 2, -5), Vector3d::null());
  }

  static std::shared_ptr<PinholeCamera> headOnCamera() {
    return std::make_shared<PinholeCamera>(Vector3d(0, 0, -5), Vector3d::null());
  }

  TEST(Rasterizer, EmptySceneRendersBackgroundOnly) {
    auto scene = std::make_shared<Scene>(Colord::white());
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
    auto scene = std::make_shared<Scene>(Colord::white());
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
    // least as many pixels as low-LOD for this centred sphere; not a
    // strict mathematical claim for arbitrary scenes.
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

  TEST(Rasterizer, ZBufferCullsOccludedGeometryAddedAfterTheOccluder) {
    // Scene 1: just a near sphere.
    // Scene 2: the same near sphere added first (so its fallback
    //          face colours match scene 1), plus a large box
    //          rendered behind it. The box would overdraw the
    //          sphere's centre pixels without depth-testing. With
    //          the Z-buffer, the box's centre pixels fail the depth
    //          test and the sphere's colour stays.
    //
    // The sphere is added first in BOTH scenes, so its fallback face
    // colours are identical when no material is attached. That makes
    // pixel-equality at the centre a stable assertion — without the
    // depth test this expectation would fail.

    auto cam = std::make_shared<PinholeCamera>(Vector3d(0, 0, -8), Vector3d::null());

    auto sceneAlone = std::make_shared<Scene>(Colord::white());
    sceneAlone->add(std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0));

    auto sceneWithBack = std::make_shared<Scene>(Colord::white());
    sceneWithBack->add(std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0));    // near, idx 0..N-1
    sceneWithBack->add(std::make_shared<Box>(Vector3d(0, 0, 10), Vector3d(5, 5, 0.1)));  // far back wall

    Rasterizer eAlone(cam, sceneAlone);
    Rasterizer eWithBack(cam, sceneWithBack);

    Buffer<Colord> bAlone(64, 64);
    Buffer<Colord> bWithBack(64, 64);
    eAlone.render(bAlone);
    eWithBack.render(bWithBack);

    // Centre pixel: covered by the near sphere in both renders. With
    // the Z-buffer, the back box's pixels at the centre fail the
    // depth test; without it, the box would overdraw the sphere
    // there because it's added second in mesh order.
    EXPECT_EQ(bAlone[32][32], bWithBack[32][32])
      << "Centre pixel changed when an occluded back wall was added — "
      << "Z-buffer is failing to cull the farther geometry.";
    EXPECT_FALSE(bAlone[32][32] == Colord::black())
      << "Centre pixel should be coloured by the near sphere.";
  }

  TEST(Rasterizer, CullModeDefaultsToBothSides) {
    Rasterizer engine(headOnCamera(), sceneWithBackFacingTriangle());
    EXPECT_EQ(Rasterizer::CullMode::Both, engine.cullMode());

    Buffer<Colord> buffer(64, 64);
    engine.render(buffer);

    EXPECT_GT(countNonBackground(buffer, Colord::black()), 0);
  }

  TEST(Rasterizer, BackfaceCullingSkipsBackFacingTriangles) {
    Rasterizer engine(headOnCamera(), sceneWithBackFacingTriangle());
    engine.setCullMode(Rasterizer::CullMode::Back);
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_EQ(0, countNonBackground(buffer, Colord::black()));
  }

  TEST(Rasterizer, BackfaceCullingKeepsFrontFacingTriangles) {
    Rasterizer engine(headOnCamera(), sceneWithFrontFacingTriangle());
    engine.setCullMode(Rasterizer::CullMode::Back);
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_GT(countNonBackground(buffer, Colord::black()), 0);
  }

  TEST(Rasterizer, FrontfaceCullingSkipsFrontFacingTriangles) {
    Rasterizer engine(headOnCamera(), sceneWithFrontFacingTriangle());
    engine.setCullMode(Rasterizer::CullMode::Front);
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_EQ(0, countNonBackground(buffer, Colord::black()));
  }

  TEST(Rasterizer, BackfaceCullingAppliesToTiledPath) {
    Rasterizer engine(headOnCamera(), sceneWithBackFacingTriangle());
    engine.setCullMode(Rasterizer::CullMode::Back);
    engine.setMaximumThreads(2);
    engine.setQueueSize(4);
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_EQ(0, countNonBackground(buffer, Colord::black()));
  }

  TEST(Rasterizer, ViewportClippedGeometryFillsFramebuffer) {
    Rasterizer engine(headOnCamera(), sceneWithOversizedRectangle());
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_EQ(64 * 64, countNonBackground(buffer, Colord::black()));
  }

  TEST(Rasterizer, TiledRenderMatchesSingleTileRender) {
    auto cam = std::make_shared<PinholeCamera>(Vector3d(0, 0, -8), Vector3d::null());
    auto scene = std::make_shared<Scene>(Colord::white());
    scene->add(std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0));
    scene->add(std::make_shared<Box>(Vector3d(0, 0, 10), Vector3d(5, 5, 0.1)));

    Rasterizer singleTile(cam, scene);
    singleTile.setLod(2);
    singleTile.setMaximumThreads(1);
    singleTile.setQueueSize(1);

    Rasterizer tiled(cam, scene);
    tiled.setLod(2);
    tiled.setMaximumThreads(4);
    tiled.setQueueSize(16);

    Buffer<Colord> expected(128, 128);
    Buffer<Colord> actual(128, 128);
    singleTile.render(expected);
    tiled.render(actual);

    expectBuffersEqual(expected, actual);
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
