# Rays and geometry

A renderer that doesn't shoot rays isn't a raytracer. This chapter
introduces the `Ray` type that all of Ray rendering's intersection code
takes as input, the `HitPoint` / `HitPointInterval` types that come
back as output, and the supporting cast — bounding boxes, ranges,
rectangles — that show up in essentially every chapter that
follows.

By the end you should know:

- the `Ray` parameterization $\mathbf{r}(t) = \mathbf{o} +
  t\mathbf{d}$ and *why* we use the scalar $t$ as the universal
  currency of ray-object intersection,
- the difference between a `HitPoint` (one intersection) and a
  `HitPointInterval` (a sequence of in/out events along the ray),
- how AABBs work and why the renderer reaches for them constantly,
- the `Range<T>` helper, which gives every "is $t$ in the valid
  interval?" check a single home.

## <a id="the-ray-type"></a>The `Ray` type
A ray is two pieces of data: an origin (where it starts) and a
direction (which way it goes). The codebase encodes this as
[`Ray<T>`](../../../include/core/math/Ray.h):

```cpp
// include/core/math/Ray.h
template<class T>
class Ray {
public:
  inline explicit Ray(const Vector4<T>& origin,
                      const Vector3<T>& direction)
    : m_origin(origin), m_direction(direction) {}

  inline const Vector4<T>& origin()    const { return m_origin; }
  inline const Vector3<T>& direction() const { return m_direction; }

  inline Vector4<T> at(T t) const {
    return Vector3<T>(m_origin) + m_direction * t;
  }
  // ...
private:
  Vector4<T> m_origin;
  Vector3<T> m_direction;
};

typedef Ray<double> Rayd;
typedef Ray<float>  Rayf;
```

