#include <gtest/gtest.h>

#include "core/Buffer.h"
#include "engine/raster/OpenGLOffscreenContext.h"
#include "engine/raster/OpenGLRasterizer.h"

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

  TEST(OpenGLRasterizer, FailsClearlyUntilDrawPathExists) {
    engine::raster::OpenGLRasterizer rasterizer(nullptr);
    Buffer<Colord> buffer(2, 2);

    try {
      rasterizer.render(buffer);
      FAIL() << "expected OpenGL rasterizer shell to report an unavailable draw path";
    } catch (const std::runtime_error& error) {
      EXPECT_NE(std::string(error.what()).find("OpenGL raster backend"), std::string::npos);
    }
  }

  TEST(OpenGLRasterizer, ProvidesSharedStatusMessage) {
    const std::string message = engine::raster::OpenGLRasterizer::statusMessage();

    EXPECT_FALSE(message.empty());
    EXPECT_NE(message.find("OpenGL raster backend"), std::string::npos);
  }
}
