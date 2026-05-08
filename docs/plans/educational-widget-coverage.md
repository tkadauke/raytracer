# Educational Widget Coverage Plan

This plan tracks documentation concepts that would benefit from interactive
widgets or, rarely, static explanatory graphics. It complements the completed
modernization plan in `docs/plans/complete/widgets.md` and the live
implementation rules in `scripts/README.md`. This file is about coverage:
where a diagram would teach a concept better than text or rendered images
alone.

## Principles

- Prefer interactive widgets for algorithms with movable geometric inputs,
  competing branches, sampled choices, or step-by-step state.
- Use static graphics only when the concept has no useful user-controlled
  parameter.
- Do not add widgets for simple visual property sweeps. Rendered image tables
  already cover colors, scalar coefficients, object sizes, background color,
  and light intensity well enough.
- Place widgets next to the class or method that owns the concept. If the same
  concept appears in both runtime and `world::` wrapper docs, embed the widget
  where the algorithm is explained and cross-reference from the wrapper if
  needed.
- Build new widgets on the shared `figure.js` primitives, with direct draggable
  handles for spatial state and sliders or segmented controls for scalar state.
- Use US English spelling in labels, comments, docs, and tests.

## Priority 1: Core Rendering Concepts

These explain behavior that is visible in rendered docs but hard to understand
from still images.

1. **Transparent material refraction and total internal reflection**
   - Targets: `include/render/materials/TransparentMaterial.h`,
     `include/render/brdf/PerfectTransmitter.h`.
   - Widget: incident ray, surface normal, reflected ray, refracted ray, IOR
     slider, and critical-angle marker.
   - Interaction: drag incident direction; adjust inner/outer IOR; show when
     refraction disappears and total internal reflection takes over.
   - Teaches: Snell's law, ray bending, reflection/transmission split, and why
     high IOR values can trap rays inside a medium.

2. **Reflective material recursion**
   - Targets: `include/render/materials/ReflectiveMaterial.h`,
     `include/render/brdf/PerfectSpecular.h`.
   - Widget: incoming ray plus draggable normal, mirror ray, and a small
     recursion tree for reflected bounces.
   - Interaction: drag the normal and incoming ray; adjust reflection
     coefficient.
   - Teaches: mirror-direction calculation and why recursive ray tracing is
     needed for reflections.

3. **Portal material ray redirection**
   - Targets: `include/render/materials/PortalMaterial.h`,
     `include/world/objects/PortalMaterial.h`.
   - Widget: portal plane, incoming ray, transformed origin/direction, and
     optional color filter swatch.
   - Interaction: drag source ray and portal transform handles; adjust filter
     color or use a simple segmented filter toggle.
   - Teaches: the portal is not a screen border; it transforms the ray and asks
     the scene what the transformed ray sees.

4. **Phong and Lambertian BRDF lobes**
   - Targets: `include/render/materials/MatteMaterial.h`,
     `include/render/materials/PhongMaterial.h`,
     `include/render/brdf/Lambertian.h`,
     `include/render/brdf/GlossySpecular.h`.
   - Widget: surface normal, light vector, view vector, diffuse term, and
     specular lobe.
   - Interaction: drag light/view vectors; adjust Phong exponent and
     coefficients.
   - Teaches: Lambertian `n dot l`, view-dependent specular highlights, and why
     the Phong exponent narrows the highlight.

5. **CSG hit intervals**
   - Targets: `include/core/math/HitPointInterval.h`,
     `include/render/primitives/Union.h`,
     `include/render/primitives/Intersection.h`,
     `include/render/primitives/Difference.h`.
   - Widget: one ray timeline with enter/exit markers for two shapes and an
     operation selector.
   - Interaction: drag interval endpoints; switch union/intersection/difference.
   - Teaches: CSG as interval set operations, positive-distance hit selection,
     and normal flipping for difference.

## Priority 2: Acceleration And Sampling

These explain performance and progressive-rendering behavior that rendered
images cannot show directly.

