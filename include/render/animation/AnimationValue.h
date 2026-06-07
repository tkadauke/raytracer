#pragma once

#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

#include "core/Color.h"
#include "core/math/Matrix.h"
#include "core/math/Vector.h"
#include "core/math/interpolation/Interpolation.h"

namespace render::animation {

  /**
  * Explicit value payload supported by render-time animation tracks.
  *
  * This type is the Qt-free boundary between editable world timeline data and
  * render-time sampling. Only values that render code can consume directly are
  * accepted.
  */
  class AnimationValue {
  public:
    enum class Type { Double, Integer, Vector3, Color, Matrix4, Boolean, String };

    AnimationValue(double value);
    AnimationValue(int value);
    AnimationValue(const Vector3d& value);
    AnimationValue(const Colord& value);
    AnimationValue(const Matrix4d& value);
    AnimationValue(bool value);
    AnimationValue(std::string value);
    AnimationValue(const char* value);

    template<class Value>
    [[nodiscard]] static constexpr bool supports() noexcept {
      using Decayed = std::decay_t<Value>;
      return std::is_same_v<Decayed, double> || std::is_same_v<Decayed, int> ||
             std::is_same_v<Decayed, Vector3d> || std::is_same_v<Decayed, Colord> ||
             std::is_same_v<Decayed, Matrix4d> || std::is_same_v<Decayed, bool> ||
             std::is_same_v<Decayed, std::string> || std::is_same_v<Decayed, const char*>;
    }

    template<class Value>
    [[nodiscard]] static AnimationValue from(Value&& value) {
      using Decayed = std::decay_t<Value>;
      if constexpr (supports<Decayed>()) {
        return AnimationValue(std::forward<Value>(value));
      } else {
        throw std::invalid_argument("unsupported render animation value type");
      }
    }

    [[nodiscard]] Type type() const noexcept;
    [[nodiscard]] const char* typeName() const noexcept;

    template<class Value>
    [[nodiscard]] const Value& get() const {
      return std::get<Value>(m_value);
    }

    [[nodiscard]] const std::variant<double, int, Vector3d, Colord, Matrix4d, bool, std::string>&
    variant() const noexcept;

  private:
    std::variant<double, int, Vector3d, Colord, Matrix4d, bool, std::string> m_value;
  };

  /**
  * Interpolates two same-typed render animation values.
  *
  * `Step` interpolation supports every explicit payload. `Linear` and
  * `SmoothStep` support numeric, vector, color, and matrix payloads.
  *
  * @throws std::invalid_argument when the value types differ.
  * @throws std::logic_error when the interpolation mode is not supported by the
  *   value type.
  */
  [[nodiscard]] AnimationValue interpolate(core::math::interpolation::InterpolationMode mode,
                                           const AnimationValue& from, const AnimationValue& to,
                                           double t);

} // namespace render::animation
