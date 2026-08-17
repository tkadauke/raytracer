# CPU culling graph pass plan - May 2026

> **Scope:** add a CPU preprocessing pass that computes graph-visible
> visibility sets before raster execution, reducing unnecessary raster work
> without changing final image semantics.
>
> **Status:** partially complete. Phase 0 visibility resources, Phase 1
> frustum culling, Phase 2 front-to-back ordering, the conservative Phase 3
> backface path, Phase 4 tile diagnostics, and Phase 5 graph artifact caching
> have landed. Remaining work is explicit tile-occlusion rejection,
> importer/source-provenance reliability in the visibility cache, full
> invalidation rules, and auto-insertion thresholds. This plan is independent
> of the OpenGL GPU rasterizer. The visibility resource is consumable by CPU
> raster and OpenGL mesh preparation, but neither backend depends on the other.
>
> **Related plans:** `docs/plans/complete/rasterizer-performance.md` archives
> the raster performance diagnostics and optimization phases.
> `docs/plans/render-graph.md`
> owns pass/resource inspection. `docs/plans/complete/opengl-gpu-rasterizer.md` tracks a
> separate GPU executor.

---

## Goals

The rasterizer currently spends substantial work covering and depth-testing
triangles that never contribute color. OpenGL can depth-test quickly, but it
does not automatically skip hidden objects, avoid draw calls, or build scene
visibility structures. The render graph can model that work explicitly as a
preprocessing pass.

The CPU culling pass should:

- run before raster beauty/AOV passes when requested or chosen by heuristic;
- consume scene, camera, pass state, and optional frame/step visibility;
- write a graph-visible visibility resource;
- let raster executors consume that resource instead of traversing all leaves;
- expose culling counts and decisions in graph trace metadata;
- preserve exact output for supported opaque cases;
- conservatively fall back to uncullable work when state is not safe.

## Non-goals

- Do not make culling a hidden side effect inside the rasterizer only.
- Do not require users to author culling nodes directly. Render intent and
  compiler heuristics should synthesize them.
- Do not cull alpha-tested, blended, stencil-sensitive, two-sided, or
  side-effecting draw work until the correctness rules are explicit.
- Do not depend on the OpenGL backend.
- Do not attempt full production-grade occlusion culling in the first slice.

## Graph shape

The intended compiled plan shape is:

```text
scene + camera + raster pass state
  -> visibility/culling pass
  -> visibility_set resource
  -> raster beauty / AOV / shadow-compatible passes
```

The visibility resource should be inspectable. A first version can be simple:

```text
VisibilitySet
  visible leaf ids
  rejected leaf ids with reason counts
  optional per-leaf screen bounds
  optional per-tile references
  metadata: input counts, output counts, timings
```

The raster pass should declare whether it reads a visibility set. If absent,
it must preserve current behavior and render the full scene.

## Correctness rules

The culling pass must be conservative:

- Frustum culling is safe for bounded opaque leaves.
- Backface culling is safe only when winding/sidedness metadata is reliable and
  pass state allows it.
- Occlusion culling is safe only for opaque depth-writing geometry that cannot
  later become visible through alpha, blending, stencil, or order-dependent
  effects.
- Mirrored transforms can invert winding and must be accounted for before
  backface decisions.
- Unsupported state should mark work as visible/unculled, not dropped.

Each rejected primitive/leaf should have a reason category so diagnostics can
explain whether work was removed by frustum, backface, projected-size, tile
occlusion, or another rule.

## Phase 0 - visibility resource and metrics baseline

Tasks:

- ~~Add `VisibilitySet` or equivalent graph resource type.~~ ✅ **Done.**
  Added `visibility_set` as a descriptor-only graph resource for the raster
  culling baseline.
- ~~Add a pass payload/state type for CPU visibility preprocessing.~~ ✅
  **Done.** Added the `visibility` pass kind and a baseline raster visibility
  payload that emits an all-visible set. The pass carries typed raster
  geometry state so exported/replayed graphs keep the LOD used for visibility
  diagnostics.
