# Primitives and intersection

A primitive is anything a ray can hit. The renderer's intersection
loop walks a list of them, asks each one *do you intersect this
ray?*, and returns the closest answer it gets back. That makes
this chapter the place where every shipped shape gets defined,
both as math and as code.

By the end you should know:

- the `Primitive` interface that every concrete shape implements,
- the analytic quadric that solves ray-sphere intersection,
- the one-line plane intersection formula,
- the slab method for ray-box intersection,
- [Möller-Trumbore](../appendix/a-glossary.md#m) for ray-triangle intersection,
- how disks and open cylinders fall out of plane and quadric math,
- the quartic-root-finding work that makes ray-torus intersection
  possible,
- and how `FlatMeshTriangle` / `SmoothMeshTriangle` carry mesh
  vertex data through the same triangle-intersection routine.

## <a id="the-primitive-interface"></a>The `Primitive` interface
The abstract base class is
[`render::Primitive`](../../../include/render/primitives/Primitive.h).
The two methods every concrete primitive must implement are:

```cpp
virtual const Primitive* intersect(
    const Rayd&        ray,
    HitPointInterval&  hitPoints,
    render::State&     state) const = 0;

virtual bool intersects(
    const Rayd&        ray,
    render::State&     state) const = 0;
```

The first returns a pointer to the primitive that was hit (or
`nullptr` for a miss) and populates `hitPoints` with the
in/out interval pair from
[Rays and geometry: `HitPointInterval`: in / out / in / out](../foundations/rays-and-geometry.md#hitpointinterval-in-out-in-out).
The second is the cheap boolean form used by shadow rays — it
returns `true` if the ray hits anything along its valid range
without computing a hit point. The split exists because shadow
rays don't need to know *where* they hit, only *whether*; doing
the cheaper test saves work for the most common shading-time
question.

The primitive hierarchy also exposes packet entry points for four-wide
ray groups. `intersectPacket(...)` is the lightweight distance-only form used
when traversal only needs hit masks and near/far distances. `intersectPacketHits(...)`
materializes the closest hit primitive and `HitPoint` for each lane, which is
the shape needed by wavefront renderers before they hand hit lanes to the
shading stage. It also accepts per-lane `State` pointers, so hit/miss counters
and trace events stay attached to the path that owned each ray. The default
packet-hit implementation falls back to scalar `intersect(...)`, while
composites merge child packet hits and keep the closest positive hit per lane.
BVH nodes keep the same materialized-hit contract while traversing their tree,
so wavefront renderers can ask the accelerated scene for packet-shaped frontier
hits without losing the primitive and hit-point data needed by material shading.
Leaf primitives can then override the materialized packet form directly;
`Sphere`, `Plane`, `Triangle`, `Box`, and mesh-backed triangle leaves already
do this so common analytic geometry and triangle-heavy BVHs avoid the generic
interval fallback on wavefront packet frontiers. Wrapper primitives matter too:
`Instance` transforms static ray packets into local space before delegating and
then transforms materialized hits back to world space, while `MeshPrimitive`
forwards packet-hit requests to its triangle leaves and preserves mesh-level
material fallback.

Most primitives also override:

- `calculateBoundingBox()` — returns the [AABB](../appendix/a-glossary.md#a) enclosing the
  primitive in world space. The [BVH](../appendix/a-glossary.md#b) builder
  ([Spatial acceleration](../scene-structure/spatial-acceleration.md))
  reads this when it builds the scene's spatial structure.
- `tessellate(int lod)` — produces a triangle mesh approximation
  at the given level of detail. The rasterizer
  ([Tessellation](../rasterization/tessellation.md)) needs
  this since rasterization works on triangles, not implicit
  surfaces.

The base class also provides `material()` and `setMaterial()` —
every primitive carries a `std::shared_ptr<Material>` that the
shader looks up at hit time
([Materials and BRDFs](materials-and-brdfs.md)).

## <a id="sphere-an-analytic-quadric"></a>Sphere: an analytic quadric
A sphere with center $\mathbf{c}$ and radius $r$ is the set of
points satisfying $\lVert\mathbf{p} - \mathbf{c}\rVert^2 = r^2$. Substitute
the ray equation $\mathbf{p}(t) = \mathbf{o} + t\mathbf{d}$ and
expand:

$$
\lVert\mathbf{o} + t\mathbf{d} - \mathbf{c}\rVert^2 = r^2
$$

Let $\mathbf{o}' = \mathbf{o} - \mathbf{c}$ to translate the
problem so the sphere is centered at the origin:

$$
(\mathbf{d} \cdot \mathbf{d}) t^2 + 2 (\mathbf{o}' \cdot \mathbf{d}) t + (\mathbf{o}' \cdot \mathbf{o}' - r^2) = 0
$$

This is a standard quadratic in $t$. The discriminant tells us
how many real solutions exist:

$$
\Delta = (\mathbf{o}' \cdot \mathbf{d})^2 - (\mathbf{d} \cdot \mathbf{d})(\mathbf{o}' \cdot \mathbf{o}' - r^2)
$$

If $\Delta < 0$ the ray misses the sphere; if $\Delta > 0$ there
are two roots $t_1 < t_2$ corresponding to the entry and exit
points of the ray through the sphere. The implementation reads
straight off this derivation:

```cpp
// src/render/primitives/Sphere.cpp:13
const Vector3d& o = ray.origin() - m_origin;
const Vector3d& d = ray.direction();

double od = o * d, dd = d * d;
double discriminant = od * od - dd * (o * o - m_radius * m_radius);

if (discriminant < 0) return nullptr;  // miss

double root = sqrt(discriminant);
double t1 = (-od - root) / dd;
double t2 = (-od + root) / dd;

Vector3d p1 = ray.at(t1), p2 = ray.at(t2);
hitPoints.add(
  HitPoint(this, t1, p1, (p1 - m_origin) / m_radius),
  HitPoint(this, t2, p2, (p2 - m_origin) / m_radius)
);
```

Note the normal computation: the surface normal at any point
$\mathbf{p}$ on a sphere is $(\mathbf{p} - \mathbf{c}) / r$ — the
unit vector from the center to the surface point. The radius
appears as the divisor specifically to normalize the result, so
the unit-length invariant
([Numbers and vectors: Length, normalization, and the unit-length invariant](../foundations/numbers-and-vectors.md#length-normalization-and-the-unit-length-invariant))
holds without an extra `normalize()` call.

The "case where $\Delta = 0$" — exactly one tangent intersection
— is a numerical-precision corner case. In double precision it
basically never happens with floating-point input, so the
implementation treats it as a miss without a special branch.

## <a id="plane-one-division"></a>Plane: one division
A plane with normal $\hat{\mathbf{n}}$ and signed distance $D$
from the origin is the set of points satisfying $\hat{\mathbf{n}}
\cdot \mathbf{p} - D = 0$. Substitute the ray equation:

$$
\hat{\mathbf{n}} \cdot (\mathbf{o} + t\mathbf{d}) - D = 0
$$

Solve for $t$:

$$
t = \frac{D - \hat{\mathbf{n}} \cdot \mathbf{o}}{\hat{\mathbf{n}} \cdot \mathbf{d}}
$$

That is the entire ray-plane formula. Two dot products and one
division. The denominator is zero when the ray is parallel to the
plane; if so, the ray either misses entirely or lies in the plane
(measure-zero edge case treated as a miss).

The implementation in
[`Plane.cpp`](../../../src/render/primitives/Plane.cpp) does
exactly this and emits a `HitPoint` whose normal is the plane's
$\hat{\mathbf{n}}$ — every point on a plane shares the same
normal. The plane has no [UV](../appendix/a-glossary.md#u) parameterization; texture sampling
on a plane has to come from a `PlanarMapping2D`
([Textures: Mappings: from hit point to $(s, t)$](textures.md#mappings-from-hit-point-to-s-t)).

The plane is infinite, so it has no bounding box. The
`calculateBoundingBox()` override returns the
`BoundingBox::infinite()` sentinel; the BVH builder skips
infinite primitives and stores them in a "always-test" list
that gets checked alongside every ray, regardless of which BVH
node the ray traverses.

## <a id="box-the-slab-method"></a>Box: the slab method
A box is the intersection of three pairs of parallel planes — one
pair for each axis. The interval of $t$ values inside each pair
is computed from two ray-plane intersections; the box's interior
is the *intersection* of the three intervals.

The procedure for an axis-aligned box with corners $\mathbf{p}_a$
(low) and $\mathbf{p}_b$ (high):

1. For each axis $i$ in $\{x, y, z\}$, compute the two slab-plane
   intersection parameters
   $t_{n,i} = (p_{a,i} - o_i) / d_i$ and
   $t_{f,i} = (p_{b,i} - o_i) / d_i$.
2. If $d_i$ is negative, swap the two so $t_{n,i}$ is always the
   smaller of the two.
3. Intersect the three intervals:
   $t_\text{enter} = \max_i t_{n,i}$ and
   $t_\text{exit} = \min_i t_{f,i}$.
4. If $t_\text{enter} > t_\text{exit}$, the ray misses; the
   intervals do not overlap. Otherwise, the entry hit is at
   $t_\text{enter}$ and the exit hit is at $t_\text{exit}$.

Six divisions and a small amount of comparison. The same
algorithm is what the BVH traversal uses on every internal node,
which is why "is this ray near this AABB?" is so cheap — it's the
slab method without the hit-point construction.

The box's surface normal at a hit depends on which face was hit.
The implementation reads this off the axis whose $t_{n,i}$ tied
for the maximum at step 3 (or whose $t_{f,i}$ tied for the
minimum, for the exit hit). A unit vector along that axis, signed
by the ray direction, is the face normal.

## <a id="triangle-moller-trumbore"></a>Triangle: Möller-Trumbore
A triangle defined by three vertices $\mathbf{p}_0$,
$\mathbf{p}_1$, $\mathbf{p}_2$ has any interior point expressible
in **barycentric coordinates**:

$$
\mathbf{p} = (1 - \beta - \gamma)\mathbf{p}_0 + \beta\mathbf{p}_1 + \gamma\mathbf{p}_2
$$

with $\beta \geq 0$, $\gamma \geq 0$, $\beta + \gamma \leq 1$.
Substitute the ray equation, and you get a linear system in
$(\beta, \gamma, t)$ that can be solved with one $3 \times 3$
matrix inverse:

$$ \begin{pmatrix} \mathbf{p}_0 - \mathbf{p}_1 & \mathbf{p}_0 - \mathbf{p}_2 & \mathbf{d} \end{pmatrix} \begin{pmatrix} \beta \\ \gamma \\ t \end{pmatrix} = \mathbf{p}_0 - \mathbf{o} $$

where each column of the $3 \times 3$ matrix is a 3D vector and
the right-hand side is a 3D vector.

Möller-Trumbore (1997) is the canonical algorithm that solves
this system using Cramer's rule applied carefully so that the
early-out tests (β out of range, γ out of range, β + γ > 1) can
fire before the full system is solved. The
[`Triangle.cpp`](../../../src/render/primitives/Triangle.cpp)
implementation follows this structure exactly: compute β and
test it; if it survives, compute γ and test it; if it survives,
compute t and the hit point.

A successful test produces the value of $t$, the hit
point $\mathbf{p}$, and — for free, since they fall out of the
linear-system solution — the three barycentric coordinates
$w_0 = 1 - \beta - \gamma$, $w_1 = \beta$, $w_2 = \gamma$.
Those coordinates drive UV interpolation and per-vertex normal
interpolation in [Mesh primitives](#mesh-primitives), and they drive the
rasterizer's perspective-correct attribute interpolation in
[MSAA and attribute interpolation](../rasterization/msaa-and-attribute-interpolation.md).

The tessellation widget has the same barycentric setup live;
it reuses the math one volume earlier:

<!-- widget: mesh_triangle_interpolation -->

The standalone `Triangle` primitive carries one face normal
shared across the whole surface, set at construction time. The
mesh-triangle variants in [Mesh primitives](#mesh-primitives) carry per-vertex data and
interpolate.

## <a id="disk-and-open-cylinder"></a>Disk and open cylinder
A **disk** is a plane intersected with a circle. Compute the
ray-plane intersection at the disk's plane; check whether the
resulting hit point is within the disk's radius from the disk's
center. If so, hit; if not, miss.

The
[`Disk`](../../../include/render/primitives/Disk.h)
implementation does exactly that: one ray-plane intersection,
one squared-distance check (`squaredLength` from
[Numbers and vectors: Length, normalization, and the unit-length invariant](../foundations/numbers-and-vectors.md#length-normalization-and-the-unit-length-invariant) —
no need for the square root when comparing against $r^2$), one
hit-point construction. The normal is the disk's plane normal,
shared across all interior hits.

An **open cylinder** is an analytic quadric similar to a sphere,
with one twist: the constraint is in 2D, not 3D. A cylinder of
radius $r$ along the $y$-axis is the set of points satisfying
$x^2 + z^2 = r^2$ — the $y$ coordinate is free. Substitute the
ray equation, project to the $xz$-plane, and the same quadratic
formula from [Sphere: an analytic quadric](#sphere-an-analytic-quadric) applies, with the $y$-coordinate consistency
handled by the cylinder's height bounds.

The
[`OpenCylinder`](../../../include/render/primitives/OpenCylinder.h)
class adds two parameters that the sphere doesn't have:
`yMin` and `yMax`, which clip the cylinder's height. Hit points
whose $y$ coordinate falls outside the range are rejected even if
they satisfy the radius equation.

The "open" in the name reflects that there are no caps — a hit on
the side surface only. To produce a closed cylinder, combine an
`OpenCylinder` with two `Disk`s using a `Composite` primitive.

## <a id="torus-a-quartic-root-problem"></a>Torus: a quartic root problem
A torus is the surface generated by sweeping a circle of radius
$r_{\text{tube}}$ around an axis at distance $r_{\text{ring}}$.
Its implicit equation is:

$$
(x^2 + y^2 + z^2 + r_{\text{ring}}^2 - r_{\text{tube}}^2)^2 - 4 r_{\text{ring}}^2 (x^2 + z^2) = 0
$$

(when the ring lies in the $xz$-plane, which is the convention
the codebase uses). Substitute the ray equation and you get a
fourth-degree polynomial in $t$, the dreaded **quartic**:

$$
a_4 t^4 + a_3 t^3 + a_2 t^2 + a_1 t + a_0 = 0
$$

Quadratic equations have a one-line closed-form solution. Cubic
equations have a closed-form solution that's three lines but
includes complex arithmetic. [Quartic](../appendix/a-glossary.md#q) equations have a closed-form
solution that runs about 50 lines — Ferrari's method, which
reduces the quartic to a cubic ("the resolvent cubic") and then
solves that cubic to back-solve the quartic.

The codebase's
[`include/core/math/Quartic.h`](../../../include/core/math/Quartic.h)
implements Ferrari's method. The
[`Torus`](../../../include/render/primitives/Torus.h) intersection
routine builds the four polynomial coefficients from the ray
equation, calls the quartic solver, gets back zero to four real
roots, sorts them by $t$, and produces hit points for the smallest
positive ones.

The cost is an order of magnitude more expensive than a sphere's
quadratic. A scene full of tori traces noticeably slower than a
scene full of spheres of the same count, all else being equal.
That's the cost of an analytic surface that doesn't have a simple
quadric form.

The torus's surface normal at a hit point requires a small
derivation: it's the gradient of the implicit equation, evaluated
at the hit point and normalized. The
[`Torus.cpp`](../../../src/render/primitives/Torus.cpp)
implementation has the closed-form gradient pre-derived and
plugged in.

## <a id="mesh-primitives"></a>Mesh primitives
A mesh is a list of vertices and a list of triangles indexed into
that list. The codebase represents the mesh data as
[`core::Mesh`](../../../include/core/geometry/Mesh.h), and the
*primitives* that wrap individual triangles of a mesh come in two
flavors:

[`FlatMeshTriangle`](../../../include/render/primitives/FlatMeshTriangle.h)
carries one face normal shared across the triangle, like the
standalone `Triangle`. The shading uses the face normal directly,
producing flat-shaded surfaces with visible faceting.

[`SmoothMeshTriangle`](../../../include/render/primitives/SmoothMeshTriangle.h)
carries three per-vertex normals and interpolates them at the hit
point using the barycentric coordinates from [Triangle: Möller-Trumbore](#triangle-moller-trumbore). The
shading uses the interpolated normal, producing smooth-shaded
surfaces that hide the underlying tessellation. This is what
turns a 200-triangle sphere mesh into something that looks
spherical at typical render resolutions.

Both share the same intersection math — Möller-Trumbore — and
differ only in how the resulting `HitPoint`'s normal field gets
filled. The UV coordinates work the same way: per-vertex UVs
stored on the mesh, interpolated at the hit point with the
barycentric weights.

Imported meshes use
[`MeshAsset`](../../../include/core/geometry/MeshAsset.h) and
[`MeshPrimitive`](../../../include/render/primitives/MeshPrimitive.h)
as the ownership boundary. The asset keeps the `core::Mesh` alive
behind a shared pointer, while the runtime primitive builds flat
or smooth triangle leaves from that shared mesh. A mesh primitive
can also attach materials per source face, so importers such as
LDraw or a future glTF loader can keep one shared geometry payload
while preserving the material assigned to each imported polygon.

## <a id="the-unifying-picture"></a>The unifying picture
Every shape in this chapter implements the same `Primitive`
interface, but the math behind each `intersect` varies wildly in
expense:

| Primitive | Math | Cost class |
|---|---|---|
| Plane | 2 dot products + 1 division | trivial |
| Disk | plane + squared-distance check | trivial |
| Sphere | one quadratic root | cheap |
| Box | 3 axis-aligned slab pairs | cheap |
| OpenCylinder | sphere math in 2D + height bounds | cheap |
| Triangle | Möller-Trumbore | cheap |
| Mesh triangle (flat or smooth) | Möller-Trumbore + interpolation | cheap |
| Torus | quartic root via Ferrari's method | expensive |

The renderer treats them all the same: it asks each primitive's
`intersect` whether the ray hits, hands the closest hit's
`HitPoint` to the material, and recurses.

## <a id="exercises"></a>Exercises
1. The sphere's intersection routine emits *both* roots of the
   quadratic, even when only one is in front of the ray. Why?
   What consumer needs the second root, and what would happen if
   the routine emitted only the smaller positive root?
2. The plane has a single normal that doesn't depend on the hit
   point. Now consider rendering a plane with a normal pointing
   away from the camera. What does the [Lambertian](../appendix/a-glossary.md#l) shading
   ([Materials and BRDFs](materials-and-brdfs.md)) produce, and why?
3. Read the box slab-method code. The "swap if $d_i$ is negative"
   step can be expressed as a `std::swap` or, equivalently, as
   `(d > 0) ? near : far` and `(d > 0) ? far : near`. Which
   variant does the implementation use? What's the trade-off?
4. The torus is the only shape in this chapter whose intersection
   routine is materially more expensive than the others. Look up
   the BVH performance test in the codebase
   ([Spatial acceleration](../scene-structure/spatial-acceleration.md)),
   and predict how the cost ratio between "scene full of spheres"
   and "scene full of tori" changes as the BVH gets more
   effective.

## See also

- Volume index: [Ray rendering](README.md)
- Previous: [Cameras](cameras.md)
- Next: [Materials and BRDFs](materials-and-brdfs.md)
- Geometry vocabulary:
  [Rays and geometry](../foundations/rays-and-geometry.md)
- Composed primitives:
  [Constructive solid geometry](../scene-structure/csg.md)
- Mesh form for rasterization:
  [Tessellation](../rasterization/tessellation.md)
- Acceleration over many primitives:
  [Spatial acceleration](../scene-structure/spatial-acceleration.md)
- Quartic-solver utility:
  [`include/core/math/Quartic.h`](../../../include/core/math/Quartic.h)

## Source anchors

<!-- source-anchors -->
- `include/render/primitives/Primitive.h`
- `include/render/primitives/Sphere.h`
- `include/render/primitives/Plane.h`
- `include/render/primitives/Box.h`
- `include/render/primitives/Triangle.h`
- `include/render/primitives/Disk.h`
- `include/render/primitives/OpenCylinder.h`
- `include/render/primitives/Rectangle.h`
- `include/render/primitives/Torus.h`
- `include/render/primitives/MeshTriangle.h`
- `include/render/primitives/FlatMeshTriangle.h`
- `include/render/primitives/SmoothMeshTriangle.h`
- `include/render/primitives/MeshPrimitive.h`
- `include/render/primitives/Instance.h`
- `include/core/geometry/MeshAsset.h`
- `include/core/math/Quartic.h`
- `include/core/math/Quadric.h`
- `include/core/math/Cubic.h`
- `include/core/math/Polynomial.h`
<!-- /source-anchors -->
