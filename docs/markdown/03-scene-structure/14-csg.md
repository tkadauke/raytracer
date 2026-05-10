# 14. Constructive solid geometry

A primitive is one shape; a *composite* is a tree of shapes
combined with set-theoretic operations — union, intersection,
difference. The classical name for this construction is
**constructive solid geometry** (CSG), and it is the
graphics-side trick for building complex shapes out of simple
ones: a glass pawn carved from a cylinder with a sphere
subtracted, a torus that's actually a difference of two
unequal solids, a Boolean union of cubes producing a
staircase. This chapter is about how CSG actually works at the
intersection-math level and about the small family of
non-Boolean composite operations the codebase ships alongside.

By the end of this chapter you should know:

- the `HitPointInterval` set operations that drive Boolean CSG,
- how Difference / Union / Intersection turn child intervals
  into a single output interval per ray,
- the support-mapping family (Minkowski sum, convex hull) and
  what they compute that Booleans don't,
- the GJK algorithm that intersects a ray with a convex
  primitive defined only by its support function.

## 14.1 Hit intervals are the unifying abstraction

A primitive's `intersect` produces a `HitPointInterval`, defined
in
[chapter 3 §3.5](../01-foundations/03-rays-and-geometry.md#3-5-hitpointinterval-in-out-in-out)
as the sequence of in/out events along the ray, sorted by $t$.
For a sphere, the interval is one entry-exit pair. For a torus,
it can be up to two pairs. For a CSG primitive, the interval is
*derived* from the children's intervals using a set operation.

The `csg_hit_intervals` widget makes this concrete with a
draggable ray timeline showing the union, intersection, and
difference behaviors:

<!-- widget: csg_hit_intervals -->

The boundary events on the ray timeline are the hit points; the
shaded segments between an "in" and an "out" event are the
ranges where the ray is *inside* the relevant solid. CSG turns
the question "what should the ray's intervals look like for this
combined shape?" into a few rules over those segments.

## 14.2 The three Boolean operations

Each Boolean composite below derives from
[`Composite`](../../../include/render/primitives/Composite.h),
which is the base for "primitive that wraps a list of children."
The rules below describe the per-ray behavior:

[`Union`](../../../include/render/primitives/Union.h) returns
the *union* of the children's intervals. If any child is
inside, the union is inside; if all children are outside, the
union is outside. Visually, the union of two overlapping
spheres looks like two spheres glued together — the surface is
the outer envelope of the contributing children, with the
overlap region's interior boundaries hidden.

[`Intersection`](../../../include/render/primitives/Intersection.h)
returns the *intersection* of the children's intervals. Inside
only when *every* child is inside; outside otherwise. Visually,
the intersection of two overlapping spheres is the lens-shaped
region they both contain.

[`Difference`](../../../include/render/primitives/Difference.h)
subtracts the second-and-later children from the first. The
output is inside where the first child is inside *and* every
later child is outside. Visually, a sphere minus a smaller
sphere produces a hollow with the smaller sphere's interior
exposed.

The three operations happen at the interval level, before the
hit point is shaded. A ray that crosses a Difference of two
spheres might generate four hit-point events (enter outer,
exit at inner-near boundary, re-enter at inner-far boundary,
exit outer); the first hit point on the ray's interior is the
one the renderer's hit-point selector picks for shading.

A subtle Difference detail: the second child's hit-point
boundaries get their normals *flipped* in the output interval.
The reason is that the second child's surfaces, in the original
sense, point *outward* from its solid; in the Difference
output, those same surfaces are now the *interior* of a cavity,
which means a ray traveling through them sees the surface from
the *opposite* side. Flipping the normals at output time keeps
the lighting code's normal-vs-ray-direction convention honest
without further special-casing.

## 14.3 Convex composites: support mapping

The Boolean composites work for any pair of children, with no
constraint on the child shapes. A second family of composites
needs the children to be **convex**, but in exchange offers
operations that don't fit the Boolean frame at all.

A **support function** for a convex shape is a function
$\text{supp}(\mathbf{v})$ that returns the *farthest point* on
the shape in the direction $\mathbf{v}$. Every convex shape has
one, and for the simple cases the codebase ships, the function
is short:

- A sphere of radius $r$ centered at $\mathbf{c}$:
  $\text{supp}(\mathbf{v}) = \mathbf{c} + r \, \hat{\mathbf{v}}$.
- An axis-aligned box with corners $\mathbf{p}_{\min},
  \mathbf{p}_{\max}$: pick the corner whose component sign
  matches $\mathbf{v}$'s on each axis.
- A triangle (or any polytope): one of the vertices, whichever
  has the highest dot product with $\mathbf{v}$.

The farthest-point widgets show this for sphere and box:

<!-- widget: sphere_farthest_point -->
<!-- widget: box_farthest_point -->

The interesting consequence is that, given the support
functions of two convex shapes, you can compute the support
function of certain *combinations* of those shapes without
ever building the combined shape explicitly.

[`MinkowskiSum`](../../../include/render/primitives/MinkowskiSum.h)
is one such operation. The Minkowski sum of two shapes $A$ and
$B$ is the set $\{\, \mathbf{a} + \mathbf{b} \mid \mathbf{a} \in A, \mathbf{b} \in B \,\}$.
Geometrically, sweep $A$'s shape over every point of $B$ and
take the union of all the swept positions. The Minkowski sum of
two spheres is a larger sphere; of a sphere and a box, a
"rounded cube" with the sphere's radius worth of fillet on
every edge; of a triangle and a sphere, the triangle's interior
plus a sphere-thick band around its boundary. The support
function works out to:

$$
\text{supp}_{A \oplus B}(\mathbf{v}) = \text{supp}_A(\mathbf{v}) + \text{supp}_B(\mathbf{v})
$$

— that is, the support of the sum is the *sum of supports* in
the same direction. One scalar lookup per child, then one
vector add. Constructing the Minkowski sum's actual surface
explicitly would take work proportional to the children's
combined complexity; deriving it via support functions takes
$O(\text{number of children})$ per query.

[`ConvexHull`](../../../include/render/primitives/ConvexHull.h)
is the support-function combinator that computes the smallest
convex shape containing all the children. The support function
is "ask each child for its support in this direction, project
all results onto a ray in that direction, return the
furthest-along result":

<!-- widget: convex_hull_farthest_point -->

Walked-through: two side-by-side boxes' convex hull is a
hexagonal extrusion connecting them; three points' convex hull
is the triangle; a sphere and a point's convex hull is a
"cone" with a hemispherical cap.

## 14.4 GJK: ray intersection on a support function

Boolean composites' `intersect` works by computing the
children's intervals and combining them — straightforward
because each child has its own native intersection routine.
Convex composites can't do this: their *output* shape doesn't
have a native intersection routine, only a support function.

The bridge is the **Gilbert-Johnson-Keerthi algorithm** (GJK,
1988). GJK takes a convex shape defined only by its support
function and incrementally constructs a *simplex* — a small
set of points (up to four in 3D) — that moves toward the
origin of the *Minkowski difference* between the ray and the
shape. The Minkowski difference $A \ominus B$ is the set $\{\,
\mathbf{a} - \mathbf{b} \mid \mathbf{a} \in A, \mathbf{b} \in B
\,\}$, and its support function is the difference of the
shape supports: $\text{supp}_{A \ominus B}(\mathbf{v}) =
\text{supp}_A(\mathbf{v}) - \text{supp}_B(-\mathbf{v})$.

The geometric idea: the ray hits the shape if and only if
$\mathbf{0} \in A \ominus B$. GJK iteratively tests the
simplex against the origin, expands the simplex toward the
origin if it doesn't contain it yet, and terminates either with
"the simplex contains the origin" (intersection) or "the
support in the current best direction can't improve the
distance" (no intersection, with the simplex's closest point
to the origin giving the actual minimum distance).

