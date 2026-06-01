# ARM SIMD backend plan - May 2026

> **Scope:** add native ARM/AArch64 SIMD support, primarily for Apple Silicon,
> parallel to the existing x86 SSE/SSE2/SSE3/AVX paths.
>
> **Status:** complete for the Epic #426 implementation slice. ARM NEON now
> supports the four-wide packet traversal surface through the shared
> `Float4`/`Mask4` backend, Phase 3-5 candidate work is benchmark-gated and
> documented, and wider/double-precision follow-ups remain future proposals
> rather than open work in this plan.
>
> **Related plans:** `docs/plans/complete/core-math-optimization.md` records the
> existing benchmark-first math work. The packet and BVH work here builds on the
> current `Ray4` and BVH packet traversal surfaces.

---

## Problem statement

The repository has several x86 SIMD optimizations, but native `arm64` builds do
not use them. On Apple Silicon, anything gated on `__SSE__`, `__SSE2__`,
`__SSE3__`, or `__AVX__` either falls back to scalar code or is not compiled.

That means ARM is missing more than small vector arithmetic:

- `Vector3f`, `Vector4f`, `Vector4d`, `Colorf`, `Colord`, and `Matrix4`
  specializations;
- the four-ray `BoundingBox::intersects4(Ray4)` packet test;
- `Sphere`, `Plane`, `Box`, and `Triangle` `Ray4` packet kernels;
- the BVH packet traversal's fast node-mask computation;
- all `Ray8` AVX-only paths.

The highest-value ARM SIMD target is the four-lane float packet path. AArch64
NEON uses 128-bit vectors, which maps directly to the existing `Ray4`
structure-of-arrays layout. It does not map directly to the eight-lane AVX
surface.

## Early evidence

A scratch benchmark on an M2 MacBook Pro compared scalar C++ and explicit NEON
for representative dot products. The fairer throughput version used four
independent accumulators to avoid a single dependency chain dominating the tiny
operation.

Median ns/op, lower is better:

| Operation | Compact scalar | Padded scalar | Explicit NEON |
|-----------|---------------:|--------------:|--------------:|
| `float3` dot, 8192-vector set | 0.4287 | 0.5623 | 0.3573 |
| `float4` dot, 8192-vector set | n/a | 0.6573 | 0.3687 |
| `double3` dot, 8192-vector set | 0.5365 | 0.7141 | 0.7082 |
| `double4` dot, 8192-vector set | n/a | 0.7216 | 0.7140 |
| `float3` dot, L1-sized set | 0.4346 | 0.4624 | 0.3066 |
| `float4` dot, L1-sized set | n/a | 0.6758 | 0.3061 |
| `double3` dot, L1-sized set | 0.4243 | 0.4652 | 0.4265 |
| `double4` dot, L1-sized set | n/a | 0.6637 | 0.4168 |

Takeaways:

- explicit NEON is promising for four-wide `float` work;
- `Vector3f` layout matters because compact scalar is 12 bytes while a SIMD
  representation is usually 16 bytes;
- `double3` is not a good first target;
- `double4` may help in compute-heavy cases, but needs repo-level benchmarks
  before it justifies another backend.

## Guiding rules

- Benchmark before and after each SIMD port.
- Prefer shared algorithms over duplicated SSE and NEON algorithm bodies.
- Keep `Ray4` as the first-class ARM packet width.
- Leave `Ray8` AVX-only until benchmarks justify a two-`Ray4` ARM path.
- Preserve scalar fallbacks for portability and for correctness comparisons.
- Do not change public semantics to chase SIMD. Differences in rounding,
  comparisons, masks, NaN handling, or color conversion need tests.
- Treat double-precision SIMD as suspect until measured in full repo workloads.
- Keep build selection compile-time for AArch64. Native Apple Silicon always has
  Advanced SIMD, so no runtime CPU dispatch is needed for the initial target.

## Phase 0 - inventory, gates, and baselines

