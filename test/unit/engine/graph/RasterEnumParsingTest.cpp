#include "engine/graph/detail/RasterEnumParsing.h"

#include <gtest/gtest.h>

#include <stdexcept>

using engine::raster::Rasterizer;
namespace parsing = engine::graph::detail;

namespace {
  [[noreturn]] void failWith(const std::string& path, const std::string& message) {
    throw std::runtime_error(path + ": " + message);
  }
}

TEST(RasterEnumParsing, ParsesKnownSpellings) {
  EXPECT_EQ(Rasterizer::CullMode::Front, parsing::cullModeFromString("front", "p", failWith));
  EXPECT_EQ(Rasterizer::TessellationQuality::Balanced,
            parsing::tessellationQualityFromString("balanced", "p", failWith));
  EXPECT_EQ(Rasterizer::MSAAShadingMode::PerFragment,
            parsing::msaaShadingModeFromString("per_fragment", "p", failWith));
  EXPECT_EQ(Rasterizer::BlendFactor::OneMinusConstantAlpha,
            parsing::blendFactorFromString("one_minus_constant_alpha", "p", failWith));
  EXPECT_EQ(Rasterizer::BlendOp::ReverseSubtract,
            parsing::blendOpFromString("reverse_subtract", "p", failWith));
  EXPECT_EQ(Rasterizer::AlphaFunc::GreaterEqual,
            parsing::alphaFuncFromString("greater_equal", "p", failWith));
  EXPECT_EQ(Rasterizer::ShadowFilterMode::PCSS,
            parsing::shadowFilterModeFromString("pcss", "p", failWith));
}

TEST(RasterEnumParsing, ReportsUnknownSpellingsThroughCallerHandler) {
  try {
    parsing::cullModeFromString("sideways", "a.b", failWith);
    FAIL() << "expected an exception";
  } catch (const std::runtime_error& e) {
    EXPECT_STREQ("a.b: expected both, back, or front", e.what());
  }
  EXPECT_THROW(parsing::blendFactorFromString("bogus", "p", failWith), std::runtime_error);
  EXPECT_THROW(parsing::shadowFilterModeFromString("bogus", "p", failWith), std::runtime_error);
}

TEST(RasterEnumParsing, HandlerThatReturnsIsALogicError) {
  const auto returning = [](const std::string&, const std::string&) {};
  EXPECT_THROW(parsing::blendOpFromString("bogus", "p", returning), std::logic_error);
}

TEST(RasterEnumParsing, ParsesColorWriteMasks) {
  EXPECT_EQ(0, parsing::colorWriteMaskFromString("none", "p", failWith));
  EXPECT_EQ(Rasterizer::ColorWriteAll, parsing::colorWriteMaskFromString("all", "p", failWith));
  EXPECT_EQ(Rasterizer::ColorWriteRed | Rasterizer::ColorWriteBlue,
            parsing::colorWriteMaskFromString("rb", "p", failWith));
  try {
    parsing::colorWriteMaskFromString("rx", "a.b", failWith);
    FAIL() << "expected an exception";
  } catch (const std::runtime_error& e) {
    EXPECT_STREQ("a.b: expected r, g, b, all, or none", e.what());
  }
}

TEST(RasterEnumParsing, SpellsColorWriteChannels) {
  EXPECT_EQ("none", parsing::colorWriteChannelsString(0));
  EXPECT_EQ("rgb", parsing::colorWriteChannelsString(Rasterizer::ColorWriteAll));
  EXPECT_EQ("rb", parsing::colorWriteChannelsString(Rasterizer::ColorWriteRed |
                                                    Rasterizer::ColorWriteBlue));
}
