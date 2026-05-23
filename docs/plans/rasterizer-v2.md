# Rasterizer v2 plan — May 2026

> **Scope:** active follow-up plan for the software rasterizer after the
> initial feature plan and focused refactor plan were completed and archived in
> `docs/plans/complete/rasterizer.md` and
> `docs/plans/complete/rasterizer-refactor.md`.
>
> **Status:** living document. Keep this focused on remaining implementation
> work, quality gaps, and design debt. Move completed sections to the changelog,
> roadmap, API docs, textbook, examples, widgets, and archived plans when the
> work lands.

---

## Current critique

The rasterizer now has a coherent pipeline and broad test coverage, but the
implementation has outgrown its original shape. The biggest risk is not one
specific bug; it is that every new pass keeps landing in the same translation
unit and becomes testable only through full-frame output.

### Source structure

- `src/engine/raster/Rasterizer.cpp` owns orchestration, scene traversal,
  tessellation, projection, clipping, culling, tile binning, depth/stencil
  policies, material evaluation, MSAA, shadow-map construction, cascade fitting,
  shadow filtering, and worker dispatch. The comments are useful, but the file
  is now the subsystem rather than an entry point.
- `include/engine/raster/Rasterizer.h` is doing public API, configuration state,
  shader-hook type definitions, and substantial educational documentation. The
  docs are valuable, but continued feature growth will make the public header
  harder to navigate and more expensive to churn.
- Many important implementation types live in an anonymous namespace. That keeps
  linkage private, but it also makes focused unit tests and reuse awkward.
- The current `VertexShader` hook runs after projection and clipping. That is
  useful as a tiny projected-vertex hook, but it is not a conventional graphics
  vertex shader stage. Future docs or API naming should make that distinction
  explicit before real transform-stage behavior is added.

### Optimization opportunities

- Tessellation and projection are repeated each frame, and shadow maps repeat
  scene traversal for each directional light cascade. A cached or retained
  raster mesh path would help normal rendering and shadow rendering.
- Shadow-map passes currently reuse too much of the color-rendering machinery.
  They should become true depth-only passes, avoid scratch color storage, and
  pre-bind shadow maps to prepared light data instead of searching by light
  pointer during fragment shading.
- The tiled path bins triangles by projected bounds into every overlapped tile.
  That works, but large triangles and dense tessellation can create enough
  duplicated work that tiled rendering still cannot be the default.
- MSAA is correct but allocation-heavy. The full-frame path renders one sample
  buffer per sample; the tiled path allocates tile-local sample buffers per task.
  Worker-local scratch reuse and optional centroid/per-fragment shading are the
  next obvious improvements.
- Texture-backed materials still build a synthetic `HitPoint` and `Rayd` per
  fragment. A raster-native material/texture evaluation path would reduce
  overhead once textured previews become common.

### Quality and feature gaps

- Depth equality is exact. That is a reasonable minimal fixed-function rule, but
  multi-pass effects may need an explicit depth precision policy.
- Built-in material shading is Lambertian-only. Phong/specular, alpha,
  transparency approximations, and richer texture inputs need explicit raster
  preview policy.
- The rasterizer has no public AOV/depth/normal/object-id outputs yet, which
  limits Modeler picking, debugging, TAA, and compositing.

---

## Active work

### 1. Split raster internals into focused implementation units

Status: completed. The shared raster vocabulary now lives in
`src/engine/raster/RasterPipelineTypes.h`, and the scene-walk / projection /
clipping / culling front end now lives in
`src/engine/raster/RasterTriangleEmitter.h`. Directional shadow-map cameras,
cascades, filtering, and visibility queries now live in
`src/engine/raster/RasterShadowMaps.h`. Raster material adaptation and the
built-in Lambertian evaluator now live in `src/engine/raster/RasterMaterial.h`
and `src/engine/raster/RasterMaterialEvaluator.h`. Pass-owned buffers,
depth/stencil state and policies, fragment policies, and tile/full-frame draw
helpers now live in `src/engine/raster/RasterPass.h`. MSAA sample patterns,
tile scratch storage, accumulation, and resolve helpers now live in
`src/engine/raster/RasterMSAA.h`. `Rasterizer.cpp` owns the high-level frame
flow, shadow-map depth-pass construction, and decisions about which pass helper
to run.

