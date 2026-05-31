# Tessellation

The raytracer in Ray rendering works on **implicit surfaces** — a
sphere is the set of points satisfying $|\mathbf{p} -
\mathbf{c}|^2 = r^2$, and the renderer asks each primitive
*does this ray intersect you?* by solving the equation. That
works because rays only need single hit-point answers; the
surface itself never has to be enumerated.

A rasterizer can't do this. Rasterization works pixel-by-pixel
over the *projection* of an explicit surface onto screen space,
and an implicit surface has no projection — there's no list of
points to project. A rasterizer needs **triangles**: an explicit
list of vertices and faces that the projector can walk one at a
time.

The bridge between the two is **tessellation**: the conversion
of an implicit primitive into a triangle mesh approximation.
This chapter is about how the codebase does that conversion,
per-primitive, with one knob (level of detail) controlling the
fidelity-vs-speed trade-off.

By the end of this chapter you should know:

- the `tessellate(int lod) → Mesh` contract,
- the `Mesh` type's representation (vertices with normals and
  UVs, faces as vertex-index lists),
- the per-primitive strategies the codebase ships (sphere
  lat-long grid, disk fan, cylinder quad strip, torus
  ring-of-rings, etc.),
- how composite primitives recursively assemble their
  children's meshes,
- and the special case of `Instance` — UVs pass through, but
  positions and normals get transformed.

## <a id="the-contract"></a>The contract
[`Primitive::tessellate(int lod)`](../../../include/render/primitives/Primitive.h)
returns a
[`std::shared_ptr<Mesh>`](../../../include/core/geometry/Mesh.h)
approximating the primitive's surface. The integer `lod`
controls the density of the triangulation: higher values
produce more triangles, smoother silhouettes, and more memory.
Lower values produce fewer triangles, visible faceting, and
faster rasterization.

Each primitive picks a sensible scaling formula. A sphere's
segment count scales as $16 \cdot 2^{\text{lod}}$ — `lod = 0`
gives 16 segments around the equator, `lod = 1` gives 32, and
so on. A disk picks the same formula. A torus takes both
ring-segments and tube-segments and scales each.

The function is pure: same primitive plus same lod produces
the same mesh, every time. The wireframe and rasterizer engines
call it once per primitive at scene build time, cache the
result, and don't re-tessellate during interactive viewport
manipulation.

## <a id="the-mesh-type"></a>The `Mesh` type
A mesh is two arrays:

