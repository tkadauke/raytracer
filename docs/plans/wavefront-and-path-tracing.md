# Wavefront ray tracing — and the path to path tracing

> **Scope:** introduce a new `RenderEngine` implementation that
> processes rays *depth-major* — all pixels at depth 1, then all
> still-active pixels at depth 2, and so on — with an image-wide
> convergence check between depth passes. Foundation for per-pixel
> adaptive depth, denoising, stochastic path tracing, and (eventually)
> GPU offload. Captured 2026-05-10 from the conversation about
> "compute one recursion at a time, stop when nothing changes."
>
> **Status:** Living document — design proposal, not yet committed.
> Open questions need decisions before any implementation issue beyond
> the throughput-cutoff prerequisite ([#133](https://github.com/tkadauke/raytracer/issues/133))
> fans out.
>
> **Rule:** the wavefront engine is a **sibling** to the existing
> `Raytracer`, not a replacement. Both ship; the user chooses. Reuses
> the shared substrate (intersection, materials, BVH, lights, camera)
> through the existing `render::` namespace — wavefront only changes
> the scheduling.

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

3. **Stochastic path tracing.** Once you're scheduling depth-major
   with one outgoing ray per hit, classical Whitted's reflect+transmit
   branching becomes unnatural. Importance-sample one outgoing
   direction at each hit, shoot many primary samples per pixel,
   average. The wavefront engine is the natural host.

4. **Denoising.** Once you have a partial image after each depth pass
   (and per-pixel uncertainty signals from path tracing), drop in a
   denoiser between passes. Cheap variant: spatiotemporal filter; rich
   variant: OIDN-style learned denoiser.

5. **GPU offload.** Wavefront is the canonical GPU ray-tracing
   architecture (Laine, Karras & Aila 2013 introduced it for that
   reason). Once data is SoA and scheduling is depth-major, the path
   to GPU is well-trodden.

Each builds on the previous. This plan covers (2) in detail, (3) and
(4) in sketch, and (5) as a future-work pointer.

---

## Same engine or separate? The architectural question

Thomas asked: "Do we need a separate render engine for that or can we
implement it as an alternative render method?"

**Answer: new sibling engine, not a rewrite of the existing
`Raytracer`.** The existing engine architecture already supports this
cleanly — see `include/engine/`, which today has three siblings
(`raster`, `wireframe`, `raytracer`), all concrete `RenderEngine`s.

### Why a sibling rather than an in-place rewrite

Three reasons:

1. **`Raytracer::rayColor` is a recursive function.** Wavefront
   scheduling is iterative-with-per-pixel-state. Trying to fit both
   into one class produces a mess. The recursive form also supports
   the single-ray probe API (`primitiveForRay`, `rayState`,
   `rayColor`) used by interactive picking, tests pinning shading
   behavior, and `RefractingRayTracer`'s debug visualization. Those
   probes don't want depth-major scheduling.
2. **A/B benchmarking.** With both engines shipping, you can run the
   same scene through both and directly measure the difference rather
   than reasoning about it.
3. **Risk containment.** If wavefront has a subtle bug for some
   material/scene combo, users have a known-good fallback. Especially
   important during the transition into path tracing, where the
   semantics genuinely change (stochastic per-sample vs. deterministic
   per-ray).

### What gets shared

Everything except the scheduling. Materials, primitives, BVH
traversal, intersection routines, lights, cameras, tonemapping, and
the per-ray `State` machinery all live in `render::` already and stay
there.

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

`RayCaster` mixin (`include/engine/raytracer/Raytracer.h:63`) is
specific to the recursive engine — wavefront doesn't inherit it,
and the single-ray probe API stays on `Raytracer` only.

### What gets refactored

Probably nothing in `Raytracer.cpp`. The wavefront engine may need
finer-grained access to the material's incoming/outgoing direction
APIs than `rayColor` currently exposes, but those should already exist
inside the shading code; we'd just need to call them at the right
seam.

If wavefront grows enough to want SoA ray batches (Ray4/Ray8 per
Phase 4 of `core-math-optimization.md`), the shared intersection
routines may grow batched overloads. The existing AoS path stays for
`Raytracer`.

---

## Wavefront architecture

### Per-pixel state

For an image of W×H pixels, allocate state arrays sized W·H:

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

Memory: a `PixelState` is probably ~80 bytes; for a 1920×1080 image
that's ~160 MB. Manageable on a desktop, tight on a laptop. For larger
resolutions or multiple samples-per-pixel state, tile the work — see
[Tile-based wavefront](#tile-based-wavefront).

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

Each pass parallelizes over the active-pixel set. The existing
tile+thread machinery in `Raytracer.cpp:Private` can be reused for the
parallel loop body — different scheduler, same threading primitive
(`QThreadPool`).

### Tile-based wavefront

For very large images or many-samples-per-pixel, hold state arrays for
a single tile at a time rather than the whole image:

```
for each tile T:
  allocate state[T]
  run the depth loop above on T
  write tile T into buffer
```

Loses image-wide convergence (now per-tile convergence), but recovers
memory. Default to whole-image for ≤1080p, fall back to tiles for
larger.

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
`Constants.h` (per Phase 3.7 of `core-math-optimization.md`).

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

**Recommendation**: ship wavefront with **C** (hybrid) as v1. Whitted
deterministic split for specular/transmissive bounces (small fan-out),
single-sample importance for diffuse. Migrate to pure **B** when path
tracing arrives in earnest. **A** is mostly a stepping stone — useful
for proving the architecture before committing to stochastic
sampling, but the implementation effort doesn't transfer well to **B**
later.

---

## Phased plan

Each phase is a separate PR. Convergence-test infrastructure ships
with the first phase that introduces it; subsequent phases reuse it.

### Phase 0 — design lock

Resolve open questions below. Pick convergence-detection scheme,
tree-branching strategy, memory layout. Commit decisions to this doc.
No code changes yet.

### Phase 1 — throughput-based cutoff in the existing engine

Tracked as [#133](https://github.com/tkadauke/raytracer/issues/133).
Prerequisite for the wavefront engine because it validates the
throughput-tracking arithmetic in a smaller, easier-to-debug context.

### Phase 2 — bare wavefront engine: same outputs as Raytracer

New `WavefrontRaytracer` sibling under `include/engine/wavefront/`.
Whitted semantics with tree-flattening (Option **A** above) so the
output matches `Raytracer` byte-for-byte (modulo floating-point
non-determinism from threading). Convergence test runs but doesn't
yet drive any cutoff — instrumented to log delta-per-pass to a
benchmark report.

**Goal**: prove the architecture without changing image output.
**Gate**: macro benchmark output (sphere / torus / BVH scenes) RMS
within 1e-3 of `Raytracer` output for the same maxDepth.

### Phase 3 — image-wide adaptive depth via convergence detection

Activate the convergence test as a stop condition. Active-pixel count
+ L2 over active subset. Threshold tuning via the macro benchmark.

**Goal**: render faster than `Raytracer` on common scenes without
visible quality loss.
**Gate**: ≥30% wall-clock improvement on the BVH-heavy scene at
matching quality (visual delta < ε).

### Phase 4 — switch tree-branching strategy from A to C

Hybrid: specular bounces stay deterministic-split; diffuse switches to
single-sample importance. Add a basic samples-per-pixel control —
shoot N primary rays through each pixel, average. Per-pixel state
becomes per-(pixel, sample), but the scheduler is unchanged.

**Goal**: indirect lighting on diffuse surfaces (the path-tracing
payoff).
**Gate**: a Cornell-box-style scene with indirect bounce produces
the visual difference path tracing is famous for.

### Phase 5 — denoising hook between passes

Add a `Denoiser` interface that runs between depth passes (or just at
the end). v1: simple spatiotemporal filter (a-trous or bilateral on
the per-pixel `accumulated` channel). v2: link to OIDN or similar
learned denoiser.

**Goal**: low-sample renders look acceptable.
**Gate**: 4spp render with denoiser produces image visually comparable
to 64spp without denoiser.

### Phase 6+ — SoA / GPU / packet traversal

Future-work. Once the wavefront engine is stable and path-tracing
semantics are locked in, the SoA / Ray4 / Ray8 work from
`core-math-optimization.md` Phase 4 plugs in here as a performance
optimization to the existing scheduler. GPU offload becomes
architecturally tractable but is a major lift; not committed.

---

## Interactions with other plans

### `core-math-optimization.md`

- Phase 1.2 (SIMD `BoundingBox::intersects`) directly benefits the
  wavefront scheduler — every depth pass slams the BVH.
- Phase 1.4 (HitPointInterval small-buffer) is even more valuable
  under wavefront, where intersection batches are explicit.
- Phase 4 (SoA / batched ray ops, deferred) plugs in here naturally
  in Phase 6.

### `point-vector-normal-types.md`

- The wavefront `PixelState` struct is the natural first user of the
  new typed Point/Direction. Land PVN first; wavefront writes against
  the new types from day one.
- `Transform<T>` wrapper is exactly the right type for the camera in
  the wavefront engine (cameras transform Points for primary-ray
  origins, Directions for primary-ray directions).

### Existing `Raytracer`

- No changes to the recursive engine. Both engines coexist
  indefinitely. `Raytracer` remains canonical for tests, debug
  visualization, and interactive picking.

---

## Risks

- **Memory explosion**. Per-pixel state arrays for 4K images at
  many-samples-per-pixel get large. Mitigation: tile-based wavefront
  for the large case; document the resolution × spp × memory
  trade-off clearly.
- **Convergence threshold tuning**. A threshold that's too tight
  defeats the speedup; too loose introduces visible artifacts.
  Mitigation: bake threshold tuning into the macro benchmark; treat
  the chosen value as a per-engine setting visible in UI/CLI.
- **Path-tracing variance.** When Phase 4 lands, low-spp renders will
  look noisy compared to Whitted's smooth output. Mitigation: ship
  with sensible default spp; document that denoising (Phase 5)
  follows; keep `Raytracer` as the noise-free fallback.
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
- **Threading model**. The existing `QThreadPool` orchestration in
  `Raytracer::Private` is tile-major. Wavefront wants pixel-major
  parallelism within each depth pass. Mitigation: shared parallel
  primitives in `render::` but different orchestration in each
  engine; benchmark thread saturation per pass.

---

## Open questions

These need decisions before Phase 2 (the bare wavefront engine)
starts.

1. **Whole-image or tile-based as the default?** Whole-image is
   simpler and supports image-wide convergence test. Tile-based
   contains memory and parallelizes obviously. Lean whole-image for
   ≤1080p with a tile fallback above; expose as user-configurable.
2. **Convergence-detection scheme.** L2, max, percentile,
   active-pixel count, or combination. Lean active-pixel count + L2
   over active subset; not locked.
3. **Tree-branching strategy for v1.** A (flatten queue), B (single
   sample), or C (hybrid). Lean **A for Phase 2**, swap to **C in
   Phase 4**. Pure **B** waits for explicit path-tracing decision.
4. **`PixelState` memory layout.** AoS (`std::vector<PixelState>`),
   SoA (parallel arrays per field), or hybrid. AoS for v1; profile and
   migrate to SoA in Phase 6 if needed.
5. **Samples-per-pixel control.** A global setting? Per-pixel
   adaptive (more samples where variance is high)? Lean global for
   v1 (matches Whitted's effectively-1-spp), per-pixel adaptive in
   Phase 4 or later.
6. **CLI / UI surface.** A new `--engine wavefront` flag to
   `rendercli`? A GUI engine-selector dropdown? Both presumably;
   detail depends on the existing CLI/UI shape.
7. **Where do scene-format defaults live?** A scene file probably
   wants to express "this scene needs path tracing for the indirect
   bounce." Add to the scene schema, or leave engine choice 100% on
   the operator?
8. **Resume / progressive rendering.** Should the wavefront engine
   produce a usable image after every depth pass for progressive
   display? (It naturally can — the `accumulated` field is already a
   valid image at every pass.) Yes, ship it. Probably want a "preview
   on" flag for the GUI.

---

## What this is not

- **It is not a replacement for the existing `Raytracer`.** Both ship.
  The recursive engine stays as canonical for tests, debug, and
  single-ray probes.
- **It is not a path tracer in Phase 2.** Phase 2 ships with Whitted
  semantics, just scheduled differently. Path tracing is Phase 4+.
- **It is not a GPU renderer.** Phase 6+ leaves that door open;
  Phase 2-5 are all CPU.
- **It is not a SoA refactor of the whole codebase.** Phase 2 uses AoS
  per-pixel state; SoA is a future optimization.

---

## Working method

1. Land Phase 1 (throughput cutoff,
   [#133](https://github.com/tkadauke/raytracer/issues/133)) first.
   Validates the throughput arithmetic in a small surface.
2. Resolve open questions before Phase 2 starts.
3. Phase 2 (bare wavefront) must produce byte-comparable output to
   `Raytracer` for the same maxDepth on the macro benchmark scenes.
   This is the regression gate.
4. Each subsequent phase has its own quality gate stated in its
   description. Don't skip the gate to ship — the whole point of the
   wavefront engine is to be measurably better than the recursive
   one; if a phase doesn't show that, regroup before continuing.
5. Every PR updates `CHANGELOG.md` under `## Unreleased`.
6. Every PR runs the full test suite end-to-end. The whole-render
   macro benchmark must be reported per `core-math-optimization.md`'s
   "Rule." This plan is partly an architecture refactor and partly a
   performance optimization — both criteria apply.