~~Create a small SIMD configuration header with project-level feature macros:~~
✅ **Done.** `include/core/SimdFeatures.h` is the canonical header, and
existing x86 SSE/SSE3/AVX call sites now use these macros:

- `RAYTRACER_SIMD_SSE`
- `RAYTRACER_SIMD_SSE2`
- `RAYTRACER_SIMD_SSE3`
- `RAYTRACER_SIMD_AVX`
- `RAYTRACER_SIMD_NEON`
- `RAYTRACER_SIMD_AARCH64` for AArch64-only NEON intrinsics such as native
  vector division and square root

~~Use compiler macros such as `__SSE__`, `__SSE2__`, `__SSE3__`, `__AVX__`,
`__ARM_NEON`, and `__aarch64__` behind that one header. Existing code should
move away from directly testing architecture macros.~~ ✅ **Done.** The new
project macros map directly to the same compiler feature macros that gated the
old code paths.

~~Baseline before changing behavior:~~ ✅ **Done.**
`docs/perf/arm-simd-phase0-baseline-2026-05-28.md` records the current
baseline evidence and the exact capture commands.

- run `VectorBenchmark`, `MatrixBenchmark`, `BoundingBoxBenchmark`,
  `BatchedRayBenchmark`, `RayPacketBenchmark`, and `BVHPacketBenchmark` on
  Apple Silicon;
- record the same subset on an x86 SSE/SSE3 machine when possible;
- capture compiler vectorization remarks for current scalar ARM code where the
  result is ambiguous;
- add a short perf note under `docs/perf/` with hardware, compiler, command
  line, and benchmark filter.

Acceptance:

- one canonical place defines SIMD feature availability;
- the current ARM baseline is documented before any NEON code lands;
- no existing SSE/SSE3 behavior changes.

## Phase 1 - small 4-wide float abstraction

~~Add an internal 4-wide float API instead of copying every SSE algorithm into a
parallel NEON file. The first useful surface is enough for packet traversal:~~
✅ **Done.** `include/core/simd/Float4.h` now exposes the shared
`Float4`/`Mask4` backend with SSE, NEON, and scalar implementations for packet
traversal:

- `simd::Float4`;
- `simd::Mask4`;
- `zero`, `set1`, `loadAligned`, `storeAligned`;
- `add`, `sub`, `mul`, `div`, `sqrt`;
- `min`, `max`;
- `cmpEq`, `cmpNe`, `cmpGt`, `cmpGe`, `cmpLe`;
- `and`, `or`, `andNot`, `select`;
- `movemask`.

Backends:

- SSE implementation backed by `__m128`;
- NEON implementation backed by `float32x4_t`;
- scalar fallback backed by `std::array<float, 4>` for non-SSE/non-NEON builds
  and for tests.

The NEON `movemask` is the only awkward primitive. Implement it once in the
backend, then keep the packet algorithms architecture-neutral.

Acceptance:

- ~~unit tests cover arithmetic, comparison, select, and movemask behavior
  across all compiled backends;~~ ✅ **Done.** `Float4Test` covers the shared
  arithmetic, comparison, selection, and mask behavior.
- ~~at least one existing packet helper is migrated without a performance
  regression on x86.~~ ✅ **Done.** `BoundingBox::intersects4` uses the shared
  `Float4`/`Mask4` helper, and the migrated BVH coherent packet benchmark
  remains in line with the recorded x86 Phase 0 packet baseline.

## Phase 2 - Ray4 packet acceleration on ARM

Port the high-value four-ray float packet path through the new abstraction:

- ~~`BoundingBox<T>::intersects4(const Ray4&)`;~~ ✅ **Done.** The packet mask
  path uses `core::simd::Float4`/`Mask4`, with tests pinning zero-direction,
  parallel-axis, NaN, infinity, and movemask behavior against the scalar ray
  path.
- ~~`Sphere::intersectPacket(const Ray4&, State&)`;~~ ✅ **Done.** Uses the
  shared four-wide SIMD backend with scalar fallback.
- ~~`Plane::intersectPacket(const Ray4&, State&)`;~~ ✅ **Done.** Uses the
  shared four-wide SIMD backend with scalar fallback.
