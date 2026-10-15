#pragma once

#include "engine/raster/Rasterizer.h"

#include <stdexcept>
#include <string>

namespace engine::graph::detail {
  /// Called after a caller-supplied error handler returns; handlers are
  /// expected to throw, so reaching this is a programming error.
  [[noreturn]] inline void errorHandlerReturned() {
    throw std::logic_error("enum parse error handler returned");
  }

  // Parsers for the string spellings of Rasterizer enums shared by the raster
  // pass state and the render engine options. Each takes the owner's error
  // handler `error(path, message)`, which must throw, so the wording of the
  // resulting exception stays specific to the caller.

  template<typename Error>
  inline engine::raster::Rasterizer::CullMode cullModeFromString(const std::string& value,
                                                                  const std::string& path,
                                                                  Error error) {
    using Rasterizer = engine::raster::Rasterizer;
    if (value == "both")
      return Rasterizer::CullMode::Both;
    if (value == "back")
      return Rasterizer::CullMode::Back;
    if (value == "front")
      return Rasterizer::CullMode::Front;
    error(path, "expected both, back, or front");
    errorHandlerReturned();
  }

  template<typename Error>
  inline engine::raster::Rasterizer::TessellationQuality
  tessellationQualityFromString(const std::string& value, const std::string& path, Error error) {
    using Rasterizer = engine::raster::Rasterizer;
    if (value == "preview")
      return Rasterizer::TessellationQuality::Preview;
    if (value == "balanced")
      return Rasterizer::TessellationQuality::Balanced;
    if (value == "final")
      return Rasterizer::TessellationQuality::Final;
    error(path, "expected preview, balanced, or final");
    errorHandlerReturned();
  }

  template<typename Error>
  inline engine::raster::Rasterizer::MSAAShadingMode
  msaaShadingModeFromString(const std::string& value, const std::string& path, Error error) {
    using Rasterizer = engine::raster::Rasterizer;
    if (value == "per_sample")
      return Rasterizer::MSAAShadingMode::PerSample;
    if (value == "per_fragment")
      return Rasterizer::MSAAShadingMode::PerFragment;
    error(path, "expected per_sample or per_fragment");
    errorHandlerReturned();
  }

  template<typename Error>
  inline engine::raster::Rasterizer::BlendFactor
  blendFactorFromString(const std::string& value, const std::string& path, Error error) {
    using Rasterizer = engine::raster::Rasterizer;
    if (value == "zero")
      return Rasterizer::BlendFactor::Zero;
    if (value == "one")
      return Rasterizer::BlendFactor::One;
    if (value == "source_color")
      return Rasterizer::BlendFactor::SourceColor;
    if (value == "one_minus_source_color")
      return Rasterizer::BlendFactor::OneMinusSourceColor;
    if (value == "source_alpha")
      return Rasterizer::BlendFactor::SourceAlpha;
    if (value == "one_minus_source_alpha")
      return Rasterizer::BlendFactor::OneMinusSourceAlpha;
    if (value == "destination_color")
      return Rasterizer::BlendFactor::DestinationColor;
    if (value == "one_minus_destination_color")
      return Rasterizer::BlendFactor::OneMinusDestinationColor;
    if (value == "constant_color")
      return Rasterizer::BlendFactor::ConstantColor;
    if (value == "one_minus_constant_color")
      return Rasterizer::BlendFactor::OneMinusConstantColor;
    if (value == "constant_alpha")
      return Rasterizer::BlendFactor::ConstantAlpha;
    if (value == "one_minus_constant_alpha")
      return Rasterizer::BlendFactor::OneMinusConstantAlpha;
    error(path, "unknown blend factor");
    errorHandlerReturned();
  }

  template<typename Error>
  inline engine::raster::Rasterizer::BlendOp
  blendOpFromString(const std::string& value, const std::string& path, Error error) {
    using Rasterizer = engine::raster::Rasterizer;
    if (value == "add")
      return Rasterizer::BlendOp::Add;
    if (value == "subtract")
      return Rasterizer::BlendOp::Subtract;
    if (value == "reverse_subtract")
      return Rasterizer::BlendOp::ReverseSubtract;
    if (value == "min")
      return Rasterizer::BlendOp::Min;
    if (value == "max")
      return Rasterizer::BlendOp::Max;
    error(path, "expected add, subtract, reverse_subtract, min, or max");
    errorHandlerReturned();
  }

  template<typename Error>
  inline engine::raster::Rasterizer::AlphaFunc
  alphaFuncFromString(const std::string& value, const std::string& path, Error error) {
    using Rasterizer = engine::raster::Rasterizer;
    if (value == "never")
      return Rasterizer::AlphaFunc::Never;
    if (value == "less")
      return Rasterizer::AlphaFunc::Less;
    if (value == "equal")
      return Rasterizer::AlphaFunc::Equal;
    if (value == "less_equal")
      return Rasterizer::AlphaFunc::LessEqual;
    if (value == "greater")
      return Rasterizer::AlphaFunc::Greater;
    if (value == "greater_equal")
      return Rasterizer::AlphaFunc::GreaterEqual;
    if (value == "not_equal")
      return Rasterizer::AlphaFunc::NotEqual;
    if (value == "always")
      return Rasterizer::AlphaFunc::Always;
    error(path, "unknown alpha function");
    errorHandlerReturned();
  }

  template<typename Error>
  inline engine::raster::Rasterizer::ShadowFilterMode
  shadowFilterModeFromString(const std::string& value, const std::string& path, Error error) {
    using Rasterizer = engine::raster::Rasterizer;
    if (value == "pcf")
      return Rasterizer::ShadowFilterMode::PCF;
    if (value == "pcss")
      return Rasterizer::ShadowFilterMode::PCSS;
    error(path, "expected pcf or pcss");
    errorHandlerReturned();
  }
}
