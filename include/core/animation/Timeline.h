#pragma once

#include <cmath>
#include <stdexcept>

namespace core::animation {

/**
  * Exact integer frame plus its seconds position on a timeline.
  *
  * `FrameTime` is a value object returned by `Timeline::timeAtFrame()`. The
  * frame number is preserved exactly while `secondsFromStart()` gives the
  * corresponding position relative to the timeline's start frame.
  */
class FrameTime {
public:
  /**
    * Creates a frame-time value.
    *
    * @param frame exact integer frame number.
    * @param secondsFromStart seconds relative to the timeline's start frame.
    */
  constexpr FrameTime(int frame, double secondsFromStart)
    : m_frame(frame), m_secondsFromStart(secondsFromStart) {
  }

  /**
    * @returns the exact integer frame number.
    */
  [[nodiscard]] constexpr int frame() const noexcept {
    return m_frame;
  }

  /**
    * @returns seconds relative to the timeline's start frame.
    */
  [[nodiscard]] constexpr double secondsFromStart() const noexcept {
    return m_secondsFromStart;
  }

private:
  int m_frame;
  double m_secondsFromStart;
};

/**
  * A frame-based scene timeline.
  *
  * `Timeline` owns the valid frame range and frame rate used to evaluate
  * frame-based animation tracks. The frame range is inclusive: both
  * `startFrame()` and `endFrame()` are considered part of the timeline.
  */
class Timeline {
public:
  /**
    * Creates a timeline with an inclusive frame range.
    *
    * @param startFrame first valid frame in the timeline.
    * @param endFrame last valid frame in the timeline.
    * @param fps number of frames per second.
    * @throws std::invalid_argument if @p fps is not finite or positive, or if
    *   @p endFrame is less than @p startFrame.
    */
  Timeline(int startFrame, int endFrame, double fps)
    : m_startFrame(startFrame), m_endFrame(endFrame), m_fps(fps) {
    if (!std::isfinite(fps) || fps <= 0.0)
      throw std::invalid_argument("timeline fps must be finite and positive");
    if (endFrame < startFrame)
      throw std::invalid_argument("timeline end frame must be greater than or equal to start frame");
  }

  /**
    * @returns the first frame in the inclusive timeline range.
    */
  [[nodiscard]] int startFrame() const noexcept {
    return m_startFrame;
  }

  /**
    * @returns the last frame in the inclusive timeline range.
    */
  [[nodiscard]] int endFrame() const noexcept {
    return m_endFrame;
  }

  /**
    * @returns the number of frames per second.
    */
  [[nodiscard]] double fps() const noexcept {
    return m_fps;
  }

  /**
    * @returns true if @p frame is inside the inclusive timeline range.
    *
    * @param frame frame number to test.
    */
  [[nodiscard]] bool containsFrame(int frame) const noexcept {
    return m_startFrame <= frame && frame <= m_endFrame;
  }

  /**
    * @returns seconds since `startFrame()` for @p frame.
    *
    * @param frame frame number to convert.
    */
  [[nodiscard]] double secondsForFrame(int frame) const noexcept {
    return static_cast<double>(frame - m_startFrame) / m_fps;
  }

  /**
    * @returns a `FrameTime` value for @p frame.
    *
    * @param frame frame number to convert.
    */
  [[nodiscard]] FrameTime timeAtFrame(int frame) const noexcept {
    return FrameTime(frame, secondsForFrame(frame));
  }

private:
  int m_startFrame;
  int m_endFrame;
  double m_fps;
};

}  // namespace core::animation
