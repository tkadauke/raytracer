#include <gtest/gtest.h>

#include "render/TracingAccumulationLayout.h"

#include <cstdint>
#include <stdexcept>
#include <string>

namespace TracingAccumulationLayoutTest {
  using namespace render;

  TEST(TracingAccumulationLayout, DefinesDefaultGpuAccumulationPlanes) {
    const TracingAccumulationLayout layout = TracingAccumulationLayout::image(640, 480);

    EXPECT_EQ(640, layout.width);
    EXPECT_EQ(480, layout.height);
    EXPECT_EQ(TracingAccumulationColorFormat::RGBA32Float, layout.colorSumFormat);
    EXPECT_EQ(TracingAccumulationSampleCountFormat::UInt32, layout.sampleCountFormat);
    EXPECT_EQ(TracingAccumulationMomentFormat::None, layout.momentFormat);
    EXPECT_EQ(TracingResolveFormat::RGBA8UnormSrgb, layout.resolveFormat);
    EXPECT_TRUE(layout.hasImageShape());
    EXPECT_FALSE(layout.hasMomentBuffer());
  }

  TEST(TracingAccumulationLayout, AccountsForHdrAccumulationAndLdrResolveSeparately) {
    const TracingAccumulationLayout layout = TracingAccumulationLayout::image(2, 3);

    EXPECT_EQ(6u, layout.pixelCount());
    EXPECT_EQ(96u, layout.colorSumBytes());
    EXPECT_EQ(24u, layout.sampleCountBytes());
    EXPECT_EQ(0u, layout.momentBytes());
    EXPECT_EQ(24u, layout.resolveBytes());
    EXPECT_EQ(120u, layout.accumulationBytes());
    EXPECT_EQ(144u, layout.totalBytes());
  }

  TEST(TracingAccumulationLayout, AddsOptionalSecondMomentPlane) {
    TracingAccumulationLayout layout = TracingAccumulationLayout::image(2, 3);
    layout.momentFormat = TracingAccumulationMomentFormat::RGBA32FloatSecondRawMoment;

    EXPECT_TRUE(layout.hasMomentBuffer());
    EXPECT_EQ(96u, layout.momentBytes());
    EXPECT_EQ(216u, layout.accumulationBytes());
    EXPECT_EQ(240u, layout.totalBytes());
  }

  TEST(TracingAccumulationLayout, NamesStableFormats) {
    EXPECT_EQ("rgba32_float", std::string(toString(TracingAccumulationColorFormat::RGBA32Float)));
    EXPECT_EQ("uint32", std::string(toString(TracingAccumulationSampleCountFormat::UInt32)));
    EXPECT_EQ("none", std::string(toString(TracingAccumulationMomentFormat::None)));
    EXPECT_EQ("rgba32_float_second_raw_moment",
              std::string(toString(TracingAccumulationMomentFormat::RGBA32FloatSecondRawMoment)));
    EXPECT_EQ("rgba8_unorm_srgb", std::string(toString(TracingResolveFormat::RGBA8UnormSrgb)));
  }

  TEST(TracingAccumulationLayout, RejectsMissingImageShape) {
    EXPECT_THROW(TracingAccumulationLayout::image(0, 1), std::invalid_argument);
    EXPECT_THROW(TracingAccumulationLayout::image(1, 0), std::invalid_argument);
    EXPECT_THROW(TracingAccumulationLayout::image(-1, 1), std::invalid_argument);
  }

  TEST(TracingAccumulationLayout, RejectsByteSizeOverflow) {
    TracingAccumulationLayout layout;
    layout.width = 1 << 29;
    layout.height = 1 << 29;

    EXPECT_NO_THROW(layout.validate());

    layout.width = static_cast<int>(0x7fffffff);
    layout.height = static_cast<int>(0x7fffffff);
    EXPECT_THROW(layout.validate(), std::overflow_error);
  }
}
