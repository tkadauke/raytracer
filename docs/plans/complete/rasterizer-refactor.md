# Rasterizer refactor plan

> **Scope:** performance and structure cleanup before adding the next feature
> wave to the software rasterizer. This plan is archived alongside
> `docs/plans/complete/rasterizer.md`, which tracked the first user-visible
> feature wave.
>
> **Rule:** one step at a time. Re-measure before and after each performance
> change, preserve rendered output unless the step explicitly says otherwise,
> and commit only after review.

## Goals

1. Make the current rasterizer faster where the hot paths are obviously leaving
   work on the table.
2. Extract reusable internals only where the abstraction can inline away or is
   outside the pixel loop.
3. Reduce duplication without removing the specialized default path that keeps
   ordinary 1x raster previews fast.

## Baseline

Measured on 2026-05-09 at commit `3c49ea6` on `arm64`.

Renderer build:

```sh
cmake --build --preset release --target rendercli
```

All timings use `rendercli --repeat 10`, so the timer wraps only
`engine->render(buffer)` and excludes process startup, scene loading, engine
setup, image conversion, and PNG writing.

| Scene / configuration | Timing |
| --- | --- |
| dense sphere, 640x480, LOD 8, 1x MSAA | `render_ms runs=10 min=1016.296 median=1062.704 avg=1163.727 max=1784.417` |
| dense sphere, 640x480, LOD 8, 1x MSAA, `--threads 8 --queue_size 16` | `render_ms runs=10 min=2448.456 median=2686.379 avg=3046.541 max=5828.553` |
| dense sphere, 640x480, LOD 8, 4x MSAA | `render_ms runs=10 min=2731.571 median=3161.505 avg=3115.803 max=3242.393` |
| dense sphere, 640x480, LOD 8, 8x MSAA | `render_ms runs=10 min=3183.751 median=3728.296 avg=3831.604 max=4827.740` |
| materials, 640x480, LOD 3, 1x MSAA | `render_ms runs=10 min=14.756 median=15.015 avg=15.241 max=16.270` |
| materials, 640x480, LOD 3, 4x MSAA | `render_ms runs=10 min=54.967 median=56.245 avg=57.757 max=63.453` |
| materials, 640x480, LOD 3, 8x MSAA | `render_ms runs=10 min=107.278 median=109.886 avg=113.274 max=138.971` |
| offscreen floor, 1920x1080, LOD 0, 1x MSAA | `render_ms runs=10 min=99.224 median=100.757 avg=104.322 max=125.483` |
| offscreen floor, 1920x1080, LOD 0, 4x MSAA | `render_ms runs=10 min=430.288 median=440.747 avg=441.074 max=456.550` |
| offscreen floor, 1920x1080, LOD 0, 8x MSAA | `render_ms runs=10 min=842.418 median=860.440 avg=866.889 max=923.038` |

Baseline read:

- The single-thread 1x path is still the fastest path on current scenes.
- The tiled dense-sphere path is about 2.5x slower than the single-tile path,
  so tile dispatch/binning should stay opt-in until later work gives each tile
  more per-pixel work.
- MSAA is close to linear on the materials and offscreen-floor scenes. The
  dense-sphere scene scales less linearly, which suggests tessellation,
  projection, and clipping setup are a large share of that workload.

## Execution order

### 1. Baseline measurements

Status: done. Numbers are recorded above.

### 2. Prepared triangle coverage in `core::rasterizeTriangleSampled`

Status: done. `core::rasterizeTriangleSampled` now prepares fixed-point edge
values, row/column deltas, area, inverse area, and the clipped bounding box
once per triangle. The public wrappers and barycentric/sample-offset contracts
are unchanged.

Latest recorded measurements are in `CHANGELOG.md`: output stayed byte-for-byte
identical on the baseline PNGs; 1x timings were broadly neutral, while repeated
coverage paths such as tiled dense-sphere and 4x/8x MSAA improved.

### 3. Templated triangle emitter

