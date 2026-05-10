# 2. Matrices and transforms

A scene is full of geometry placed at different positions, oriented
differently, and scaled differently. A matrix is how the codebase
records "this object is two meters to the right, rotated 30 degrees
around its up axis, and twice as tall as the unit version." This
chapter explains how matrices encode those transforms, the two
gotchas (point-versus-direction, normals-versus-everything-else)
that catch every newcomer to graphics, and how `Instance` packages
the whole thing into a single class.

By the end you should know:

- why we work in 4×4 even though the world is three-dimensional,
- how to transform a *point* and how that differs from
  transforming a *direction*,
- the inverse-transpose trick that keeps normals perpendicular
  under non-uniform scaling,
- the composition-order convention this codebase uses (and how to
  read `M = T * R * S` in it),
- where quaternions slot in,
- and the four-matrix dance in
  [`Instance::setMatrix`](../../../include/render/primitives/Instance.h),
  which is the one place the rest of the book really cares about.

## 2.1 Why 4×4

A 3×3 matrix can rotate, scale, and shear, but it can't
*translate*. The arithmetic of matrix-vector multiplication has no
slot for a constant offset — every entry of the output is a linear
combination of the input components.

Translation is an *affine* operation, not linear. To express it as
a matrix multiplication, we pad both the matrix and the vector
with one extra row / column. A point $(x, y, z)$ becomes the
homogeneous 4-vector $(x, y, z, 1)$, and a direction $(d_x, d_y,
d_z)$ becomes $(d_x, d_y, d_z, 0)$. The translation by
$(t_x, t_y, t_z)$ becomes the matrix:

$$ T = \begin{pmatrix} 1 & 0 & 0 & t_x \\ 0 & 1 & 0 & t_y \\ 0 & 0 & 1 & t_z \\ 0 & 0 & 0 & 1 \end{pmatrix} $$

Multiply $T$ by the homogeneous point $(x, y, z, 1)^T$ and you get
$(x + t_x, y + t_y, z + t_z, 1)^T$ — the translated point. Multiply
the same $T$ by the homogeneous direction $(d_x, d_y, d_z, 0)^T$
and the translation contribution drops out entirely (because the
fourth component is zero), leaving the unchanged direction. That's
exactly what we want: translating a point moves it; translating a
direction does nothing, because directions only carry "which way,"
not "from where."

Concretely: the codebase uses
[`Matrix4<double>`](../../../include/core/math/Matrix.h) for every
spatial transform. Camera-to-world, world-to-camera, model-to-world
on an `Instance`, the homogeneous clip-volume projection in the
rasterizer ([chapter 19](../04-rasterization/19-clipping-depth-stencil.md))
— all `Matrix4d`. The 3×3 form (`Matrix3<double>`) shows up only
where translation is genuinely irrelevant, namely transforming
*directions* and *normals* (§2.4).

## 2.2 The shape of `Matrix<N, T>`

The class follows the same templated layout as the vector class
from [chapter 1](01-numbers-and-vectors.md). From
[`include/core/math/Matrix.h`](../../../include/core/math/Matrix.h):

```cpp
// include/core/math/Matrix.h
template<int Dimensions, class T,
         class VectorType = Vector<Dimensions, T>,
         class Derived = void>
class Matrix {
  // ...
};

template<class T>
class Matrix4 : public Matrix<4, T, Vector4<T>, Matrix4<T>> { ... };
```

The CRTP `Derived` parameter does the same job it did for `Vector`
— operator return types come back as the derived class instead of
the parent template instance. The `VectorType` parameter encodes
which vector size pairs with which matrix size, so
`Matrix4d * Vector4d` produces a `Vector4d` and the type system
catches a `Matrix4d * Vector3d` mistake at compile time.

The default constructor produces the *identity* matrix — diagonal
of ones, zeros elsewhere. A "fresh" `Matrix4d` is the do-nothing
transform.

The class ships static factory methods for the common transforms:

```cpp
// translations
Matrix4d::translate(Vector3d(2, 0, 0));
Matrix4d::translate(2, 0, 0);

// rotations
Matrix4d::rotateX(Angle::fromDegrees(45));
Matrix4d::rotateY(angle);
Matrix4d::rotateZ(angle);
Matrix4d::rotate(angleX, angleY, angleZ);

// scaling
Matrix4d::scale(2.0);                            // uniform
Matrix4d::scale(2.0, 1.0, 0.5);                  // per-axis
```

The `Angle` type from
[`include/core/math/Angle.h`](../../../include/core/math/Angle.h)
is a strongly-typed scalar so the API can't mix radians and
degrees by accident. (Mixing those is one of the most common
graphics bugs in any library that uses raw `double` for both.)

The arithmetic operators do the textbook thing: `A * B` is matrix
multiplication, `A * v` is matrix-vector multiplication, `A +
B` and `A - B` are componentwise, `A.transposed()` returns the
transpose, `A.inverted()` returns the inverse, and
`A.determinant()` returns the scalar determinant.

