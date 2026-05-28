# CPU culling graph pass plan - May 2026

> **Scope:** add a CPU preprocessing pass that computes graph-visible
> visibility sets before raster execution, reducing unnecessary raster work
> without changing final image semantics.
>
> **Status:** planning. This plan is independent of the OpenGL GPU rasterizer.
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

- Add `VisibilitySet` or equivalent graph resource type.
- Add a pass payload/state type for CPU visibility preprocessing.
- Record input leaf/triangle counts, visible counts, rejected counts, and pass
  timing.
- Add trace metadata and Modeler property display for the selected culling
  node.
- Add rendercli graph export coverage showing the culling node and resource.
- Keep the pass disabled by default until it has at least frustum coverage.

Acceptance:

- A compiled graph can contain a visibility pass and resource.
- The graph view shows the pass and the raster pass dependency edge.
- A raster pass can ignore the resource and still render normally.

## Phase 1 - frustum culling

Tasks:

- Compute camera frustum planes from the active render camera where projection
  is supported.
- Test leaf bounding boxes against the frustum.
- Emit visible leaf ids and rejected-frustum counts.
- Teach CPU raster to consume the visibility set and skip rejected leaves.
- Add metrics comparing full-scene leaf count to submitted leaf count.

Acceptance:

- Offscreen bounded objects are skipped.
- Render output is identical for opaque test scenes.
- Rendercli functional tests cover an offscreen geometry fixture.
- Trace metadata shows how many leaves were rejected by frustum culling.

## Phase 2 - front-to-back ordering

Tasks:

- Sort visible opaque work roughly by camera depth after frustum culling.
- Preserve original order for order-dependent pass state.
- Surface ordering metrics in trace metadata.
- Feed ordered work into CPU raster and later GPU raster draw submission.

Acceptance:

- Output remains identical for opaque fixtures.
- Depth-test/color-write counters improve or remain neutral on representative
  dense scenes.
- The graph trace identifies when ordering was disabled by unsafe state.

## Phase 3 - conservative backface and sidedness filtering

Tasks:

- Preserve reliable sidedness/winding facts from importers and tessellation.
- Detect negative-determinant transforms that flip winding.
- Reject backfacing triangles or leaves only when material/pass state and
  metadata make it safe.
- Attach rejected-backface counts to the visibility resource.

Acceptance:

- Known one-sided opaque fixtures keep identical output and submit less work.
- Two-sided, double-sided, and unknown-winding assets remain visible.
- LDraw/glTF imported cases report whether sidedness was reliable.

## Phase 4 - tile-aware coarse occlusion

Tasks:

- Divide the image into coarse screen tiles.
- Project visible leaf bounds to tiles.
- Maintain conservative per-tile depth summaries for opaque depth-writing work.
- Reject later leaf/tile combinations that cannot pass depth.
- Keep partially visible work in the set if any tile remains uncertain.

Acceptance:

- Dense occluded scenes reduce coverage/depth-test counts, not only color
  writes.
- Alpha/blend/stencil paths do not use unsafe occlusion shortcuts.
- Trace metadata reports tile count, rejected tile references, and uncertain
  cases.

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