- ~~`Box::intersectPacket(const Ray4&, State&)`;~~ ✅ **Done.** Uses the shared
  four-wide SIMD backend with scalar fallback.
- ~~`Triangle::intersectPacket(const Ray4&, State&)`;~~ ✅ **Done.** Uses the
  shared four-wide SIMD backend with scalar fallback.
- ~~`BVH::intersectPacketNode` node-mask computation.~~ ✅ **Done.** Uses
  `core::simd::movemask` over the shared bounding-box packet mask.

The existing `Ray4` layout is already suitable for NEON: each coordinate is an
aligned four-float lane array. Avoid changing that layout unless a benchmark
proves the change pays for its blast radius.

Tests:

- keep the existing scalar-vs-packet correctness tests;
- expand architecture guards from `__SSE__` to the project SIMD feature macros;
- add NEON coverage for packet bounding-box masks and primitive hit masks;
- compare packet results against scalar `Rayd` results with float tolerances.

Benchmarks:

- `BatchedRayBenchmark`;
- `BoundingBoxBenchmark`;
- `RayPacketBenchmark`;
- `BVHPacketBenchmark`;
- one coherent-ray and one incoherent-ray BVH case.

The benchmark surface now includes explicit `BoundingBoxBenchmark`
`intersects4` packet entries. Record before/after ARM numbers for
`BoundingBoxBenchmark` and `BatchedRayBenchmark` once an Apple Silicon worker
is available for this phase.

Post-port x86 packet-kernel evidence is recorded in
`docs/perf/arm-simd-phase2-packet-kernels-2026-05-29.md`; ARM before/after
timing still needs an ARM runner.

Acceptance:

- native ARM `Ray4` packet benchmarks beat the scalar fallback by a meaningful
  margin;
- x86 packet benchmarks remain at least neutral;
- ARM packet results match scalar results lane-by-lane.

## Phase 3 - float math, color, and matrix follow-up

After packet acceleration, evaluate the single-object math types:

- `Vector3f`;
- `Vector4f`;
- `Colorf`;
- `Matrix4f` matrix-matrix and matrix-vector operations.

✅ **Done.** No Phase 3 NEON whole-type specialization is retained without ARM
before/after evidence. The decision and rerun filter are recorded in
`docs/perf/arm-simd-phase3-float-math-candidates-2026-05-30.md`; `ColorBenchmark`
now covers Colorf arithmetic and RGB packing for future ARM candidate runs.

The scratch benchmark suggests `Vector3f` and `Vector4f` can benefit from NEON,
but layout and call-site autovectorization matter. For `Colorf`, the arithmetic
port is straightforward, but the actual value depends on how hot color
modulation is in real renders.

Specific risks:

- SIMD `Vector3f` and `Colorf` are likely 16-byte objects, while compact scalar
  representations may be 12 bytes;
- existing SSE `Color::rgb()` conversion may not exactly match scalar casting
  semantics. Define and test the desired behavior before adding a NEON version;
- whole-type template specializations make ABI and include-order behavior
  architecture-dependent. Keep changes localized or consider a storage/backend
  trait before adding many more specializations.

Acceptance:

- `VectorBenchmark`, `MatrixBenchmark`, and representative render benchmarks
  show a real improvement before any Phase 3 NEON candidate is retained;
- ~~object size/alignment expectations are explicit in tests;~~ ✅ **Done.**
  `ColorTest`, `VectorTest`, and `MatrixTest` pin the current specialized and
  generic layouts.
- ~~color conversion behavior is pinned by tests.~~ ✅ **Done.** `ColorTest`
  covers midpoint quantization and overflow clamping before any NEON Colorf
  specialization lands.

## Phase 4 - double precision triage

~~Do not assume the double-precision SSE/SSE2 choices should be mirrored on
ARM. Initial evidence is mixed:~~ ✅ **Done.** Phase 4 keeps every
double-precision ARM candidate on the generic scalar/autovectorized path. No
NEON double path is retained without before/after Apple Silicon evidence and a
hot repo benchmark win; see
`docs/perf/arm-simd-phase4-double-precision-candidates-2026-05-30.md`.

