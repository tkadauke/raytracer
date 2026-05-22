# Rasterizer plan — May 2026

> **Scope:** Execution order for the software rasterizer work promoted into
> `docs/roadmap.md`. This is a working plan, not a commitment; update it when
> measurements or implementation details change the order.
>
> **Status:** Living document. Completed items should be marked here, reflected
> in `docs/roadmap.md` where needed, and measured in `CHANGELOG.md` when they
> affect performance.

---

## Proposed execution order

1. ✅ **Rasterizer housekeeping + baselines**
   - Cleaned stale rasterizer header docs so they describe the current
     per-leaf primitive traversal, per-primitive materials, and opt-in tiled
     path.
   - Added benchmark scenes that exercise dense tessellation, material-backed
     shading, offscreen geometry, and tiled rendering.
   - Recorded canonical `rendercli --repeat` timing commands for rasterizer
     comparisons.

2. ✅ **Backface culling**
   - Added `Rasterizer::CullMode::{Both,Back,Front}` and `rendercli --cull`.
   - Pinned winding conventions in each primitive's tessellation tests.
   - Kept the default two-sided until material-sidedness exists.

3. ✅ **Homogeneous clip-space clipping**
   - Added `Camera::projectPointToClipSpace` for `PinholeCamera` and
     `OrthographicCamera`.
   - Clip near-plane and viewport edges in homogeneous space before the
     perspective divide.
   - Use cached per-vertex clip outcodes so fully-inside triangles skip the
     Sutherland-Hodgman path.

4. ✅ **Depth/stencil + shader hook skeleton**
   - Kept the current Z-buffer path as the default fast path.
   - Added configurable depth functions, depth clear value, optional depth
     writes, 8-bit stencil compare/write state, and stencil operations.
   - Added small vertex/fragment shader hooks over projected and interpolated
     mesh attributes.

5. ✅ **UV/attribute interpolation**
   - `HitPoint` now carries UV coordinates for texture evaluation.
   - Raster vertices/fragments carry perspective-correct UVs through both
     fixed-function material shading and fragment shader hooks.
   - Added `UVMapping2D` and unit coverage for UV-backed albedo sampling.

6. ✅ **MSAA + resolve**
   - Added fixed 1x/2x/4x/8x subpixel sample patterns.
   - Added per-sample colour/depth/stencil buffers and a float-framebuffer
     resolve path.
   - Current implementation shades covered samples directly; centroid /
     per-fragment shading remains a performance follow-up.

7. **Rasterized shadow maps**
   - ✅ Added an opt-in depth-only directional-light shadow pass for the
     built-in Lambertian fragment path, with configurable shadow-map size and
     depth bias exposed on `Rasterizer` and `rendercli`.
   - ✅ Added rendered docs for off/on, shadow-map resolution, and depth-bias
     comparisons, plus an interactive widget for the light-pass depth map and
     camera-pass comparison.
   - ✅ Added configurable PCF radius for percentage-closer filtering, exposed
     on `Rasterizer`, `rendercli`, tests, and rendered docs.
   - ✅ Added GeneratedRayTracer render-dialog controls for shadow-map enable,
     resolution, bias, and PCF radius.
   - ✅ Added opt-in PCSS blocker-search filtering, exposed on `Rasterizer`,
     `rendercli`, GeneratedRayTracer controls, tests, API docs, and rendered
     docs.
   - Next add cascaded shadow maps for comparison with raytraced shadow rays.

8. ✅ **GeneratedRaytracer / RenderWidget front-back buffers**
   - Added a render-thread back buffer and UI-thread front `QImage`.
   - `RenderEngine` now reports active/completed tiles for progress overlays
     and dirty-tile publication; `Raytracer` publishes completed LDR tiles as
     workers finish.
   - The render dialog now exposes periodic whole-buffer updates, completed-tile
     publishing, and final-frame double buffering for every engine. Raytracer
     defaults to periodic progress; Rasterizer defaults to double buffering.
   - The central GeneratedRayTracer preview keeps the previous front image
     visible while a new render starts, reuses the previous raytracer back
     buffer for 16 ms whole-buffer point-interlaced updates, and double-buffers
     Rasterizer/Wireframe previews while queueing the latest camera pose until
     the current frame finishes. Point-interlaced view planes choose their
     coarse starting resolution from the full view plane instead of each worker
     tile so tiled raytracer previews still begin visibly coarse. Raytracer
     cancellation uses an atomic camera flag with additional sample-loop checks
     so interrupted live previews can hand off to the next frame sooner.