Extract internal files without changing public behavior:

- ✅ `RasterTriangleEmitter` — scene traversal, tessellation, projection,
  homogeneous clipping, culling, vertex hook adaptation, and face emission.
- ✅ `RasterShadowMaps` — directional shadow cameras, cascade fitting,
  stabilization, filtering, and visibility queries.
- ✅ `RasterMaterial` — fallback colors, matte/texture adaptation, and the
  built-in Lambertian evaluator.
- ✅ `RasterPass` — pass buffers, tile/full-buffer views, depth/stencil
  policies, fragment policies, and depth-only pass support.
- ✅ `RasterMSAA` — sample patterns, sample offsets, scratch storage,
  accumulation, and resolve.

Preserve the current fast path: ordinary `queueSize == 1`, `msaa == 1` renders
must continue streaming emitted triangles directly into full-frame buffers.

### 2. Fix deterministic triangle coverage

Status: completed. The edge-function rasterizer now uses a top-left fill rule
for samples exactly on triangle edges, and the focused tests cover:

- adjacent triangles covering a rectangle do not double-apply coverage along the
  shared edge;
- rasterizer stencil operations do not observe shared-edge double shading;
- coverage is stable between equivalent triangulations of the same quad;
- single-tile and tiled paths agree across a shared triangle edge.

### 3. Preserve subpixel screen coordinates

Status: completed. Projected screen coordinates now stay fractional in
`RasterVertex`, tile binning uses conservative fractional bounds, and
`PreparedRasterTriangle` converts vertex positions and sample offsets into
1/256-pixel fixed-point edge equations.

The focused tests cover:

- tiny camera moves should not make static edges jump by whole pixels earlier
  than necessary;
- MSAA sample offsets should use the same subpixel coordinate convention;
- tiled and full-frame output should remain equivalent.

### 4. Make clip-depth policy explicit

Status: completed. `Rasterizer` now carries explicit near and far clip-depth
state. The near plane defaults to 0.1 eye-relative depth, the far plane defaults
to infinity, finite far depths participate in homogeneous clipping, and clone /
render tests pin the behavior.

The textbook and API docs describe the unit convention: pinhole cameras measure
depth from the perspective eye, orthographic cameras use camera-space z, and
these clip depths are rasterizer visibility planes rather than raytracer ray
interval limits.

### 5. Add raster pass outputs for diagnostics and Modeler workflows

Status: completed. `Rasterizer::DiagnosticOutputBuffers` is a local
borrowed-buffer surface for direct raster renders. Same-size output buffers are
cleared at render start and populated from the fragment path after stencil and
depth tests decide the committed pass state. The surface is deliberately not
copied through `cloneForRender()`; GUI snapshot resources belong in the later
render-pass resource contract. MSAA diagnostics report the last passing subpixel
sample for each pixel, so exact per-pixel inspection uses 1x rendering.

- ✅ depth buffer export;
- ✅ normal buffer export;
- ✅ primitive/material/face ID buffers for picking and debug views;
- ✅ stencil value export for depth/stencil inspection tests.

### 6. Improve shadow-map implementation quality

Status: completed for the current directional-light shadow-map scope. Cascade
split diagnostics should stay in the textbook/widget surface for now, not in the
Modeler live preview. The live preview should stay focused on final-frame
inspection until Modeler has a broader debug-overlay or multi-view framework.
Point and spot-light shadow maps remain deferred; directional-light behavior now
has the expected controls, fitting, filtering, stabilization, and documented
border policy.

Carry forward the remaining shadow work and add the implementation gaps found in
review:

- ✅ decide whether the cascade split diagnostic belongs inside the Modeler live
  preview;
- ✅ make shadow passes true depth-only passes with no color scratch buffer;
- ✅ pre-bind shadow maps to prepared light data instead of doing per-fragment
  light-pointer lookup;
