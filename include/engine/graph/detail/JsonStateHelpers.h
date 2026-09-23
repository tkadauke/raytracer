#pragma once

#include "core/json/JsonValue.h"
#include "core/math/Number.h"
#include "core/math/Rect.h"
#include "engine/graph/RenderGraphTypes.h"
#include "engine/raster/Rasterizer.h"

#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <string>

namespace engine::graph::detail {
  using Rasterizer = engine::raster::Rasterizer;
  template<typename Error>
  [[nodiscard]] inline Recti rectFromJson(const QJsonObject& object, const char* key,
                                          const std::string& path, Error error) {
    const auto value = object.value(key);
    if (!value.isArray())
      error(path + "." + key, "expected [x, y, width, height]");

    const auto array = value.toArray();
    if (array.size() != 4)
      error(path + "." + key, "expected four integers");

    int values[4];
    for (int i = 0; i != 4; ++i) {
      if (!array.at(i).isDouble())
        error(path + "." + key + "[" + std::to_string(i) + "]", "expected integer");
      const double number = array.at(i).toDouble();
      if (!core::json::isIntegerValued(number))
        error(path + "." + key + "[" + std::to_string(i) + "]", "expected integer");
      values[i] = static_cast<int>(number);
    }
    const Recti rect(values);
    if (!rect.hasNonNegativeSize())
      error(path + "." + key, "width and height must be non-negative");
    return rect;
  }

  [[nodiscard]] inline QJsonArray rectToJson(const Recti& rect) {
    return QJsonArray{rect.left(), rect.top(), rect.width(), rect.height()};
  }

  template<typename Error>
  [[nodiscard]] inline Colord colorFromJson(const QJsonObject& object, const char* key,
                                            const std::string& path, Error error) {
    const auto value = object.value(key);
    const auto values = core::json::requireNumberArray<3>(
      value, "expected [r, g, b]", "expected three numbers", "expected number",
      [&](std::optional<int> index, const char* message) {
        error(index ? path + "." + key + "[" + std::to_string(*index) + "]" : path + "." + key,
              message);
      });
    if (!values)
      error(path + "." + key, "expected [r, g, b]");

    for (int i = 0; i != 3; ++i) {
      if (!std::isfinite((*values)[static_cast<std::size_t>(i)]) ||
          (*values)[static_cast<std::size_t>(i)] < 0.0 ||
          (*values)[static_cast<std::size_t>(i)] > 1.0) {
        error(path + "." + key + "[" + std::to_string(i) + "]", "expected number from 0 to 1");
      }
    }
    return Colord(*values);
  }

  [[nodiscard]] inline QJsonArray colorToJson(const Colord& color) {
    return core::json::colorToJsonArray(color);
  }

  // ---- JSON field extraction helpers ----------------------------------------
  //
  // Each helper takes an `error` callable with signature
  //   void error(const std::string& path, const std::string& message)
  // that is expected to throw on malformed input.

  [[nodiscard]] inline bool hasField(const QJsonObject& object, const char* key) {
    return !object.value(key).isUndefined();
  }

  /// Builds the "Invalid <label> at <path>: <message>" runtime_error shared
  /// by every JSON-parsing error site in the render-graph module (pass
  /// state, render graph, render plan, ...).
  [[noreturn]] inline void throwLabeledJsonError(const std::string& label, const std::string& path,
                                                 const std::string& message) {
    throw std::runtime_error("Invalid " + label + " at " + path + ": " + message);
  }

  /// Builds the "Invalid <kind> pass state at <path>: <message>" runtime_error
  /// shared by each pass-state type's local `stateError` callback (raster,
  /// raytracer, wireframe, postprocess, ...) passed as the `Error` template
  /// argument to the field-extraction helpers below.
  [[noreturn]] inline void throwPassStateError(const char* passTypeName, const std::string& path,
                                               const std::string& message) {
    throwLabeledJsonError(std::string(passTypeName) + " pass state", path, message);
  }

  /// Clamps a level-of-detail value to the non-negative range shared by every
  /// `setLod()` on the rasterizer/wireframe pass-state and options types.
  [[nodiscard]] inline int clampedLod(int lod) {
    return atLeast(0, lod);
  }