- ~~Record input leaf/triangle counts, visible counts, rejected counts, and pass
  timing.~~ ✅ **Done.** The baseline payload records all-visible
  leaf/triangle counts and rejected-zero counts in the pass trace; pass timing
  comes from the existing graph trace timer.
- ~~Add trace metadata and Modeler property display for the selected culling
  node.~~ ✅ **Done.** The graph node and selected-pass properties show the
  visibility pass trace message through the shared graph inspector surfaces.
- ~~Add rendercli graph export coverage showing the culling node and resource.~~
  ✅ **Done.** `--raster_culling on|auto` now exports the synthesized
  `raster_visibility` node and `raster_visibility_set` resource.
- ~~Keep the pass disabled by default until it has at least frustum coverage.~~
  ✅ **Done.** The pass is opt-in through raster render intent and command-line
  overrides.

Acceptance:

- ~~A compiled graph can contain a visibility pass and resource.~~ ✅ **Done.**
- ~~The graph view shows the pass and the raster pass dependency edge.~~ ✅
  **Done.** The baseline uses ordinary resource edges, so the existing graph
  view displays it without special-case UI.
- ~~A raster pass can ignore the resource and still render normally.~~ ✅
  **Done.** The first payload records an all-visible baseline and current
  raster payloads keep drawing the full scene.

## Phase 1 - frustum culling

Tasks:

- ~~Compute camera frustum planes from the active render camera where projection
  is supported.~~ ✅ **Done.** The visibility payload uses the same
  homogeneous clip-space test shape as the raster front end and keeps leaves
  visible when the active camera cannot project bounds conservatively.
- ~~Test leaf bounding boxes against the frustum.~~ ✅ **Done.** Transformed
  leaf bounds are rejected only when all eight corners are outside the same
  clip plane.
- ~~Emit visible leaf ids and rejected-frustum counts.~~ ✅ **Done.** The first
  concrete set stores traversal-order leaf decisions plus rejected-frustum
  counts; stable authored leaf ids remain a future improvement for inspection
  and cache persistence.
- ~~Teach CPU raster to consume the visibility set and skip rejected leaves.~~
  ✅ **Done.** The software raster front end skips rejected leaf indices before
  tessellation and treats missing indices as visible.
- ~~Add metrics comparing full-scene leaf count to submitted leaf count.~~ ✅
  **Done.** Trace messages include input, visible, rejected, and
  frustum-rejected leaf/triangle counts.

Acceptance:

- ~~Offscreen bounded objects are skipped.~~ ✅ **Done.** Unit coverage pins
  that CPU raster visibility sets skip rejected leaves before tessellation.
- ~~Render output is identical for opaque test scenes.~~ ✅ **Done.** Unit
  coverage compares graph raster output with and without visibility culling for
  an opaque offscreen-leaf scene.
- ~~Rendercli functional tests cover an offscreen geometry fixture.~~ ✅
  **Done.** Functional coverage now renders a generated scene with one visible
  box and one offscreen box, then asserts the graph trace reports a
  frustum-rejected leaf and a CPU-consumable visibility set.
- ~~Trace metadata shows how many leaves were rejected by frustum culling.~~ ✅
  **Done.**

## Phase 2 - front-to-back ordering

Tasks:

- ~~Sort visible opaque work roughly by camera depth after frustum culling.~~
  ✅ **Done.** Visibility passes now sort visible bounded leaves by projected
  camera depth when the compiled raster state is order-independent.
- ~~Preserve original order for order-dependent pass state.~~ ✅ **Done.**
  Front-to-back ordering is disabled for currently graph-visible unsafe state
  such as blending, stencil tests, disabled depth writes, or nonstandard depth
  comparisons.
- ~~Surface ordering metrics in trace metadata.~~ ✅ **Done.** Trace messages
  report whether front-to-back ordering was enabled, disabled, or unsupported,
  plus the number of ordered leaves.
