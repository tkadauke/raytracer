# Acceleration policy benchmark, 2026-05-28

Command:

```sh
cmake --preset benchmark
cmake --build --preset benchmark --target benchmarks -j2
./build/benchmark/benchmarks/benchmarks \
  --benchmark_filter='bm_policy' \
  --benchmark_min_time=0.03s \
  --benchmark_out=docs/perf/acceleration-policy-benchmark-2026-05-28.json \
  --benchmark_out_format=json
```

Host summary from Google Benchmark: 4 x 2496 MHz CPU, L1d 32 KiB x4,
L2 4096 KiB x4, L3 16384 KiB x1. The host was busy
(`Load Average: 13.83, 12.23, 11.55`), so policy decisions below use the
reported CPU time and broad ratios, not single-digit percentage differences.

## Workloads

- `procedural_clustered_spheres`: 400 procedural sphere leaves in four separated clusters.
- `mesh_heavy_terrain`: 2,592 triangle leaves in a regular height-field mesh.
- `imported_ply_shark`: 5,112 triangle leaves loaded from `test/fixtures/shark.ply`.
- `imported_assembly_mixed_boxes`: 540 mixed-size box leaves approximating repeated imported CAD/LDraw parts.

Each workload records index build cost, closest-hit primary-style
intersection cost, boolean shadow-ray cost, and a primary-ray render-impact
proxy over the same 512 deterministic rays.

## CPU time per 512-ray batch

All times are milliseconds. Bold marks the fastest accelerated structure for
the query type; Linear is retained as the fallback baseline.

| Workload | Query | Linear | Grid | BVH |
| --- | ---: | ---: | ---: | ---: |
| procedural_clustered_spheres | build | 0.008 | **0.103** | 0.206 |
| procedural_clustered_spheres | intersect | 10.400 | **0.225** | 0.500 |
| procedural_clustered_spheres | shadow | 10.450 | 0.168 | **0.125** |
| procedural_clustered_spheres | render proxy | 11.390 | **0.227** | 0.522 |
| mesh_heavy_terrain | build | 0.053 | **1.366** | 2.346 |
| mesh_heavy_terrain | intersect | 49.840 | **0.379** | 0.409 |
| mesh_heavy_terrain | shadow | 34.842 | 0.262 | **0.126** |
| mesh_heavy_terrain | render proxy | 66.500 | **0.581** | 0.681 |
| imported_ply_shark | build | 0.111 | **2.194** | 6.595 |
| imported_ply_shark | intersect | 185.502 | **1.294** | 1.908 |
| imported_ply_shark | shadow | 129.094 | 0.957 | **0.275** |
| imported_ply_shark | render proxy | 192.376 | 1.482 | **1.269** |
| imported_assembly_mixed_boxes | build | 0.011 | 0.384 | **0.291** |
| imported_assembly_mixed_boxes | intersect | 12.569 | **0.287** | 2.746 |
| imported_assembly_mixed_boxes | shadow | 0.200 | 0.028 | **0.025** |
| imported_assembly_mixed_boxes | render proxy | 7.921 | **0.151** | 1.539 |

## Policy conclusion

The linear fallback is only competitive on construction cost. For
empty/single-leaf scenes it avoids accelerator setup entirely; for every
multi-leaf workload above, either Grid or BVH beats Linear by orders of
magnitude on ray queries. This backs the Auto policy's Linear choice for
empty/small scenes and rules it out as the general default.

Grid is the best primary-ray choice for regular or cell-friendly workloads:
clustered procedural spheres, regular terrain triangles, and repeated box
assemblies all show Grid ahead of BVH on the primary render proxy. This is why
Grid remains an explicit user-selectable policy and the right manual choice
for primary-heavy benchmark scenes with regular spatial density.

BVH is the best shadow-ray choice on all four workloads and is the best
render-impact proxy for the imported PLY triangle soup. Whitted-style renders
trace shadow rays for visible surfaces, and imported/static scenes are the
case where setup cost is amortized over many ray queries. That combination is
the measured reason Auto keeps BVH as the default for multi-leaf scenes while
leaving Grid available for regular, primary-heavy scenes where the benchmark
shows it can win.

Raw Google Benchmark JSON is committed next to this note as
`docs/perf/acceleration-policy-benchmark-2026-05-28.json`.
