#include <gtest/gtest.h>

#include "core/Buffer.h"
#include "core/math/HitPoint.h"
#include "engine/raster/Rasterizer.h"
#include "src/engine/raster/RasterShadowMaps.h"
#include "render/cameras/OrthographicCamera.h"
#include "render/cameras/PinholeCamera.h"
#include "render/lights/DirectionalLight.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/Box.h"
#include "render/primitives/Rectangle.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "render/primitives/Triangle.h"
#include "render/textures/ConstantColorTexture.h"
#include "render/textures/Texture.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

namespace RasterizerTest {
  using namespace ::testing;
  using namespace render;
  using namespace engine::raster;

  class UVColorTexture : public Texturec {
  public:
    virtual Colord evaluate(const Rayd&, const HitPoint& hitPoint) const {
      return Colord(hitPoint.uv().x(), hitPoint.uv().y(), 0.0);
    }
  };

  class OverridingConstantColorTexture : public ConstantColorTexture {
  public:
    OverridingConstantColorTexture()
        : ConstantColorTexture(Colord::red()) {
    }

    Colord evaluate(const Rayd&, const HitPoint& hitPoint) const override {
      return Colord(hitPoint.uv().x(), hitPoint.uv().y(), 0.0);
    }
  };

  struct TrackedTriangleScene {
    std::shared_ptr<Scene> scene;
    std::shared_ptr<Triangle> triangle;
    std::shared_ptr<MatteMaterial> material;
  };

  // Counter helper — total pixels in the buffer matching `color`.
  static int countPixels(const Buffer<Colord>& buffer, const Colord& color) {
    int count = 0;
    for (int y = 0; y < buffer.height(); ++y)
      for (int x = 0; x < buffer.width(); ++x)
        if (buffer[y][x] == color)
          ++count;
    return count;
  }

  // Counter helper — total pixels in the buffer NOT matching the
  // background color. The rasterizer shades from material albedo
  // when available and falls back to a face hash otherwise; these
  // tests only need to know that something painted *some* pixels.
  static int countNonBackground(const Buffer<Colord>& buffer, const Colord& bg) {
    int count = 0;
    for (int y = 0; y < buffer.height(); ++y)
      for (int x = 0; x < buffer.width(); ++x)
        if (!(buffer[y][x] == bg))
          ++count;
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
    scene->add(std::make_shared<Box>(Vector3d::null, Vector3d(1, 1, 1)));
    return scene;
  }

  static std::shared_ptr<Scene> sceneWithSphere() {
    auto scene = std::make_shared<Scene>(Colord::white());
    scene->add(std::make_shared<Sphere>(Vector3d::null, 1));
    return scene;
  }

  static std::shared_ptr<Scene> sceneWithOversizedRectangle() {
    auto scene = std::make_shared<Scene>(Colord::white());
    scene->add(std::make_shared<Rectangle>(Vector3d(-100, -100, 0), Vector3d(200, 0, 0),
                                           Vector3d(0, 200, 0)));
    return scene;
  }

  static std::shared_ptr<Scene> sceneWithBackFacingTriangle() {
    auto scene = std::make_shared<Scene>(Colord::white());
    scene->add(
      std::make_shared<Triangle>(Vector3d(-1, -1, 0), Vector3d(1, -1, 0), Vector3d(0, 1, 0)));
    return scene;
  }

  static std::shared_ptr<Scene> sceneWithFrontFacingTriangle() {
    auto scene = std::make_shared<Scene>(Colord::white());
    scene->add(
      std::make_shared<Triangle>(Vector3d(-1, -1, 0), Vector3d(0, 1, 0), Vector3d(1, -1, 0)));
    return scene;
  }

