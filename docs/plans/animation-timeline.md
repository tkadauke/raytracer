# Animation and timeline plan — May 2026

> **Scope:** Add scene timelines, keyframes, and `rendercli` animation output
> without turning `rendercli` into the animation system. This plan expands
> roadmap §4.7 and connects it to the existing velocity-only motion-blur model
> in chapter 16.
>
> **Status:** Phase 1 shared timeline math is implemented under
> `include/core/animation/`, with reusable interpolation policies under
> `include/core/math/interpolation/`. Phase 2 world-scene JSON loading,
> saving, and frame evaluation is implemented under `include/world/animation/`
> and `Scene`'s top-level `animation` block handling. `rendercli` animation
> flags are still pending. Update this file as API shape, JSON format, or
> implementation order changes.

---

## Goals

1. Let command-line users render one evaluated frame or an image sequence from
   a scene file.
2. Give the UI and JSON scene graph a real timeline/keyframe model.
3. Preserve a clean split between authoring concepts (`world`) and runtime
   rendering concepts (`render`).
4. Reuse one interpolation/time-sampling core rather than building separate
   animation systems per frontend or engine.
5. Provide enough animation quickly to test temporal rendering behavior, such
   as rasterizer shadow-map stabilization under camera motion.

Non-goals for the first pass:

- Video encoding as the primary output path.
- A full graph editor, dopesheet, nonlinear editor, or curve editor.
- Skeletal animation, blend shapes, physics, or constraints.
- Continuous motion-blur sampling from arbitrary keyframed properties.
- Editing animation in the GeneratedRayTracer UI beyond loading/saving and
  evaluating timelines.

---

## Architecture

The project ultimately needs both world-side and render-side animation, but
they should share one core model.

### 1. Shared timeline and interpolation core

Add a Qt-free core layer for pure timeline and interpolation math. Timeline
types live under `include/core/animation/`; interpolation policies live under
`include/core/math/interpolation/` so they can also serve rasterization,
sampling, UI widgets, and other non-animation callers later.

- `FrameTime` / `TimelineTime`
  - Exact frame number plus seconds conversion through fps.
  - Support integer frame evaluation and normalized shutter time later.
- `Timeline`
  - fps
  - start frame
  - end frame
  - optional shutter open / close in frame-relative units
- `Keyframe<T>`
  - frame or seconds timestamp
  - typed value
  - interpolation mode
- interpolation policies
  - `step`
  - `linear`
  - `smoothstep`
  - future: cubic Hermite, Bezier handles, quaternion slerp
- typed value helpers
  - scalar
  - `Vector2d`, `Vector3d`
  - `Colord`
  - rotation values once the project settles on Euler-vs-quaternion authoring

This layer must not know about:

- `world::Scene`
- `render::Scene`
- `QObject` / `Q_PROPERTY`
- JSON
- render engines

The test surface for this layer should be ordinary unit tests: interpolation
at exact keys, between keys, before/after keys, invalid fps/frame ranges, and
step-only value handling.

### 2. World-side authoring timeline

Add the first real timeline integration on the `world` side, because the
current scene file path already loads editable `world::` objects before
converting to runtime `render::` objects.

World-side tracks target authoring properties:

```text
element id + property path + keyframes
```

Examples:

- object `{id}.position`
- object `{id}.rotation`
- object `{id}.scale`
- camera `{id}.position`
- camera `{id}.target`
- light `{id}.direction`
- material `{id}.diffuseColor`
- future high-level/UI concepts such as rounded box corners, generated
  primitive parameters, and scripted-surface parameters

This layer can use Qt reflection deliberately:

- Validate property names with `QMetaObject`.
- Read/write `Q_PROPERTY` values through `QVariant`.
- Convert supported `QVariant` payloads to shared-core typed values.
- Reject unsupported property types with useful load/evaluation errors.

World evaluation answers:

```text
Given editable scene S and frame F, produce an editable scene S_F.
```

The simple implementation may deep-copy the world scene, apply tracks, and then
reuse existing `toRaytracerScene()` conversion. That is not the most efficient
long-term path, but it is the least invasive way to make `rendercli --frame`
and `rendercli --animation` work.

### 3. Render-side runtime timeline

Render-side animation should come later and stay independent of Qt/world.

Render-side tracks target runtime values that render engines can sample
efficiently:

- transform matrices
- camera pose
- light direction/color/intensity
- material parameters
- primitive deformation or topology only when the runtime representation can
  actually support it

Render-side evaluation answers:

```text
Given compiled render scene R and time t, provide runtime values for t.
```

This unlocks:

- true continuous time sampling inside a shutter interval
- keyframed motion blur beyond the existing `world::Surface::velocity`
- path-tracer time samples
- motion vectors for TAA/compositing
- frame-level distributed rendering without rebuilding full world scenes in
  each worker when tracks compile cleanly

The bridge from world to render should classify tracks:

- **Runtime-continuous** — can compile to render-side tracks and be sampled at
  arbitrary shutter times. Examples: transforms, camera pose, light direction,
  simple material scalar/color parameters.
- **Frame-baked** — needs world evaluation and scene rebuild per frame.
  Examples: rounded box radius if it changes tessellation, primitive
  parameters that change topology, scripted-surface parameters.
- **Step-only** — bools, enums, object references, and structural graph changes.

The first implementation should not try to compile render-side tracks. The
classification should still be documented early so the file format and APIs do
not imply every animated property is continuous or cheap.

---

## Scene file shape

Use a top-level `animation` block. Avoid baking animation into every object in
the first version.

Example:

```json
{
  "animation": {
    "fps": 24,
    "startFrame": 1,
    "endFrame": 120,
    "tracks": [
      {
        "target": "{object-id}",
        "property": "position",
        "interpolation": "linear",
        "keys": [
          { "frame": 1, "value": [0, 0, 0] },
          { "frame": 120, "value": [4, 0, 0] }
        ]
      }
    ]
  },
  "objects": []
}
```

Open questions to resolve during implementation:

- Whether key times are frame-only in v1 or may also use seconds.
- Whether per-key interpolation overrides track interpolation.
- Whether property paths should support nested names immediately
  (`material.diffuseColor`) or only direct element properties in v1.
- How to preserve stable element IDs when duplicating/editing scenes in the UI.

Recommended v1 choices:

- Frame-number key times only.
- Track-level interpolation only.
- Direct element property names only.
- Tracks target element IDs, not names.
- Loader errors are explicit for missing targets/properties instead of silently
  ignoring broken animation.

---

## `rendercli` behavior

Keep still-image behavior unchanged. Add animation flags that explicitly opt in
to timeline evaluation.

### Single evaluated frame

```sh
rendercli scene.json frame_0042.png --frame 42
```

Behavior:

- Load world scene.
- Evaluate top-level `animation` at frame 42.
- Convert evaluated world scene to runtime scene.
- Render one image with the selected engine and existing flags.

If the scene has no `animation` block, `--frame` should render the static scene
and still succeed. This makes command-line scripts simpler.

### Image sequence

```sh
rendercli scene.json frames/frame_%04d.png --animation
```

Behavior:

- Use scene `animation.startFrame/endFrame/fps` unless CLI overrides are given.
- Require an output filename with a printf-style integer placeholder.
- Render each frame independently.
- Print progress and frame timing.
- Keep existing `--timing` / `--repeat` semantics clear:
  - `--timing` can report per-frame timing plus summary.
  - `--repeat` should remain a still-frame benchmarking feature until there is
    a clear animation benchmark mode.

Useful overrides:

```sh
--frame_start 24
--frame_end 96
--fps 30
--animation
```

Do not make video encoding the first path. Image sequences are easier to test,
resume, diff, parallelize, and feed into external encoders.

### Optional video wrapper later

Later:

```sh
rendercli scene.json out.mp4 --animation --encode ffmpeg
```

This should be a thin wrapper around image-sequence rendering plus an external
encoder, not a hard dependency for animation support.

---

## Implementation order

### Phase 0 — Format and invariants

- ✅ Add this plan.
- Decide exact JSON schema for timeline blocks.
- Decide where stable IDs live in serialized world scenes if any ambiguity
  exists.
- Add golden JSON fixtures for static scene, one animated transform, broken
  target, and unsupported property type.

### Phase 1 — Shared core timeline math

