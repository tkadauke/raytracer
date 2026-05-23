# Timelines and interpolation

Animation starts with a small question: if a scene says "at frame 1 this
value is A, and at frame 24 this value is B," what value should frame 12 see?
The core animation layer answers that question without pulling in Qt, JSON,
scene graphs, or a render engine.

## <a id="time-is-frame-based"></a>Time is frame-based
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

## <a id="tracks-are-typed"></a>Tracks are typed
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

## <a id="interpolation-is-core-math"></a>Interpolation is core math
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

## <a id="step-interpolation"></a>Step interpolation
Step interpolation is the hold behavior. It is what bools, enums, references,
and other discrete values usually need.

<!-- widget: interpolation_step -->

In an animation track, exact-key lookup happens before segment interpolation.
So a step track returns the previous key between two keys, and returns the
next key exactly when the sampled frame is that key's frame.

## <a id="linear-interpolation"></a>Linear interpolation
Linear interpolation maps time progress directly to value progress. If `t` is
`0.25`, the sampled value is one quarter of the way from A to B.

<!-- widget: interpolation_linear -->

This is the default mode for `AnimationTrack<T>` because it is the most useful
numeric behavior and works for scalar, vector, and color values.

## <a id="smoothstep-interpolation"></a>Smoothstep interpolation
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

## <a id="world-scene-timelines"></a>World-scene timelines
The world-scene layer connects the core timeline math to editable scene files.
[`world::Timeline`](../../../include/world/animation/Timeline.h) stores the
animation frame range and a list of
[`world::AnimationTrack`](../../../include/world/animation/AnimationTrack.h)
objects. `Scene` reads and writes these tracks through a top-level
`animation` block:

```json
{
  "animation": {
    "fps": 24,
    "startFrame": 1,
    "endFrame": 120,
    "tracks": [
      {
        "target": "camera-id",
        "property": "position",
        "interpolation": "linear",
        "keys": [
          { "frame": 1, "value": [0, 0, 0] },
          { "frame": 120, "value": [4, 0, 0] }
        ]
      }
    ]
  }
}
```

World tracks target one element id and one direct `Q_PROPERTY` name. During
evaluation, the track finds the target element, reads the property's Qt type,
decodes the keyed JSON values, samples the core typed track, and writes the
sampled value back through `QObject::setProperty()`.

The supported world property types are:

- `double`
- `Vector3d`
- `Colord`
- `bool`, with step interpolation only

`Scene::evaluateAnimationAtFrame(frame)` applies the scene's tracks in place.
`Scene::evaluatedAtFrame(frame)` returns a deep-copied scene with the tracks
applied, leaving the authoring scene unchanged. Missing targets, missing
properties, unsupported property types, and non-step bool interpolation fail
with explicit runtime errors.

## <a id="rendering-one-frame"></a>Rendering one frame
`rendercli --frame N` loads a JSON scene, evaluates its world timeline at
frame `N`, and then builds the runtime render scene and camera from that
evaluated world state. The flag does not require an animation block; static
scenes render unchanged.

The checked-in animated scene fixtures exercise the value types that world
tracks can currently write:

- [`animation_frame_demo.json`](../../../scenes/animation_frame_demo.json)
  moves one sphere across its timeline.
- [`animated_camera_pan.json`](../../../scenes/animated_camera_pan.json)
  animates camera `position`, `target`, and `zoom`.
- [`animated_light_sweep.json`](../../../scenes/animated_light_sweep.json)
  animates a directional light's `direction`, `color`, and `intensity`.
- [`animated_material_fade.json`](../../../scenes/animated_material_fade.json)
  animates scene `background` and texture `color`.
- [`animated_motion_blur_sweep.json`](../../../scenes/animated_motion_blur_sweep.json)
  animates a sphere's timeline `position` and per-shutter `velocity`.
- [`animated_visibility_steps.json`](../../../scenes/animated_visibility_steps.json)
  uses step interpolation on `visible` bool tracks.

For a single-frame render, choose a frame and output path:

```sh
rendercli --engine raster --frame 1 \
  scenes/animation_frame_demo.json frame_0001.png

rendercli --engine raster --frame 48 \
  scenes/animation_frame_demo.json frame_0048.png
```

Frame parsing is strict: `--frame` accepts integer frame numbers.

## <a id="rendering-an-image-sequence"></a>Rendering an image sequence
`rendercli --animation` renders each frame in a timeline range to a separate
image file. By default, the range comes from the scene's `animation` block.
`--frame_start`, `--frame_end`, and `--fps` override the loaded timeline values
for the sequence run.

The output path must contain exactly one printf-style integer placeholder:

```sh
rendercli --engine raster --animation \
  scenes/animation_frame_demo.json frames/frame_%04d.png
```

For each frame, `rendercli` evaluates the world scene at that frame, builds a
fresh runtime render scene from the evaluated world state, renders one image,
and prints a progress line with the frame number, output path, and render
time. Static scenes do not have a timeline range, so `--animation` requires an
`animation` block.

## <a id="previewing-animation-in-modeler"></a>Previewing animation in Modeler
Modeler reads and writes the same top-level `animation` block as
`rendercli`. When a loaded scene has a timeline, the Timeline dock exposes the
inclusive frame range as a slider and spinbox. Changing the current frame
evaluates a copied scene and sends that evaluated copy to the live preview.

The property editor stays attached to the authoring scene. Editing an
property there changes the base value stored in the JSON scene; frame
evaluation is applied only to the copied scene used for preview and final
renders. Changing frames resets the central preview to the evaluated scene
camera, and the viewport remains interactive from that keyed pose.

## <a id="exercises"></a>Exercises
1. Given keys `(10, 4.0)` and `(22, 10.0)`, compute the linear sampled value
   at frame 16.
2. Why can `StepInterpolator<std::string>` work while
   `LinearInterpolator<std::string>` cannot?
3. For `SmoothStepInterpolator<double>` from `0` to `1`, compare the sampled
   values at `t = 0.2`, `0.5`, and `0.8` against linear interpolation.
4. Why does smoothstep have the same midpoint as linear interpolation but
   different values at `t = 0.2` and `t = 0.8`?

## See also

- Volume index: [Animation](README.md)
- Previous volume:
  [Render graph](../render-graph/README.md)
- Motion blur already in the renderer:
  [Instances and motion blur](../scene-structure/instances-and-motion-blur.md)
- Sampling streams:
  [Two access patterns: sets and streams](../ray-rendering/sampling-and-anti-aliasing.md#two-access-patterns-sets-and-streams)

## Source anchors

<!-- source-anchors -->
- `include/core/animation/AnimationTrack.h`
- `include/core/animation/Timeline.h`
- `include/core/math/interpolation/Interpolation.h`
- `include/world/animation/AnimationTrack.h`
- `include/world/animation/Timeline.h`
- `include/world/objects/Scene.h`
- `src/modeler/MainWindow.cpp`
- `tools/rendercli/rendercli.cpp`
- `scenes/animation_frame_demo.json`
- `test/unit/core/animation/AnimationTrackTest.cpp`
- `test/unit/core/math/interpolation/InterpolationTest.cpp`
- `test/unit/world/animation/AnimationTrackTest.cpp`
- `test/unit/world/animation/TimelineTest.cpp`
- `test/unit/world/objects/SceneTest.cpp`
- `test/rendercli/FrameOptionTest.cmake`
<!-- /source-anchors -->
