#include "engine/graph/RasterPassState.h"

#include "engine/graph/detail/JsonStateHelpers.h"
#include "engine/graph/RenderPlan.h"
#include "engine/raster/OpenGLRasterizer.h"
#include "core/util/QStringUtil.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <utility>

namespace engine::graph {
  namespace {
    using Rasterizer = engine::raster::Rasterizer;

    [[noreturn]] void stateError(const std::string& path, const std::string& message) {
      throw std::runtime_error("Invalid raster pass state at " + path + ": " + message);
    }

    [[noreturn]] void openGLUnsupported(const std::string& feature) {
      throw std::runtime_error("OpenGL raster backend does not support " + feature +
                               " yet; use raster backend 'cpu'");
    }


    const char* toString(Rasterizer::CullMode mode) {
      return detail::enumName<Rasterizer::CullMode>(mode,
                                            {{Rasterizer::CullMode::Both, "both"},
                                             {Rasterizer::CullMode::Back, "back"},
                                             {Rasterizer::CullMode::Front, "front"}},
                                            "both");
    }

    const char* toString(Rasterizer::TessellationQuality quality) {
      return detail::enumName<Rasterizer::TessellationQuality>(
        quality,
        {{Rasterizer::TessellationQuality::Preview, "preview"},
         {Rasterizer::TessellationQuality::Balanced, "balanced"},
         {Rasterizer::TessellationQuality::Final, "final"}},
        "balanced");
    }

    Rasterizer::TessellationQuality tessellationQualityFromString(const std::string& value,
                                                                  const std::string& path) {
      if (value == "preview")
        return Rasterizer::TessellationQuality::Preview;
      if (value == "balanced")
        return Rasterizer::TessellationQuality::Balanced;
      if (value == "final")
        return Rasterizer::TessellationQuality::Final;
      stateError(path, "expected preview, balanced, or final");
    }

    Rasterizer::CullMode cullModeFromString(const std::string& value, const std::string& path) {
      if (value == "both")
        return Rasterizer::CullMode::Both;
      if (value == "back")
        return Rasterizer::CullMode::Back;
      if (value == "front")
        return Rasterizer::CullMode::Front;
      stateError(path, "expected both, back, or front");
    }

    const char* toString(Rasterizer::MSAAShadingMode mode) {
      return detail::enumName<Rasterizer::MSAAShadingMode>(
        mode,
        {{Rasterizer::MSAAShadingMode::PerSample, "per_sample"},
         {Rasterizer::MSAAShadingMode::PerFragment, "per_fragment"}},
        "per_sample");
    }

    Rasterizer::MSAAShadingMode msaaShadingModeFromString(const std::string& value,
                                                          const std::string& path) {
      if (value == "per_sample")
        return Rasterizer::MSAAShadingMode::PerSample;
      if (value == "per_fragment")
        return Rasterizer::MSAAShadingMode::PerFragment;
      stateError(path, "expected per_sample or per_fragment");
    }

    const char* toString(Rasterizer::DepthPrepassMode mode) {
      return detail::enumName<Rasterizer::DepthPrepassMode>(mode,
                                                    {{Rasterizer::DepthPrepassMode::Off, "off"},
                                                     {Rasterizer::DepthPrepassMode::On, "on"},
                                                     {Rasterizer::DepthPrepassMode::Auto, "auto"}},
                                                    "off");
    }

    Rasterizer::DepthPrepassMode depthPrepassModeFromString(const std::string& value,
                                                            const std::string& path) {
      if (value == "off")
        return Rasterizer::DepthPrepassMode::Off;
      if (value == "on")
        return Rasterizer::DepthPrepassMode::On;
      if (value == "auto")
        return Rasterizer::DepthPrepassMode::Auto;
      stateError(path, "expected off, on, or auto");
    }

    const char* toString(Rasterizer::PostProcessAA aa) {
      return detail::enumName<Rasterizer::PostProcessAA>(aa,
                                                 {{Rasterizer::PostProcessAA::None, "none"},
                                                  {Rasterizer::PostProcessAA::FXAA, "fxaa"},
                                                  {Rasterizer::PostProcessAA::SMAA, "smaa"},
                                                  {Rasterizer::PostProcessAA::TAA, "taa"}},
                                                 "none");
    }

    Rasterizer::PostProcessAA postProcessAAFromString(const std::string& value,
                                                      const std::string& path) {
      if (value == "none")
        return Rasterizer::PostProcessAA::None;
      if (value == "fxaa")
        return Rasterizer::PostProcessAA::FXAA;
      if (value == "smaa")
        return Rasterizer::PostProcessAA::SMAA;
      if (value == "taa")
        return Rasterizer::PostProcessAA::TAA;
      stateError(path, "expected none, fxaa, smaa, or taa");
    }

    const char* toString(Rasterizer::BlendFactor factor) {
      return detail::enumName<Rasterizer::BlendFactor>(
        factor,
        {{Rasterizer::BlendFactor::Zero, "zero"},
         {Rasterizer::BlendFactor::One, "one"},
         {Rasterizer::BlendFactor::SourceColor, "source_color"},
         {Rasterizer::BlendFactor::OneMinusSourceColor, "one_minus_source_color"},
         {Rasterizer::BlendFactor::SourceAlpha, "source_alpha"},
         {Rasterizer::BlendFactor::OneMinusSourceAlpha, "one_minus_source_alpha"},
         {Rasterizer::BlendFactor::DestinationColor, "destination_color"},
         {Rasterizer::BlendFactor::OneMinusDestinationColor, "one_minus_destination_color"},
         {Rasterizer::BlendFactor::ConstantColor, "constant_color"},
         {Rasterizer::BlendFactor::OneMinusConstantColor, "one_minus_constant_color"},
         {Rasterizer::BlendFactor::ConstantAlpha, "constant_alpha"},
         {Rasterizer::BlendFactor::OneMinusConstantAlpha, "one_minus_constant_alpha"}},
        "one");
    }

    Rasterizer::BlendFactor blendFactorFromString(const std::string& value,
                                                  const std::string& path) {
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
      stateError(path, "unknown blend factor");
    }

    const char* toString(Rasterizer::BlendOp op) {
      return detail::enumName<Rasterizer::BlendOp>(
        op,
        {{Rasterizer::BlendOp::Add, "add"},
         {Rasterizer::BlendOp::Subtract, "subtract"},
         {Rasterizer::BlendOp::ReverseSubtract, "reverse_subtract"},
         {Rasterizer::BlendOp::Min, "min"},
         {Rasterizer::BlendOp::Max, "max"}},
        "add");
    }

    Rasterizer::BlendOp blendOpFromString(const std::string& value, const std::string& path) {
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
      stateError(path, "expected add, subtract, reverse_subtract, min, or max");
    }

    const char* toString(Rasterizer::DepthFunc func) {
      return detail::enumName<Rasterizer::DepthFunc>(
        func,
        {{Rasterizer::DepthFunc::Never, "never"},
         {Rasterizer::DepthFunc::Less, "less"},
         {Rasterizer::DepthFunc::Equal, "equal"},
         {Rasterizer::DepthFunc::LessEqual, "less_equal"},
         {Rasterizer::DepthFunc::Greater, "greater"},
         {Rasterizer::DepthFunc::GreaterEqual, "greater_equal"},
         {Rasterizer::DepthFunc::NotEqual, "not_equal"},
         {Rasterizer::DepthFunc::Always, "always"}},
        "less");
    }

    Rasterizer::DepthFunc depthFuncFromString(const std::string& value, const std::string& path) {
      if (value == "never")
        return Rasterizer::DepthFunc::Never;
      if (value == "less")
        return Rasterizer::DepthFunc::Less;
      if (value == "equal")
        return Rasterizer::DepthFunc::Equal;
      if (value == "less_equal")
        return Rasterizer::DepthFunc::LessEqual;
      if (value == "greater")
        return Rasterizer::DepthFunc::Greater;
      if (value == "greater_equal")
        return Rasterizer::DepthFunc::GreaterEqual;
      if (value == "not_equal")
        return Rasterizer::DepthFunc::NotEqual;
      if (value == "always")
        return Rasterizer::DepthFunc::Always;
      stateError(path, "unknown depth function");
    }

