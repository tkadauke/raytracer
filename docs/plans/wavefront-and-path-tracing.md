# Wavefront ray tracing — and the path to path tracing

> **Scope:** introduce a new `RenderEngine` implementation that
> processes rays *depth-major* — all pixels at depth 1, then all
> still-active pixels at depth 2, and so on — with an image-wide
> convergence check between depth passes. Foundation for per-pixel
> adaptive depth, denoising, stochastic path tracing, and (eventually)
> GPU offload. Captured 2026-05-10 from the conversation about
> "compute one recursion at a time, stop when nothing changes."
>
> **Status:** Living document. Updated 2026-05-31 after the render graph,
> render-intent, scalar path-tracing, packet-intersection, and OpenGL raster
> work landed. Phase 1, the throughput-cutoff prerequisite, is done, and Phase
> 2 now makes ray integrator selection graph-visible. A scalar
> `PathTracingIntegrator` also exists now. Phase 3 has started with the
> `WavefrontRaytracer` engine and graph executor surface. Depth-major
> path-tracing batches now report active-path and radiance-delta metrics,
> graph-visible convergence thresholds can stop path batches early, and
> Whitted batches now consume material-published continuation queues, and all
> built-in runtime materials now expose the wavefront material interfaces
> needed by Whitted and path-tracing batches. Phase 3's bare wavefront parity
> gate is covered by rendercli RMS checks for static
> sphere/CSG, transparent torus, and BVH-heavy sphere fixtures. rendercli now
> exposes convergence overrides as typed intent-derived graph state instead of
> hidden direct-engine settings. Depth-major path batches also publish
> per-depth sample-color snapshots so graph-backed Wavefront previews can show
> progress before the pass finishes, and denoiser-enabled wavefront batches can
> use filtered between-depth snapshots as convergence feedback.
>
> **Rule:** the wavefront engine is a **sibling** to the existing
> `Raytracer`, not a replacement. Both ship; the user chooses through render
> intent / render graph compilation. Reuses the shared substrate
> (intersection, materials, BVH, lights, camera, samplers, BSDF/light sampling)
> through the existing `render::` namespace — wavefront changes scheduling and
> per-path state ownership.

---

## The two ideas this plan resolves

Two related techniques, in order of architectural disruption:

1. **Throughput-based adaptive depth cutoff** — short-term, no new
   engine. Stop recursion when the running attenuation product drops
   below ε. Tracked as
   [issue #133](https://github.com/tkadauke/raytracer/issues/133).
   *Not the subject of this plan, but called out as the prerequisite.*

2. **Wavefront / depth-major scheduling** — medium-term, new sibling
   `RenderEngine`. Compute the whole image for depth 1, then depth 2
   for still-active pixels, etc., with an image-wide convergence test
   between passes. **This is the focus of this plan.**

The natural progression continues past wavefront:

3. **Stochastic path tracing.** The first scalar version has now
   landed as `render::PathTracingIntegrator`: it is an iterative
   megakernel `Integrator`, consumes `SampleStream`, uses
   `Material::sampleBsdf` / `evalBsdf`, samples lights for next-event
   estimation, and applies Russian roulette. That is deliberately not
   a wavefront scheduler. The wavefront work should reuse this
   substrate and re-host compatible path-tracing semantics in
   depth-major queues once the Whitted-parity scheduler is proven.

4. **Denoising.** Once you have a partial image after each depth pass
   (and per-pixel uncertainty signals from path tracing), drop in a
   denoiser between passes. Cheap variant: spatiotemporal filter; rich
   variant: OIDN-style learned denoiser.

5. **GPU offload.** Wavefront is the canonical GPU ray-tracing
   architecture (Laine, Karras & Aila 2013 introduced it for that
   reason). The repository now has an OpenGL raster backend and graph
   GPU/CPU resource-domain plumbing, but that is rasterization
   infrastructure, not GPU ray tracing. CPU wavefront comes first;
   GPU ray tracing remains a separate future plan.

Each builds on the previous. This plan covers (2) in detail, (3) and
(4) in sketch, and (5) as a future-work pointer.

---

## Same engine or separate? The architectural question

Thomas asked: "Do we need a separate render engine for that or can we
implement it as an alternative render method?"

**Answer: new sibling engine, not a rewrite of the existing
`Raytracer`.** The existing engine architecture still supports this
cleanly, but the integration point has changed since this plan was
first written: rendercli and Modeler now render through the graph by
default, and scenes carry editable render intent. A wavefront engine
therefore needs both a concrete `RenderEngine` implementation and a
graph executor surface (`RenderExecutorPreference`,
`RenderExecutorKind`, pass payload/state, rendercli/modeler intent UI).

### Why a sibling rather than an in-place rewrite

Four reasons:

1. **Scheduling and radiance policy are now separate concepts.**
   `Raytracer` owns camera/framebuffer/tile scheduling and delegates
   per-ray radiance to `render::Integrator` (`WhittedIntegrator` by
   default, `PathTracingIntegrator` when selected). Wavefront changes
   scheduling and per-path state ownership. It should not be crammed
   into `Raytracer::render()` as a mode.
2. **The recursive raytracer still owns single-ray probes.** APIs such
   as `primitiveForRay`, `rayState`, and `rayColor` are used by
   interactive picking and tests pinning shading behavior. Those
   probes should remain on `Raytracer`; wavefront should not inherit
   the `RayCaster` probe contract just to satisfy callers that want a
   single scalar ray.
3. **A/B benchmarking.** With both engines shipping, you can run the
   same scene through both and directly measure the difference rather
   than reasoning about it.
4. **Risk containment.** If wavefront has a subtle bug for some
   material/scene combo, users have a known-good fallback. Especially
   important during the transition into path tracing, where the
   semantics genuinely change (stochastic per-sample vs. deterministic
   per-ray).

### What gets shared

Everything except scheduling and queue/state ownership. Materials,
primitives, BVH traversal, intersection routines, lights, cameras,
samplers/sample streams, tonemapping, BSDF sampling, and the per-ray
`State` machinery all live in `render::` already and stay there.

The wavefront engine reuses them via the existing interfaces:

```cpp
namespace engine::wavefront {
  class WavefrontRaytracer : public render::RenderEngine {
    // Owns: per-pixel state arrays, the depth-major scheduler loop,
    //       convergence detection.
    // Delegates: intersection, shading, material evaluation to
    //            render::* shared code.
  };
}
```

`RayCaster` mixin (`include/engine/raytracer/Raytracer.h`) is
specific to the recursive compatibility path — wavefront doesn't
inherit it, and the single-ray probe API stays on `Raytracer` only.

### What gets refactored

Probably little in `Raytracer.cpp`. The relevant transport seams
already exist:

- `render::Integrator` is the scalar single-ray radiance policy seam.
- `PathTracingIntegrator` is an iterative scalar path tracer and a good
  reference for BSDF, direct-lighting, and Russian-roulette behavior.
- `Material::supportsBsdfSampling`, `evalBsdf`, `sampleBsdf`, and
  `bsdfPdf` are the material-side path-tracing hooks.
- `SampleStream` reserves named stochastic dimensions for pixel, lens,
  time, BSDF, light, and continuation samples.
- `RenderRaytracerOptions` / `RaytracerBeautyPassState` carry graph-visible
  ray-family execution, sampling, view-plane, integrator selection, and
  wavefront convergence-threshold state.

If wavefront grows enough to want SoA ray batches (Ray4/Ray8 per
Phase 4 of `complete/core-math-optimization.md`), the shared
intersection routines already have a usable packet substrate:
`Ray4`/`Ray8`, primitive packet entry points, `BoundingBox::intersects4`,
packet primitive kernels, and BVH packet traversal. The existing AoS
scalar path stays for `Raytracer`.

---

## Wavefront architecture

### Per-pixel state

Each tile owns a per-pixel state array sized to the tile's pixel count
(typically 64×64 or 128×128 — same tile-size knob as the recursive
engine):

```cpp
struct PixelState {
  Point<T>     origin;        // current ray origin (next bounce)
  Direction<T> direction;     // current ray direction
  Color        throughput;    // running attenuation product
  Color        accumulated;   // sum of direct contributions so far
  uint8_t      depth;         // bounces taken
  uint8_t      flags;         // active | hit_background | terminated
};
```

(Once the PVN type split lands, the geometric types here become the
typed Point/Direction. Until then, plain `Vector3<double>`.)

Memory: a `PixelState` is probably ~80 bytes; a 64×64 tile is ~320 KB,
comfortably in L2. The whole-image per-pixel state would be ~160 MB at
1080p — *would*, if we held it all at once, but we don't:
tile-based scheduling means only the active worker threads' tiles are
hot at any moment.

### The scheduler

```cpp
void WavefrontRaytracer::render(Buffer<Colord>& buffer) {
  initializePrimaryRays(state);            // depth 0: from camera through pixels

  for (uint8_t depth = 0; depth < maxDepth_; ++depth) {
    // Pass 1: intersection (parallel over active pixels)
    intersectActive(state, hits);

    // Pass 2: shade hits, write direct lighting into accumulated,
    //         compute next ray (origin + direction), update throughput.
    //         For Whitted: only one outgoing ray (pick reflect OR transmit
    //                       OR direct return; see Tree branching below).
    //         For path tracing: importance-sample one outgoing direction.
    shadeAndScatter(state, hits, depth);

    // Pass 3: deactivate pixels meeting any termination criterion
    //         - throughput below ε (biased cutoff)
    //         - Russian Roulette (unbiased, path-tracing variant)
    //         - hit background (no more rays)
    //         - depth == maxDepth
    deactivateTerminated(state);

    // Pass 4: convergence test — what's the image-wide delta since
    //         the previous pass? Stop if below threshold.
    if (converged(state, prevImage, depth)) break;

    swapImageBuffers(state, prevImage);
  }

  writeFinalImage(state, buffer);
}
```

### Parallelism: tile-based by default

Tile-based scheduling is **how the wavefront engine exploits multiple
CPU cores**, the same way the existing `Raytracer` does today. This
isn't only a memory-management concern (though it helps there too) —
it's the central reason CPU-side wavefront wants tiles:

1. **Multicore utilization.** Each tile is an independent unit of work
   that a thread can pull from the pool. Reuse the existing
   `QThreadPool` + tile dispatch in `Raytracer.cpp:Private`; only the
   *body* of the tile-worker differs (depth-major scheduler vs
   recursive `rayColor`).
2. **BVH cache coherence.** Rays in a spatial tile hit clustered BVH
   nodes; cache lines are shared across the rays a single thread
   processes. This is the standard reason classical raytracers tile.
   Wavefront benefits identically.
3. **Memory pressure.** Per-pixel state for a 64×64 tile fits in L2;
   per-pixel state for a 1920×1080 whole image is ~160 MB and trashes
   L3. Tiling keeps the hot data hot.
4. **Progressive display.** Existing GUI overlays "tiles complete"
   visualization. Wavefront with tiles reuses it for free.

The scheduler from the previous section runs *per tile* — the outer
loop is "for each tile, dispatched to a worker thread"; the inner
loop is the depth-major loop over that tile's pixels:

```
for each tile T (dispatched in parallel via QThreadPool):
  allocate state[T]  // per-pixel state for this tile's pixels
  initialize primary rays for T's pixels
  for depth in 0..maxDepth:
    intersect active rays in T
    shade and scatter
    deactivate terminated pixels in T
    if tile-local convergence reached for T: break
  write tile T into the buffer + report local convergence delta
```

Tile size is the same tuning knob `Raytracer` already exposes
(thread count, queue size, tile dimensions). Defaults should match
the recursive engine's defaults until profiling suggests otherwise.

### Why not other parallelism schemes

Two alternatives considered and not picked for v1:

- **Whole-image, parallelize-over-pixel-chunks.** Threads pull index
  ranges from a shared queue of active pixels. Better load balancing
  at deep passes (when the active set is sparse), but loses BVH cache
  coherence — a thread processing pixels (3,17), (88,201), (412,49)
  hits very different BVH nodes than one processing a contiguous
  tile. Profile in Phase 4+ if tile imbalance becomes the bottleneck;
  not worth the complexity in v1.
- **Hybrid work-stealing.** Threads start with their assigned tile,
  steal from neighbors when their tile empties. The natural endgame
  for v1 if profiling shows idle threads waiting on slow tiles. Defer
  to Phase 7+ unless data demands it sooner.

### Implication for convergence detection

With tile-based scheduling, "image-wide convergence" is naturally
expressed as **aggregated across tiles**: each tile reports its local
convergence delta (or active-pixel count) when its depth pass
completes; the engine sums them for the global stop condition.

This is also a feature, not just an accommodation: tiles that
converge early stop locally, freeing worker threads to keep working
on tiles that haven't converged. The active region of the image keeps
computing while the quiet region stops — better thread utilization
than "wait for the whole image to converge."

### Convergence detection

Several options, in order of strictness:

- **L2 / RMS over the whole image.** Cheap. `Δ = ‖I_k − I_{k−1}‖₂ /
  ‖I_k‖₂`. Stop when below threshold. Drowns out local features that
  change a lot in a small region.
- **Maximum absolute pixel delta.** Conservative — stops only when *no
  pixel* changes significantly. Good for guaranteed quality at the
  cost of more bounces.
- **Percentile (e.g. 99th).** Compromise.
- **Active-pixel count.** Bypass image comparison entirely: stop when
  fewer than k% of pixels are still active (their throughput dropped
  below cutoff). Cheapest and most robust signal for the "everything
  has converged" case.
- **Perceptual delta-E.** Most expensive, most aligned with what a
  human would notice. Probably overkill for v1.

**Recommendation for v1**: combine *active-pixel count drops below k%*
**and** *L2 over the active subset drops below ε*. The first is cheap
and catches the common case; the second guards against the long tail.

The convergence threshold and active-pixel threshold both belong in
`Constants.h` (per Phase 3.7 of `complete/core-math-optimization.md`).

---

## Tree branching: Whitted vs path tracing

In classical Whitted ray tracing, each hit spawns **both** a
reflection ray and a transmission ray. At depth k, the number of
active rays per pixel is up to 2ᵏ. Per-pixel state explodes:

```
depth 0: 1 ray per pixel
depth 1: up to 2 rays per pixel
depth 2: up to 4 rays per pixel
...
depth 10: up to 1024 rays per pixel
```

Wavefront scheduling with this tree shape needs a **per-pixel ray
queue**, not a single per-pixel state. Memory and bookkeeping go up
sharply.

Three resolutions:

### A. Flatten the tree: ray queue per pixel

Each pixel owns a small queue (`std::vector<RayState>` or a
fixed-capacity inline buffer). Each depth pass: drain the queue,
process all rays, write back up to 2× rays for the next pass.

- Pro: preserves Whitted semantics exactly.
- Con: memory grows exponentially in worst case; have to cap queue
  size and drop overflow (introducing implicit bias).

### B. Importance-sample one outgoing ray (path tracing)

At each hit, instead of recursing on both reflect and transmit, pick
ONE outgoing direction by importance sampling (probability
proportional to its contribution). Average many primary samples per
pixel to converge to the same expected radiance.

- Pro: per-pixel state stays at one ray. Wavefront becomes
  embarrassingly clean.
- Pro: this is path tracing — opens up indirect lighting that Whitted
  fundamentally can't do.
- Con: noise. Need many samples per pixel for low-variance images.
- Con: semantically different — pinning tests will need updating.

### C. Hybrid: split-on-deterministic, sample-on-stochastic

Reflect/transmit at a glass surface is deterministic and well-defined;
keep tree branching for those (cap depth to ≤4 surface crossings to
contain the explosion). For diffuse/glossy bounces (where Whitted
already cheats by sampling lights directly), importance-sample one
outgoing direction.

- Pro: smaller noise than pure path tracing for the kinds of scenes
  the current raytracer is best at.
- Con: more complex than either extreme. Two code paths to test.

**Updated recommendation**: use **A** only for the bare Whitted-parity
wavefront engine, because exact-ish parity is the cleanest scheduler proof.
For wavefront path tracing, start with **B** because the scalar
`PathTracingIntegrator` has already established the material/light/sampler
contract for single-continuation paths. Treat **C** as an optional quality
optimization for deterministic specular/transmissive chains after measurement,
not as the default first path-tracing design.

---

## Phased plan

Each phase is a separate PR. Convergence-test infrastructure ships
with the first phase that introduces it; subsequent phases reuse it.

### Phase 0 — design lock

Resolve open questions below. Tree-branching sequencing is now locked; the
remaining design decisions are convergence detection, first-phase memory layout,
and how much scalar path-tracing behavior to factor before wavefront owns the
queues. Commit decisions to this doc before starting the bare wavefront engine.

### ~~Phase 1 — throughput-based cutoff in the existing engine~~ ✅ **Done.**

Tracked as [#133](https://github.com/tkadauke/raytracer/issues/133).
`render::State::throughput` now propagates attenuation through reflective,
transparent, and portal recursion, and the active Whitted integrator returns
the scene background when throughput falls below `RAYTRACER_THROUGHPUT_CUTOFF`. This
validates the throughput-tracking arithmetic in a smaller, easier-to-debug
context before the wavefront engine exists.

Follow-up utility work is also in place: `render/PathTermination.h` now exposes
explicit Russian-roulette continuation probabilities and survival weights, with
unit coverage proving the expected throughput stays unchanged. ✅ **Done.**
Supports Epic #358.

### Phase 1.5 — scalar path-tracing substrate ✅ **Done.**

This was not in the original plan, but it landed before the wavefront engine:
`render::PathTracingIntegrator` is now a concrete `Integrator` sibling to
`WhittedIntegrator`. It is an iterative scalar megakernel path tracer with:

- deterministic per-primary `SampleStream` consumption,
- named BSDF/light/continuation sample dimensions,
- material BSDF hooks (`sampleBsdf`, `evalBsdf`, `bsdfPdf`),
- next-event estimation through `Light::sample`,
- Russian-roulette termination,
- Whitted fallback for materials that do not yet support BSDF sampling.

This does **not** replace wavefront. It narrows the future wavefront task: reuse
these sampling/material semantics and replace the scalar megakernel loop with a
depth-major scheduler once the scheduler is ready.

### ~~Phase 2 — render-intent and graph parity for ray integrators~~ ✅ **Done.**

Before adding another engine, remove the current direct/graph divergence:
rendercli can select `--integrator whitted|pathtracer` for the direct
raytracer path, but the render graph cannot express that selection.

Tasks:

- Add a validated integrator field to `RenderRaytracerOptions`.
- Persist it through `RenderIntent`, `RenderIntentElement` ("Render Settings"),
  JSON import/export, and subview override merging.
- Add the matching field to `RaytracerBeautyPassState` and make
  `RaytracerBeautyPassState::applyTo` install `WhittedIntegrator` or
  `PathTracingIntegrator`.
- Wire rendercli `--integrator` into the intent-derived path, not just the
  direct-engine path.
- Expose the setting in Modeler render settings with a dropdown, and hide
  path-tracer-specific controls until `Path Tracer` is selected.
- Add rendercli functional tests that `--render_graph_out` records the
  integrator and that graph rendering and direct rendering honor the same
  choice.

**Goal**: the graph can represent and replay raytracer integrator choice.
**Gate**: no direct-only integrator behavior remains except legacy
`--no_render_graph` debugging.

Render intent, `RenderRaytracerOptions`, `RaytracerBeautyPassState`,
rendercli graph compilation, Modeler Render Settings, and replayed graph JSON
now carry `whitted` / `pathtracer` integrator choice. ✅ **Done.**

### ~~Phase 3 — bare wavefront engine: same outputs as Raytracer~~ ✅ **Done.**

New `WavefrontRaytracer` sibling under `include/engine/wavefront/`.
Whitted semantics with tree-flattening (Option **A** above) so the
output matches `Raytracer` byte-for-byte (modulo floating-point
non-determinism from threading). Add it as a graph executor as part of
the same slice: `RenderExecutorPreference`, `RenderExecutorKind`,
executor definition, pass payload/state, rendercli selection, Modeler
render settings, and graph inspector display names. Convergence test
runs but doesn't yet drive any cutoff — instrumented to log
delta-per-pass to a benchmark report.

Initial work has landed the `WavefrontRaytracer` `RenderEngine`, graph executor
metadata, graph pass payload, rendercli selection, and Modeler render-settings
selection. Follow-up work moved camera primary-ray generation into an explicit
wavefront-owned tile loop and added a virtual integrator batch API.
`WhittedIntegrator::radianceBatch` now processes material-published
reflection/refraction continuations as explicit depth-major queues, falling
back to scalar compatibility only for materials that still require synchronous
`RayCaster` recursion. `PathTracingIntegrator::radianceBatch` now processes
compatible path-tracing samples depth-major across the tile. The engine also
records per-render metrics for
rendered pixels, primary samples, tile count, min/average/max non-empty tile
sample counts, queue decision, integrator name, batch execution mode, batch
sizes, active sample counts per depth,
per-depth radiance-delta L2/RMS/max values, configured convergence thresholds,
convergence stop decisions, and total render time; graph execution traces
attach those metrics to the `wavefront_beauty` pass for Modeler and rendercli
inspection. rendercli now carries a small graph-level parity check that renders
the same deterministic scene through recursive raytracer and wavefront
executors and requires matching image hashes, plus a glass-scene parity check
that exercises transparent Whitted continuation queues and a reflection-heavy
scene check that exercises recursive reflective continuation queues. The
rendercli image-probe helper can now compare two rendered images by normalized
RGB RMS delta, so future macro-scene parity gates do not have to rely on exact
PNG hashes when harmless quantization/threading differences appear. A generated
BVH-heavy sphere fixture now uses that RMS path as the first macro-scene parity
gate, and the transparent torus parity fixture now also records an RMS gate
before enforcing exact image parity. The deterministic static-scene parity
fixture records the same RMS gate for the simple sphere/CSG baseline.

**Goal**: prove the architecture without changing image output. ✅ **Done.**
rendercli now compares recursive raytracer and wavefront output for static
sphere/CSG, transparent torus, reflective, transparent-glass, and BVH-heavy
fixtures. The sphere/torus/BVH fixtures use normalized RGB RMS thresholds of
1e-3 and currently produce zero delta.

### Phase 4 — image-wide adaptive depth via convergence detection 🚧 **Started.**

Activate the convergence test as a stop condition. Active-pixel/sample count
+ L2 over active subset. Threshold tuning via the macro benchmark.
Graph-visible thresholds stop a tile when both the remaining active-sample
fraction and the current RMS radiance delta are below their configured limits.
The policy applies to depth-major path-tracing batches and explicit Whitted
continuation queues; scalar compatibility fallback calls still run to completion
because they are not queue-visible. Those thresholds can be authored in scene
intent, adjusted in Modeler Render Settings, or passed through rendercli with
`--wavefront_convergence`, `--wavefront_no_convergence`,
`--wavefront_convergence_active_fraction`, and
`--wavefront_convergence_rms_delta`; exported graph JSON carries the resolved
pass state. Modeler Render Settings now add Preview/Balanced/Final convergence
quality presets over those same thresholds, with raw active-fraction and
RMS-delta fields still available for advanced tuning. rendercli can now export
the same wavefront metrics as a dedicated JSON report or compact stdout summary
for direct and graph-backed wavefront renders, which gives the default-tuning
work a repeatable data source. The rendercli graph functional suite now has a
metrics-backed regression that drives intentionally loose path-tracing
thresholds and requires the report to show convergence-stopped tiles after the
first depth. Wavefront convergence metrics now also include a stopped-depth
histogram, and rendercli's compact metrics line reports earliest/latest stopped
depths so tuning captures can distinguish "most tiles stopped immediately" from
"only a few late-depth tiles stopped". Metrics also report total active
sample-depths processed, giving captures a direct work-saved counter beside
wall-clock timings.
`benchmarks/wavefront_convergence_capture.sh` now packages the Phase 4 macro
capture loop: it generates a BVH-heavy Whitted scene, compares
recursive and wavefront variants, captures the reflection-heavy Whitted scene
with real secondary-ray queues, captures wavefront metrics, and also compares
path-tracing convergence against the non-converged indirect-bounce scene. The
script accepts `WAVEFRONT_CONVERGENCE_QUEUE_SIZE` so convergence captures can
pin tile count while tuning thresholds and scheduler defaults, and it now
writes active sample-depth work comparisons for converged vs. non-converged
wavefront variants. `WAVEFRONT_CONVERGENCE_SWEEP` can run multiple
active-fraction/RMS threshold pairs against that same baseline, producing
threshold-named image comparisons and work-saved reports so Phase 4 policy
tuning does not require hand-editing the script between captures.
Remaining work is to run that capture across representative dimensions and tune
defaults from the measured timing/quality data. A first capture showed that a
single-depth diffuse Whitted scene is not a useful convergence-speedup proof:
there are no continuation queues to stop, and wavefront overhead is still
higher than recursive raytracing. The batch integrators now skip radiance-delta
diagnostic math unless metrics or convergence need it, but the Phase 4 gate
still requires either a better representative fixture or further scheduler
optimization. rendercli also disables progressive-display snapshots for final
image writes, so wavefront command-line renders no longer pay the per-depth
display-publishing cost that Modeler previews still need. Wavefront metrics
collection is now opt-in as well: graph-backed passes enable it when execution
trace or metrics output is requested, direct rendercli wavefront renders enable
it only for `--wavefront_metrics_out` / `--wavefront_metrics_summary`, and
plain final-image renders avoid the batch metric accumulation path. rendercli
now also resolves a size-aware default ray-family queue size and writes it into
compiled graph pass state, so graph-backed wavefront renders do not silently
fall back to a much coarser thread-count-sized queue than the direct engine
path. Whitted convergence accounting now tracks unique active sample indices
per depth instead of continuation-ray fanout, so branched reflection/refraction
trees no longer inflate the active fraction or radiance-delta RMS denominator.
A 320x240 `reflection_whitted` capture after that change showed recursive
Whitted and wavefront Whitted roughly tied on this MacBook
(`raytracer_whitted` median ~473 ms,
`wavefront_whitted_no_convergence` median ~479 ms,
`wavefront_whitted_convergence` median ~487 ms), with `last_active=22073`
unique active samples at depth 8 and only one convergence-stopped tile under
the current defaults. A 160x120, 16spp, max-depth-16
`pathtracer_bounce` capture with 300 tiles showed why stopped-tile count alone
is a weak speedup signal: convergence stopped 295/300 tiles and kept the image
delta small (`rms_delta=0.0019742863`), but active sample-depths only dropped
from 336446 to 336002 (~0.13%). Phase 4 still needs threshold/policy work that
cuts meaningful sample-depth work before convergence can be counted as a
speedup.

A follow-up 160x120, 16spp, max-depth-16 `pathtracer_bounce` threshold sweep
with 300 tiles showed the same tradeoff:

| active fraction | RMS threshold | sample-depths saved | image RMS delta |
| --- | --- | --- | --- |
| 0.05 | 0.002 | 444 / 336446 (0.13%) | 0.0019742863 |
| 0.25 | 0.005 | 2287 / 336446 (0.68%) | 0.0059664692 |
| 0.50 | 0.005 | 4018 / 336446 (1.19%) | 0.0073844098 |
| 1.00 | 0.002 | 8815 / 336446 (2.62%) | 0.0160156144 |
| 1.00 | 0.0005 | 8563 / 336446 (2.55%) | 0.0159527258 |

That makes convergence a useful graph-visible quality knob and diagnostic, but
not the main near-term speed path. The next speed work should focus on
scheduler/intersection cost (packet traversal, SoA state, or other Phase 7
implementation work) before Phase 4 can honestly claim its 30% speed gate.
The shipped opt-in Balanced convergence defaults now match the conservative
capture-script baseline from that sweep: active fraction `0.05` and RMS delta
`0.002`. Preview remains looser (`0.05` / `0.02`) and Final remains exact
(`0.0` / `0.0`). The capture script reads those shipped constants when its
threshold environment overrides are unset, so future default tuning only has
one source of truth. rendercli graph export also resolves
`--wavefront_convergence` without explicit numeric thresholds to those shipped
defaults, so exported graph JSON shows the concrete pass state the engine will
execute.
Wavefront metrics now report summed worker time for sample generation and
integrator batches as well, giving future captures a direct way to tell whether
the worker bottleneck is camera/sample setup or intersection/material transport
work. These worker-time counters are intentionally not wall-clock sub-spans and
can exceed total render time when tiles execute in parallel. The
sample-generation bucket is now split into sampler stream creation, camera
primary-ray sampling, sample enqueueing, and residual loop/bookkeeping overhead,
so captures can distinguish stream setup from camera math before optimizing the
tile setup path again. A small 64x48, 4spp, max-depth-4 `pathtracer_bounce`
capture reported ~15.6 ms sample-generation worker time with ~13.9 ms in
camera primary-ray sampling, ~0.3 ms in stream creation, ~0.4 ms in sample
enqueueing, and ~1.1 ms residual overhead. That points the next sample-setup
optimization at camera primary-ray generation rather than sampler allocation.
View planes now cache their camera-centered scaled pixel basis whenever setup or
pixel size changes, so primary-ray generation can reuse pre-scaled top-left,
right, and down vectors instead of rebuilding that transform for every sample.
A repeat of the same small capture after that cache reported ~13.5 ms
sample-generation worker time with ~11.7 ms in camera primary-ray sampling for
the non-converged run and identical output; keep treating these small captures
as directional, not as a Phase 4 speed-gate result. Camera `rayForPixel`
implementations now also use affine `Matrix4::transformPoint` and
`transformDirection` helpers for primary ray origins/directions, avoiding
per-ray homogeneous vector transforms and temporary 3x3 matrices in the
wavefront sample setup path. A small post-transform capture remained around
~13.0 ms sample-generation worker time with ~11.2 ms in primary-ray sampling,
so the remaining primary-ray cost is deeper camera math such as direction
normalization and per-sample pinhole setup rather than just transform wrappers.
`SampleStream::primarySample()` now gives cameras a single renderer-owned
pixel/time read, and sampler-backed streams override it so wavefront primary-ray
setup does not dispatch separately for pixel jitter and shutter time while
custom sampler streams keep their default sequential behavior. A small
post-change capture remained around ~13.4 ms sample-generation worker time with
~11.6 ms in primary-ray sampling, so this is API/dispatch cleanup rather than a
standalone macro-speed solution.
Wavefront tile rendering now asks each camera for a per-tile primary-ray
generator. The default generator preserves `Camera::primaryRaySample(...)`, and
`PinholeCamera` overrides it to precompute its constant ray origin once per
tile before emitting sampled rays. This keeps the common pinhole path
type-switch-free while reducing repeated setup inside the sample loop. A
96x64, 8spp, max-depth-8 `wavefront_indirect_bounce_demo` capture reported
primary-ray worker time dropping from ~14.4 ms to ~0.7 ms, with identical
submitted sample/frontier counts; remaining sample-generation overhead is now
mostly outside camera primary-ray math.
`ThinLensCamera` now uses the same hook to precompute its eye origin, camera
basis, and focal constants before the tile sample loop while preserving the
existing pixel/time/lens sample-stream order.
Wavefront denoiser feature prepasses also use the camera generator now, so
albedo/normal/depth collection does not fall back to the slower generic
primary-ray path when the beauty pass has a camera-specific generator.
The integrator batch bucket is now split further into
intersection and shading worker time, so captures can distinguish BVH/primitive
traversal cost from material, direct lighting, and continuation sampling cost
before the next speed optimization is chosen. Metrics also report the residual
integrator overhead after the
intersection and shading buckets are subtracted from total integrator worker
time, making scheduler, progress, convergence, and frontier bookkeeping cost
visible instead of implied by subtraction. Path-tracing batches now further
break that residual down into path setup, frontier bookkeeping, progress
snapshot publication, and convergence-test worker time, giving Phase 7 captures
a direct way to choose the next scheduler optimization. Whitted batches now
charge initial queued-ray/result/mark-buffer setup to the setup bucket and
report the same progress snapshot and convergence-test timing buckets when
those hooks are enabled, keeping scheduler diagnostics comparable across
wavefront integrators. The Whitted batch path now also avoids copying/scanning
the full tile result buffer for radiance-delta metrics: metrics/convergence
snapshots track only the unique active sample indices at each depth, and
per-depth continuation queues reserve capacity from the current frontier before
material continuations are appended.
Wavefront metrics now also publish an explicit integrator residual worker-time
bucket after subtracting intersection, shading, path setup, frontier
bookkeeping, progress snapshots, and convergence tests from the tile batch
time. rendercli compact summaries and the convergence capture work comparison
include the same value, so Phase 7 captures can see how much scheduler cost is
still unclassified before choosing the next optimization.
For high-sample path-tracing batches, sample-generation metrics showed retained
sample-stream setup was the larger worker-time bucket. Camera primary-sample
generation now has a caller-owned stream overload, and wavefront tiles retain
built-in sampler streams inside per-tile `SampleStreamStorage` instead of
allocating one stream object per sample. Custom sampler subclasses still flow
through the owning fallback path so their virtual `stream()` overrides remain
observable. The built-in sampler stream storage now reserves contiguous tile
storage for the known primary-sample count and spills any late or excess streams
to a deque fallback, preserving stable `SampleStream*` pointers while removing
deque block churn from the normal wavefront tile path. A follow-up capture with
the same 160x120, 16spp, max-depth-16 `pathtracer_bounce` shape still showed
no stable macro speed-gate win: non-converged sample-generation worker time
ranged from ~331 ms to ~428 ms, while the converged variant ranged from ~203 ms
to ~330 ms. Treat this as allocation-churn cleanup, not as completion of the
Phase 4 performance target. Path-tracing batches also now accumulate directly
into the sample-color buffer returned to the wavefront tile, so final result
assembly no longer copies every sample and progress snapshots can publish the
same live buffer instead of allocating a per-depth copy. The path-tracing
survivor queue now compacts in place as well, removing the second per-batch
`nextPaths` vector while preserving each path's pointer into the sample-color
buffer. This reduces retained queue memory and one move/push path in the hot
depth loop, but it is still scheduler cleanup rather than proof of the Phase 4
speed gate. Hit-frontier records also no longer carry before-depth radiance
colors; those colors are computed only at the miss/shading sites where
radiance-delta metrics or convergence can consume them. Whitted and
path-tracing frontier hit/miss, packet, and packet-fallback counters are now
metric-gated too, so plain wavefront renders do not maintain diagnostic
counters that only JSON/trace/summary output can consume. Packet fallback
reason snapshots are also allocated only while those metrics are active,
removing per-packet `std::map` construction from ordinary Whitted and
path-tracing wavefront renders. Whitted continuation queues now retain their
storage across depth passes as well, so explicit
reflection/refraction frontiers reuse the same two queue buffers instead of
constructing a fresh successor queue each pass. Whitted convergence/metrics
also now collect active samples from sparse frontier marks, avoiding a full
sample-tile scan when only a few branched continuations remain active. Disabled
`ScopedTimer` instances
also skip clock reads now, so ordinary renders do not pay timing overhead when
no metric bucket is requested. A follow-up 160x120, 16spp,
max-depth-16 `pathtracer_bounce`
capture with 300 tiles reported sample-generation worker time at ~198 ms
without convergence and ~156 ms with convergence, down from the earlier
~875 ms / ~710 ms retained-stream setup captures, while preserving the same
`rms_delta=0.0019742863` convergence image delta.
A later capture with the integrator phase split reported `pathtracer_bounce`
integrator worker time around ~633 ms, with ~97 ms in scene intersection and
~30 ms in material/shading for the non-converged variant. That means the next
speed slice should not assume BVH traversal alone dominates; path-state loop,
active-frontier, progress/convergence, and batch bookkeeping overhead remain
large enough to measure before packet traversal is introduced. The path tracer's
active-frontier intersection stage now lives in an integrator-owned instance
method with private batch-path/hit state, so a packet or SoA implementation can
replace that stage without mixing traversal policy into the shading loop.
Wavefront metrics now also publish per-depth frontier ray hit/miss arrays, and
rendercli's compact metrics summary prints total frontier hit/miss rays. That
adds a baseline diagnostic for the next scheduler/intersection slices: captures
can now distinguish "we spent time testing many rays that missed" from "the
frontier is still mostly shading-visible geometry" before changing traversal
policy. The convergence capture script now includes those same median
frontier-hit/frontier-miss totals in its work comparison files, so performance
captures retain the diagnostic alongside active sample-depth savings.
A 160x120, max-depth-5 `bvh_whitted` capture with metrics enabled showed the
current packet frontier on the BVH-heavy parity fixture comfortably clearing
the Phase 4 wall-clock speed gate at matching quality:
`raytracer_whitted` median ~10.34 ms,
`wavefront_whitted_no_convergence` median ~3.32 ms, and
`rms_delta=0.0` / `differing_pixels=0`. The converged variant reported
`wavefront_whitted_convergence` median ~3.64 ms with the same exact image and
no active-sample-depth savings, because this fixture terminates after the
primary depth (`active_sample_depths=19200` in both wavefront variants).
Treat this as evidence that packet-frontier wavefront scheduling is now fast
on BVH-heavy primary-ray scenes, not as evidence that the convergence policy is
complete; meaningful adaptive-depth savings still need multi-depth scenes.

**Goal**: render faster than `Raytracer` on common scenes without
visible quality loss.
**Gate**: ≥30% wall-clock improvement on the BVH-heavy scene at
matching quality (visual delta < ε).

### Phase 5 — wavefront path-tracing semantics ✅ **Done.**

Re-host the scalar `PathTracingIntegrator` behavior in the depth-major
scheduler. Do not call the scalar integrator wholesale for each ray; factor
shared material/light-sampling behavior into reusable methods or small objects
where needed so wavefront owns queues, active masks, and per-path state.

The first slice is in place: `Integrator::radianceBatch` lets the wavefront
executor submit a tile's primary samples as a batch, and
`PathTracingIntegrator` overrides that hook with a depth-major loop over active
path states. Wavefront pass trace metadata now exposes the selected integrator,
whether the batch ran through scalar fallback or depth-major path scheduling,
aggregate active-path counts per depth, and per-depth radiance-delta metrics.
The RMS-delta and active-sample-fraction thresholds are configurable through
intent and pass state and now drive early termination for depth-major path
batches. The batch settings also accept a progress observer; the path-tracing
batch calls it after each completed depth with the current sample colors, and
the Wavefront engine writes those snapshots into its current tile buffers so
Modeler can show progress during a graph-backed Wavefront pass. The batch
scheduler now compacts still-active path state between depths, so later depths
visit only live paths rather than scanning the full original sample set after
most samples have terminated, and it no longer carries a separate active-index
frontier beside the compact path queue. A 96x64, 8spp, max-depth-8
`wavefront_indirect_bounce_demo` capture with a pinned sampling seed matched
the pre-change image exactly (`rms_delta=0.0`, `differing_pixels=0`) while
preserving the same packet/frontier work counters. Wavefront tiles also reserve
their pixel and sample mapping buffers from the known tile dimensions before
camera sample generation, reducing batch setup churn. The path-tracing batch
loop now also separates each depth into an active-frontier intersection phase
followed by a hit-frontier shading phase. Image output stays the same, but the
scheduler shape now has an explicit insertion point for future packet traversal
or intersection batching. All built-in runtime materials
(`MatteMaterial`, `PhongMaterial`, `ReflectiveMaterial`, `TransparentMaterial`,
and `PortalMaterial`) now expose both wavefront Whitted continuations and
path-tracing BSDF sampling; compatibility fallback metrics remain in place for
future custom materials or new built-ins that have not implemented those
interfaces yet.
`scenes/wavefront_indirect_environment_demo.json` is the first reusable Phase 5
sanity scene: it has a wavefront/pathtracer render intent, black ambient, no
direct lights, and a matte object that is visible only because diffuse BSDF
sampling gathers environment radiance. rendercli now compares that scene
against a Whitted override and requires the images to differ.
`scenes/wavefront_indirect_bounce_demo.json` adds the stronger diffuse-bounce
gate: a side-lit red wall bounces light onto otherwise-dark neutral receivers,
and rendercli compares the graph-backed wavefront/pathtracer result against a
Whitted override to require the visible indirect contribution.
Batch metrics also count material compatibility shading fallbacks. Phong now
publishes a finite diffuse/glossy BSDF for path tracing, Reflective publishes
its mirror branch as a delta BSDF sample, Transparent publishes reflection,
transmission, and total-internal-reflection delta samples, and Portal publishes
its redirected ray as a delta continuation sample. Graph traces still publish
compatibility counters for custom or future materials that terminate at a legacy
Whitted-shaded surface instead of continuing through a sampled BSDF; rendercli
has graph-trace regression checks that require this metadata and pin transparent
glass scenes to zero compatibility material samples.

Start with pure single-continuation path tracing (Option **B**) unless a
measured scene proves deterministic specular split (Option **C**) is needed for
acceptable variance. The existing samples-per-pixel controls become
per-(pixel, sample) state in the wavefront scheduler.

**Goal**: indirect lighting on diffuse surfaces (the path-tracing
payoff).
**Gate**: a Cornell-box-style scene with indirect bounce produces
the visual difference path tracing is famous for. ✅ **Done.** The
`wavefront_indirect_bounce_demo.json` rendercli gate now requires a visible
diffuse-bounce difference from Whitted.

### Phase 6 — denoising hook between passes ✅ **Done.**

Add a `Denoiser` interface that runs between depth passes (or just at
the end). v1: simple spatiotemporal filter (a-trous or bilateral on
the per-pixel `accumulated` channel). v2: link to OIDN or similar
learned denoiser.

The first hook is now in place: `render::Denoiser` defines the cloneable
postprocess interface, `render::BoxDenoiser` provides a tiny deterministic
spatial filter for tests and early experimentation, and
`WavefrontRaytracer` can apply an explicitly installed denoiser to the HDR
buffer before the final display buffer is rewritten. The next slice connected
that hook to graph-visible ray-family pass state: render intent serializes
`denoise: {type, radius}`, rendercli exposes `--wavefront_denoiser` and
`--wavefront_denoise_radius`, and Modeler Render Settings exposes the same box
denoiser controls when the wavefront executor is selected. Wavefront metrics
now also include denoiser diagnostics: whether filtering ran, the denoiser name,
its published parameters, and denoise time. Better edge-aware/AOV-aware filters
are now started with `render::BilateralDenoiser`, a color-bilateral filter that
preserves strong color edges better than the original box filter and is exposed
through render intent, graph JSON, rendercli, and Modeler Render Settings.
`DenoiserFrame` now carries the beauty buffer plus optional albedo, normal, and
depth feature buffers, and the wavefront engine fills them from primary-hit
material albedo, surface normals, and ray depth when a denoiser is installed.
`BilateralDenoiser` now uses compatible albedo/normal/depth feature buffers as
additional edge-stopping weights, and wavefront metrics report which feature
buffers were supplied to the denoiser plus how long the feature prepass took
for graph trace inspection. Denoisers now explicitly request the feature
buffers they use, so featureless filters skip the feature-prepass cost instead
of always materializing albedo/normal/depth. The feature prepass now uses the
same tile scheduler and per-tile sampling seed derivation as the beauty pass,
keeping AOV samples aligned without making the prepass a serial bottleneck.
Depth-progress tile snapshots are now run through a cloned denoiser before the
preview buffers are published, so denoiser-enabled previews show filtered
progress instead of raw progress followed by a final filtered jump. The same
denoised tile snapshots can now feed the scheduler's convergence decision even
when progressive display is disabled: integrators ask their observer for an
optional convergence RMS override after each depth, and Wavefront supplies that
feedback from denoised progress pixels without feeding filtered color back into
light transport. The feature-buffer prepass is also graph-visible as its own
denoise `featurePrepass` metric block and reports active tiles through the
render engine while it is running, so large denoiser-enabled previews no longer
look idle before the first beauty tile. Completed-tile publication still only
copies beauty tiles; the prepass overlay does not publish blank feature-buffer
work into the display image. Metrics and compact rendercli summaries report how
many depths used observer convergence feedback.
`scenes/wavefront_denoise_demo.json` now provides a reusable low-sample,
graph-backed bilateral-denoising scene so scene-authored denoiser intent can be
tested in rendercli and inspected in Modeler. The rendercli graph functional
suite now uses that scene as a quality gate: a 4spp bilateral-denoised render
must stay within normalized RGB RMS 0.03 of a 64spp reference. The gate avoids
requiring every stochastic raw 4spp sample to be worse than the filtered image,
because individual low-sample draws can occasionally land closer to the
reference by chance.

**Goal**: low-sample renders look acceptable.
**Gate**: 4spp render with denoiser produces image visually comparable
to 64spp without denoiser. ✅ **Done.** Covered by the
`wavefront_denoise_demo.json` rendercli RMS regression.
The rendercli denoiser regression now also pins the stochastic seed for its
reference, raw, and filtered renders, so the quality gate measures the denoiser
instead of incidental sample-stream differences.

### Phase 7+ — SoA / GPU / packet traversal

Partially pre-landed. The SoA / Ray4 / Ray8 substrate from
`complete/core-math-optimization.md` Phase 4 is already available: packet ray
transport, primitive packet entry points, `BoundingBox::intersects4`, packet
primitive kernels, and BVH Ray4/Ray8 active-mask traversal. Once the wavefront
engine is stable and path-tracing semantics are locked in, this work plugs into
the scheduler as a performance optimization. GPU offload remains a major lift;
not committed.

The first handoff slice is now in place: primitives expose a four-wide
`intersectPacketHits(...)` API that returns the closest hit primitive and
`HitPoint` per lane, and composites merge child packet hits by closest positive
distance. The default implementation still falls back to scalar `intersect(...)`;
the value is the contract shape. The API carries per-lane `State` pointers, so
wavefront frontier traversal can ask the scene for packet-shaped hit
materialization without losing the data needed by material shading, hit/miss
bookkeeping, or trace events. BVH now preserves that materialized-hit contract
through packet tree traversal, and the path-tracing active-frontier stage
submits full four-ray chunks through `Scene::intersectPacketHits(...)` before
falling back to the scalar path for leftovers. Whitted batches now use the same
four-ray queued-frontier submission shape; packet miss lanes stay on the packet
path, while packet hit lanes are scalar-refined before material shading so the
strict recursive-Whitted parity gates remain stable at reflective edges. Later
primitive-specific overrides can make leaf packet materialization faster.
`Sphere`, `Plane`,
`Triangle`, `Box`, `Disk`, `Rectangle`, `OpenCylinder`, and `Torus` are the
first such leaf overrides: they materialize closest positive packet hit points
directly and update per-lane state without building scalar `HitPointInterval`
objects.
`Curve` also has explicit Ray4/Ray8 packet paths, but only to report misses
directly: runtime curves are currently raster/wireframe/overlay geometry and do
not ray-intersect.
Mesh-backed triangle leaves now participate too: `MeshTriangle` owns
cross-platform four-wide barycentric packet solving and materializes flat or
smooth hit normals through subclass hooks, so imported triangle meshes can stay
on the materialized packet frontier instead of falling back to scalar
`HitPointInterval` construction per lane.
That contract now survives common imported-geometry wrappers: `Instance`
transforms a static packet into local space and transforms materialized hits
back to world space, while `MeshPrimitive` forwards packet hit requests to its
triangle leaves and preserves mesh-level material fallback.
Those imported-geometry paths now preserve Ray8 packet hits too:
`MeshTriangle` materializes closest positive barycentric hits for each lane in
an eight-wide packet, `MeshPrimitive` merges Ray8 child hits by closest
distance, and static `Instance` transforms Ray8 child hits back to world space
without dropping to base scalar materialization.
Boolean and closed-solid CSG composites (`Union`, `Difference`,
`Intersection`, and `ClosedSolidUnion`) now expose packet interval composition:
children materialize lane-local `HitPointInterval`s, the CSG node applies the
same union/difference/intersection/closed-solid set operation per lane, and the
closest positive hit is then projected back to the packet-hit contract. Sphere,
Box, OpenCylinder, Torus, and static and moving Instance wrappers publish direct
Ray4/Ray8 packet intervals, so common beveled dice/cylinder/ring-style CSG
avoids scalar materialization fallback at both the CSG node and its key leaves.
Moving `Instance` wrappers now own lane-local Ray4/Ray8 packet hits and
intervals too: each lane keeps its `State::timeSample` when transforming into
the wrapped primitive, so motion-blurred instances preserve shutter-time
semantics without inheriting the generic packet fallback.
`ConvexOperation` subclasses now own Ray4/Ray8 packet hit and interval
materialization for support-map CSG. That path still evaluates the GJK-style
support query per lane rather than claiming a true SIMD support-map kernel, but
convex-hull and Minkowski-sum frontiers no longer inherit the generic scalar
packet fallback.
`Grid` also owns Ray4/Ray8 packet-hit materialization through lane-local DDA
traversal. That still is not a true vectorized packet-grid walker, but packet
frontiers that pass through Grid preserve DDA cell pruning without being
reported as generic scalar packet fallback.
The fallback metric keeps the remaining unsupported leaves visible.
Wavefront metrics now also expose packet-frontier utilization:
`frontierPacketChunksPerDepth`, `frontierPacketRaysPerDepth`,
`frontierRay4PacketChunksPerDepth`, `frontierRay8PacketChunksPerDepth`,
`frontierScalarRaysPerDepth`, and `frontierPacketScalarFallbackRaysPerDepth`
distinguish packet chunks, the exact ray count carried by those chunks, the
Ray4/Ray8 chunk split, scalar tail rays, and packet lanes that still had to use
base scalar hit
materialization in JSON reports, graph trace metadata, rendercli compact
summaries, and convergence capture work comparisons. Composite, BVH, and CSG
packet traversal now mask inactive lanes before forwarding per-lane state to
child materializers, so those fallback counters describe work that survived the
parent traversal instead of lanes the parent had already rejected. Whitted
batches now also publish `frontierPacketRefinedRaysPerDepth`, the packet-hit
lanes that were materialized through the packet path but then scalar-refined
before shading.
Metrics also aggregate `frontierPacketScalarFallbackRaysByReason`, which keeps
the base packet-hit fallback path visible in JSON reports, rendercli summaries,
graph pass trace text, and convergence capture comparisons when a primitive or
wrapper still reaches the conservative scalar materialization contract.
Keeping packet rays explicit instead of deriving them from chunk count prepares
the metrics for mixed Ray4/Ray8 frontiers. The packet chunk metric is now split
by width too, so captures can see whether frontiers are mostly full Ray8
packets, Ray4 tails, or scalar leftovers.
That mixed-width frontier is now underway: primitive packet-hit results are
width-generic, `Primitive` exposes an eight-wide scalar fallback, plain
composites merge eight-wide child hits, BVH preserves materialized eight-wide
hits through active-mask tree traversal, and Whitted/path-tracing batches try
full Ray8 frontier chunks before Ray4 chunks and scalar tails. Whitted batches
now compact traceable continuations ahead of terminated continuation rays before
choosing packet chunks, so interleaved throughput/max-depth terminations do not
force otherwise traceable rays down the scalar tail path. Packet hits now carry
a per-lane scalar-fallback flag through composites, BVH, mesh wrappers, and
instances, so Whitted reflective materials do not repeat scalar refinement when
the packet hit was already produced by scalar materialization. The Ray8 slice is
currently a scheduler and BVH/composite contract plus first leaf kernels for
`Sphere`, `Plane`, `Box`, `Triangle`, `Disk`, and `Rectangle`, not a claim that
every primitive has an eight-wide materialization kernel. `OpenCylinder` and
`Torus` now join that eight-wide leaf set for curved analytic frontiers; the
imported-mesh leaf/wrapper path also preserves Ray8 materialized hits,
`ConvexOperation` materializes Ray8 support-map CSG hits and intervals per lane,
`Grid` materializes Ray8 hits through DDA traversal, and `Curve` reports Ray8
misses directly. The packet scalar-fallback metric intentionally reports the
remaining leaf gaps so the next performance slices can target them directly.
The graph-backed wavefront metrics functional test now pins the stable wavefront
fixture at `frontier_packet_scalar_fallback_rays=0`,
`frontier_packet_scalar_fallback_by_reason=none`,
`frontier_packet_refined_rays=0`, and
`frontier_packet_refined_by_material=none`, so regressions in the common packet
frontier path are caught even when they do not change the final image.
Materials now own that decision: local Matte/Phong shading and built-in
reflective, transparent, and portal continuations consume packet hits directly.
The packet carries the original double-precision ray alongside the float SoA
lanes used for traversal, so packet hit materialization can recover the
double-precision origin/direction that strict Whitted secondary-ray parity
needs. Custom materials keep the conservative scalar-refinement default. Metrics
also aggregate refined packet lanes by material-family label (`reflective`,
`transparent`, `portal`, or conservative `custom`) in JSON reports, rendercli
summaries, and graph pass trace text, so captures can show which material owns
any remaining refinement cost. The convergence capture script carries those
material buckets into its work-comparison output when both compared variants
include wavefront metrics. These counters give Phase 7 captures a direct signal
for whether the next speed slice should improve packet filling, scalar-tail
handling, packet-hit precision, or remaining leaf packet traversal cost.
Wavefront metrics now also include min/average/max non-empty tile sample counts
in the tiling block and rendercli compact summaries. That gives Phase 7
captures a direct load-balance baseline before changing queue-size defaults,
tile dimensions, or work-stealing behavior. The convergence capture script now
carries both the tile metrics and the Ray4/Ray8 packet chunk split into
work-comparison reports, so a tuning run records those signals beside timing
and active-depth work. `WAVEFRONT_CONVERGENCE_QUEUE_SWEEP` now runs the same
capture across multiple queue sizes and writes each result under a separate
`queue_<size>` output directory, so tile-count defaults can be compared without
hand-editing the script or overwriting earlier captures. Queue sweeps also
write a per-scene `queue_sweep.summary.txt` with median render time, primary
samples, tile load, Ray8/Ray4 packet chunks, scalar tails, packet fallback
lanes, and worker-time buckets for each queue/variant pair.
A first 160x120 BVH queue sweep also exposed a rendercli graph-path mismatch:
graph-backed ray-family renders inherited the scene camera's progressive view
plane while direct rendercli final renders used `TiledViewPlane`. Very small
queue tiles made the progressive iterator visit extra samples, so high
queue-count metrics overstated primary work. rendercli now fills in
`TiledViewPlane` as a graph pass default only when scene intent leaves the
ray-family view plane unresolved. A follow-up 160x120, queue-size-512 BVH
capture reported the expected `samples=19200`, `tile_count=512`,
`average_nonempty_tile_samples=37.5`, and exact image parity, so queue-size
tuning can use the fixed graph path without confusing iterator oversampling
with scheduler cost.
The first scalar-tail handling slice is now in place too: when a Whitted or
path-tracing wavefront frontier has two or three traceable rays left after full
Ray8/Ray4 chunks, the integrator submits those rays as active lanes in one Ray4
packet and leaves inactive lanes masked by the existing packet-state contract.
`frontierPacketRaysPerDepth` still reports the exact active-lane count, so
captures can distinguish "one partial Ray4 chunk carrying two rays" from "one
full Ray4 chunk carrying four rays." Queue-size defaults and broader tile-size
tuning remain open; this only removes avoidable scalar tails caused by small
frontier remainders.

---

## Interactions with other plans

### `render-graph.md`

This plan now depends on the graph path. rendercli renders through the graph by
default, Modeler previews are graph-backed, and scenes carry editable render
intent. A wavefront implementation must therefore add graph-visible executor
and pass state from the start; `--engine wavefront` as a direct-engine bypass is
useful for focused debugging, but it is not the primary user surface.

The first wavefront slice now uses the same graph-visible
`RaytracerBeautyPassState` for ray-family beauty passes. That keeps sampler,
view-plane, recursion-depth, thread/queue, and integrator choices intent-derived
instead of direct-engine-only.

### `complete/core-math-optimization.md`

- Phase 1.2 (SIMD `BoundingBox::intersects`) directly benefits the
  wavefront scheduler — every depth pass slams the BVH.
- Phase 1.4 (HitPointInterval small-buffer) is even more valuable
  under wavefront, where intersection batches are explicit.
- Phase 4 (SoA / batched ray ops) has landed and plugs in here naturally in
  Phase 7.

### `point-vector-normal-types.md`

- The wavefront `PixelState` struct is the natural first user of the
  new typed Point/Direction. Land PVN first; wavefront writes against
  the new types from day one.
- `Transform<T>` wrapper is exactly the right type for the camera in
  the wavefront engine (cameras transform Points for primary-ray
  origins, Directions for primary-ray directions).

### `whitted-ray-packets.md`

The packet plan remains a scalar `Raytracer` optimization plan, not a
replacement for wavefront. Its Phase 0 decision was to pursue sample packets
inside the Whitted-style render path when that work resumes. Wavefront can use
the same packet intersection substrate later, but the scheduler work in this
plan should not wait for packetized Whitted rendering.

### Existing `Raytracer`

- No changes to the recursive engine. Both engines coexist
  indefinitely. `Raytracer` remains canonical for tests, debug
  visualization, and interactive picking.

---

## Risks

- **Memory explosion**. Per-pixel state arrays for 4K images at
  many-samples-per-pixel get large. Mitigation: tile-based scheduling
  is the default (not just a large-case fallback) — per-tile state
  fits in L2/L3, and only the per-tile state is hot at any moment.
- **Convergence threshold tuning**. A threshold that's too tight
  defeats the speedup; too loose introduces visible artifacts.
  Mitigation: bake threshold tuning into the macro benchmark; treat
  the chosen value as a per-engine setting visible in UI/CLI.
- **Path-tracing variance.** When Phase 5 lands, low-spp renders will
  look noisy compared to Whitted's smooth output. Mitigation: ship
  with sensible default spp; document that denoising (Phase 6)
  follows; keep `Raytracer` as the noise-free fallback.
- **Material BSDF coverage.** The scalar path tracer falls back to
  Whitted shading for materials that do not implement BSDF sampling.
  A wavefront path tracer can make the same compatibility choice, but
  it will not get true indirect lighting through those materials until
  the material side is refactored. Mitigation: track material BSDF
  support explicitly and keep graph/pass trace metadata visible so
  users can see when a pass used compatibility shading.
- **Graph/direct divergence.** rendercli has historically grown
  direct-engine switches faster than graph-visible intent. Mitigation:
  Phase 2 makes integrator selection graph-visible before wavefront
  adds another backend choice.
- **Test pinning breakage.** Tests that pin exact pixel values from
  the recursive engine will not pin against the wavefront engine
  (different ordering, different floating-point accumulation order).
  Mitigation: tolerance-based pixel comparisons for cross-engine
  tests; per-engine exact-match tests where exact match matters.
- **Single-ray probe APIs** (`primitiveForRay`, `rayState`,
  `rayColor`) don't exist on the wavefront engine. Mitigation: keep
  `RayCaster` mixin on `Raytracer` only; UI code that depends on
  single-ray probes uses `Raytracer` even when `WavefrontRaytracer`
  is doing the main render.
- **Threading model**. The wavefront engine is still tile-major
  for the same reasons the recursive engine is — tiles map cleanly
  onto `QThreadPool` workers, preserve BVH cache coherence, and
  enable the existing progressive-display UI. The depth-major
  scheduling happens *inside* each tile's worker, not across the
  whole image. Mitigation: reuse the existing tile dispatch in
  `Raytracer.cpp:Private`; only the tile-worker body differs.

---

## Open questions

Most of these decisions are now locked by the implemented wavefront engine.
The entries that remain open are Phase 7+ performance/layout questions rather
than prerequisites for the engine surface.

1. ~~**Whole-image or tile-based as the default?**~~ **Resolved**:
   tile-based, always. That's how the engine exploits multiple CPU
   cores — same reason the existing `Raytracer` is tile-based. See
   [Parallelism: tile-based by default](#parallelism-tile-based-by-default).
   The remaining question is the right tile size; default to whatever
   the recursive engine uses, retune in Phase 7 from metrics-backed captures.
   The tiling metrics now expose min/average/max non-empty tile sample counts
   so tuning can distinguish image size effects from real load imbalance.
2. ~~**Convergence-detection scheme.**~~ **Resolved for v1**: use active
   sample fraction plus RMS radiance delta over the active subset. Preview,
   Balanced, and Final presets ship through render intent, Modeler Render
   Settings, graph JSON, and rendercli. Phase 4 tuning remains open as a
   performance-policy question, not as an API decision.
3. ~~**Tree-branching strategy for v1.**~~ **Resolved for sequencing**:
   use **A** for the Whitted-parity scheduler proof, then start
   wavefront path tracing with **B** because scalar path tracing already
   landed with that shape. Revisit **C** only after variance/performance
   data says deterministic specular splitting is worth the complexity.
4. **`PixelState` memory layout.** AoS for v1. Path tracing currently keeps
   compact per-path records plus separate sample-color storage; Phase 7 remains
   the place to migrate hot frontier state to SoA or hybrid storage if captures
   prove the scheduler is memory-layout bound.
5. ~~**Samples-per-pixel control.**~~ **Resolved for v1**: global
   samples-per-pixel already exists in rendercli and render intent.
   Wavefront consumes the same setting. Per-pixel adaptive sampling is
   later work.
6. ~~**CLI / UI surface.**~~ **Partially resolved**: render intent /
   render graph are the primary surface; direct `--engine wavefront`
   remains useful as a debug bypass. Modeler should expose the same
   choice in Render Settings, and render dialogs should preview the
   compiled graph before rendering.
7. ~~**Where do scene-format defaults live?**~~ **Resolved**: engine and
   integrator preferences live in scene-backed render intent, not in
   file-format-specific scene loaders. Importers may suggest sensible
   cameras/lights/backgrounds, but they should not prescribe graph
   executors except through ordinary render intent metadata.
8. ~~**Resume / progressive rendering.**~~ **Resolved for v1**: wavefront
   batches publish per-depth sample-color snapshots through the batch observer,
   and graph-backed Modeler previews can display those intermediate images while
   final-image rendercli paths can disable snapshot publication.
9. ~~**Integrator factoring.**~~ **Resolved for current scope**: direct
   lighting, active-frontier intersection, packet/scalar traversal accounting,
   hit/miss bookkeeping, and active-path compaction now live on the owning
   integrator types. Keep future scheduler changes on instance methods or
   dedicated behavior-owning classes rather than helper-function piles.

---

## What this is not

- **It is not a replacement for the existing `Raytracer`.** Both ship.
  The recursive engine stays as canonical for tests, debug, and
  single-ray probes.
- **It is not the scalar path tracer.** `PathTracingIntegrator` already
  exists and stays useful as the simple teaching implementation. Wavefront
  path tracing is the later scheduler/queue form of compatible transport
  semantics.
- **It is not a path tracer in the first wavefront engine phase.** The
  first wavefront engine ships with Whitted semantics, just scheduled
  differently. Wavefront path tracing comes after graph/intent parity and
  Whitted scheduler proof.
- **It is not a GPU renderer.** Phase 7+ leaves that door open;
  Phases 2-6 are all CPU.
- **It is not a SoA refactor of the whole codebase.** Phase 2 only updates
  intent plumbing; the first wavefront engine phase uses AoS per-pixel state.
  SoA is a future optimization.

---

## Working method

1. ~~Land Phase 1 (throughput cutoff,
   [#133](https://github.com/tkadauke/raytracer/issues/133)) first.
   Validates the throughput arithmetic in a small surface.~~ ✅ **Done.**
2. ~~Land the scalar path-tracing substrate.~~ ✅ **Done.**
   `PathTracingIntegrator`, `SampleStream`, material BSDF hooks, light
   sampling, and Russian-roulette support are now present.
3. ~~Do Phase 2 before the wavefront engine: make integrator choice part of
   scene-backed render intent and compiled graph pass state.~~ ✅ **Done.**
4. Resolve the remaining scheduler open questions as Phase 3 moves from the
   executor shell to explicit depth-major queues.
5. Phase 3 (bare wavefront) must produce byte-comparable output to
   `Raytracer` for the same maxDepth on the macro benchmark scenes.
   This is the regression gate.
6. Each subsequent phase has its own quality gate stated in its
   description. Don't skip the gate to ship — the whole point of the
   wavefront engine is to be measurably better than the recursive
   one; if a phase doesn't show that, regroup before continuing.
7. Every PR updates `CHANGELOG.md` under `## Unreleased`.
8. Every PR runs the full test suite end-to-end. The whole-render
   macro benchmark must be reported per `complete/core-math-optimization.md`'s
   "Rule." This plan is partly an architecture refactor and partly a
   performance optimization — both criteria apply.
