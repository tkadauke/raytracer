# ARM SIMD Phase 0 Baseline

Purpose: preserve pre-NEON benchmark evidence for Epic #426 before adding ARM
SIMD implementations. This file ties together the existing Apple Silicon math
baseline and the packet benchmark run available from this Syrus worker.

## Apple Silicon Baseline

The committed Apple Silicon baseline is
`docs/perf/math-baseline-2026-05-10.txt`.

- Hardware: Apple Silicon arm64, 12-core.
- OS: macOS 26.3.1, Darwin 25.3.0.
- Compiler: Apple clang 16.0.0 (`clang-1600.0.26.6`).
- Source commit: `a064505e91b374a355bf714eda5f696e7d266c48`.
- Build: `cmake --preset benchmark` with release flags `-O3
  -funroll-loops -mtune=native`.
- Run command:
  `./build/benchmark/benchmarks/benchmarks --benchmark_min_time=0.1s --benchmark_filter='-bm_intersect<|-bm_shadow|-bm_build<'`.

Representative Apple Silicon results from that run:

```text
bm_intersects<float>                         769 ns        768 ns
bm_intersects<double>                        770 ns        769 ns
bm_matmul<float, 4, Matrix4f>               5.52 ns       5.50 ns
bm_matmul<double, 4, Matrix4d>              5.58 ns       5.57 ns
bm_mat_vec<float, 4, Matrix4f, Vector4f>    1.56 ns       1.55 ns
bm_mat_vec<double, 4, Matrix4d, Vector4d>   1.55 ns       1.55 ns
bm_dot<Vector3f>                           0.442 ns      0.442 ns
bm_dot<Vector3d>                           0.443 ns      0.443 ns
bm_dot<Vector4f>                           0.551 ns      0.550 ns
bm_dot<Vector4d>                           0.557 ns      0.556 ns
```

Compiler vectorization note: the Apple Silicon run predates
`RAYTRACER_SIMD_NEON`; no ARM intrinsic path was compiled. These numbers are
therefore the generic C++ plus Apple Clang autovectorization baseline for
`VectorBenchmark`, `MatrixBenchmark`, and `BoundingBoxBenchmark`. When rerunning
on Apple Silicon for NEON work, capture remarks with:

```sh
cmake --preset benchmark \
  -DCMAKE_CXX_FLAGS="-Rpass=loop-vectorize -Rpass-missed=loop-vectorize -Rpass-analysis=loop-vectorize"
cmake --build --preset benchmark --target benchmarks 2> docs/perf/arm-simd-phase0-vectorization-remarks.txt
```

## Syrus Worker Packet Baseline

This implement run executed on an x86_64 Syrus worker, not Apple Silicon:

```text
Linux syrus-worker-578f78747b-k4rsf 5.15.0-177-generic x86_64
CPU: 12th Gen Intel(R) Core(TM) i7-1260P, 4 vCPU under KVM
Compiler: c++ (Debian 12.2.0-14+deb12u1)
Branch/commit before local commit: syrus/issue-427-567 at ff5c1687
Benchmark binary: ./build/benchmark/benchmarks/benchmarks
Compile flags include: -O3 -funroll-loops -mtune=native -msse3
SIMD gates in this TU set: SSE=1, SSE2=1, SSE3=1, AVX=0, NEON=0
```

Command:

```sh
cmake --preset benchmark
cmake --build --preset benchmark --target benchmarks
./build/benchmark/benchmarks/benchmarks \
  --benchmark_filter='bm_(dot|add|sub|scalar_mul|scalar_div|length|squared_length|normalize|cross|reflect|refract|lerp|clamp|saturate|cwise_min|cwise_max|dot_batch|matmul|mat_vec|inverted4|stable_inverse4|determinant4|transposed|transform_point|transform_direction|build_transform3|intersects|intersect_interval|contains_point|union|intersection|volume|include_point|sphere_|triangle_|plane_|box_|bounding_box_|bvh_)' \
  --benchmark_min_time=0.05s
```

Relevant packet and traversal results from this run:

```text
bm_bvh_scalar_coherent                 102879 ns CPU
bm_bvh_packet4_coherent                 38752 ns CPU
bm_bvh_scalar_incoherent              1229062 ns CPU
bm_bvh_packet4_incoherent             1215085 ns CPU
bm_bvh_primary_render_scalar           865446 ns CPU
bm_bvh_primary_render_packet4          490033 ns CPU

bm_sphere_scalar/10000                 543952 ns CPU
bm_sphere_packet/10000                  69977 ns CPU
bm_triangle_scalar/10000               685688 ns CPU
bm_triangle_packet/10000               181701 ns CPU
bm_plane_scalar/10000                  439797 ns CPU
bm_plane_packet/10000                   37313 ns CPU
bm_box_scalar/10000                    636490 ns CPU
bm_box_packet/10000                     96301 ns CPU
bm_bounding_box_scalar/10000            39532 ns CPU
bm_bounding_box_packet/10000            12137 ns CPU

bm_sphere_scalar_four_ray_loop         205848 ns CPU
bm_sphere_ray4_packet                   24062 ns CPU
```

This x86 run verifies that the Phase 0 macro migration preserves existing
SSE/SSE3 packet behavior. It is not a substitute for a full Apple Silicon packet
baseline, but it records the currently available pre-NEON packet evidence in
the same place as the ARM baseline notes.
