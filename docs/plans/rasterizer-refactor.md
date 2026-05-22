# Rasterizer refactor plan

> **Scope:** performance and structure cleanup before adding the next feature
> wave to the software rasterizer. Keep this plan separate from
> `docs/plans/rasterizer.md`, which tracks user-visible feature order.
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

Status: partially done. `RasterTriangleEmitter` now owns the shared scene walk,
per-leaf tessellation, projection, homogeneous clipping, culling, optional
vertex-shader adaptation, and face-fan emission. `RasterTriangleSet` owns the
emitted triangles plus their tile bins, and both single-tile and tiled paths draw
through the same raster loop.

Remaining work:

- Add an immediate default raster sink if measurement shows that materializing
  and binning a `RasterTriangleSet` is still too expensive for ordinary 1x
  single-tile previews.
- Keep the sink type compile-time visible so the default path does not pay for
  virtual dispatch or `std::function` calls.
- Re-measure before touching the remaining fragment/material path.

### 4. Fragment/depth/stencil policies

Status: done. `withPreparedTrianglePolicies` resolves public rasterizer state
once per pass into concrete stencil, depth, and fragment policy objects:
disabled-vs-enabled stencil, write-vs-read-only depth, built-in Lambertian
fragments, programmable fragments, and depth-only shadow-map fragments. The
inner triangle loop receives concrete policy types and makes direct calls.

Follow-up performance work belongs under the material evaluator and tile-local
MSAA items below.

### 5. Per-primitive material evaluator

Current issue: built-in shading does material type discovery in the fragment
path. `dynamic_pointer_cast`, `HitPoint`, and synthetic `Rayd` construction are
too expensive to repeat for every visible sample when the material is known per
primitive.

Plan:

- Build a small `RasterMaterial` / material-evaluator value while emitting
  triangles.
- Detect matte-material and fallback-colour cases once per primitive.
- Only build the full texture-evaluation hit context when the chosen evaluator
  actually needs it.

### 6. MSAA tile-local storage

Current issue: MSAA renders one full-frame colour/depth/stencil set per sample,
with one clear and one resolve pass per sample.

Plan:

- Render all samples for a tile inside the same task.
- Use tile-local sample colour/depth/stencil buffers.
- Resolve each tile directly into the output framebuffer.
- Keep single-tile and tiled output equivalence tests.

## Non-goals for this pass

- Public rasterizer API changes.
- A render-pass DAG.
- Geometric 2D viewport clipping; keep that as an educational feature, not a
  performance dependency.
- Virtual interfaces in pixel-loop code.
