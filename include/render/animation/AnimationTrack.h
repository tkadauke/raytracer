#pragma once

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <stdexcept>
#include <utility>
#include <vector>

#include "render/animation/AnimationValue.h"

namespace render::animation {

  /**
  * One runtime-continuous key on a render animation track.
  */
  struct Keyframe {
    /**
    * Runtime position in frames, seconds, or another caller-owned continuous
    * time domain.
    */
    double time;

    /**
    * Explicit value held by this key.
    */
    AnimationValue value;
  };

  /**
  * Qt-free render-time animation track.
  *
  * The track clamps outside its key range and can be sampled at arbitrary
  * continuous times, including subframe shutter offsets. All key values must
  * have the same explicit `AnimationValue::Type`.
  */
  class AnimationTrack {
  public:
    AnimationTrack(std::initializer_list<Keyframe> keyframes,
                   core::math::interpolation::InterpolationMode interpolationMode =
                     core::math::interpolation::InterpolationMode::Linear)
        : AnimationTrack(std::vector<Keyframe>(keyframes), interpolationMode) {
    }

    explicit AnimationTrack(std::vector<Keyframe> keyframes,
                            core::math::interpolation::InterpolationMode interpolationMode =
                              core::math::interpolation::InterpolationMode::Linear)
        : m_keyframes(std::move(keyframes)),
          m_interpolationMode(interpolationMode) {
      if (m_keyframes.empty())
        throw std::invalid_argument("render animation track must have at least one keyframe");

      for (const auto& keyframe : m_keyframes) {
        if (!std::isfinite(keyframe.time))
          throw std::invalid_argument("render animation keyframe time must be finite");
      }

      std::sort(m_keyframes.begin(), m_keyframes.end(),
                [](const auto& left, const auto& right) { return left.time < right.time; });

      const auto duplicate = std::adjacent_find(
        m_keyframes.begin(), m_keyframes.end(),
        [](const auto& left, const auto& right) { return left.time == right.time; });
      if (duplicate != m_keyframes.end())
        throw std::invalid_argument("render animation track contains duplicate keyframe times");

      const auto type = m_keyframes.front().value.type();
      const auto mismatch =
        std::find_if(m_keyframes.begin(), m_keyframes.end(),
                     [type](const auto& keyframe) { return keyframe.value.type() != type; });
      if (mismatch != m_keyframes.end())
        throw std::invalid_argument("render animation track keyframe value types must match");
    }

    [[nodiscard]] const std::vector<Keyframe>& keyframes() const noexcept {
      return m_keyframes;
    }

    [[nodiscard]] core::math::interpolation::InterpolationMode interpolationMode() const noexcept {
      return m_interpolationMode;
    }

    [[nodiscard]] AnimationValue::Type valueType() const noexcept {
      return m_keyframes.front().value.type();
    }

    [[nodiscard]] AnimationValue sample(double time) const {
      if (!std::isfinite(time))
        throw std::invalid_argument("render animation sample time must be finite");

      if (time <= m_keyframes.front().time)
        return m_keyframes.front().value;
      if (time >= m_keyframes.back().time)
        return m_keyframes.back().value;

      const auto next =
        std::upper_bound(m_keyframes.begin(), m_keyframes.end(), time,
                         [](double value, const auto& keyframe) { return value < keyframe.time; });
      const auto previous = next - 1;
      if (previous->time == time)
        return previous->value;

      const auto span = next->time - previous->time;
      const auto t = (time - previous->time) / span;
      return interpolate(m_interpolationMode, previous->value, next->value, t);
    }

  private:
    std::vector<Keyframe> m_keyframes;
    core::math::interpolation::InterpolationMode m_interpolationMode;
  };

} // namespace render::animation