  /// @returns `*state` if non-null, otherwise @p fallback. Shared by the
  /// pass-state `valueFromPass()` overrides, which look up their node's
  /// state via `fromPass()` and fall back to a default value when absent —
  /// most fall back to a default-constructed instance, but callers that
  /// need a different fallback (e.g. `RasterShadowPassState::previewDefaults()`)
  /// can pass one explicitly.
  template<typename T>
  [[nodiscard]] inline T valueOrDefault(const T* state, T fallback = T()) {
    return state ? *state : fallback;
  }

  /// Looks up @p pass's typed state via @p asState (one of
  /// `RenderPassState::asRasterXxxPassState()`), returning `nullptr` if the
  /// pass carries no state at all, or throwing if it carries state of a
  /// different type. Shared by the raster pass-state `fromPass()` overrides
  /// that require their pass to carry the matching state type when any
  /// state is present at all (`RasterShadowPassState`,
  /// `RasterVisibilityPassState`, `RasterBeautyPassState`) — callers that
  /// need additional preconditions (e.g. `RasterBeautyPassState` also
  /// requires a rasterizer executor) check those before calling this.
  template<typename State>
  [[nodiscard]] inline const State* passStateOrThrow(const RenderPassNode& pass,
                                                      const State* (RenderPassState::*asState)() const,
                                                      const char* label) {
    if (!pass.state)
      return nullptr;

    const State* state = (pass.state.get()->*asState)();
    if (!state)
      throw std::runtime_error("pass '" + pass.id + "' does not carry " + label);
    return state;
  }

  template<typename Error>
  inline void rejectUnknownFields(const QJsonObject& object, const std::string& path,
                                  std::initializer_list<const char*> allowed, Error error) {
    for (auto it = object.begin(); it != object.end(); ++it) {
      const std::string key = it.key().toStdString();
      const bool matched = std::find(allowed.begin(), allowed.end(), key) != allowed.end();
      if (!matched)
        error(path + "." + key, "unknown field");
    }
  }

  template<typename Error>
  [[nodiscard]] inline QJsonObject objectField(const QJsonObject& object, const char* key,
                                               const std::string& path, Error error) {
    const auto value = object.value(key);
    if (value.isUndefined())
      return {};
    if (!value.isObject())
      error(path + "." + key, "expected object");
    return value.toObject();
  }

  template<typename Error>
  [[nodiscard]] inline int intField(const QJsonObject& object, const char* key,
                                    const std::string& path, Error error) {
    const auto value = object.value(key);
    if (!value.isDouble())
      error(path + "." + key, "expected integer");
    const double number = value.toDouble();
    if (!core::json::isIntegerValued(number))
      error(path + "." + key, "expected integer");
    return static_cast<int>(number);
  }

  template<typename Error>
  [[nodiscard]] inline std::uint64_t uint64Field(const QJsonObject& object, const char* key,
                                                  const std::string& path, Error error) {
    constexpr std::uint64_t maxExact = 9007199254740991ULL;
    const auto value = object.value(key);
    if (!value.isDouble())
      error(path + "." + key, "expected non-negative integer");
    const double number = value.toDouble();
    if (!core::json::isIntegerValued(number) || number < 0.0 ||
        number > static_cast<double>(maxExact))
      error(path + "." + key, "expected non-negative integer");
    return static_cast<std::uint64_t>(number);
  }

  template<typename Error>
  [[nodiscard]] inline std::uint8_t byteField(const QJsonObject& object, const char* key,
                                               const std::string& path, Error error) {
    const int v = intField(object, key, path, error);
    if (v < 0 || v > 255)
      error(path + "." + key, "expected integer from 0 to 255");
    return static_cast<std::uint8_t>(v);
  }

  template<typename Error>
  [[nodiscard]] inline double doubleField(const QJsonObject& object, const char* key,
                                          const std::string& path, Error error) {
    const auto value = object.value(key);
    if (!value.isDouble())
      error(path + "." + key, "expected number");
    const double number = value.toDouble();
    if (!std::isfinite(number))
      error(path + "." + key, "expected finite number");
    return number;
  }

