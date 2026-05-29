# ARM SIMD Phase 2 Packet Kernel Evidence

Purpose: record the post-port benchmark evidence for Epic #426 Phase 2 after
Sphere, Plane, Box, and Triangle Ray4 packet kernels moved from direct `__m128`
intrinsics to the shared `core::simd::Float4`/`Mask4` backend.

## Syrus Worker Post-Port Run

This run executed on an x86_64 Syrus worker, not Apple Silicon:

```text
Linux x86_64, 4 vCPU under KVM
Compiler: c++ (Debian 12.2.0-14+deb12u1)
Benchmark binary: ./build/benchmark/benchmarks/benchmarks
Compile flags include: -O3 -funroll-loops -mtune=native -msse3
SIMD gates in these translation units: SSE=1, NEON=0
```

Command:

```sh
cmake --preset benchmark
cmake --build --preset benchmark --target benchmarks
./build/benchmark/benchmarks/benchmarks \
  --benchmark_filter='bm_(sphere|triangle|plane|box)_(scalar|packet)|bm_sphere_(scalar_four_ray_loop|ray4_packet)' \
  --benchmark_min_time=0.05s
```

Relevant post-port x86 results:

```text
bm_sphere_scalar/10000             590681 ns CPU
bm_sphere_packet/10000              83171 ns CPU
bm_triangle_scalar/10000           729848 ns CPU
bm_triangle_packet/10000           189500 ns CPU
bm_plane_scalar/10000              501808 ns CPU
bm_plane_packet/10000               40733 ns CPU
bm_box_scalar/10000                668015 ns CPU
bm_box_packet/10000                102511 ns CPU

bm_sphere_scalar_four_ray_loop     228557 ns CPU
bm_sphere_ray4_packet               28283 ns CPU
```

For comparison, the Phase 0 x86 packet baseline in
`docs/perf/arm-simd-phase0-baseline-2026-05-28.md` recorded:

```text
bm_sphere_scalar/10000             543952 ns CPU
bm_sphere_packet/10000              69977 ns CPU
bm_triangle_scalar/10000           685688 ns CPU
bm_triangle_packet/10000           181701 ns CPU
bm_plane_scalar/10000              439797 ns CPU
bm_plane_packet/10000               37313 ns CPU
bm_box_scalar/10000                636490 ns CPU
bm_box_packet/10000                 96301 ns CPU

bm_sphere_scalar_four_ray_loop     205848 ns CPU
bm_sphere_ray4_packet               24062 ns CPU
```

The local x86 run is broadly neutral for the migrated kernels given the noisy
shared worker load average reported by Google Benchmark (about 15 during the
run). ARM NEON before/after timing was not captured in this environment; rerun
the same command on an ARM runner before closing the benchmark acceptance item.