9. **Post-process AA / TAA**
   - ✅ Added reusable FXAA over the float framebuffer, exposed through
     `Rasterizer::setPostProcessAA`, `rendercli --post_aa fxaa`, and the
     GeneratedRayTracer render dialog.
   - Add SMAA as a sharper image-space follow-up if FXAA blurs too much fine
     texture/detail in previews.
   - Add TAA later once history buffers, motion vectors, and display-buffer
     ownership are explicit.

10. ✅ **Tile-parallel rasterizer performance retry**
    - Re-measured after MSAA, shader hooks, shadow maps, PCF, double buffering,
      FXAA, tile-local MSAA storage, and the immediate 1x single-tile stream
      path landed. The tiled path now wins on screen-heavy 1x and 4x scenes,
      but still loses badly on dense tessellation, so `queueSize > 1` stays
      opt-in until a scene-aware default policy exists.
    - Next retry should decide whether the engine needs an automatic tiling
      heuristic, coarser per-tile work, tile-local depth/color storage with a
      final stitch, a GPU path, or a scene where shading cost dwarfs
      tessellation/binning/task/cache overhead.

11. **Frustum/spatial culling integration**
    - Integrate once the broader `SpatialIndex` work exists.
    - High value for large scenes, but less urgent than the current rasterizer
      pipeline work.

12. **2D geometric viewport clipping**
    - Keep as an educational comparison.
    - Do not prioritize it ahead of homogeneous clipping because the current
      scissor path is already fast and correct.

---

## Task 1 baseline suite

Canonical scenes live under `benchmarks/scenes/`:

- `rasterizer_baseline_dense_sphere.json` — one material-backed sphere. Run
  at high `--lod` to stress tessellation, projection caching, near-plane
  clipping setup, and the triangle loop without scene-composition noise.
- `rasterizer_baseline_materials.json` — checker floor, sphere, box, and
  torus with explicit `MatteMaterial` albedos. Catches regressions where the
  rasterizer falls back to face hashes instead of preserving materials.
- `rasterizer_baseline_offscreen_floor.json` — huge floor plus foreground and
  background geometry. Catches regressions in framebuffer clipping and
  Z-buffer behavior on projected triangles whose bounding boxes extend far
  outside the image.

Build the renderer before timing:

```sh
cmake --build --preset release --target rendercli --parallel
```

Use `rendercli --repeat N` so measurements exclude process startup,
scene loading, engine setup, image conversion, and PNG writing. The timer wraps
only `engine->render(buffer)`; the output image is saved once from the final
run.

```sh
build/release/tools/rendercli/rendercli \
  --engine raster --width 640 --height 480 --lod 8 \
  --repeat 10 \
  benchmarks/scenes/rasterizer_baseline_dense_sphere.json \
  /tmp/rasterizer-dense-sphere.png

build/release/tools/rendercli/rendercli \
  --engine raster --width 640 --height 480 --lod 3 \
  --repeat 10 \
  benchmarks/scenes/rasterizer_baseline_materials.json \
  /tmp/rasterizer-materials.png

build/release/tools/rendercli/rendercli \
  --engine raster --width 1920 --height 1080 --lod 0 \
  --repeat 10 \
  benchmarks/scenes/rasterizer_baseline_offscreen_floor.json \
  /tmp/rasterizer-offscreen-floor.png

build/release/tools/rendercli/rendercli \
  --engine raster --width 640 --height 480 --lod 8 \
  --threads 8 --queue_size 16 --repeat 10 \
  benchmarks/scenes/rasterizer_baseline_dense_sphere.json \
  /tmp/rasterizer-dense-sphere-tiled.png
```