Status: done. `RasterTriangleEmitter` now owns the shared scene walk,
per-leaf tessellation, projection, homogeneous clipping, culling, optional
vertex-shader adaptation, and face-fan emission. Ordinary 1x single-tile
previews stream emitted triangles directly into the full-frame pass buffers
through concrete depth/stencil/fragment policy types, so they skip
`RasterTriangleSet` materialization and tile binning. Tiled rendering and MSAA
still retain a `RasterTriangleSet` because they reuse projected/clipped
triangles across tile ownership or multiple sample offsets.

Measured on May 22 2026 with `rendercli --repeat`; output PNGs stayed
byte-identical.

| Scene | Runs | Before median | After median |
| --- | ---: | ---: | ---: |
| `rasterizer_baseline_materials.json` 640x480 LOD 3, 1x | 10 | 18.760 ms | 16.829 ms |
| `rasterizer_baseline_offscreen_floor.json` 1920x1080 LOD 0, 1x | 10 | 104.814 ms | 100.461 ms |
| `rasterizer_baseline_dense_sphere.json` 640x480 LOD 8, 1x | 5 | 4194.552 ms | 1841.772 ms |

### 4. Fragment/depth/stencil policies

Status: done. `withPreparedTrianglePolicies` resolves public rasterizer state
once per pass into concrete stencil, depth, and fragment policy objects:
disabled-vs-enabled stencil, write-vs-read-only depth, built-in Lambertian
fragments, programmable fragments, and depth-only shadow-map fragments. The
inner triangle loop receives concrete policy types and makes direct calls.

Follow-up performance work belongs under the material evaluator and tile-local
MSAA items below.

### 5. Per-primitive material evaluator

Status: done. `RasterTriangleEmitter` now builds a `RasterMaterialSource` once
per leaf primitive, and emitted triangles carry a small `RasterMaterial` value
that either returns a cached color or evaluates the original texture.

What changed:

- Matte-material and fallback-color type discovery moved out of the fragment
  path.
- Exact `ConstantColorTexture` matte albedos are cached as colors; subclasses
  keep their virtual `evaluate` behavior.
- Arbitrary textures still receive the full interpolated `HitPoint` and
  synthetic `Rayd`, but only on the texture path.

Measured on May 22 2026 against `rasterizer_baseline_materials.json` at
640x480 LOD 3 with `rendercli --repeat 10`; output PNGs stayed byte-identical.

| Mode | Before median | After median |
| --- | ---: | ---: |
| 1x | 21.602 ms | 20.688 ms |
| 4x MSAA | 69.370 ms | 63.474 ms |

### 6. MSAA tile-local storage

Status: done for the queued tiled MSAA path. The default `queueSize == 1` path
keeps the specialized full-frame MSAA loop so ordinary single-tile renders do
not pick up tile-coordinate translation overhead. When `queueSize > 1`, each
tile task now renders all sample offsets using tile-local color/depth/stencil
buffers and resolves the tile directly into the output framebuffer.

What changed:

- Full-frame and tile-local buffer views are separate template types, so the
  default path keeps direct framebuffer indexing while the tiled path translates
  global fragment coordinates into local storage.
- Tiled MSAA equivalence tests now cover uneven tile sizes and stencil-enabled
  MSAA, in addition to the existing color/depth case.
- Before/after PNGs stayed byte-identical, including default-vs-tiled output.

Measured on May 22 2026 with `rendercli --repeat`; tiled commands used
`--threads 8 --queue_size 16`.

| Scene | Mode | Runs | Before median | After median |
| --- | --- | ---: | ---: | ---: |
| `rasterizer_baseline_materials.json` 640x480 LOD 3 | 4x MSAA default | 10 | 64.660 ms | 59.718 ms |
| `rasterizer_baseline_materials.json` 640x480 LOD 3 | 4x MSAA tiled | 10 | 38.918 ms | 20.236 ms |
| `rasterizer_baseline_offscreen_floor.json` 1920x1080 LOD 0 | 4x MSAA default | 5 | 454.027 ms | 436.677 ms |
| `rasterizer_baseline_offscreen_floor.json` 1920x1080 LOD 0 | 4x MSAA tiled | 5 | 231.608 ms | 95.671 ms |

## Non-goals for this pass

- Public rasterizer API changes.
- A render-pass DAG.
- Geometric 2D viewport clipping; keep that as an educational feature, not a
  performance dependency.
- Virtual interfaces in pixel-loop code.
