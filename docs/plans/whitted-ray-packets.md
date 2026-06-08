# Whitted integrator ray-packet plan - May 2026

> **Scope:** broaden the use of `Ray4` packets from the BVH benchmark
> microsurface they currently inhabit into the actual `WhittedIntegrator`
> render path, so the 5-12× per-primitive packet speedups we measured
> (see `docs/perf/arm-simd-phase2-packet-kernels-2026-05-29.md` and
> `docs/perf/arm-simd-phase5-ray8-policy-apple-silicon-2026-05-30.md`)
> translate into end-to-end faster `rendercli` numbers.
>
> **Status:** partially implemented through the depth-major batch/frontier path,
> not through the original direct `Raytracer` camera sample loop. Phase 0
> measurement is complete and the shipped `Integrator::radianceBatch(...)`
> implementations now use Ray4/Ray8 packet frontier intersections for Whitted
> and path-tracing batches, with metrics and unit coverage under
> `test/unit/render/*IntegratorTest.cpp` and
> `test/unit/engine/wavefront/WavefrontRaytracerTest.cpp`. The original direct
> `Camera::render` / `RayCaster::rayColorPacket` path, explicit shadow-ray
> packets, and direct rendercli packet parity/perf gates remain open.
>
> **Related plans:**
> `docs/plans/complete/arm-simd.md` (the substrate — Float4/Mask4/Ray4 backends,
> all phases done). `docs/plans/complete/rasterizer-performance.md` (different
> code path entirely — that one targets the OpenGL/CPU rasterizer; this
> one targets the path-tracer-style integrator).

---

## Why

When this plan was written, every `core::simd::Float4` win and every
`intersectPacket(Ray4)` override sat on a shelf as far as the actual raytracer
was concerned. The old scalar-only chain was:

```text
Camera::render (per pixel, per sample)
  ↓
RayCaster::rayColor(ray)
  ↓
WhittedIntegrator::radiance(ray, state)          ← scalar
  ↓
Scene::intersect(ray, hitPoints, state)          ← scalar
  ↓
BVH::intersect(ray, ...) / Sphere::intersect(...) ← scalar
```

That is no longer the whole story. The direct single-ray `Raytracer` probe path
is still scalar, but the depth-major batch path now has production call sites:
`WhittedIntegrator::radianceBatch(...)` and
`PathTracingIntegrator::radianceBatch(...)` compact active frontiers, submit
Ray8/Ray4 chunks to `Scene::intersectPacketHits(...)`, scalar-refine packet hits
when material state requires it, and report packet/scalar/fallback counters in
wavefront metrics. This plan therefore remains open only for the direct
camera/sample-loop optimization and shadow-ray packet slices.

The two questions this plan answers:

1. **Is it even possible?** Yes, for primary rays and (with more
   effort) shadow rays. Past the first reflection/refraction bounce,
   re-batching diverged rays into coherent packets is approximately
   "rewrite the integrator," and the existing literature on packet
   Whitted vs wavefront path-tracing says it stops paying off there.
2. **Is it worth it?** Unknown without measurement. The packet
   speedups are real for the intersect kernel, but a full render may
   be bottlenecked on tonemap, BRDF, sampling, or recursion overhead
   long before BVH traversal. Phase 0 is a measurement gate.

## Non-goals

- **Not a path-tracer rewrite.** Wavefront or per-bounce ray re-sorting
  is a different algorithm; this plan stays within the Whitted-style
  integrator structure.
- **Not replacing the scalar integrator.** Education path stays
  intact. Packet variant lives alongside as an optimization.
- **Not packetizing material shading.** BRDF evaluation, color math,
  recursive calls into reflection/refraction, shadow-ray emission
  past the first bounce — all stay scalar. We packetize the
  intersection step only.
- **Not changing the public `Integrator` interface.** External callers
  see `radiance(Rayd&, State&)`; the packet variant is an internal
  fast path.