- ✅ add slope-scaled bias in addition to constant bias;
- ✅ tighten directional cascade fitting using light-space bounds rather than broad
  scene/cascade diagonals;
- ✅ revisit cascade split policy, including practical log/linear splits;
- ✅ document the current "outside the shadow map is lit" border behavior;
- ✅ defer point/spot-light shadows until directional-light behavior is solid.

### 7. Revisit tiled rendering defaults

Status: measured; keep `queueSize > 1` opt-in. The repeatable coverage is now
`benchmarks/RasterizerTilingBenchmark.cpp`, which compares single-tile and
queued-tile rendering for synthetic scenes whose projected triangles are small,
medium, and large. Run it with:

```sh
cmake --preset benchmark
cmake --build --preset benchmark --target benchmarks
./build/benchmark/benchmarks/benchmarks --benchmark_filter=bm_rasterizerTiling
```

The benchmark reports render time plus policy-input counters:

- framebuffer dimensions (`frame_px`);
- emitted post-clipping triangle count (`triangles`);
- average and maximum projected triangle bounding-box area
  (`avg_projected_bbox_px`, `max_projected_bbox_px`);
- duplicated tile-list work (`tile_refs`, `avg_tiles_per_triangle`);
- configured worker and scheduling inputs (`threads`, `queue_size`);
- MSAA sample count (`msaa_samples`).

These are the minimum scene/runtime signals a future default policy should use:
screen area, triangle count, projected coverage distribution, expected tile-bin
duplication, thread count, queue size, MSAA sample count, and eventually a cheap
fragment-cost class for material/shader work. The policy should also distinguish
"many small triangles" from "few large triangles"; both can have similar final
covered pixels but very different tile-list duplication and scheduling costs.

A short validation run on May 23, 2026 (`--benchmark_min_time=0.02s`, four
worker threads, 640x480) showed the intended signal spread:

- small projected triangles: 2,400 triangles, 64 px average projected bounds,
  `avg_tiles_per_triangle` 1.00 single-tile versus 1.12 tiled;
- medium projected triangles: 160 triangles, 960 px average projected bounds,
  `avg_tiles_per_triangle` 1.00 single-tile versus 1.50 tiled;
- large projected triangles: 2 triangles, 76,800 px average projected bounds,
  `avg_tiles_per_triangle` 1.00 single-tile versus 9.00 tiled;
- medium 4x MSAA: same scene counters as medium 1x, with `msaa_samples=4` to
  expose the multiplicative coverage/shading cost.

A quick release-build retry on May 22, 2026, with `rendercli --repeat 3` and
four worker threads confirmed the same split behavior as the earlier pass:

- `rasterizer_baseline_materials.json`, 640x480, LOD 3, 1x: single-tile
  21.874 ms median, tiled 15.873 ms median;
- `rasterizer_baseline_materials.json`, 640x480, LOD 3, 4x MSAA: single-tile
  69.933 ms median, tiled 28.440 ms median;
- `rasterizer_baseline_dense_sphere.json`, 640x480, LOD 8, 1x: single-tile
  2336.393 ms median, tiled 4100.182 ms median.

Queued tiles win on screen-heavy 1x and 4x scenes but still lose badly on dense
tessellation. Do not make tiled rendering the default until the policy can
predict this difference from scene/render statistics. The next retry should
decide whether the default policy needs:

- an automatic tiling heuristic based on resolution, triangle count, projected
  coverage, MSAA level, and expected fragment cost;
- coarser per-tile work;
- large-triangle handling so one triangle is not duplicated across many tile
  lists unnecessarily;
- tile-local depth/color storage with a final stitch for more paths;
- a future GPU path instead of continued CPU tiling work.

Keep `queueSize > 1` opt-in until this decision is backed by measurements.

### 8. Integrate frustum and spatial culling

Status: primitive-bound culling is partially complete; broader spatial-index
integration remains open.

Carry forward the original frustum/spatial culling item:

- wait for the broader `SpatialIndex` work;
- ✅ cull finite primitive bounds before tessellation when all AABB corners are
  outside the same homogeneous clip plane; skip this camera-pass early-out when
  a custom vertex shader can move projected positions;
