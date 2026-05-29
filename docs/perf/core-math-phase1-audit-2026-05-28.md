# Core Math Phase 1 Benchmark Audit

Date: 2026-05-28

Epic: #361
Dependencies: #388, #389, #390, #391

This note ties together the measurement trail for core math optimization Phase
1. The raw benchmark captures remain the source of truth:

- Baseline: `docs/perf/math-baseline-2026-05-10.txt`
- Post-phase snapshot: `docs/perf/math-after-phase1-2026-05-17.txt`
- PRNG candidate comparison on Linux: `docs/perf/random-prng-2026-05-28.txt`
- Rejected `Vector3<double>` SIMD paths: `docs/perf/phase-2.3-vec3d-decision-2026-05-10.md`

## Executive Conclusion

Phase 1 has an auditable benchmark trail. The shipped changes are supported by
measurement, and the one optimization that missed its original target
(`BoundingBox::intersects`) is documented as a partial win rather than
overstated. Measurements also rejected several plausible alternatives:
`std::mt19937_64` for random numbers, a float-specific SSE bounding-box path,
and the legacy UB-dependent `Vector3<double>` SSE3 specialization family.

## Phase 1 Results

All times below are CPU nanoseconds unless noted. The baseline and post-phase
snapshot were both collected with the benchmark preset; the benchmark runner
reported background load on the post-phase run, so the table uses it as an audit
record, not as a precision tuning report.

| Item | Benchmark | Before | After | Conclusion |
|---|---:|---:|---:|---|
| 1.1 PRNG | `bm_random_unit<double>` | 6.29 ns | 2.64 ns | Faster on the Apple baseline/post run; the later Linux candidate comparison is stronger: PCG32 1.55 ns vs legacy `std::rand` 15.2 ns and `mt19937_64` 6.39 ns. |
| 1.1 PRNG threaded | `bm_random_threaded/threads:8` | 7.81 ns CPU | 17.2 ns CPU | The Apple post-run was noisy and not the deciding threaded measurement. The Linux comparison showed PCG32 1.74 ns CPU vs legacy `std::rand` 152 ns CPU at 8 threads. |
| 1.2 Bounding box | `bm_intersects<double>` | 769 ns, 332.7 M/s | 736 ns, 348.0 M/s | Small single-batch gain in the post snapshot. Per-PR 256-ray batch evidence showed 222 M/s -> 302-342 M/s; the original 3x target was not met. |
| 1.2 Bounding interval | `bm_intersect_interval<double>` | n/a | 44,875 ns, 222.8 M/s | New API surface measured in the post snapshot; it avoids recomputing slab math for BVH child sorting. |
| 1.3 SSE3 dot UB | `bm_dot<Vector3d>` | 0.443 ns | 0.452 ns | UB-free lane extraction stayed within noise (<5%) and preserved correctness. |
| 1.3 SSE3 dot UB | `bm_dot<Vector4d>` | 0.556 ns | 0.567 ns | Same conclusion for the 4D double specialization. |
| 1.4 HitPointInterval SBO | `bm_push_n<1>` | 24.1 ns | 2.77 ns | 8.7x faster and now uses inline storage for the common <=4-hit path. |
| 1.4 HitPointInterval SBO | `bm_push_n<2>` | 58.3 ns | 5.51 ns | 10.6x faster on the two-hit path. |
| 1.4 HitPointInterval SBO | `bm_push_n<4>` | 85.6 ns | 10.4 ns | 8.2x faster while still fitting inline storage. |
| 1.4 HitPointInterval SBO | `bm_single_hit_cycle` | 67.4 ns | 6.44 ns | 10.5x faster on the sphere-like hot path. |
| 1.5 Polynomial sorted result | `bm_quartic_sorted_result<float>` | 160 ns | 83.1 ns | 1.9x faster, with heap allocation removed from `sortedResult()`. |
| 1.5 Polynomial sorted result | `bm_quartic_sorted_result<double>` | 161 ns | 88.3 ns | 1.8x faster, with the same allocation removal. |

## Rejected Or Reverted Alternatives

- `Number::random` did not use `std::mt19937_64`: the candidate benchmark in
  `random-prng-2026-05-28.txt` measured PCG32 at about 1.55 ns for random
  doubles, versus 6.39 ns for thread-local `mt19937_64` and 15.2 ns for the
  legacy `std::rand` baseline.
- The float-specific SSE bounding-box path was removed after measurement showed
  it blocked compiler autovectorization of the surrounding ray loop. The shipped
  float path uses the generic branchless slab implementation.
- The `BoundingBox::intersects` rewrite did not satisfy the original 3x batch
  speedup gate on the 2.5 GHz build VM. The accepted conclusion is a measured
  partial win, about 1.4-1.5x for the documented double batch, plus the new
  interval-returning API.
- The `Vector3<double>` SSE3 specialization was later deleted rather than
  repaired. The decision benchmark showed that the old dot-product advantage was
  tied to UB type-punning; the UB-free SSE3 repair was slower than scalar, and
  the AVX2 candidate lost badly on cross product.

## Roadmap State

`docs/plans/complete/core-math-optimization.md` is archived and marks every
Phase 1 item as done. The audit status is now explicit:

- Phase 1.1: thread-local PCG32 shipped; `mt19937_64` rejected by benchmark.
- Phase 1.2: branchless/SSE2 bounding-box slab shipped; original 3x target not
  met and documented.
- Phase 1.3: SSE3 dot-product UB removed with benchmark-neutral results.
- Phase 1.4: `HitPointInterval` small-buffer optimization shipped with inline
  storage on the hot path.
- Phase 1.5: `Polynomial::sortedResult()` now returns bounded inline storage.
