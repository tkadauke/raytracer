# Plan inventory

This directory contains active or parked plans. Plans that have met their
acceptance criteria live in [`complete/`](complete/).

> **Implementation audit:** 2026-11-23, through `7396e783`. Every plan's
> status, checklist, and the table below were re-checked against the code.
> The changes since the previous audit (`c370563a`, 2026-11-16) are
> behavior-preserving helper deduplication (GPU tracing scene/path helpers,
> glTF and G-code importers, mesh triangle helpers). They do not complete any
> open plan phase, so the "Still missing" columns are unchanged and no plan
> moved to `complete/` in this audit.

## Active plans

| Plan | Implemented | Still missing |
|---|---|---|
| [`cpu-culling-graph-pass.md`](cpu-culling-graph-pass.md) | Visibility resources, frustum culling, front-to-back ordering, conservative backface filtering, tile diagnostics, and artifact caching | Tile occlusion rejection, importer/source-provenance reliability, complete invalidation rules, and auto-insertion thresholds |
| [`framework-critique.md`](framework-critique.md) | Doc-render sizing/staleness checks, modern widget primitives and tests, ES6 migration, and dimensioned sample streams | Central metatype registration, generated Ruby scene DSL metadata, camera parameter-widget retirement, scoped widget CSS, and a concrete consumer before adding 3D widgets |
| [`gpu-wavefront-intersection.md`](gpu-wavefront-intersection.md) | The v1 hybrid closest-hit/any-hit service, compiled scene records, CPU/platform kernels for the documented subset, fallback reporting, and parity/performance surfaces | Phase 8 breadth: additional primitives/features and the direct platform-performance work explicitly retained by the plan |
| [`opengl-gpu-residency.md`](opengl-gpu-residency.md) | Typed resident color resources and rasterizer-side color publish/load | Depth/stencil residency and load, graph-executor resident-handle binding, graph attachment-load support, and end-to-end multi-pass tests |
| [`point-vector-normal-types.md`](point-vector-normal-types.md) | Phase 1's `Point3`, `Direction3`, `Normal3`, and `NormalMatrix3` types | Renderer migration (Phases 2-5), the optional policy layer, 2D follow-up, and optional affine layer |
| [`render-graph.md`](render-graph.md) | Core graph compilation/execution, inspection and traces, raster shadows, AOVs, readback, subviews, selector routing, scheduling, and initial history support | Wider scene-feature expansion, selector-authored subviews, true graph GPU residency, automatic previous-frame capture/motion vectors, and remaining inspection/composition polish |
| [`tracing-execution-backends.md`](tracing-execution-backends.md) | E1-E14 and the E15 backend contract/user documentation; restricted Metal and Vulkan full-GPU loops are operational | E15 backend breadth, completed auto-selection semantics, cross-platform parity, and a documented passing platform speed gate |
| [`wavefront-and-path-tracing.md`](wavefront-and-path-tracing.md) | CPU wavefront/path-tracing Phases 1-6 | Phase 7+ SoA, packet, and layout optimization questions |
| [`whitted-ray-packets.md`](whitted-ray-packets.md) | Packet frontier intersections in depth-major Whitted and path-tracing batches | The direct camera/render loop, explicit shadow packets, and direct rendercli parity/performance gates |

## Parked plans

[`github_actions/`](github_actions/) holds workflows that cannot currently run
in the Syrus grading model. Its README records the replacement coverage and
the missing trigger/post-grade capabilities. These files are parked rather
than complete.

## Completion rule

Move a plan to `complete/` only when its own gate or all non-deferred phases
are satisfied. A partially implemented phase stays in this directory and its
entry above should name the remaining implementation, test, documentation, or
performance evidence.