- feed the rasterizer from a frustum-friendly scene view instead of walking
  every tessellated primitive each frame.

This should be high value for large scenes and for shadow cascades.

### 9. Expand material, texture, and blend support

Define explicit raster-preview behavior for:

- ✅ Phong/specular material terms use the same local highlight model as the
  raytracer, while still omitting recursive reflection/refraction;
- ✅ a reusable raster material preview scene and rendered API/textbook image
  exercise Matte coefficient differences plus broad and tight Phong highlights;
- ~~alpha test and material/texture-sourced alpha blending~~ ✅ **Done.** Raster fragments now carry transient alpha from transparent-material opacity and texture intensity through alpha test and source-alpha blend factors;
- ✅ color write masks and RGB blend state, including rendercli flags plus
  rendered API/textbook examples;
- ✅ direct UV texture lookup paths for exact `UVColorTexture` and UV-mapped
  `CheckerBoardTexture`, with arbitrary textures still falling back to virtual
  `Texture::evaluate(...)` on a synthesized raster hit context;
- mipmapping or other texture filtering once image textures are prominent;
- tangent-space normal mapping if/when normal maps enter the material system;
- material-sidedness-driven default culling.

Avoid silently pretending recursive raytracer features are supported. Reflective
and transparent materials need an explicit approximation or diagnostic fallback.

### 10. Continue post-process AA work

Carry forward the remaining post-process AA items:

- add SMAA if FXAA proves too blurry for fine preview detail;
- ~~define the TAA resource contract for history buffers, motion vectors,
  jitter, and display-buffer ownership~~ ✅ **Done.** This plan now names the
  resources and reset rules required before TAA accumulation can land;
- add TAA accumulation once the resource contract below is backed by frame
  resources, motion-vector production, and render-state lifetime in the
  engine-agnostic pass system;
- ✅ add a cheaper rasterizer MSAA per-fragment shading mode beside the current
  per-covered-sample shading. The new mode keeps coverage/depth/stencil
  per sample and caches the first passing shaded color per prepared
  triangle/pixel across sample passes; it is not a full centroid sampler.

#### Temporal anti-aliasing resource contract

TAA is a history-dependent post-process, not just another image filter. The
rasterizer must not accumulate until the frame owns or receives a complete set
of same-sized temporal resources:

- **Current resolved color**: the color image produced by the current raster
  pass after MSAA resolve and before TAA. This is the source for the temporal
  resolve; it can be transient.
- **History color**: the previous frame's post-TAA color in display-linear
  space, read-only during the current resolve. A separate **next history color**
  target receives the current resolved TAA result. Implementations may ping-pong
  these two buffers, but the contract is read/write separation for the frame.
- **Current depth**: the committed current-frame depth at the same resolution as
  color. It is used for disocclusion rejection, neighborhood clamps, and depth
  comparisons during reprojection. The existing diagnostic depth output proves
  the rasterizer can expose this value, but a future pass resource must preserve
  it as a first-class attachment rather than a debug side channel.
- **History depth**: the previous frame's committed depth, same resolution and
  depth convention as current depth. It is invalid whenever history color is
  invalid.
- **Motion vectors**: per-pixel screen-space displacement from the current pixel
  to the previous frame's pixel, in render-target pixel units or an explicitly
  documented normalized equivalent. The vector must include camera motion,
  object transform motion, and any previous/current projection jitter
  difference. Pixels with no reliable previous correspondence need a validity
  convention, either a mask or a motion-vector sentinel owned by the eventual
  pass graph.
- **Current and previous jitter**: the subpixel projection offsets applied to
  the current and previous frame. Jitter is part of frame state; it must be
  generated deterministically from the frame index and preserved with the
  evaluated camera/projection data used to build motion vectors.
- **History validity/reset state**: a per-frame reason that tells the TAA pass
  whether it may accumulate or must seed history from the current color.