- compact scalar `double3` is competitive or better than explicit NEON;
- `double4` can benefit in compute-heavy loops;
- single-ray `BoundingBox<double>::intersects` may already be well optimized by
  the compiler.

~~Evaluate these separately:~~ ✅ **Done.** Each candidate has an explicit
decision in the Phase 4 perf note:

- `BoundingBox<double>::intersects`;
- `BoundingBox<double>::intersect(Ray, Range&)`;
- `Vector4d`;
- `Matrix4d`;
- `Colord`.

Acceptance:

- each double-precision NEON path has before/after benchmark evidence; ✅
  **Done.** No double-precision NEON path was retained, so there is no retained
  path requiring before/after evidence.
- paths that do not win stay scalar; ✅ **Done.** All Phase 4 candidates remain
  on the ARM generic path.
- any retained double SIMD path improves a hot benchmark, not just a tiny
  isolated microbenchmark. ✅ **Done.** No candidate met this bar; future
  candidates must pair the focused microbenchmark with
  `AccelerationPolicyBenchmark` or another representative render benchmark.

## Phase 5 - Ray8 and wider packet policy

~~Keep `Ray8` AVX-only initially. AArch64 NEON is 128-bit, so an ARM `Ray8`
implementation would be two `Float4` chunks rather than one native vector. That
can still be useful for API symmetry or traversal batching, but it is a
separate algorithmic choice.~~ ✅ **Done.** `Ray8` stays AVX-only for Epic #426.
`docs/perf/arm-simd-phase5-ray8-policy-2026-05-30.md` records the benchmark
evidence: direct two-`Ray4` coherent traversal is already measurable through
`BVHPacketBenchmark`, and the available x86 AVX probe did not show a wider
`Ray8` traversal win.

Possible later work:

- keep `Ray8` operations AVX-only unless Apple Silicon benchmarks show a
  two-`Ray4` ARM wrapper materially beats direct two-`Ray4` calls;
- continue measuring whether larger packet traversal improves coherent BVH
  traversal enough to offset sparse masks and extra bookkeeping;
- consider a generic `RayPacket<N>` algorithm over backend vector chunks only
  if it changes traversal bookkeeping rather than adding API symmetry.

Acceptance:

- ~~no `Ray8` ARM work lands without BVH packet benchmark evidence;~~ ✅
  **Done.** The Phase 5 perf note records the evidence and keeps ARM `Ray8`
  unimplemented.
- ~~`Ray4` remains the primary ARM packet path.~~ ✅ **Done.** Phase 5 leaves
  ARM packet traversal centered on `Ray4`.

## Documentation and maintenance

✅ **Done for the Epic #426 implementation slice.**

- Comments and docs now reserve x86-specific names (`SSE`, `SSE2`, `SSE3`,
  `AVX`) for x86-only code and use "SIMD" for shared packet behavior that also
  covers ARM NEON.
- Architecture guards in maintained tests and benchmarks use
  `RAYTRACER_SIMD_*` project feature macros where practical; raw compiler
  macros are confined to `include/core/SimdFeatures.h` and the regression test
  that verifies that mapping.
- Benchmark evidence lives under `docs/perf/`; `docs/perf/README.md` indexes
  the ARM SIMD baseline, packet-kernel evidence, float/double candidate
  decisions, and Ray8 policy note.
- Project performance guidance now treats ARM NEON packet traversal as a
  supported optimization surface, with the same benchmark-before/after rule as
  the existing x86 SIMD paths.
- `CHANGELOG.md` records the shared SIMD backend, project feature gates, and
  Ray4 packet behavior changes.

## Out of scope

- runtime CPU dispatch between scalar and NEON on Apple Silicon;
- SVE/SVE2 support;
- GPU acceleration;
- rewriting public math APIs around a new SIMD library;
- changing renderer precision policy from the current float packet/double
  scalar split.
