# ARM SIMD Phase 5 Ray8 Policy

Purpose: record the benchmark-backed policy decision for Epic #426 Phase 5:
whether ARM should grow a `Ray8` implementation built from two `Ray4` chunks, or
whether `Ray8` should remain the native AVX-only packet width.

## Decision

Keep `Ray8` AVX-only. `Ray4` remains the primary ARM packet traversal width.

The available evidence does not justify adding an ARM `Ray8` API that internally
splits work into two `Ray4` traversals. That design would add another packet
surface and scalar-vs-packet test matrix without giving ARM a wider native SIMD
operation. The useful ARM operation is already measurable as two explicit
`Ray4` packet traversals over an eight-ray coherent tile, so callers can batch at
that level without a new `Ray8` implementation.

If this decision is revisited, the entry criteria are:

- Apple Silicon numbers from `BVHPacketBenchmark`, including
  `bm_bvh_scalar_coherent8` and `bm_bvh_packet4x2_coherent8`;
- scalar-vs-packet correctness coverage for any new ARM `Ray8` behavior;
- before/after `BVHPacketBenchmark` numbers showing the new `Ray8` surface beats
  the explicit two-`Ray4` path by enough to pay for the extra bookkeeping and
  maintenance.

## Benchmark Surface Added

`benchmarks/BVHPacketBenchmark.cpp` now includes:

- `bm_bvh_scalar_coherent8`, an eight-ray scalar coherent baseline available on
  every build;
- `bm_bvh_packet4x2_coherent8`, the hypothetical ARM `Ray8` chunking strategy
  expressed directly as two `BVH::intersectPacket(Ray4)` calls;
- the existing `bm_bvh_packet8_coherent`, still compiled only when
  `RAYTRACER_SIMD_AVX` is true.

## Syrus Worker SSE Run

This run executed on an x86_64 Syrus worker, not Apple Silicon:

```text
Linux x86_64, 4 vCPU under KVM
Compiler: c++ (Debian 12.2.0-14+deb12u1)
Benchmark binary: ./build/benchmark/benchmarks/benchmarks
Compile flags include: -O3 -funroll-loops -mtune=native -msse3
SIMD gates in these translation units: SSE=1, NEON=0, AVX=0
Google Benchmark load average: 17.32, 13.35, 9.83
```

Command:

```sh
cmake --preset benchmark
cmake --build --preset benchmark --target benchmarks
./build/benchmark/benchmarks/benchmarks \
  --benchmark_filter='bm_bvh_(scalar_coherent8|packet4x2_coherent8|scalar_coherent|packet4_coherent|primary_render_(scalar|packet4))$' \
  --benchmark_min_time=0.2s
```

Relevant results:

```text
bm_bvh_scalar_coherent            103395 ns CPU   39.6149M rays/s
bm_bvh_packet4_coherent            40471 ns CPU  101.208M rays/s
bm_bvh_primary_render_scalar     1134125 ns CPU   57.7855M rays/s
bm_bvh_primary_render_packet4     602447 ns CPU  108.783M rays/s
bm_bvh_scalar_coherent8           365299 ns CPU   22.4255M rays/s
bm_bvh_packet4x2_coherent8         87585 ns CPU   93.5318M rays/s
```

On this worker, the explicit two-`Ray4` path is about 4.17x faster than the
eight-ray scalar coherent baseline. That confirms the existing `Ray4` packet
path remains valuable when callers batch eight coherent rays, but it does not
show a reason to add a distinct ARM `Ray8` implementation.

## Syrus Worker AVX Probe

To compare the existing native `Ray8` path against the two-`Ray4` strategy, a
separate AVX-enabled build was configured:

```sh
cmake -S . -B build/benchmark-avx -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DRAYTRACER_BUILD_BENCHMARKS=ON \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_CXX_FLAGS=-mavx
cmake --build build/benchmark-avx --target benchmarks
./build/benchmark-avx/benchmarks/benchmarks \
  --benchmark_filter='bm_bvh_(scalar_coherent8|packet4x2_coherent8|packet8_coherent|scalar_coherent|packet4_coherent)$' \
  --benchmark_min_time=0.2s
```

Relevant results:

```text
bm_bvh_scalar_coherent             98966 ns CPU   41.3878M rays/s
bm_bvh_packet4_coherent            46143 ns CPU   88.7668M rays/s
bm_bvh_scalar_coherent8           391172 ns CPU   20.9422M rays/s
bm_bvh_packet8_coherent           384262 ns CPU   21.3188M rays/s
bm_bvh_packet4x2_coherent8         87431 ns CPU   93.6968M rays/s
```

The existing AVX `Ray8` traversal was effectively neutral against the eight-ray
scalar baseline on this benchmark and far slower than the explicit two-`Ray4`
path. That is not ARM evidence, but it does show that wider packet traversal is
not automatically better for this BVH implementation: sparse masks, scalar node
tests, leaf fallback behavior, and extra bookkeeping can erase the coherence
benefit.

## Follow-up Trigger

Reopen Phase 5 only if an Apple Silicon run shows an implementation-level
two-`Ray4` `Ray8` wrapper materially beats direct two-`Ray4` calls, or if a
future generic packet traversal changes the bookkeeping enough that wider
packets become a different algorithm rather than API symmetry.