Reset conditions are not optional. Accumulation must be disabled and history
must be reseeded when the render target size changes, TAA settings change,
history resources are missing or discarded, the camera cuts or jumps without a
continuous previous transform, the scene is loaded/replaced, the animation frame
is scrubbed non-sequentially, object topology or IDs are not stable enough to
produce motion vectors, or the color/depth convention changes. Ordinary
continuous camera/object animation should not reset; it should produce motion
vectors and preserve previous evaluated transforms.

The existing systems that need to expose or preserve this state are:

- `Rasterizer::AttachmentBuffers` / pass-owned buffers: current depth must move
  from diagnostic-only observation into a retained frame resource when a render
  graph exists.
- `RasterTriangleEmitter` and the material/fragment path: rasterized fragments
  need enough previous/current clip-space data to write motion vectors beside
  color and depth.
- `render::Camera` and projection setup: camera clones or evaluated frame
  snapshots must retain previous and current projection matrices plus jitter.
- `world::Scene::evaluatedAtFrame(...)` and Modeler frame scrubbing: animation
  evaluation must distinguish sequential playback from timeline jumps and scene
  replacement so history can be preserved or invalidated deliberately.
- `RenderWidget` / display-buffer ownership: GUI rendering currently renders
  isolated engine snapshots and discards finished jobs on edits. TAA history
  belongs to the presented frame sequence, not to borrowed diagnostic pointers
  copied through `cloneForRender()`.

The small validation scaffold in
`src/engine/raster/RasterTemporalResources.h` deliberately checks only the
implementation-neutral contract: required buffers are present, match the render
target size, jitter values are finite, and reset conditions block accumulation
without making the resources incomplete. It does not choose ownership,
ping-ponging, motion-vector encoding, or render-graph node shape.

### 11. Add 2D geometric viewport clipping as a teaching path

Status: completed. `core::clipTriangleToRect(...)` now implements the
screen-space Sutherland-Hodgman reference helper for already-projected
triangles, and `core::fanTriangulateRasterClipPolygon(...)` fan-splits the
resulting convex polygon. The clipping chapter presents this as the 2D
counterpart to the runtime homogeneous clipper.

This remains a teaching helper, not a performance optimization. The runtime
rasterizer keeps homogeneous clipping before perspective divide, followed by
the existing scissor/bounding-box raster path.

Future teaching work: add a 3D frustum-clipping widget that shows the render
camera's frustum as a wireframe volume, clips source geometry against that
volume, and lets an independent observer camera orbit the setup.

### 12. Prepare for multi-pass effects

Status: completed for the current rasterizer-local pass-state scope. A broader
render-pass graph remains separate roadmap work.

Before planar reflections, portals, decals, or selection overlays become real
features, the rasterizer needs more fixed-function state:

- ✅ scissor and viewport state, including rendercli flags, unit coverage, API
  docs, textbook notes, and rendered visual examples;
- ✅ blend state;
- ✅ color write masks;
- ✅ constant depth bias that applies outside shadow maps too, including
  rendercli flags, unit coverage, API docs, and textbook notes;
- ✅ explicit load/clear/store behavior for color, depth, and stencil buffers,
  including direct-render color load/store state, borrowed depth/stencil
  attachments, unit coverage, API docs, and textbook notes.

Portal material previews should use this path rather than the current
raytracer-only material shader: first mark the portal surface in stencil, then
render the scene from the portal-transformed camera into that stencil region,
applying the material's color filter at composite time. The existing
`scripts/docs/portal_material.rb` image stays raytracer-only until that pass
composition exists.

Do not grow this into a rasterizer-only multi-pass API. The eventual
render-pass graph should be engine-agnostic: one frame may raytrace a room,
rasterize an offscreen computer display, draw a wireframe diagnostic view, and
composite the pieces through shared color/depth/stencil/AOV resources. The
rasterizer-local state above is only the fixed-function vocabulary needed to
participate in that broader graph later.

---

## Archived work

The initial rasterizer feature plan is archived at
`docs/plans/complete/rasterizer.md`. The focused performance/refactor pass is
archived at `docs/plans/complete/rasterizer-refactor.md`.

Their remaining open items were migrated here:

- Modeler cascade split diagnostic decision;
- SMAA/TAA follow-up;
- tile-parallel default-policy retry;
- frustum/spatial culling integration.