1. **BVH SAH split and traversal**
   - Targets: `include/render/primitives/BVH.h`.
   - Widget: draggable primitive AABBs, candidate split positions, surface-area
     costs, selected split, and a ray that prunes missed subtrees.
   - Interaction: drag boxes; move the ray; optionally toggle one build step at
     a time.
   - Teaches: why BVHs adapt to scene distribution, what SAH is optimizing, and
     why ray-AABB tests skip whole subtrees.

2. ~~**Grid 3D-DDA traversal**~~ ✅ **Done.** Added
   `grid_dda_traversal.js` next to `Grid`'s runtime docs, with draggable ray
   origin/direction handles, grid-density control, entry marker, current cell,
   `t_next` readout, and visited-cell trail.
   - Targets: `include/render/primitives/Grid.h`.
   - Widget: 2D grid slice, ray entry point, current cell, `t_next` per axis,
     and visited-cell trail.
   - Interaction: drag ray origin and direction; adjust grid density.
   - Teaches: uniform grid cell stepping and why grids work best for evenly
     distributed primitives.

3. ~~**ViewPlane iteration strategies**~~ ✅ **Done.** Added
   `viewplane_iteration_order.js` next to `ViewPlane`'s runtime iteration docs;
   it compares row-major, tiled, interlaced, and shuffled traversal with a
   progress slider.

4. ~~**Sampler patterns and sample streams**~~ ✅ **Done.** Added `sampler_streams.js` with sampler-pattern and independent-dimension controls, embedded from `Sampler` docs.
   - Targets: `include/render/samplers/Sampler.h`,
     `include/render/samplers/RegularSampler.h`,
     `include/render/samplers/JitteredSampler.h`,
     `include/render/samplers/RandomSampler.h`,
     `include/render/samplers/SampleStream.h`.
   - Widget: subpixel sample square plus separate stream dimensions for pixel,
     lens, and shutter-time samples.
   - Interaction: choose sampler type and sample count; show repeated sets or
     independent stream dimensions.
   - Teaches: regular vs jittered vs random sampling, stratification, and why
     extra camera dimensions should not reuse the exact same 2D pattern.

5. ~~**Motion blur time sampling**~~ ✅ **Done.** `motion_blur_time_sampling.js`
   embeds the shutter-time sampling diagram in `Instance::setVelocity`, with a
   world-side pointer from `Surface::setVelocity`.
   - Targets: `include/world/objects/Surface.h`,
     `include/render/primitives/Instance.h`.
   - Widget: object path over shutter time, sampled positions, and accumulated
     ghosted silhouettes.
   - Interaction: drag velocity vector; scrub shutter time; switch regular vs
     stochastic sampling.
   - Teaches: time is another sample dimension and linear velocity turns one
     static primitive into many time-offset intersections.

## Priority 3: Projection, Mapping, And Geometry

These are useful follow-ups once the core rendering and sampling widgets exist.

1. ~~**Forward projection and clip-space depth**~~ ✅ **Done.** Added
   `camera_forward_projection.js` and embedded it beside
   `Camera::projectPointToClipSpace`, with pinhole/orthographic controls and
   tests.
   - Targets: `include/render/cameras/Camera.h`,
     `include/render/cameras/PinholeCamera.h`,
     `include/render/cameras/OrthographicCamera.h`.
   - Widget: camera, view plane, draggable world point, projected pixel, depth,
     and homogeneous `w`.
   - Interaction: toggle pinhole vs orthographic; drag the world point through
     and behind the camera.
   - Teaches: perspective divide, eye-relative depth, why orthographic has
     `w = 1`, and why the rasterizer needs clip-space projection.

2. ~~**Wide-angle and panoramic camera mappings**~~ ✅ **Done.** Added
   `wide_angle_camera_mappings.js` and embedded it in the fisheye,
   spherical, and equirectangular runtime camera docs.
   - Targets: `include/render/cameras/FishEyeCamera.h`,
     `include/render/cameras/SphericalCamera.h`,
     `include/render/cameras/EquirectangularCamera.h`.
   - Widget: image rectangle and unit sphere side by side.
   - Interaction: drag an image point; show the corresponding ray direction on
     the sphere; adjust FOV where applicable.
   - Teaches: fisheye disc cutoff, spherical partial panorama, equirectangular
     seam, and pole stretching.

