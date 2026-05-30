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

Additional BVH node-mask packet evidence captured after `BVH::intersectPacketNode`
was verified to use `BoundingBox::intersects4`:

```sh
./build/benchmark/benchmarks/benchmarks \
  --benchmark_filter='bm_bvh_(scalar|packet4)_(coherent|incoherent)' \
  --benchmark_min_time=0.05s
```

```text
bm_bvh_scalar_coherent             102487 ns CPU
bm_bvh_packet4_coherent             41151 ns CPU
bm_bvh_scalar_incoherent          1332469 ns CPU
bm_bvh_packet4_incoherent         1367662 ns CPU
```

On this x86 worker, coherent BVH Ray4 traversal measured about 2.49x faster
than four scalar traversals. Incoherent packet traversal measured about 2.6%
slower than scalar, within the expected noise envelope for sparse active masks
under the reported worker load average of about 10-11.

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

bm_bvh_scalar_coherent             102879 ns CPU
bm_bvh_packet4_coherent             38752 ns CPU
bm_bvh_scalar_incoherent          1229062 ns CPU
bm_bvh_packet4_incoherent         1215085 ns CPU
```

The local x86 run is broadly neutral for the migrated kernels given the noisy
shared worker load average reported by Google Benchmark (about 15 during the
primitive-kernel run and about 10-11 during the BVH run). ARM NEON before/after
timing was not captured in this environment; rerun the same command on an ARM
runner before closing the benchmark acceptance item.
