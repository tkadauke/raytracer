#include <gtest/gtest.h>

#include "core/Buffer.h"
#include "core/geometry/Mesh.h"
#include "engine/wireframe/Wireframe.h"
#include "render/primitives/Box.h"
#include "render/primitives/Curve.h"
#include "render/primitives/Instance.h"
#include "render/primitives/Primitive.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "test/helpers/BufferTestHelper.h"
#include "test/helpers/CameraTestHelper.h"

#include <memory>

namespace WireframeTest {
  using namespace render;
  using namespace engine::wireframe;
  using test::helpers::angledCamera;
  using test::helpers::countPixels;
  using test::helpers::headOnCamera;

  class NearPlaneTrianglePrimitive : public Primitive {
  public:
    const Primitive* intersect(const Rayd&, HitPointInterval&, render::State&) const override {
      return nullptr;
    }

    std::shared_ptr<Mesh> tessellate(int = 0) const override {
      auto mesh = std::make_shared<Mesh>();
      mesh->addVertex(Vector3d(-1, 0, -6), Vector3d::forward());
      mesh->addVertex(Vector3d(0, 0, 0), Vector3d::forward());
      mesh->addVertex(Vector3d(1, 0, -6), Vector3d::forward());
      mesh->addFace({0, 1, 2});
      return mesh;
    }

  protected:
    BoundingBoxd calculateBoundingBox() const override {
      return BoundingBoxd(Vector3d(-1, 0, -6), Vector3d(1, 0, 0));
    }
  };

  static std::shared_ptr<render::Scene> sceneWithBox() {
    auto scene = std::make_shared<Scene>(Colord::black());
    scene->add(std::make_shared<Box>(Vector3d::null, Vector3d(1, 1, 1)));
    return scene;
  }

  TEST(Wireframe, EmptySceneRendersBackground) {
    auto scene = std::make_shared<Scene>(Colord::black());
    Wireframe engine(angledCamera(), scene);
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    // 64×64 = 4096 pixels, all background.
    EXPECT_EQ(64 * 64, countPixels(buffer, Colord::black()));
  }

  TEST(Wireframe, SceneWithBoxProducesEdgePixels) {
    Wireframe engine(angledCamera(), sceneWithBox());
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
    EXPECT_GT(black, white);             // most pixels still background
    EXPECT_EQ(128 * 128, white + black); // every pixel is one or the other
  }

  TEST(Wireframe, SceneWithCurveRibbonProducesEdgePixels) {
    auto scene = std::make_shared<Scene>(Colord::black());
    scene->add(std::make_shared<Curve>(
      core::Polyline({Vector3d(-1.0, -0.5, 0.0), Vector3d(0.0, 0.5, 0.0),
                      Vector3d(1.0, -0.5, 0.0)}),
      0.25, Curve::TessellationMode::Ribbon));
    Wireframe engine(headOnCamera(), scene);
    Buffer<Colord> buffer(128, 128);

    engine.render(buffer);

    EXPECT_GT(countPixels(buffer, Colord::white()), 0);
  }

  TEST(Wireframe, CurveOverlayModeDrawsZeroWidthCurve) {
    auto scene = std::make_shared<Scene>(Colord::black());
    scene->add(std::make_shared<Curve>(
      core::Polyline({Vector3d(-1.0, -0.5, 0.0), Vector3d(0.0, 0.5, 0.0),
                      Vector3d(1.0, -0.5, 0.0)}),
      0.0, Curve::TessellationMode::Ribbon));
    Wireframe engine(headOnCamera(), scene);
    engine.setGeometryMode(Wireframe::GeometryMode::CurveOverlay);
    Buffer<Colord> buffer(128, 128);

    engine.render(buffer);

    EXPECT_GT(countPixels(buffer, Colord::white()), 0);
  }

  TEST(Wireframe, TessellatedModeDoesNotDrawZeroWidthCurve) {
    auto scene = std::make_shared<Scene>(Colord::black());
    scene->add(std::make_shared<Curve>(
      core::Polyline({Vector3d(-1.0, -0.5, 0.0), Vector3d(1.0, 0.5, 0.0)}), 0.0,
      Curve::TessellationMode::Ribbon));
    Wireframe engine(headOnCamera(), scene);
    Buffer<Colord> buffer(128, 128);

    engine.render(buffer);

    EXPECT_EQ(0, countPixels(buffer, Colord::white()));
  }

  TEST(Wireframe, CurveOverlayModeAppliesInstanceTransform) {
    auto scene = std::make_shared<Scene>(Colord::black());
    auto curve = std::make_shared<Curve>(
      core::Polyline({Vector3d(-0.5, -0.5, 0.0), Vector3d(0.5, -0.5, 0.0)}), 0.0,
      Curve::TessellationMode::Ribbon);
    auto instance = std::make_shared<Instance>(curve);
    Matrix4d matrix;
    matrix.setCell(1, 3, 1.0);
    instance->setMatrix(matrix);
    scene->add(instance);
    Wireframe engine(headOnCamera(), scene);
    engine.setGeometryMode(Wireframe::GeometryMode::CurveOverlay);
    Buffer<Colord> buffer(128, 128);

    engine.render(buffer);

    EXPECT_GT(countPixels(buffer, Colord::white()), 0);
  }

