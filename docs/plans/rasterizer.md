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

1. **Rasterizer housekeeping + baselines**
   - Clean stale rasterizer header docs.
   - Add or refresh golden scenes that exercise the rasterizer's current
     behavior.
   - Record canonical `rendercli` timing commands for rasterizer comparisons.

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

5. **UV/attribute interpolation**
   - Carry UVs and material inputs through raster fragments.
   - Make albedo texture sampling the first visible milestone.

6. **MSAA + resolve**
   - Start with per-sample coverage/depth and per-fragment shading.
   - Resolve into the float framebuffer so tonemapping and postprocessing stay
     engine-agnostic.
   - Add per-sample shading later as the expensive correctness mode.

7. **Rasterized shadow maps**
   - Add a depth-only shadow pass after depth/stencil infrastructure exists.
   - Start with directional-light shadow maps, then add PCF, PCSS, and
     cascades for comparison with raytraced shadow rays.

8. **GeneratedRaytracer / RenderWidget front-back buffers**
   - Add dirty-tile publication and immutable paint snapshots.
   - Separate progressive preview behavior from final-frame display.

9. **Post-process AA / TAA**
   - Add FXAA/SMAA first for preview engines.
   - Add TAA later once history buffers, motion vectors, and display-buffer
     ownership are explicit.

10. **Tile-parallel rasterizer performance retry**
    - Revisit once MSAA, shaders, and shadows make per-pixel work heavier.
    - Keep `queueSize > 1` opt-in until measurements show it beats the
      streaming single-tile path.

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
