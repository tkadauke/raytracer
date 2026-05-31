#include <gtest/gtest.h>

#include "engine/raster/gl/Context.h"

namespace engine::raster::gl::tests {
  // No concrete backend lives in the engine library yet — this commit
  // only introduces the abstract type. The fixture confirms the header
  // compiles standalone and pins the `Availability` value type.

  TEST(GlAvailability, AvailableCarriesDetailAndNoError) {
    const Availability info = Availability::available("OpenGL 2.1 compatibility");
    EXPECT_TRUE(info.available());
    EXPECT_EQ("OpenGL 2.1 compatibility", info.detail());
    EXPECT_TRUE(info.error().empty());
  }

  TEST(GlAvailability, UnavailableCarriesErrorAndNoDetail) {
    const Availability info = Availability::unavailable("no GL context");
    EXPECT_FALSE(info.available());
    EXPECT_TRUE(info.detail().empty());
    EXPECT_EQ("no GL context", info.error());
  }
}
