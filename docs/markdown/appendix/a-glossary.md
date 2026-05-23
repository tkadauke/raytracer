# Appendix A — Glossary

Hand-written; *not* generated. One line per entry, with a link
to the chapter where the concept is introduced. Drift is fixed
by editing this file.

## A

- **AABB** — Axis-Aligned Bounding Box. A rectangular box
  whose faces are parallel to the world-coordinate axes. Used
  for fast ray-rejection in BVH and Grid acceleration.
  [Rays and geometry: Bounding boxes](../foundations/rays-and-geometry.md#bounding-boxes).
- **ACES** — Academy Color Encoding System. The Narkowicz
  polynomial fit ships as one of the three tonemap operators.
  [Tone mapping: ACES (filmic-ish, punchy midtones)](../ray-rendering/tone-mapping.md#aces-filmic-ish-punchy-midtones).
- **Affine transform** — translation + rotation + scale +
  shear; the family of transforms preserving lines and
  parallelism. The transformation matrices used for scene
  placement.
  [Matrices and transforms: Why 4×4](../foundations/matrices-and-transforms.md#why-4-4).
- **Aliasing** — visible artifacts produced by
  undersampling a continuous signal. Fixed by Monte Carlo
  super-sampling (raytracer) or MSAA (rasterizer).
  [Sampling and anti-aliasing: Aliasing as undersampling](../ray-rendering/sampling-and-anti-aliasing.md#aliasing-as-undersampling).
- **AOV** — Arbitrary Output Variable. Per-pixel auxiliary
  buffer holding intermediate quantities (depth, normal,
  albedo). Future topic; not yet in the codebase.

## B

- **Barycentric coordinates** — three-tuple $(w_0, w_1, w_2)$
  with $\sum w_i = 1$, parameterizing points inside a
  triangle as a weighted average of its vertices.
  [Primitives and intersection: Triangle: Möller-Trumbore](../ray-rendering/primitives-and-intersection.md#triangle-moller-trumbore).
- **Bresenham** — Bresenham's all-octants integer line
  rasterizer (1965). The wireframe engine's per-edge
  rasterizer.
  [Wireframe rendering: Bresenham's line algorithm](../rasterization/wireframe-rendering.md#bresenhams-line-algorithm).
- **BRDF** — Bidirectional Reflectance Distribution Function.
  $f_r(\mathbf{w}_i, \mathbf{w}_o)$ — the directional
  reflectance of a surface point.
  [Materials and BRDFs: What a BRDF is](../ray-rendering/materials-and-brdfs.md#what-a-brdf-is).
- **BSDF** — Bidirectional Scattering Distribution Function.
  Container abstraction over BRDF + BTDF that unifies
  reflection and transmission behind one interface.
  [Materials and BRDFs: The `BSDF` interface](../ray-rendering/materials-and-brdfs.md#the-bsdf-interface).
- **BTDF** — Bidirectional Transmittance Distribution
  Function. The transmission analogue of BRDF, for materials
  that pass light through.
  [Materials and BRDFs: The four shipped BRDFs](../ray-rendering/materials-and-brdfs.md#the-four-shipped-brdfs).
- **BVH** — Bounding Volume Hierarchy. Binary tree of nested
  AABBs that prunes most of the scene per ray.
  [Spatial acceleration: The bounding volume hierarchy](../scene-structure/spatial-acceleration.md#the-bounding-volume-hierarchy).

## C

- **Centroid** — area-weighted center of a 2D region. Used by
  Blob and Silhouette as the reference point for radial-
  variance descriptors.
  [Blob analysis and silhouettes: What a blob carries](../image-and-vision/blob-analysis-and-silhouettes.md#what-a-blob-carries).
- **Clip space** — homogeneous 4D coordinates after the
  camera's projection but before the perspective divide.
  Where the rasterizer does its clipping.
  [Clipping, depth, stencil: Sutherland-Hodgman in homogeneous clip space](../rasterization/clipping-depth-stencil.md#sutherland-hodgman-in-homogeneous-clip-space).
- **CRTP** — Curiously Recurring Template Pattern. The
  template trick `Vector<N, T>` and `Matrix<N, T>` use to
  return the derived type from base-class operators.
  [Numbers and vectors: The shape of `Vector<N, T>`](../foundations/numbers-and-vectors.md#the-shape-of-vector-n-t).
- **CSG** — Constructive Solid Geometry. Boolean and
  support-mapping composition of primitives via
  HitPointInterval set operations.
  [Constructive solid geometry](../scene-structure/csg.md).
- **CullMode** — `Both` / `Back` / `Front` switch on the
  rasterizer that drops triangles by their projected
  screen-space winding.
  [Clipping, depth, stencil: Face culling](../rasterization/clipping-depth-stencil.md#face-culling).

## D

- **DDA** — Digital Differential Analyzer. The 3D
  cell-traversal algorithm the Grid acceleration structure
  uses.
  [Spatial acceleration: The uniform grid alternative](../scene-structure/spatial-acceleration.md#the-uniform-grid-alternative).
- **Depth buffer** — see *Z-buffer*.
- **DOF** — Depth of Field. The defocus blur produced by a
  finite-aperture camera.
  [Cameras: Thin-lens: depth of field](../ray-rendering/cameras.md#thin-lens-depth-of-field).
- **Dot product** — $\mathbf{a} \cdot \mathbf{b} = \lVert\mathbf{a}\rVert\lVert\mathbf{b}\rVert\cos\theta$;
  the workhorse 1D scalar derived from two vectors.
  [Numbers and vectors: Dot product](../foundations/numbers-and-vectors.md#dot-product).

## E

- **Edge function** — Pineda 1988 inside-test for triangle
  rasterization. The signed area of the sub-triangle formed
  by an edge and a candidate pixel.
  [The rasterization pipeline: The edge-function inside-test](../rasterization/the-rasterization-pipeline.md#the-edge-function-inside-test).
- **Equirectangular** — full-sphere $360° \times 180°$
  panorama projection. The format used for environment maps
  and VR sky textures.
  [Cameras: Wide-angle: spherical, fisheye, equirectangular](../ray-rendering/cameras.md#wide-angle-spherical-fisheye-equirectangular).

## F

- **Ferrari's method** — closed-form solution of the quartic
  equation. Used by the torus-ray intersection routine.
  [Primitives and intersection: Torus: a quartic root problem](../ray-rendering/primitives-and-intersection.md#torus-a-quartic-root-problem).
- **Fragment shader** — the per-pixel function in the
  rasterizer pipeline that turns interpolated attributes
  plus a material into a final color.
  [The rasterization pipeline: Fragment shading](../rasterization/the-rasterization-pipeline.md#fragment-shading).
- **Fresnel** — the angle-dependent ratio of reflected to
  transmitted light at a refractive surface. Used by
  TransparentMaterial.
  [Materials and BRDFs: The five shipped materials](../ray-rendering/materials-and-brdfs.md#the-five-shipped-materials).

## G

- **Gamma encode** — the nonlinear mapping from linear-RGB
  to sRGB display values. The codebase currently omits this
  step (see [Color and buffers](../foundations/color-and-buffers.md)).
  [Color and buffers: Linear RGB vs sRGB](../foundations/color-and-buffers.md#linear-rgb-vs-srgb).
- **GJK** — Gilbert-Johnson-Keerthi distance algorithm
  (1988). Computes ray intersections with convex shapes
  defined only by a support function.
  [Constructive solid geometry: GJK: ray intersection on a support function](../scene-structure/csg.md#gjk-ray-intersection-on-a-support-function).
- **Glossy specular** — the Phong cosine-power lobe BRDF.
  [Materials and BRDFs: The four shipped BRDFs](../ray-rendering/materials-and-brdfs.md#the-four-shipped-brdfs).

## H

- **HDR** — High Dynamic Range. Float-precision colors with
  no upper clamp, accumulated through the render pipeline
  before tonemap.
  [Tone mapping: Why HDR exists](../ray-rendering/tone-mapping.md#why-hdr-exists).
- **Heckbert-Moreton** — the 1991 perspective-correct
  attribute-interpolation trick. Interpolate $\text{attr}/z$
  and $1/z$ in screen space, then divide.
  [MSAA and attribute interpolation: The Heckbert-Moreton $1/z$ trick](../rasterization/msaa-and-attribute-interpolation.md#the-heckbert-moreton-1z-trick).
- **HitPoint** — record carrying the intersection data the
  raytracer uses for shading: distance, point, normal, UV,
  primitive pointer.
  [Rays and geometry: `HitPoint`: what comes back](../foundations/rays-and-geometry.md#hitpoint-what-comes-back).
- **HitPointInterval** — sequence of in/out hit-point events
  along a ray. The data structure CSG operations work on.
  [Rays and geometry: `HitPointInterval`: in / out / in / out](../foundations/rays-and-geometry.md#hitpointinterval-in-out-in-out).
- **Homogeneous coordinates** — 4-vector representation of
  3D points and directions: $(x, y, z, 1)$ for points,
  $(x, y, z, 0)$ for directions.
  [Matrices and transforms: Why 4×4](../foundations/matrices-and-transforms.md#why-4-4).

## I

- **Implicit surface** — a surface defined by an equation
  $f(\mathbf{p}) = 0$ rather than an explicit triangle list.
  Spheres, planes, tori. Rendered by the raytracer.
  [Primitives and intersection](../ray-rendering/primitives-and-intersection.md).
- **Instance** — primitive that wraps another primitive in a
  transform. Same geometry, many positions / orientations.
  [Instances and motion blur](../scene-structure/instances-and-motion-blur.md).
- **Inverse-transpose** — the matrix $(M^{-1})^T$ used to
  transform normals correctly under non-uniform scale.
  [Matrices and transforms: Point vs direction vs normal](../foundations/matrices-and-transforms.md#point-vs-direction-vs-normal).

## J

- **Jittered sampler** — stratified sampler that places one
  sample per grid cell, randomized within the cell. The
  variance-reducing default.
  [Sampling and anti-aliasing: The three samplers](../ray-rendering/sampling-and-anti-aliasing.md#the-three-samplers).

## L

- **Lambertian** — view-independent diffuse reflection model;
  the constant-BRDF that produces matte surfaces.
  [Materials and BRDFs: The four shipped BRDFs](../ray-rendering/materials-and-brdfs.md#the-four-shipped-brdfs).
- **LDR** — Low Dynamic Range. 8-bit-per-channel display
  values, packed as `0x00RRGGBB`.
  [Tone mapping: Why HDR exists](../ray-rendering/tone-mapping.md#why-hdr-exists).
- **LOD** — Level of Detail. Integer parameter controlling
  tessellation density and rasterizer rendering cost.
  [Tessellation: Picking a level of detail](../rasterization/tessellation.md#picking-a-level-of-detail).

## M

- **Möller-Trumbore** — the canonical ray-triangle
  intersection algorithm (1997).
  [Primitives and intersection: Triangle: Möller-Trumbore](../ray-rendering/primitives-and-intersection.md#triangle-moller-trumbore).
- **Minkowski sum** — set $\{\mathbf{a} + \mathbf{b}\}$
  generated by sweeping one shape over another. Composable
  via support functions.
  [Constructive solid geometry: Convex composites: support mapping](../scene-structure/csg.md#convex-composites-support-mapping).
- **Monte Carlo integration** — averaging many random
  samples to approximate an integral. The math behind both
  raytracer multi-sample-per-pixel and (eventually) path
  tracing.
  [Sampling and anti-aliasing: Aliasing as undersampling](../ray-rendering/sampling-and-anti-aliasing.md#aliasing-as-undersampling).
- **MSAA** — Multi-Sample Anti-Aliasing. Coverage-only
  super-sampling that runs the fragment shader once per
  pixel.
  [MSAA and attribute interpolation: MSAA: coverage sampling, not shading sampling](../rasterization/msaa-and-attribute-interpolation.md#msaa-coverage-sampling-not-shading-sampling).

## N

- **Normal** — unit vector perpendicular to a surface at a
  given point. Drives Lambertian shading and reflection
  direction.
  [Numbers and vectors: Cross product](../foundations/numbers-and-vectors.md#cross-product),
  [Matrices and transforms: Point vs direction vs normal](../foundations/matrices-and-transforms.md#point-vs-direction-vs-normal).

## P

- **PCF** — Percentage-Closer Filtering. The shadow-map
  technique of averaging multiple depth comparisons in a
  kernel around the projected light-space point, producing a
  soft penumbra.
  [Lights and shading: Shadow maps for the rasterizer](../ray-rendering/lights-and-shading.md#shadow-maps-for-the-rasterizer).
- **PCSS** — Percentage-Closer Soft Shadows. A shadow-map
  technique that first searches for blockers, estimates penumbra
  width from blocker-to-receiver depth, then applies PCF with that
  adaptive radius.
  [Lights and shading: Shadow maps for the rasterizer](../ray-rendering/lights-and-shading.md#shadow-maps-for-the-rasterizer).
- **Peter Panning** — the visual artifact where a shadow
  detaches from its caster, caused by an over-aggressive
  shadow-map bias.
  [Lights and shading: Shadow maps for the rasterizer](../ray-rendering/lights-and-shading.md#shadow-maps-for-the-rasterizer).
- **Phong** — the classical lobe-based reflection model
  combining a Lambertian diffuse base with a glossy
  specular highlight.
  [Materials and BRDFs: The five shipped materials](../ray-rendering/materials-and-brdfs.md#the-five-shipped-materials).
- **Pineda** — Juan Pineda's 1988 edge-function rasterization
  algorithm. The basis of the codebase's `core::rasterizeTriangle`.
  [The rasterization pipeline: The edge-function inside-test](../rasterization/the-rasterization-pipeline.md#the-edge-function-inside-test).
- **Pinhole camera** — the canonical infinite-depth-of-field
  perspective projection model.
  [Cameras: Pinhole: the canonical case](../ray-rendering/cameras.md#pinhole-the-canonical-case).
- **PLY** — Stanford Triangle Format. The codebase's only
  mesh-import format and only untrusted input.
  [PLY parsing](../tools-and-io/ply-parsing.md).
- **Polsby-Popper** — circularity descriptor
  $4\pi \cdot \text{area} / \text{perimeter}^2$, normalized
  to $[0, 1]$.
  [Blob analysis and silhouettes: What a blob carries](../image-and-vision/blob-analysis-and-silhouettes.md#what-a-blob-carries).

## Q

- **Quartic** — fourth-degree polynomial. Solved by Ferrari's
  method during ray-torus intersection.
  [Primitives and intersection: Torus: a quartic root problem](../ray-rendering/primitives-and-intersection.md#torus-a-quartic-root-problem).
- **Quaternion** — 4-tuple alternative representation of
  rotation, useful for SLERP-interpolation between
  orientations.
  [Matrices and transforms: Quaternions, briefly](../foundations/matrices-and-transforms.md#quaternions-briefly).

## R

- **Radial variance** — standard deviation of boundary-point
  distance from the centroid, normalized by the mean. The
  shape-classifier's universal roundness metric.
  [Blob analysis and silhouettes: What a blob carries](../image-and-vision/blob-analysis-and-silhouettes.md#what-a-blob-carries).
- **Ray** — origin + direction + parameter $t$ that walks
  the line $\mathbf{r}(t) = \mathbf{o} + t\mathbf{d}$.
  [Rays and geometry: The `Ray` type](../foundations/rays-and-geometry.md#the-ray-type).
- **Reflection** — mirror direction
  $\mathbf{r} = \mathbf{i} - 2(\mathbf{i} \cdot \mathbf{n})\mathbf{n}$.
  Implemented by ReflectiveMaterial.
  [Materials and BRDFs: The five shipped materials](../ray-rendering/materials-and-brdfs.md#the-five-shipped-materials).
- **Reinhard** — the 2002 tonemap operator $y = x / (1 + x)$.
  Compresses everywhere, never saturates.
  [Tone mapping: Reinhard (compressed everywhere)](../ray-rendering/tone-mapping.md#reinhard-compressed-everywhere).
- **Refraction** — Snell's law direction redirect at a
  refractive interface. Implemented by PerfectTransmitter.
  [Materials and BRDFs: The four shipped BRDFs](../ray-rendering/materials-and-brdfs.md#the-four-shipped-brdfs).

## S

- **SAH** — Surface Area Heuristic. The cost-formula-driven
  split-selection criterion for BVH construction.
  [Spatial acceleration: The Surface Area Heuristic](../scene-structure/spatial-acceleration.md#the-surface-area-heuristic).
- **Shadow acne** — visual artifact where a surface appears to
  shadow itself, caused by an under-aggressive shadow-map
  bias.
  [Lights and shading: Shadow maps for the rasterizer](../ray-rendering/lights-and-shading.md#shadow-maps-for-the-rasterizer).
- **Shadow map** — a depth-only buffer rendered from a light's
  point of view, used by the rasterizer at fragment shading
  time to decide whether a surface is in shadow.
  [Lights and shading: Shadow maps for the rasterizer](../ray-rendering/lights-and-shading.md#shadow-maps-for-the-rasterizer).
- **Sampler** — generates sub-pixel and lens sample positions
  for anti-aliasing and depth-of-field.
  [Sampling and anti-aliasing](../ray-rendering/sampling-and-anti-aliasing.md).
- **Scheimpflug** — the geometric condition relating tilted
  view, lens, and focal planes in tilt-shift photography.
  [Cameras: Tilt-shift: a tilted focal plane](../ray-rendering/cameras.md#tilt-shift-a-tilted-focal-plane).
- **Shadow ray** — secondary ray cast from a hit point toward
  a light to test light-source visibility.
  [Lights and shading: The shadow ray](../ray-rendering/lights-and-shading.md#the-shadow-ray).
- **Silhouette** — outer-extreme sample set of a target-color
  region. The engine-agnostic shape primitive.
  [Blob analysis and silhouettes: The `Silhouette` shortcut](../image-and-vision/blob-analysis-and-silhouettes.md#the-silhouette-shortcut).
- **Slab method** — ray-vs-AABB intersection algorithm using
  three pairs of parallel planes.
  [Primitives and intersection: Box: the slab method](../ray-rendering/primitives-and-intersection.md#box-the-slab-method).
- **SSE3** — Streaming SIMD Extensions 3. Used for the
  `Vector3<double>` / `Vector4<double>` and `Colord` hot-path
  specializations.
  [Numbers and vectors: The SSE3 specializations](../foundations/numbers-and-vectors.md#the-sse3-specializations).
- **Stencil buffer** — per-pixel 8-bit buffer used as an
  application-defined fragment filter.
  [Clipping, depth, stencil: The stencil buffer](../rasterization/clipping-depth-stencil.md#the-stencil-buffer).
- **Stratified sampling** — sampling pattern that partitions
  the unit square into equal cells with one sample per cell.
  [Sampling and anti-aliasing: The stratification invariant](../ray-rendering/sampling-and-anti-aliasing.md#the-stratification-invariant).
- **Sutherland-Hodgman** — the 1974 polygon-clipping
  algorithm against a half-space.
  [Clipping, depth, stencil: Sutherland-Hodgman in homogeneous clip space](../rasterization/clipping-depth-stencil.md#sutherland-hodgman-in-homogeneous-clip-space).
- **Support function** — for a convex shape, returns the
  farthest point in a given direction. The basis of GJK and
  Minkowski-sum / convex-hull composition.
  [Constructive solid geometry: Convex composites: support mapping](../scene-structure/csg.md#convex-composites-support-mapping).

## T

- **Tessellation** — converting an implicit primitive into a
  triangle-mesh approximation. The bridge from raytracer
  shapes to rasterizer shapes.
  [Tessellation](../rasterization/tessellation.md).
- **Thin-lens camera** — finite-aperture camera that
  produces depth of field. Requires a multi-sample sampler.
  [Cameras: Thin-lens: depth of field](../ray-rendering/cameras.md#thin-lens-depth-of-field).
- **Tonemap** — function from HDR `Colord` to LDR display
  pixels. Linear / Reinhard / ACES are the three operators.
  [Tone mapping](../ray-rendering/tone-mapping.md).

## U

- **UV** — texture coordinates $(s, t) \in [0, 1]^2$.
  [Textures](../ray-rendering/textures.md).

## V

- **View plane** — the 2D pixel grid the camera projects
  through, plus the iteration order the renderer walks it
  in.
  [View planes](../scene-structure/view-planes.md).

## W

- **Whitted** — the recursive ray-tracing algorithm
  introduced by Turner Whitted in 1980. The shipped
  raytracer.
  [The Whitted pipeline](../ray-rendering/the-whitted-pipeline.md).
- **Wireframe** — render mode that draws only mesh edges,
  not filled surfaces.
  [Wireframe rendering](../rasterization/wireframe-rendering.md).

## Z

- **Z-buffer** — per-pixel depth buffer used to resolve
  visibility during rasterization.
  [Clipping, depth, stencil: The depth test](../rasterization/clipping-depth-stencil.md#the-depth-test).

---

## See also

- [Top-level TOC](../README.md)
- [B. Bibliography](b-bibliography.md)
- [C. Source map](c-source-map.md)
