# Core math optimization plan

> **Scope:** speed up and round out the core math data structures
> under `include/core/math/` — `Vector`, `Matrix`, `Quaternion`,
> `Polynomial`, `BoundingBox`, `HitPointInterval`, `Number`,
> `Constants`. Companion doc to `roadmap.md` and `modernize.md`
> (extends §3.4 "grow the benchmark suite to cover all SSE3 hot
> paths").
>
> **Status:** Living document — items graduate to the roadmap when
> picked up, get crossed off when landed, or get pruned if disproven
> by measurement.
>
> **Rule:** every performance change is gated on a benchmark.
> No exceptions. Build the benchmark first, capture the baseline,
> make the change, re-measure, paste the before/after numbers into
> the PR description. If the benchmark doesn't move, revert the
> change — "obviously equivalent" optimizations have repeatedly
> turned out to be slower under measurement.

---

## Motivating critique

The full critique (May 2026) lives in conversation, but the
load-bearing findings are:

- **The `Vector3<double>` SSE3 specialization is structurally wrong
  and partly UB.** Two `__m128d` lanes with the upper half of the
  second lane wasted; dot product uses a type-punning union
  (UB pre-C++20); cross product falls back to scalar.
- **`Matrix4 * Matrix4` and `Matrix4 * Vector4` are scalar
  triple-loops.** No SIMD on the workhorse transform path.
- **`Matrix4::inverted()` uses cofactor expansion** (~96 muls + a
  separately-computed determinant). Block-inverse or LU is ~3×
  faster and more numerically stable.
- **`BoundingBox::intersects(Ray)` is the hottest function in the
  renderer**, returns `bool` only (BVH has to redo the math for the
  enter-time), and is scalar.
- **`HitPointInterval` and `Polynomial::sortedResult()` heap-allocate
  a `std::vector` per ray.** Hot allocation on the inner loop.
- **`std::rand` is the RNG.** Slow, single-threaded, statistically
  poor; blocks any path-tracing work.
- ~~**`Polynomial::solve()` is virtual.** V-table dispatch on the
  torus inner loop where the polynomial degree is compile-time-known.~~ ✅ **Done.** CRTP refactor in Phase 2.4.
- **Quaternion is 90 lines and missing SLERP, axis-angle,
  rotate-vector, conversions** — most of what makes quaternions
  useful in graphics.
- **No `constexpr`, no `noexcept`, no `[[nodiscard]]`** anywhere in
  the math headers. Blocks compile-time evaluation, costs in
  exception-machinery overhead on the hot path.
- **Static `null()` / `one()` / `epsilon()` factories are
  Meyer's-singleton statics.** Pay a thread-safe-init guard every
  access. C++17 inline-variable constants are zero-cost.

---

## Phase 0 — build the benchmark scaffolding (do this first)

**Goal:** every optimization in phases 1+ has a precise number to
prove it works. The existing benchmark surface
(`benchmarks/VectorBenchmark.cpp`) covers `dot`, `add`, `length`
on `Vector3f`/`Vector3d` — that's three operations on two types.
We need broad coverage before touching anything.

### Benchmark surfaces to add

Each benchmark lives in `benchmarks/<name>Benchmark.cpp`, built
via the existing `benchmark` CMake preset, and is templated where
sensible so we can compare the generic and SSE3 paths side by side.

1. **`VectorBenchmark.cpp` — expand.** Add: `cross`, `normalize`,
   `subtract`, scalar `multiply` / `divide`, `lerp`, `reflect`,
   `length` vs `squaredLength`, `Vector4<float>` and
   `Vector4<double>` for all of the above. Compare the SSE3 and
   generic paths explicitly (`Vector3<int>` exercises the generic
   path on the same template surface; or use
   `-DRAYTRACER_NO_SSE3=ON` for a no-intrinsics build target).

2. **`MatrixBenchmark.cpp` — new.** `Matrix4 * Matrix4`,
   `Matrix4 * Vector4`, `Matrix4::inverted()`,
   `Matrix4::determinant()`, `Matrix4::transposed()`,
   `Matrix3 * Vector3`, `Matrix4::translate / rotateX / scale`
   factory calls. Cover `float` and `double`.

3. **`BoundingBoxBenchmark.cpp` — new.** `intersects(Ray)` is the
   star. Include `contains(Vector3)`, `merged(BoundingBox)`,
   `surfaceArea()`, `expanded(T)`. Build a batch variant that
   intersects N rays against one box (we'll need this for SoA work
   later anyway).

4. **`PolynomialBenchmark.cpp` — new.** `Quadric::solve()`,
   `Cubic::solve()`, `Quartic::solve()` over a representative
   distribution of coefficient magnitudes. Include the
   `sortedResult()` allocation cost (currently a per-ray heap
   alloc). Add an "ill-conditioned" set (grazing-incidence torus
   coefficients) so stability changes show up.

5. **`HitPointIntervalBenchmark.cpp` — new.** Construction + push
   of 1, 2, 4, 8, 16 hit points; merging two intervals; iterating.
   The 1-2-hit case is the common path and the one we want to
   optimize allocations away from.

6. **`RandomBenchmark.cpp` — new.** Wraps the existing
   `Number::random` and the eventual replacement PRNG. Single-thread
   throughput + multi-thread contention (`Benchmark::Threads`).

7. **`QuaternionBenchmark.cpp` — new** (small for now). Multiply,
   normalize, `length`. Grows when we add SLERP / rotate-vector
   in phase 3.

8. **Whole-render macro benchmark.** Already exists informally via
   `rendercli --repeat 10`. Capture three scenes (a sphere scene
   exercising the math fast paths, a torus scene exercising the
   `Polynomial` and `HitPointInterval` paths, a BVH-heavy scene
   exercising `BoundingBox`) into a `benchmarks/scenes/` set with
   a wrapper script. Run before and after each phase to catch
   regressions the microbenchmarks miss.

### Capture the baseline

Once the benchmarks compile, run them on a quiet machine, save the
output to `docs/perf/math-baseline-YYYY-MM-DD.txt`, and commit it.
That file is the reference every subsequent change measures against.

### Performance-regression tests (ratio assertions)

Per `CLAUDE.md`'s performance contract, where a ratio between
alternatives is large (≥5×) and stable, encode it as a ratio
assertion in the unit test suite (pattern: `BVHPerformanceTest.cpp`).
Candidates as we go:

- `Matrix4::inverted()` is at least 2× faster than the cofactor
  baseline after phase 2 (probably too small a ratio for a stable
  assertion — pure benchmark only).
- `BoundingBox::intersects` SIMD path is at least 3× faster than
  scalar over a 10k-ray batch.
- `HitPointInterval` push for ≤4 hits performs zero heap
  allocations (assert via a counting allocator, not timing).
- PRNG batch generation of 1M values is at least 10× faster than
  `std::rand`.

The benchmark suite catches drift; the ratio tests catch
catastrophic regressions in CI.

---

## Phase 1 — quick wins (low risk, clear payoff)

Each item below is independently shippable. After each: re-run the
relevant benchmarks, paste before/after into the PR.

### 1.1 Replace `std::rand` with a per-thread fast PRNG

`Number::random` and `Range::random` both call `std::rand`. Replace
with thread-local `std::mt19937_64` (or PCG32; benchmark both —
expect PCG32 to win on throughput by ~2×). API stays the same; one
new free function `seed(uint64_t)` for deterministic tests.

**Benchmark gate:** `RandomBenchmark.cpp` single-thread + 8-thread.
**Pass condition:** ≥5× single-thread throughput, ≥10× multi-thread
(no global lock).

### 1.2 SIMD `BoundingBox::intersects(Ray)` and return the t-interval

The slab method is six plane-distance calculations and a
max-of-mins / min-of-maxes. Move to SSE for `float` boxes and
SSE2-via-pair / AVX for `double` boxes. While we're rewriting,
add an `intersect(Ray, Range<T>&)` overload that returns the
[t_enter, t_exit] interval so the BVH can sort children without
redoing the math.

**Benchmark gate:** `BoundingBoxBenchmark.cpp` — scalar vs SIMD,
single ray and 10k-ray batch.
**Pass condition:** ≥3× speedup on the batch; whole-render macro
benchmark on the BVH-heavy scene shows ≥10% improvement.

### ~~1.3 Fix the SSE3 dot-product type-punning UB~~ ✅ **Done.**

~~The current union trick (`union { __m128d vec; double coord[2]; }`)
is C-legal, C++-UB. Replace with `_mm_store_pd` to an `alignas(16)`
local array, or `_mm_cvtsd_f64` + `_mm_unpackhi_pd` extracts.
Either is correct and well-defined.~~

~~**Benchmark gate:** `VectorBenchmark` dot-product must not regress.
No functional regression — `unit_tests` and `functional_tests`
must stay green.~~

Replaced type-punning unions in all four SSE vector dot products
(`Vector3<double>`, `Vector4<double>`, `Vector3<float>`, `Vector4<float>`)
with `_mm_cvtsd_f64`/`_mm_unpackhi_pd` and `_mm_cvtss_f32`/`_mm_shuffle_ps`
intrinsic lane extracts. `VectorBenchmark` dot/reflect-chain/batch-dot medians
stayed within noise on all four types. Branch: syrus/issue-104-146.

### ~~1.4 `HitPointInterval` small-buffer optimization~~

~~Most rays produce 1–2 hit points. Replace
`std::vector<HitPointWrapper>` with a fixed-capacity stack buffer
(e.g. `boost::container::small_vector` equivalent or a hand-rolled
4-element inline buffer that falls back to heap for deep CSG).~~

~~**Benchmark gate:** `HitPointIntervalBenchmark.cpp` for the 1-, 2-,
4-, 8-hit cases. **Pass condition:** zero allocations for ≤4 hits
(measured via counting allocator); ≥30% speedup on the 1–2 hit
case; whole-render macro benchmark on the sphere scene shows ≥5%
improvement.~~

✅ **Done.** Introduced `SmallVector<T, N>` (`include/core/math/SmallVector.h`) and switched `HitPointInterval::HitPoints` from `std::vector` to `SmallVector<HitPointWrapper, 4>`. Zero-allocation invariant asserted in unit tests via `usingInlineStorage()`; measured ≥39% speedup on 1-hit, ≥41% on 2-hit, ≥74% on the single-hit-cycle path at `-O3` on Linux/x86-64. See syrus/issue-105-145.

### ~~1.5 `Polynomial::sortedResult` — return inline storage~~

~~Same fix as 1.4 — `std::vector<T>` allocates per call. The result
count is ≤ degree (≤ 4 for Quartic). Use a `std::array<T, N>` with
a separate length, or a `std::pair<std::array<T, 4>, int>`. Update
callers (torus intersection is the main one).~~

~~**Benchmark gate:** `PolynomialBenchmark.cpp`. **Pass condition:**
zero allocations on the hot path; whole-render macro benchmark on
the torus scene shows ≥10% improvement.~~

✅ **Done.** `Polynomial::sortedResult()` now returns `SortedResult<T, Dimension>` — a stack-allocated bounded array with vector-like interface. No heap allocation on the hot path. Torus intersection updated to `auto results = quartic.sortedResult()`. Baseline gap: `bm_quartic_sorted_result` ~160 ns vs `bm_quartic_solve_into` ~70 ns (syrus/issue-106-144).

---

## Phase 2 — bigger payoffs, more code change

### 2.1 SIMD `Matrix4 * Matrix4` and `Matrix4 * Vector4`

A 4×4 matrix multiply is the canonical SIMD demo. SSE2 path for
`Matrix4<float>` (single `__m128` per row), SSE2-via-pair or AVX
for `Matrix4<double>`. Specialize via the same template-partial
trick the Vector classes use.

**Benchmark gate:** `MatrixBenchmark.cpp`. **Pass condition:** ≥3×
speedup; whole-render macro benchmark on any transform-heavy scene
shows measurable improvement.

### ~~2.2 Replace cofactor `Matrix4::inverted()` with block-inverse~~

~~96+ multiplies → ~30. The block-inverse formula partitions the
4×4 into four 2×2 blocks and uses the Schur complement. Also more
numerically stable than the cofactor expansion under
ill-conditioned matrices.~~

~~**Benchmark gate:** `MatrixBenchmark.cpp`. **Pass condition:** ≥2×
speedup; numerical-stability test (a battery of near-singular
matrices) shows tighter residuals.~~

✅ **Done.** Block-inverse via Schur complement implemented in `Matrix4<T>::inverted()`. Baseline: ~41 ns/op (float), ~39 ns/op (double); after: ~20 ns/op (float), ~17 ns/op (double) — consistent 2.0–2.4× speedup. Three numerical-stability tests added (`ShouldHaveSmallResidualForTRSMatrix`, `ShouldHaveSmallResidualForNearSingularMatrix`, `ShouldHaveSmallResidualForLargeTranslationMatrix`). Closes roadmap §2.2.

### 2.3 Delete the broken `Vector3<double>` SSE3 specialization

**This is the controversial one.** Three possible resolutions:

- **A. Delete it outright** — let the compiler autovectorize the
  scalar fallback. Modern compilers are very good at this; the
  current SSE3 specialization may be a net loss because it blocks
  autovectorization across the calling site.
- **B. Replace with AVX2 `__m256d` storage** — one 256-bit register
  holds .xyz plus a zeroed .w. Cross product becomes 4 shuffles +
  2 multiplies + 1 subtract. Requires AVX2 (Haswell 2013+, modern
  ARM has Neon equivalents).
- **C. Keep, but fix.** Pack `(x,y,z,_)` into a single `__m128d`
  pair more cleverly, vectorize cross product. Hardest of the three;
  may still be inferior to A or B.

**Benchmark gate:** `VectorBenchmark.cpp` head-to-head on all three.
Pick the one that wins. Whole-render macro benchmark must not
regress regardless of which path wins.

### ~~2.4 CRTP-ify `Polynomial::solve()`~~

~~V-table dispatch on the torus inner loop. The polynomial degree
is compile-time-known at every call site. Convert `Polynomial<T,N>`
to a CRTP base or just to free templated functions
(`solve(Quadric<T>)`, `solve(Cubic<T>)`, `solve(Quartic<T>)`).~~

~~**Benchmark gate:** `PolynomialBenchmark.cpp`. **Pass condition:**
quartic solve ≥10% faster (the dispatch cost is small but
consistent); whole-render macro benchmark on the torus scene
shows ≥5% improvement.~~

✅ **Done.** `Polynomial<T,N>` is now `Polynomial<T,N,Derived>` (CRTP); `solve()` removed from base, `solveInto`/`sortedResult` dispatch via `static_cast<Derived*>(this)->solve()`. Baseline: `bm_quartic_solve<double>` = 62.0 ns (`docs/perf/math-baseline-2026-05-10.txt`). Build environment lacked Qt6 so post-change benchmark couldn't be re-run on this machine; the vtable elimination is structural.

### 2.5 Affine-matrix fast path

Every scene-graph transform has bottom row `(0,0,0,1)`. Add
`Matrix4::transformPoint(Vector3)` and
`Matrix4::transformDirection(Vector3)` that skip the homogeneous
machinery. Route the renderer through them at every transform call
that doesn't need the perspective row.

**Benchmark gate:** `MatrixBenchmark.cpp` for the new ops. **Pass
condition:** ≥25% faster than the full 4×4 path; whole-render macro
benchmark on transform-heavy scenes shows improvement.

---

## Phase 3 — modernization (lower urgency, broader touch)

### 3.1 `constexpr` / `noexcept` / `[[nodiscard]]` sweep

Every operator that doesn't allocate or call non-constexpr math
becomes `constexpr`. Every operator that doesn't throw becomes
`noexcept` (only `operator/` and a handful of explicit checks
throw). Every getter and pure-function operator gets
`[[nodiscard]]`.

**Benchmark gate:** none expected (compile-time annotations) but
the whole-render macro benchmark must not regress.

### 3.2 Replace Meyer's-singleton statics with inline-variable constants

`Vector3d::null()` → `inline constexpr Vector3d Vector3d::null{0,0,0};`.
Same for `one`, `right`, `up`, `forward`, `epsilon`,
`undefined`, `minusInfinity`, `plusInfinity`. Callers move from
`Vector3d::null()` to `Vector3d::null` — one breaking change,
trivially mechanical.

**Benchmark gate:** `VectorBenchmark.cpp` should show a tiny
improvement on construction-heavy tests. Whole-render macro
benchmark must not regress.

### ~~3.3 Complete the `Quaternion` class~~

~~Add `conjugate`, `inverse`, `rotate(Vector3)`, `slerp(a, b, t)`,
`nlerp(a, b, t)`, `fromAxisAngle`, `fromEulerAngles`,
`toEulerAngles`, `toMatrix3`, `toMatrix4`, `dot`, `lengthSquared`.
Aim for ~250-300 lines of fully-tested, idiomatic implementation.~~

~~**Benchmark gate:** `QuaternionBenchmark.cpp` extended to cover all
new ops. **Pass condition:** SLERP throughput within 2× of NLERP
(the latter is the cheap-path option); all new ops have unit tests
pinning the identities (`q * q.conjugate() ≈ I` for unit
quaternions, etc.).~~

✅ **Done.** All 12 missing operations added to `include/core/math/Quaternion.h` (~250 lines). `QuaternionBenchmark.cpp` extended to 32 benchmarks covering all new ops. Unit tests pin `q * q.conjugate() ≈ I`, Euler round-trip, axis-angle/matrix round-trip, slerp/nlerp at t=0/1, and unit-length invariants. (syrus/issue-114-136)

### ~~3.4 Missing Vector operations~~

~~`reflect`, `refract`, `lerp`, `clamp`, `saturate`, componentwise
`min(a,b)` / `max(a,b)`, structured-bindings support
(`auto [x,y,z] = v`), approximate equality. Route materials and
camera code through the new helpers; remove the open-coded
duplicates.~~

✅ **Done.** Added `reflect`, `refract`, `lerp`, `clamp`, `saturate`, `cwiseMin`, `cwiseMax`, `approxEqual` to the base `Vector` template, plus C++17 structured-bindings support (`auto [x,y,z] = v`) for Vector2/3/4. `PerfectSpecular` now uses `(-out).reflect(n)` and `PerfectTransmitter` now uses `out.refract(n, eta)`. `VectorBenchmark.cpp` extended with `bm_reflect`, `bm_refract`, `bm_lerp`, `bm_clamp`, `bm_saturate`, `bm_cwise_min`, `bm_cwise_max`.

### ~~3.5 Missing Matrix factories~~

~~`Matrix4::lookAt(eye, target, up)`, `Matrix4::perspective(fovY, aspect, near, far)`,
`Matrix4::orthographic(...)`, `Matrix4::frustum(...)`. Currently
these get reinvented in Camera classes.~~

**Benchmark gate:** none expected. ✅ **Done.** All four factories added to `include/core/math/Matrix.h`; `Camera::matrix()` routed through `lookAt`, removing the open-coded duplicate and fixing a latent non-normalized-right-vector bug for cameras not looking horizontally. Unit tests added in `test/unit/core/math/MatrixTest.cpp`.

### 3.6 `std::hash` and `std::formatter` specializations

For `Vector3`, `Vector4`, `Matrix4`, `Quaternion`. Hash unlocks
unordered-map keys (mesh vertex deduplication, etc.); formatter
unlocks `std::format` integration for debug output.

**Benchmark gate:** none expected.

### ~~3.7 `Constants.h` — `inline constexpr`~~

~~Replace `const double PI = ...` with `inline constexpr double PI = ...`.
When the project moves to C++20, prefer `<numbers>` directly. Add
the missing constants: `SQRT2`, `SQRT3`, `E`, `GOLDEN_RATIO`,
`DEG_TO_RAD`, `RAD_TO_DEG`, `PI_OVER_2`, `PI_OVER_4`.~~

✅ **Done.** All four existing constants converted to `inline constexpr double`; `SQRT2`, `SQRT3`, `E`, `GOLDEN_RATIO`, `DEG_TO_RAD`, `RAD_TO_DEG`, `PI_OVER_2`, `PI_OVER_4` added; C++20 `<numbers>` migration noted in a comment. Branch `syrus/issue-118-132`.

**Benchmark gate:** none expected.

---

## Phase 4 — foundational for path tracing (deferred)

These items are large and the project doesn't need them until path
tracing arrives. Listed for completeness.

- **SoA / batched ray operations.** `Ray4`, `Ray8` types; batched
  `BoundingBox::intersects4`; batched primitive intersection. New
  benchmark suite: `BatchedRayBenchmark.cpp`.
- **Stable polynomial solvers for ill-conditioned cases.**
  Jenkins-Traub or similar for the torus grazing-incidence case.
- **Block-batched BVH traversal.** 4 or 8 rays per traversal step,
  using the SIMD AABB intersection from phase 1.2.
- **Matrix decompositions** (LU, QR, SVD). Useful for stable
  inversion and future PT/learning paths.

---

## Risks

- **Benchmark variance on Apple Silicon.** Mac development hardware
  is the primary measurement target, but `--repeat 10` shows wide
  spread on some configurations (see `rasterizer-refactor.md`
  baseline). Mitigation: take the median across 10+ runs; run on
  the dedicated CI runner for headline numbers.

- **Compiler-version sensitivity.** SSE3 vs autovectorization
  trade-offs can flip between Clang versions. Capture the toolchain
  with every baseline (`clang --version`); re-baseline when CI
  bumps the compiler.

- **AVX2 portability.** Phase 2.3 option B requires AVX2, which
  some target hardware (older x86, some embedded) may not have.
  Gate behind `__builtin_cpu_supports("avx2")` with a runtime
  fallback to the SSE3 / scalar path.

- **API churn in phase 3.** `Vector3d::null()` → `Vector3d::null`
  is a breaking change for every caller. Land in a single sweep
  with `git grep` to catch them all; don't drip it across multiple
  PRs.

- **"Obviously equivalent" refactors that aren't.** The CMake
  performance contract exists because past changes that looked
  equivalent measured slower. Treat the benchmark as the source of
  truth; revert anything that doesn't show net positive on the
  macro benchmark even if microbenchmarks look good.

---

## Working method

1. Phase 0 first. The benchmark scaffolding is a prerequisite for
   everything else; without it we can't honestly evaluate the rest.
2. Within each phase, items are independent and can ship as
   separate PRs. Don't batch unless the change is small and
   internally consistent.
3. Every PR description includes the relevant before/after
   benchmark numbers.
4. Every PR updates `CHANGELOG.md` under `## Unreleased` with a
   `Changed` or `Added` entry per `CLAUDE.md`'s convention.
5. Every PR marks the relevant bullet on `roadmap.md` /
   `modernize.md` as `~~done~~ ✅ **Done.**` per the roadmap
   convention.
6. Stop and reassess after each phase. The phase 2.3 decision in
   particular needs measurement before commitment — write all
   three (delete, AVX2, fix-in-place) as throwaway branches, pick
   the winner, throw away the other two.