    const char* toString(Rasterizer::AlphaFunc func) {
      return detail::enumName<Rasterizer::AlphaFunc>(
        func,
        {{Rasterizer::AlphaFunc::Never, "never"},
         {Rasterizer::AlphaFunc::Less, "less"},
         {Rasterizer::AlphaFunc::Equal, "equal"},
         {Rasterizer::AlphaFunc::LessEqual, "less_equal"},
         {Rasterizer::AlphaFunc::Greater, "greater"},
         {Rasterizer::AlphaFunc::GreaterEqual, "greater_equal"},
         {Rasterizer::AlphaFunc::NotEqual, "not_equal"},
         {Rasterizer::AlphaFunc::Always, "always"}},
        "always");
    }

    Rasterizer::AlphaFunc alphaFuncFromString(const std::string& value, const std::string& path) {
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
      stateError(path, "unknown alpha function");
    }

    const char* toString(Rasterizer::StencilFunc func) {
      return detail::enumName<Rasterizer::StencilFunc>(
        func,
        {{Rasterizer::StencilFunc::Never, "never"},
         {Rasterizer::StencilFunc::Less, "less"},
         {Rasterizer::StencilFunc::Equal, "equal"},
         {Rasterizer::StencilFunc::LessEqual, "less_equal"},
         {Rasterizer::StencilFunc::Greater, "greater"},
         {Rasterizer::StencilFunc::GreaterEqual, "greater_equal"},
         {Rasterizer::StencilFunc::NotEqual, "not_equal"},
         {Rasterizer::StencilFunc::Always, "always"}},
        "always");
    }

    Rasterizer::StencilFunc stencilFuncFromString(const std::string& value,
                                                  const std::string& path) {
      if (value == "never")
        return Rasterizer::StencilFunc::Never;
      if (value == "less")
        return Rasterizer::StencilFunc::Less;
      if (value == "equal")
        return Rasterizer::StencilFunc::Equal;
      if (value == "less_equal")
        return Rasterizer::StencilFunc::LessEqual;
      if (value == "greater")
        return Rasterizer::StencilFunc::Greater;
      if (value == "greater_equal")
        return Rasterizer::StencilFunc::GreaterEqual;
      if (value == "not_equal")
        return Rasterizer::StencilFunc::NotEqual;
      if (value == "always")
        return Rasterizer::StencilFunc::Always;
      stateError(path, "unknown stencil function");
    }

    const char* toString(Rasterizer::StencilOp op) {
      return detail::enumName<Rasterizer::StencilOp>(
        op,
        {{Rasterizer::StencilOp::Keep, "keep"},
         {Rasterizer::StencilOp::Zero, "zero"},
         {Rasterizer::StencilOp::Replace, "replace"},
         {Rasterizer::StencilOp::IncrementClamp, "increment_clamp"},
         {Rasterizer::StencilOp::DecrementClamp, "decrement_clamp"},
         {Rasterizer::StencilOp::Invert, "invert"}},
        "keep");
    }

    Rasterizer::StencilOp stencilOpFromString(const std::string& value, const std::string& path) {
      if (value == "keep")
        return Rasterizer::StencilOp::Keep;
      if (value == "zero")
        return Rasterizer::StencilOp::Zero;
      if (value == "replace")
        return Rasterizer::StencilOp::Replace;
      if (value == "increment_clamp")
        return Rasterizer::StencilOp::IncrementClamp;
      if (value == "decrement_clamp")
        return Rasterizer::StencilOp::DecrementClamp;
      if (value == "invert")
        return Rasterizer::StencilOp::Invert;
      stateError(path, "unknown stencil operation");
    }

    const char* toString(Rasterizer::AttachmentLoadOp op) {
      return detail::enumName<Rasterizer::AttachmentLoadOp>(op,
                                                    {{Rasterizer::AttachmentLoadOp::Clear, "clear"},
                                                     {Rasterizer::AttachmentLoadOp::Load, "load"}},
                                                    "clear");
    }

    Rasterizer::AttachmentLoadOp attachmentLoadOpFromString(const std::string& value,
                                                            const std::string& path) {
      if (value == "clear")
        return Rasterizer::AttachmentLoadOp::Clear;
      if (value == "load")
        return Rasterizer::AttachmentLoadOp::Load;
      stateError(path, "expected clear or load");
    }

    const char* toString(Rasterizer::AttachmentStoreOp op) {
      return detail::enumName<Rasterizer::AttachmentStoreOp>(
        op,
        {{Rasterizer::AttachmentStoreOp::Store, "store"},
         {Rasterizer::AttachmentStoreOp::Discard, "discard"}},
        "store");
    }

    Rasterizer::AttachmentStoreOp attachmentStoreOpFromString(const std::string& value,
                                                              const std::string& path) {
      if (value == "store")
        return Rasterizer::AttachmentStoreOp::Store;
      if (value == "discard")
        return Rasterizer::AttachmentStoreOp::Discard;
      stateError(path, "expected store or discard");
    }

    const char* toString(Rasterizer::ShadowFilterMode mode) {
      return detail::enumName<Rasterizer::ShadowFilterMode>(
        mode,
        {{Rasterizer::ShadowFilterMode::PCF, "pcf"}, {Rasterizer::ShadowFilterMode::PCSS, "pcss"}},
        "pcf");
    }

    Rasterizer::ShadowFilterMode shadowFilterModeFromString(const std::string& value,
                                                            const std::string& path) {
      if (value == "pcf")
        return Rasterizer::ShadowFilterMode::PCF;
      if (value == "pcss")
        return Rasterizer::ShadowFilterMode::PCSS;
      stateError(path, "expected pcf or pcss");
    }

    std::string colorWriteMaskString(std::uint8_t mask) {
      mask &= Rasterizer::ColorWriteAll;
      if (mask == 0)
        return "none";

      std::string result;
      if (mask & Rasterizer::ColorWriteRed)
        result.push_back('r');
      if (mask & Rasterizer::ColorWriteGreen)
        result.push_back('g');
      if (mask & Rasterizer::ColorWriteBlue)
        result.push_back('b');
      return result;
    }

    std::uint8_t colorWriteMaskFromString(const std::string& value, const std::string& path) {
      if (value == "none")
        return 0;
      if (value == "all")
        return Rasterizer::ColorWriteAll;

      std::uint8_t mask = 0;
      for (const char ch : value) {
        if (ch == 'r') {
          mask |= Rasterizer::ColorWriteRed;
        } else if (ch == 'g') {
          mask |= Rasterizer::ColorWriteGreen;
        } else if (ch == 'b') {
          mask |= Rasterizer::ColorWriteBlue;
        } else {
          stateError(path, "expected r, g, b, all, or none");
        }
      }
      return mask;
    }
  }

  RasterExecutionState RasterExecutionState::fromJson(const QJsonObject& object,
                                                      const std::string& path) {
    detail::rejectUnknownFields(object, path, {"threads", "queueSize", "backend"}, stateError);
    RasterExecutionState state;
    if (detail::hasField(object, "threads"))
      state.setMaximumThreads(detail::intField(object, "threads", path, stateError));
    if (detail::hasField(object, "queueSize"))
      state.setQueueSize(detail::intField(object, "queueSize", path, stateError));
    if (detail::hasField(object, "backend"))
      state.setBackend(engine::raster::RasterBackend::fromString(
        detail::stringField(object, "backend", path, stateError), path + ".backend"));
    return state;
  }

  QJsonObject RasterExecutionState::toJson() const {
    QJsonObject object;
    if (m_maximumThreads)
      object["threads"] = *m_maximumThreads;
    if (m_queueSize)
      object["queueSize"] = *m_queueSize;
    if (m_backend && !m_backend->isCPU())
      object["backend"] = qstr(m_backend->id());
    return object;
  }

  bool RasterExecutionState::empty() const {
    return toJson().isEmpty();
  }