- ~~Feed ordered work into CPU raster and later GPU raster draw submission.~~
  ✅ **Done.** The visibility set can now carry an explicit visible-leaf order;
  the CPU raster front end consumes it while preserving original face-index
  progression, and OpenGL raster mesh preparation consumes the same set before
  GPU upload.

Acceptance:

- Output remains identical for opaque fixtures.
- ~~Depth-test/color-write counters improve or remain neutral on
  representative dense scenes.~~ ✅ **Done.** Diagnostic counter coverage now
  pins that a visibility set skips a duplicate rejected leaf before coverage,
  depth-test, shading, and color-write counters are incremented.
- ~~The graph trace identifies when ordering was disabled by unsafe state.~~ ✅
  **Done.** Ordering trace status reports enabled, disabled, unsupported, or
  not needed.

## Phase 3 - conservative backface and sidedness filtering

Tasks:

- Preserve reliable sidedness/winding facts from importers and tessellation. ✅
  **Partial.** The visibility pass now honors runtime material sidedness
  defaults when no explicit cull override is set, matching raster submission for
  front/back/two-sided materials. LDraw imports carry a `WindingReliability` fact
  (`include/core/geometry/MeshFaceMetadata.h`, set in
  `src/core/formats/ldraw/LDrawGeometryCompiler.cpp`) that
  `TriangleCullPolicy::shouldCull` (`src/engine/raster/RasterTriangleEmitter.cpp`)
  consumes. glTF imports do not: `GltfSceneImporter.cpp` adds faces with no
  winding-reliability metadata (defaulting to reliable) and only surfaces
  `doubleSided` as an unsupported-feature warning instead of mapping it to
  two-sided material sidedness.
- ~~Detect negative-determinant transforms that flip winding for pass-state
  backface filtering.~~ ✅ **Done.** The first filtering path projects
  transformed leaf triangles before applying the same screen-space cull policy
  as raster submission, so mirrored transforms affect the visibility decision
  the same way they affect the rasterizer. Importer-authored winding metadata
  still remains future work.
- ~~Reject backfacing triangles or leaves only when pass state makes it safe.~~
  ✅ **Done.** The visibility pass now rejects only whole leaves whose
  tessellated triangles are all inside clip space and all backfacing under an
  explicit one-sided raster cull mode. Clipped, two-sided, unknown, or mixed
  leaves stay visible.
- ~~Attach rejected-backface counts to the visibility resource.~~ ✅
  **Done.** Visibility-set traces now report backface rejected leaf/triangle
  counters for both the initial metrics-only path and the conservative
  filtering path.

Acceptance:

- ~~Known one-sided opaque fixtures keep identical output and submit less work.~~ ✅ **Done.** `RasterVisibilityCullingPreservesOpaqueOutput` confirms zero differing pixels with culling enabled/disabled; `RasterVisibilityUsesMaterialSidednessForBackfaceRejection` pins that front-sided material rejects backface leaves and emits fewer submitted triangles.
- ~~Two-sided, double-sided, and unknown-winding assets remain visible.~~ ✅ **Done.** `RasterVisibilityKeepsTwoSidedMaterialBackfacesVisible` pins zero backface-rejected leaves for two-sided and unknown-winding materials.
- LDraw/glTF imported cases report whether sidedness was reliable. ⏳
  **Partially done.** LDraw is covered end-to-end (`WindingReliability` set by
  `LDrawGeometryCompiler`, consumed by the visibility/raster cull policy, pinned
  in `LDrawGeometryCompilerTest.cpp` and `RasterizerTest.cpp`). glTF imports
  still report no sidedness-reliability signal at all.

## Phase 4 - tile-aware coarse occlusion

Tasks:

- ~~Divide the image into coarse screen tiles.~~ ✅ **Done.** Visibility-set
  resources now carry the render target tile grid using 32x32 pixel coarse
  tiles.