- **Vertices** — a list of `Mesh::Vertex` records, each
  containing a 3D position, a surface normal (unit length), and
  a 2D [UV](../appendix/a-glossary.md#u) coordinate.
- **Faces** — a list of `std::vector<int>` records, each
  containing the vertex indices of one polygon. The vertices
  must be coplanar; faces with fewer than three vertices throw
  `InvalidMeshFaceException`.

The vertex layout is the canonical "vertex with attributes"
form that every rasterizer in the world uses. The normal field
matters for smooth shading ([The rasterization pipeline](the-rasterization-pipeline.md) describes how the
rasterizer interpolates normals across a triangle). The UV
field matters for texture sampling (the rasterizer carries the
interpolated UV through to the fragment shader, where the
material's texture lookup happens).

Faces are *polygons*, not triangles, because some primitives
naturally produce quads. The mesh's `triangleIterator` yields
triangles by fan-decomposing each polygon at face-iteration
time. Most consumers — including the rasterizer — go through
that iterator and never see the polygons directly.

## <a id="sphere-lat-long-grid"></a>Sphere: lat-long grid
A sphere's textbook tessellation places vertices on a regular
lat-long grid: longitude varies from 0 to $2\pi$ around the
equator, latitude varies from $-\pi/2$ to $+\pi/2$ between the
poles, and the surface is the cross product of the two.

```
        ⎯⎯⎯⎯⎯  pole
       /     \
      / / | \ \    ← N latitude rings, each with M longitude
     |  | |  |  |    segments
      \ \ | / /
       \     /
        ⎯⎯⎯⎯⎯  pole
```

The widget shows the construction:

<!-- widget: sphere_tessellate -->

`Sphere::tessellate(lod)` produces a quad mesh of
`segments × (segments / 2)` quads (where `segments = 16 << lod`),
plus one triangle fan at each pole because the lat-long grid
collapses the longitudinal segments into a single point at the
poles. The quads at the equator are wide; the quads near the
poles are narrow; everything is bounded by a unit-radius sphere
in local space.

The vertex normals are radial — a sphere's surface normal at
any point is the unit vector from the center to that point, so
the normal *equals* the position (after normalization) when the
sphere is at the origin with radius 1. The UV coords map
longitude to $u$ and latitude to $v$, both in $[0, 1]$, which
gives the sphere a textbook lat-long parameterization.

Higher LODs give smoother silhouettes. A [LOD](../appendix/a-glossary.md#l)-0 sphere has 16
segments around — visibly polygonal at any reasonable render
size. LOD 3 (128 segments) is essentially indistinguishable
from a true sphere at typical render resolutions.

## <a id="disk-triangle-fan"></a>Disk: triangle fan
A disk is flat, so the tessellation is a 2D problem dressed in
3D coordinates. The strategy: place one vertex at the disk's
center, then `N` vertices around the rim, then connect each
adjacent pair of rim vertices to the center to form a triangle
fan. The widget:

<!-- widget: disk_tessellate -->

`Disk::tessellate(lod)` produces $1 + N$ vertices and $N$
triangles, with $N = 16 << \text{lod}$. The center vertex's
normal is the disk's plane normal; every rim vertex shares the
same normal because the disk is flat. The UV coords map the
disk into the unit square: the center is $(0.5, 0.5)$, and the
rim is on the unit circle within the square.

This is the simplest "non-trivial" tessellation in the
codebase: one center, a ring of rim vertices, fan-connected.
The same recipe shows up in many other primitives (sphere
poles, end-caps for closed cylinders, decorative geometry).

## <a id="open-cylinder-quad-strip-with-seam-duplicate-uvs"></a>Open cylinder: quad strip with seam-duplicate UVs
A cylinder, as a tessellation, is a single quad strip wrapped
around the y-axis. The strategy: place `N` vertices around the
bottom rim, `N` around the top, and connect each adjacent
column of (bottom, top) into a quad. The widget shows the
construction with adjustable segment count:

<!-- widget: open_cylinder_tessellate -->

The interesting detail is the **UV seam duplication**. The
cylinder wraps in $u$ — the first column at $u = 0$ and the
last column at $u = 1$ describe geometrically the *same*
position in 3D. A naive tessellation would identify those
columns: one set of vertices, used by both adjacent quads.

That breaks textures. A textured cylinder with `u = 0` mapped
to the left edge of the texture and `u = 1` mapped to the right
edge needs the seam vertex to carry *both* UVs at once — the
quad on the seam's left side wants $u = 1$, the quad on the
seam's right side wants $u = 0$. A single shared vertex can't
provide both.

The solution is **duplicate vertices at the seam**: the
geometry is the same, but each side of the seam gets its own
vertex with its own UV. `OpenCylinder::tessellate` produces
`2 × (segments + 1)` vertices for `segments` quads in the strip
— the extra column at `segments + 1` carries the wrap-around UV
that the closing quad needs. The texture then samples cleanly
across the seam.

Vertex normals point radially outward from the cylinder's axis
— the cylinder's surface normal at any point is the unit vector
from the axis to that point. With `numSegments` quads around,
adjacent vertices have slightly different normals, and the
rasterizer's smooth-shading interpolation produces the
characteristic curved cylinder appearance even at low LODs.

## <a id="torus-ring-of-rings"></a>Torus: ring of rings
A torus is the most geometrically interesting primitive in this
chapter. The tessellation is two-dimensional in topology but
must close in both dimensions — the major loop wraps around
the ring axis, and the minor loop wraps around the tube cross-
section.

The widget:

<!-- widget: torus_tessellate -->

`Torus::tessellate(lod)` produces `(major_segments + 1) ×
(minor_segments + 1)` vertices: a regular grid in
$(\theta_{\text{major}}, \theta_{\text{minor}})$ space, with
both dimensions getting the seam duplication from
[Open cylinder: quad strip with seam-duplicate UVs](#open-cylinder-quad-strip-with-seam-duplicate-uvs). The
faces are quads, just like the cylinder's. The vertex normals
are the gradient of the torus's implicit equation, evaluated at
each grid point — analytically derived in
[Primitives and intersection: Torus: a quartic root problem](../ray-rendering/primitives-and-intersection.md#torus-a-quartic-root-problem).

The UVs map the major loop to $u$ and the minor loop to $v$,
both in $[0, 1]$. A texture applied with this parameterization
wraps once around the ring and once around the tube — the
canonical "checker pattern on a torus" you see in textbooks.

The tessellation is the rasterizer's *only* path to rendering a
torus. The raytracer's quartic root finding doesn't apply to a
rasterizer; the rasterizer wants triangles, and `tessellate` is
how it gets them.

## <a id="the-trivial-primitives"></a>The trivial primitives
A few primitives have one-line tessellations:

- [`Box`](../../../include/render/primitives/Box.h) — six
  rectangles, each a quad. 8 vertices total (the box's
  corners), 6 quads. UVs map each face's surface to the unit
  square.
- [`Triangle`](../../../include/render/primitives/Triangle.h) —
  one triangle. 3 vertices, 1 face. UVs are barycentric-style
  at the corners.
- [`Rectangle`](../../../include/render/primitives/Rectangle.h)
  — one quad. 4 vertices, 1 face. UVs map the rectangle to the
  unit square.
- [`Plane`](../../../include/render/primitives/Plane.h) —
  *empty mesh*. Planes are infinite, and an infinite mesh has
  to be clipped to a finite region before tessellation. The
  override returns an empty mesh and emits a warning. To
  rasterize a plane, replace it with a `Rectangle` of the
  desired finite extent.

The trivial primitives don't have a `lod` parameter that does
anything — increasing the LOD on a triangle still produces one
triangle. The signature carries `lod` for uniformity with the
parametric primitives, not because subdivision is happening.

## <a id="composites-recursion"></a>Composites: recursion
[`Composite`](../../../include/render/primitives/Composite.h)
holds a list of children, and its `tessellate` recursively
calls each child's `tessellate` and concatenates the results.
The vertex indices need remapping when children's meshes are
merged — child A's vertex 0 and child B's vertex 0 are
different vertices in the combined mesh — so the implementation
walks each child's faces and offsets the indices by the
running vertex count.

This works for any composite. `Grid` and `Scene`
([Spatial acceleration](../scene-structure/spatial-acceleration.md))
inherit `Composite::tessellate` unchanged because their child
geometry is exactly Composite's child list. The [CSG](../appendix/a-glossary.md#c) composites
([Constructive solid geometry](../scene-structure/csg.md)) override with
empty meshes — mesh-Boolean operations aren't implemented, as
that chapter notes.

## <a id="instances-transform-vertex-points-and-normals"></a>Instances: transform vertex points and normals
[`Instance`](../../../include/render/primitives/Instance.h)
wraps a primitive in a transform. Its tessellation transforms
the wrapped primitive's mesh:

- **Vertex points** transform by the instance's
  `m_pointMatrix` — straight matrix-vector multiplication.
- **Vertex normals** transform by the instance's
  `m_normalMatrix` (the inverse-transpose) and re-normalize, to
  preserve perpendicularity under non-uniform scale. This is
  exactly the same machinery from
  [Matrices and transforms: The four-matrix dance: `Instance`](../foundations/matrices-and-transforms.md#the-four-matrix-dance-instance).
- **Vertex UVs** pass through unchanged — UVs are surface
  parameterizations, not 3D-space attributes, and the spatial
  transform doesn't apply to them.

For motion-blurred instances
([Instances and motion blur: Velocity and motion blur](../scene-structure/instances-and-motion-blur.md#velocity-and-motion-blur)),
the tessellation captures the instance at `timeSample = 0`. A
time-aware rasterizer would re-tessellate per frame; the
current rasterizer caches the LOD-0 mesh and applies the
*static* transform, so motion-blurred instances render as
their `timeSample = 0` configuration.

## <a id="picking-a-level-of-detail"></a>Picking a level of detail
The right LOD is a balance between visual quality and render
time. The rasterizer's per-frame cost scales roughly linearly
with the triangle count — twice as many triangles is twice as
much vertex transform, clip, and fragment shading work. For
interactive previews, a low LOD (0–2) is the right pick. For
final output, the higher the LOD the smoother the silhouette,
but most scenes look indistinguishable beyond LOD 4.

The wireframe engine
([Wireframe rendering](wireframe-rendering.md)) is the LOD-tuning
visualization: rendering the same scene at LOD 0, 1, 2, 3
shows the silhouette progression at the wire level, where
faceting is most visible because there's no shading to hide it.

Raster preview intent adds a second, screen-space control on top
of the integer LOD. The `preview`, `balanced`, and `final`
tessellation quality presets set a maximum projected error in
pixels, and an advanced max screen-space error override can set
that threshold directly. During raster triangle emission, finite
primitives report their projected size; if the requested error
budget allows a cheaper variant, the emitter asks for a lower LOD
and records the reduction in raster metrics. Repeated source-backed
parts share cached LOD variants, so a large imported model can avoid
rebuilding the same small curved part many times.

`final` quality preserves the explicitly requested LOD unless an
override is set. That keeps high-quality output available while
letting interactive previews spend fewer triangles on features that
are only a few pixels wide.

## <a id="curves-ribbons-tubes-overlays"></a>Curves: ribbons, tubes, overlays
[`core::Polyline`](../../../include/core/geometry/Polyline.h)
stores path-like data as ordered 3D points, where segment `i`
connects point `i` to point `i + 1`. Whole-curve metadata lives
on the shared [`core::Curve`](../../../include/core/geometry/Curve.h)
base, and per-segment metadata lives beside the derived segment.
That split lets importers preserve both file-level data and
segment-varying attributes such as G-code feed rate, route type,
molecule chain, trajectory time, or simulation phase.

[`render::Curve`](../../../include/render/primitives/Curve.h)
turns that path into visible output. In ribbon mode, every
non-zero segment becomes one quad with the requested width. In
tube mode, every non-zero segment becomes a ring-pair mesh; the
`lod` value increases the number of tube sides. Both modes feed
ordinary mesh-consuming engines, so the rasterizer, wireframe
renderer, and exporters can handle curves through the same
`Primitive::tessellate` contract as spheres and boxes.

Curves also have a semantic overlay path. Instead of building
physical faces, `forEachCurveOverlaySegment` exposes the original
center line so render graph overlays can draw thin path strokes
even when the curve width is zero. If a
[`core::AttributeColorMap`](../../../include/core/geometry/AttributeColorMap.h)
is attached, scalar or categorical segment attributes become
deterministic colors for ribbon faces, tube faces, or overlay
segments. Missing attributes fall back to the curve's material or
default overlay color.

## <a id="exercises"></a>Exercises
1. Predict the triangle count for `Sphere::tessellate(0)`,
   `Sphere::tessellate(1)`, and `Sphere::tessellate(3)`.
   Compare to the count for `Disk::tessellate(0)`. Where does
   the sphere's bigger count come from?
2. The cylinder's seam duplication adds one extra vertex
   column — `2 × (segments + 1)` vertices instead of
   `2 × segments`. The torus duplicates *both* dimensions.
   Estimate the torus's vertex count for `lod = 2`.
3. The rasterizer caches each primitive's tessellation at scene
   build time. What happens if you change a primitive's
   geometry (e.g. resize a sphere) and re-render without
   re-tessellating? Where would the bug appear in the
   rendered output?
4. The `Composite::tessellate` recursion concatenates child
   meshes and remaps indices. What's the asymptotic cost of
   tessellating a Composite of $N$ children, each with $M$
   vertices? Why doesn't it dominate the rasterizer's per-frame
   cost?

## See also

- Volume index: [Rasterization](README.md)
- Previous:
  [Instances and motion blur](../scene-structure/instances-and-motion-blur.md)
- Next: [The rasterization pipeline](the-rasterization-pipeline.md)
- Implicit-surface side:
  [Primitives and intersection](../ray-rendering/primitives-and-intersection.md)
- Instance transform math:
  [The four-matrix dance: `Instance`](../foundations/matrices-and-transforms.md#the-four-matrix-dance-instance)

## Source anchors

<!-- source-anchors -->
- `include/core/geometry/Mesh.h`
- `include/core/geometry/Curve.h`
- `include/core/geometry/Polyline.h`
- `include/core/geometry/AttributeColorMap.h`
- `include/engine/raster/detail/RasterTriangleEmitter.h`
- `include/render/primitives/Primitive.h`
- `include/render/primitives/Curve.h`
- `include/render/primitives/Sphere.h`
- `include/render/primitives/Disk.h`
- `include/render/primitives/OpenCylinder.h`
- `include/render/primitives/Torus.h`
- `include/render/primitives/Box.h`
- `include/render/primitives/Triangle.h`
- `include/render/primitives/Rectangle.h`
- `include/render/primitives/Composite.h`
- `include/render/primitives/Instance.h`
<!-- /source-anchors -->