  void RasterExecutionState::applyTo(Rasterizer& rasterizer) const {
    if (m_maximumThreads)
      rasterizer.setMaximumThreads(*m_maximumThreads);
    if (m_queueSize) {
      rasterizer.setQueueSize(*m_queueSize);
    } else {
      rasterizer.setAutomaticQueueSize();
    }
  }

  void RasterExecutionState::setMaximumThreads(int threads) {
    m_maximumThreads = atLeast(1, threads);
  }

  void RasterExecutionState::setQueueSize(int queueSize) {
    m_queueSize = atLeast(1, queueSize);
  }

  void RasterExecutionState::setBackend(engine::raster::RasterBackend backend) {
    m_backend = backend;
  }

  engine::raster::RasterBackend RasterExecutionState::backend() const {
    return m_backend.value_or(engine::raster::RasterBackend::cpu());
  }

  RasterGeometryState RasterGeometryState::fromJson(const QJsonObject& object,
                                                    const std::string& path) {
    detail::rejectUnknownFields(object, path, {"lod", "quality", "maxScreenSpaceError", "cullMode"}, stateError);
    RasterGeometryState state;
    if (detail::hasField(object, "lod"))
      state.setLod(detail::intField(object, "lod", path, stateError));
    if (detail::hasField(object, "quality"))
      state.setTessellationQuality(
        tessellationQualityFromString(detail::stringField(object, "quality", path, stateError), path + ".quality"));
    if (detail::hasField(object, "maxScreenSpaceError"))
      state.setMaximumScreenSpaceError(detail::doubleField(object, "maxScreenSpaceError", path, stateError));
    if (detail::hasField(object, "cullMode"))
      state.setCullMode(
        cullModeFromString(detail::stringField(object, "cullMode", path, stateError), path + ".cullMode"));
    return state;
  }

  QJsonObject RasterGeometryState::toJson() const {
    QJsonObject object;
    if (m_lod != 0)
      object["lod"] = m_lod;
    if (m_tessellationQuality != Rasterizer::TessellationQuality::Balanced)
      object["quality"] = toString(m_tessellationQuality);
    if (m_maximumScreenSpaceError)
      object["maxScreenSpaceError"] = *m_maximumScreenSpaceError;
    if (m_cullMode)
      object["cullMode"] = toString(*m_cullMode);
    return object;
  }

  bool RasterGeometryState::empty() const {
    return toJson().isEmpty();
  }

  void RasterGeometryState::applyTo(Rasterizer& rasterizer) const {
    rasterizer.setLod(m_lod);
    rasterizer.setTessellationQuality(m_tessellationQuality);
    if (m_maximumScreenSpaceError) {
      rasterizer.setMaximumScreenSpaceError(*m_maximumScreenSpaceError);
    } else {
      rasterizer.clearMaximumScreenSpaceErrorOverride();
    }
    if (m_cullMode) {
      rasterizer.setCullMode(*m_cullMode);
    } else {
      rasterizer.clearCullModeOverride();
    }
  }

  void RasterGeometryState::applyTo(engine::raster::OpenGLRasterizer& rasterizer) const {
    rasterizer.setLod(m_lod);
    rasterizer.setTessellationQuality(m_tessellationQuality);
    if (m_maximumScreenSpaceError) {
      rasterizer.setMaximumScreenSpaceError(*m_maximumScreenSpaceError);
    } else {
      rasterizer.clearMaximumScreenSpaceErrorOverride();
    }
    if (m_cullMode) {
      rasterizer.setCullMode(*m_cullMode);
    } else {
      rasterizer.clearCullModeOverride();
    }
  }

  void RasterGeometryState::setLod(int lod) {
    m_lod = detail::clampedLod(lod);
  }

  void RasterGeometryState::setTessellationQuality(Rasterizer::TessellationQuality quality) {
    m_tessellationQuality = quality;
  }

  void RasterGeometryState::setMaximumScreenSpaceError(double pixels) {
    m_maximumScreenSpaceError = finiteAtLeast(0.0, pixels);
  }

  void RasterGeometryState::clearMaximumScreenSpaceErrorOverride() {
    m_maximumScreenSpaceError.reset();
  }

  void RasterGeometryState::setCullMode(Rasterizer::CullMode mode) {
    m_cullMode = mode;
  }

  int RasterGeometryState::lod() const {
    return m_lod;
  }

  Rasterizer::TessellationQuality RasterGeometryState::tessellationQuality() const {
    return m_tessellationQuality;
  }

  double RasterGeometryState::maximumScreenSpaceError() const {
    return m_maximumScreenSpaceError.value_or(
      Rasterizer::presetScreenSpaceError(m_tessellationQuality));
  }

  bool RasterGeometryState::hasMaximumScreenSpaceErrorOverride() const {
    return m_maximumScreenSpaceError.has_value();
  }

  std::optional<Rasterizer::CullMode> RasterGeometryState::cullModeOverride() const {
    return m_cullMode;
  }

  Rasterizer::CullMode RasterGeometryState::cullMode() const {
    return m_cullMode.value_or(Rasterizer::CullMode::Both);
  }

  bool RasterGeometryState::hasCullModeOverride() const {
    return m_cullMode.has_value();
  }

  RasterSamplingState RasterSamplingState::fromJson(const QJsonObject& object,
                                                    const std::string& path) {
    detail::rejectUnknownFields(object, path, {"msaaSamples", "msaaShadingMode", "postProcessAA"}, stateError);
    RasterSamplingState state;
    if (detail::hasField(object, "msaaSamples"))
      state.setMSAASamples(detail::intField(object, "msaaSamples", path, stateError));
    if (detail::hasField(object, "msaaShadingMode"))
      state.setMSAAShadingMode(msaaShadingModeFromString(
        detail::stringField(object, "msaaShadingMode", path, stateError), path + ".msaaShadingMode"));
    if (detail::hasField(object, "postProcessAA"))
      state.setPostProcessAA(postProcessAAFromString(detail::stringField(object, "postProcessAA", path, stateError),
                                                     path + ".postProcessAA"));
    return state;
  }

  QJsonObject RasterSamplingState::toJson() const {
    QJsonObject object;
    if (m_msaaSamples != 1)
      object["msaaSamples"] = m_msaaSamples;
    if (m_msaaShadingMode != Rasterizer::MSAAShadingMode::PerSample || m_msaaSamples != 1)
      object["msaaShadingMode"] = toString(m_msaaShadingMode);
    if (m_postProcessAA != Rasterizer::PostProcessAA::None)
      object["postProcessAA"] = toString(m_postProcessAA);
    return object;
  }

  bool RasterSamplingState::empty() const {
    return toJson().isEmpty();
  }

  void RasterSamplingState::applyTo(Rasterizer& rasterizer) const {
    rasterizer.setMSAASamples(m_msaaSamples);
    rasterizer.setMSAAShadingMode(m_msaaShadingMode);
    rasterizer.setPostProcessAA(m_postProcessAA);
  }

  void RasterSamplingState::applyTo(engine::raster::OpenGLRasterizer& rasterizer) const {
    validateSupportedByOpenGL();
    rasterizer.setMSAASamples(m_msaaSamples);
    rasterizer.setMSAAShadingMode(m_msaaShadingMode);
  }

  void RasterSamplingState::validateSupportedByOpenGL() const {
    if (m_postProcessAA != Rasterizer::PostProcessAA::None) {
      openGLUnsupported("raster pass post-process anti-aliasing");
    }
  }

  void RasterSamplingState::setMSAASamples(int samples) {
    if (samples <= 1) {
      m_msaaSamples = 1;
    } else if (samples <= 2) {
      m_msaaSamples = 2;
    } else if (samples <= 4) {
      m_msaaSamples = 4;
    } else {
      m_msaaSamples = 8;
    }
  }

  void RasterSamplingState::setMSAAShadingMode(Rasterizer::MSAAShadingMode mode) {
    m_msaaShadingMode = mode;
  }

  void RasterSamplingState::setPostProcessAA(Rasterizer::PostProcessAA aa) {
    m_postProcessAA = aa;
  }

  int RasterSamplingState::msaaSamples() const {
    return m_msaaSamples;
  }

