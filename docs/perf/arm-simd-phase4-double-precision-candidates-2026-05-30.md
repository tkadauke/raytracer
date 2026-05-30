# ARM SIMD Phase 4 Double-Precision Candidate Decision

Purpose: record the Epic #426 Phase 4 decision for double-precision ARM SIMD
candidates. This phase intentionally evaluates double precision separately from
the existing x86 SSE/SSE2 choices instead of mirroring those paths on NEON.

## Decision

No double-precision NEON specialization is retained by this change.

On ARM, the relevant types and operations continue to use the generic C++
implementations plus the compiler's normal scalar/autovectorized codegen. The
existing x86 behavior is unchanged: `BoundingBox<double>` keeps its SSE2 slab
specialization, `Vector4d` keeps its SSE3 specialization, `Matrix4d` keeps its
SSE2/SSE3 operator specializations, and `Colord` keeps its SSE3 specialization
when those x86 features are available.

This is a conservative decision, not a claim that NEON double precision can
never win. The project rule for Phase 4 is stricter: keep a NEON double path
only when it has before/after Apple Silicon evidence and also improves a hot
repo benchmark, not just an isolated microbenchmark.

## Candidate Status

`BoundingBox<double>::intersects` and
`BoundingBox<double>::intersect(Ray, Range&)`: keep scalar on ARM. These are
real hot paths through BVH traversal, so any future NEON prototype must show a
win in both `BoundingBoxBenchmark` and a representative spatial-index workload
such as `AccelerationPolicyBenchmark`'s BVH cases. The Phase 0 Apple Silicon
baseline already measured `bm_intersects<double>` at about 770 ns for the
256-ray batch with generic ARM code, and no NEON after-run exists.

`Vector4d`: keep scalar on ARM. Scratch evidence suggested `double4` can help
in compute-heavy loops, but the committed Apple Silicon baseline already has
very small single-object timings (`bm_dot<Vector4d>` about 0.557 ns). A future
candidate must improve `VectorBenchmark` and also a render or transform-heavy
repo benchmark before it is retained.

`Matrix4d`: keep scalar on ARM. The Apple Silicon Phase 0 generic baseline
recorded `bm_matmul<double, 4, Matrix4d>` at about 5.58 ns and
`bm_mat_vec<double, 4, Matrix4d, Vector4d>` at about 1.55 ns. Those are
candidate microbenchmarks only; a retained NEON path also needs a hot workload
win, for example transform-heavy render/import benchmarks.

`Colord`: keep scalar on ARM. `ColorBenchmark` now covers `Colord` arithmetic,
RGB packing, and a batched modulation loop, but there is no Apple Silicon
before/after evidence showing a repo-level win. As with `Colorf`, RGB
conversion has observable rounding behavior that must be pinned before adding a
new architecture-specific implementation.

## Required Future Evidence

Run this scalar baseline on Apple Silicon before prototyping a double NEON
candidate:

```sh
cmake --preset benchmark
cmake --build --preset benchmark --target benchmarks
./build/benchmark/benchmarks/benchmarks \
  --benchmark_filter='bm_(intersects_batch<double>|intersect_interval<double>|intersects_precomputed_inverse<double>|intersect_interval_precomputed_inverse<double>|dot<Vector4d>|dot_batch<Vector4d>|matmul<double, 4, Matrix4d>|mat_vec<double, 4, Matrix4d, Vector4d>|color_add<Colord>|color_scalar_mul<Colord>|color_modulate<Colord>|color_rgb<Colord>|color_modulate_batch<Colord>|policy(Intersect|ShadowRay|PrimaryRenderImpact))' \
  --benchmark_min_time=0.1s
```

Then run the same command after the NEON prototype. Keep the path only if:

- the targeted microbenchmark improves beyond noise;
- the relevant hot workload also improves;
- unrelated hot workloads are neutral;
- correctness and layout tests still pass on both ARM and x86.

## Syrus Worker Smoke Run

This run executed on an x86_64 Syrus worker, not Apple Silicon:

```text
Linux syrus-worker-578f78747b-k4rsf 5.15.0-177-generic x86_64
Compiler: c++ (Debian 12.2.0-14+deb12u1)
Benchmark binary: ./build/benchmark/benchmarks/benchmarks
Compile flags include: -O3 -funroll-loops -mtune=native -msse3
SIMD gates in these translation units: SSE=1, SSE2=1, SSE3=1, NEON=0
```

Command:

```sh
cmake --preset benchmark
cmake --build --preset benchmark --target benchmarks
./build/benchmark/benchmarks/benchmarks \
  --benchmark_filter='bm_(intersects_batch<double>|intersect_interval<double>|intersects_precomputed_inverse<double>|intersect_interval_precomputed_inverse<double>|dot<Vector4d>|dot_batch<Vector4d>|matmul<double, 4, Matrix4d>|mat_vec<double, 4, Matrix4d, Vector4d>|color_add<Colord>|color_scalar_mul<Colord>|color_modulate<Colord>|color_rgb<Colord>|color_modulate_batch<Colord>|policy(Intersect|ShadowRay|PrimaryRenderImpact))' \
  --benchmark_min_time=0.02s
```

The smoke run is only used to verify the benchmark filter and current x86
coverage. It is intentionally not used as ARM candidate evidence.

Relevant x86 smoke results:

```text
bm_intersects_batch<double>                            31451 ns CPU
bm_intersect_interval<double>                          31013 ns CPU
bm_intersects_precomputed_inverse<double>              25549 ns CPU
bm_intersect_interval_precomputed_inverse<double>      38764 ns CPU

bm_dot<Vector4d>                                       0.867 ns CPU
bm_dot_batch<Vector4d>                                  1201 ns CPU

bm_matmul<double, 4, Matrix4d>                          6.28 ns CPU
bm_mat_vec<double, 4, Matrix4d, Vector4d>               5.47 ns CPU

bm_color_add<Colord>                                   0.481 ns CPU
bm_color_scalar_mul<Colord>                            0.810 ns CPU
bm_color_modulate<Colord>                              0.493 ns CPU
bm_color_rgb<Colord>                                    1.98 ns CPU
bm_color_modulate_batch<Colord>                          723 ns CPU
```

Representative hot-workload coverage from the same run:

```text
bm_policyIntersect/0/2              413489 ns CPU  procedural_clustered_spheres/bvh
bm_policyIntersect/1/2              308932 ns CPU  mesh_heavy_terrain/bvh
bm_policyIntersect/2/2             1003781 ns CPU  imported_ply_shark/bvh
bm_policyIntersect/3/2             1400171 ns CPU  imported_assembly_mixed_boxes/bvh

bm_policyShadowRay/0/2               90834 ns CPU  procedural_clustered_spheres/bvh
bm_policyShadowRay/1/2               89699 ns CPU  mesh_heavy_terrain/bvh
bm_policyShadowRay/2/2              195240 ns CPU  imported_ply_shark/bvh
bm_policyShadowRay/3/2               21517 ns CPU  imported_assembly_mixed_boxes/bvh

bm_policyPrimaryRenderImpact/0/2    412429 ns CPU  procedural_clustered_spheres/bvh
bm_policyPrimaryRenderImpact/1/2    309432 ns CPU  mesh_heavy_terrain/bvh
bm_policyPrimaryRenderImpact/2/2   1114276 ns CPU  imported_ply_shark/bvh
bm_policyPrimaryRenderImpact/3/2   1478615 ns CPU  imported_assembly_mixed_boxes/bvh
```
