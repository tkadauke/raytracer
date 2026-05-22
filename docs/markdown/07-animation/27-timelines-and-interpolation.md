# 27. Timelines and interpolation

Animation starts with a small question: if a scene says "at frame 1 this
value is A, and at frame 24 this value is B," what value should frame 12 see?
The core animation layer answers that question without pulling in Qt, JSON,
scene graphs, or a render engine.

## 27.1 Time is frame-based

The timeline primitive lives in
[`Timeline`](../../../include/core/animation/Timeline.h). It records a start
frame, an end frame, and frames per second. Construction rejects invalid frame
ranges and non-positive frame rates, so downstream code can treat the timeline
as a valid interval.

`Timeline::secondsForFrame()` converts frame numbers to seconds relative to
the timeline start:

```cpp
// include/core/animation/Timeline.h
seconds = (frame - startFrame) / fps;
```

That means frame `startFrame` is time zero for the timeline.

## 27.2 Tracks are typed

[`AnimationTrack<T>`](../../../include/core/animation/AnimationTrack.h) stores
`Keyframe<T>` values:

```cpp
template<class Value>
struct Keyframe {
  int frame;
  Value value;
};
```

The track constructor sorts keys by frame and rejects duplicates. Sampling a
frame outside the keyed range clamps to the nearest key: before the first key
returns the first value, and after the last key returns the last value. A
single-key track therefore has a stable value for every frame.

Between adjacent keys, the track computes a normalized fraction:

```cpp
t = (frame - previous.frame) / (next.frame - previous.frame);
```

It then delegates to the interpolation layer. The track does not know whether
the value is a scalar, `Vector3d`, or `Colord`; it asks the core math
interpolator to sample that segment.

## 27.3 Interpolation is core math

Interpolation policies live under
[`include/core/math/interpolation`](../../../include/core/math/interpolation/),
not under animation. Animation consumes this core math layer when a track
needs a value between two samples.

The policies are:

- `StepInterpolator<T>` — hold the first value.
- `LinearInterpolator<T>` — blend from `from` to `to` with
  `from + (to - from) * t`.
- `SmoothStepInterpolator<T>` — remap `t` with `t * t * (3 - 2 * t)`, then
  use the same linear value blend.

`Interpolator<T>` dispatches an `InterpolationMode` to the matching policy.
Linear and smoothstep modes require the value type to support addition,
subtraction, and scalar multiplication. Step interpolation works for
non-numeric values because it only returns the first value.

## 27.4 Step interpolation

Step interpolation is the hold behavior. It is what bools, enums, references,
and other discrete values usually need.

<!-- widget: interpolation_step -->

In an animation track, exact-key lookup happens before segment interpolation.
So a step track returns the previous key between two keys, and returns the
next key exactly when the sampled frame is that key's frame.

## 27.5 Linear interpolation

Linear interpolation maps time progress directly to value progress. If `t` is
`0.25`, the sampled value is one quarter of the way from A to B.

<!-- widget: interpolation_linear -->

This is the default mode for `AnimationTrack<T>` because it is the most useful
numeric behavior and works for scalar, vector, and color values.

## 27.6 Smoothstep interpolation

Smoothstep interpolation still follows the straight value path from A to B,
but it does not move at constant speed along that path. It eases out of A,
moves fastest near the middle, and eases into B.

<!-- widget: interpolation_smoothstep -->

The implementation is a timing remap plus a linear blend:

```cpp
u = t * t * (3 - 2 * t);
value = LinearInterpolator<T>::interpolate(from, to, u);
```

So the path is linear in value space, but the value as a function of frame is
not linear.

## 27.7 Exercises

1. Given keys `(10, 4.0)` and `(22, 10.0)`, compute the linear sampled value
   at frame 16.
2. Why can `StepInterpolator<std::string>` work while
   `LinearInterpolator<std::string>` cannot?
3. For `SmoothStepInterpolator<double>` from `0` to `1`, compare the sampled
   values at `t = 0.2`, `0.5`, and `0.8` against linear interpolation.
4. Why does smoothstep have the same midpoint as linear interpolation but
   different values at `t = 0.2` and `t = 0.8`?

## See also

- Volume index: [Volume VII — Animation](README.md)
- Previous volume:
  [Volume VI — Tools & I/O](../06-tools-and-io/README.md)
- Motion blur already in the renderer:
  [16. Instances and motion blur](../03-scene-structure/16-instances-and-motion-blur.md)
- Sampling streams:
  [10. Sampling and anti-aliasing §10.4](../02-ray-rendering/10-sampling-and-anti-aliasing.md#10-4-two-access-patterns-sets-and-streams)

## Source anchors

<!-- source-anchors -->
- `include/core/animation/AnimationTrack.h`
- `include/core/animation/Timeline.h`
- `include/core/math/interpolation/Interpolation.h`
- `test/unit/core/animation/AnimationTrackTest.cpp`
- `test/unit/core/math/interpolation/InterpolationTest.cpp`
<!-- /source-anchors -->