  Rasterizer::MSAAShadingMode RasterSamplingState::msaaShadingMode() const {
    return m_msaaShadingMode;
  }

  RasterDepthPrepassState RasterDepthPrepassState::fromJson(const QJsonObject& object,
                                                            const std::string& path) {
    detail::rejectUnknownFields(object, path, {"mode"}, stateError);
    RasterDepthPrepassState state;
    if (detail::hasField(object, "mode"))
      state.setMode(depthPrepassModeFromString(detail::stringField(object, "mode", path, stateError), path + ".mode"));
    return state;
  }

  QJsonObject RasterDepthPrepassState::toJson() const {
    QJsonObject object;
    if (m_mode != Rasterizer::DepthPrepassMode::Off)
      object["mode"] = toString(m_mode);
    return object;
  }

  bool RasterDepthPrepassState::empty() const {
    return toJson().isEmpty();
  }

  void RasterDepthPrepassState::applyTo(Rasterizer& rasterizer) const {
    rasterizer.setDepthPrepassMode(m_mode);
  }

  void RasterDepthPrepassState::applyTo(engine::raster::OpenGLRasterizer&) const {
    if (m_mode != Rasterizer::DepthPrepassMode::Off) {
      openGLUnsupported("measured depth prepass");
    }
  }

  void RasterDepthPrepassState::setMode(Rasterizer::DepthPrepassMode mode) {
    m_mode = mode;
  }

  Rasterizer::DepthPrepassMode RasterDepthPrepassState::mode() const {
    return m_mode;
  }

  RasterFramebufferState RasterFramebufferState::fromJson(const QJsonObject& object,
                                                          const std::string& path) {
    detail::rejectUnknownFields(object, path,
                        {"viewport",
                         "scissor",
                         "colorLoadOp",
                         "colorStoreOp",
                         "depthFunc",
                         "depthBias",
                         "depthClearValue",
                         "depthLoadOp",
                         "depthStoreOp",
                         "depthWrite",
                         "colorWriteMask",
                         "blending",
                         "blendSource",
                         "blendDestination",
                         "blendOp",
                         "blendConstantColor",
                         "blendConstantAlpha",
                         "alphaTest",
                         "alphaFunc",
                         "alphaReference",
                         "stencilTest",
                         "stencilFunc",
                         "stencilReference",
                         "stencilMask",
                         "stencilClearValue",
                         "stencilLoadOp",
                         "stencilStoreOp",
                         "stencilWriteMask",
                         "stencilFailOp",
                         "stencilDepthFailOp",
                         "stencilPassOp"}, stateError);
    RasterFramebufferState state;
    if (detail::hasField(object, "viewport"))
      state.setViewportRect(detail::rectFromJson(object, "viewport", path, stateError));
    if (detail::hasField(object, "scissor"))
      state.setScissorRect(detail::rectFromJson(object, "scissor", path, stateError));
    if (detail::hasField(object, "colorLoadOp"))
      state.setColorLoadOp(attachmentLoadOpFromString(detail::stringField(object, "colorLoadOp", path, stateError),
                                                      path + ".colorLoadOp"));
    if (detail::hasField(object, "colorStoreOp"))
      state.setColorStoreOp(attachmentStoreOpFromString(detail::stringField(object, "colorStoreOp", path, stateError),
                                                        path + ".colorStoreOp"));
    if (detail::hasField(object, "depthFunc"))
      state.setDepthFunc(
        depthFuncFromString(detail::stringField(object, "depthFunc", path, stateError), path + ".depthFunc"));
    if (detail::hasField(object, "depthBias"))
      state.setDepthBias(detail::doubleField(object, "depthBias", path, stateError));
    if (detail::hasField(object, "depthClearValue"))
      state.setDepthClearValue(detail::doubleField(object, "depthClearValue", path, stateError));
    if (detail::hasField(object, "depthLoadOp"))
      state.setDepthLoadOp(attachmentLoadOpFromString(detail::stringField(object, "depthLoadOp", path, stateError),
                                                      path + ".depthLoadOp"));
    if (detail::hasField(object, "depthStoreOp"))
      state.setDepthStoreOp(attachmentStoreOpFromString(detail::stringField(object, "depthStoreOp", path, stateError),
                                                        path + ".depthStoreOp"));
    if (detail::hasField(object, "depthWrite"))
      state.setDepthWriteEnabled(detail::boolField(object, "depthWrite", path, stateError));
    if (detail::hasField(object, "colorWriteMask"))
      state.setColorWriteMask(colorWriteMaskFromString(detail::stringField(object, "colorWriteMask", path, stateError),
                                                       path + ".colorWriteMask"));
    if (detail::hasField(object, "blending"))
      state.setBlendingEnabled(detail::boolField(object, "blending", path, stateError));
    if (detail::hasField(object, "blendSource") || detail::hasField(object, "blendDestination")) {
      const auto source =
        detail::hasField(object, "blendSource")
          ? blendFactorFromString(detail::stringField(object, "blendSource", path, stateError), path + ".blendSource")
          : Rasterizer::BlendFactor::One;
      const auto destination =
        detail::hasField(object, "blendDestination")
          ? blendFactorFromString(detail::stringField(object, "blendDestination", path, stateError),
                                  path + ".blendDestination")
          : Rasterizer::BlendFactor::Zero;
      state.setBlendFactors(source, destination);
    }
    if (detail::hasField(object, "blendOp"))
      state.setBlendOp(blendOpFromString(detail::stringField(object, "blendOp", path, stateError), path + ".blendOp"));
    if (detail::hasField(object, "blendConstantColor") || detail::hasField(object, "blendConstantAlpha")) {
      const Colord color = detail::hasField(object, "blendConstantColor")
                             ? detail::colorFromJson(object, "blendConstantColor", path, stateError)
                             : Colord::white();
      const double alpha = detail::hasField(object, "blendConstantAlpha")
                             ? detail::doubleField(object, "blendConstantAlpha", path, stateError)
                             : 1.0;
      state.setBlendConstant(color, alpha);
    }
    if (detail::hasField(object, "alphaTest"))
      state.setAlphaTestEnabled(detail::boolField(object, "alphaTest", path, stateError));
    if (detail::hasField(object, "alphaFunc") || detail::hasField(object, "alphaReference")) {
      const auto func =
        detail::hasField(object, "alphaFunc")
          ? alphaFuncFromString(detail::stringField(object, "alphaFunc", path, stateError), path + ".alphaFunc")
          : Rasterizer::AlphaFunc::Always;
      const double reference =
        detail::hasField(object, "alphaReference") ? detail::doubleField(object, "alphaReference", path, stateError) : 0.0;
      state.setAlphaFunc(func, reference);
    }
    if (detail::hasField(object, "stencilTest"))
      state.setStencilTestEnabled(detail::boolField(object, "stencilTest", path, stateError));
    if (detail::hasField(object, "stencilFunc") || detail::hasField(object, "stencilReference") ||
        detail::hasField(object, "stencilMask")) {
      const auto func =
        detail::hasField(object, "stencilFunc")
          ? stencilFuncFromString(detail::stringField(object, "stencilFunc", path, stateError), path + ".stencilFunc")
          : Rasterizer::StencilFunc::Always;
      const std::uint8_t reference =
        detail::hasField(object, "stencilReference") ? detail::byteField(object, "stencilReference", path, stateError) : 0;
      const std::uint8_t mask =
        detail::hasField(object, "stencilMask") ? detail::byteField(object, "stencilMask", path, stateError) : 0xff;
      state.setStencilFunc(func, reference, mask);
    }
    if (detail::hasField(object, "stencilClearValue"))
      state.setStencilClearValue(detail::byteField(object, "stencilClearValue", path, stateError));
    if (detail::hasField(object, "stencilLoadOp"))
      state.setStencilLoadOp(attachmentLoadOpFromString(detail::stringField(object, "stencilLoadOp", path, stateError),
                                                        path + ".stencilLoadOp"));
    if (detail::hasField(object, "stencilStoreOp"))
      state.setStencilStoreOp(attachmentStoreOpFromString(
        detail::stringField(object, "stencilStoreOp", path, stateError), path + ".stencilStoreOp"));
    if (detail::hasField(object, "stencilWriteMask"))
      state.setStencilWriteMask(detail::byteField(object, "stencilWriteMask", path, stateError));
    if (detail::hasField(object, "stencilFailOp") || detail::hasField(object, "stencilDepthFailOp") ||
        detail::hasField(object, "stencilPassOp")) {
      const auto stencilFail =
        detail::hasField(object, "stencilFailOp")
          ? stencilOpFromString(detail::stringField(object, "stencilFailOp", path, stateError), path + ".stencilFailOp")
          : Rasterizer::StencilOp::Keep;
      const auto depthFail =
        detail::hasField(object, "stencilDepthFailOp")
          ? stencilOpFromString(detail::stringField(object, "stencilDepthFailOp", path, stateError),
                                path + ".stencilDepthFailOp")
          : Rasterizer::StencilOp::Keep;
      const auto pass =
        detail::hasField(object, "stencilPassOp")
          ? stencilOpFromString(detail::stringField(object, "stencilPassOp", path, stateError), path + ".stencilPassOp")
          : Rasterizer::StencilOp::Keep;
      state.setStencilOps(stencilFail, depthFail, pass);
    }
    return state;
  }

