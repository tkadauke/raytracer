# Appendix A — Glossary

Hand-written; *not* generated. One line per entry, with a link
to the chapter where the concept is introduced. Drift is fixed
by editing this file.

## A

- **AABB** — Axis-Aligned Bounding Box. A rectangular box
  whose faces are parallel to the world-coordinate axes. Used
  for fast ray-rejection in BVH and Grid acceleration.
  [Chapter 3 §3.6](../01-foundations/03-rays-and-geometry.md#3-6-bounding-boxes).
- **ACES** — Academy Color Encoding System. The Narkowicz
  polynomial fit ships as one of the three tonemap operators.
  [Chapter 12 §12.5](../02-ray-rendering/12-tone-mapping.md#12-5-aces-filmic-ish-punchy-midtones).
- **Affine transform** — translation + rotation + scale +
  shear; the family of transforms preserving lines and
  parallelism. The transformation matrices used for scene
  placement.
  [Chapter 2 §2.1](../01-foundations/02-matrices-and-transforms.md#2-1-why-4-4).
- **Aliasing** — visible artifacts produced by
  undersampling a continuous signal. Fixed by Monte Carlo
  super-sampling (raytracer) or MSAA (rasterizer).
  [Chapter 10 §10.1](../02-ray-rendering/10-sampling-and-anti-aliasing.md#10-1-aliasing-as-undersampling).
- **AOV** — Arbitrary Output Variable. Per-pixel auxiliary
  buffer holding intermediate quantities (depth, normal,
  albedo). Future topic; not yet in the codebase.

## B

- **Barycentric coordinates** — three-tuple $(w_0, w_1, w_2)$
  with $\sum w_i = 1$, parameterizing points inside a
  triangle as a weighted average of its vertices.
  [Chapter 7 §7.5](../02-ray-rendering/07-primitives-and-intersection.md#7-5-triangle-moller-trumbore).
- **Bresenham** — Bresenham's all-octants integer line
  rasterizer (1965). The wireframe engine's per-edge
  rasterizer.
  [Chapter 20 §20.2](../04-rasterization/20-wireframe-rendering.md#20-2-bresenhams-line-algorithm).
- **BRDF** — Bidirectional Reflectance Distribution Function.
  $f_r(\mathbf{w}_i, \mathbf{w}_o)$ — the directional
  reflectance of a surface point.
  [Chapter 8 §8.2](../02-ray-rendering/08-materials-and-brdfs.md#8-2-what-a-brdf-is).
- **BSDF** — Bidirectional Scattering Distribution Function.
  Container abstraction over BRDF + BTDF that unifies
  reflection and transmission behind one interface.
  [Chapter 8 §8.5](../02-ray-rendering/08-materials-and-brdfs.md#8-5-the-bsdf-interface).
- **BTDF** — Bidirectional Transmittance Distribution
  Function. The transmission analogue of BRDF, for materials
  that pass light through.
  [Chapter 8 §8.3](../02-ray-rendering/08-materials-and-brdfs.md#8-3-the-four-shipped-brdfs).
- **BVH** — Bounding Volume Hierarchy. Binary tree of nested
  AABBs that prunes most of the scene per ray.
  [Chapter 15 §15.2](../03-scene-structure/15-spatial-acceleration.md#15-2-the-bounding-volume-hierarchy).

## C

- **Centroid** — area-weighted center of a 2D region. Used by
  Blob and Silhouette as the reference point for radial-
  variance descriptors.
  [Chapter 23 §23.3](../05-image-and-vision/23-blob-analysis-and-silhouettes.md#23-3-what-a-blob-carries).
- **Clip space** — homogeneous 4D coordinates after the
  camera's projection but before the perspective divide.
  Where the rasterizer does its clipping.
  [Chapter 19 §19.2](../04-rasterization/19-clipping-depth-stencil.md#19-2-sutherland-hodgman-in-homogeneous-clip-space).
- **CRTP** — Curiously Recurring Template Pattern. The
  template trick `Vector<N, T>` and `Matrix<N, T>` use to
  return the derived type from base-class operators.
  [Chapter 1 §1.2](../01-foundations/01-numbers-and-vectors.md#1-2-the-shape-of-vector-n-t).
- **CSG** — Constructive Solid Geometry. Boolean and
  support-mapping composition of primitives via
  HitPointInterval set operations.
  [Chapter 14](../03-scene-structure/14-csg.md).
- **CullMode** — `Both` / `Back` / `Front` switch on the
  rasterizer that drops triangles by their projected
  screen-space winding.
  [Chapter 19 §19.5](../04-rasterization/19-clipping-depth-stencil.md#19-5-face-culling).

## D

- **DDA** — Digital Differential Analyzer. The 3D
  cell-traversal algorithm the Grid acceleration structure
  uses.
  [Chapter 15 §15.4](../03-scene-structure/15-spatial-acceleration.md#15-4-the-uniform-grid-alternative).
- **Depth buffer** — see *Z-buffer*.
- **DOF** — Depth of Field. The defocus blur produced by a
  finite-aperture camera.
  [Chapter 6 §6.5](../02-ray-rendering/06-cameras.md#6-5-thin-lens-depth-of-field).
- **Dot product** — $\mathbf{a} \cdot \mathbf{b} = \lVert\mathbf{a}\rVert\lVert\mathbf{b}\rVert\cos\theta$;
  the workhorse 1D scalar derived from two vectors.
  [Chapter 1 §1.4](../01-foundations/01-numbers-and-vectors.md#1-4-dot-product).

## E

- **Edge function** — Pineda 1988 inside-test for triangle
  rasterization. The signed area of the sub-triangle formed
  by an edge and a candidate pixel.
  [Chapter 18 §18.2](../04-rasterization/18-the-rasterization-pipeline.md#18-2-the-edge-function-inside-test).
- **Equirectangular** — full-sphere $360° \times 180°$
  panorama projection. The format used for environment maps
  and VR sky textures.
  [Chapter 6 §6.4](../02-ray-rendering/06-cameras.md#6-4-wide-angle-spherical-fisheye-equirectangular).

## F

- **Ferrari's method** — closed-form solution of the quartic
  equation. Used by the torus-ray intersection routine.
  [Chapter 7 §7.7](../02-ray-rendering/07-primitives-and-intersection.md#7-7-torus-a-quartic-root-problem).
- **Fragment shader** — the per-pixel function in the
  rasterizer pipeline that turns interpolated attributes
  plus a material into a final color.
  [Chapter 18 §18.5](../04-rasterization/18-the-rasterization-pipeline.md#18-5-fragment-shading).
- **Fresnel** — the angle-dependent ratio of reflected to
  transmitted light at a refractive surface. Used by
  TransparentMaterial.
  [Chapter 8 §8.4](../02-ray-rendering/08-materials-and-brdfs.md#8-4-the-five-shipped-materials).

## G

- **Gamma encode** — the nonlinear mapping from linear-RGB
  to sRGB display values. The codebase currently omits this
  step (see chapter 4).
  [Chapter 4 §4.3](../01-foundations/04-color-and-buffers.md#4-3-linear-rgb-vs-srgb).
- **GJK** — Gilbert-Johnson-Keerthi distance algorithm
  (1988). Computes ray intersections with convex shapes
  defined only by a support function.
  [Chapter 14 §14.4](../03-scene-structure/14-csg.md#14-4-gjk-ray-intersection-on-a-support-function).
- **Glossy specular** — the Phong cosine-power lobe BRDF.
  [Chapter 8 §8.3](../02-ray-rendering/08-materials-and-brdfs.md#8-3-the-four-shipped-brdfs).

## H

- **HDR** — High Dynamic Range. Float-precision colors with
  no upper clamp, accumulated through the render pipeline
  before tonemap.
  [Chapter 12 §12.1](../02-ray-rendering/12-tone-mapping.md#12-1-why-hdr-exists).
- **Heckbert-Moreton** — the 1991 perspective-correct
  attribute-interpolation trick. Interpolate $\text{attr}/z$
  and $1/z$ in screen space, then divide.
  [Chapter 21 §21.2](../04-rasterization/21-msaa-and-attribute-interpolation.md#21-2-the-heckbert-moreton-1z-trick).
- **HitPoint** — record carrying the intersection data the
  raytracer uses for shading: distance, point, normal, UV,
  primitive pointer.
  [Chapter 3 §3.4](../01-foundations/03-rays-and-geometry.md#3-4-hitpoint-what-comes-back).
- **HitPointInterval** — sequence of in/out hit-point events
  along a ray. The data structure CSG operations work on.
  [Chapter 3 §3.5](../01-foundations/03-rays-and-geometry.md#3-5-hitpointinterval-in-out-in-out).
- **Homogeneous coordinates** — 4-vector representation of
  3D points and directions: $(x, y, z, 1)$ for points,
  $(x, y, z, 0)$ for directions.
  [Chapter 2 §2.1](../01-foundations/02-matrices-and-transforms.md#2-1-why-4-4).

## I

- **Implicit surface** — a surface defined by an equation
  $f(\mathbf{p}) = 0$ rather than an explicit triangle list.
  Spheres, planes, tori. Rendered by the raytracer.
  [Chapter 7](../02-ray-rendering/07-primitives-and-intersection.md).
- **Instance** — primitive that wraps another primitive in a
  transform. Same geometry, many positions / orientations.
  [Chapter 16](../03-scene-structure/16-instances-and-motion-blur.md).
- **Inverse-transpose** — the matrix $(M^{-1})^T$ used to
  transform normals correctly under non-uniform scale.
  [Chapter 2 §2.4](../01-foundations/02-matrices-and-transforms.md#2-4-point-vs-direction-vs-normal).

## J

- **Jittered sampler** — stratified sampler that places one
  sample per grid cell, randomized within the cell. The
  variance-reducing default.
  [Chapter 10 §10.2](../02-ray-rendering/10-sampling-and-anti-aliasing.md#10-2-the-three-samplers).

## L

- **Lambertian** — view-independent diffuse reflection model;
  the constant-BRDF that produces matte surfaces.
  [Chapter 8 §8.3](../02-ray-rendering/08-materials-and-brdfs.md#8-3-the-four-shipped-brdfs).
- **LDR** — Low Dynamic Range. 8-bit-per-channel display
  values, packed as `0x00RRGGBB`.
  [Chapter 12 §12.1](../02-ray-rendering/12-tone-mapping.md#12-1-why-hdr-exists).
- **LOD** — Level of Detail. Integer parameter controlling
  tessellation density and rasterizer rendering cost.
  [Chapter 17 §17.10](../04-rasterization/17-tessellation.md#17-10-picking-a-level-of-detail).

## M

- **Möller-Trumbore** — the canonical ray-triangle
  intersection algorithm (1997).
  [Chapter 7 §7.5](../02-ray-rendering/07-primitives-and-intersection.md#7-5-triangle-moller-trumbore).
- **Minkowski sum** — set $\{\mathbf{a} + \mathbf{b}\}$
  generated by sweeping one shape over another. Composable
  via support functions.
  [Chapter 14 §14.3](../03-scene-structure/14-csg.md#14-3-convex-composites-support-mapping).
- **Monte Carlo integration** — averaging many random
  samples to approximate an integral. The math behind both
  raytracer multi-sample-per-pixel and (eventually) path
  tracing.
  [Chapter 10 §10.1](../02-ray-rendering/10-sampling-and-anti-aliasing.md#10-1-aliasing-as-undersampling).
- **MSAA** — Multi-Sample Anti-Aliasing. Coverage-only
  super-sampling that runs the fragment shader once per
  pixel.
  [Chapter 21 §21.3](../04-rasterization/21-msaa-and-attribute-interpolation.md#21-3-msaa-coverage-sampling-not-shading-sampling).

## N

- **Normal** — unit vector perpendicular to a surface at a
  given point. Drives Lambertian shading and reflection
  direction.
  [Chapter 1 §1.5](../01-foundations/01-numbers-and-vectors.md#1-5-cross-product),
  [chapter 2 §2.4](../01-foundations/02-matrices-and-transforms.md#2-4-point-vs-direction-vs-normal).

## P

- **PCF** — Percentage-Closer Filtering. The shadow-map
  technique of averaging multiple depth comparisons in a
  kernel around the projected light-space point, producing a
  soft penumbra.
  [Chapter 9 §9.7](../02-ray-rendering/09-lights-and-shading.md#9-7-shadow-maps-for-the-rasterizer).
- **PCSS** — Percentage-Closer Soft Shadows. A shadow-map
  technique that first searches for blockers, estimates penumbra
  width from blocker-to-receiver depth, then applies PCF with that
  adaptive radius.
  [Chapter 9 §9.7](../02-ray-rendering/09-lights-and-shading.md#9-7-shadow-maps-for-the-rasterizer).
- **Peter Panning** — the visual artifact where a shadow
  detaches from its caster, caused by an over-aggressive
  shadow-map bias.
  [Chapter 9 §9.7](../02-ray-rendering/09-lights-and-shading.md#9-7-shadow-maps-for-the-rasterizer).
- **Phong** — the classical lobe-based reflection model
  combining a Lambertian diffuse base with a glossy
  specular highlight.
  [Chapter 8 §8.4](../02-ray-rendering/08-materials-and-brdfs.md#8-4-the-five-shipped-materials).
- **Pineda** — Juan Pineda's 1988 edge-function rasterization
  algorithm. The basis of the codebase's `core::rasterizeTriangle`.
  [Chapter 18 §18.2](../04-rasterization/18-the-rasterization-pipeline.md#18-2-the-edge-function-inside-test).
- **Pinhole camera** — the canonical infinite-depth-of-field
  perspective projection model.
  [Chapter 6 §6.2](../02-ray-rendering/06-cameras.md#6-2-pinhole-the-canonical-case).
- **PLY** — Stanford Triangle Format. The codebase's only
  mesh-import format and only untrusted input.
  [Chapter 25](../06-tools-and-io/25-ply-parsing.md).
- **Polsby-Popper** — circularity descriptor
  $4\pi \cdot \text{area} / \text{perimeter}^2$, normalized
  to $[0, 1]$.
  [Chapter 23 §23.3](../05-image-and-vision/23-blob-analysis-and-silhouettes.md#23-3-what-a-blob-carries).

## Q

- **Quartic** — fourth-degree polynomial. Solved by Ferrari's
  method during ray-torus intersection.
  [Chapter 7 §7.7](../02-ray-rendering/07-primitives-and-intersection.md#7-7-torus-a-quartic-root-problem).
- **Quaternion** — 4-tuple alternative representation of
  rotation, useful for SLERP-interpolation between
  orientations.
  [Chapter 2 §2.5](../01-foundations/02-matrices-and-transforms.md#2-5-quaternions-briefly).

## R

- **Radial variance** — standard deviation of boundary-point
  distance from the centroid, normalized by the mean. The
  shape-classifier's universal roundness metric.
  [Chapter 23 §23.3](../05-image-and-vision/23-blob-analysis-and-silhouettes.md#23-3-what-a-blob-carries).
- **Ray** — origin + direction + parameter $t$ that walks
  the line $\mathbf{r}(t) = \mathbf{o} + t\mathbf{d}$.
  [Chapter 3 §3.1](../01-foundations/03-rays-and-geometry.md#3-1-the-ray-type).
- **Reflection** — mirror direction
  $\mathbf{r} = \mathbf{i} - 2(\mathbf{i} \cdot \mathbf{n})\mathbf{n}$.
  Implemented by ReflectiveMaterial.
  [Chapter 8 §8.4](../02-ray-rendering/08-materials-and-brdfs.md#8-4-the-five-shipped-materials).
- **Reinhard** — the 2002 tonemap operator $y = x / (1 + x)$.
  Compresses everywhere, never saturates.
  [Chapter 12 §12.4](../02-ray-rendering/12-tone-mapping.md#12-4-reinhard-compressed-everywhere).
- **Refraction** — Snell's law direction redirect at a
  refractive interface. Implemented by PerfectTransmitter.
  [Chapter 8 §8.3](../02-ray-rendering/08-materials-and-brdfs.md#8-3-the-four-shipped-brdfs).

## S

- **SAH** — Surface Area Heuristic. The cost-formula-driven
  split-selection criterion for BVH construction.
  [Chapter 15 §15.3](../03-scene-structure/15-spatial-acceleration.md#15-3-the-surface-area-heuristic).
- **Shadow acne** — visual artifact where a surface appears to
  shadow itself, caused by an under-aggressive shadow-map
  bias.
  [Chapter 9 §9.7](../02-ray-rendering/09-lights-and-shading.md#9-7-shadow-maps-for-the-rasterizer).
- **Shadow map** — a depth-only buffer rendered from a light's
  point of view, used by the rasterizer at fragment shading
  time to decide whether a surface is in shadow.
  [Chapter 9 §9.7](../02-ray-rendering/09-lights-and-shading.md#9-7-shadow-maps-for-the-rasterizer).
- **Sampler** — generates sub-pixel and lens sample positions
  for anti-aliasing and depth-of-field.
  [Chapter 10](../02-ray-rendering/10-sampling-and-anti-aliasing.md).
- **Scheimpflug** — the geometric condition relating tilted
  view, lens, and focal planes in tilt-shift photography.
  [Chapter 6 §6.6](../02-ray-rendering/06-cameras.md#6-6-tilt-shift-a-tilted-focal-plane).
- **Shadow ray** — secondary ray cast from a hit point toward
  a light to test light-source visibility.
  [Chapter 9 §9.4](../02-ray-rendering/09-lights-and-shading.md#9-4-the-shadow-ray).
- **Silhouette** — outer-extreme sample set of a target-color
  region. The engine-agnostic shape primitive.
  [Chapter 23 §23.4](../05-image-and-vision/23-blob-analysis-and-silhouettes.md#23-4-the-silhouette-shortcut).
- **Slab method** — ray-vs-AABB intersection algorithm using
  three pairs of parallel planes.
  [Chapter 7 §7.4](../02-ray-rendering/07-primitives-and-intersection.md#7-4-box-the-slab-method).
- **SSE3** — Streaming SIMD Extensions 3. Used for the
  `Vector3<double>` / `Vector4<double>` and `Colord` hot-path
  specializations.
  [Chapter 1 §1.7](../01-foundations/01-numbers-and-vectors.md#1-7-the-sse3-specializations).
- **Stencil buffer** — per-pixel 8-bit buffer used as an
  application-defined fragment filter.
  [Chapter 19 §19.4](../04-rasterization/19-clipping-depth-stencil.md#19-4-the-stencil-buffer).
- **Stratified sampling** — sampling pattern that partitions
  the unit square into equal cells with one sample per cell.
  [Chapter 10 §10.3](../02-ray-rendering/10-sampling-and-anti-aliasing.md#10-3-the-stratification-invariant).
- **Sutherland-Hodgman** — the 1974 polygon-clipping
  algorithm against a half-space.
  [Chapter 19 §19.2](../04-rasterization/19-clipping-depth-stencil.md#19-2-sutherland-hodgman-in-homogeneous-clip-space).
- **Support function** — for a convex shape, returns the
  farthest point in a given direction. The basis of GJK and
  Minkowski-sum / convex-hull composition.
  [Chapter 14 §14.3](../03-scene-structure/14-csg.md#14-3-convex-composites-support-mapping).

## T

- **Tessellation** — converting an implicit primitive into a
  triangle-mesh approximation. The bridge from raytracer
  shapes to rasterizer shapes.
  [Chapter 17](../04-rasterization/17-tessellation.md).
- **Thin-lens camera** — finite-aperture camera that
  produces depth of field. Requires a multi-sample sampler.
  [Chapter 6 §6.5](../02-ray-rendering/06-cameras.md#6-5-thin-lens-depth-of-field).
- **Tonemap** — function from HDR `Colord` to LDR display
  pixels. Linear / Reinhard / ACES are the three operators.
  [Chapter 12](../02-ray-rendering/12-tone-mapping.md).

## U

- **UV** — texture coordinates $(s, t) \in [0, 1]^2$.
  [Chapter 11](../02-ray-rendering/11-textures.md).

## V

- **View plane** — the 2D pixel grid the camera projects
  through, plus the iteration order the renderer walks it
  in.
  [Chapter 13](../03-scene-structure/13-view-planes.md).

## W

- **Whitted** — the recursive ray-tracing algorithm
  introduced by Turner Whitted in 1980. The shipped
  raytracer.
  [Chapter 5](../02-ray-rendering/05-the-whitted-pipeline.md).
- **Wireframe** — render mode that draws only mesh edges,
  not filled surfaces.
  [Chapter 20](../04-rasterization/20-wireframe-rendering.md).

## Z

- **Z-buffer** — per-pixel depth buffer used to resolve
  visibility during rasterization.
  [Chapter 19 §19.3](../04-rasterization/19-clipping-depth-stencil.md#19-3-the-depth-test).

---

## See also

- [Top-level TOC](../README.md)
- [B. Bibliography](b-bibliography.md)
- [C. Source map](c-source-map.md)