The widget shows the algorithm running step by step:

<!-- widget: support_mapping_gjk -->

For a Whitted ray-trace through a convex composite, the
algorithm answers two questions:

1. *Is there an intersection?* — the boolean form, used by
   shadow rays.
2. *What is the smallest $t$ at which the ray enters the
   shape?* — the geometric form, used by primary rays. This
   reduces to GJK on the line-segment-vs-shape problem along
   the ray, with binary search to refine the entry $t$.

[`ConvexOperation`](../../../include/render/primitives/ConvexOperation.h)
is the shared base class for `MinkowskiSum` and `ConvexHull`.
Both override `farthestPoint(direction)` (the support
function) and inherit the `intersect` / `intersects`
implementation that drives GJK.

The algorithm's strength is the dimensionality independence: a
support function is a single scalar input and a single vector
output, regardless of how many children the convex composite
has or how complex each child is. The cost scales linearly with
the number of children per query — and the intersection cost
scales with the number of GJK iterations needed to converge,
typically a small constant for shapes that don't have
near-tangent surfaces.

## 14.5 `ClosedSolidUnion` — the workaround

[`ClosedSolidUnion`](../../../include/render/primitives/ClosedSolidUnion.h)
is a small specialization that trades generality for speed.
It wraps a list of *closed* convex children — meaning every
child has a fully-defined inside-out boundary — and produces
their union without going through the full Boolean
union pipeline. This is faster than `Union` for the common case
of "I want a primitive that is the union of N spheres" because
the per-ray cost is a `min` over the children's intervals, not
the full set-union construction.