  QJsonObject RasterFramebufferState::toJson() const {
    QJsonObject object;
    if (m_viewportRect)
      object["viewport"] = detail::rectToJson(*m_viewportRect);
    if (m_scissorRect)
      object["scissor"] = detail::rectToJson(*m_scissorRect);
    if (m_colorLoadOp != Rasterizer::AttachmentLoadOp::Clear)
      object["colorLoadOp"] = toString(m_colorLoadOp);
    if (m_colorStoreOp != Rasterizer::AttachmentStoreOp::Store)
      object["colorStoreOp"] = toString(m_colorStoreOp);
    if (m_depthFunc != Rasterizer::DepthFunc::Less)
      object["depthFunc"] = toString(m_depthFunc);
    if (m_depthBias != 0.0)
      object["depthBias"] = m_depthBias;
    if (std::isfinite(m_depthClearValue))
      object["depthClearValue"] = m_depthClearValue;
    if (m_depthLoadOp != Rasterizer::AttachmentLoadOp::Clear)
      object["depthLoadOp"] = toString(m_depthLoadOp);
    if (m_depthStoreOp != Rasterizer::AttachmentStoreOp::Store)
      object["depthStoreOp"] = toString(m_depthStoreOp);
    if (!m_depthWriteEnabled)
      object["depthWrite"] = false;
    if (m_colorWriteMask != Rasterizer::ColorWriteAll)
      object["colorWriteMask"] = qstr(colorWriteMaskString(m_colorWriteMask));
    if (m_blendingEnabled)
      object["blending"] = true;
    if (m_sourceBlendFactor != Rasterizer::BlendFactor::One)
      object["blendSource"] = toString(m_sourceBlendFactor);
    if (m_destinationBlendFactor != Rasterizer::BlendFactor::Zero)
      object["blendDestination"] = toString(m_destinationBlendFactor);
    if (m_blendOp != Rasterizer::BlendOp::Add)
      object["blendOp"] = toString(m_blendOp);
    if (!(m_blendConstantColor == Colord::white()))
      object["blendConstantColor"] = detail::colorToJson(m_blendConstantColor);
    if (m_blendConstantAlpha != 1.0)
      object["blendConstantAlpha"] = m_blendConstantAlpha;
    if (m_alphaTestEnabled)
      object["alphaTest"] = true;
    if (m_alphaFunc != Rasterizer::AlphaFunc::Always)
      object["alphaFunc"] = toString(m_alphaFunc);
    if (m_alphaReference != 0.0)
      object["alphaReference"] = m_alphaReference;
    if (m_stencilTestEnabled)
      object["stencilTest"] = true;
    if (m_stencilFunc != Rasterizer::StencilFunc::Always)
      object["stencilFunc"] = toString(m_stencilFunc);
    if (m_stencilReference != 0)
      object["stencilReference"] = static_cast<int>(m_stencilReference);
    if (m_stencilMask != 0xff)
      object["stencilMask"] = static_cast<int>(m_stencilMask);
    if (m_stencilClearValue != 0)
      object["stencilClearValue"] = static_cast<int>(m_stencilClearValue);
    if (m_stencilLoadOp != Rasterizer::AttachmentLoadOp::Clear)
      object["stencilLoadOp"] = toString(m_stencilLoadOp);
    if (m_stencilStoreOp != Rasterizer::AttachmentStoreOp::Store)
      object["stencilStoreOp"] = toString(m_stencilStoreOp);
    if (m_stencilWriteMask != 0xff)
      object["stencilWriteMask"] = static_cast<int>(m_stencilWriteMask);
    if (m_stencilFailOp != Rasterizer::StencilOp::Keep)
      object["stencilFailOp"] = toString(m_stencilFailOp);
    if (m_stencilDepthFailOp != Rasterizer::StencilOp::Keep)
      object["stencilDepthFailOp"] = toString(m_stencilDepthFailOp);
    if (m_stencilPassOp != Rasterizer::StencilOp::Keep)
      object["stencilPassOp"] = toString(m_stencilPassOp);
    return object;
  }

  bool RasterFramebufferState::empty() const {
    return toJson().isEmpty();
  }

  void RasterFramebufferState::applyTo(Rasterizer& rasterizer) const {
    if (m_viewportRect) {
      rasterizer.setViewportRect(*m_viewportRect);
    } else {
      rasterizer.clearViewportRect();
    }
    if (m_scissorRect) {
      rasterizer.setScissorRect(*m_scissorRect);
    } else {
      rasterizer.clearScissorRect();
    }
    rasterizer.setColorLoadOp(m_colorLoadOp);
    rasterizer.setColorStoreOp(m_colorStoreOp);
    rasterizer.setDepthFunc(m_depthFunc);
    rasterizer.setDepthBias(m_depthBias);
    rasterizer.setDepthClearValue(m_depthClearValue);
    rasterizer.setDepthLoadOp(m_depthLoadOp);
    rasterizer.setDepthStoreOp(m_depthStoreOp);
    rasterizer.setDepthWriteEnabled(m_depthWriteEnabled);
    rasterizer.setColorWriteMask(m_colorWriteMask);
    rasterizer.setBlendingEnabled(m_blendingEnabled);
    rasterizer.setBlendFactors(m_sourceBlendFactor, m_destinationBlendFactor);
    rasterizer.setBlendOp(m_blendOp);
    rasterizer.setBlendConstant(m_blendConstantColor, m_blendConstantAlpha);
    rasterizer.setAlphaTestEnabled(m_alphaTestEnabled);
    rasterizer.setAlphaFunc(m_alphaFunc, m_alphaReference);
    rasterizer.setStencilTestEnabled(m_stencilTestEnabled);
    rasterizer.setStencilFunc(m_stencilFunc, m_stencilReference, m_stencilMask);
    rasterizer.setStencilClearValue(m_stencilClearValue);
    rasterizer.setStencilLoadOp(m_stencilLoadOp);
    rasterizer.setStencilStoreOp(m_stencilStoreOp);
    rasterizer.setStencilWriteMask(m_stencilWriteMask);
    rasterizer.setStencilOps(m_stencilFailOp, m_stencilDepthFailOp, m_stencilPassOp);
  }