Measurement rules:

- Record the machine/date, commit, command, and the reported
  `render_ms runs=... min=... median=... avg=... max=...` line.
- Keep single-tile and tiled results separate; tiled correctness exists today,
  but tiled speed is not expected to win until later raster stages add more
  per-pixel work.
- If a rasterizer performance change lands, update this section only when the
  command set changes; put the actual before/after measurements in
  `CHANGELOG.md`.

## Task 10 tile-parallel retry

Measured again on May 22 2026 after tile-local MSAA storage and the immediate
1x single-tile stream path. Tiled commands used `--threads 8 --queue_size 16`.
Correctness is pinned by unit tests and the measured PNGs below stayed
byte-identical between single-tile and tiled output.

The conclusion changed from the May 10 retry but is still not "make tiled the
default." Tiling is now a clear win for screen-heavy materials/offscreen scenes
and for queued 4x MSAA, but the dense tessellation baseline still spends enough
time in projection/binning/cache traffic that the tiled path loses by more than
2x. Keep `queueSize > 1` opt-in until a scene-aware default policy exists.

| Scene | Mode | Runs | Single-tile median | Tiled median |
| --- | --- | ---: | ---: | ---: |
| `rasterizer_baseline_materials.json` 640x480 LOD 3 | 1x | 10 | 16.829 ms | 12.429 ms |
| `rasterizer_baseline_materials.json` 640x480 LOD 3 | 4x MSAA | 10 | 59.718 ms | 20.236 ms |
| `rasterizer_baseline_offscreen_floor.json` 1920x1080 LOD 0 | 1x | 10 | 100.461 ms | 46.715 ms |
| `rasterizer_baseline_offscreen_floor.json` 1920x1080 LOD 0 | 4x MSAA | 5 | 436.677 ms | 95.671 ms |
| `rasterizer_baseline_dense_sphere.json` 640x480 LOD 8 | 1x | 5 | 1841.772 ms | 4028.655 ms |

## Task 2 backface culling

The rasterizer now exposes three cull modes:

- `Rasterizer::CullMode::Both` / `rendercli --cull both` — default two-sided
  rendering.
- `Rasterizer::CullMode::Back` / `rendercli --cull back` — skip back-facing
  triangles.
- `Rasterizer::CullMode::Front` / `rendercli --cull front` — skip
  front-facing triangles for diagnostics and teaching.

Culling runs after near-plane clipping and before triangle rasterization or
tile binning. With the current camera projection, source meshes are wound CCW
when viewed from outside; after projection, visible front-facing triangles have
negative screen-space signed area and back-facing triangles have positive
screen-space signed area. The rasterizer tests pin that convention for
hand-authored triangles and for both single-tile and tiled paths.

Winding belongs to the primitive tessellation implementations, so those tests
live beside each source primitive's existing tessellation tests:

- `BoxTessellate.FacesAreWoundWithOutwardNormals`
- `DiskTessellate.ShouldWindFacesWithDiskNormal`
- `OpenCylinderTessellate.ShouldWindFacesWithRadialNormals`
- `RectangleTessellate.ShouldWindFacesWithPlaneNormal`
- `SphereTessellate.FacesAreWoundWithRadialNormals`
- `TorusTessellate.FacesAreWoundWithParametricNormals`
- `TriangleTessellate.ShouldWindFaceWithFlatNormal`

Measurement note: on the dense sphere baseline at 640x480 LOD 8 with
`--repeat 10`, `--cull both` reported median `1158.779 ms` and `--cull back`
reported median `1133.989 ms`; the final PNGs were byte-identical. The modest
speedup is expected because this cull stage still happens after tessellation,
per-mesh projection, and near-plane setup. Full measurements are in
`CHANGELOG.md`.

## Task 3 homogeneous clip-space clipping

The rasterizer now consumes `Camera::projectPointToClipSpace` rather than
combining `eyeRelativeDepth` with already-divided screen coordinates. The clip
space convention is:

- `x / w` and `y / w` are normalized viewport coordinates in `[-1, 1]`.
- `z` is the positive eye-relative depth used by the Z-buffer.
- `w` is the perspective divisor; orthographic cameras use `w = 1`.

`PinholeCamera` returns defined clip coordinates even for points behind the
eye, so triangles that cross the eye/near plane can be clipped before any
divide. `OrthographicCamera` uses the same normalized viewport convention with
unit `w`. Cameras without a closed-form clip projection still render empty in
the software rasterizer.

The triangle path uses cached per-vertex clip outcodes:

- `(out0 & out1 & out2) != 0` rejects triangles fully outside one clip plane.
- `(out0 | out1 | out2) == 0` accepts fully-inside triangles without running
  Sutherland-Hodgman.
- Mixed triangles run the fixed-size homogeneous Sutherland-Hodgman clipper
  against the near plane plus left/right/top/bottom viewport planes.

There is still no far plane because cameras do not expose one yet. The clipper
is structured so a far-depth plane can be added once camera/render settings
grow an explicit far-clip policy.

Measurement note: after the clip-outcode fast path and cached per-vertex screen
coordinates, the dense sphere baseline at 640x480 LOD 8 with `--repeat 10`
reported `render_ms runs=10 min=999.101 median=1039.896 avg=1134.154
max=1609.866`. The offscreen-floor baseline at 1920x1080 LOD 0 reported
`render_ms runs=10 min=95.352 median=98.215 avg=98.771 max=104.422`. Full
measurements are in `CHANGELOG.md`.

## Task 4 depth/stencil + shader hook skeleton

The rasterizer now exposes the fixed-function stages that were previously
hard-coded into the pixel loop:

- `Rasterizer::DepthFunc::{Never,Less,Equal,LessEqual,Greater,GreaterEqual,NotEqual,Always}`
- `setDepthClearValue` and `setDepthWriteEnabled`
- `Rasterizer::StencilFunc` with reference/mask state
- `Rasterizer::StencilOp::{Keep,Zero,Replace,IncrementClamp,DecrementClamp,Invert}`
- `setStencilClearValue`, `setStencilWriteMask`, and `setStencilOps`
- `VertexShader` over clipped/projected vertex attributes
- `FragmentShader` over perspective-correct interpolated fragment attributes

Defaults preserve the fixed-function output: depth func `Less`, depth writes
enabled, depth clear `+infinity`, stencil disabled, and no custom shader hooks.
The single-tile default path stays specialized so normal raster renders do not
pay per-fragment `std::function` or stencil/depth-dispatch overhead. The
generic path is used when depth/stencil/fragment state changes, when a vertex
shader is installed, or for the opt-in tiled path.

Unit coverage pins the new behavior:

- `DepthStencilAndShaderDefaultsMatchFixedPipeline`
- `DepthFuncNeverRejectsFragments`
- `DisabledDepthWritesLetLaterGeometryOverdraw`
- `StencilFailOpCanSeedLaterGeometry`
- `FragmentShaderOverridesBuiltInShading`
- `VertexShaderCanAdjustProjectedPosition`

Measurement note: after restoring the specialized fixed-function single-tile
path, the dense sphere baseline at 640x480 LOD 8 with `--repeat 10` reported
`render_ms runs=10 min=1014.534 median=1057.099 avg=1156.787 max=1726.229`.
The offscreen-floor baseline at 1920x1080 LOD 0 reported `render_ms runs=10
min=98.212 median=109.761 avg=110.942 max=132.385`. The materials baseline at
640x480 LOD 3 reported `render_ms runs=10 min=14.073 median=14.313 avg=14.544
max=15.527`. Full measurements are in `CHANGELOG.md`.

## Task 5 UV/attribute interpolation

UVs now travel through the same raster attribute path as normals and world
positions:

- `HitPoint` stores optional `Vector2d` UV coordinates and preserves them
  across normal swapping and instance transforms.
- `Rasterizer::VertexInput`, `VertexOutput`, and `FragmentInput` expose UVs to
  shader hooks.
