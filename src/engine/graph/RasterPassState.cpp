#include "engine/graph/RasterPassState.h"

#include "engine/graph/RenderPlan.h"
#include "engine/graph/detail/JsonStateHelpers.h"
#include "engine/raster/OpenGLRasterizer.h"

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

    QString qstr(const std::string& value) {
      return QString::fromStdString(value);
    }

    QString qstr(const char* value) {
      return QString::fromUtf8(value);
    }

    [[noreturn]] void stateError(const std::string& path, const std::string& message) {
      throw std::runtime_error("Invalid raster pass state at " + path + ": " + message);
    }

    [[noreturn]] void openGLUnsupported(const std::string& feature) {
      throw std::runtime_error("OpenGL raster backend does not support " + feature +
                               " yet; use raster backend 'cpu'");
    }

    bool hasField(const QJsonObject& object, const char* key) {
      return !object.value(key).isUndefined();
    }

    void rejectUnknownFields(const QJsonObject& object, const std::string& path,
                             std::initializer_list<const char*> allowed) {
      for (auto it = object.begin(); it != object.end(); ++it) {
        const std::string key = it.key().toStdString();
        bool matched = false;
        for (const char* allowedKey : allowed) {
          if (key == allowedKey) {
            matched = true;
            break;
          }
        }
        if (!matched)
          stateError(path + "." + key, "unknown field");
      }
    }

    QJsonObject objectField(const QJsonObject& object, const char* key, const std::string& path) {
      const auto value = object.value(key);
      if (value.isUndefined())
        return {};
      if (!value.isObject())
        stateError(path + "." + key, "expected object");
      return value.toObject();
    }

    int intField(const QJsonObject& object, const char* key, const std::string& path) {
      const auto value = object.value(key);
      if (!value.isDouble())
        stateError(path + "." + key, "expected integer");

      const double number = value.toDouble();
      if (!std::isfinite(number) || std::floor(number) != number)
        stateError(path + "." + key, "expected integer");
      return static_cast<int>(number);
    }

    std::uint8_t byteField(const QJsonObject& object, const char* key, const std::string& path) {
      const int value = intField(object, key, path);
      if (value < 0 || value > 255)
        stateError(path + "." + key, "expected integer from 0 to 255");
      return static_cast<std::uint8_t>(value);
    }

    double doubleField(const QJsonObject& object, const char* key, const std::string& path) {
      const auto value = object.value(key);
      if (!value.isDouble())
        stateError(path + "." + key, "expected number");

      const double number = value.toDouble();
      if (!std::isfinite(number))
        stateError(path + "." + key, "expected finite number");
      return number;
    }

    bool boolField(const QJsonObject& object, const char* key, const std::string& path) {
      const auto value = object.value(key);
      if (!value.isBool())
        stateError(path + "." + key, "expected boolean");
      return value.toBool();
    }

    std::string stringField(const QJsonObject& object, const char* key, const std::string& path) {
      const auto value = object.value(key);
      if (!value.isString())
        stateError(path + "." + key, "expected string");
      return value.toString().toStdString();
    }

    Recti rectFromJson(const QJsonObject& object, const char* key, const std::string& path) {
      return detail::rectFromJson(object, key, path, stateError);
    }

    Colord colorFromJson(const QJsonObject& object, const char* key, const std::string& path) {
      return detail::colorFromJson(object, key, path, stateError);
    }

    template<class T>
    const char* enumName(T value, std::initializer_list<std::pair<T, const char*>> values,
                         const char* fallback) {
      for (const auto& [parsed, name] : values) {
        if (value == parsed)
          return name;
      }
      return fallback;
    }

    const char* toString(Rasterizer::CullMode mode) {
      return enumName<Rasterizer::CullMode>(mode,
                                            {{Rasterizer::CullMode::Both, "both"},
                                             {Rasterizer::CullMode::Back, "back"},
                                             {Rasterizer::CullMode::Front, "front"}},
                                            "both");
    }

    const char* toString(Rasterizer::TessellationQuality quality) {
      return enumName<Rasterizer::TessellationQuality>(
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
      return enumName<Rasterizer::MSAAShadingMode>(
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
      return enumName<Rasterizer::DepthPrepassMode>(mode,
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
      return enumName<Rasterizer::PostProcessAA>(aa,
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
      return enumName<Rasterizer::BlendFactor>(
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
      return enumName<Rasterizer::BlendOp>(
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
      return enumName<Rasterizer::DepthFunc>(
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
      return enumName<Rasterizer::AlphaFunc>(
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
      return enumName<Rasterizer::StencilFunc>(
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
      return enumName<Rasterizer::StencilOp>(
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
      return enumName<Rasterizer::AttachmentLoadOp>(op,
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
      return enumName<Rasterizer::AttachmentStoreOp>(
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
      return enumName<Rasterizer::ShadowFilterMode>(
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
    rejectUnknownFields(object, path, {"threads", "queueSize", "backend"});
    RasterExecutionState state;
    if (hasField(object, "threads"))
      state.setMaximumThreads(intField(object, "threads", path));
    if (hasField(object, "queueSize"))
      state.setQueueSize(intField(object, "queueSize", path));
    if (hasField(object, "backend"))
      state.setBackend(engine::raster::RasterBackend::fromString(
        stringField(object, "backend", path), path + ".backend"));
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
    m_maximumThreads = std::max(1, threads);
  }

  void RasterExecutionState::setQueueSize(int queueSize) {
    m_queueSize = std::max(1, queueSize);
  }

  void RasterExecutionState::setBackend(engine::raster::RasterBackend backend) {
    m_backend = backend;
  }

  engine::raster::RasterBackend RasterExecutionState::backend() const {
    return m_backend.value_or(engine::raster::RasterBackend::cpu());
  }

  RasterGeometryState RasterGeometryState::fromJson(const QJsonObject& object,
                                                    const std::string& path) {
    rejectUnknownFields(object, path, {"lod", "quality", "maxScreenSpaceError", "cullMode"});
    RasterGeometryState state;
    if (hasField(object, "lod"))
      state.setLod(intField(object, "lod", path));
    if (hasField(object, "quality"))
      state.setTessellationQuality(
        tessellationQualityFromString(stringField(object, "quality", path), path + ".quality"));
    if (hasField(object, "maxScreenSpaceError"))
      state.setMaximumScreenSpaceError(doubleField(object, "maxScreenSpaceError", path));
    if (hasField(object, "cullMode"))
      state.setCullMode(
        cullModeFromString(stringField(object, "cullMode", path), path + ".cullMode"));
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
    m_lod = std::max(0, lod);
  }

  void RasterGeometryState::setTessellationQuality(Rasterizer::TessellationQuality quality) {
    m_tessellationQuality = quality;
  }

  void RasterGeometryState::setMaximumScreenSpaceError(double pixels) {
    m_maximumScreenSpaceError = std::isfinite(pixels) ? std::max(0.0, pixels) : 0.0;
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
    rejectUnknownFields(object, path, {"msaaSamples", "msaaShadingMode", "postProcessAA"});
    RasterSamplingState state;
    if (hasField(object, "msaaSamples"))
      state.setMSAASamples(intField(object, "msaaSamples", path));
    if (hasField(object, "msaaShadingMode"))
      state.setMSAAShadingMode(msaaShadingModeFromString(
        stringField(object, "msaaShadingMode", path), path + ".msaaShadingMode"));
    if (hasField(object, "postProcessAA"))
      state.setPostProcessAA(postProcessAAFromString(stringField(object, "postProcessAA", path),
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
    rejectUnknownFields(object, path, {"mode"});
    RasterDepthPrepassState state;
    if (hasField(object, "mode"))
      state.setMode(depthPrepassModeFromString(stringField(object, "mode", path), path + ".mode"));
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
    rejectUnknownFields(object, path,
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
                         "stencilPassOp"});
    RasterFramebufferState state;
    if (hasField(object, "viewport"))
      state.setViewportRect(rectFromJson(object, "viewport", path));
    if (hasField(object, "scissor"))
      state.setScissorRect(rectFromJson(object, "scissor", path));
    if (hasField(object, "colorLoadOp"))
      state.setColorLoadOp(attachmentLoadOpFromString(stringField(object, "colorLoadOp", path),
                                                      path + ".colorLoadOp"));
    if (hasField(object, "colorStoreOp"))
      state.setColorStoreOp(attachmentStoreOpFromString(stringField(object, "colorStoreOp", path),
                                                        path + ".colorStoreOp"));
    if (hasField(object, "depthFunc"))
      state.setDepthFunc(
        depthFuncFromString(stringField(object, "depthFunc", path), path + ".depthFunc"));
    if (hasField(object, "depthBias"))
      state.setDepthBias(doubleField(object, "depthBias", path));
    if (hasField(object, "depthClearValue"))
      state.setDepthClearValue(doubleField(object, "depthClearValue", path));
    if (hasField(object, "depthLoadOp"))
      state.setDepthLoadOp(attachmentLoadOpFromString(stringField(object, "depthLoadOp", path),
                                                      path + ".depthLoadOp"));
    if (hasField(object, "depthStoreOp"))
      state.setDepthStoreOp(attachmentStoreOpFromString(stringField(object, "depthStoreOp", path),
                                                        path + ".depthStoreOp"));
    if (hasField(object, "depthWrite"))
      state.setDepthWriteEnabled(boolField(object, "depthWrite", path));
    if (hasField(object, "colorWriteMask"))
      state.setColorWriteMask(colorWriteMaskFromString(stringField(object, "colorWriteMask", path),
                                                       path + ".colorWriteMask"));
    if (hasField(object, "blending"))
      state.setBlendingEnabled(boolField(object, "blending", path));
    if (hasField(object, "blendSource") || hasField(object, "blendDestination")) {
      const auto source =
        hasField(object, "blendSource")
          ? blendFactorFromString(stringField(object, "blendSource", path), path + ".blendSource")
          : Rasterizer::BlendFactor::One;
      const auto destination =
        hasField(object, "blendDestination")
          ? blendFactorFromString(stringField(object, "blendDestination", path),
                                  path + ".blendDestination")
          : Rasterizer::BlendFactor::Zero;
      state.setBlendFactors(source, destination);
    }
    if (hasField(object, "blendOp"))
      state.setBlendOp(blendOpFromString(stringField(object, "blendOp", path), path + ".blendOp"));
    if (hasField(object, "blendConstantColor") || hasField(object, "blendConstantAlpha")) {
      const Colord color = hasField(object, "blendConstantColor")
                             ? colorFromJson(object, "blendConstantColor", path)
                             : Colord::white();
      const double alpha = hasField(object, "blendConstantAlpha")
                             ? doubleField(object, "blendConstantAlpha", path)
                             : 1.0;
      state.setBlendConstant(color, alpha);
    }
    if (hasField(object, "alphaTest"))
      state.setAlphaTestEnabled(boolField(object, "alphaTest", path));
    if (hasField(object, "alphaFunc") || hasField(object, "alphaReference")) {
      const auto func =
        hasField(object, "alphaFunc")
          ? alphaFuncFromString(stringField(object, "alphaFunc", path), path + ".alphaFunc")
          : Rasterizer::AlphaFunc::Always;
      const double reference =
        hasField(object, "alphaReference") ? doubleField(object, "alphaReference", path) : 0.0;
      state.setAlphaFunc(func, reference);
    }
    if (hasField(object, "stencilTest"))
      state.setStencilTestEnabled(boolField(object, "stencilTest", path));
    if (hasField(object, "stencilFunc") || hasField(object, "stencilReference") ||
        hasField(object, "stencilMask")) {
      const auto func =
        hasField(object, "stencilFunc")
          ? stencilFuncFromString(stringField(object, "stencilFunc", path), path + ".stencilFunc")
          : Rasterizer::StencilFunc::Always;
      const std::uint8_t reference =
        hasField(object, "stencilReference") ? byteField(object, "stencilReference", path) : 0;
      const std::uint8_t mask =
        hasField(object, "stencilMask") ? byteField(object, "stencilMask", path) : 0xff;
      state.setStencilFunc(func, reference, mask);
    }
    if (hasField(object, "stencilClearValue"))
      state.setStencilClearValue(byteField(object, "stencilClearValue", path));
    if (hasField(object, "stencilLoadOp"))
      state.setStencilLoadOp(attachmentLoadOpFromString(stringField(object, "stencilLoadOp", path),
                                                        path + ".stencilLoadOp"));
    if (hasField(object, "stencilStoreOp"))
      state.setStencilStoreOp(attachmentStoreOpFromString(
        stringField(object, "stencilStoreOp", path), path + ".stencilStoreOp"));
    if (hasField(object, "stencilWriteMask"))
      state.setStencilWriteMask(byteField(object, "stencilWriteMask", path));
    if (hasField(object, "stencilFailOp") || hasField(object, "stencilDepthFailOp") ||
        hasField(object, "stencilPassOp")) {
      const auto stencilFail =
        hasField(object, "stencilFailOp")
          ? stencilOpFromString(stringField(object, "stencilFailOp", path), path + ".stencilFailOp")
          : Rasterizer::StencilOp::Keep;
      const auto depthFail =
        hasField(object, "stencilDepthFailOp")
          ? stencilOpFromString(stringField(object, "stencilDepthFailOp", path),
                                path + ".stencilDepthFailOp")
          : Rasterizer::StencilOp::Keep;
      const auto pass =
        hasField(object, "stencilPassOp")
          ? stencilOpFromString(stringField(object, "stencilPassOp", path), path + ".stencilPassOp")
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
    rejectUnknownFields(object, path,
                        {"enabled", "mapSize", "cascadeCount", "cascadeSplitLambda", "bias",
                         "slopeBias", "filterRadius", "filterMode"});
    RasterShadowState state;
    if (hasField(object, "enabled"))
      state.setShadowMapsEnabled(boolField(object, "enabled", path));
    if (hasField(object, "mapSize"))
      state.setShadowMapSize(intField(object, "mapSize", path));
    if (hasField(object, "cascadeCount"))
      state.setShadowCascadeCount(intField(object, "cascadeCount", path));
    if (hasField(object, "cascadeSplitLambda"))
      state.setShadowCascadeSplitLambda(doubleField(object, "cascadeSplitLambda", path));
    if (hasField(object, "bias"))
      state.setShadowBias(doubleField(object, "bias", path));
    if (hasField(object, "slopeBias"))
      state.setShadowSlopeBias(doubleField(object, "slopeBias", path));
    if (hasField(object, "filterRadius"))
      state.setShadowFilterRadius(intField(object, "filterRadius", path));
    if (hasField(object, "filterMode"))
      state.setShadowFilterMode(
        shadowFilterModeFromString(stringField(object, "filterMode", path), path + ".filterMode"));
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
    m_mapSize = std::max(1, size);
  }

  void RasterShadowState::setShadowCascadeCount(int count) {
    m_cascadeCount = std::clamp(count, 1, 4);
  }

  void RasterShadowState::setShadowCascadeSplitLambda(double lambda) {
    m_cascadeSplitLambda = std::isfinite(lambda) ? std::clamp(lambda, 0.0, 1.0) : 0.5;
  }

  void RasterShadowState::setShadowBias(double bias) {
    m_bias = std::max(0.0, bias);
  }

  void RasterShadowState::setShadowSlopeBias(double bias) {
    m_slopeBias = std::max(0.0, bias);
  }

  void RasterShadowState::setShadowFilterRadius(int radius) {
    m_filterRadius = std::max(0, radius);
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
    rejectUnknownFields(object, path, {"shadows"});

    RasterShadowPassState state;
    if (hasField(object, "shadows"))
      state.m_shadows =
        RasterShadowState::fromJson(objectField(object, "shadows", path), path + ".shadows");
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
    const auto* state = fromPass(pass);
    return state ? *state : RasterShadowPassState::previewDefaults();
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
    rejectUnknownFields(object, path, {"geometry", "frontToBackOrdering"});
    RasterVisibilityPassState state;
    if (hasField(object, "geometry"))
      state.m_geometry =
        RasterGeometryState::fromJson(objectField(object, "geometry", path), path + ".geometry");
    if (hasField(object, "frontToBackOrdering"))
      state.setFrontToBackOrderingEnabled(boolField(object, "frontToBackOrdering", path));
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
    const auto* state = fromPass(pass);
    return state ? *state : RasterVisibilityPassState();
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
    rejectUnknownFields(
      object, path,
      {"execution", "geometry", "sampling", "depthPrepass", "framebuffer", "shadows"});
    RasterBeautyPassState state;
    QJsonObject samplingObject;
    if (hasField(object, "execution"))
      state.m_execution =
        RasterExecutionState::fromJson(objectField(object, "execution", path), path + ".execution");
    if (hasField(object, "geometry"))
      state.m_geometry =
        RasterGeometryState::fromJson(objectField(object, "geometry", path), path + ".geometry");
    if (hasField(object, "sampling")) {
      samplingObject = objectField(object, "sampling", path);
      state.m_sampling = RasterSamplingState::fromJson(samplingObject, path + ".sampling");
    }
    if (hasField(object, "depthPrepass"))
      state.m_depthPrepass = RasterDepthPrepassState::fromJson(
        objectField(object, "depthPrepass", path), path + ".depthPrepass");
    if (hasField(object, "framebuffer"))
      state.m_framebuffer = RasterFramebufferState::fromJson(
        objectField(object, "framebuffer", path), path + ".framebuffer");
    if (hasField(object, "shadows"))
      state.m_shadows =
        RasterShadowState::fromJson(objectField(object, "shadows", path), path + ".shadows");
    if (state.m_execution.backend().isOpenGL() && state.m_sampling.msaaSamples() > 1 &&
        !hasField(samplingObject, "msaaShadingMode")) {
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
    const auto* state = fromPass(pass);
    return state ? *state : RasterBeautyPassState();
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
