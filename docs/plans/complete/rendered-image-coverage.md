# Rendered Image Coverage

This is the audit trail for class-level rendered documentation images.
The durable rules live in `scripts/README.md`; this file records the
completed coverage pass and the cases that were intentionally deferred
or classified as non-gaps.

## Principles

- Primitive and surface class docs should show every engine that can
  currently render that primitive meaningfully: Raytracer, Rasterizer,
  and Wireframe.
- Rasterizer coverage requires the object to produce a non-empty mesh
  through `tessellate()` and the active camera to implement
  `projectPointToClipSpace`.
- Wireframe coverage requires the object to produce a non-empty mesh
  through `tessellate()` and the active camera to implement
  `projectPoint`.
- CSG primitives whose mesh boolean tessellation returns an empty mesh
  are raytracer-only until mesh booleans exist.
- Aggregators with no intrinsic shape, such as `Composite`,
  `Instance`, `Grid`, and `Scene`, do not need their own class-level
  engine images; their coverage is their children's coverage.
- Materials should be rendered with engines that evaluate material
  semantics. Wireframe is excluded because it ignores materials.
- Cameras should be rendered with Raytracer first, then with
  Rasterizer and Wireframe only when the camera has the required
  forward projection API.
- Property sweeps are raytracer-only by default. Add extra engines
  only when the engine difference is the point of the image.
- Engine-specific concepts, such as Rasterizer MSAA or Wireframe LOD,
  should only render with the relevant engine.
- Multi-engine class images should use `class_doc(engines: [...])`
  and the generated `__<engine>` suffixes. Runtime headers and
  `world::` wrapper headers must reference the actual generated image
  names.

## Completed Coverage Passes

- Fixed stale unsuffixed image references in the `world::` wrappers
  for `Box`, `Sphere`, `MatteMaterial`, `PhongMaterial`,
  `ReflectiveMaterial`, `TransparentMaterial`, `PinholeCamera`, and
  `OrthographicCamera`.
- Added class-level Raytracer and Rasterizer render drivers for
  `OpenCylinder`, `Disk`, `Triangle`, `Rectangle`, and `Torus`, while
  rendering Wireframe through the same multi-engine `class_doc`
  blocks.
- Added Wireframe class images for `PinholeCamera` and
  `OrthographicCamera`.
- Normalized supported-engine class tables for `Box`, `Sphere`, and
  `Cylinder` so the `__wireframe` image is generated and presented
  alongside the Raytracer and Rasterizer images.
- Added a world-side `PortalMaterial` wrapper and Raytracer class
  image so portal materials can be rendered through the standard
  docs JSON pipeline.
- `rake check:doc-images` now passes with all `@image html`
  references backed by files under `docs/images`.

## Deferred Or Non-Gaps

### Material coverage

- Wireframe material renders are intentionally absent because
  Wireframe ignores material shading.

### Deferred engine coverage

- `Ring` is raytracer-only. This is not a current coverage gap:
  its default shape is built from `Difference`, and CSG tessellation
  is not implemented.
- `Union`, `Intersection`, `Difference`, `MinkowskiSum`, and
  `ConvexHull` are raytracer-only. This is expected until CSG mesh
  boolean tessellation exists.
- `ScriptedSurface` has raytracer-only `dice` and `brick` renders.
  Rasterizer and Wireframe coverage should wait until scripted
  surfaces and the CSG-heavy scripts they use can produce useful
  tessellated meshes.
- `FishEyeCamera`, `SphericalCamera`, and `EquirectangularCamera` are
  raytracer-only. That is expected until they implement a forward
  projection suitable for Rasterizer or Wireframe.
- `ThinLensCamera` and `TiltShiftCamera` now expose an equivalent
  pinhole forward projection for Rasterizer and Wireframe previews.
  Their aperture blur and focal-plane effects remain raytracer-only
  because mesh projection has one vertex position, not per-sample
  lens rays to average; tilt/shift-specific preview support remains
  deferred, so Rasterizer and Wireframe use plain pinhole framing.

### Non-gaps

- Property docs for primitive dimensions, camera FOV/zoom, material
  coefficients, sampler density, lights, scene colors, motion blur,
  and tonemapping are correctly raytracer-only unless a future pass
  identifies specific educational value in an engine comparison.
- Rasterizer UV, Rasterizer MSAA, Rasterizer LOD, and Wireframe LOD
  are correctly engine-specific.
- `PortalMaterial` is correctly Raytracer-only for now. Rasterized
  portal previews belong to the future stencil / render-pass graph
  work tracked in the roadmap.
