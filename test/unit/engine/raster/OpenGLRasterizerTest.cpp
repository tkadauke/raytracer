#include <gtest/gtest.h>

#include "core/Buffer.h"
#include "engine/raster/OpenGLOffscreenContext.h"
#include "engine/raster/OpenGLRasterizer.h"

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
    rasterizer.setCullMode(engine::raster::Rasterizer::CullMode::Back);
    rasterizer.setViewportRect(Recti(4, 5, 20, 21));
    rasterizer.setScissorRect(Recti(6, 7, 18, 19));
    rasterizer.setColorWriteMask(engine::raster::Rasterizer::ColorWriteGreen);
    rasterizer.setBlendingEnabled(true);
    rasterizer.setBlendFactors(engine::raster::Rasterizer::BlendFactor::ConstantAlpha,
                               engine::raster::Rasterizer::BlendFactor::OneMinusConstantAlpha);
    rasterizer.setBlendOp(engine::raster::Rasterizer::BlendOp::Max);
    rasterizer.setBlendConstant(Colord(0.1, 0.2, 0.3), 0.4);
    rasterizer.setAlphaTestEnabled(true);
    rasterizer.setAlphaFunc(engine::raster::Rasterizer::AlphaFunc::Greater, 0.6);
    rasterizer.setDepthFunc(engine::raster::Rasterizer::DepthFunc::GreaterEqual);
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

    auto clone =
      std::dynamic_pointer_cast<engine::raster::OpenGLRasterizer>(rasterizer.cloneForRender());

    ASSERT_NE(nullptr, clone);
    EXPECT_EQ(4, clone->msaaSamples());
    EXPECT_TRUE(clone->hasCullModeOverride());
    EXPECT_EQ(engine::raster::Rasterizer::CullMode::Back, clone->cullMode());
    EXPECT_TRUE(clone->viewportEnabled());
    EXPECT_EQ(4, clone->viewportRect().left());
    EXPECT_EQ(20, clone->viewportRect().width());
    EXPECT_TRUE(clone->scissorTestEnabled());
    EXPECT_EQ(6, clone->scissorRect().left());
    EXPECT_EQ(18, clone->scissorRect().width());
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

  TEST(OpenGLRasterizer, DoesNotReportReadbackBeforeSuccessfulRender) {
    engine::raster::OpenGLRasterizer rasterizer(nullptr);

    EXPECT_TRUE(rasterizer.readbackTraceMessage().empty());
    EXPECT_TRUE(rasterizer.traceMessages().empty());
  }

  TEST(OpenGLRasterizer, ProvidesSharedStatusMessage) {
    const std::string message = engine::raster::OpenGLRasterizer::statusMessage();

    EXPECT_FALSE(message.empty());
    EXPECT_NE(message.find("OpenGL raster backend"), std::string::npos);
  }
}
