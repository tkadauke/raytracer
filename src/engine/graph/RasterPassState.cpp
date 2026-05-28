#include "engine/graph/RasterPassState.h"

#include "engine/graph/RenderPlan.h"
#include "engine/raster/OpenGLRasterizer.h"

#include <QJsonArray>

#include <algorithm>
#include <cmath>
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

    QJsonArray rectToJson(const Recti& rect) {
      return QJsonArray{rect.left(), rect.top(), rect.width(), rect.height()};
    }

    Recti rectFromJson(const QJsonObject& object, const char* key, const std::string& path) {
      const auto value = object.value(key);
      if (!value.isArray())
        stateError(path + "." + key, "expected [x, y, width, height]");

      const auto array = value.toArray();
      if (array.size() != 4)
        stateError(path + "." + key, "expected four integers");

      int values[4];
      for (int i = 0; i != 4; ++i) {
        if (!array.at(i).isDouble())
          stateError(path + "." + key + "[" + std::to_string(i) + "]", "expected integer");
        const double number = array.at(i).toDouble();
        if (!std::isfinite(number) || std::floor(number) != number)
          stateError(path + "." + key + "[" + std::to_string(i) + "]", "expected integer");
        values[i] = static_cast<int>(number);
      }
      if (values[2] < 0 || values[3] < 0)
        stateError(path + "." + key, "width and height must be non-negative");
      return Recti(values[0], values[1], values[2], values[3]);
    }

    QJsonArray colorToJson(const Colord& color) {
      return QJsonArray{color.r(), color.g(), color.b()};
    }

    Colord colorFromJson(const QJsonObject& object, const char* key, const std::string& path) {
      const auto value = object.value(key);
      if (!value.isArray())
        stateError(path + "." + key, "expected [r, g, b]");

      const auto array = value.toArray();
      if (array.size() != 3)
        stateError(path + "." + key, "expected three numbers");

      double values[3];
      for (int i = 0; i != 3; ++i) {
        if (!array.at(i).isDouble())
          stateError(path + "." + key + "[" + std::to_string(i) + "]", "expected number");
        values[i] = array.at(i).toDouble();
        if (!std::isfinite(values[i]) || values[i] < 0.0 || values[i] > 1.0)
          stateError(path + "." + key + "[" + std::to_string(i) + "]",
                     "expected number from 0 to 1");
      }
      return Colord(values[0], values[1], values[2]);
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
    rejectUnknownFields(object, path, {"lod", "cullMode"});
    RasterGeometryState state;
    if (hasField(object, "lod"))
      state.setLod(intField(object, "lod", path));
    if (hasField(object, "cullMode"))
      state.setCullMode(
        cullModeFromString(stringField(object, "cullMode", path), path + ".cullMode"));
    return state;
  }

  QJsonObject RasterGeometryState::toJson() const {
    QJsonObject object;
    if (m_lod != 0)
      object["lod"] = m_lod;
    if (m_cullMode)
      object["cullMode"] = toString(*m_cullMode);
    return object;
  }

  bool RasterGeometryState::empty() const {
    return toJson().isEmpty();
  }

  void RasterGeometryState::applyTo(Rasterizer& rasterizer) const {
    rasterizer.setLod(m_lod);
    if (m_cullMode && *m_cullMode != Rasterizer::CullMode::Both) {
      rasterizer.setCullMode(*m_cullMode);
    } else {
      rasterizer.clearCullModeOverride();
    }
  }

  void RasterGeometryState::applyTo(engine::raster::OpenGLRasterizer& rasterizer) const {
    rasterizer.setLod(m_lod);
    if (m_cullMode && *m_cullMode != Rasterizer::CullMode::Both) {
      rasterizer.setCullMode(*m_cullMode);
    } else {
      rasterizer.clearCullModeOverride();
    }
  }

  void RasterGeometryState::setLod(int lod) {
    m_lod = std::max(0, lod);
  }

  void RasterGeometryState::setCullMode(Rasterizer::CullMode mode) {
    m_cullMode = mode;
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
    if (m_msaaShadingMode != Rasterizer::MSAAShadingMode::PerSample)
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

  RasterFramebufferState RasterFramebufferState::fromJson(const QJsonObject& object,
                                                          const std::string& path) {
    rejectUnknownFields(object, path,
                        {"viewport", "scissor", "depthBias", "colorWriteMask", "blending",
                         "blendSource", "blendDestination", "blendOp", "blendConstantColor",
                         "blendConstantAlpha", "alphaTest", "alphaFunc", "alphaReference"});
    RasterFramebufferState state;
    if (hasField(object, "viewport"))
      state.setViewportRect(rectFromJson(object, "viewport", path));
    if (hasField(object, "scissor"))
      state.setScissorRect(rectFromJson(object, "scissor", path));
    if (hasField(object, "depthBias"))
      state.setDepthBias(doubleField(object, "depthBias", path));
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
    return state;
  }

  QJsonObject RasterFramebufferState::toJson() const {
    QJsonObject object;
    if (m_viewportRect)
      object["viewport"] = rectToJson(*m_viewportRect);
    if (m_scissorRect)
      object["scissor"] = rectToJson(*m_scissorRect);
    if (m_depthBias != 0.0)
      object["depthBias"] = m_depthBias;
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
      object["blendConstantColor"] = colorToJson(m_blendConstantColor);
    if (m_blendConstantAlpha != 1.0)
      object["blendConstantAlpha"] = m_blendConstantAlpha;
    if (m_alphaTestEnabled)
      object["alphaTest"] = true;
    if (m_alphaFunc != Rasterizer::AlphaFunc::Always)
      object["alphaFunc"] = toString(m_alphaFunc);
    if (m_alphaReference != 0.0)
      object["alphaReference"] = m_alphaReference;
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
    rasterizer.setDepthBias(m_depthBias);
    rasterizer.setColorWriteMask(m_colorWriteMask);
    rasterizer.setBlendingEnabled(m_blendingEnabled);
    rasterizer.setBlendFactors(m_sourceBlendFactor, m_destinationBlendFactor);
    rasterizer.setBlendOp(m_blendOp);
    rasterizer.setBlendConstant(m_blendConstantColor, m_blendConstantAlpha);
    rasterizer.setAlphaTestEnabled(m_alphaTestEnabled);
    rasterizer.setAlphaFunc(m_alphaFunc, m_alphaReference);
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
    rasterizer.setColorWriteMask(m_colorWriteMask);
    rasterizer.setBlendingEnabled(m_blendingEnabled);
    rasterizer.setBlendFactors(m_sourceBlendFactor, m_destinationBlendFactor);
    rasterizer.setBlendOp(m_blendOp);
    rasterizer.setBlendConstant(m_blendConstantColor, m_blendConstantAlpha);
    rasterizer.setAlphaTestEnabled(m_alphaTestEnabled);
    rasterizer.setAlphaFunc(m_alphaFunc, m_alphaReference);
  }

  void RasterFramebufferState::validateSupportedByOpenGL() const {
    if (m_depthBias != 0.0) {
      openGLUnsupported("depth bias");
    }
  }

  void RasterFramebufferState::setViewportRect(const Recti& rect) {
    m_viewportRect = rect;
  }

  void RasterFramebufferState::setScissorRect(const Recti& rect) {
    m_scissorRect = rect;
  }

  void RasterFramebufferState::setDepthBias(double bias) {
    m_depthBias = std::isfinite(bias) ? bias : 0.0;
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

  void RasterShadowState::applyTo(engine::raster::OpenGLRasterizer&) const {
    if (m_enabled) {
      openGLUnsupported("shadow maps");
    }
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

  RasterBeautyPassState RasterBeautyPassState::fromJson(const QJsonObject& object,
                                                        const std::string& path) {
    rejectUnknownFields(object, path,
                        {"execution", "geometry", "sampling", "framebuffer", "shadows"});
    RasterBeautyPassState state;
    if (hasField(object, "execution"))
      state.m_execution =
        RasterExecutionState::fromJson(objectField(object, "execution", path), path + ".execution");
    if (hasField(object, "geometry"))
      state.m_geometry =
        RasterGeometryState::fromJson(objectField(object, "geometry", path), path + ".geometry");
    if (hasField(object, "sampling"))
      state.m_sampling =
        RasterSamplingState::fromJson(objectField(object, "sampling", path), path + ".sampling");
    if (hasField(object, "framebuffer"))
      state.m_framebuffer = RasterFramebufferState::fromJson(
        objectField(object, "framebuffer", path), path + ".framebuffer");
    if (hasField(object, "shadows"))
      state.m_shadows =
        RasterShadowState::fromJson(objectField(object, "shadows", path), path + ".shadows");
    return state;
  }

  const RasterBeautyPassState* RasterBeautyPassState::fromPass(const RenderPassNode& pass) {
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
    m_framebuffer.applyTo(rasterizer);
    m_shadows.applyTo(rasterizer);
  }

  void RasterBeautyPassState::applyTo(engine::raster::OpenGLRasterizer& rasterizer) const {
    m_geometry.applyTo(rasterizer);
    m_sampling.applyTo(rasterizer);
    m_framebuffer.applyTo(rasterizer);
    m_shadows.applyTo(rasterizer);
  }

  void RasterBeautyPassState::writeTo(RenderPassNode& pass) const {
    if (empty()) {
      pass.state.reset();
    } else {
      pass.state = std::make_shared<RasterBeautyPassState>(*this);
    }
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

  const RasterFramebufferState& RasterBeautyPassState::framebuffer() const {
    return m_framebuffer;
  }

  const RasterShadowState& RasterBeautyPassState::shadows() const {
    return m_shadows;
  }

}