- **Not Ray8 on ARM.** Per `docs/perf/arm-simd-phase5-ray8-policy-*`,
  Ray4 is the ARM packet width and that decision stays. AVX builds
  can add a parallel Ray8 path later if measurement justifies it.

## What changes (the obstacles up front)

Four things in the current integrator design make packetization harder
than a simple `intersect → intersectPacket` swap. Calling them out so
the phases below address them deliberately:

1. **Material divergence.** Four adjacent pixels can hit four
   different primitives (and four different materials). You can run
   intersection as a packet, but BRDF evaluation has to scalarize —
   you can't vectorize `MatteMaterial::shade` and
   `PhongMaterial::shade` together. The plan: packet the intersect
   step, scalarize the shade step. This caps the speedup at "intersect
   was X% of the wall clock, so we save up to X%·(1 - 1/N)".
2. **Recursion divergence.** Each lane's hit may bounce to a different
   recursion depth (one ray hits a mirror, another hits a matte
   surface). The plan: after the first packet intersect, lanes that
   need recursion fall back to the scalar integrator one ray at a
   time. We get the win on the *first* intersect of a packet only —
   no chained packet recursion.
3. **State divergence.** `State` carries recursion depth, throughput,
   stats counters, the hit-point cache for AOVs. Packet code needs
   four parallel `State` objects. Mostly mechanical, but every
   stats-emitting line needs to know which lane it's writing to.
4. **AOV / trace event divergence.** `state.recordEvent(...)` is
   per-ray. In packet mode there's no obvious "this happened to the
   packet" event. The plan: events stay per-lane; the packet path
   emits four events when interesting, one event when not.

## Phase 0 — measurement gate ✅ **Done — decision: proceed, skip Phase 1, jump to Phase 3.**

Baseline captured in
`docs/perf/whitted-packet-phase0-2026-05-31.md`. spp scaling on
three Phase-0 scenes (dice, glass_torus, molecule) shows 85-100%
of wall time is per-ray work; fixed render overhead is negligible.
Combined with the NEON `BVH::intersectPacket(Ray4)` 5.85× speedup
documented in
`docs/perf/arm-simd-phase5-ray8-policy-apple-silicon-2026-05-30.md`,
the gate criterion (≥30% achievable end-to-end speedup on the
mesh scene) is met.

**Implementation pivot:** the first shipped packet use did **not** follow the
Phase 1/3 direct-camera route. The wavefront work introduced
`Integrator::radianceBatch(...)` and made active ray/path frontiers packet-aware
instead. That path covers primary and secondary frontiers produced by
Whitted/path-tracing batches, preserves the scalar single-ray educational
surface, and exposes the packet-fill/fallback metrics needed to decide whether
the direct `Raytracer` sample-loop optimization is still worth doing.

## Phase 0 — measurement gate (original — now historical)

**Goal:** decide whether to do this at all.

Before writing a single line of packet integrator code, capture a
baseline that the rest of the plan can be measured against. Without
this, "did the change help" is unanswerable.

Tasks:

