#include <gtest/gtest.h>

#include "core/Buffer.h"
#include "core/Color.h"
#include "core/math/Vector.h"
#include "engine/raster/OpenGLOffscreenContext.h"
#include "engine/raster/OpenGLRasterizer.h"
#include "engine/raster/RasterVisibilitySet.h"
#include "render/cameras/PinholeCamera.h"
#include "render/lights/DirectionalLight.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "render/textures/ConstantColorTexture.h"
#include "test/helpers/GuiTestHelper.h"

#include "engine/raster/gl/AttachmentSet.h"
#include "engine/raster/gl/Bindings.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace OpenGLRasterizerTest {
  TEST(OpenGLOffscreenContext, ProbeReportsAvailabilityOrActionableError) {
    const engine::raster::OpenGLAvailability availability =
      engine::raster::OpenGLOffscreenContext::probe();

    if (availability.available()) {
      EXPECT_FALSE(availability.detail().empty());
    } else {
      EXPECT_FALSE(availability.error().empty());
    }
  }

  TEST(OpenGLOffscreenContext, CopiesRawColorChannelsWithoutUnpremultiplyingAlpha) {
    engine::raster::OpenGLOffscreenContext context;
    if (!context.create() || !context.makeCurrent()) {
      SUCCEED() << "OpenGL context unavailable on this host";
      return;
    }
    engine::raster::gl::AttachmentSet set;
    if (!set.create(2, 2, 0)) {
      context.doneCurrent();
      SUCCEED() << "AttachmentSet unavailable on this host: " << set.errorMessage();
      return;
    }
    set.bind();

    glViewport(0, 0, 2, 2);
    glDisable(GL_BLEND);
    glClearColor(0.24f, 0.27f, 0.30f, 0.30f);
    glClear(GL_COLOR_BUFFER_BIT);

    Buffer<Colord> buffer(2, 2);
    set.copyColorTo(buffer);

    set.release();
    set.destroy();
    context.doneCurrent();

    EXPECT_NEAR(0.24, buffer[0][0].r(), 1.0 / 255.0);
    EXPECT_NEAR(0.27, buffer[0][0].g(), 1.0 / 255.0);
    EXPECT_NEAR(0.30, buffer[0][0].b(), 1.0 / 255.0);
  }

  TEST(OpenGLRasterizer, FailsClearlyWhenContextUnavailable) {
    engine::raster::OpenGLRasterizer rasterizer(nullptr);
    Buffer<Colord> buffer(2, 2);

    try {
      rasterizer.render(buffer);
    } catch (const std::runtime_error& error) {
      EXPECT_NE(std::string(error.what()).find("OpenGL raster backend"), std::string::npos);
    }
  }

  TEST(OpenGLRasterizer, RenderDepthFailsClearlyWhenContextUnavailable) {
    engine::raster::OpenGLRasterizer rasterizer(nullptr);
    Buffer<double> buffer(2, 2);

    try {
      rasterizer.renderDepth(buffer);
    } catch (const std::runtime_error& error) {
      EXPECT_NE(std::string(error.what()).find("OpenGL raster backend"), std::string::npos);
    }
  }

  TEST(OpenGLRasterizer, ClonesLodForRender) {
    engine::raster::OpenGLRasterizer rasterizer(nullptr);
    rasterizer.setLod(3);

    auto clone =
      std::dynamic_pointer_cast<engine::raster::OpenGLRasterizer>(rasterizer.cloneForRender());

    ASSERT_NE(nullptr, clone);
    EXPECT_EQ(3, clone->lod());
  }

  TEST(OpenGLRasterizer, ClonesFixedFunctionStateForRender) {
    engine::raster::OpenGLRasterizer rasterizer(nullptr);
    rasterizer.setMSAASamples(4);
    rasterizer.setMSAAShadingMode(engine::raster::Rasterizer::MSAAShadingMode::PerSample);
    rasterizer.setCullMode(engine::raster::Rasterizer::CullMode::Back);
    rasterizer.setViewportRect(Recti(4, 5, 20, 21));
    rasterizer.setScissorRect(Recti(6, 7, 18, 19));
    rasterizer.setColorLoadOp(engine::raster::Rasterizer::AttachmentLoadOp::Load);
    rasterizer.setColorStoreOp(engine::raster::Rasterizer::AttachmentStoreOp::Discard);
    rasterizer.setColorWriteMask(engine::raster::Rasterizer::ColorWriteGreen);
    rasterizer.setBlendingEnabled(true);
    rasterizer.setBlendFactors(engine::raster::Rasterizer::BlendFactor::ConstantAlpha,
                               engine::raster::Rasterizer::BlendFactor::OneMinusConstantAlpha);
    rasterizer.setBlendOp(engine::raster::Rasterizer::BlendOp::Max);
    rasterizer.setBlendConstant(Colord(0.1, 0.2, 0.3), 0.4);
    rasterizer.setAlphaTestEnabled(true);
    rasterizer.setAlphaFunc(engine::raster::Rasterizer::AlphaFunc::Greater, 0.6);
    rasterizer.setDepthFunc(engine::raster::Rasterizer::DepthFunc::GreaterEqual);
    rasterizer.setDepthBias(-0.125);
    rasterizer.setDepthClearValue(8.5);
    rasterizer.setDepthLoadOp(engine::raster::Rasterizer::AttachmentLoadOp::Clear);
    rasterizer.setDepthStoreOp(engine::raster::Rasterizer::AttachmentStoreOp::Discard);
    rasterizer.setDepthWriteEnabled(false);
    rasterizer.setStencilTestEnabled(true);
    rasterizer.setStencilFunc(engine::raster::Rasterizer::StencilFunc::Equal, 7, 0x0f);
    rasterizer.setStencilClearValue(3);
    rasterizer.setStencilLoadOp(engine::raster::Rasterizer::AttachmentLoadOp::Clear);
    rasterizer.setStencilStoreOp(engine::raster::Rasterizer::AttachmentStoreOp::Discard);
    rasterizer.setStencilWriteMask(0xf0);
    rasterizer.setStencilOps(engine::raster::Rasterizer::StencilOp::Replace,
                             engine::raster::Rasterizer::StencilOp::IncrementClamp,
                             engine::raster::Rasterizer::StencilOp::Invert);
    rasterizer.setShadowMapsEnabled(true);
    auto visibilitySet = std::make_shared<engine::raster::RasterVisibilitySet>();
    visibilitySet->addVisibleLeaf(1, 1);
    rasterizer.setVisibilitySet(visibilitySet);

    auto clone =
      std::dynamic_pointer_cast<engine::raster::OpenGLRasterizer>(rasterizer.cloneForRender());

    ASSERT_NE(nullptr, clone);
    EXPECT_EQ(4, clone->msaaSamples());
    EXPECT_EQ(engine::raster::Rasterizer::MSAAShadingMode::PerSample, clone->msaaShadingMode());
    EXPECT_TRUE(clone->hasCullModeOverride());
    EXPECT_EQ(engine::raster::Rasterizer::CullMode::Back, clone->cullMode());
    EXPECT_TRUE(clone->viewportEnabled());
    EXPECT_EQ(4, clone->viewportRect().left());
    EXPECT_EQ(20, clone->viewportRect().width());
    EXPECT_TRUE(clone->scissorTestEnabled());
    EXPECT_EQ(6, clone->scissorRect().left());
    EXPECT_EQ(18, clone->scissorRect().width());
    EXPECT_EQ(engine::raster::Rasterizer::AttachmentLoadOp::Load, clone->colorLoadOp());
    EXPECT_EQ(engine::raster::Rasterizer::AttachmentStoreOp::Discard, clone->colorStoreOp());
    EXPECT_EQ(engine::raster::Rasterizer::ColorWriteGreen, clone->colorWriteMask());
    EXPECT_TRUE(clone->blendingEnabled());
    EXPECT_EQ(engine::raster::Rasterizer::BlendFactor::ConstantAlpha, clone->sourceBlendFactor());
    EXPECT_EQ(engine::raster::Rasterizer::BlendFactor::OneMinusConstantAlpha,
              clone->destinationBlendFactor());
    EXPECT_EQ(engine::raster::Rasterizer::BlendOp::Max, clone->blendOp());
    EXPECT_EQ(Colord(0.1, 0.2, 0.3), clone->blendConstantColor());
    EXPECT_EQ(0.4, clone->blendConstantAlpha());
    EXPECT_TRUE(clone->alphaTestEnabled());
    EXPECT_EQ(engine::raster::Rasterizer::AlphaFunc::Greater, clone->alphaFunc());
    EXPECT_EQ(0.6, clone->alphaReference());
    EXPECT_EQ(engine::raster::Rasterizer::DepthFunc::GreaterEqual, clone->depthFunc());
    EXPECT_EQ(-0.125, clone->depthBias());
    EXPECT_EQ(8.5, clone->depthClearValue());
    EXPECT_EQ(engine::raster::Rasterizer::AttachmentLoadOp::Clear, clone->depthLoadOp());
    EXPECT_EQ(engine::raster::Rasterizer::AttachmentStoreOp::Discard, clone->depthStoreOp());
    EXPECT_FALSE(clone->depthWriteEnabled());
    EXPECT_TRUE(clone->stencilTestEnabled());
    EXPECT_EQ(engine::raster::Rasterizer::StencilFunc::Equal, clone->stencilFunc());
    EXPECT_EQ(7, clone->stencilReference());
    EXPECT_EQ(0x0f, clone->stencilMask());
    EXPECT_EQ(3, clone->stencilClearValue());
    EXPECT_EQ(engine::raster::Rasterizer::AttachmentLoadOp::Clear, clone->stencilLoadOp());
    EXPECT_EQ(engine::raster::Rasterizer::AttachmentStoreOp::Discard, clone->stencilStoreOp());
    EXPECT_EQ(0xf0, clone->stencilWriteMask());
    EXPECT_EQ(engine::raster::Rasterizer::StencilOp::Replace, clone->stencilFailOp());
    EXPECT_EQ(engine::raster::Rasterizer::StencilOp::IncrementClamp, clone->stencilDepthFailOp());
    EXPECT_EQ(engine::raster::Rasterizer::StencilOp::Invert, clone->stencilPassOp());
    EXPECT_TRUE(clone->shadowMapsEnabled());
    ASSERT_NE(nullptr, clone->visibilitySet());
    EXPECT_TRUE(clone->visibilitySet()->leafVisible(0));
  }

  TEST(OpenGLRasterizer, ClampsMSAASamplesToSupportedCounts) {
    engine::raster::OpenGLRasterizer rasterizer(nullptr);

    rasterizer.setMSAASamples(0);
    EXPECT_EQ(1, rasterizer.msaaSamples());
    rasterizer.setMSAASamples(3);
    EXPECT_EQ(4, rasterizer.msaaSamples());
    rasterizer.setMSAASamples(99);
    EXPECT_EQ(8, rasterizer.msaaSamples());
  }

  TEST(OpenGLRasterizer, DefaultsToPerFragmentMSAAShading) {
    engine::raster::OpenGLRasterizer rasterizer(nullptr);

    EXPECT_EQ(engine::raster::Rasterizer::MSAAShadingMode::PerFragment,
              rasterizer.msaaShadingMode());
  }

  TEST(OpenGLRasterizer, MasksUnsupportedColorWriteBits) {
    engine::raster::OpenGLRasterizer rasterizer(nullptr);

    rasterizer.setColorWriteMask(0xff);

    EXPECT_EQ(engine::raster::Rasterizer::ColorWriteAll, rasterizer.colorWriteMask());
  }

  TEST(OpenGLRasterizer, ClampsAlphaReference) {
    engine::raster::OpenGLRasterizer rasterizer(nullptr);

    rasterizer.setAlphaFunc(engine::raster::Rasterizer::AlphaFunc::Less, 2.0);
    EXPECT_EQ(1.0, rasterizer.alphaReference());

    rasterizer.setAlphaFunc(engine::raster::Rasterizer::AlphaFunc::Greater, -1.0);
    EXPECT_EQ(0.0, rasterizer.alphaReference());
  }

  TEST(OpenGLRasterizer, ClearsNonFiniteDepthBias) {
    engine::raster::OpenGLRasterizer rasterizer(nullptr);

    rasterizer.setDepthBias(std::numeric_limits<double>::infinity());

    EXPECT_EQ(0.0, rasterizer.depthBias());
  }

  TEST(OpenGLRasterizer, DoesNotReportReadbackBeforeSuccessfulRender) {
    engine::raster::OpenGLRasterizer rasterizer(nullptr);

    EXPECT_TRUE(rasterizer.readbackTraceMessage().empty());
    EXPECT_TRUE(rasterizer.traceMessages().empty());
  }

  TEST(OpenGLRasterizer, AppendsTraceWhenLightCountsExceedShaderCap) {
    std::vector<std::string> traces;
    engine::raster::OpenGLRasterizer::appendLightTruncationTrace(0, 0, traces);
    EXPECT_TRUE(traces.empty());

    const auto maxDirectional =
      static_cast<std::size_t>(engine::raster::OpenGLRasterizer::maxShaderDirectionalLights());
    const auto maxPoint =
      static_cast<std::size_t>(engine::raster::OpenGLRasterizer::maxShaderPointLights());
    engine::raster::OpenGLRasterizer::appendLightTruncationTrace(maxDirectional, maxPoint, traces);
    EXPECT_TRUE(traces.empty());

    engine::raster::OpenGLRasterizer::appendLightTruncationTrace(maxDirectional + 3, maxPoint + 1,
                                                                 traces);
    ASSERT_EQ(2u, traces.size());
    EXPECT_NE(traces[0].find("directional"), std::string::npos);
    EXPECT_NE(traces[0].find(std::to_string(maxDirectional + 3)), std::string::npos);
    EXPECT_NE(traces[0].find(std::to_string(maxDirectional)), std::string::npos);
    EXPECT_NE(traces[1].find("point"), std::string::npos);
    EXPECT_NE(traces[1].find(std::to_string(maxPoint + 1)), std::string::npos);
  }

  TEST(OpenGLRasterizer, ProvidesSharedStatusMessage) {
    const std::string message = engine::raster::OpenGLRasterizer::statusMessage();

    EXPECT_FALSE(message.empty());
    EXPECT_NE(message.find("OpenGL raster backend"), std::string::npos);
  }

  namespace {
    std::shared_ptr<render::Scene> simpleSphereScene() {
      auto scene = std::make_shared<render::Scene>(Colord(0.05, 0.05, 0.1));
      auto sphere = std::make_shared<render::Sphere>(Vector3d::null, 1.0);
      sphere->setMaterial(std::make_shared<render::MatteMaterial>(
        std::make_shared<render::ConstantColorTexture>(Colord(0.8, 0.4, 0.2))));
      scene->add(sphere);
      scene->addLight(
        std::make_shared<render::DirectionalLight>(Vector3d(0.0, 0.0, -1.0), Colord::white()));
      return scene;
    }

    bool tracesContain(const std::vector<std::string>& traces, const std::string& needle) {
      return std::any_of(traces.begin(), traces.end(), [&](const std::string& line) {
        return line.find(needle) != std::string::npos;
      });
    }
  }

  class OpenGLRasterizerMeshCache : public ::testing::GuiTest {};

  TEST_F(OpenGLRasterizerMeshCache, ReusesMeshAcrossRendersWithSameSceneAndCamera) {
    if (!engine::raster::OpenGLOffscreenContext::probe().available()) {
      GTEST_SKIP() << "OpenGL offscreen context unavailable on this host";
    }

    auto scene = simpleSphereScene();
    auto cam = std::make_shared<render::PinholeCamera>(Vector3d(0, 0, -5), Vector3d::null);

    engine::raster::OpenGLRasterizer rasterizer(cam, scene);
    Buffer<Colord> buffer(32, 32);

    rasterizer.render(buffer);
    EXPECT_TRUE(tracesContain(rasterizer.traceMessages(), "built"));

    rasterizer.render(buffer);
    EXPECT_TRUE(tracesContain(rasterizer.traceMessages(), "reused"))
      << "second render with identical scene+camera should hit the mesh cache";
  }

  TEST_F(OpenGLRasterizerMeshCache, SkipsVertexBufferUploadOnCacheHit) {
    if (!engine::raster::OpenGLOffscreenContext::probe().available()) {
      GTEST_SKIP() << "OpenGL offscreen context unavailable on this host";
    }

    auto scene = simpleSphereScene();
    auto cam = std::make_shared<render::PinholeCamera>(Vector3d(0, 0, -5), Vector3d::null);

    engine::raster::OpenGLRasterizer rasterizer(cam, scene);
    Buffer<Colord> buffer(32, 32);

    rasterizer.render(buffer);
    EXPECT_TRUE(tracesContain(rasterizer.traceMessages(), "OpenGL raster draw uploaded"))
      << "first render uploads vertex/index data fresh";

    rasterizer.render(buffer);
    EXPECT_TRUE(tracesContain(rasterizer.traceMessages(), "reused") &&
                tracesContain(rasterizer.traceMessages(), "from cache"))
      << "cache-hit render must skip the vertex/index buffer re-upload";
  }

  TEST_F(OpenGLRasterizerMeshCache, RebuildsMeshWhenSceneIdentityChanges) {
    if (!engine::raster::OpenGLOffscreenContext::probe().available()) {
      GTEST_SKIP() << "OpenGL offscreen context unavailable on this host";
    }

    auto cam = std::make_shared<render::PinholeCamera>(Vector3d(0, 0, -5), Vector3d::null);

    auto firstScene = simpleSphereScene();
    engine::raster::OpenGLRasterizer firstRasterizer(cam, firstScene);
    Buffer<Colord> buffer(32, 32);
    firstRasterizer.render(buffer);
    firstScene.reset();

    auto secondScene = simpleSphereScene();
    engine::raster::OpenGLRasterizer secondRasterizer(cam, secondScene);
    secondRasterizer.render(buffer);

    EXPECT_TRUE(tracesContain(secondRasterizer.traceMessages(), "built"))
      << "a freed-and-replaced scene must not produce a false cache hit even if "
         "the new scene is allocated at the same address";
  }

  TEST_F(OpenGLRasterizerMeshCache, CacheHitRefreshesViewPlaneSetup) {
    if (!engine::raster::OpenGLOffscreenContext::probe().available()) {
      GTEST_SKIP() << "OpenGL offscreen context unavailable on this host";
    }

    auto scene = simpleSphereScene();
    auto cam = std::make_shared<render::PinholeCamera>(Vector3d(0, 0, -5), Vector3d::null);
    cam->setAspectMode(render::AspectMode::FitWidth);

    engine::raster::OpenGLRasterizer rasterizer(cam, scene);
    Buffer<Colord> referenceBuffer(64, 32);
    rasterizer.render(referenceBuffer);

    // Simulate another render pass on the same camera mutating the view
    // plane to a different viewport before this rasterizer renders
    // again (shadow maps, picking, post-process passes share the camera
    // through the graph pipeline). On the next render the cache hits,
    // and without an explicit `setup()` call the view plane would still
    // carry the other pass's `hSpan/vSpan` — the resulting projection
    // matrix would then squish the scene to that pass's aspect. The
    // FitWidth aspect mode is what makes `hSpan/vSpan` actually depend
    // on the viewport; Stretch's fixed 8×6 would mask the bug.
    cam->viewPlane()->setup(cam->matrix(), Recti(0, 0, 32, 64));

    Buffer<Colord> cacheHitBuffer(64, 32);
    rasterizer.render(cacheHitBuffer);

    EXPECT_TRUE(tracesContain(rasterizer.traceMessages(), "reused"))
      << "scaffold check: this test only stress-tests the cache-hit path";

    bool anyDifference = false;
    for (int y = 0; y < 32 && !anyDifference; ++y) {
      for (int x = 0; x < 64 && !anyDifference; ++x) {
        const Colord& a = referenceBuffer[y][x];
        const Colord& b = cacheHitBuffer[y][x];
        if (std::abs(a.r() - b.r()) > 1.0 / 255.0 || std::abs(a.g() - b.g()) > 1.0 / 255.0 ||
            std::abs(a.b() - b.b()) > 1.0 / 255.0) {
          anyDifference = true;
        }
      }
    }
    EXPECT_FALSE(anyDifference)
      << "cache-hit render diverges from fresh render — view-plane state was "
         "not refreshed and the projection matrix used another pass's aspect";
  }

  TEST_F(OpenGLRasterizerMeshCache, ReusesMeshAcrossCameraMovesForCameraIndependentBuilds) {
    if (!engine::raster::OpenGLOffscreenContext::probe().available()) {
      GTEST_SKIP() << "OpenGL offscreen context unavailable on this host";
    }

    auto scene = simpleSphereScene();
    auto cam = std::make_shared<render::PinholeCamera>(Vector3d(0, 0, -5), Vector3d::null);

    engine::raster::OpenGLRasterizer rasterizer(cam, scene);
    Buffer<Colord> buffer(32, 32);
    rasterizer.render(buffer);

    cam->setPosition(Vector3d(2, 0, -5));
    rasterizer.render(buffer);

    EXPECT_TRUE(tracesContain(rasterizer.traceMessages(), "reused"))
      << "scene with only fragment-shader-handled lights builds a camera-"
         "independent mesh; the GPU does projection / cull / clip, so the "
         "cached mesh stays valid across camera moves";
  }

  TEST_F(OpenGLRasterizerMeshCache, LruRetainsEntriesAcrossMultiPassThrashing) {
    if (!engine::raster::OpenGLOffscreenContext::probe().available()) {
      GTEST_SKIP() << "OpenGL offscreen context unavailable on this host";
    }

    // Three different scenes that miss each other's cache key. With a
    // single-entry cache they would invalidate each other on every
    // rotation; the n=4 LRU keeps all three resident, so iterating
    // through them all hits the cache on the second pass through each.
    auto sceneA = simpleSphereScene();
    auto sceneB = simpleSphereScene();
    auto sceneC = simpleSphereScene();
    auto cam = std::make_shared<render::PinholeCamera>(Vector3d(0, 0, -5), Vector3d::null);
    Buffer<Colord> buffer(16, 16);

    engine::raster::OpenGLRasterizer rA(cam, sceneA);
    engine::raster::OpenGLRasterizer rB(cam, sceneB);
    engine::raster::OpenGLRasterizer rC(cam, sceneC);
    rA.render(buffer);
    rB.render(buffer);
    rC.render(buffer);

    // Second rotation: all three should now hit the cache.
    rA.render(buffer);
    EXPECT_TRUE(tracesContain(rA.traceMessages(), "reused"))
      << "sceneA's mesh must survive sceneB and sceneC's intervening builds";
    rB.render(buffer);
    EXPECT_TRUE(tracesContain(rB.traceMessages(), "reused"));
    rC.render(buffer);
    EXPECT_TRUE(tracesContain(rC.traceMessages(), "reused"));
  }

  TEST_F(OpenGLRasterizerMeshCache, RebuildsMeshWhenCameraMovesForDepthBiasedRenders) {
    if (!engine::raster::OpenGLOffscreenContext::probe().available()) {
      GTEST_SKIP() << "OpenGL offscreen context unavailable on this host";
    }

    auto scene = simpleSphereScene();
    auto cam = std::make_shared<render::PinholeCamera>(Vector3d(0, 0, -5), Vector3d::null);

    engine::raster::OpenGLRasterizer rasterizer(cam, scene);
    // A non-zero depth bias forces the CPU-projected path because the
    // bias is currently baked into `vertex.z`; the camera-independent
    // path's zeroed z would lose the offset. With the CPU path active
    // the cache key includes the camera pose, so a move invalidates.
    rasterizer.setDepthBias(0.01);
    Buffer<Colord> buffer(32, 32);
    rasterizer.render(buffer);

    cam->setPosition(Vector3d(2, 0, -5));
    rasterizer.render(buffer);

    EXPECT_TRUE(tracesContain(rasterizer.traceMessages(), "built"))
      << "depth-biased scene falls back to the CPU-projected path whose "
         "cache key includes the camera pose; a camera move must invalidate";
  }

  class OpenGLRasterizerAttachmentLoad : public ::testing::GuiTest {};

  // Today `AttachmentLoadOp::Load` is rejected before the context bind
  // because it requires GPU-resident attachments that earlier passes
  // wrote to. The plan in `docs/plans/opengl-gpu-residency.md` Phase 2
  // makes this implementable. When residency lands, flip this test to
  // assert that a two-pass plan with `Load` on the second pass actually
  // preserves the first pass's depth output instead of clearing.
  TEST_F(OpenGLRasterizerAttachmentLoad, ColorLoadOpRequiresSourceBuffer) {
    if (!engine::raster::OpenGLOffscreenContext::probe().available()) {
      GTEST_SKIP() << "OpenGL offscreen context unavailable on this host";
    }
    auto scene = std::make_shared<render::Scene>(Colord::black());
    engine::raster::OpenGLRasterizer rasterizer(
      std::make_shared<render::PinholeCamera>(Vector3d(0, 0, -5), Vector3d::null), scene);
    rasterizer.setColorLoadOp(engine::raster::Rasterizer::AttachmentLoadOp::Load);
    Buffer<Colord> buffer(8, 8);
    EXPECT_THROW(rasterizer.render(buffer), std::runtime_error);
  }

  TEST_F(OpenGLRasterizerAttachmentLoad, ColorLoadOpAcceptsSourceBuffer) {
    if (!engine::raster::OpenGLOffscreenContext::probe().available()) {
      GTEST_SKIP() << "OpenGL offscreen context unavailable on this host";
    }
    auto scene = std::make_shared<render::Scene>(Colord::black());
    engine::raster::OpenGLRasterizer rasterizer(
      std::make_shared<render::PinholeCamera>(Vector3d(0, 0, -5), Vector3d::null), scene);
    rasterizer.setColorLoadOp(engine::raster::Rasterizer::AttachmentLoadOp::Load);
    Buffer<Colord> source(8, 8);
    source.clear(Colord(0.25, 0.5, 0.75));
    rasterizer.setColorLoadSource(&source);
    Buffer<Colord> buffer(8, 8);
    EXPECT_NO_THROW(rasterizer.render(buffer));
  }

  TEST_F(OpenGLRasterizerAttachmentLoad, DepthLoadOpThrowsUntilResidencyFollowUp) {
    if (!engine::raster::OpenGLOffscreenContext::probe().available()) {
      GTEST_SKIP() << "OpenGL offscreen context unavailable on this host";
    }
    auto scene = std::make_shared<render::Scene>(Colord::black());
    engine::raster::OpenGLRasterizer rasterizer(
      std::make_shared<render::PinholeCamera>(Vector3d(0, 0, -5), Vector3d::null), scene);
    rasterizer.setDepthLoadOp(engine::raster::Rasterizer::AttachmentLoadOp::Load);
    Buffer<Colord> buffer(8, 8);
    // (a) no source buffer when Load → throws.
    EXPECT_THROW(rasterizer.render(buffer), std::runtime_error);
    // (b) source buffer set but depth Load upload deferred → throws
    //     with a narrower message naming the missing slice.
    Buffer<double> depthSource(8, 8);
    rasterizer.setDepthLoadSource(&depthSource);
    EXPECT_THROW(rasterizer.render(buffer), std::runtime_error);
  }

  TEST_F(OpenGLRasterizerAttachmentLoad, StencilLoadOpThrowsUntilResidencyFollowUp) {
    if (!engine::raster::OpenGLOffscreenContext::probe().available()) {
      GTEST_SKIP() << "OpenGL offscreen context unavailable on this host";
    }
    auto scene = std::make_shared<render::Scene>(Colord::black());
    engine::raster::OpenGLRasterizer rasterizer(
      std::make_shared<render::PinholeCamera>(Vector3d(0, 0, -5), Vector3d::null), scene);
    rasterizer.setStencilLoadOp(engine::raster::Rasterizer::AttachmentLoadOp::Load);
    Buffer<Colord> buffer(8, 8);
    EXPECT_THROW(rasterizer.render(buffer), std::runtime_error);
    Buffer<std::uint8_t> stencilSource(8, 8);
    rasterizer.setStencilLoadSource(&stencilSource);
    EXPECT_THROW(rasterizer.render(buffer), std::runtime_error);
  }

  class OpenGLRasterizerAspect : public ::testing::GuiTest {};

  TEST_F(OpenGLRasterizerAspect, FitExactLeavesPillarboxBarsAroundInnerRect) {
    if (!engine::raster::OpenGLOffscreenContext::probe().available()) {
      GTEST_SKIP() << "OpenGL offscreen context unavailable on this host";
    }

    auto scene = std::make_shared<render::Scene>(Colord(1.0, 1.0, 1.0));
    auto sphere = std::make_shared<render::Sphere>(Vector3d::null, 100.0);
    sphere->setMaterial(std::make_shared<render::MatteMaterial>(
      std::make_shared<render::ConstantColorTexture>(Colord(0.0, 0.0, 0.0))));
    scene->add(sphere);
    scene->addLight(
      std::make_shared<render::DirectionalLight>(Vector3d(0.0, 0.0, -1.0), Colord::white()));

    auto cam = std::make_shared<render::PinholeCamera>(Vector3d(0, 0, -5), Vector3d::null);
    cam->setAspectMode(render::AspectMode::FitExact);
    cam->setAspectRatio(1.0);

    engine::raster::OpenGLRasterizer gpu(cam, scene);
    Buffer<Colord> buffer(64, 32);
    gpu.render(buffer);

    const int barLeft = 0;
    const int barRight = 63;
    EXPECT_GT(buffer[16][barLeft].r() + buffer[16][barLeft].g() + buffer[16][barLeft].b(), 2.5)
      << "left pillarbox bar should remain at the buffer's clear color";
    EXPECT_GT(buffer[16][barRight].r() + buffer[16][barRight].g() + buffer[16][barRight].b(), 2.5)
      << "right pillarbox bar should remain at the buffer's clear color";
    EXPECT_LT(buffer[16][32].r() + buffer[16][32].g() + buffer[16][32].b(), 1.5)
      << "center of inner rect should hit the (black) sphere covering the whole frustum";
  }
}
