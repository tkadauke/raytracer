# CPU culling graph pass plan - May 2026

> **Scope:** add a CPU preprocessing pass that computes graph-visible
> visibility sets before raster execution, reducing unnecessary raster work
> without changing final image semantics.
>
> **Status:** Phase 0 baseline started. This plan is independent of the OpenGL GPU rasterizer.
> The visibility resource should be consumable by CPU raster first and later by
> GPU raster, but neither backend should depend on the other.
>
> **Related plans:** `docs/plans/rasterizer-performance.md` tracks raster
> performance diagnostics and optimization phases. `docs/plans/render-graph.md`
> owns pass/resource inspection. `docs/plans/opengl-gpu-rasterizer.md` tracks a
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

- Preserve reliable sidedness/winding facts from importers and tessellation.
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

- Known one-sided opaque fixtures keep identical output and submit less work.
- Two-sided, double-sided, and unknown-winding assets remain visible.
- LDraw/glTF imported cases report whether sidedness was reliable.

## Phase 4 - tile-aware coarse occlusion

Tasks:

- ~~Divide the image into coarse screen tiles.~~ ✅ **Done.** Visibility-set
  resources now carry the render target tile grid using 32x32 pixel coarse
  tiles.
- ~~Project visible leaf bounds to tiles for diagnostics.~~ ✅ **Done.** The
  visibility pass records tile references for visible leaves whose transformed
  bounds project wholly inside clip space, and marks clipped or unknown bounds
  as uncertain instead of using them for later occlusion decisions.
- Maintain conservative per-tile depth summaries for opaque depth-writing work.
- Reject later leaf/tile combinations that cannot pass depth.
- Keep partially visible work in the set if any tile remains uncertain.

Acceptance:

- Dense occluded scenes reduce coverage/depth-test counts, not only color
  writes.
- Alpha/blend/stencil paths do not use unsafe occlusion shortcuts.
- ~~Trace metadata reports tile count and uncertain cases.~~ ✅ **Done.**
  Current traces report tile grid dimensions, covered tiles, visible tile
  references, and uncertain visible leaves. Rejected tile references remain
  TODO until occlusion rejection exists.

## Phase 5 - caching and invalidation

Tasks:

- Cache stable scene acceleration structures such as leaf bounds, mesh bounds,
  material cullability, and source/import provenance.
- Invalidate scene-side cache entries when geometry, transform, material,
  visibility, animation frame, or import-generated output changes.
- Keep camera-dependent culling results per camera/frame/view pass, not global.
- Expose cache hit/miss metadata in the culling pass trace.

Acceptance:

- Moving only the camera reuses scene acceleration data but recomputes the
  camera visibility set.
- Moving geometry invalidates the affected bounds.
- Changing material sidedness/cull state invalidates cullability facts.

## Modeler and rendercli surfaces

Modeler:

- show the culling pass as a graph node;
- selecting it shows input/output counts and rejection reasons in the property
  editor;
- optionally show a visibility debug preview for rejected/visible groups;
- keep the graph stable while the user is actively moving the camera, using the
  same freeze/heuristic rules as other graph execution state.

rendercli:

- add an opt-in spelling such as `--raster_culling on|off|auto`;
- export culling metadata in graph trace JSON;
- include visibility/culling resource information in graph JSON/DOT/text
  exports;
- support focused functional tests without image-diff fragility.

## Testing

Required coverage:

- unit tests for frustum plane extraction and bounding-box classification;
- unit tests for visibility resource serialization/export if graph JSON stores
  it;
- unit tests for conservative state gating;
- rendercli functional test for offscreen geometry culling;
- graph compiler tests proving intent synthesizes the culling pass rather than
  requiring direct node authoring;
- raster output parity tests for opaque scenes with culling enabled/disabled;
- regression tests for two-sided/blended/stencil cases staying uncullable until
  supported.

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