A few details earn comment. The origin is a `Vector4` (homogeneous
point) so transforms by `Matrix4d` work without conversion — see
[Matrices and transforms: Point vs direction vs normal](matrices-and-transforms.md#point-vs-direction-vs-normal).
The direction is a `Vector3` (no homogeneous bit) because
directions don't translate; the `Matrix3d` from
[`Instance::m_directionMatrix`](../../../include/render/primitives/Instance.h)
acts on it directly.

The convention from
[Numbers and vectors: Length, normalization, and the unit-length invariant](numbers-and-vectors.md#length-normalization-and-the-unit-length-invariant)
is in force: **the direction is unit length**. Every public API
that constructs a ray normalizes before returning. Intersection
math throughout the book assumes this.

The widget below is the labeled diagram. Origin $o$, direction
$\mathbf{d}$, and the start of the parameter axis at $t = 0$:

<!-- widget: ray_class -->

## <a id="the-parameter-t-is-the-universal-currency"></a>The parameter $t$ is the universal currency
The interesting method on `Ray` is one line:

```cpp
inline Vector4<T> at(T t) const {
  return Vector3<T>(m_origin) + m_direction * t;
}
```

`at(t)` evaluates the ray at parameter $t$ — the point along the
ray at distance $t$ from the origin (since direction is unit
length, $t$ literally is distance, not just a "parameter").

Every ray-object intersection routine in the book returns its
answer as a value of $t$. Sphere intersection: solve a quadratic in
$t$, return the smaller positive root. Plane intersection: one
division, return the resulting $t$. Triangle intersection: three
dot products, return the resulting $t$ if it's positive and the
barycentric coords are inside. Box intersection: compare $t$
intervals against the three slab planes, return the entry $t$.

Why $t$ and not the actual 3D point of intersection?

- **Comparison is cheap.** "Which of these intersections happens
  first along the ray?" is a single scalar comparison on $t$, not
  a Euclidean distance computation.
- **Range clipping is cheap.** The shadow-ray code asks "is the
  closest intersection between $t = \epsilon$ and $t = t_{\text{light}}$?"
  That's two `<` comparisons.
- **The point follows from $t$ for free.** `ray.at(t)` is one
  multiply and one add; the renderer only computes the actual hit
  point when it needs it.
- **Floating-point precision is tighter.** Comparing two scalars
  is more numerically stable than comparing two 3D points
  component-wise.

The `at(t)` evaluation only happens at the boundary where the
intersection result becomes a `HitPoint` and the actual world-space
position needs to be remembered.

The interactive widget makes this concrete:

<!-- widget: ray_at -->

Drag the parameter slider; the highlighted point moves along the
ray. That's the only "ray geometry" you need internalized for the
rest of the book.

## <a id="the-ray-class-all-of-it"></a>The `Ray` class, all of it
The full `Ray` class has a few additional helpers worth naming:

- `epsilonShifted()` returns a copy of the ray with its origin
  pushed forward by `epsilon` along the direction. This is the
  standard trick for shadow rays: when a ray bounces off a
  surface, the next ray's origin is the hit point on that
  surface, and floating-point error means the new ray "slightly
  hits" its own surface unless you push the origin forward. The
  static `epsilon` constant is `1e-7` for `double` and `1e-4` for
  `float`.
- `from(origin)` and `to(direction)` return a copy of the ray
  with one component swapped. Used when a Ray-routine wants "the
  same ray but starting somewhere else" or "the same ray but
  pointed differently."
- `projectedDistance(point)` returns the scalar projection of a
  point onto the ray's direction — the value of $t$ at the
  closest point on the ray to the given point.
- `project(point)` returns `at(projectedDistance(point))` — the
  closest point on the ray to the input point.
- `distanceTo(point)` returns the perpendicular distance from the
  ray to the input point.

The projection helpers come up in the spatial-acceleration code
([Spatial acceleration](../scene-structure/spatial-acceleration.md))
and in any "is this point near this ray?" check. The widget below
shows a few sample points being projected onto a ray:

<!-- widget: ray_project -->

## <a id="hitpoint-what-comes-back"></a>`HitPoint`: what comes back
A `HitPoint`
([`include/core/math/HitPoint.h`](../../../include/core/math/HitPoint.h))
is the data a successful intersection produces. Five fields:

```cpp
class HitPoint {
  const render::Primitive* m_primitive;
  double                   m_distance;     // the t value
  Vector4d                 m_point;        // ray.at(t), cached
  Vector3d                 m_normal;       // surface normal, unit length
  Vector2d                 m_uv;           // texture coordinates
};
```

- `m_primitive` is a back-pointer to the primitive the ray hit.
  Materials, [BVH](../appendix/a-glossary.md#b) bookkeeping, and the per-primitive scene picking
  all consult this. It's a non-owning pointer; the primitive lives
  in the scene.
- `m_distance` is the $t$ from [The parameter $t$ is the universal currency](#the-parameter-t-is-the-universal-currency). Why store it after computing
  the point? Because the intersection logic needs to compare hit
  distances cheaply — the `operator<` comparator on `HitPoint`
  compares this field.
- `m_point` is the cached `ray.at(distance)`. The intersection
  routine fills it once so consumers don't recompute it.
- `m_normal` is the unit-length surface normal at the hit point.
  The unit-length invariant from
  [Numbers and vectors: Length, normalization, and the unit-length invariant](numbers-and-vectors.md#length-normalization-and-the-unit-length-invariant)
  applies here too.
- `m_uv` is the surface's $(u, v)$ texture coordinates at the hit
  point. Filled by primitives that have a meaningful [UV](../appendix/a-glossary.md#u)
  parameterization (sphere, cylinder, mesh triangles); zero
  otherwise.

The class also carries two helpers worth knowing:

- `swappedNormal()` returns a copy with the normal negated. Used
  when a ray hits the *back* of a surface (the dot product of ray
  direction and normal is positive instead of negative); [CSG](../appendix/a-glossary.md#c)
  intersection
  ([Constructive solid geometry](../scene-structure/csg.md)) and refraction
  inside-the-glass cases call it.
- `transform(pointMatrix, normalMatrix)` returns a copy with the
  point transformed by `pointMatrix` and the normal transformed
  by `normalMatrix` — and re-normalized, since transforming a
  normal doesn't preserve unit length under non-uniform scale.
  This is the consumer-side counterpart to the four-matrix dance
  from [Matrices and transforms: The four-matrix dance: `Instance`](matrices-and-transforms.md#the-four-matrix-dance-instance).

The widget below visualizes the geometry: a ray at origin $r$ hits
a sphere at hit point $p$, distance $d$, normal $n$.

<!-- widget: hitpoint_class -->

## <a id="hitpointinterval-in-out-in-out"></a>`HitPointInterval`: in / out / in / out
A single `HitPoint` answers "where does this ray first hit
*something*?" That's enough for primary rays and shadow rays. It
isn't enough for **CSG** — constructive solid geometry, where you
combine primitives via union, intersection, and difference
([Constructive solid geometry](../scene-structure/csg.md)).

The example: a ray pointed at a sphere with a smaller sphere
subtracted from its interior. The ray enters the outer sphere,
*exits* the inner sphere a moment later (from the front face of
the cavity), enters the inner sphere again (from the back face),
and finally exits the outer sphere. Four hit points, alternating
in and out, all describing one CSG shape.

[`HitPointInterval`](../../../include/core/math/HitPointInterval.h)
is the data structure that carries this:

```cpp
class HitPointInterval {
public:
  class HitPointWrapper {
  public:
    HitPoint point;
    bool     in;        // true = entering, false = exiting
    bool operator<(const HitPointWrapper& other) const {
      return point.distance() < other.point.distance();
    }
  };
  // ... add, addIn, addOut, set operations ...
private:
  std::vector<HitPointWrapper> m_hitPoints;
};
```

A vector of `HitPoint`s, each tagged with whether it's an entry
event or an exit event. Sorted by $t$. Set operations on these
intervals — union, intersection, difference — are how CSG works at
the math level, and they're covered in
[Constructive solid geometry](../scene-structure/csg.md). For the rest of
Ray rendering, all you need is "this is the interval form of a
[HitPoint](../appendix/a-glossary.md#h), used when a primitive can produce more than one
intersection per ray."

## <a id="bounding-boxes"></a>Bounding boxes
Most ray-object intersection routines have a fast-rejection path:
"is the ray nowhere near this object's bounding box? then skip
the expensive intersection math entirely." That fast-rejection is
the entire reason
[`BoundingBox<T>`](../../../include/core/math/BoundingBox.h)
exists.

A bounding box is two corners: `min` (lowest $x$, $y$, $z$) and
`max` (highest of each). The box is the axis-aligned rectangle
$[\text{min}.x, \text{max}.x] \times [\text{min}.y, \text{max}.y]
\times [\text{min}.z, \text{max}.z]$. "[AABB](../appendix/a-glossary.md#a)" — axis-aligned
bounding box — is the canonical name in the literature.

<!-- widget: bounding_box_class -->

The default constructor produces an *empty* box: `min = +∞`, `max
= -∞`. That degenerate state is the identity element for the
"include" operation:

<!-- widget: bounding_box_include -->

`include(point)` widens the box to contain `point`. Starting from
the empty box and including every primitive's bounds gives you the
scene's bounding box. The widget shows what the operation looks
like geometrically: the smallest AABB that contains both the
existing box and the new point.

The operations the book uses elsewhere:

- `grow(amount)` widens the box by a scalar in every direction.
  Useful for "expand by an epsilon to swallow up boundary
  precision issues."

  <!-- widget: bounding_box_grown_by -->

- `move(offset)` translates the whole box by a vector. Used by
  `Instance` to compute the world-space bounds of a
  transformed-and-positioned primitive without having to retest
  every vertex.

  <!-- widget: bounding_box_moved_by -->

- The intersection of two boxes ($A \cap B$) is itself a box —
  possibly empty if the two don't overlap.

  <!-- widget: bounding_box_and -->

- The union ($A \cup B$) is *not* in general a box (consider two
  far-apart small boxes), so the codebase computes the
  *bounding box of the union*, the smallest AABB containing both.

  <!-- widget: bounding_box_or -->

The intersection routine for ray-vs-box is the **slab method**:
treat the box as the intersection of three pairs of parallel
planes, compute the $t$ interval the ray is inside each pair, and
take the intersection of all three intervals. The returned interval
is the $t$-range during which the ray is inside the box. If the
interval is empty, the ray missed the box. Linear in the number
of dimensions and cheap enough to run for every primitive in a
scene.

This is what makes BVH traversal
([Spatial acceleration](../scene-structure/spatial-acceleration.md))
fast: descend into a child only if the ray's $t$ interval
intersects the child's bounding box. Boxes that miss prune entire
subtrees.

## <a id="range-a-closed-interval-helper"></a>`Range<T>`: a closed interval helper
`Range<T>` is a tiny class with disproportionate reach across the
codebase. From
[`include/core/math/Range.h`](../../../include/core/math/Range.h):

```cpp
template<class T>
class Range {
public:
  inline explicit Range(const T& begin, const T& end)
    : m_begin(begin), m_end(end) {}

  inline T begin() const { return m_begin; }
  inline T end()   const { return m_end; }

  inline bool contains(const T& v) const {
    return begin() <= v && v <= end();
  }
  inline T clamp(const T& v) const {
    return v < begin() ? begin() : (v > end() ? end() : v);
  }
  inline T random() const { /* uniform in [begin, end] */ }
};

typedef Range<double> Ranged;
typedef Range<float>  Rangef;
```

The class wraps two endpoints with a `contains(v)` membership
test, a `clamp(v)` projection, and a `random()` sample. The
closed-interval semantics — both endpoints included — are a
convention; the membership check uses `<=`.

`Range<double>` shows up everywhere a "valid $t$ interval" needs to
be expressed: shadow ray clipping, sampler stratification cells,
view plane row sampling, spectral wavelength bins. Anywhere a
piece of code wants to talk about "between this and that, included
on both ends."

## <a id="putting-it-together"></a>Putting it together
The full ray-trace step, in this chapter's vocabulary:

```
Camera produces a Rayd
      │
      ▼
Scene::intersect(ray, hitPoints, state)
   each Primitive does fast-reject vs. its BoundingBox
   then writes its hits into the HitPointInterval
      │
      ▼
HitPoint chosen as the smallest-t entering hit
      │
      ▼
HitPoint.point() / .normal() / .uv() drive shading
      │
      ▼
Shading produces a Colord
```

That's [The Whitted pipeline](../ray-rendering/the-whitted-pipeline.md)'s
sequence diagram with the types from this chapter substituted in.
Everything the rest of the book does, geometrically, is some
elaboration of this loop.

## <a id="exercises"></a>Exercises
1. Write `Ray::reflected(const HitPoint&)` that returns a new ray
   whose origin is the hit point and whose direction is the
   reflection of this ray's direction off the surface. (Use the
   formula from the Numbers and vectors exercises.) Call `epsilonShifted()`
   on the result. Why?
2. Why does `Ray::epsilon` differ between `float` (1e-4) and
   `double` (1e-7)? What goes wrong if you set the `double`
   epsilon to 1e-12?
3. Construct a `BoundingBox` containing the origin and a single
   point, then compute the intersection of that box with a
   slightly-shifted box. What's the result? Convince yourself it
   matches what the slab method would compute for the same input.
4. Read `HitPointInterval`'s sorting invariant. What can a
   primitive's `intersect()` routine assume about the order in
   which it should append hit points? What breaks if it appends
   them out of order?

## See also

- Volume index: [Foundations](README.md)
- Previous: [Matrices and transforms](matrices-and-transforms.md)
- Next: [Color and buffers](color-and-buffers.md)
- Picked up by:
  - [Primitives and intersection](../ray-rendering/primitives-and-intersection.md)
    — every concrete primitive's `intersect` routine
  - [Constructive solid geometry](../scene-structure/csg.md)
    — set operations on `HitPointInterval`
  - [Spatial acceleration](../scene-structure/spatial-acceleration.md)
    — BVH and grids both build on `BoundingBox`
- Vector vocabulary: [Numbers and vectors](numbers-and-vectors.md)
- Matrix-on-`HitPoint`: [The four-matrix dance: `Instance`](matrices-and-transforms.md#the-four-matrix-dance-instance)

## Source anchors

<!-- source-anchors -->
- `include/core/math/Ray.h`
- `include/core/math/HitPoint.h`
- `include/core/math/HitPointInterval.h`
- `include/core/math/BoundingBox.h`
- `include/core/math/Range.h`
- `include/core/math/Rect.h`
<!-- /source-anchors -->
