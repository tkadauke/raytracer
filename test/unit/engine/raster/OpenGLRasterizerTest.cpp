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

  TEST(OpenGLRasterizer, ProvidesSharedStatusMessage) {
    const std::string message = engine::raster::OpenGLRasterizer::statusMessage();

    EXPECT_FALSE(message.empty());
    EXPECT_NE(message.find("OpenGL raster backend"), std::string::npos);
  }
}