  void RasterFramebufferState::applyTo(engine::raster::OpenGLRasterizer& rasterizer) const {
    validateSupportedByOpenGL();
    if (m_viewportRect) {
      rasterizer.setViewportRect(*m_viewportRect);
    } else {
      rasterizer.clearViewportRect();
    }
    if (m_scissorRect) {
      rasterizer.setScissorRect(*m_scissorRect);
    } else {
      rasterizer.clearScissorRect();
    }
    rasterizer.setColorLoadOp(m_colorLoadOp);
    rasterizer.setColorStoreOp(m_colorStoreOp);
    rasterizer.setDepthFunc(m_depthFunc);
    rasterizer.setDepthBias(m_depthBias);
    rasterizer.setDepthClearValue(m_depthClearValue);
    rasterizer.setDepthLoadOp(m_depthLoadOp);
    rasterizer.setDepthStoreOp(m_depthStoreOp);
    rasterizer.setDepthWriteEnabled(m_depthWriteEnabled);
    rasterizer.setColorWriteMask(m_colorWriteMask);
    rasterizer.setBlendingEnabled(m_blendingEnabled);
    rasterizer.setBlendFactors(m_sourceBlendFactor, m_destinationBlendFactor);
    rasterizer.setBlendOp(m_blendOp);
    rasterizer.setBlendConstant(m_blendConstantColor, m_blendConstantAlpha);
    rasterizer.setAlphaTestEnabled(m_alphaTestEnabled);
    rasterizer.setAlphaFunc(m_alphaFunc, m_alphaReference);
    rasterizer.setStencilTestEnabled(m_stencilTestEnabled);
    rasterizer.setStencilFunc(m_stencilFunc, m_stencilReference, m_stencilMask);
    rasterizer.setStencilClearValue(m_stencilClearValue);
    rasterizer.setStencilLoadOp(m_stencilLoadOp);
    rasterizer.setStencilStoreOp(m_stencilStoreOp);
    rasterizer.setStencilWriteMask(m_stencilWriteMask);
    rasterizer.setStencilOps(m_stencilFailOp, m_stencilDepthFailOp, m_stencilPassOp);
  }

  void RasterFramebufferState::validateSupportedByOpenGL() const {
    if (m_colorLoadOp == Rasterizer::AttachmentLoadOp::Load) {
      openGLUnsupported("color attachment load");
    }
    if (m_depthLoadOp == Rasterizer::AttachmentLoadOp::Load) {
      openGLUnsupported("depth attachment load");
    }
    if (m_stencilLoadOp == Rasterizer::AttachmentLoadOp::Load) {
      openGLUnsupported("stencil attachment load");
    }
  }

  void RasterFramebufferState::setViewportRect(const Recti& rect) {
    m_viewportRect = rect;
  }

  void RasterFramebufferState::setScissorRect(const Recti& rect) {
    m_scissorRect = rect;
  }

  void RasterFramebufferState::setColorLoadOp(Rasterizer::AttachmentLoadOp op) {
    m_colorLoadOp = op;
  }

  void RasterFramebufferState::setColorStoreOp(Rasterizer::AttachmentStoreOp op) {
    m_colorStoreOp = op;
  }

  void RasterFramebufferState::setDepthFunc(Rasterizer::DepthFunc func) {
    m_depthFunc = func;
  }

  void RasterFramebufferState::setDepthBias(double bias) {
    m_depthBias = std::isfinite(bias) ? bias : 0.0;
  }

  void RasterFramebufferState::setDepthClearValue(double value) {
    m_depthClearValue = value;
  }

  void RasterFramebufferState::setDepthLoadOp(Rasterizer::AttachmentLoadOp op) {
    m_depthLoadOp = op;
  }

  void RasterFramebufferState::setDepthStoreOp(Rasterizer::AttachmentStoreOp op) {
    m_depthStoreOp = op;
  }

  void RasterFramebufferState::setDepthWriteEnabled(bool enabled) {
    m_depthWriteEnabled = enabled;
  }

  void RasterFramebufferState::setColorWriteMask(std::uint8_t mask) {
    m_colorWriteMask = mask & Rasterizer::ColorWriteAll;
  }

  void RasterFramebufferState::setBlendingEnabled(bool enabled) {
    m_blendingEnabled = enabled;
  }

  void RasterFramebufferState::setBlendFactors(Rasterizer::BlendFactor source,
                                               Rasterizer::BlendFactor destination) {
    m_sourceBlendFactor = source;
    m_destinationBlendFactor = destination;
  }

  void RasterFramebufferState::setBlendOp(Rasterizer::BlendOp op) {
    m_blendOp = op;
  }

  void RasterFramebufferState::setBlendConstant(const Colord& color, double alpha) {
    m_blendConstantColor = color;
    m_blendConstantAlpha = std::isfinite(alpha) ? std::clamp(alpha, 0.0, 1.0) : 1.0;
  }

  void RasterFramebufferState::setAlphaTestEnabled(bool enabled) {
    m_alphaTestEnabled = enabled;
  }

  void RasterFramebufferState::setAlphaFunc(Rasterizer::AlphaFunc func, double reference) {
    m_alphaFunc = func;
    m_alphaReference = std::isfinite(reference) ? std::clamp(reference, 0.0, 1.0) : 0.0;
  }

  void RasterFramebufferState::setStencilTestEnabled(bool enabled) {
    m_stencilTestEnabled = enabled;
  }

  void RasterFramebufferState::setStencilFunc(Rasterizer::StencilFunc func, std::uint8_t reference,
                                              std::uint8_t mask) {
    m_stencilFunc = func;
    m_stencilReference = reference;
    m_stencilMask = mask;
  }

  void RasterFramebufferState::setStencilClearValue(std::uint8_t value) {
    m_stencilClearValue = value;
  }

  void RasterFramebufferState::setStencilLoadOp(Rasterizer::AttachmentLoadOp op) {
    m_stencilLoadOp = op;
  }

  void RasterFramebufferState::setStencilStoreOp(Rasterizer::AttachmentStoreOp op) {
    m_stencilStoreOp = op;
  }

  void RasterFramebufferState::setStencilWriteMask(std::uint8_t mask) {
    m_stencilWriteMask = mask;
  }

  void RasterFramebufferState::setStencilOps(Rasterizer::StencilOp stencilFail,
                                             Rasterizer::StencilOp depthFail,
                                             Rasterizer::StencilOp pass) {
    m_stencilFailOp = stencilFail;
    m_stencilDepthFailOp = depthFail;
    m_stencilPassOp = pass;
  }

  void RasterFramebufferState::configureStencilWritePass(std::uint8_t value) {
    setStencilTestEnabled(true);
    setStencilFunc(Rasterizer::StencilFunc::Always, value);
    setStencilClearValue(0);
    setStencilLoadOp(Rasterizer::AttachmentLoadOp::Clear);
    setStencilStoreOp(Rasterizer::AttachmentStoreOp::Store);
    setStencilWriteMask(0xff);
    setStencilOps(Rasterizer::StencilOp::Keep, Rasterizer::StencilOp::Keep,
                  Rasterizer::StencilOp::Replace);
  }

  bool RasterFramebufferState::supportsFrontToBackVisibilityOrdering() const {
    const bool depthOrdersFragments =
      m_depthFunc == Rasterizer::DepthFunc::Less || m_depthFunc == Rasterizer::DepthFunc::LessEqual;
    return depthOrdersFragments && m_depthWriteEnabled && !m_blendingEnabled &&
           !m_stencilTestEnabled;
  }

  bool RasterFramebufferState::stencilTestEnabled() const {
    return m_stencilTestEnabled;
  }

  RasterShadowState RasterShadowState::fromJson(const QJsonObject& object,
                                                const std::string& path) {
    detail::rejectUnknownFields(object, path,
                        {"enabled", "mapSize", "cascadeCount", "cascadeSplitLambda", "bias",
                         "slopeBias", "filterRadius", "filterMode"}, stateError);
    RasterShadowState state;
    if (detail::hasField(object, "enabled"))
      state.setShadowMapsEnabled(detail::boolField(object, "enabled", path, stateError));
    if (detail::hasField(object, "mapSize"))
      state.setShadowMapSize(detail::intField(object, "mapSize", path, stateError));
    if (detail::hasField(object, "cascadeCount"))
      state.setShadowCascadeCount(detail::intField(object, "cascadeCount", path, stateError));
    if (detail::hasField(object, "cascadeSplitLambda"))
      state.setShadowCascadeSplitLambda(detail::doubleField(object, "cascadeSplitLambda", path, stateError));
    if (detail::hasField(object, "bias"))
      state.setShadowBias(detail::doubleField(object, "bias", path, stateError));
    if (detail::hasField(object, "slopeBias"))
      state.setShadowSlopeBias(detail::doubleField(object, "slopeBias", path, stateError));
    if (detail::hasField(object, "filterRadius"))
      state.setShadowFilterRadius(detail::intField(object, "filterRadius", path, stateError));
    if (detail::hasField(object, "filterMode"))
      state.setShadowFilterMode(
        shadowFilterModeFromString(detail::stringField(object, "filterMode", path, stateError), path + ".filterMode"));
    return state;
  }

