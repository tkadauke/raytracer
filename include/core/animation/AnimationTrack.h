#pragma once

#include <algorithm>
#include <initializer_list>
#include <stdexcept>
#include <utility>
#include <vector>

#include "core/math/interpolation/Interpolation.h"

namespace core::animation {

  namespace interpolation = core::math::interpolation;

  /**
  * One typed keyframe on an animation track.
  *
  * The frame number is an exact integer timeline frame. The value is copied
  * into the owning `AnimationTrack<Value>`.
  */
  template<class Value>
  struct Keyframe {
    /**
    * Exact integer frame where @p value is keyed.
    */
    int frame;

    /**
    * Value held by this keyframe.
    */
    Value value;
  };

  /**
  * A sorted, typed animation track with one interpolation mode.
  *
  * The track clamps outside its key range: frames before the first key evaluate
  * to the first key value, and frames after the last key evaluate to the last
  * key value.
  *
  * @tparam Value value type sampled by the track. `Step` interpolation works
  *   with any copyable value type. `Linear` and `SmoothStep` interpolation
  *   require addition, subtraction, and multiplication by a scalar, as checked
  *   by `core::math::interpolation::Interpolator<Value>`.
  */
  template<class Value>
  class AnimationTrack {
  public:
    /**
    * Creates a track from an initializer list of keyframes.
    *
    * @param keyframes keyed values that define the track.
    * @param interpolationMode mode used to sample between adjacent keyframes.
    * @throws std::invalid_argument if @p keyframes is empty or contains
    *   duplicate frame numbers.
    */
    AnimationTrack(
      std::initializer_list<Keyframe<Value>> keyframes,
      interpolation::InterpolationMode interpolationMode = interpolation::InterpolationMode::Linear)
        : AnimationTrack(std::vector<Keyframe<Value>>(keyframes), interpolationMode) {
    }

    /**
    * Creates a track from a vector of keyframes.
    *
    * Keyframes are sorted by frame number during construction.
    *
    * @param keyframes keyed values that define the track.
    * @param interpolationMode mode used to sample between adjacent keyframes.
    * @throws std::invalid_argument if @p keyframes is empty or contains
    *   duplicate frame numbers.
    */
    explicit AnimationTrack(
      std::vector<Keyframe<Value>> keyframes,
      interpolation::InterpolationMode interpolationMode = interpolation::InterpolationMode::Linear)
        : m_keyframes(std::move(keyframes)),
          m_interpolationMode(interpolationMode) {
      if (m_keyframes.empty())
        throw std::invalid_argument("animation track must have at least one keyframe");

      std::sort(m_keyframes.begin(), m_keyframes.end(),
                [](const auto& left, const auto& right) { return left.frame < right.frame; });

      const auto duplicate = std::adjacent_find(
        m_keyframes.begin(), m_keyframes.end(),
        [](const auto& left, const auto& right) { return left.frame == right.frame; });
      if (duplicate != m_keyframes.end())
        throw std::invalid_argument("animation track contains duplicate keyframe frames");
    }

    /**
    * @returns the sorted keyframes owned by the track.
    */
    [[nodiscard]] const std::vector<Keyframe<Value>>& keyframes() const noexcept {
      return m_keyframes;
    }

    /**
    * @returns the interpolation mode used between adjacent keyframes.
    */
    [[nodiscard]] interpolation::InterpolationMode interpolationMode() const noexcept {
      return m_interpolationMode;
    }

    /**
    * Evaluates the track at @p frame.
    *
    * Exact keyframes return their keyed value. Frames before the first key
    * return the first key's value; frames after the last key return the last
    * key's value. Frames between two keys are sampled with the track's
    * interpolation mode.
    *
    * @param frame frame number to sample.
    * @throws std::logic_error if the interpolation mode requires arithmetic
    *   that `Value` does not support.
    */
    [[nodiscard]] Value sample(int frame) const {
      if (frame <= m_keyframes.front().frame)
        return m_keyframes.front().value;
      if (frame >= m_keyframes.back().frame)
        return m_keyframes.back().value;

      const auto next =
        std::upper_bound(m_keyframes.begin(), m_keyframes.end(), frame,
                         [](int value, const auto& keyframe) { return value < keyframe.frame; });
      const auto previous = next - 1;
      if (previous->frame == frame)
        return previous->value;

      const auto span = static_cast<double>(next->frame - previous->frame);
      const auto t = static_cast<double>(frame - previous->frame) / span;
      return interpolation::Interpolator<Value>::interpolate(m_interpolationMode, previous->value,
                                                             next->value, t);
    }

  private:
    std::vector<Keyframe<Value>> m_keyframes;
    interpolation::InterpolationMode m_interpolationMode;
  };

} // namespace core::animation
