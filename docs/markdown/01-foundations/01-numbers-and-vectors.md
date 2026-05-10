# 1. Numbers and vectors

Every renderer is, at the bottom, a great deal of arithmetic on
small lists of numbers. A ray's direction is three numbers. A
pixel's color is three. A vertex position is three; a homogeneous
point is four. The raytracer renders an image by performing,
literally, billions of these arithmetic operations per frame, all
on objects you can write down on the back of an envelope.

This chapter introduces the fundamental data type those operations
operate on — the **vector** — and the choice of number that fills
its slots. By the end you should know:

- which scalar type the codebase uses, and why,
- how `Vector<N, T>` factors the operations that all vector sizes
  share,
- the dot and cross products, both the math and the geometric
  intuition,
- the unit-length invariant that quietly underpins every shading
  computation in the rest of the book,
- and where the [SSE3](../appendix/a-glossary.md#s) specializations slot in.

## 1.1 Scalars: `double` is the default

The codebase is `double`-first. Every non-color geometric quantity
— ray origins, ray directions, hit-point coordinates, normals,
matrix entries, intersection parameters — is a `double`. There are
two exceptions:

1. **Colors.** `Colord` (the [chapter 4](04-color-and-buffers.md)
   topic) is also `double`, but other color storage may be `float`
   or `unsigned int` depending on whether we're holding [HDR](../appendix/a-glossary.md#h) or [LDR](../appendix/a-glossary.md#l)
   data. That choice is independent of geometry.
2. **The SSE3 specializations.** `Vector3<float>` and
   `Vector4<float>` exist for cases where you specifically need
   the four-floats-per-XMM-register density. Geometry doesn't use
   them; some debug and instrumentation paths do.

Why default to `double`? A `float` carries about 7 decimal digits
of precision; a `double` carries about 16. A scene with a
100-meter extent rendered with `float` precision has around 10
micrometers of slop on every coordinate before quantization noise
becomes visible in shading. That sounds like a lot until you
realize that a self-intersection epsilon used to push a shadow ray
off a surface also lives in those same units, and at large
coordinates the epsilon and the noise become comparable. `double`
makes most of those concerns go away.

`float` would be the right call if we were targeting a GPU
framebuffer that doesn't store doubles, or if we needed to fit
scene data into a tight memory budget. We aren't, and we don't, so
the default is `double`.

The header for the typedefs is short:

```cpp
// include/core/math/Vector.h
typedef Vector2<float>  Vector2f;
typedef Vector2<double> Vector2d;
typedef Vector3<float>  Vector3f;
typedef Vector3<double> Vector3d;
typedef Vector4<float>  Vector4f;
typedef Vector4<double> Vector4d;
```

Throughout the rest of this book, when prose says "vector" without
qualification, it means `Vector3<double>` (`Vector3d`). When it
needs the homogeneous form, it says `Vector4d`.

## 1.2 The shape of `Vector<N, T>`

Reading vector code from any other rendering project, you will
typically see one of two designs:

- **One class per size.** A `Vec3` and a `Vec4` and a `Vec2`, each
  re-implementing `dot`, `length`, `normalize`, `operator+`,
  `operator-`, etc. Concrete, explicit, lots of duplication.
- **One templated class.** A `Vector<N, T>` parameterized by size
  and component type, with all the dimension-agnostic operations
  implemented once.

The codebase picks the second. The class definition is in
[`include/core/math/Vector.h`](../../../include/core/math/Vector.h):

```cpp
// include/core/math/Vector.h:47
template<int Dimensions, class T, class StorageCellType = T,
         class Derived = void>
class Vector {
  // ...
};
```

The `Dimensions` and `T` parameters do the obvious thing. The two
extra parameters are where the design pays for itself.

`StorageCellType` controls the *layout* of the components in
memory. Most of the time it's the same as `T`, which gives you the
naive packed-array layout. The SSE3 specializations override it to
hold the components in an XMM register's worth of storage instead
(see §1.7).

`Derived` is the subclass that wants to inherit the
dimension-agnostic operations. When you write
`Vector3<T>::operator+`, you want the result to be a `Vector3<T>`
— not a `Vector<3, T>` that you then have to convert. The trick is
the curiously-recurring template pattern ([CRTP](../appendix/a-glossary.md#c)): the parent
template knows what the child type is at compile time, and returns
that type from operations defined in the parent. The class
declaration of `Vector3<T>` makes the loop:

```cpp
// include/core/math/Vector.h:638
template<class T>
class Vector3 : public Vector<3, T, T, Vector3<T>> {
  // ...
};
```

Read that as: "I am the size-3 specialization; my storage is `T`;
when you need the *type* of an operation result, it's
`Vector3<T>`, not the parent template instance." The same pattern
applies to `Vector2<T>` and `Vector4<T>`.

You will not have to think about `Derived` ever again after this
chapter. It's mentioned only so the operator signatures later in
the file aren't surprising.

## 1.3 Construction, indexing, and the obvious operators

`Vector<N, T>` has a default constructor that produces the null
vector, a copy constructor for C-array initializers, and a generic
copy constructor that converts between any two `Vector` instances
of the same dimension regardless of the component type. So:

```cpp
Vector3d a;                        // (0, 0, 0)
Vector3d b{1.0, 2.0, 3.0};         // (1, 2, 3)
Vector3d c = b;                    // (1, 2, 3)
Vector3f d = b;                    // (1.0f, 2.0f, 3.0f) — converted
```

Component access uses `x()`, `y()`, `z()` (and `w()` on the
`Vector4` variants), or a generic `coordinate(int i)` that takes a
runtime index. The named accessors compile down to identical code;
the generic one exists for loops over an unknown dimension.

The arithmetic operators compose the way you would expect. From
[`Vector.h`](../../../include/core/math/Vector.h):

- `operator+` and `operator+=` add component-wise.
- `operator-` and `operator-=` subtract component-wise.
- Unary `operator-` negates each component.
- `operator*(const T&)` and `operator/=(const T&)` scale every
  component by the same scalar.
- `operator*(const VectorType&)` is the dot product (see §1.4).
- `operator==` and `operator!=` test exact equality (so use them
  cautiously on floating point).

All of those are defined exactly once on the templated parent and
inherited by every concrete `Vector2`, `Vector3`, `Vector4`. The
only operations that are subclass-specific are the ones whose
result type genuinely depends on the dimension: `Vector3::cross`
makes no sense in 2D; `Vector4::project` (the homogeneous divide)
makes no sense in 3D.

## 1.4 Dot product

The dot product is the workhorse. Two vectors $\mathbf{a}$ and
$\mathbf{b}$ produce a single scalar:

$$
\mathbf{a} \cdot \mathbf{b} = a_x b_x + a_y b_y + a_z b_z
$$

The implementation is one loop:

```cpp
// include/core/math/Vector.h:204
inline T dotProduct(const VectorType& other) const {
  T result = T();
  for (int i = 0; i != Dimensions; ++i) {
    result += coordinate(i) * other.coordinate(i);
  }
  return result;
}
```

`operator*` between two vectors is a synonym, so `a * b` is the
dot product, while `a * 3.0` is the scalar product. The compiler
disambiguates by argument type.

The geometric reading is what makes the dot product useful. Two
vectors $\mathbf{a}$ and $\mathbf{b}$ separated by an angle
$\theta$ satisfy

$$
\mathbf{a} \cdot \mathbf{b} = \lVert\mathbf{a}\rVert \, \lVert\mathbf{b}\rVert \cos\theta
$$

so the dot product reads off the cosine of the angle between them
(once you divide out the lengths). When **both** vectors are unit
length, that division is free and $\mathbf{a} \cdot \mathbf{b}$
*is* the cosine. This is the entire reason the unit-length
invariant in §1.6 matters.

Three uses of the dot product show up in essentially every chapter
that follows:

1. **[Lambertian](../appendix/a-glossary.md#l) shading.** The brightness of a surface lit by a
   directional light is $\mathbf{n} \cdot \mathbf{l}$, where
   $\mathbf{n}$ is the surface normal and $\mathbf{l}$ points
   toward the light. Both unit length, so the dot product is the
   cosine of the incident angle directly.
   ([Chapter 8](../02-ray-rendering/08-materials-and-brdfs.md).)
2. **Plane intersection.** A ray $\mathbf{p}(t) = \mathbf{o} +
   t\mathbf{d}$ hits a plane with normal $\mathbf{n}$ and distance
   $D$ from the origin at $t = (D - \mathbf{n} \cdot \mathbf{o}) /
   (\mathbf{n} \cdot \mathbf{d})$. Two dot products, one division.
   ([Chapter 7](../02-ray-rendering/07-primitives-and-intersection.md).)
3. **Side-of-plane tests.** $\mathbf{n} \cdot (\mathbf{p} -
   \mathbf{p}_0)$ is positive on one side of the plane through
   $\mathbf{p}_0$ with normal $\mathbf{n}$, negative on the other,
   zero on the plane. The signed-area test that drives [Pineda](../appendix/a-glossary.md#p)'s
   rasterization algorithm
   ([chapter 18](../04-rasterization/18-the-rasterization-pipeline.md))
   is this in 2D.

## 1.5 Cross product

Cross product is 3D-only. Given $\mathbf{a}$ and $\mathbf{b}$ in
3D, $\mathbf{a} \times \mathbf{b}$ is the vector perpendicular to
both, with length $\lVert\mathbf{a}\rVert\,\lVert\mathbf{b}\rVert\sin\theta$ and
direction set by the right-hand rule:

$$ \mathbf{a} \times \mathbf{b} = \begin{pmatrix} a_y b_z - a_z b_y \\ a_z b_x - a_x b_z \\ a_x b_y - a_y b_x \end{pmatrix} $$

The implementation reads straight off the formula:

```cpp
// include/core/math/Vector.h:801
inline Vector3<T> crossProduct(const Vector3<T>& other) const {
  return Vector3<T>(y() * other.z() - z() * other.y(),
                    z() * other.x() - x() * other.z(),
                    x() * other.y() - y() * other.x());
}
```

The `operator^` overload is a synonym, so `a ^ b` is the cross
product. (XOR for vectors makes no sense, so the operator was
free.)

What it's used for, in the rest of the book:

- **Building orthogonal frames.** Given a forward direction
  $\mathbf{f}$ and an up direction $\mathbf{u}$, the right
  direction is $\mathbf{r} = \mathbf{f} \times \mathbf{u}$. This
  is how cameras assemble their basis
  ([chapter 6](../02-ray-rendering/06-cameras.md)).
- **Triangle normals.** A triangle with vertices $\mathbf{a}$,
  $\mathbf{b}$, $\mathbf{c}$ has the (unnormalized) face normal
  $(\mathbf{b} - \mathbf{a}) \times (\mathbf{c} - \mathbf{a})$.
  [Möller-Trumbore](../appendix/a-glossary.md#m) intersection
  ([chapter 7](../02-ray-rendering/07-primitives-and-intersection.md))
  builds on this.
- **Signed area in 2D.** The $z$-component of $(\mathbf{b} -
  \mathbf{a}) \times (\mathbf{c} - \mathbf{a})$ is twice the
  signed area of the triangle in the $xy$-plane. This is the
  inside-outside test the rasterizer uses
  ([chapter 18](../04-rasterization/18-the-rasterization-pipeline.md)).

The result of a cross product is *not* in general a unit vector.
If you want a unit normal, normalize after.

## 1.6 Length, normalization, and the unit-length invariant

The length of a vector is just the square root of its dot product
with itself:

$$
\lVert\mathbf{v}\rVert = \sqrt{\mathbf{v} \cdot \mathbf{v}}
$$

```cpp
// include/core/math/Vector.h:306
inline T length() const {
  return std::sqrt(squaredLength());
}

inline T squaredLength() const {
  return derived() * derived();
}
```

`squaredLength` exists separately because *most ray code that asks
"is this vector long enough?" doesn't need the square root*. If
you only need to compare lengths, comparing the squares is
equivalent — and a square root is one of the most expensive
floating-point operations on the CPU. Reach for `squaredLength`
unless you actually need the absolute length.

A unit vector is a vector of length 1. Normalizing makes one:

```cpp
// include/core/math/Vector.h:356
inline VectorType normalized() const {
  return derived() / length();
}

inline void normalize() {
  derived() /= length();
}
```

There are two flavors. `normalized()` returns a fresh unit vector
and leaves the original alone; `normalize()` mutates in place. The
naming convention is the same one `reverse` / `reversed` and
`transpose` / `transposed` use elsewhere in the math library.

### The invariant

> **Most ray code in this codebase assumes that direction vectors
> are normalized.**

This is one of those project conventions you can't see by reading
a single function. The [Whitted](../appendix/a-glossary.md#w) tracer assumes `Ray::direction()`
is unit length. The shading code assumes the surface normal you
hand it from a `HitPoint` is unit length. The Lambertian [BRDF](../appendix/a-glossary.md#b)
assumes the light direction is unit length. The reflection
formula $\mathbf{r} = \mathbf{i} - 2(\mathbf{i} \cdot
\mathbf{n})\mathbf{n}$ only produces a unit reflected direction
when $\mathbf{i}$ and $\mathbf{n}$ both are.

If you violate this — pass a non-unit normal to a shading
function, or skip the normalize step after a cross product — the
math doesn't crash. It just produces subtly wrong results: a
shaded surface that's twice as bright as it should be, a
reflection ray that diverges over recursive bounces, a shadow ray
whose `t` parameter doesn't measure world-space distance anymore.
These are some of the hardest bugs to track down in a renderer
because they masquerade as "the lighting looks slightly off."

When in doubt, normalize. When you know the producer normalized
already (because you read the code), don't pay for the second
normalization. The convention works because every public API that
returns a direction does the normalize before returning. Your job
when adding a new direction-producing function is to keep that
invariant honest.

The cheap sanity check is `isNormalized()`, which compares the
exact length to 1:

```cpp
// include/core/math/Vector.h:372
inline bool isNormalized() const {
  return length() == T(1);
}
```

This is exact-equality on a floating-point quantity, so it really
tests "did this vector survive a normalization step without any
subsequent mutation," not "is this approximately unit length." For
approximate checks, compute `length()` and compare against 1 with
a tolerance.

## 1.7 The SSE3 specializations

The headers under
[`include/core/math/vector/sse3/`](../../../include/core/math/vector/sse3/)
override specific instantiations — `Vector3<float>`, `Vector4<float>`,
and `Vector4<double>` — to use SSE / SSE3 intrinsics for storage and
arithmetic.

The trick is in the storage type. The default `Vector3<float>`
holds three floats in an array of three floats — 12 bytes. The SSE3
specialization holds them in a single `__m128` register — 16 bytes,
with the fourth lane zeroed:

```cpp
// include/core/math/vector/sse3/Vector3f.h
template<>
class Vector3<float> : public Vector<3, float, __m128, Vector3<float>> {
  // ... constructors and operators using _mm_add_ps, _mm_mul_ps, etc.
};
```

The add and subtract operators become single `_mm_add_ps` /
`_mm_sub_ps` calls that operate on all four lanes in one instruction.
On a hot loop that touches a million vectors, this is measurably
faster than the scalar form.

`Vector3<double>` does *not* have an SSE3 specialization — benchmarking
(Phase 2.3) showed that fixing the UB in the original two-`__m128d`
implementation made dot product 80% slower than scalar, and AVX2 was
2× slower on cross product. The scalar path with the compiler's
autovectorizer wins on every operation and is what the generic template
provides.

The specializations are entirely transparent: you write `Vector3f
a, b; auto c = a + b;` and the SSE3 path is selected automatically
when you compile with `-msse3` (which the project's release preset
does). When you compile without SSE3 — for a target that doesn't
have it, or in a debug build — the generic template definition
takes over and the same source code still works.

The book treats this as an implementation detail. The math is the
same; the API is the same; the operators are the same. SSE3 only
matters when you're optimizing a hot path or chasing a precision
discrepancy.

## 1.8 Exercises

1. Write a function that takes three points $\mathbf{a}$,
   $\mathbf{b}$, $\mathbf{c}$ and returns the unit normal of the
   triangle they define. Use `crossProduct` and `normalized`.
   What's the geometric meaning of the *un-normalized* result's
   length?
2. The reflection formula is $\mathbf{r} = \mathbf{i} - 2(\mathbf{i}
   \cdot \mathbf{n})\mathbf{n}$. Implement it as a free function
   that takes the incident direction $\mathbf{i}$ and the surface
   normal $\mathbf{n}$ and returns the reflected direction. Assume
   both inputs are unit length. Convince yourself by hand that the
   output is also unit length.
3. Find one place in the codebase where `squaredLength` is used in
   preference to `length`. Why? Try replacing it with `length` and
   describe what would change in the rendered output.
4. Read the SSE3 `Vector3<float>` operator implementations in
   [`include/core/math/vector/sse3/Vector3f.h`](../../../include/core/math/vector/sse3/Vector3f.h).
   Do they produce the same result as the generic template bit-for-bit?
   Are there inputs where they wouldn't?

## See also

- Volume index: [Volume I — Foundations](README.md)
- Next chapter: [2. Matrices and transforms](02-matrices-and-transforms.md)
- Used in:
  - [3. Rays and geometry](03-rays-and-geometry.md) — origin,
    direction, the whole chapter rides on `Vector3d`
  - [7. Primitives and intersection](../02-ray-rendering/07-primitives-and-intersection.md) — hit-point math, per-shape derivations
  - [8. Materials and BRDFs](../02-ray-rendering/08-materials-and-brdfs.md) — the dot-product cosines from §1.4

## Source anchors

<!-- source-anchors -->
- `include/core/math/Vector.h`
- `include/core/math/vector/sse3/`
- `include/core/math/Number.h`
- `include/core/math/Constants.h`
<!-- /source-anchors -->