  static TrackedTriangleScene sceneWithTrackedFrontFacingTriangle() {
    TrackedTriangleScene result;
    result.scene = std::make_shared<Scene>(Colord::white());
    result.triangle =
      std::make_shared<Triangle>(Vector3d(-1, -1, 0), Vector3d(0, 1, 0), Vector3d(1, -1, 0));
    result.material = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)));
    result.triangle->setMaterial(result.material);
    result.scene->add(result.triangle);
    return result;
  }

  static std::shared_ptr<Scene>
  sceneWithTexturedFrontFacingTriangle(std::shared_ptr<Texturec> texture) {
    auto scene = std::make_shared<Scene>(Colord::white());
    auto triangle =
      std::make_shared<Triangle>(Vector3d(-1, -1, 0), Vector3d(0, 1, 0), Vector3d(1, -1, 0));
    triangle->setMaterial(std::make_shared<MatteMaterial>(std::move(texture)));
    scene->add(triangle);
    return scene;
  }

  static std::shared_ptr<Scene> sceneWithTexturedFrontFacingTriangle() {
    return sceneWithTexturedFrontFacingTriangle(std::make_shared<UVColorTexture>());
  }

  static std::shared_ptr<Scene> sceneWithSlopedTriangle() {
    auto scene = std::make_shared<Scene>(Colord::black());
    scene->add(
      std::make_shared<Triangle>(Vector3d(-2, -2, 0), Vector3d(2, -2, 2), Vector3d(0, 2, 4)));
    return scene;
  }

  static void expectCenterLooksLikeTriangleUV(const Colord& color) {
    EXPECT_GT(color.r(), 0.35);
    EXPECT_LT(color.r(), 0.65);
    EXPECT_GT(color.g(), 0.15);
    EXPECT_LT(color.g(), 0.35);
    EXPECT_DOUBLE_EQ(0.0, color.b());
  }

  static void configureScreenSpaceEdgeTriangle(Rasterizer& engine) {
    engine.setVertexShader([](const Rasterizer::VertexInput& vertex) {
      Vector3d screen(32, 16, 1);
      if (vertex.worldPosition.x() < -0.5) {
        screen = Vector3d(16, 16, 1);
      } else if (vertex.worldPosition.x() > 0.5) {
        screen = Vector3d(16, 32, 1);
      }
      return Rasterizer::VertexOutput{vertex.worldPosition, vertex.normal, vertex.uv,
                                      vertex.clipPosition, screen};
    });
    engine.setFragmentShader([](const Rasterizer::FragmentInput&) { return Colord::white(); });
  }

  static void configureScreenSpaceQuad(Rasterizer& engine) {
    engine.setVertexShader([](const Rasterizer::VertexInput& vertex) {
      const double x = vertex.worldPosition.x() < 0.0 ? 8.0 : 24.0;
      const double y = vertex.worldPosition.y() < 0.0 ? 8.0 : 24.0;
      return Rasterizer::VertexOutput{vertex.worldPosition, vertex.normal, vertex.uv,
                                      vertex.clipPosition, Vector3d(x, y, 1.0)};
    });
  }

  static void configureScreenSpaceSubpixelTriangle(Rasterizer& engine, double rightX) {
    engine.setVertexShader([=](const Rasterizer::VertexInput& vertex) {
      Vector3d screen(8.0, 24.0, 1.0);
      if (vertex.worldPosition.x() < -0.5) {
        screen = Vector3d(8.0, 8.0, 1.0);
      } else if (vertex.worldPosition.x() > 0.5) {
        screen = Vector3d(rightX, 8.0, 1.0);
      }
      return Rasterizer::VertexOutput{vertex.worldPosition, vertex.normal, vertex.uv,
                                      vertex.clipPosition, screen};
    });
    engine.setFragmentShader([](const Rasterizer::FragmentInput&) { return Colord::white(); });
  }

  static void configureScreenSpaceMSAASubpixelTriangle(Rasterizer& engine) {
    engine.setVertexShader([](const Rasterizer::VertexInput& vertex) {
      Vector3d screen(16.0, 32.0, 1.0);
      if (vertex.worldPosition.x() < -0.5) {
        screen = Vector3d(16.0, 0.0, 1.0);
      } else if (vertex.worldPosition.x() > 0.5) {
        screen = Vector3d(32.0, 0.0, 1.0);
      }
      return Rasterizer::VertexOutput{vertex.worldPosition, vertex.normal, vertex.uv,
                                      vertex.clipPosition, screen};
    });
    engine.setFragmentShader([](const Rasterizer::FragmentInput&) { return Colord::white(); });
  }

  static std::shared_ptr<Scene> sceneWithOverlappingTriangles() {
    auto scene = std::make_shared<Scene>(Colord::white());
    scene->add(
      std::make_shared<Triangle>(Vector3d(-1, -1, 0), Vector3d(0, 1, 0), Vector3d(1, -1, 0)));
    scene->add(
      std::make_shared<Triangle>(Vector3d(-1, -1, 1), Vector3d(0, 1, 1), Vector3d(1, -1, 1)));
    return scene;
  }

  static std::shared_ptr<Scene> sceneWithDuplicateTriangles() {
    auto scene = std::make_shared<Scene>(Colord::white());
    scene->add(
      std::make_shared<Triangle>(Vector3d(-1, -1, 0), Vector3d(0, 1, 0), Vector3d(1, -1, 0)));
    scene->add(
      std::make_shared<Triangle>(Vector3d(-1, -1, 0), Vector3d(0, 1, 0), Vector3d(1, -1, 0)));
    return scene;
  }

  static std::shared_ptr<Scene> sceneWithAdjacentQuadTriangles() {
    auto scene = std::make_shared<Scene>(Colord::white());
    const Vector3d p00(-1.0, -1.0, 0.0);
    const Vector3d p10(1.0, -1.0, 0.0);
    const Vector3d p01(-1.0, 1.0, 0.0);
    const Vector3d p11(1.0, 1.0, 0.0);
    scene->add(std::make_shared<Triangle>(p00, p10, p01));
    scene->add(std::make_shared<Triangle>(p10, p11, p01));
    return scene;
  }

  static std::shared_ptr<PinholeCamera> camera() {
    return std::make_shared<PinholeCamera>(Vector3d(2, 2, -5), Vector3d::null);
  }

  static std::shared_ptr<PinholeCamera> headOnCamera() {
    return std::make_shared<PinholeCamera>(Vector3d(0, 0, -5), Vector3d::null);
  }

  static std::shared_ptr<MatteMaterial> matte(const Colord& color) {
    return std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(color));
  }

  static std::shared_ptr<Scene> sceneWithDirectionalShadowCaster() {
    auto scene = std::make_shared<Scene>(Colord(0.1, 0.1, 0.1));

    auto wall = std::make_shared<Rectangle>(Vector3d(-2.0, -2.0, 1.0), Vector3d(0.0, 4.0, 0.0),
                                            Vector3d(4.0, 0.0, 0.0));
    wall->setMaterial(matte(Colord::white()));
    scene->add(wall);

    auto caster = std::make_shared<Box>(Vector3d(0.0, 0.0, 0.0), Vector3d(0.35, 0.35, 0.35));
    caster->setMaterial(matte(Colord::white()));
    scene->add(caster);

    scene->addLight(std::make_shared<DirectionalLight>(Vector3d(-0.5, 0.2, -1.0), Colord::white()));
    return scene;
  }

  static Colord colorAtWorldPoint(const Buffer<Colord>& buffer,
                                  const std::shared_ptr<PinholeCamera>& cam,
                                  const Vector3d& point) {
    const Vector2d screen = cam->projectPoint(point);
    EXPECT_TRUE(screen.isDefined());
    if (screen.isUndefined())
      return Colord::black();
    const int x = static_cast<int>(std::lround(screen.x()));
    const int y = static_cast<int>(std::lround(screen.y()));
    EXPECT_GE(x, 0);
    EXPECT_GE(y, 0);
    EXPECT_LT(x, buffer.width());
    EXPECT_LT(y, buffer.height());
    if (x < 0 || y < 0 || x >= buffer.width() || y >= buffer.height())
      return Colord::black();
    return buffer[y][x];
  }

  static int countPixelsBrightenedByFiltering(const Buffer<Colord>& hardShadow,
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

  static int countPixelsDarkenedByFiltering(const Buffer<Colord>& hardShadow,
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

  static engine::raster::detail::DirectionalShadowMap syntheticShadowMap(double constantBias,
                                                                         double slopeBias) {
    const Vector3d lightDirection(0.0, 0.0, -1.0);
    auto shadowCamera = std::make_shared<engine::raster::detail::DirectionalShadowCamera>(
      Vector3d::null, lightDirection, 1.0);
    shadowCamera->setViewPlane(std::make_shared<ViewPlane>());
    shadowCamera->viewPlane()->setup(Matrix4d(), Recti(4, 4));

    auto depthBuffer = std::make_unique<Buffer<double>>(4, 4);
    depthBuffer->clear(std::numeric_limits<double>::infinity());
    (*depthBuffer)[2][2] = 1.95;

    std::vector<engine::raster::detail::DirectionalShadowCascade> cascades;
    cascades.push_back({std::move(shadowCamera), std::move(depthBuffer), 0.0, 1.0});
    return engine::raster::detail::DirectionalShadowMap(
      nullptr, nullptr, std::move(cascades), constantBias, slopeBias, 0,
      Rasterizer::ShadowFilterMode::PCF);
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
    // least as many pixels as low-LOD for this centered sphere; not a
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
    engine.setBackgroundColor(Colord::black());
    Buffer<Colord> buffer(64, 64);

    engine.cancel();
    engine.render(buffer);

    // Background still cleared; no triangles drawn.
    EXPECT_EQ(64 * 64, countPixels(buffer, Colord::black()));
  }

  TEST(Rasterizer, ZBufferCullsOccludedGeometryAddedAfterTheOccluder) {
    // Scene 1: just a near sphere.
    // Scene 2: the same near sphere added first (so its fallback
    //          face colors match scene 1), plus a large box
    //          rendered behind it. The box would overdraw the
    //          sphere's center pixels without depth-testing. With
    //          the Z-buffer, the box's center pixels fail the depth
    //          test and the sphere's color stays.
    //
    // The sphere is added first in BOTH scenes, so its fallback face
    // colors are identical when no material is attached. That makes
    // pixel-equality at the center a stable assertion — without the
    // depth test this expectation would fail.

    auto cam = std::make_shared<PinholeCamera>(Vector3d(0, 0, -8), Vector3d::null);

    auto sceneAlone = std::make_shared<Scene>(Colord::white());
    sceneAlone->add(std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0));

    auto sceneWithBack = std::make_shared<Scene>(Colord::white());
    sceneWithBack->add(std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0)); // near, idx 0..N-1
    sceneWithBack->add(
      std::make_shared<Box>(Vector3d(0, 0, 10), Vector3d(5, 5, 0.1))); // far back wall

    Rasterizer eAlone(cam, sceneAlone);
    Rasterizer eWithBack(cam, sceneWithBack);

    Buffer<Colord> bAlone(64, 64);
    Buffer<Colord> bWithBack(64, 64);
    eAlone.render(bAlone);
    eWithBack.render(bWithBack);

    // Center pixel: covered by the near sphere in both renders. With
    // the Z-buffer, the back box's pixels at the center fail the
    // depth test; without it, the box would overdraw the sphere
    // there because it's added second in mesh order.
    EXPECT_EQ(bAlone[32][32], bWithBack[32][32])
      << "Center pixel changed when an occluded back wall was added — "
      << "Z-buffer is failing to cull the farther geometry.";
    EXPECT_FALSE(bAlone[32][32] == Colord::black())
      << "Center pixel should be colored by the near sphere.";
  }

  TEST(Rasterizer, DepthStencilAndShaderDefaultsMatchFixedPipeline) {
    Rasterizer engine(headOnCamera(), sceneWithFrontFacingTriangle());

    EXPECT_EQ(Rasterizer::DepthFunc::Less, engine.depthFunc());
    EXPECT_TRUE(engine.depthWriteEnabled());
    EXPECT_FALSE(engine.stencilTestEnabled());
    EXPECT_EQ(Rasterizer::StencilFunc::Always, engine.stencilFunc());
    EXPECT_EQ(Rasterizer::StencilOp::Keep, engine.stencilFailOp());
    EXPECT_EQ(Rasterizer::StencilOp::Keep, engine.stencilDepthFailOp());
    EXPECT_EQ(Rasterizer::StencilOp::Keep, engine.stencilPassOp());
    EXPECT_FALSE(static_cast<bool>(engine.vertexShader()));
    EXPECT_FALSE(static_cast<bool>(engine.fragmentShader()));
    EXPECT_EQ(1, engine.msaaSamples());
    EXPECT_DOUBLE_EQ(0.1, engine.nearClipDepth());
    EXPECT_TRUE(std::isinf(engine.farClipDepth()));
    EXPECT_EQ(Rasterizer::PostProcessAA::None, engine.postProcessAA());
    EXPECT_FALSE(engine.shadowMapsEnabled());
    EXPECT_EQ(256, engine.shadowMapSize());
    EXPECT_EQ(1, engine.shadowCascadeCount());
    EXPECT_DOUBLE_EQ(1e-3, engine.shadowBias());
    EXPECT_DOUBLE_EQ(0.0, engine.shadowSlopeBias());
    EXPECT_EQ(0, engine.shadowFilterRadius());
    EXPECT_EQ(Rasterizer::ShadowFilterMode::PCF, engine.shadowFilterMode());
    EXPECT_EQ(nullptr, engine.diagnosticOutputBuffers().depth);
    EXPECT_EQ(nullptr, engine.diagnosticOutputBuffers().normal);
    EXPECT_EQ(nullptr, engine.diagnosticOutputBuffers().primitive);
    EXPECT_EQ(nullptr, engine.diagnosticOutputBuffers().material);
    EXPECT_EQ(nullptr, engine.diagnosticOutputBuffers().face);
    EXPECT_EQ(nullptr, engine.diagnosticOutputBuffers().stencil);
  }

  TEST(Rasterizer, ClonePreservesPostProcessAAAndShadowFilterMode) {
    Rasterizer engine(headOnCamera(), sceneWithFrontFacingTriangle());
    engine.setPostProcessAA(Rasterizer::PostProcessAA::FXAA);
    engine.setNearClipDepth(0.5);
    engine.setFarClipDepth(25.0);
    engine.setShadowCascadeCount(3);
    engine.setShadowSlopeBias(0.02);
    engine.setShadowFilterMode(Rasterizer::ShadowFilterMode::PCSS);
    Buffer<double> depth(1, 1);
    Rasterizer::DiagnosticOutputBuffers outputs;
    outputs.depth = &depth;
    engine.setDiagnosticOutputBuffers(outputs);

    auto clone = std::dynamic_pointer_cast<Rasterizer>(engine.cloneForRender());

    ASSERT_NE(nullptr, clone);
    EXPECT_EQ(Rasterizer::PostProcessAA::FXAA, clone->postProcessAA());
    EXPECT_DOUBLE_EQ(0.5, clone->nearClipDepth());
    EXPECT_DOUBLE_EQ(25.0, clone->farClipDepth());
    EXPECT_EQ(3, clone->shadowCascadeCount());
    EXPECT_DOUBLE_EQ(0.02, clone->shadowSlopeBias());
    EXPECT_EQ(Rasterizer::ShadowFilterMode::PCSS, clone->shadowFilterMode());
    EXPECT_EQ(nullptr, clone->diagnosticOutputBuffers().depth);
  }

  TEST(Rasterizer, DiagnosticOutputBuffersCapturePassingFragmentData) {
    auto tracked = sceneWithTrackedFrontFacingTriangle();
    Rasterizer engine(headOnCamera(), tracked.scene);
    engine.setStencilTestEnabled(true);
    engine.setStencilFunc(Rasterizer::StencilFunc::Always, 7);
    engine.setStencilOps(Rasterizer::StencilOp::Keep, Rasterizer::StencilOp::Keep,
                         Rasterizer::StencilOp::Replace);

    Buffer<Colord> color(64, 64);
    Buffer<double> depth(64, 64);
    Buffer<Vector3d> normal(64, 64);
    Buffer<const Primitive*> primitive(64, 64);
    Buffer<const Material*> material(64, 64);
    Buffer<std::uint64_t> face(64, 64);
    Buffer<std::uint8_t> stencil(64, 64);

    Rasterizer::DiagnosticOutputBuffers outputs;
    outputs.depth = &depth;
    outputs.normal = &normal;
    outputs.primitive = &primitive;
    outputs.material = &material;
    outputs.face = &face;
    outputs.stencil = &stencil;
    engine.setDiagnosticOutputBuffers(outputs);

    engine.render(color);

    EXPECT_EQ(engine.depthClearValue(), depth[0][0]);
    EXPECT_TRUE(normal[0][0].isUndefined());
    EXPECT_EQ(nullptr, primitive[0][0]);
    EXPECT_EQ(nullptr, material[0][0]);
    EXPECT_EQ(std::numeric_limits<std::uint64_t>::max(), face[0][0]);
    EXPECT_EQ(0, stencil[0][0]);

    EXPECT_TRUE(std::isfinite(depth[32][32]));
    EXPECT_GT(depth[32][32], 0.0);
    EXPECT_NEAR(1.0, normal[32][32].length(), 1e-9);
    EXPECT_EQ(tracked.triangle.get(), primitive[32][32]);
    EXPECT_EQ(tracked.material.get(), material[32][32]);
    EXPECT_EQ(0u, face[32][32]);
    EXPECT_EQ(7, stencil[32][32]);
  }

  TEST(Rasterizer, DiagnosticOutputBuffersIgnoreMismatchedBuffers) {
    Rasterizer engine(headOnCamera(), sceneWithFrontFacingTriangle());
    Buffer<Colord> color(64, 64);
    Buffer<double> depth(32, 32);
    depth.clear(123.0);

    Rasterizer::DiagnosticOutputBuffers outputs;
    outputs.depth = &depth;
    engine.setDiagnosticOutputBuffers(outputs);

    engine.render(color);

    EXPECT_DOUBLE_EQ(123.0, depth[16][16]);
  }

  TEST(Rasterizer, ClearDiagnosticOutputBuffersStopsWrites) {
    Rasterizer engine(headOnCamera(), sceneWithFrontFacingTriangle());
    Buffer<Colord> color(64, 64);
    Buffer<double> depth(64, 64);
    depth.clear(123.0);

    Rasterizer::DiagnosticOutputBuffers outputs;
    outputs.depth = &depth;
    engine.setDiagnosticOutputBuffers(outputs);
    engine.clearDiagnosticOutputBuffers();

    engine.render(color);

    EXPECT_DOUBLE_EQ(123.0, depth[32][32]);
  }

  TEST(Rasterizer, ShadowCascadeCountClampsToSupportedRange) {
    Rasterizer engine(headOnCamera(), sceneWithFrontFacingTriangle());

    engine.setShadowCascadeCount(3);
    EXPECT_EQ(3, engine.shadowCascadeCount());

    engine.setShadowCascadeCount(0);
    EXPECT_EQ(1, engine.shadowCascadeCount());

    engine.setShadowCascadeCount(9);
    EXPECT_EQ(4, engine.shadowCascadeCount());
  }

  TEST(Rasterizer, ShadowFilterRadiusClampsOnlyNegativeValues) {
    Rasterizer engine(headOnCamera(), sceneWithFrontFacingTriangle());

    engine.setShadowFilterRadius(3);
    EXPECT_EQ(3, engine.shadowFilterRadius());

    engine.setShadowFilterRadius(-3);

    EXPECT_EQ(0, engine.shadowFilterRadius());
  }

  TEST(Rasterizer, ShadowSlopeBiasClampsOnlyNegativeValues) {
    Rasterizer engine(headOnCamera(), sceneWithFrontFacingTriangle());

    engine.setShadowSlopeBias(0.25);
    EXPECT_DOUBLE_EQ(0.25, engine.shadowSlopeBias());

    engine.setShadowSlopeBias(-0.25);

    EXPECT_DOUBLE_EQ(0.0, engine.shadowSlopeBias());
  }

  TEST(Rasterizer, ShadowSlopeBiasAddsToleranceForGrazingReceivers) {
    const Vector3d receiver(0.0, 0.0, 0.0);
    const Vector3d lightDirection(0.0, 0.0, -1.0);
    const Vector3d lightFacingNormal(0.0, 0.0, -1.0);
    const Vector3d grazingNormal(0.0, 1.0, -0.1);
    auto constantOnly = syntheticShadowMap(0.01, 0.0);
    auto slopeBiased = syntheticShadowMap(0.01, 0.01);

    EXPECT_DOUBLE_EQ(0.0, constantOnly.visibility(receiver, lightFacingNormal, lightDirection));
    EXPECT_DOUBLE_EQ(0.0, slopeBiased.visibility(receiver, lightFacingNormal, lightDirection));
    EXPECT_DOUBLE_EQ(1.0, slopeBiased.visibility(receiver, grazingNormal, lightDirection));
  }

  TEST(Rasterizer, ClipDepthsClampToValidRange) {
    Rasterizer engine(headOnCamera(), sceneWithFrontFacingTriangle());

    engine.setNearClipDepth(-1.0);
    EXPECT_GT(engine.nearClipDepth(), 0.0);

    engine.setFarClipDepth(engine.nearClipDepth() * 0.5);
    EXPECT_GT(engine.farClipDepth(), engine.nearClipDepth());

    engine.clearFarClipDepth();
    EXPECT_TRUE(std::isinf(engine.farClipDepth()));
  }

  TEST(Rasterizer, NearClipDepthCanClipGeometryBeforeRasterization) {
    Rasterizer engine(headOnCamera(), sceneWithFrontFacingTriangle());
    engine.setBackgroundColor(Colord::black());
    engine.setNearClipDepth(100.0);
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_EQ(0, countNonBackground(buffer, Colord::black()));
  }

  TEST(Rasterizer, FarClipDepthCanClipGeometryBeforeRasterization) {
    Rasterizer engine(headOnCamera(), sceneWithFrontFacingTriangle());
    engine.setBackgroundColor(Colord::black());
    engine.setFarClipDepth(1.0);
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_EQ(0, countNonBackground(buffer, Colord::black()));
  }

  TEST(Rasterizer, DepthFuncNeverRejectsFragments) {
    Rasterizer engine(headOnCamera(), sceneWithFrontFacingTriangle());
    engine.setBackgroundColor(Colord::black());
    engine.setDepthFunc(Rasterizer::DepthFunc::Never);
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_EQ(0, countNonBackground(buffer, Colord::black()));
  }

  TEST(Rasterizer, DisabledDepthWritesLetLaterGeometryOverdraw) {
    const Colord nearColor(1.0, 0.0, 0.0);
    const Colord farColor(0.0, 1.0, 0.0);

    Rasterizer defaultDepth(headOnCamera(), sceneWithOverlappingTriangles());
    defaultDepth.setFragmentShader([&](const Rasterizer::FragmentInput& fragment) {
      return fragment.faceIdx == 0 ? nearColor : farColor;
    });

    Rasterizer noDepthWrites(headOnCamera(), sceneWithOverlappingTriangles());
    noDepthWrites.setDepthWriteEnabled(false);
    noDepthWrites.setFragmentShader([&](const Rasterizer::FragmentInput& fragment) {
      return fragment.faceIdx == 0 ? nearColor : farColor;
    });

    Buffer<Colord> defaultBuffer(64, 64);
    Buffer<Colord> noWriteBuffer(64, 64);
    defaultDepth.render(defaultBuffer);
    noDepthWrites.render(noWriteBuffer);

    EXPECT_EQ(nearColor, defaultBuffer[32][32]);
    EXPECT_EQ(farColor, noWriteBuffer[32][32]);
  }

  TEST(Rasterizer, StencilFailOpCanSeedLaterGeometry) {
    const Colord secondTriangleColor(0.0, 0.5, 1.0);

    Rasterizer engine(headOnCamera(), sceneWithDuplicateTriangles());
    engine.setStencilTestEnabled(true);
    engine.setStencilFunc(Rasterizer::StencilFunc::Equal, 1);
    engine.setStencilOps(Rasterizer::StencilOp::Replace, Rasterizer::StencilOp::Keep,
                         Rasterizer::StencilOp::Keep);
    engine.setFragmentShader([&](const Rasterizer::FragmentInput&) { return secondTriangleColor; });

    Buffer<Colord> buffer(64, 64);
    engine.render(buffer);

    EXPECT_EQ(secondTriangleColor, buffer[32][32]);
  }

  TEST(Rasterizer, SharedTriangleEdgeDoesNotDoubleApplyStencil) {
    const Colord overlapColor(1.0, 0.0, 0.0);

    Rasterizer engine(headOnCamera(), sceneWithAdjacentQuadTriangles());
    engine.setBackgroundColor(Colord::black());
    engine.setDepthFunc(Rasterizer::DepthFunc::Always);
    engine.setStencilTestEnabled(true);
    engine.setStencilFunc(Rasterizer::StencilFunc::Equal, 1);
    engine.setStencilOps(Rasterizer::StencilOp::Replace, Rasterizer::StencilOp::Keep,
                         Rasterizer::StencilOp::Keep);
    configureScreenSpaceQuad(engine);
    engine.setFragmentShader([&](const Rasterizer::FragmentInput&) { return overlapColor; });

    Buffer<Colord> buffer(32, 32);
    engine.render(buffer);

    EXPECT_EQ(0, countPixels(buffer, overlapColor));
  }

  TEST(Rasterizer, StencilEnabledBuiltInFragmentMatchesDefaultWhenAlwaysPasses) {
    Rasterizer fixedPipeline(headOnCamera(), sceneWithFrontFacingTriangle());
    Rasterizer stencilPipeline(headOnCamera(), sceneWithFrontFacingTriangle());
    stencilPipeline.setStencilTestEnabled(true);

    Buffer<Colord> fixedBuffer(64, 64);
    Buffer<Colord> stencilBuffer(64, 64);
    fixedPipeline.render(fixedBuffer);
    stencilPipeline.render(stencilBuffer);

    expectBuffersEqual(fixedBuffer, stencilBuffer);
  }

  TEST(Rasterizer, FragmentShaderOverridesBuiltInShading) {
    const Colord shaderColor(0.25, 0.5, 0.75);
    Rasterizer engine(headOnCamera(), sceneWithFrontFacingTriangle());
    engine.setFragmentShader([&](const Rasterizer::FragmentInput&) { return shaderColor; });
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_EQ(shaderColor, buffer[32][32]);
  }

  TEST(Rasterizer, FragmentShaderReceivesInterpolatedUV) {
    Rasterizer engine(headOnCamera(), sceneWithFrontFacingTriangle());
    engine.setFragmentShader([&](const Rasterizer::FragmentInput& fragment) {
      return Colord(fragment.uv.x(), fragment.uv.y(), 0.0);
    });
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    expectCenterLooksLikeTriangleUV(buffer[32][32]);
  }

  TEST(Rasterizer, BuiltInMaterialTextureReceivesInterpolatedUV) {
    Rasterizer engine(headOnCamera(), sceneWithTexturedFrontFacingTriangle());
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    expectCenterLooksLikeTriangleUV(buffer[32][32]);
  }

  TEST(Rasterizer, BuiltInMaterialConstantTextureUsesStoredAlbedo) {
    const Colord albedo(0.25, 0.5, 0.75);
    auto scene =
      sceneWithTexturedFrontFacingTriangle(std::make_shared<ConstantColorTexture>(albedo));
    Rasterizer engine(headOnCamera(), scene);
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_EQ(albedo, buffer[32][32]);
  }

  TEST(Rasterizer, BuiltInMaterialKeepsVirtualTextureBehaviorForConstantTextureSubclasses) {
    auto scene =
      sceneWithTexturedFrontFacingTriangle(std::make_shared<OverridingConstantColorTexture>());
    Rasterizer engine(headOnCamera(), scene);
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    expectCenterLooksLikeTriangleUV(buffer[32][32]);
  }

  TEST(Rasterizer, OrthographicProjectionInterpolatesWorldPositionLinearly) {
    auto cam = std::make_shared<OrthographicCamera>(Vector3d(0.0, 0.0, -5.0), Vector3d::null);
    Rasterizer engine(cam, sceneWithSlopedTriangle());
    engine.setFragmentShader([](const Rasterizer::FragmentInput& input) {
      return Colord(input.worldPosition.z() / 5.0, 0.0, 0.0);
    });
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_NEAR(0.5, buffer[32][32].r(), 0.03);
  }

  TEST(Rasterizer, DirectionalShadowMapsDarkenOccludedDiffuseLight) {
    auto cam = std::make_shared<PinholeCamera>(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.5));
    auto scene = sceneWithDirectionalShadowCaster();

    Rasterizer directOnly(cam, scene);
    Rasterizer shadowed(cam, scene);
    shadowed.setShadowMapsEnabled(true);
    shadowed.setShadowMapSize(256);
    shadowed.setShadowBias(0.1);

    Buffer<Colord> directBuffer(96, 96);
    Buffer<Colord> shadowBuffer(96, 96);
    directOnly.render(directBuffer);
    shadowed.render(shadowBuffer);

    const Colord directShadowPoint = colorAtWorldPoint(directBuffer, cam, Vector3d(0.6, 0.0, 1.0));
    const Colord shadowedShadowPoint =
      colorAtWorldPoint(shadowBuffer, cam, Vector3d(0.6, 0.0, 1.0));
    const Colord shadowedLitPoint = colorAtWorldPoint(shadowBuffer, cam, Vector3d(-1.2, 0.0, 1.0));

    EXPECT_GT(directShadowPoint.r(), shadowedShadowPoint.r() + 0.4);
    EXPECT_GT(shadowedLitPoint.r(), shadowedShadowPoint.r() + 0.4);
    EXPECT_LT(shadowedShadowPoint.r(), 0.35);
  }

  TEST(Rasterizer, ShadowFilterRadiusSoftensHardShadowBoundary) {
    auto cam = std::make_shared<PinholeCamera>(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.5));
    auto scene = sceneWithDirectionalShadowCaster();

    Rasterizer hard(cam, scene);
    hard.setShadowMapsEnabled(true);
    hard.setShadowMapSize(64);
    hard.setShadowBias(0.1);

    Rasterizer filtered(cam, scene);
    filtered.setShadowMapsEnabled(true);
    filtered.setShadowMapSize(64);
    filtered.setShadowBias(0.1);
    filtered.setShadowFilterRadius(2);

    Buffer<Colord> hardBuffer(96, 96);
    Buffer<Colord> filteredBuffer(96, 96);
    hard.render(hardBuffer);
    filtered.render(filteredBuffer);

    EXPECT_GT(countPixelsBrightenedByFiltering(hardBuffer, filteredBuffer), 0);
    EXPECT_GT(countPixelsDarkenedByFiltering(hardBuffer, filteredBuffer), 0);
  }

  TEST(Rasterizer, VertexShaderCanAdjustProjectedPosition) {
    Rasterizer engine(headOnCamera(), sceneWithFrontFacingTriangle());
    engine.setBackgroundColor(Colord::black());
    engine.setVertexShader([](const Rasterizer::VertexInput& vertex) {
      return Rasterizer::VertexOutput{vertex.worldPosition, vertex.normal, vertex.uv,
                                      vertex.clipPosition,
                                      vertex.screenPosition + Vector3d(1000, 0, 0)};
    });
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_EQ(0, countNonBackground(buffer, Colord::black()));
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
    engine.setBackgroundColor(Colord::black());
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
    engine.setBackgroundColor(Colord::black());
    engine.setCullMode(Rasterizer::CullMode::Front);
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_EQ(0, countNonBackground(buffer, Colord::black()));
  }

  TEST(Rasterizer, BackfaceCullingAppliesToTiledPath) {
    Rasterizer engine(headOnCamera(), sceneWithBackFacingTriangle());
    engine.setBackgroundColor(Colord::black());
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
    auto cam = std::make_shared<PinholeCamera>(Vector3d(0, 0, -8), Vector3d::null);
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

  TEST(Rasterizer, TiledRenderMatchesSingleTileRenderWithUnevenTileSizes) {
    auto cam = std::make_shared<PinholeCamera>(Vector3d(0, 0, -8), Vector3d::null);
    auto scene = std::make_shared<Scene>(Colord::white());
    scene->add(std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0));
    scene->add(std::make_shared<Box>(Vector3d(0, 0, 10), Vector3d(5, 5, 0.1)));

    Rasterizer singleTile(cam, scene);
    singleTile.setLod(2);
    singleTile.setMaximumThreads(1);
    singleTile.setQueueSize(1);

    Rasterizer tiled(cam, scene);
    tiled.setLod(2);
    tiled.setMaximumThreads(3);
    tiled.setQueueSize(6);

    Buffer<Colord> expected(127, 95);
    Buffer<Colord> actual(127, 95);
    singleTile.render(expected);
    tiled.render(actual);

    expectBuffersEqual(expected, actual);
  }

  TEST(Rasterizer, TiledRenderMatchesSingleTileRenderAcrossSharedTriangleEdge) {
    const Colord fillColor(0.2, 0.4, 0.8);

    Rasterizer singleTile(headOnCamera(), sceneWithAdjacentQuadTriangles());
    configureScreenSpaceQuad(singleTile);
    singleTile.setFragmentShader([&](const Rasterizer::FragmentInput&) { return fillColor; });

    Rasterizer tiled(headOnCamera(), sceneWithAdjacentQuadTriangles());
    tiled.setMaximumThreads(2);
    tiled.setQueueSize(4);
    configureScreenSpaceQuad(tiled);
    tiled.setFragmentShader([&](const Rasterizer::FragmentInput&) { return fillColor; });

    Buffer<Colord> expected(32, 32);
    Buffer<Colord> actual(32, 32);
    singleTile.render(expected);
    tiled.render(actual);

    expectBuffersEqual(expected, actual);
  }

  TEST(Rasterizer, SubpixelScreenCoordinatesDoNotRoundAtHalfPixel) {
    Rasterizer beforeHalfPixel(headOnCamera(), sceneWithFrontFacingTriangle());
    beforeHalfPixel.setBackgroundColor(Colord::black());
    configureScreenSpaceSubpixelTriangle(beforeHalfPixel, 18.49);

    Rasterizer afterHalfPixel(headOnCamera(), sceneWithFrontFacingTriangle());
    afterHalfPixel.setBackgroundColor(Colord::black());
    configureScreenSpaceSubpixelTriangle(afterHalfPixel, 18.51);

    Buffer<Colord> before(32, 32);
    Buffer<Colord> after(32, 32);
    beforeHalfPixel.render(before);
    afterHalfPixel.render(after);

    expectBuffersEqual(before, after);
  }

  TEST(Rasterizer, SubpixelScreenCoordinatesChangeCoverageAtSamplePoint) {
    Rasterizer beforeSamplePoint(headOnCamera(), sceneWithFrontFacingTriangle());
    beforeSamplePoint.setBackgroundColor(Colord::black());
    configureScreenSpaceSubpixelTriangle(beforeSamplePoint, 18.99);

    Rasterizer afterSamplePoint(headOnCamera(), sceneWithFrontFacingTriangle());
    afterSamplePoint.setBackgroundColor(Colord::black());
    configureScreenSpaceSubpixelTriangle(afterSamplePoint, 19.01);

    Buffer<Colord> before(32, 32);
    Buffer<Colord> after(32, 32);
    beforeSamplePoint.render(before);
    afterSamplePoint.render(after);

    EXPECT_LT(countNonBackground(before, Colord::black()),
              countNonBackground(after, Colord::black()));
  }

  TEST(Rasterizer, TiledRenderMatchesSingleTileRenderWithSubpixelVertices) {
    Rasterizer singleTile(headOnCamera(), sceneWithFrontFacingTriangle());
    configureScreenSpaceSubpixelTriangle(singleTile, 18.51);

    Rasterizer tiled(headOnCamera(), sceneWithFrontFacingTriangle());
    tiled.setMaximumThreads(2);
    tiled.setQueueSize(4);
    configureScreenSpaceSubpixelTriangle(tiled, 18.51);

    Buffer<Colord> expected(32, 32);
    Buffer<Colord> actual(32, 32);
    singleTile.render(expected);
    tiled.render(actual);

    expectBuffersEqual(expected, actual);
  }

  TEST(Rasterizer, MSAAResolveBlendsPartiallyCoveredEdge) {
    Rasterizer engine(headOnCamera(), sceneWithFrontFacingTriangle());
    engine.setBackgroundColor(Colord::black());
    engine.setMSAASamples(4);
    configureScreenSpaceEdgeTriangle(engine);
    Buffer<Colord> buffer(40, 40);

    engine.render(buffer);

    EXPECT_NEAR(0.5, buffer[24][24].r(), 1e-9);
    EXPECT_NEAR(0.5, buffer[24][24].g(), 1e-9);
    EXPECT_NEAR(0.5, buffer[24][24].b(), 1e-9);
  }

  TEST(Rasterizer, MSAASampleOffsetsUseSubpixelScreenCoordinates) {
    Rasterizer engine(headOnCamera(), sceneWithFrontFacingTriangle());
    engine.setBackgroundColor(Colord::black());
    engine.setMSAASamples(2);
    configureScreenSpaceMSAASubpixelTriangle(engine);
    Buffer<Colord> buffer(40, 40);

    engine.render(buffer);

    EXPECT_NEAR(0.5, buffer[16][16].r(), 1e-9);
    EXPECT_NEAR(0.5, buffer[16][16].g(), 1e-9);
    EXPECT_NEAR(0.5, buffer[16][16].b(), 1e-9);
  }

  TEST(Rasterizer, TiledMSAAMatchesSingleTileMSAA) {
    Rasterizer singleTile(headOnCamera(), sceneWithFrontFacingTriangle());
    singleTile.setMSAASamples(4);
    configureScreenSpaceEdgeTriangle(singleTile);

    Rasterizer tiled(headOnCamera(), sceneWithFrontFacingTriangle());
    tiled.setMSAASamples(4);
    tiled.setMaximumThreads(2);
    tiled.setQueueSize(4);
    configureScreenSpaceEdgeTriangle(tiled);

    Buffer<Colord> expected(40, 40);
    Buffer<Colord> actual(40, 40);
    singleTile.render(expected);
    tiled.render(actual);

    expectBuffersEqual(expected, actual);
  }

  TEST(Rasterizer, TiledMSAAMatchesSingleTileMSAAWithUnevenTileSizes) {
    Rasterizer singleTile(headOnCamera(), sceneWithFrontFacingTriangle());
    singleTile.setMSAASamples(4);
    configureScreenSpaceEdgeTriangle(singleTile);

    Rasterizer tiled(headOnCamera(), sceneWithFrontFacingTriangle());
    tiled.setMSAASamples(4);
    tiled.setMaximumThreads(3);
    tiled.setQueueSize(6);
    configureScreenSpaceEdgeTriangle(tiled);

    Buffer<Colord> expected(41, 37);
    Buffer<Colord> actual(41, 37);
    singleTile.render(expected);
    tiled.render(actual);

    expectBuffersEqual(expected, actual);
  }

  TEST(Rasterizer, TiledMSAAMatchesSingleTileMSAAWithStencil) {
    const Colord secondTriangleColor(0.0, 0.5, 1.0);

    Rasterizer singleTile(headOnCamera(), sceneWithDuplicateTriangles());
    singleTile.setMSAASamples(4);
    singleTile.setStencilTestEnabled(true);
    singleTile.setStencilFunc(Rasterizer::StencilFunc::Equal, 1);
    singleTile.setStencilOps(Rasterizer::StencilOp::Replace, Rasterizer::StencilOp::Keep,
                             Rasterizer::StencilOp::Keep);
    singleTile.setFragmentShader(
      [&](const Rasterizer::FragmentInput&) { return secondTriangleColor; });

    Rasterizer tiled(headOnCamera(), sceneWithDuplicateTriangles());
    tiled.setMSAASamples(4);
    tiled.setMaximumThreads(2);
    tiled.setQueueSize(4);
    tiled.setStencilTestEnabled(true);
    tiled.setStencilFunc(Rasterizer::StencilFunc::Equal, 1);
    tiled.setStencilOps(Rasterizer::StencilOp::Replace, Rasterizer::StencilOp::Keep,
                        Rasterizer::StencilOp::Keep);
    tiled.setFragmentShader([&](const Rasterizer::FragmentInput&) { return secondTriangleColor; });

    Buffer<Colord> expected(64, 64);
    Buffer<Colord> actual(64, 64);
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
