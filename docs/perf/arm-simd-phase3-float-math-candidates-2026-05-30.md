# ARM SIMD Phase 3 Float Math Candidate Decision

Purpose: record the Epic #426 Phase 3 decision for single-object float math
surfaces after the Ray4 packet backend landed.

## Candidate Status

No new NEON specialization is retained by this change.

The current Apple Silicon baseline for the relevant math benchmarks is still
the Phase 0 generic/autovectorized run in
`docs/perf/math-baseline-2026-05-10.txt`:

```text
bm_matmul<float, 4, Matrix4f>               5.52 ns       5.50 ns
bm_mat_vec<float, 4, Matrix4f, Vector4f>    1.56 ns       1.55 ns
bm_dot<Vector3f>                           0.442 ns      0.442 ns
bm_dot<Vector4f>                           0.551 ns      0.550 ns
```

Those numbers are already very small for the single-object surfaces. Without an
Apple Silicon after-run proving a meaningful win in `VectorBenchmark`,
`MatrixBenchmark`, and a representative render benchmark, the Phase 3 rule says
the candidates stay scalar/generic on ARM.

`Colorf` is also intentionally left without a NEON specialization. The x86 SSE
`Colorf::rgb()` path uses packed SIMD conversion, while the generic template
uses scalar casts through `rInt()`/`gInt()`/`bInt()`. Midpoint values therefore
have observable rounding-versus-truncation differences. `ColorTest` now pins
the compiled behavior before any future ARM color port chooses a conversion
contract.

## Benchmark Surface

This change adds `ColorBenchmark.cpp` so future ARM runs can measure:

```text
bm_color_add<Colorf>
bm_color_scalar_mul<Colorf>
bm_color_modulate<Colorf>
bm_color_rgb<Colorf>
bm_color_modulate_batch<Colorf>
```

The existing Phase 3 benchmark filters remain:

```sh
./build/benchmark/benchmarks/benchmarks \
  --benchmark_filter='bm_(dot|add|sub|scalar_mul|scalar_div|length|squared_length|normalize|cross|reflect|refract|lerp|clamp|saturate|cwise_min|cwise_max|dot_batch)<Vector(3f|4f)>|bm_(matmul|mat_vec)<float, 4|bm_(color_add|color_scalar_mul|color_modulate|color_rgb|color_modulate_batch)<Colorf>|bm_bvh_primary_render_(scalar|packet4)' \
  --benchmark_min_time=0.1s
```

Run that command on Apple Silicon before landing any future Phase 3 NEON
candidate. Keep the candidate only if the ARM after-run shows a real improvement
in the relevant microbenchmark and does not regress the representative render
benchmark.

## Syrus Worker Smoke Run

This run executed on an x86_64 Syrus worker, not Apple Silicon:

```text
Linux x86_64, 4 vCPU under KVM
Compiler: c++ (Debian 12.2.0-14+deb12u1)
Benchmark binary: ./build/benchmark/benchmarks/benchmarks
Compile flags include: -O3 -funroll-loops -mtune=native -msse3
SIMD gates in these translation units: SSE=1, NEON=0
Load average during run: 16.43, 17.40, 14.88
```

Command:

```sh
cmake --preset benchmark
cmake --build --preset benchmark --target benchmarks
./build/benchmark/benchmarks/benchmarks \
  --benchmark_filter='bm_(color_add<Colorf>|color_scalar_mul<Colorf>|color_modulate<Colorf>|color_rgb<Colorf>|color_modulate_batch<Colorf>|dot<Vector3f>|dot<Vector4f>|matmul<float, 4, Matrix4f>|mat_vec<float, 4, Matrix4f, Vector4f>|bvh_primary_render_scalar|bvh_primary_render_packet4)' \
  --benchmark_min_time=0.02s
```

Relevant x86 smoke results:

```text
bm_bvh_primary_render_scalar                1722939 ns CPU
bm_bvh_primary_render_packet4                838333 ns CPU
bm_color_add<Colorf>                          0.427 ns CPU
bm_color_scalar_mul<Colorf>                   0.946 ns CPU
bm_color_modulate<Colorf>                     0.419 ns CPU
bm_color_rgb<Colorf>                           3.25 ns CPU
bm_color_modulate_batch<Colorf>                1222 ns CPU
bm_matmul<float, 4, Matrix4f>                  7.35 ns CPU
bm_mat_vec<float, 4, Matrix4f, Vector4f>       4.66 ns CPU
bm_dot<Vector3f>                               1.11 ns CPU
bm_dot<Vector4f>                               1.21 ns CPU
```

This run verifies the new benchmark registrations and the representative render
filter on the available worker. It is intentionally not used as ARM candidate
evidence.

## Layout and Include-Order Guardrails

The unit suite now makes object layout expectations explicit:

- `Vector3f` is 16 bytes and 16-byte aligned only when its x86 SSE
  specialization is compiled; otherwise it remains the compact generic layout.
- `Vector4f` is 16 bytes, with 16-byte alignment only for the x86 SSE
  specialization.
- `Colorf` is 16 bytes and 16-byte aligned only for the x86 SSE specialization;
  `Colord` is 32 bytes and 16-byte aligned only for the x86 SSE3
  specialization.
- `Matrix4f` keeps the generic 16-float storage layout. Its current x86 SIMD
  acceleration specializes member operations, not the object representation.

Because no NEON whole-type specialization is retained, ARM include order and
ABI remain on the generic template path for these single-object surfaces.
