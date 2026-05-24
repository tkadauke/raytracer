#pragma once

#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace core::math::interpolation {

  /**
  * Interpolation used between two adjacent samples.
  */
  enum class InterpolationMode { Step, Linear, SmoothStep };

  /**
  * Names for interpolation modes in serialized inputs.
  */
  class InterpolationModes {
  public:
    /**
    * @returns the canonical serialized name for @p mode.
    */
    static std::string name(InterpolationMode mode) {
      switch (mode) {
      case InterpolationMode::Step:
        return "step";
      case InterpolationMode::Linear:
        return "linear";
      case InterpolationMode::SmoothStep:
        return "smoothstep";
      }

      throw std::invalid_argument("unknown interpolation mode");
    }

    /**
    * Parses a serialized interpolation mode.
    *
    * @throws std::invalid_argument if @p name is not a supported interpolation
    *   mode.
    */
    static InterpolationMode fromName(const std::string& name) {
      if (name == "step")
        return InterpolationMode::Step;
      if (name == "linear")
        return InterpolationMode::Linear;
      if (name == "smoothstep")
        return InterpolationMode::SmoothStep;

      throw std::invalid_argument("unsupported interpolation mode: " + name);
    }
  };

  namespace detail {

    inline double smoothstepWeight(double t) {
      return t * t * (3.0 - 2.0 * t);
    }

    template<class Value, class = void>
    struct IsLinearInterpolatable : std::false_type {};

    template<class Value>
    struct IsLinearInterpolatable<
      Value, std::void_t<decltype(std::declval<Value>() +
                                  (std::declval<Value>() - std::declval<Value>()) * 0.5)>>
        : std::true_type {};

  } // namespace detail

  /**
  * Holds the first value until the next sample is reached.
  *
  * @htmlonly
  * <script type="text/javascript" src="figure.js"></script>
  * <script type="text/javascript" src="interpolation_step.js"></script>
  * @endhtmlonly
  */
  template<class Value>
  class StepInterpolator {
  public:
    static Value interpolate(const Value& from, const Value& to, double t) {
      return t >= 1.0 ? to : from;
    }
  };

  /**
  * Blends linearly between two values.
  *
  * @htmlonly
  * <script type="text/javascript" src="figure.js"></script>
  * <script type="text/javascript" src="interpolation_linear.js"></script>
  * @endhtmlonly
  */
  template<class Value>
  class LinearInterpolator {
  public:
    static Value interpolate(const Value& from, const Value& to, double t) {
      return from + (to - from) * t;
    }
  };

  /**
  * Blends between two values with smooth ease-in/ease-out timing.
  *
  * @htmlonly
  * <script type="text/javascript" src="figure.js"></script>
  * <script type="text/javascript" src="interpolation_smoothstep.js"></script>
  * @endhtmlonly
  */
  template<class Value>
  class SmoothStepInterpolator {
  public:
    static Value interpolate(const Value& from, const Value& to, double t) {
      return LinearInterpolator<Value>::interpolate(from, to, detail::smoothstepWeight(t));
    }
  };

  /**
  * Dispatches a requested interpolation mode for a value type.
  */
  template<class Value>
  class Interpolator {
  public:
    static Value interpolate(InterpolationMode mode, const Value& from, const Value& to, double t) {
      if (mode == InterpolationMode::Step)
        return StepInterpolator<Value>::interpolate(from, to, t);

      if constexpr (detail::IsLinearInterpolatable<Value>::value) {
        if (mode == InterpolationMode::Linear)
          return LinearInterpolator<Value>::interpolate(from, to, t);
        if (mode == InterpolationMode::SmoothStep)
          return SmoothStepInterpolator<Value>::interpolate(from, to, t);
      } else {
        throw std::logic_error("interpolation mode requires a linearly interpolatable value type");
      }

      throw std::invalid_argument("unknown interpolation mode");
    }
  };

} // namespace core::math::interpolation