- ✅ Add Qt-free timeline/keyframe types and reusable core math interpolation policies.
- ✅ Support `step`, `linear`, and `smoothstep`.
- ✅ Support scalar, `Vector3d`, and `Colord` values first.
- ✅ Document implemented behavior in API widgets and textbook chapter 27.
- ✅ Unit tests for:
  - exact key lookup
  - before-first and after-last behavior
  - between-key interpolation
  - single-key tracks
  - invalid frame ranges / fps
  - unsupported interpolation mode parsing

### Phase 2 — World timeline loading and evaluation

- ✅ Add `world::Timeline` / `world::AnimationTrack` wrappers that use the shared
  core math but target `world` element IDs and property names.
- ✅ Load/save top-level `animation` JSON.
- ✅ Evaluate an editable scene at a frame by applying sampled property values.
- ✅ Start with direct properties:
  - `position`
  - `rotation`
  - `scale`
  - camera `position`
  - camera `target`
  - directional light `direction`
  - material color where the property is already simple and stable
- ✅ Tests:
  - round-trip animation JSON
  - transforms evaluate at expected frames
  - missing target/property fails clearly
  - bool/enum/object-reference properties are rejected or step-only

### Phase 3 — `rendercli --frame`

- Add `--frame N`.
- Evaluate world scene before runtime conversion.
- Render one frame through existing engine flags.
- Add a fixture scene where a camera pan changes rasterizer shadow output.
- Functional tests:
  - static scene with `--frame` still renders
  - animated transform at frame 1 vs frame N produces different pixels
  - invalid frame argument fails with a clear CLI error

### Phase 4 — `rendercli --animation` image sequences

- Add `--animation`.
- Add output pattern validation.
- Add frame range / fps overrides.
- Render `startFrame..endFrame` inclusive.
- Print progress line per frame.
- Tests:
  - output sequence filenames are generated correctly
  - frame override narrows the range
  - missing `%d`-style placeholder fails
  - empty/invalid timeline range fails

### Phase 5 — GeneratedRayTracer read-only timeline awareness

- Load/save animation blocks without losing them.
- Add minimal UI awareness:
  - current frame spinbox or scrubber
  - preview evaluates scene at current frame
  - no full keyframe editor yet
- Ensure property editing still targets the base authoring scene, not a baked
  evaluated clone.

### Phase 6 — Render-side compilation

- Add render-side animation primitives for runtime-continuous tracks.
- Compile eligible world tracks to render tracks.
- Keep frame-baked tracks on the world-evaluation path.
- Integrate with shutter-time sampling:
  - frame time + sample's shutter offset -> continuous time
  - transforms/cameras/lights sampled per ray where needed
- Revisit the current `Surface::velocity` motion-blur model:
  - Preserve it as a convenience property or compile it as a two-key transform
    track.
  - Document how velocity and explicit transform keyframes interact.

---

## Testing strategy

Animation needs tests at multiple levels:

- **Core unit tests** for interpolation and time conversion.
- **World unit tests** for JSON load/save and property application.
- **Render functional tests** for visible behavior at selected frames.
- **CLI tests** for output naming, range handling, and error messages.
- **Visual fixtures** for cases where tests are too brittle but humans need a
  quick inspection path:
  - camera pan over cascaded shadow maps
  - moving object with raytraced motion blur
  - material color fade
  - rotating light direction

Avoid full video-output assertions. Prefer frame-specific PNG checks or simple
pixel-difference predicates.

---

## Documentation surfaces

When implementation begins, keep these in sync:

- `CHANGELOG.md` for behavior-affecting pieces.
- `docs/roadmap.md` §4.7 as milestones complete.
- `docs/markdown/03-scene-structure/16-instances-and-motion-blur.md` when
  velocity becomes part of a broader timeline model.
- `docs/markdown/06-tools-and-io/26-the-example-apps.md` when `rendercli` and
  GeneratedRayTracer gain animation controls.
- API docs on any new public timeline/keyframe classes.
- Rendered examples once there is a stable animated fixture; likely commit
  representative still frames rather than video files.

---

## Open questions

- Should rotations be authored as Euler angles for UI continuity, then compiled
  to quaternions for interpolation?
- Should interpolation be per-track only in v1, or should keys own outgoing
  interpolation modes from the start?
- How should timeline evaluation handle tracks targeting properties that affect
  object registration/factory type?
- Should `rendercli --animation` support resume/skip-existing in v1?
- What is the long-term file-format relationship between this native timeline,
  glTF animation import, and eventual USD import/export?
