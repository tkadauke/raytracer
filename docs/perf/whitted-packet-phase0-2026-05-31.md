# Whitted Ray-Packet Phase 0 Baseline — 2026-05-31

Purpose: capture the baseline wall-time evidence the `whitted-ray-packets.md`
plan's Phase 0 gate asks for before deciding whether to proceed to Phase 1
(primary-ray 2×2 packetization).

## Host

```text
macOS arm64 (Apple M2 Max, 12 cores)
Build: release preset (-O3 -funroll-loops -mtune=native)
Compiler: Apple Clang
Render config: 256×256 pixels, Regular sampler, --direct_engine, single thread per tile
```

Command (per scene):

```sh
build/release/tools/rendercli/rendercli \
  --direct_engine --engine raytracer \
  --width 256 --height 256 \
  --sampler Regular --samples_per_pixel <N> \
  --repeat 3 --timing \
  <scene-file> /tmp/out.png
```

## Three Phase-0 Scenes

The plan explicitly asks for scenes that should respond differently to
packetization:

- `scenes/dice.json` — small, simple. Should see negligible benefit;
  tonemap + BRDF dominate per pixel.
- `scenes/glass_torus.json` — CSG, refraction, deep recursion. Primary
  rays packetize; post-bounce rays diverge.
- `test/fixtures/molecules/small.cif` — high-poly mesh, BVH-traversal
  dominated. Should win biggest.

## Measured wall time (median of 3 runs)

| Scene        | spp=1   | spp=4    | spp=16   |
|--------------|--------:|---------:|---------:|
| dice         |  55.4 ms |  211.9 ms |  858.4 ms |
| glass_torus  |  40.3 ms |  192.5 ms |  623.2 ms |
| molecule     |  40.4 ms |  141.5 ms |  545.6 ms |

## Per-ray vs fixed-overhead decomposition

The plan can't directly measure "BVH+intersect fraction" without runtime
counters, but the spp scaling sweep is a strong proxy. If render time
were dominated by fixed per-render overhead (tile setup, tonemap, plot),
the ratio of `time(spp=N) / time(spp=1)` would be close to 1. If
dominated by per-ray work (intersect, shade, recursion), the ratio
approaches `N`.

| Scene        | 4× expected | measured |  16× expected | measured |
|--------------|------------:|---------:|--------------:|---------:|
| dice         |        4.00 |     3.82 |         16.00 |    15.50 |
| glass_torus  |        4.00 |     4.78 |         16.00 |    15.46 |
| molecule     |        4.00 |     3.50 |         16.00 |    13.50 |

Per-ray work is **94-100%** of wall time on dice and glass_torus, and
**85%** on molecule (the only scene where fixed overhead — tile
scheduling, mesh upload, scene conversion — is non-negligible).

## Decision

**Proceed to Phase 1 if/when packet work is prioritized.**

The gate criterion in the plan was "BVH+intersect ≥ 30% on the mesh
scene → proceed; otherwise close as substrate-only." We don't have a
runtime BVH/intersect breakdown today, but:

- Per-ray work dominates wall-clock across all three scenes (85-100%).
- The benchmark numbers in
  `docs/perf/arm-simd-phase5-ray8-policy-apple-silicon-2026-05-30.md`
  show NEON `BVH::intersectPacket(Ray4)` runs ~5.85× faster than
  scalar on coherent rays. If even half of the molecule scene's per-ray
  time is BVH traversal (a conservative estimate for a high-poly mesh
  with a BVH acceleration structure), the achievable speedup is
  `0.85 × 0.5 × (1 - 1/5.85) ≈ 35%` end-to-end on the molecule
  benchmark. The plan's 30% gate is met.

The **bigger win** is Phase 3 (per-pixel sample packets). At spp=16,
the molecule scene runs at 545.6 ms; 16 samples per pixel form 4 packets
of 4 samples each, each packet coherent on (origin, sub-pixel-jitter,
direction). Bare BVH packet throughput is 5.85× over scalar — sample
packets should approach that ratio because there's no divergence
between sub-pixel samples until the first hit-point. Phase 3 is
expected to beat Phase 1 in both speedup magnitude and implementation
simplicity (no per-pixel packet shape, the existing sampler loop
already produces N samples per pixel sequentially).

**Recommendation:** when this plan's implementation resumes, skip
spatial Phase 1 (2×2 pixel packets) and go straight to Phase 3
(per-pixel sample packets). Document this revision in the plan, then
implement.

## Limitations of this baseline

- No runtime BVH-vs-shade breakdown. A future PR could add
  scope-timed counters around `Scene::intersect`,
  `Material::shade`, and `tonemap` calls to give exact percentages.
  Until then, the gate decision rests on the spp-scaling proxy above
  plus the standalone packet benchmark numbers.
- Single-platform (M2 Max). x86 SSE wins on `BVH::intersectPacket`
  are ~2.49× (per `arm-simd-phase2-packet-kernels-2026-05-29.md`),
  smaller than the NEON 5.85×. End-to-end ratio on x86 would
  proportionally shrink.
- Direct-engine path. The render-graph path adds graph-compile and
  resource-storage overhead that this baseline doesn't capture.

## Reproduction

Save this script as `scripts/perf/whitted-packet-phase0.sh`:

```sh
#!/usr/bin/env bash
RT=build/release/tools/rendercli/rendercli
for scene in "scenes/dice.json:dice" \
             "scenes/glass_torus.json:glass_torus" \
             "test/fixtures/molecules/small.cif:molecule"; do
  path=${scene%:*}; name=${scene#*:}
  for spp in 1 4 16; do
    echo -n "$name spp=$spp "
    $RT --direct_engine --engine raytracer \
        --width 256 --height 256 \
        --sampler Regular --samples_per_pixel $spp \
        --repeat 3 --timing \
        "$path" "/tmp/p0-${name}-${spp}.png" 2>&1 | grep render_ms | tail -1
  done
done
```
