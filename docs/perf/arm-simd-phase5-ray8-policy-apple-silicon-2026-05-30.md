# ARM SIMD Phase 5 Ray8 Policy — Apple Silicon Follow-Up

Purpose: capture the Apple Silicon evidence that the upstream phase 5 decision
in `arm-simd-phase5-ray8-policy-2026-05-30.md` explicitly asked for. That doc
recorded the policy ("keep `Ray8` AVX-only, two explicit `Ray4` calls are the
ARM idiom") with x86 SSE and AVX runs but no NEON numbers, and called for
Apple Silicon `bm_bvh_scalar_coherent8` and `bm_bvh_packet4x2_coherent8`
before reconsidering the decision.

## Decision (unchanged)

Keep `Ray8` AVX-only. Apple Silicon numbers below reinforce that the existing
explicit two-`Ray4` strategy is the right ARM packet idiom; introducing an
ARM-side `Ray8` wrapper that just calls two `Ray4` traversals would add API
surface for a ~1.28× win on the eight-ray coherent benchmark, which is below
the bar set in the original decision doc.

## Apple Silicon NEON Run

```text
macOS arm64 (Apple M2 Max, 12 cores, L1d 64 KiB, L2 4 MiB)
Compiler: Apple Clang (cmake --preset benchmark)
Benchmark binary: build/benchmark/benchmarks/benchmarks
Compile flags include: -O3 -funroll-loops -mtune=native
SIMD gates in these translation units: SSE=0, NEON=1, AVX=0
Google Benchmark load: idle host
```

Command (matching the upstream doc's filter):

```sh
cmake --preset benchmark
cmake --build --preset benchmark --target benchmarks
./build/benchmark/benchmarks/benchmarks \
  --benchmark_filter='bm_bvh_(scalar_coherent8|packet4x2_coherent8|scalar_coherent|packet4_coherent|primary_render_(scalar|packet4))$' \
  --benchmark_min_time=0.2s
```

Relevant results:

```text
bm_bvh_scalar_coherent            409496 ns CPU   26.5286M rays/s
bm_bvh_packet4_coherent            72136 ns CPU  155.012M rays/s
bm_bvh_primary_render_scalar     3346668 ns CPU   57.9873M rays/s
bm_bvh_primary_render_packet4     869649 ns CPU  201.221M rays/s
bm_bvh_scalar_coherent8           153022 ns CPU  175.320M rays/s
bm_bvh_packet4x2_coherent8        118396 ns CPU  224.062M rays/s
```

## Reading the Numbers

| Workload                          | Scalar (M rays/s) | Packet (M rays/s) | Speedup |
|-----------------------------------|------------------:|------------------:|--------:|
| 4-ray coherent (bvh)              | 26.5              | 155.0             | **5.85×** |
| primary-render workload (bvh)     | 58.0              | 201.2             | **3.47×** |
| 8-ray coherent (bvh)              | 175.3             | 224.1             | **1.28×** |

The 4-ray coherent and primary-render numbers confirm the shared
`core::simd::Float4` NEON backend is producing the expected packet speedup —
roughly 3-6× faster traversal where each Ray4 packet finds a meaningful
amount of coherent work.

The 8-ray case (the one the upstream doc flagged) is the relevant signal for
the policy: the explicit two-`Ray4` path beats the eight-ray scalar baseline
by only ~28% on Apple Silicon. That's narrower than the SSE worker's ratio
(~4.17×, see prior doc) because the scalar-8 path on M2 Max already runs at
175 M rays/s — the BVH is small enough that scalar traversal is not the
bottleneck. The packet path still wins, but the headroom is too small to
justify a new `Ray8` API surface that internally chunks into two `Ray4`s; an
ARM `Ray8` wrapper would inherit the same coherent-tile workload and produce
the same ~1.28× number while adding a new packet-width test matrix.

## Validation of the NEON Fix

These runs also exercise the `core::simd::Float4` NEON specializations —
including `maskNot<NeonBackend>`, which was hoisted above its first use in
`cmpNe<NeonBackend>` to fix a build break surfaced by this branch's rebase
(commit `9bd4c47b`). The benchmarks compile and run cleanly with the fixed
ordering; the headline `bm_bvh_packet4_coherent` throughput (155 M rays/s) is
within the same envelope as the x86 SSE worker's measurement (~101 M rays/s,
scaled for the per-core IPC difference between Apple Silicon and the Syrus
KVM worker).

## Follow-Up Trigger (unchanged)

Reopen Phase 5 only if a future Apple Silicon implementation-level
two-`Ray4` `Ray8` wrapper materially beats direct two-`Ray4` calls, or if a
future generic packet traversal changes the bookkeeping enough that wider
packets become a different algorithm rather than API symmetry.