  QJsonObject RasterShadowState::toJson() const {
    QJsonObject object;
    if (m_enabled)
      object["enabled"] = true;
    if (m_mapSize != 256)
      object["mapSize"] = m_mapSize;
    if (m_cascadeCount != 1)
      object["cascadeCount"] = m_cascadeCount;
    if (m_cascadeSplitLambda != 0.5)
      object["cascadeSplitLambda"] = m_cascadeSplitLambda;
    if (m_bias != 1e-3)
      object["bias"] = m_bias;
    if (m_slopeBias != 0.0)
      object["slopeBias"] = m_slopeBias;
    if (m_filterRadius != 0)
      object["filterRadius"] = m_filterRadius;
    if (m_filterMode != Rasterizer::ShadowFilterMode::PCF)
      object["filterMode"] = toString(m_filterMode);
    return object;
  }

  bool RasterShadowState::empty() const {
    return toJson().isEmpty();
  }

  void RasterShadowState::applyTo(Rasterizer& rasterizer) const {
    rasterizer.setShadowMapsEnabled(m_enabled);
    rasterizer.setShadowMapSize(m_mapSize);
    rasterizer.setShadowCascadeCount(m_cascadeCount);
    rasterizer.setShadowCascadeSplitLambda(m_cascadeSplitLambda);
    rasterizer.setShadowBias(m_bias);
    rasterizer.setShadowSlopeBias(m_slopeBias);
    rasterizer.setShadowFilterRadius(m_filterRadius);
    rasterizer.setShadowFilterMode(m_filterMode);
  }

  void RasterShadowState::applyTo(engine::raster::OpenGLRasterizer& rasterizer) const {
    rasterizer.setShadowMapsEnabled(m_enabled);
  }

  void RasterShadowState::setShadowMapsEnabled(bool enabled) {
    m_enabled = enabled;
  }

  void RasterShadowState::setShadowMapSize(int size) {
    m_mapSize = atLeast(1, size);
  }

  void RasterShadowState::setShadowCascadeCount(int count) {
    m_cascadeCount = std::clamp(count, 1, 4);
  }

  void RasterShadowState::setShadowCascadeSplitLambda(double lambda) {
    m_cascadeSplitLambda = std::isfinite(lambda) ? std::clamp(lambda, 0.0, 1.0) : 0.5;
  }

  void RasterShadowState::setShadowBias(double bias) {
    m_bias = atLeast(0.0, bias);
  }

  void RasterShadowState::setShadowSlopeBias(double bias) {
    m_slopeBias = atLeast(0.0, bias);
  }

  void RasterShadowState::setShadowFilterRadius(int radius) {
    m_filterRadius = atLeast(0, radius);
  }

  void RasterShadowState::setShadowFilterMode(Rasterizer::ShadowFilterMode mode) {
    m_filterMode = mode;
  }

  bool RasterShadowState::enabled() const {
    return m_enabled;
  }

  int RasterShadowState::mapSize() const {
    return m_mapSize;
  }

  RenderResourceDescriptor RasterShadowState::resourceDescriptor(RenderResourceId id,
                                                                 std::string name) const {
    RenderResourceDescriptor descriptor;
    descriptor.id = std::move(id);
    descriptor.name = std::move(name);
    descriptor.type = RenderResourceType::ShadowMap;
    descriptor.format = RenderResourceFormat::DepthDouble;
    descriptor.width = m_mapSize;
    descriptor.height = m_mapSize;
    descriptor.sampleCount = 1;
    descriptor.domain = RenderResourceDomain::CPU;
    descriptor.lifetime = RenderResourceLifetime::PersistentCache;
    return descriptor;
  }

  RasterShadowPassState RasterShadowPassState::fromJson(const QJsonObject& object,
                                                        const std::string& path) {
    detail::rejectUnknownFields(object, path, {"shadows"}, stateError);

    RasterShadowPassState state;
    if (detail::hasField(object, "shadows"))
      state.m_shadows =
        RasterShadowState::fromJson(detail::objectField(object, "shadows", path, stateError), path + ".shadows");
    return state;
  }

  RasterShadowPassState RasterShadowPassState::previewDefaults() {
    RasterShadowPassState state;
    state.m_shadows.setShadowMapsEnabled(true);
    state.m_shadows.setShadowMapSize(256);
    state.m_shadows.setShadowCascadeCount(4);
    state.m_shadows.setShadowBias(0.1);
    state.m_shadows.setShadowFilterRadius(1);
    state.m_shadows.setShadowFilterMode(Rasterizer::ShadowFilterMode::PCF);
    return state;
  }

  const RasterShadowPassState* RasterShadowPassState::fromPass(const RenderPassNode& pass) {
    if (!pass.state)
      return nullptr;

    const auto* state = pass.state->asRasterShadowPassState();
    if (!state) {
      throw std::runtime_error("pass '" + pass.id + "' does not carry raster shadow state");
    }
    return state;
  }

  RasterShadowPassState RasterShadowPassState::valueFromPass(const RenderPassNode& pass) {
    return detail::valueOrDefault(fromPass(pass), RasterShadowPassState::previewDefaults());
  }

  const RasterShadowPassState* RasterShadowPassState::asRasterShadowPassState() const {
    return this;
  }

  QJsonObject RasterShadowPassState::toJson() const {
    QJsonObject object;
    if (!m_shadows.empty()) {
      object["shadows"] = m_shadows.toJson();
    }
    return object;
  }

  bool RasterShadowPassState::empty() const {
    return toJson().isEmpty();
  }

  void RasterShadowPassState::applyTo(Rasterizer& rasterizer) const {
    m_shadows.applyTo(rasterizer);
  }

  void RasterShadowPassState::applyTo(engine::raster::OpenGLRasterizer& rasterizer) const {
    m_shadows.applyTo(rasterizer);
  }

  void RasterShadowPassState::writeTo(RenderPassNode& pass) const {
    if (empty()) {
      pass.state.reset();
    } else {
      pass.state = std::make_shared<RasterShadowPassState>(*this);
    }
  }

  std::size_t RasterShadowPassState::writeToRasterShadowPasses(RenderPlan& plan) const {
    const std::size_t changed =
      plan.setPassState(RenderPassKind::Shadow, RenderExecutorKind::Rasterizer,
                        empty() ? nullptr : std::make_shared<RasterShadowPassState>(*this));

    for (const auto& pass : plan.passes()) {
      if (pass.kind != RenderPassKind::Shadow || pass.executor != RenderExecutorKind::Rasterizer)
        continue;

      for (const auto& write : pass.writes) {
        const auto* resource = plan.findResource(write.resource);
        if (!resource || resource->type != RenderResourceType::ShadowMap)
          continue;

        plan.setResourceDescriptor(m_shadows.resourceDescriptor(resource->id, resource->name));
      }
    }

    return changed;
  }

  RasterShadowState& RasterShadowPassState::shadows() {
    return m_shadows;
  }

  const RasterShadowState& RasterShadowPassState::shadows() const {
    return m_shadows;
  }

  RasterVisibilityPassState RasterVisibilityPassState::fromJson(const QJsonObject& object,
                                                                const std::string& path) {
    detail::rejectUnknownFields(object, path, {"geometry", "frontToBackOrdering"}, stateError);
    RasterVisibilityPassState state;
    if (detail::hasField(object, "geometry"))
      state.m_geometry =
        RasterGeometryState::fromJson(detail::objectField(object, "geometry", path, stateError), path + ".geometry");
    if (detail::hasField(object, "frontToBackOrdering"))
      state.setFrontToBackOrderingEnabled(detail::boolField(object, "frontToBackOrdering", path, stateError));
    return state;
  }

