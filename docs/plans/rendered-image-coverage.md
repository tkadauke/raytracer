# Rendered Image Coverage

This is the audit trail for class-level rendered documentation images.
The durable rules live in `scripts/README.md`; this file tracks the
current gaps.

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

## Current Gaps

### Broken image references

`rake check:doc-images` currently fails because several headers still
reference old unsuffixed class image names after the
raytracer/rasterizer comparison images were introduced:

- `box.png` from `include/world/objects/Box.h`
- `matte_material_red.png` from `include/world/objects/MatteMaterial.h`
- `orthographic_camera_cube.png` from
  `include/world/objects/OrthographicCamera.h`
- `phong_material_red.png` from
  `include/world/objects/PhongMaterial.h`
- `pinhole_camera_cube.png` from
  `include/render/cameras/PinholeCamera.h` and
  `include/world/objects/PinholeCamera.h`
- `reflective_material_red.png` from
  `include/world/objects/ReflectiveMaterial.h`
- `sphere.png` from `include/world/objects/Sphere.h`
- `transparent_material.png` from
  `include/world/objects/TransparentMaterial.h`

The same lint run reports
`pinhole_camera_cube__raytracer.png` and
`pinhole_camera_cube__raster.png` as orphan PNGs because the Pinhole
headers still point at `pinhole_camera_cube.png`.

### Primitive and surface coverage

- `OpenCylinder`, `Disk`, `Triangle`, `Rectangle`, and `Torus` have
  Wireframe images only. They need class-level Raytracer and
  Rasterizer renders because they have ray intersections, world
  wrappers, and non-empty `tessellate()` implementations.
- `Box`, `Sphere`, and `Cylinder` have the needed Raytracer,
  Rasterizer, and Wireframe images, but the references are not
  normalized. `Box` and `Sphere` still have stale `world::` wrapper
  references, and all three split Wireframe out instead of presenting
  the default class image as a consistent engine comparison.
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

### Material coverage

- `MatteMaterial`, `PhongMaterial`, `ReflectiveMaterial`, and
  `TransparentMaterial` already have Raytracer and Rasterizer class
  images in the runtime headers, but the corresponding `world::`
  wrapper headers still reference stale unsuffixed names.
- `PortalMaterial` has no rendered documentation image. It is a
  runtime-only material today, so adding coverage needs a doc-render
  DSL path or a small bespoke render driver. Raytracer should be the
  first image; Rasterizer has no portal semantics and should be
  skipped or shown only as an explicit limitation comparison.
- Wireframe material renders are intentionally absent because
  Wireframe ignores material shading.

### Camera coverage

- `PinholeCamera` has generated Raytracer and Rasterizer images, but
  the headers reference the stale unsuffixed name. It also needs a
  Wireframe class image because Wireframe can project Pinhole camera
  points.
- `OrthographicCamera` has generated Raytracer and Rasterizer images,
  but its `world::` wrapper references the stale unsuffixed name. It
  also needs a Wireframe class image because Wireframe can project
  Orthographic camera points.
- `FishEyeCamera`, `SphericalCamera`, and `EquirectangularCamera` are
  raytracer-only. That is expected until they implement a forward
  projection suitable for Rasterizer or Wireframe.
- `ThinLensCamera` and `TiltShiftCamera` are raytracer-only. That is
  expected today because they do not implement the Rasterizer or
  Wireframe forward projection APIs.

### Non-gaps

- Property docs for primitive dimensions, camera FOV/zoom, material
  coefficients, sampler density, lights, scene colors, motion blur,
  and tonemapping are correctly raytracer-only unless a future pass
  identifies specific educational value in an engine comparison.
- Rasterizer UV, Rasterizer MSAA, Rasterizer LOD, and Wireframe LOD
  are correctly engine-specific.
