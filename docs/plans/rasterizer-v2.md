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

Carry forward the remaining shadow work and add the implementation gaps found in
review:

- decide whether the cascade split diagnostic belongs inside the Modeler live
  preview;
- ✅ make shadow passes true depth-only passes with no color scratch buffer;
- ✅ pre-bind shadow maps to prepared light data instead of doing per-fragment
  light-pointer lookup;
- add slope-scaled and/or normal-offset bias in addition to constant bias;
- tighten directional cascade fitting using light-space bounds rather than broad
  scene/cascade diagonals;
- revisit cascade split policy, including practical log/linear splits;
- document the current "outside the shadow map is lit" border behavior;
- defer point/spot-light shadows until directional-light behavior is solid.

### 7. Revisit tiled rendering defaults

The completed tile-parallel retry showed that queued tiles now win on
screen-heavy 1x and 4x scenes but still lose badly on dense tessellation. The
next retry should decide whether the default policy needs:

- an automatic tiling heuristic based on resolution, triangle count, projected
  coverage, MSAA level, and expected fragment cost;
- coarser per-tile work;
- large-triangle handling so one triangle is not duplicated across many tile
  lists unnecessarily;
- tile-local depth/color storage with a final stitch for more paths;
- a future GPU path instead of continued CPU tiling work.

Keep `queueSize > 1` opt-in until this decision is backed by measurements.

### 8. Integrate frustum and spatial culling

Carry forward the original frustum/spatial culling item:

- wait for the broader `SpatialIndex` work;
- cull primitive bounds before tessellation where possible;
- feed the rasterizer from a frustum-friendly scene view instead of walking
  every tessellated primitive each frame.

This should be high value for large scenes and for shadow cascades.

### 9. Expand material, texture, and blend support

Define explicit raster-preview behavior for:

- Phong/specular material terms;
- alpha test and alpha blending;
- color write masks and blend state;
- direct UV texture lookup paths;
- mipmapping or other texture filtering once image textures are prominent;
- tangent-space normal mapping if/when normal maps enter the material system;
- material-sidedness-driven default culling.

Avoid silently pretending recursive raytracer features are supported. Reflective
and transparent materials need an explicit approximation or diagnostic fallback.

### 10. Continue post-process AA work

Carry forward the remaining post-process AA items:

- add SMAA if FXAA proves too blurry for fine preview detail;
- add TAA only after history buffers, motion vectors, and display-buffer
  ownership are explicit;
- decide whether rasterizer MSAA needs a cheaper centroid/per-fragment shading
  mode beside the current per-covered-sample shading.

### 11. Add 2D geometric viewport clipping as a teaching path

Carry forward the original 2D clipping item:

- add a screen-space Sutherland-Hodgman clipper against viewport edges;
- fan-triangulate the resulting 3-7 vertex polygon;
- present it as the educational counterpart to the current homogeneous clipper.

Do not treat this as a performance optimization. The current scissor/bounding
box path is the practical runtime path.

### 12. Prepare for multi-pass effects

Before planar reflections, portals, decals, or selection overlays become real
features, the rasterizer needs more fixed-function state:

- scissor and viewport state;
- blend state;
- color write masks;
- depth bias that applies outside shadow maps too;
- explicit load/clear/store behavior for color, depth, and stencil buffers.

This can stay rasterizer-local at first, but it should align with the future
render-pass graph rather than inventing incompatible state names.

---

## Archived work

The initial rasterizer feature plan is archived at
`docs/plans/complete/rasterizer.md`. The focused performance/refactor pass is
archived at `docs/plans/complete/rasterizer-refactor.md`.

Their remaining open items were migrated here:

- Modeler cascade split diagnostic decision;
- SMAA/TAA follow-up;
- tile-parallel default-policy retry;
- frustum/spatial culling integration;
- 2D geometric viewport clipping.
