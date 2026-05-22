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

- The edge-function coverage path needs a deliberate top-left fill rule so
  shared triangle edges do not double-shade in stencil, selection, or diagnostic
  passes.
- Projected vertices are rounded to integer pixel coordinates before
  rasterization. Keeping subpixel coordinates through edge setup should reduce
  shimmer under animation and camera motion.
- Depth equality is exact. That is a reasonable minimal fixed-function rule, but
  multi-pass effects may need an explicit depth precision policy.
- The near clip depth is hard-coded, and far-plane behavior is still undefined.
- Built-in material shading is Lambertian-only. Phong/specular, alpha,
  transparency approximations, and richer texture inputs need explicit raster
  preview policy.
- The rasterizer has no public AOV/depth/normal/object-id outputs yet, which
  limits Modeler picking, debugging, TAA, and compositing.

---

## Active work

### 1. Split raster internals into focused implementation units

Status: started. The shared raster vocabulary now lives in
`src/engine/raster/RasterPipelineTypes.h`, and the scene-walk / projection /
clipping / culling front end now lives in
`src/engine/raster/RasterTriangleEmitter.h`. Directional shadow-map cameras,
cascades, filtering, and visibility queries now live in
`src/engine/raster/RasterShadowMaps.h`. `Rasterizer.cpp` still owns pass policy
dispatch, MSAA orchestration, shadow-map depth-pass construction, and frame
setup. Raster material adaptation and the built-in Lambertian evaluator now live
in `src/engine/raster/RasterMaterial.h` and
`src/engine/raster/RasterMaterialEvaluator.h`.

Extract internal files without changing public behavior:

- ✅ `RasterTriangleEmitter` — scene traversal, tessellation, projection,
  homogeneous clipping, culling, vertex hook adaptation, and face emission.
- ✅ `RasterShadowMaps` — directional shadow cameras, cascade fitting,
  stabilization, filtering, and visibility queries.
- ✅ `RasterMaterial` — fallback colors, matte/texture adaptation, and the
  built-in Lambertian evaluator.
- `RasterPass` — pass buffers, tile/full-buffer views, depth/stencil policies,
  fragment policies, and depth-only pass support.
- `RasterMSAA` — sample patterns, sample offsets, scratch storage, accumulation,
  and resolve.

Preserve the current fast path: ordinary `queueSize == 1`, `msaa == 1` renders
must continue streaming emitted triangles directly into full-frame buffers.

### 2. Fix deterministic triangle coverage

Add focused tests for shared-edge rasterization:

- adjacent triangles covering a rectangle should not double-apply stencil
  operations along the shared edge;
- coverage should be stable between equivalent triangulations of the same quad;
- single-tile and tiled paths should agree.

Then implement a top-left fill convention in the edge-function rasterizer.

### 3. Preserve subpixel screen coordinates

Stop rounding projected vertices before edge setup. Keep screen coordinates in
fixed-point or floating form until `PreparedRasterTriangle` builds edge
equations.

Verify with animation-sensitive tests:

- tiny camera moves should not make static edges jump by whole pixels earlier
  than necessary;
- MSAA sample offsets should use the same subpixel coordinate convention;
- tiled and full-frame output should remain equivalent.

### 4. Make clip-depth policy explicit

Move near/far clipping out of hard-coded rasterizer constants:

- expose near-depth semantics consistently with existing camera projection;
- add deliberate far-plane behavior before enabling far-plane homogeneous
  clipping;
- document the difference between raytracer depth limits and rasterizer clip
  planes.

This carries forward the original far-plane follow-up from the roadmap and the
near-plane issue found during implementation review.

### 5. Add raster pass outputs for diagnostics and Modeler workflows

Introduce a small output-resource surface before larger render-graph work:

- depth buffer export;
- normal buffer export;
- object/material ID buffer for picking and debug views;
- optional stencil/depth inspection images for tests and documentation.

Keep this local to the rasterizer until the broader `RenderPass` contract exists.

### 6. Improve shadow-map implementation quality

Carry forward the remaining shadow work and add the implementation gaps found in
review:

- decide whether the cascade split diagnostic belongs inside the Modeler live
  preview;
- make shadow passes true depth-only passes with no color scratch buffer;
- pre-bind shadow maps to prepared light data instead of doing per-fragment
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