## 2.3 Composition order: column-vector convention

Two camps exist on this question. The **row-vector** camp writes a
vector as a 1×N row and applies transforms by post-multiplying:
$\mathbf{v}' = \mathbf{v} M$. The transform on the right of an
expression is the *first* applied. The **column-vector** camp
writes a vector as an N×1 column and applies transforms by
pre-multiplying: $\mathbf{v}' = M \mathbf{v}$. The transform on
the right of an expression is the *last* applied. Both produce
the same image; the two conventions are simply transposes of each
other.

This codebase is column-vector. So when you see:

```cpp
Matrix4d M = Matrix4d::translate(2, 0, 0)
           * Matrix4d::rotateY(angle)
           * Matrix4d::scale(2.0);
Vector4d v_world = M * v_local;
```

Read it right-to-left: scale first, then rotate around Y, then
translate. The resulting `M` is the model-to-world matrix for an
object that's "twice as big, rotated, and offset two units to the
right of the origin."

The mental shortcut: the transform *closest to the vector* is
applied *first*. That's the operation the vector "sees" first as
it travels through the matrix product. Matrix multiplication is
associative, so the parenthesization doesn't matter — but the
*order* does, because matrix multiplication isn't commutative.
Translate-then-rotate is not the same as rotate-then-translate
(the latter rotates the translation vector along with the object).

If you confuse the order, the symptom is usually obvious: an
object that's supposed to be at the origin ends up wandering on a
circle, or a translation ends up scaled by a factor. The math is
right; the operation order is wrong.

## 2.4 Point vs direction vs normal

This is the section that catches everyone, so it's worth being
explicit.

### Points

A point in 3D space becomes the homogeneous 4-vector $(x, y, z,
1)$. Transformation is the obvious thing: `M * p_homogeneous`
gives you the transformed point's homogeneous coordinates. The
final $w$ component will normally still be 1 (for affine
transforms — translation, rotation, scale, shear); it differs from
1 only when a *projective* transform is involved (a real camera
projection, the clip-space stuff in
[chapter 19](../04-rasterization/19-clipping-depth-stencil.md)),
in which case you divide all four components by $w$ to recover
the projected 3D point. That's the "perspective divide."

For pure scene-graph transforms, you can ignore $w$ entirely and
treat the result as a 3D point.

### Directions

A direction becomes the homogeneous 4-vector $(d_x, d_y, d_z, 0)$.
The zero in the last slot makes the translation column of the
matrix drop out, so directions only get rotated, scaled, and
sheared — not translated. This is exactly what you want: "north"
is the same direction whether you're standing in Berlin or Tokyo;
moving the observer doesn't change which way is north.

`Matrix3d` (the rotation-and-scale-only form) is a more honest
representation of the same idea: it has no translation column at
all. The `Instance` implementation pulls the upper-left 3×3
submatrix out of the full `Matrix4d` for exactly this reason — see
§2.6.

### Normals

Here's the gotcha. **Normals don't transform like directions.**

A normal is the direction perpendicular to a surface. If you take
a sphere and stretch it horizontally so it becomes an ellipsoid
(non-uniform scaling), the normal at a given surface point now
points in a different direction *relative to the new surface*
than the rotated-and-scaled direction would.

The math is short: if $M$ is the matrix that transforms a surface,
the matrix that correctly transforms its normals is $\big(M^{-1}
\big)^T$ — the inverse-transpose of $M$. Why: the normal is the
gradient of the implicit equation that defines the surface, and
the gradient transforms by the inverse-transpose of the surface's
own transform (see any standard graphics text for the derivation).

For a *uniform* scale and pure rotation, $M$ is orthogonal up to a
scalar, so $\big(M^{-1}\big)^T = M$ (modulo the scalar) and you
don't see the difference. As soon as the scale is non-uniform,
$M^{-1}$ stops equaling $M^T$ and the difference becomes visible:
the "naive" transformed normal tilts away from perpendicular,
while the inverse-transpose transformed normal stays correctly
perpendicular to the deformed surface. The §2.6 widget shows
this side-by-side in the context of the full `Instance` transform.

The practical rule: any time you write code that transforms a
normal, use the normal matrix, not the direction matrix. The
codebase enforces this through the `Instance` interface — you
never construct a normal-transform manually; `Instance` does it
for you (§2.6).

## 2.5 Quaternions, briefly

Rotations are a special case worth a separate representation.
[`Quaternion<T>`](../../../include/core/math/Quaternion.h) is a
4-tuple $(w, x, y, z)$ that encodes a rotation as $w =
\cos(\theta/2)$ and $(x, y, z) = \sin(\theta/2) \hat{\mathbf{a}}$,
where $\hat{\mathbf{a}}$ is the unit rotation axis and $\theta$ is
the rotation angle.

Three things make quaternions worth carrying:

1. **No gimbal lock.** Sequences of Euler-angle rotations
   (rotate-X-then-Y-then-Z) have a singularity where one axis
   "collapses into" another and you lose a degree of freedom. The
   classic Apollo-program-spaceship near-disaster. Quaternions
   don't have this problem.
2. **Cheap interpolation.** SLERP (spherical linear interpolation)
   between two quaternions produces a smooth, constant-angular-
   velocity rotation path between two orientations. Doing the same
   with rotation matrices requires more work and produces less
   pleasant results.
3. **Compact storage.** Four numbers vs nine for a 3×3 rotation
   matrix.

The codebase treats them as a side conversion: `Matrix4` exposes
`toQuaternion()` and `Quaternion` exposes `toMatrix4()`, but the
spatial-transform pipeline always feeds matrices into the
intersection / projection code. Quaternions live where they win
(animation interpolation, anywhere a rotation needs to be slerped
between keyframes). For a Whitted raytracer doing static scenes,
they're a shelf item.

## 2.6 The four-matrix dance: `Instance`

The whole point of all this is to put it to work.
[`Instance`](../../../include/render/primitives/Instance.h) wraps a
`Primitive` plus a `Matrix4d` so the same underlying geometry can
appear at multiple positions, orientations, and scales without
duplicating the geometry data. Its `setMatrix` routine is where
the chapter pays off:

```cpp
// src/render/primitives/Instance.cpp:73
void Instance::setMatrix(const Matrix4d& matrix) {
  m_pointMatrix     = matrix;
  m_originMatrix    = matrix.inverted();
  m_directionMatrix = Matrix3d(m_originMatrix);
  m_normalMatrix    = m_directionMatrix.transposed();
}
```

All four matrices are precomputed from the one input. Reading them
in order:

- **`m_pointMatrix`** is the matrix you provided. It's used to
  transform hit-point world positions back to world space after
  the wrapped primitive's intersection routine has produced them
  in local space.
- **`m_originMatrix`** is the *inverse*. It's used at intersection
  time to transform the world-space ray origin *into the wrapped
  primitive's local space*. The wrapped primitive (a unit sphere,
  say, defined at the origin) doesn't know it's been instanced
  somewhere else; it sees a ray that's already in its local
  coordinates.
- **`m_directionMatrix`** is the upper-left 3×3 of `m_originMatrix`
  — the "no translation" version, used to transform the ray's
  *direction* into local space. Same reason: directions don't
  translate.
- **`m_normalMatrix`** is the *transpose* of `m_directionMatrix`.
  Since `m_directionMatrix` is already the inverse, the transpose
  of it is the inverse-transpose of the original — exactly the
  matrix the §2.4 normal-transform rule needs.

The widget below shows the four matrices in action. The left side
is the world-space ray; the right side is the same ray transformed
into the primitive's local space (`m_originMatrix` on the origin,
`m_directionMatrix` on the direction). The compared "naive normal"
vs "inverse-transpose normal" arrows on the deformed shape show
why `m_normalMatrix` exists as a separate precomputed matrix.

<!-- widget: instance_transform_normals -->

That's the full content of "what you need to know about
matrices to follow the rest of the book." Every matrix-touching
piece of code downstream uses these four conventions, and the
shipped tests pin them.

## 2.7 Exercises

1. Predict the result of `Matrix4d::translate(2, 0, 0) *
   Matrix4d::rotateY(Angle::fromDegrees(90)) * Vector4d(1, 0, 0,
   1)`. Then write it out and check.
2. Construct a non-uniform scale (say, $(2, 1, 1)$). Compute its
   inverse-transpose by hand. For a normal pointing in the
   $\mathbf{x}$ direction, where does the naive scaled normal
   point? Where does the correctly-transformed normal point?
3. Read `Instance::setVelocity` and look up how `m_pointMatrix`
   relates to motion blur (covered in
   [chapter 16](../03-scene-structure/16-instances-and-motion-blur.md)).
   Why doesn't the velocity affect any of the *other* three
   matrices?
4. The codebase doesn't precompute a `m_originDirection3Matrix`
   that would skip building `m_directionMatrix` from
   `m_originMatrix`. Why? (Hint: think about what it would cost
   when nothing in the scene uses the velocity-matrix path.)

## See also

- Volume index: [Volume I — Foundations](README.md)
- Previous: [1. Numbers and vectors](01-numbers-and-vectors.md)
- Next: [3. Rays and geometry](03-rays-and-geometry.md)
- The full `Instance` story:
  [16. Instances and motion blur](../03-scene-structure/16-instances-and-motion-blur.md)
- Where projective transforms (the $w$-divide) come up:
  [19. Clipping, depth, stencil](../04-rasterization/19-clipping-depth-stencil.md)

## Source anchors

<!-- source-anchors -->
- `include/core/math/Matrix.h`
- `include/core/math/Quaternion.h`
- `include/render/primitives/Instance.h`
<!-- /source-anchors -->
