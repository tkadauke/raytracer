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

#include <QOpenGLContext>
#include <QOpenGLFunctions>

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
    if (!context.create(2, 2) || !context.makeCurrent() || !context.bindFramebuffer()) {
      SUCCEED() << "OpenGL context unavailable on this host";
      return;
    }

    QOpenGLFunctions* functions = QOpenGLContext::currentContext()->functions();
    functions->glViewport(0, 0, 2, 2);
    functions->glDisable(GL_BLEND);
    functions->glClearColor(0.24f, 0.27f, 0.30f, 0.30f);
    functions->glClear(GL_COLOR_BUFFER_BIT);

    Buffer<Colord> buffer(2, 2);
    context.copyColorTo(buffer);

    context.releaseFramebuffer();
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
}