  const RasterVisibilityPassState* RasterVisibilityPassState::fromPass(const RenderPassNode& pass) {
    if (!pass.state)
      return nullptr;

    const auto* state = pass.state->asRasterVisibilityPassState();
    if (!state) {
      throw std::runtime_error("pass '" + pass.id + "' does not carry raster visibility state");
    }
    return state;
  }

  RasterVisibilityPassState RasterVisibilityPassState::valueFromPass(const RenderPassNode& pass) {
    return detail::valueOrDefault(fromPass(pass));
  }

  const RasterVisibilityPassState* RasterVisibilityPassState::asRasterVisibilityPassState() const {
    return this;
  }

  QJsonObject RasterVisibilityPassState::toJson() const {
    QJsonObject object;
    if (!m_geometry.empty())
      object["geometry"] = m_geometry.toJson();
    if (!m_frontToBackOrderingEnabled)
      object["frontToBackOrdering"] = false;
    return object;
  }

  bool RasterVisibilityPassState::empty() const {
    return toJson().isEmpty();
  }

  void RasterVisibilityPassState::writeTo(RenderPassNode& pass) const {
    if (empty()) {
      pass.state.reset();
    } else {
      pass.state = std::make_shared<RasterVisibilityPassState>(*this);
    }
  }

  RasterGeometryState& RasterVisibilityPassState::geometry() {
    return m_geometry;
  }

  const RasterGeometryState& RasterVisibilityPassState::geometry() const {
    return m_geometry;
  }

  void RasterVisibilityPassState::setFrontToBackOrderingEnabled(bool enabled) {
    m_frontToBackOrderingEnabled = enabled;
  }

  bool RasterVisibilityPassState::frontToBackOrderingEnabled() const {
    return m_frontToBackOrderingEnabled;
  }

  RasterBeautyPassState RasterBeautyPassState::fromJson(const QJsonObject& object,
                                                        const std::string& path) {
    detail::rejectUnknownFields(
      object, path,
      {"execution", "geometry", "sampling", "depthPrepass", "framebuffer", "shadows"}, stateError);
    RasterBeautyPassState state;
    QJsonObject samplingObject;
    if (detail::hasField(object, "execution"))
      state.m_execution =
        RasterExecutionState::fromJson(detail::objectField(object, "execution", path, stateError), path + ".execution");
    if (detail::hasField(object, "geometry"))
      state.m_geometry =
        RasterGeometryState::fromJson(detail::objectField(object, "geometry", path, stateError), path + ".geometry");
    if (detail::hasField(object, "sampling")) {
      samplingObject = detail::objectField(object, "sampling", path, stateError);
      state.m_sampling = RasterSamplingState::fromJson(samplingObject, path + ".sampling");
    }
    if (detail::hasField(object, "depthPrepass"))
      state.m_depthPrepass = RasterDepthPrepassState::fromJson(
        detail::objectField(object, "depthPrepass", path, stateError), path + ".depthPrepass");
    if (detail::hasField(object, "framebuffer"))
      state.m_framebuffer = RasterFramebufferState::fromJson(
        detail::objectField(object, "framebuffer", path, stateError), path + ".framebuffer");
    if (detail::hasField(object, "shadows"))
      state.m_shadows =
        RasterShadowState::fromJson(detail::objectField(object, "shadows", path, stateError), path + ".shadows");
    if (state.m_execution.backend().isOpenGL() && state.m_sampling.msaaSamples() > 1 &&
        !detail::hasField(samplingObject, "msaaShadingMode")) {
      state.m_sampling.setMSAAShadingMode(Rasterizer::MSAAShadingMode::PerFragment);
    }
    return state;
  }

  const RasterBeautyPassState* RasterBeautyPassState::fromPass(const RenderPassNode& pass) {
    if (pass.executor != RenderExecutorKind::Rasterizer)
      return nullptr;
    if (!pass.state)
      return nullptr;

    const auto* state = pass.state->asRasterBeautyPassState();
    if (!state) {
      throw std::runtime_error("pass '" + pass.id + "' does not carry raster beauty state");
    }
    return state;
  }

  RasterBeautyPassState RasterBeautyPassState::valueFromPass(const RenderPassNode& pass) {
    return detail::valueOrDefault(fromPass(pass));
  }

  const RasterBeautyPassState* RasterBeautyPassState::asRasterBeautyPassState() const {
    return this;
  }

  QJsonObject RasterBeautyPassState::toJson() const {
    QJsonObject object;
    if (!m_execution.empty())
      object["execution"] = m_execution.toJson();
    if (!m_geometry.empty())
      object["geometry"] = m_geometry.toJson();
    if (!m_sampling.empty())
      object["sampling"] = m_sampling.toJson();
    if (!m_depthPrepass.empty())
      object["depthPrepass"] = m_depthPrepass.toJson();
    if (!m_framebuffer.empty())
      object["framebuffer"] = m_framebuffer.toJson();
    if (!m_shadows.empty())
      object["shadows"] = m_shadows.toJson();
    return object;
  }

  bool RasterBeautyPassState::empty() const {
    return toJson().isEmpty();
  }

  void RasterBeautyPassState::applyTo(Rasterizer& rasterizer) const {
    m_execution.applyTo(rasterizer);
    m_geometry.applyTo(rasterizer);
    m_sampling.applyTo(rasterizer);
    m_depthPrepass.applyTo(rasterizer);
    m_framebuffer.applyTo(rasterizer);
    m_shadows.applyTo(rasterizer);
  }

  void RasterBeautyPassState::applyTo(engine::raster::OpenGLRasterizer& rasterizer) const {
    m_geometry.applyTo(rasterizer);
    m_sampling.applyTo(rasterizer);
    m_depthPrepass.applyTo(rasterizer);
    m_framebuffer.applyTo(rasterizer);
    m_shadows.applyTo(rasterizer);
  }

  void RasterBeautyPassState::writeTo(RenderPassNode& pass) const {
    if (empty()) {
      pass.state.reset();
    } else {
      pass.state = std::make_shared<RasterBeautyPassState>(*this);
    }
    pass.concurrency = m_execution.backend().isOpenGL() ? RenderConcurrencyLimit::limited(1)
                                                        : RenderConcurrencyLimit::parallel();
    pass.canRunConcurrently = pass.concurrency.allowsParallelExecution();
  }

  std::size_t RasterBeautyPassState::writeToRasterBeautyPasses(RenderPlan& plan) const {
    return plan.setPassState(RenderPassKind::Beauty, RenderExecutorKind::Rasterizer,
                             empty() ? nullptr : std::make_shared<RasterBeautyPassState>(*this));
  }

  std::size_t RasterBeautyPassState::writeToRasterAOVPasses(RenderPlan& plan) const {
    return plan.setPassState(RenderPassKind::AOV, RenderExecutorKind::Rasterizer,
                             empty() ? nullptr : std::make_shared<RasterBeautyPassState>(*this));
  }

  RasterExecutionState& RasterBeautyPassState::execution() {
    return m_execution;
  }

  RasterGeometryState& RasterBeautyPassState::geometry() {
    return m_geometry;
  }

  RasterSamplingState& RasterBeautyPassState::sampling() {
    return m_sampling;
  }

  RasterDepthPrepassState& RasterBeautyPassState::depthPrepass() {
    return m_depthPrepass;
  }

  RasterFramebufferState& RasterBeautyPassState::framebuffer() {
    return m_framebuffer;
  }

  RasterShadowState& RasterBeautyPassState::shadows() {
    return m_shadows;
  }

  const RasterExecutionState& RasterBeautyPassState::execution() const {
    return m_execution;
  }

  const RasterGeometryState& RasterBeautyPassState::geometry() const {
    return m_geometry;
  }

  const RasterSamplingState& RasterBeautyPassState::sampling() const {
    return m_sampling;
  }

  const RasterDepthPrepassState& RasterBeautyPassState::depthPrepass() const {
    return m_depthPrepass;
  }

  const RasterFramebufferState& RasterBeautyPassState::framebuffer() const {
    return m_framebuffer;
  }

  const RasterShadowState& RasterBeautyPassState::shadows() const {
    return m_shadows;
  }

}