- ~~Project visible leaf bounds to tiles for diagnostics.~~ ✅ **Done.** The
  visibility pass records tile references for visible leaves whose transformed
  bounds project wholly inside clip space, and marks clipped or unknown bounds
  as uncertain instead of using them for later occlusion decisions.
- ~~Maintain conservative per-tile depth summaries for visible work.~~ ✅
  **Done.** Tile-covered visible leaves now contribute their nearest projected
  bounds depth to each referenced tile, and the visibility trace reports
  summarized tile and depth-reference counts. These summaries are suppressed
  for order-dependent state such as blending/stencil. They are still diagnostic
  and are not yet used to reject occluded work.
- Reject later leaf/tile combinations that cannot pass depth.
- ~~Keep partially visible work in the set if any tile remains uncertain.~~ ✅
  **Done.** Unit coverage pins that a partially clipped leaf remains visible,
  contributes no tile-depth summary, and reports an uncertain tile leaf.

Acceptance:

- Dense occluded scenes reduce coverage/depth-test counts, not only color
  writes.
- ~~Alpha/blend/stencil paths do not use unsafe occlusion shortcuts.~~ ✅
  **Done for the diagnostics baseline.** Order-dependent pass state keeps tile
  coverage visible but suppresses tile depth summaries so later occlusion
  rejection cannot treat those passes as depth-safe.
- ~~Trace metadata reports tile count and uncertain cases.~~ ✅ **Done.**
  Current traces report tile grid dimensions, covered tiles, visible tile
  references, depth-summary counts, and uncertain visible leaves. Rejected tile
  references remain TODO until occlusion rejection exists.

## Phase 5 - caching and invalidation

Tasks:

- Cache stable scene acceleration structures such as ~~leaf bounds~~,
  ~~mesh bounds~~, ~~material cullability~~, and source/import provenance. ✅
  **Partial.** Raster visibility preprocessing now shares per-primitive/lod mesh
  statistics plus transformed primitive bounds across graph render clones and
  invalidates those entries when primitive bounds or transforms change. It also
  shares material-sidedness-derived cullability facts and invalidates them when
  material sidedness changes; source/import provenance remains TODO.
- Invalidate scene-side cache entries when geometry, transform, material,
  visibility, animation frame, or import-generated output changes. ⏳
  **Partially done.** `RasterVisibilitySceneCache` fingerprints and invalidates
  on primitive bounds/transform changes and on material-sidedness changes.
  Animation-frame changes are only caught incidentally (through the transform/
  bounds fingerprint they move), not as a first-class invalidation trigger, and
  import-generated output regeneration is not yet tracked at all.
- ~~Keep camera-dependent culling results per camera/frame/view pass, not
  global.~~ ✅ **Done.** Raster visibility-set artifacts are cached with the
  pass state, target descriptor, camera fingerprint, and transformed scene
  geometry fingerprint, so display-only changes can reuse the culling result
  while camera moves produce separate entries.
- ~~Expose cache hit/miss metadata in the culling pass trace.~~ ✅ **Done.**
  Visibility pass trace messages and resource snapshots now report stored/hit
  cache status for the graph-owned visibility-set artifact.

Acceptance:

- ~~Moving only the camera reuses scene acceleration data but recomputes the
  camera visibility set.~~ ✅ **Done.** Pinned in `GraphRenderEngineTest.cpp`.
- ~~Moving geometry invalidates the affected bounds.~~ ✅ **Done.** Pinned in
  `RasterVisibilitySceneCacheTest.cpp`.
- ~~Changing material sidedness/cull state invalidates cullability facts.~~ ✅
  **Done.** Also pinned in `RasterVisibilitySceneCacheTest.cpp`.

## Modeler and rendercli surfaces

Modeler:

- ~~show the culling pass as a graph node;~~ ✅ **Done.** Phase 0 acceptance:
  the graph view shows the pass and the raster pass dependency edge via ordinary
  resource edges in the existing graph view.
- ~~selecting it shows input/output counts and rejection reasons in the property
  editor;~~ ✅ **Done.** Phase 0: trace metadata and Modeler property display
  for the selected culling node expose leaf/rejection counts through the shared
  graph inspector surfaces.