- The homogeneous clipper interpolates UVs when it creates clip-edge vertices.
- The raster pixel loop perspective-correctly interpolates UVs, then passes the
  full hit context into `MatteMaterial` diffuse textures.
- `UVMapping2D` maps `HitPoint::uv()` directly to texture-space `(s, t)`.

The first visible milestone is pinned by `Rasterizer.BuiltInMaterialTextureReceivesInterpolatedUV`:
a matte triangle uses a texture whose colour is derived from the interpolated
UV, and the built-in material path renders that albedo without a custom
fragment shader. `Rasterizer.FragmentShaderReceivesInterpolatedUV` pins the
programmable fragment hook separately.

Measurement note: the extra UV math is present in the hot path, but current
baseline impact is small. With `rendercli --repeat 10`, the dense sphere
baseline at 640x480 LOD 8 reported `render_ms runs=10 min=1022.142
median=1039.353 avg=1142.892 max=1635.037`. The offscreen-floor baseline at
1920x1080 LOD 0 reported `render_ms runs=10 min=98.156 median=99.687
avg=100.820 max=105.644`. The materials baseline at 640x480 LOD 3 reported
`render_ms runs=10 min=14.308 median=15.128 avg=15.427 max=18.247`. Full
measurements are in `CHANGELOG.md`.

## Task 6 MSAA + resolve

`Rasterizer::setMSAASamples` accepts the supported 1x/2x/4x/8x sample counts,
and `rendercli --engine raster --msaa N` exposes the same setting from the
command line. `--msaa 1` is the default and preserves the existing single-sample
fast path. Values above 1 use a fixed subpixel pattern around the historical
integer pixel sample point.

The implementation keeps the engine-level framebuffer contract unchanged:

- `core::rasterizeTriangleSampled` evaluates the edge-function inside-test at
  a subpixel offset while still emitting the owning integer pixel and
  barycentric weights.
- The rasterizer projects, clips, culls, and bins triangles once per render,
  then renders each MSAA sample against independent colour, depth, and optional
  stencil buffers.
- The final resolve averages the sample colours into the existing
  `Buffer<Colord>` output, so tonemapping and future postprocess passes do not
  need an MSAA-specific display path.
- The tiled path is supported for MSAA and is pinned against single-tile output,
  but it stays opt-in for the same reason as the 1x tiled path: current scenes
  still do not have enough per-tile work to repay binning overhead reliably.
- GeneratedRaytracer's render dialog exposes the same 1x/2x/4x/8x selector
  when the Rasterizer engine is active.
- The rasterizer docs include both static 1x/4x comparison renders and an
  interactive MSAA coverage widget that shows sample positions, partial edge
  coverage, and the resolved per-pixel value.

Current limitation: the first implementation shades at each covered sample
position. That is correct for attributes and depth, but more expensive than a
centroid/per-fragment shading mode that computes one colour for a pixel and
replicates it to covered samples. Keep that optimization in the raster-quality
backlog until measurements show MSAA cost is blocking preview use.

Measurement note: full measurements are in `CHANGELOG.md`. On this change, the
materials baseline moved from median `14.431 ms` at `--msaa 1` to
`55.513 ms` at `--msaa 4`; the offscreen-floor baseline moved from median
`101.965 ms` to `442.536 ms`; the dense LOD 8 sphere moved from median
`1035.287 ms` to `3778.575 ms`. The canonical comparison command is the
materials baseline with `--msaa 1` and `--msaa 4`:

```sh
build/release/tools/rendercli/rendercli \
  --engine raster --width 640 --height 480 --lod 3 \
  --msaa 1 --repeat 10 \
  benchmarks/scenes/rasterizer_baseline_materials.json \
  /tmp/rasterizer-materials-msaa1.png

build/release/tools/rendercli/rendercli \
  --engine raster --width 640 --height 480 --lod 3 \
  --msaa 4 --repeat 10 \
  benchmarks/scenes/rasterizer_baseline_materials.json \
  /tmp/rasterizer-materials-msaa4.png
```