  template<typename Error>
  [[nodiscard]] inline bool boolField(const QJsonObject& object, const char* key,
                                      const std::string& path, Error error) {
    const auto value = object.value(key);
    if (!value.isBool())
      error(path + "." + key, "expected boolean");
    return value.toBool();
  }

  template<typename Error>
  [[nodiscard]] inline std::string stringField(const QJsonObject& object, const char* key,
                                               const std::string& path, Error error) {
    const auto value = object.value(key);
    if (!value.isString())
      error(path + "." + key, "expected string");
    return value.toString().toStdString();
  }

  template<class T, typename Error>
  [[nodiscard]] inline T enumValue(const std::string& value,
                                   std::initializer_list<std::pair<const char*, T>> values,
                                   const std::string& path, Error error) {
    for (const auto& [name, parsed] : values) {
      if (value == name)
        return parsed;
    }
    error(path, "unknown value '" + value + "'");
    throw std::logic_error("unreachable");
  }

  template<class T>
  [[nodiscard]] inline const char* enumName(T value,
                                             std::initializer_list<std::pair<T, const char*>> values,
                                             const char* fallback = "unknown") {
    for (const auto& [parsed, name] : values) {
      if (value == parsed)
        return name;
    }
    return fallback;
  }

  // ---- Rasterizer enum/string conversions ------------------------------------
  //
  // Shared by RasterPassState.cpp and RenderEngineOptions.cpp, whose JSON
  // schemas for the rasterizer settings they each expose (per-pass overrides
  // vs. render-engine-wide options) use the same string vocabulary. Each
  // takes the caller's `Error` callback (`stateError`/`optionsError`) so the
  // thrown exception carries the right "pass state" vs. "render engine
  // options" label.

  template<typename Error>
  inline Rasterizer::TessellationQuality
  tessellationQualityFromString(const std::string& value, const std::string& path, Error error) {
    if (value == "preview")
      return Rasterizer::TessellationQuality::Preview;
    if (value == "balanced")
      return Rasterizer::TessellationQuality::Balanced;
    if (value == "final")
      return Rasterizer::TessellationQuality::Final;
    error(path, "expected preview, balanced, or final");
    throw std::logic_error("unreachable");
  }

  template<typename Error>
  inline Rasterizer::CullMode
  cullModeFromString(const std::string& value, const std::string& path, Error error) {
    if (value == "both")
      return Rasterizer::CullMode::Both;
    if (value == "back")
      return Rasterizer::CullMode::Back;
    if (value == "front")
      return Rasterizer::CullMode::Front;
    error(path, "expected both, back, or front");
    throw std::logic_error("unreachable");
  }

  template<typename Error>
  inline Rasterizer::MSAAShadingMode
  msaaShadingModeFromString(const std::string& value, const std::string& path, Error error) {
    if (value == "per_sample")
      return Rasterizer::MSAAShadingMode::PerSample;
    if (value == "per_fragment")
      return Rasterizer::MSAAShadingMode::PerFragment;
    error(path, "expected per_sample or per_fragment");
    throw std::logic_error("unreachable");
  }

  template<typename Error>
  inline Rasterizer::BlendFactor
  blendFactorFromString(const std::string& value, const std::string& path, Error error) {
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
    throw std::logic_error("unreachable");
  }

  template<typename Error>
  inline Rasterizer::BlendOp
  blendOpFromString(const std::string& value, const std::string& path, Error error) {
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
    throw std::logic_error("unreachable");
  }

  template<typename Error>
  inline Rasterizer::AlphaFunc
  alphaFuncFromString(const std::string& value, const std::string& path, Error error) {
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
    throw std::logic_error("unreachable");
  }

  template<typename Error>
  inline Rasterizer::ShadowFilterMode
  shadowFilterModeFromString(const std::string& value, const std::string& path, Error error) {
    if (value == "pcf")
      return Rasterizer::ShadowFilterMode::PCF;
    if (value == "pcss")
      return Rasterizer::ShadowFilterMode::PCSS;
    error(path, "expected pcf or pcss");
    throw std::logic_error("unreachable");
  }

  template<typename Error>
  inline std::uint8_t
  colorWriteMaskFromString(const std::string& value, const std::string& path, Error error) {
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
        error(path, "expected r, g, b, all, or none");
      }
    }
    return mask;
  }
}