3. ~~**Texture coordinate mapping**~~ ✅ **Done.** Added `texture_coordinate_mapping.js` and embedded it in the checker texture runtime docs, with controls for planar vs UV mapping, U/V scale, sample-point dragging, and parity lookup.
   - Targets: `include/world/objects/CheckerBoardTexture.h`,
     `include/render/textures/mappings/PlanarMapping2D.h`,
     `include/render/textures/mappings/UVMapping2D.h`,
     `include/render/textures/CheckerBoardTexture.h`.
   - Widget: surface point, generated `(s, t)`, checker parity, and a tiny
     sampled texture preview.
   - Interaction: toggle planar vs UV mapping; adjust U/V scale; drag sample
     point.
   - Teaches: texture evaluation is a coordinate-mapping step followed by a
     color lookup; checker parity comes from `floor(s) + floor(t)`.

4. **Instance transforms and normal transforms**
   - Targets: `include/render/primitives/Instance.h`,
     `include/world/objects/Transformable.h`.
   - Widget: world ray, local-space ray, transformed object, geometric normal,
     and inverse-transpose normal under non-uniform scale.
   - Interaction: adjust scale/rotation; drag ray; toggle point, direction, and
     normal transform views.
   - Teaches: intersecting an instance means transforming the ray into local
     space, while normals need inverse-transpose handling.

5. **Mesh triangle interpolation**
   - Targets: `include/render/primitives/FlatMeshTriangle.h`,
     `include/render/primitives/SmoothMeshTriangle.h`,
     `include/render/primitives/Triangle.h`.
   - Widget: draggable triangle with barycentric coordinates and per-vertex
     normals/UVs.
   - Interaction: drag hit point inside the triangle; toggle flat vs smooth
     normal interpolation.
   - Teaches: barycentric coordinates are shared by ray-triangle tests,
     smooth normals, UV interpolation, and rasterizer attributes.

## Priority 4: Specialized Or Deferred Concepts

These are valuable but should wait until the higher-priority concepts are in
place, or until nearby roadmap work makes them more visible.

1. **Depth, stencil, and culling state**
   - Targets: `include/engine/raster/Rasterizer.h`.
   - Widget: small framebuffer, overlapping triangles, depth buffer, stencil
     mask, and cull-mode selector.
   - Teaches: why render passes can mark a region first, then draw only through
     that mask; useful groundwork for planar reflections and portals.

2. **Support mapping and GJK**
   - Targets: `include/render/primitives/Primitive.h`,
     `include/render/primitives/MinkowskiSum.h`,
     `include/core/math/GJKSimplex.h`.
   - Widget: two convex shapes, support points, Minkowski difference point, and
     evolving simplex.
   - Teaches: convex intersection via support functions. This should follow the
     simpler farthest-point and CSG interval widgets.

3. **Color model conversions**
   - Target: `include/core/Color.h`.
   - Widget: likely static or lightly interactive swatches for RGB, HSV, and
     CMYK relationships.
   - Teaches: useful background for future color-science work, but less central
     to the renderer than rays, materials, projection, and sampling.

## Explicit Non-Targets

- Simple property sweeps already represented by rendered image tables:
  material colors, reflection/transmission coefficients by themselves, light
  intensity, scene background, box size, ring dimensions, object position, and
  transform scale.
- Widgets that duplicate a rendered image without exposing the underlying
  algorithm. A widget should explain the "why" or "how", not merely show another
  picture of the same result.
- New whole-widget drag gestures. Follow `scripts/README.md`: visible handles
  for spatial state, sliders or segmented controls for scalar state.

## Completion Criteria

- Every priority-1 item has a widget embedded next to the relevant runtime
  documentation.
- The widget gallery generated by `rake docs:widgets` contains each new widget
  and remains usable for bulk review.
- `rake test:scripts:js` covers shared widget behavior added for these diagrams.
- `rake check:doc-images` still passes after any Doxygen embed changes.
- When a plan item is completed, move it into a "Completed" section here or
  archive this plan under `docs/plans/complete/` once all non-deferred items are
  finished.
