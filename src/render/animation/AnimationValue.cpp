#include "render/animation/AnimationValue.h"

#include <utility>

namespace {

  namespace interpolation = core::math::interpolation;

  template<class Value>
  render::animation::AnimationValue interpolateArithmetic(interpolation::InterpolationMode mode,
                                                          const Value& from, const Value& to,
                                                          double t) {
    return render::animation::AnimationValue(
      interpolation::Interpolator<Value>::interpolate(mode, from, to, t));
  }

  Matrix4d interpolateMatrix(interpolation::InterpolationMode mode, const Matrix4d& from,
                             const Matrix4d& to, double t) {
    if (mode == interpolation::InterpolationMode::Step)
      return interpolation::StepInterpolator<Matrix4d>::interpolate(from, to, t);

    double weight = t;
    if (mode == interpolation::InterpolationMode::SmoothStep) {
      weight = t * t * (3.0 - 2.0 * t);
    } else if (mode != interpolation::InterpolationMode::Linear) {
      throw std::invalid_argument("unknown interpolation mode");
    }

    Matrix4d result;
    for (int row = 0; row != 4; ++row) {
      for (int col = 0; col != 4; ++col) {
        result[row][col] = from.cell(row, col) + (to.cell(row, col) - from.cell(row, col)) * weight;
      }
    }
    return result;
  }

  void requireSameType(const render::animation::AnimationValue& from,
                       const render::animation::AnimationValue& to) {
    if (from.type() != to.type())
      throw std::invalid_argument("render animation values must have the same type");
  }

} // namespace

namespace render::animation {

  AnimationValue::AnimationValue(double value)
      : m_value(value) {
  }

  AnimationValue::AnimationValue(int value)
      : m_value(value) {
  }

  AnimationValue::AnimationValue(const Vector3d& value)
      : m_value(value) {
  }

  AnimationValue::AnimationValue(const Colord& value)
      : m_value(value) {
  }

  AnimationValue::AnimationValue(const Matrix4d& value)
      : m_value(value) {
  }

  AnimationValue::AnimationValue(bool value)
      : m_value(value) {
  }

  AnimationValue::AnimationValue(std::string value)
      : m_value(std::move(value)) {
  }

  AnimationValue::AnimationValue(const char* value)
      : m_value(std::string(value)) {
  }

  AnimationValue::Type AnimationValue::type() const noexcept {
    return static_cast<Type>(m_value.index());
  }

  const char* AnimationValue::typeName() const noexcept {
    switch (type()) {
    case Type::Double:
      return "double";
    case Type::Integer:
      return "int";
    case Type::Vector3:
      return "Vector3d";
    case Type::Color:
      return "Colord";
    case Type::Matrix4:
      return "Matrix4d";
    case Type::Boolean:
      return "bool";
    case Type::String:
      return "string";
    }

    return "unknown";
  }

  const std::variant<double, int, Vector3d, Colord, Matrix4d, bool, std::string>&
  AnimationValue::variant() const noexcept {
    return m_value;
  }

  AnimationValue interpolate(interpolation::InterpolationMode mode, const AnimationValue& from,
                             const AnimationValue& to, double t) {
    requireSameType(from, to);

    switch (from.type()) {
    case AnimationValue::Type::Double:
      return interpolateArithmetic(mode, from.get<double>(), to.get<double>(), t);
    case AnimationValue::Type::Integer:
      return interpolateArithmetic(mode, from.get<int>(), to.get<int>(), t);
    case AnimationValue::Type::Vector3:
      return interpolateArithmetic(mode, from.get<Vector3d>(), to.get<Vector3d>(), t);
    case AnimationValue::Type::Color:
      return interpolateArithmetic(mode, from.get<Colord>(), to.get<Colord>(), t);
    case AnimationValue::Type::Matrix4:
      return AnimationValue(interpolateMatrix(mode, from.get<Matrix4d>(), to.get<Matrix4d>(), t));
    case AnimationValue::Type::Boolean:
      if (mode == interpolation::InterpolationMode::Step)
        return interpolateArithmetic(mode, from.get<bool>(), to.get<bool>(), t);
      break;
    case AnimationValue::Type::String:
      if (mode == interpolation::InterpolationMode::Step)
        return interpolateArithmetic(mode, from.get<std::string>(), to.get<std::string>(), t);
      break;
    }

    throw std::logic_error("render animation value type supports only step interpolation");
  }

} // namespace render::animation