  TEST(Wireframe, BackgroundColorIsConfigurable) {
    auto scene = std::make_shared<Scene>(Colord::black());
    Wireframe engine(angledCamera(), scene);
    engine.setBackgroundColor(Colord(0.2, 0.3, 0.4));
    Buffer<Colord> buffer(32, 32);

    engine.render(buffer);

    EXPECT_EQ(32 * 32, countPixels(buffer, Colord(0.2, 0.3, 0.4)));
  }

  TEST(Wireframe, EdgeColorIsConfigurable) {
    Wireframe engine(angledCamera(), sceneWithBox());
    engine.setEdgeColor(Colord(1.0, 0.0, 0.5));
    Buffer<Colord> buffer(128, 128);

    engine.render(buffer);

    // Some pixels in the chosen edge colour, none white.
    EXPECT_GT(countPixels(buffer, Colord(1.0, 0.0, 0.5)), 0);
    EXPECT_EQ(0, countPixels(buffer, Colord::white()));
  }

  TEST(Wireframe, NearClipDepthIsConfigurable) {
    Wireframe engine(angledCamera(), sceneWithBox());

    EXPECT_DOUBLE_EQ(0.1, engine.nearClipDepth());
    engine.setNearClipDepth(0.5);
    EXPECT_DOUBLE_EQ(0.5, engine.nearClipDepth());
    engine.setNearClipDepth(-1.0);
    EXPECT_DOUBLE_EQ(0.1, engine.nearClipDepth());
  }

  TEST(Wireframe, ClonePreservesNearClipDepth) {
    Wireframe engine(angledCamera(), sceneWithBox());
    engine.setNearClipDepth(0.5);

    auto clone = std::dynamic_pointer_cast<Wireframe>(engine.cloneForRender());

    ASSERT_TRUE(clone);
    EXPECT_DOUBLE_EQ(0.5, clone->nearClipDepth());
  }

  TEST(Wireframe, ClipsEdgesCrossingNearPlane) {
    auto scene = std::make_shared<Scene>(Colord::black());
    scene->add(std::make_shared<NearPlaneTrianglePrimitive>());
    Wireframe engine(headOnCamera(), scene);
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_GT(countPixels(buffer, Colord::white()), 0);
  }

  TEST(Wireframe, HigherLodProducesMoreEdges) {
    auto scene = std::make_shared<Scene>(Colord::black());
    // Use a Sphere primitive — UV sphere quad count scales 4× per LOD step.
    // (Box is LOD-invariant, so wouldn't show the difference.)
    auto sphere = std::make_shared<Sphere>(Vector3d::null, 1.0);
    scene->add(sphere);

    Wireframe engineLow(angledCamera(), scene);
    engineLow.setLod(0);
    Buffer<Colord> bufferLow(256, 256);
    engineLow.render(bufferLow);
    int edgeLow = countPixels(bufferLow, Colord::white());

    Wireframe engineHigh(angledCamera(), scene);
    engineHigh.setLod(2);
    Buffer<Colord> bufferHigh(256, 256);
    engineHigh.render(bufferHigh);
    int edgeHigh = countPixels(bufferHigh, Colord::white());

    EXPECT_GT(edgeHigh, edgeLow);
  }

  TEST(Wireframe, HandlesNullSceneGracefully) {
    Wireframe engine(angledCamera(), nullptr);
    engine.setBackgroundColor(Colord(0.1, 0.1, 0.1));
    Buffer<Colord> buffer(32, 32);

    engine.render(buffer);

    // Background still cleared even with no scene.
    EXPECT_EQ(32 * 32, countPixels(buffer, Colord(0.1, 0.1, 0.1)));
  }

  TEST(Wireframe, CancelStopsFurtherDrawing) {
    Wireframe engine(angledCamera(), sceneWithBox());
    Buffer<Colord> buffer(64, 64);

    // Pre-cancel — the render loop checks the flag at face boundaries
    // and exits early. Background still gets cleared (the clear runs
    // before the draw loop), but no edges are drawn.
    engine.cancel();
    engine.render(buffer);

    EXPECT_EQ(0, countPixels(buffer, Colord::white()));
    EXPECT_EQ(64 * 64, countPixels(buffer, Colord::black()));
  }

  TEST(Wireframe, UncancelAllowsSubsequentRender) {
    Wireframe engine(angledCamera(), sceneWithBox());
    Buffer<Colord> buffer(64, 64);

    engine.cancel();
    engine.uncancel();
    engine.render(buffer);

    EXPECT_GT(countPixels(buffer, Colord::white()), 0);
  }
}