You shouldn't reach for it casually — `Union` is the right
default, and `ClosedSolidUnion` is the optimization for the
specific case where the speed matters.

## 14.6 Tessellation is not yet supported

The CSG composites — Boolean and convex alike — share one
limitation: their `tessellate(int lod)` overrides return an
empty mesh. The reason is that **mesh-Boolean operations are
hard**: producing a triangle mesh of, say, the difference of
two spheres requires polygon clipping, robust intersection
computation, and edge-case handling that none of the published
mesh-Boolean libraries solve well in the general case.

The codebase punts on this for now. CSG primitives render
correctly under the raytracer (where intersection works on hit
intervals, not on triangle data) but render as empty in the
rasterizer / wireframe engines. This is queued under roadmap
§4.2.a; the eventual plan is to integrate one of the published
mesh-Boolean libraries (Cork, libigl's Boolean operations, or
manifold) and produce the triangle mesh on demand.

## 14.7 Picking a composite

For "how do I subtract this shape from that one?", reach for
**Difference**. The most common CSG operation by far.

For "how do I combine these N shapes into one?", reach for
**Union**. Reach for `ClosedSolidUnion` only if the speed
matters and all children are closed convex.

For "what is the overlapping region of these two shapes?",
reach for **Intersection**. Less common, but unique to CSG —
no other operation produces this output.

For "I want the rounded version of this object" or "I want a
shape thickened by a sphere of radius r", reach for
**MinkowskiSum** with the second operand being a small sphere.

For "I want the convex bounding shape of these objects",
reach for **ConvexHull**.

## 14.8 Exercises

1. Predict the output of `Difference(sphere_at_origin,
   sphere_at_origin)` — a sphere subtracted from itself. What
   does the rendered output look like, and why?
2. Read the `Difference::intersect` implementation. Confirm
   that the second child's hit-point normals are flipped in
   the output interval. What rendered artifact would appear if
   the flip were skipped?
3. Construct a `MinkowskiSum` of a triangle and a sphere of
   radius 0.1. Sketch the resulting shape on paper. Where do
   the rounded corners come from, geometrically?
4. The `ConvexHull` of three colinear points is degenerate.
   What does its support function return, and what does GJK do
   with that?
5. Estimate how many GJK iterations are needed to find the
   closest point on a sphere centered at the origin to a unit-
   length-direction ray pointed away from the origin. Verify
   by reading the `GJKSimplex.h` interface and the
   `ConvexOperation::intersect` implementation.

## See also

- Volume index: [Volume III — Scene structure](README.md)
- Previous: [13. View planes](13-view-planes.md)
- Next: [15. Spatial acceleration](15-spatial-acceleration.md)
- HitPointInterval vocabulary:
  [3. Rays and geometry §3.5](../01-foundations/03-rays-and-geometry.md#3-5-hitpointinterval-in-out-in-out)
- Per-primitive intersection:
  [7. Primitives and intersection](../02-ray-rendering/07-primitives-and-intersection.md)
- GJK + simplex implementation:
  [`include/core/math/GJKSimplex.h`](../../../include/core/math/GJKSimplex.h)

## Source anchors

<!-- source-anchors -->
- `include/render/primitives/Difference.h`
- `include/render/primitives/Union.h`
- `include/render/primitives/Intersection.h`
- `include/render/primitives/MinkowskiSum.h`
- `include/render/primitives/ConvexHull.h`
- `include/render/primitives/ConvexOperation.h`
- `include/render/primitives/ClosedSolidUnion.h`
- `include/render/primitives/Composite.h`
- `include/core/math/HitPointInterval.h`
- `include/core/math/GJKSimplex.h`
<!-- /source-anchors -->
