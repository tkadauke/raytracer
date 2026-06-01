# Rasterizer performance plan - May 2026

> **Scope:** make the CPU rasterizer faster on dense imported scenes, especially
> LEGO/LDraw models, without reducing the value of the rasterizer as an
> inspectable teaching implementation.
>
> **Status:** complete. The diagnostic baseline, importer/culling correctness
> pass, screen-space LOD, front-to-back/coarse occlusion shortcuts, adaptive
> scheduling, and optional depth prepass have all landed with metrics,
> rendercli/Modeler surfaces, and benchmark notes.
>
> **Related plans:** `docs/plans/render-graph.md` owns graph-visible AOVs and
> trace inspection. `docs/plans/complete/rasterizer-v2.md` archives the
> completed rasterizer feature plan. This plan tracks the next performance wave.

---

## Problem statement

The raster counter AOVs show that complex LEGO models are expensive before they
reach the final framebuffer write. `raster_coverage_count` and
`raster_depth_test_count` are hot across much of the model, while
`raster_color_write_count` is less uniformly hot. That points at hidden,
overlapping, or over-tessellated triangles being covered and depth-tested many
times before visibility rejects them.

The first goal is to distinguish these cases quantitatively:

- many triangles projected to the same pixels because imported geometry is dense;
- back-facing triangles that should have been culled but are still rasterized;
- small curved features tessellated beyond their on-screen size;
- triangles processed in an order that prevents early depth rejection from
  helping higher-level culling;
- per-tile binning or task scheduling overhead dominating small views;
- real color overwrites from same-depth/near-depth geometry that depth testing
  does not reject.

## Guiding rules

- Start from diagnostics. Do not optimize blind.
- Keep render output identical unless a mode explicitly trades quality for
  speed.
- Preserve correctness for two-sided, alpha-tested, blended, and stencil paths.
- Keep user-facing choices in render intent or engine options. Do not require
  users to author graph nodes for performance behavior.
- Prefer changes that also improve teaching surfaces: counters, trace metadata,
  graph inspection, and before/after examples.
- Record before/after timings and counter deltas for each optimization commit.

## Phase 0 - baseline diagnostics

This is the required first slice.

### Per-render counters

Add a structured raster metrics object gathered during `Rasterizer::render()`
and attached to render-graph execution trace metadata when the rasterizer runs
inside the graph. At minimum it should record:

- input scene summary: leaf primitive count, mesh count, material count, light
  count, imported asset/source kind where available;
- tessellation summary: generated mesh vertices, generated mesh faces, prepared
  triangles before culling, triangles after culling, triangles after clipping;
- tile/binning summary: tile count, non-empty tile count, triangle references
  stored in tile bins, max/p95 triangle references per tile;
- fragment-loop totals: covered samples, stencil tests/fails, depth tests,
  depth passes, depth fails, shaded fragments, alpha-test fails, color writes;
- diagnostic image statistics: max and p50/p90/p95/p99 values for coverage,
  depth-test, depth-pass, shade, and color-write counters;
- timing summary: import/source rebuild time where available, tessellation and
  triangle emission, tile binning, raster loop, MSAA resolve, postprocess, and
  total render time.

The counters should be available even when diagnostic AOV images are not
requested. A render farm should not need to materialize full per-pixel AOVs just
to get aggregate performance telemetry.

### Surfacing

Expose the baseline in three places:

- **rendercli:** add an opt-in machine-readable report, for example
  `--raster_metrics_out file.json`, and a concise text summary when requested.
- **Render graph trace:** attach the metrics to raster pass trace metadata so
  selecting a raster pass in Modeler can show the aggregate counts in the
  property editor.
- **Modeler graph/preview UI:** show timing and key counters for the selected
  raster node. The full heatmap AOVs remain opt-in preview views.

### Baseline scenes

Measure at least:

- ~~`~/Downloads/10018-1.mpd` or a checked-in smaller LDraw fixture with similar
  dense curved geometry~~ ✅ **Done.** `benchmarks/raster_baseline_capture.sh
  dense_ldraw` synthesizes a project-owned dense curved MPD fixture for repeatable
  import benchmarking without redistributing third-party LDraw data.
- ~~`benchmarks/scenes/rasterizer_baseline_materials.json`~~ ✅ **Done.**
  Captured by `benchmarks/raster_baseline_capture.sh materials`.
- ~~`benchmarks/scenes/rasterizer_baseline_dense_sphere.json`~~ ✅ **Done.**
  Captured by `benchmarks/raster_baseline_capture.sh dense_sphere`.