- ~~optionally show a visibility debug preview for rejected/visible groups;~~
  ✅ **Done.** Visibility-set trace snapshots now include a tile debug preview:
  summarized tiles render green, covered-but-depth-uncertain tiles render
  yellow, and uncovered tiles render black while the snapshot text keeps the
  leaf/rejection metrics.
- keep the graph stable while the user is actively moving the camera, using the
  same freeze/heuristic rules as other graph execution state.

rendercli:

- ~~add an opt-in spelling such as `--raster_culling on|off|auto`;~~ ✅
  **Done.** Confirmed implemented in `tools/rendercli/rendercli.cpp` with
  `off|on|auto` values.
- ~~export culling metadata in graph trace JSON;~~ ✅ **Done.** Visibility pass
  trace messages and visibility-set resource snapshots now expose leaf,
  rejection, tile, and tile-depth summary metrics in trace JSON.
- ~~include visibility/culling resource information in graph JSON/DOT/text
  exports;~~ ✅ **Done.** Resource descriptors now carry tooling feature
  annotations, the compiled visibility-set resource is tagged as visibility,
  culling, and rasterizer data, and text/DOT/JSON exports include those tags
  plus the resource shape.
- ~~support focused functional tests without image-diff fragility.~~ ✅
  **Done.** Phase 1 acceptance: rendercli functional tests cover an offscreen
  geometry fixture asserting graph trace reports without relying on image diffs.

## Testing

Required coverage:

- ~~unit tests for frustum plane extraction and bounding-box classification;~~
  ✅ **Done.** Phase 1 unit coverage pins that CPU raster visibility sets skip
  rejected leaves before tessellation (frustum classification per Phase 1
  acceptance).
- ~~unit tests for visibility resource serialization/export if graph JSON stores
  it;~~ ✅ **Done.** Phase 0 graph export coverage shows the culling node and
  resource in exported JSON/DOT/text; Phase 5 cache metadata tests cover
  serialized hit/miss status.
- ~~unit tests for conservative state gating;~~ ✅ **Done.** Phase 2 coverage
  pins depth-test/color-write counter improvement and ordering-disabled-by-unsafe-state
  trace; Phase 3 pins conservative backface gating and pass-state safety checks.
- ~~rendercli functional test for offscreen geometry culling;~~ ✅ **Done.**
  Phase 1 acceptance: rendercli functional tests cover a generated offscreen
  geometry fixture and assert graph trace reports a frustum-rejected leaf.
- ~~graph compiler tests proving intent synthesizes the culling pass rather than
  requiring direct node authoring;~~ ✅ **Done.**
  `RenderGraphCompilerTest.cpp` (`RasterVisibilityCullingOptionAddsVisibilityDependency`
  and neighboring cases) compiles a render intent and asserts the visibility
  pass/resource are synthesized; there is no direct/manual node-authoring API
  for this pass.
- ~~raster output parity tests for opaque scenes with culling enabled/disabled;~~
  ✅ **Done.** Phase 1 acceptance: unit coverage compares graph raster output
  with and without visibility culling for an opaque offscreen-leaf scene.
- ~~regression tests for two-sided/blended/stencil cases staying uncullable
  until supported.~~ ✅ **Done.** Phase 4 acceptance: alpha/blend/stencil paths
  suppressed from tile depth summaries; Phase 2 ordering disabled for unsafe
  state; Phase 3 two-sided/unknown-winding leaves stay visible.

## Open questions

- Should the first visibility resource identify leaves, primitives, triangles,
  or all three?
- Should culling be a separate pass by default, or only appear when render
  intent asks for diagnostics/performance mode?
- What is the right threshold for auto-inserting the culling pass?
- How should culling interact with graph trace snapshots when rejected resources
  are not materialized?
- Should visibility sets become reusable inputs for raytracer acceleration
  diagnostics later, or remain raster-only?
