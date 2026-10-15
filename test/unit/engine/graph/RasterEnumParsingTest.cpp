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