- ~~`benchmarks/scenes/rasterizer_baseline_offscreen_floor.json`~~ ✅ **Done.**
  Captured by `benchmarks/raster_baseline_capture.sh offscreen_floor`.
- ~~one alpha/blend/stencil scene to prevent optimizations from silently breaking
  fixed-function state~~ ✅ **Done.**
  `benchmarks/scenes/rasterizer_baseline_alpha_blend_stencil.json` runs through
  source-alpha blending, alpha testing, and the stencil-composite graph view.

For each scene, capture:

- ~~1x and 4x MSAA where applicable~~ ✅ **Done.** The capture wrapper runs both.
- ~~default LOD and one lower/higher LOD~~ ✅ **Done.** The wrapper captures LOD 0
  and LOD 2; lower-than-default is documented as deferred because current raster
  LOD is non-negative and defaults to the minimum.
- ~~wall-clock render time~~ ✅ **Done.** `--repeat` writes `render_ms` summaries.
- ~~the metrics JSON~~ ✅ **Done.** Each variant writes `*.metrics.json`.
- ~~the five raster counter AOVs as optional visual references~~ ✅ **Done.**
  `benchmarks/raster_baseline_capture.sh --aovs ...` exports the counter previews.

### Acceptance criteria

Phase 0 is done when a developer can answer these questions from one render:

- How many triangles were emitted, culled, clipped, binned, and rasterized?
- How many covered samples were rejected by stencil/depth/alpha?
- How many fragments were shaded and how many wrote color?
- Which pass took the time?
- Are the hot pixels a few outliers or broad overdraw across the image?

## Phase 1 - importer and culling correctness

Backface culling is the likely first optimization after diagnostics.

Tasks:

- Preserve reliable imported sidedness/winding metadata through LDraw import
  and generated mesh construction.
- Distinguish reliable BFC geometry from unknown or corrected winding cases.
- Ensure front-sided opaque materials default to backface culling, two-sided
  materials remain two-sided, and explicit caller cull state wins.
- Add metrics for triangles rejected by culling and by winding/degeneracy.
- Compare LEGO before/after coverage and depth-test totals.

Risks:

- LDraw parts with bad or ambiguous BFC metadata may disappear if culled too
  aggressively.
- Mirrored transforms can invert winding; the importer or raster triangle
  emitter must account for that deliberately.

## Phase 2 - screen-space LOD for dense curved geometry

LEGO models contain many studs, cylinders, discs, and rounded primitives whose
full tessellation may be wasteful at preview scale.

Tasks:

- ~~Add projected-size metrics for tessellated primitives or imported part
  instances.~~ ✅ **Done.** Raster metrics include maximum projected primitive
  size plus screen-space LOD reductions for Epic #356 Phase 2.
- ~~Introduce render-intent quality presets for raster tessellation, with an
  advanced override for maximum screen-space error.~~ ✅ **Done.** Render
  intent, Modeler Render Settings, and rendercli expose preview/balanced/final
  quality plus `--raster_max_screen_space_error`.
- ~~Let primitive tessellators and source importers choose lower-detail meshes
  when the projected error stays below the requested quality.~~ ✅ **Done.**
  `RasterTriangleEmitter` lowers requested LOD only when projected error allows
  it, while `final` preserves the requested LOD by default.
- ~~Cache LOD variants for repeated source parts so large imports do not rebuild
  identical geometry repeatedly.~~ ✅ **Done.** Repeated primitive/LOD variants
  use a shared cache and metrics report cache hits/misses.
- ~~Keep high-quality final render settings available and visible in the
  compiled graph state.~~ ✅ **Done.** `RasterGeometryState` serializes quality
  and max screen-space error into graph JSON and applies them to CPU/OpenGL
  raster paths.

Acceptance:

- ~~Lower preview quality reduces prepared triangles and coverage/depth-test
  counts on LEGO scenes.~~ ✅ **Done.** Unit coverage pins LOD reductions and
  cache hits; closeout benchmark notes live in
  `docs/perf/raster-baseline-scenes-2026-05-31.md`.
- ~~The same scene can still request high-quality tessellation for final
  renders.~~ ✅ **Done.** Final quality leaves requested LOD intact unless an
  explicit error override is set.

## Phase 3 - ordering and coarse occlusion

Depth testing already prevents many color writes. To reduce coverage and depth
tests, the rasterizer needs earlier rejection.

Tasks:

- ~~Sort opaque work roughly front-to-back at object, mesh, or per-tile level.~~
  ✅ **Done.** Retained opaque, depth-writing raster batches are stably sorted
  by nearest prepared-triangle depth before tile binning for Epic #356 Phase 3.
- ~~Add tile-level hierarchical depth summaries after enough opaque geometry has
  rendered.~~ ✅ **Done.** Eligible tile loops rebuild a conservative maximum
  stored-depth summary after the warm-up threshold and at regular intervals.
- ~~Reject whole triangle/tile combinations when conservative depth bounds prove
  the triangle cannot pass.~~ ✅ **Done.** The CPU raster tile loop skips
  triangle/tile pairs whose nearest incoming depth is still behind the tile's
  conservative stored-depth bound.
- ~~Keep alpha-tested, blended, stencil-writing, and two-sided passes out of
  unsafe occlusion shortcuts unless the state is explicitly supported.~~ ✅
  **Done.** The shortcut is disabled for alpha test, blending, stencil, non-Less
  depth, disabled depth writes, and two-sided triangles.

Acceptance:

- ~~Coverage/depth-test totals fall on occluded dense scenes, not just shaded
  fragment or color-write totals.~~ ✅ **Done.** Unit coverage compares the
  shortcut with an alpha-test fallback and `rendercli --raster_metrics_summary`
  now reports coverage/depth-test deltas.
- ~~Rendering remains identical for opaque test scenes.~~ ✅ **Done.** Raster
  unit coverage compares optimized and fallback color buffers and the full
  `Rasterizer.*` suite covers tiled/MSAA/fixed-function parity.

## Phase 4 - binning and scheduling follow-up

Once triangle counts and occlusion are better understood, revisit CPU work
distribution.

Tasks:

- ~~Use metrics to identify tile-list duplication and pathological tile
  loads.~~ ✅ **Done.** Scheduling metrics record evaluated queue sizes,
  resolved queue, tile-reference duplication, and decision reason.
- ~~Consider adaptive tile sizes or coarse bins for dense imported scenes.~~ ✅
  **Done.** The automatic policy retries coarser queue sizes before falling
  back to single-tile rendering.
- ~~Keep the existing automatic queue-size policy, but feed it the new metrics
  instead of only pre-render estimates.~~ ✅ **Done.** Queue decisions now use
  measured tile-bin behavior and stay visible in rendercli summaries and graph
  trace metadata.
- ~~Add a benchmark case for dense imported geometry once a suitable fixture is
  checked in or synthesized.~~ ✅ **Done.** `RasterizerTilingBenchmark` and
  `benchmarks/raster_baseline_capture.sh dense_ldraw` cover synthesized dense
  imported geometry.

## Phase 5 - optional depth prepass

A depth prepass can help expensive shading and some occlusion schemes, but it
can also double coverage work. Treat it as a later opt-in or heuristic feature.

Tasks:

- ~~Enable only for opaque raster passes with expensive shading or when
  hierarchical rejection can consume the prepass result.~~ ✅ **Done.** The
  prepass is gated to supported opaque 1x CPU raster state and `auto`
  suppresses cheap/unsupported passes.
- ~~Measure whether depth prepass reduces total time, not only color writes.~~
  ✅ **Done.** Metrics report prepass, color-pass, and total measured seconds;
  rendercli summaries include the prepass decision and measured time.
- ~~Expose the decision in graph state and trace metadata.~~ ✅ **Done.**
  `RasterDepthPrepassState`, rendercli `--depth_prepass`, Modeler Render
  Settings, and graph trace metadata expose the requested mode and decision.

## Documentation and tests

Each shipped optimization should include:

- unit tests for the new metric or culling/LOD decision;
- rendercli functional coverage for the user-facing option or metrics output;
- Modeler widget/property updates when the metric or option is inspectable;
- textbook updates in `docs/markdown/` when behavior affects visible previews
  or render-graph inspection;
- a changelog entry for user-visible behavior;
- before/after timings and counter deltas in this plan or a linked benchmark
  note.

## Open questions

- Should raster metrics be always collected, or should the deepest counters be
  gated behind an opt-in flag to avoid overhead on normal preview renders?
- Do we need a smaller checked-in LDraw performance fixture, or should the plan
  rely on local large MPD files until licensing is resolved?
- What absolute heatmap thresholds should the Modeler expose for raster counter
  AOVs?
- Should graph trace metadata store all metrics for every frame, or should long
  animations default to summary-only output?