- **Pick three scenes** that should respond to packet wins differently:
  - `scenes/dice.json` (small, simple — should see negligible
    benefit; tonemap + BRDF dominate)
  - `scenes/glass_torus.json` (CSG, refraction, deep recursion —
    primary rays packetize, post-bounce rays don't)
  - a high-poly mesh scene from `scenes/` or
    `test/fixtures/molecule/` (BVH-traversal-dominated, the case
    that should win biggest)
- **Run `rendercli --timing` repeatedly** at fixed dimensions and
  fixed sample counts; capture min/median/avg/max wall time per scene.
  Save raw numbers under `docs/perf/whitted-packet-phase0-<date>.md`
  in the existing arm-simd format.
- **Decompose where time goes.** Add tracing or use `STATS=1`
  counters to split the per-frame budget into: BVH traversal,
  primitive intersect, material shade, recursion, tonemap, plot.
  This is the most important data point. If BVH+intersect is < 20%
  of wall time, the integrator packetization is not going to move
  the needle and the plan stops here.
- **Decision:** if BVH+intersect ≥ 30% on the mesh scene, proceed to
  Phase 1. Otherwise, document the measurement and close the plan as
  "packet integrator infrastructure exists, end-to-end benefit too
  small to justify integrator surgery."

Acceptance:

- Three baseline runs captured in a perf doc with reproducible commands.
- An honest verdict on whether Phase 1 is worth doing.

## Phase 1 — primary-ray packets in 2×2 sub-tiles

**Goal:** the obvious win — coherent primary rays go through
`BVH::intersectPacket`.

**Status:** not implemented for direct `Raytracer` tile dispatch. The shipped
packet frontier path lives behind `Integrator::radianceBatch(...)` and
`engine::wavefront::WavefrontRaytracer`, not behind
`Camera::renderPacketed(...)` or `RayCaster::rayColorPacket(...)`.

Tasks:

- **Camera packet generation.** New
  `Camera::rayPacketForPixels(x0,y0, x1,y1, x2,y2, x3,y3, samplers)`
  returning `Ray4` with origin/direction SoA-packed. Lanes that
  fall outside the buffer rect get marked inactive (mask bit clear)
  rather than skipped — keeps the packet shape rectangular.
- **Tile dispatch in 2×2.** New
  `Camera::renderPacketed(raycaster, buffer, rect)` that walks the
  tile in 2×2 sub-tiles, generates one Ray4, calls a new
  `RayCaster::rayColorPacket(Ray4, State[4]) → array<Colord, 4>`,
  plots all four. Gated on:
  - pixel size = 1 (no downsampling)
  - samples-per-pixel = 1 (Phase 3 handles multi-sample)
  - integrator type = `WhittedIntegrator` (initially)
  - rect dimensions even on both axes; odd edges fall back to scalar
- **Packet integrator entry.**
  `WhittedIntegrator::radiancePacket(Ray4 rays, array<State, 4>& states) → array<Colord, 4>`
  that:
  1. Calls `scene.intersectPacket(rays, states[0])` for BVH +
     primitive intersect.
  2. For each lane: if hit, scalarize — extract `Rayd` from
     `rays.rayd(lane)`, build the corresponding `HitPoint` from
     `RayPacketIntersection4::tNear[lane]`, call
     `material->shade(...)` as today. If miss, return background.
  3. Aggregate the four `Colord` results.
  
  Falls back to four sequential `radiance(...)` calls if the packet
  intersect path is unavailable for any reason (mixed cull modes,
  composite primitives without packet overrides, etc.).
- **Result-shape glue.** `intersectPacket` only returns geometric
  intersection (which lane hit, at what distance). It does NOT return
  the hit primitive, surface normal, or material — the current
  scalar `Scene::intersect` populates `HitPointInterval` with all
  that. The packet path either:
  - calls a per-lane `Scene::intersectAt(lane)` to fill the
    per-lane `HitPointInterval` after the packet narrows the
    candidate set, OR
  - extends `RayPacketIntersection4` to carry the hit primitive
    pointer per lane (heavier but avoids the second walk).
  
  Decision belongs in this phase. Start with option (1) — fewer
  invasive type changes; only worth option (2) if measurement says
  the per-lane re-walk dominates.

Acceptance:

- A multi-sphere scene rendered through the packet path produces
  byte-identical output to the scalar path (gated by a parity test).
- `rendercli --timing` on the Phase 0 mesh scene shows the
  predicted speedup (capped at the BVH-fraction of wall time).
- The packet path is exercised by at least one fixture-rendered scene
  in the existing functional suite.
- Falls back cleanly to scalar in every other case.

## Phase 2 — shadow-ray packets (harder, optional)

**Goal:** the second-most-coherent ray class.

**Status:** still open. Packet frontiers cover camera/continuation path
intersections; direct-light shadow visibility remains scalar.

Shadow rays from a 2×2 hit cluster toward the same light are mostly
coherent (similar origins, similar directions). Batching them as a
Ray4 packet uses `BVH`'s shadow-test path with a `hitMask` return.

Tasks:

- **Material-side shadow-ray API.** Materials currently call
  `scene.intersects(shadowRay, state)` one ray at a time during
  shading. Need a new `RayCaster::shadowPacket(...)` that materials
  can opt into when the shading context provides one. Most materials
  don't need this; `MatteMaterial` and `PhongMaterial` do.
- **Deferred shading.** To batch shadow rays for a packet, you can't
  shade lanes immediately on hit — you have to:
  1. Gather hit points for all 4 lanes,
  2. Per-light, emit a 4-wide shadow Ray4,
  3. Distribute the resulting per-lane visibility back to the per-lane
     shade calls.
  
  This reorders the integrator's shape. Worth doing only if Phase 1
  measurement shows shadow rays are a meaningful fraction of cost
  (shadow-heavy scenes with one or two key lights, dozens of
  primitives).
- **Multi-light handling.** With N lights, scalar Whitted runs N
  shadow rays per hit, sequentially. Packet variant runs N Ray4
  packets per 2×2 cluster. The benefit scales with N; the
  bookkeeping does too.
- **Off-by-default flag.** `--raytracer.packet_shadows=on` until
  measurement justifies promoting to default.

Acceptance:

- Parity test confirms identical output with and without packet
  shadows on a multi-light scene.
- `--timing` shows speedup on a shadow-heavy scene; documented in
  the phase doc.
- Off by default; opt-in flag.

## Phase 3 — anti-aliasing samples as packets

**Goal:** the other obvious coherent ray cluster.

**Status:** not implemented as a direct `Camera::render` sample-loop fast path.
Multi-sample wavefront renders do submit batches through packet frontiers after
sample generation, but the original same-pixel four-sample `Ray4` entry point
does not exist.

When `samplesPerPixel > 1`, N samples per pixel share the same hit
point (for primary visibility) and very similar ray directions
(sub-pixel jitter only). A 4-sample tile = perfect Ray4 candidate.

Tasks:

- **Sample loop refactor.** `Camera::render`'s inner sample loop
  (`for sampleIndex = 0..samplesPerPixel`) currently runs sequentially.
  Replace the per-pixel sample loop with a sample-packet loop when
  `samplesPerPixel % 4 == 0` and the integrator advertises packet
  support.
- **Same `radiancePacket` interface as Phase 1.** Just feeds it
  4 same-pixel-different-jitter rays instead of 4 adjacent-pixel
  rays. Material divergence within a 4-sample packet is much rarer
  than across pixels — most samples within a pixel hit the same
  primitive — so this is a higher-quality packet than Phase 1's.
- **Compose with Phase 1.** If `samplesPerPixel = 4` and tile is
  2×2-friendly, you can pick *either* spatial packets (4 pixels × 1
  sample) or temporal packets (1 pixel × 4 samples). Which wins
  depends on coherence; the bench measures.

Acceptance:

- Parity test: 4-sample render matches scalar 4-sample render
  byte-for-byte.
- `--timing` on a 4-spp scene shows speedup over scalar 4-spp.

## Phase 4 — packetized scalar path inside BRDF (probably not)

**Goal:** explicitly NOT trying to do this; documenting why.

Reflective/refractive recursion past the first bounce diverges fast.
A "mirror sphere in front of a matte cube" scene has reflection rays
going in completely different directions per pixel — re-packeting
those would cost more in re-sort than it saves in traversal.

The "wavefront" approach used in pure path tracers (Aila & Laine,
Megakernel vs Wavefront) re-sorts rays into coherent buckets each
bounce. That's a different algorithm and a different integrator,
not a direct Whitted extension. The project now ships that sibling path:
`WavefrontRaytracer` drives `WhittedIntegrator::radianceBatch(...)` and
`PathTracingIntegrator::radianceBatch(...)`, and those batch implementations
use Ray4/Ray8 frontier intersections while keeping material evaluation scalar
where needed.

Decision unchanged unless somebody produces benchmark evidence that
specifically post-bounce packet re-sort wins on a real scene. Until
then, stay scalar past the first intersect.

## Test / CI

Required across Phases 1-3 for the remaining direct `Raytracer` packet path:

- **Parity tests** under
  `test/functional/engine/raytracer/PacketIntegratorParityTest.cpp`:
  render the Phase 0 scenes twice (scalar vs packet) and assert
  byte-identical output. Catches every regression the scalar path
  doesn't.
- **Cancellation tests.** `isCancelled()` is checked per-ray today;
  packet path checks once per packet. Add a test that cancels mid-tile
  and confirms the packet path stops cleanly without writing partial
  pixels.
- **State-divergence tests.** Pin behavior when one lane in a packet
  hits a primitive and another doesn't — the stats counters and AOV
  hit-point cache should agree with the scalar baseline lane-by-lane.
- **Benchmark on every change** to the packet integrator entry. Same
  rule as the existing perf-contract on Vector/Color SSE3
  specializations.

Already covered for the shipped batch/frontier path:

- Unit tests pin `WhittedIntegrator::radianceBatch(...)` Ray4/Ray8 packet
  frontier use, partial packets, continuation compaction, scalar refinement,
  and fallback metrics.
- Unit tests pin the same frontier packet behavior for
  `PathTracingIntegrator::radianceBatch(...)`.
- Wavefront tests and metrics JSON cover packet chunk counts, Ray4/Ray8 split,
  scalar tails, packet fallback reasons, and refined packet lanes.

## Open questions

- **`RayPacketIntersection4` shape.** Currently carries `hitMask`,
  `tNear`, `tFar`. Doesn't carry the hit primitive or material. Phase
  1's "result-shape glue" decision affects this — does the type
  grow, or does the packet path re-walk per lane? Probably re-walk
  for now; measure first.
- **`State` parallel array vs `PacketState`.** Four parallel `State`
  objects (`std::array<State, 4>`) is the obvious shape, but it
  multiplies the per-ray state allocation by 4. A SoA `PacketState`
  with parallel arrays inside saves allocation but reorganizes
  every state read. Start with `std::array<State, 4>` because it
  composes with the existing scalar fallback path trivially.
- **Composite primitives without packet overrides.** `Union`,
  `Intersection`, `ClosedSolidUnion`, `Difference` don't override
  `intersectPacket` today. The default falls back to four scalar
  calls — correct but loses the packet win. Phase 1 should
  measure whether scenes with CSG composites are common enough to
  justify per-Composite packet overrides; if yes, those are a
  follow-up.
- **Scalar fallback predicate.** What exactly forces the packet path
  back to scalar? Off the top of my head: pixel size > 1,
  samples-per-pixel > 1 (until Phase 3), non-Whitted integrator,
  rect dimensions odd on either axis, integrator's cancellation
  flag set. Inventory should be exhaustive — silent fallbacks are
  the bug that erases the headline speedup with no warning.
- **Educational impact.** The scalar `radiance(...)` is the
  educational reference. Adding `radiancePacket(...)` alongside it
  doubles the read surface for the integrator chapter. Plan: the
  textbook keeps pointing at the scalar version; the packet variant
  is described in a follow-up performance chapter as "what changes
  when you SIMD this." Trade-off is acceptable as long as the
  scalar code path stays the canonical one.
- **Direct-engine value after wavefront packets.** Phase 0 no longer kills the
  packet idea; packet frontiers shipped. The remaining question is narrower:
  whether a direct `Raytracer` camera/sample-loop packet path still earns its
  complexity now that the graph-visible wavefront path can consume the same
  scene through packet frontiers.
