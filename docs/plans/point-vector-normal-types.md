# Point / Vector / Direction / Normal type split

> **Scope:** introduce semantic geometric types — `Point`, `Vector`,
> `Direction`, `Normal` — alongside the existing `Vector3<T>` /
> `Vector4<T>` in `include/core/math/`, paired with companion matrix
> types `Matrix2<T>` / `Matrix3<T>` / `Matrix4<T>` (existing sizes,
> disambiguated by operand) and `NormalMatrix<T, N>` (inverse-transpose).
> All types parametrized by both element type `T` and dimension `N ∈
> {2, 3}` — the public surface in this plan is 3D only; 2D types
> drop in as a follow-up via the same N-templated machinery without
> a rewrite. Each geometric type carries different transform semantics
> and (in debug builds) a different runtime invariant. Companion doc
> to `core-math-optimization.md`. Captured 2026-05-10 from the
> conversation that motivated the idea; design refined 2026-05-11.
>
> **Status:** Living document — design proposal, not yet committed.
> Most open questions from the original draft are resolved (see
> [Resolved design decisions](#resolved-design-decisions) below); the
> few remaining are tagged as such.
>
> **Rule:** the headline isn't the debug assertions — it's the
> compile-time prevention of wrong-transform-on-normal and similar
> class-of-bug errors. Runtime invariant enforcement is a bonus that
> catches a strict subset of issues the type system can't (denormalized
> directions stored after arithmetic). Don't sell the plan on the
> assertions; sell it on the type-correct transforms.

---

## Motivating bugs

The pattern this plan adopts is the PBRT split (Point3 / Vector3 /
Normal3). The specific bug classes it eliminates at the type level:

- **Normals transformed with the forward matrix.** A surface normal
  under a non-orthonormal transform must be transformed with the
  *inverse-transpose* of the matrix, not the matrix itself. Today
  every call site that transforms a normal has to remember this.
  `Matrix * Normal` should silently pick the right operation.
- **Translation applied to a direction.** A ray's direction vector
  has homogeneous w=0; the current 4×4 transform path "works"
  because callers happen to use the 3×3 sub-block, but there's
  nothing stopping a careless edit from passing the direction
  through the full affine path.
- **Arithmetic that doesn't type-check geometrically.** `Point +
  Point` is meaningless. `Point - Point` produces a vector, not a
  point. Scaling a point by a scalar is meaningless (you can scale
  its homogeneous representation but the result is no longer at the
  same projective position). The current `Vector3<T>` allows all of
  these — code that wants to be careful has to be careful by
  convention.
- **Denormalized directions/normals silently propagating.** A
  direction that gets non-uniform-scaled and not renormalized
  produces wrong cosine terms in lighting, wrong reflection vectors,
  etc. Debug assertions on assignment catch this at the boundary.

The type system catches the first three at compile time. The debug
invariants catch the fourth at the earliest write that breaks it.

---

## The four types

All parametrized by element type `T` (`float` / `double`) and
dimension `N` (`2` / `3`). The public surface in this plan ships 3D
only; 2D types come in a follow-up (no rewrite — see
[Implementation machinery](#implementation-machinery)).

**Concrete classes**, not typedefs of a generic template: each type
gets its own `class` declaration inheriting from the shared template
base. Distinct type identity in error messages, debuggers, and reader
intuition.

**Element-type typedefs** in the glm / Eigen style:

```cpp
using Point3f = Point3<float>;
using Point3d = Point3<double>;
using Direction3f = Direction3<float>;
using Direction3d = Direction3<double>;
using Normal3f = Normal3<float>;
using Normal3d = Normal3<double>;
using Vector3f = Vector3<float>;
using Vector3d = Vector3<double>;
// 2D analogs land in the follow-up phase.
```

### `Point<T, N>` — a position in space

- **Stores `w` explicitly** as part of the type's storage. `Point2`
  is `(x, y, w)` — 3 components. `Point3` is `(x, y, z, w)` — 4
  components. `w` defaults to 1.0 at construction.
- Transforms via the full affine matrix (`Matrix3 * Point2` for 2D,
  `Matrix4 * Point3` for 3D — see [Matrix transforms by type](#matrix-transforms-by-type)).
- Invariant (debug): `w` ≈ 1 within tolerance.
- Conceptually a *coordinate*, not a magnitude.
- The explicit `w` is the load-bearing design choice that lets the
  matrix companion types stay size-named (`Matrix3`, `Matrix4`)
  without semantic ambiguity — see
  [Why explicit `w`](#why-explicit-w-on-point).

### `Vector<T, N>` — a free-form displacement

- Stores N components only (no w; w is implicitly 0 for displacement
  arithmetic).
- Transforms via the linear-only matrix (`Matrix2 * Vector2`,
  `Matrix3 * Vector3`).
- No magnitude invariant; this is the "anything else" bucket.
- The closest type to the existing `Vector3<T>`. Most current uses
  of `Vector3` are semantically Vectors.

### `Direction<T, N>` — a unit-length displacement

- Stores N components only.
- Transforms via the linear matrix, **renormalized** if the matrix is
  non-orthonormal (or skipped via an `isOrthonormal` tag — open
  question, [#10](#open-questions)).
- Invariant (debug): `|d|` ≈ 1.
- Use cases: ray directions, light directions, view directions.

### `Normal<T, N>` — a unit-length surface normal

- Stores N components only.
- Transforms via the **inverse-transpose** of the linear matrix
  (`NormalMatrix<T, N>`). This is the load-bearing differentiator
  from Direction — see [Matrix companion types](#matrix-companion-types).
- Invariant (debug): `|n|` ≈ 1.
- Use cases: shading normals, geometric normals, anything used in a
  cosine-weighted lighting integrand.

Direction and Normal share an invariant (unit length) but differ in
*how they transform*. That difference is the entire reason for the
two-type split rather than one `UnitVector`.

### Why explicit `w` on Point

Storing `w` on `Point` makes the matrix-companion design clean:

- `Matrix3` is "a 3×3 matrix." It applies to anything whose storage
  is 3 components — `Vector3` (linear-3D transform of a displacement),
  or `Point2` (affine-2D transform of a homogeneous position). The
  *operand's stored layout* decides what the operation means; the
  matrix size doesn't have to encode the semantic.
- `Matrix4` is "a 4×4 matrix" — applies to `Vector4` or `Point3`.
- No naming collision between "linear-3D" and "affine-2D" — both are
  `Matrix3 * <3-component-thing>`, and the 3-component-thing's type
  carries the geometry.
- Perspective division becomes the natural operation it should be:
  divide `(x, y, z, w)` by `w`. No special "now interpret as
  homogeneous" mode.

Storage cost: `Point2` is 3 floats instead of 2; `Point3` is 4
instead of 3. For SIMD this often helps — `Point3<float>` aligns to
16 bytes naturally, which most SIMD-friendly graphics libraries do
anyway for the same reason.

---

## Arithmetic and conversion algebra

The legal-operation table the types enforce:

| Operation | Result | Notes |
| --- | --- | --- |
| `Point + Vector` | `Point` | The canonical "move from a point" |
| `Point - Vector` | `Point` | Same, reversed |
| `Vector + Point` | `Point` | Commutative shorthand |
| `Point - Point` | `Vector` | "Displacement from a to b" |
| `Point + Point` | **illegal** | Geometrically meaningless |
| `Vector + Vector` | `Vector` | |
| `Vector - Vector` | `Vector` | |
| `Vector * scalar` | `Vector` | |
| `Point * scalar` | **illegal** | Use homogeneous scaling explicitly if needed |
| `Direction + Direction` | `Vector` | Result is not unit-length; drops to Vector |
| `Direction * scalar` | `Vector` | Same — scaled direction is a vector |
| `Direction - Direction` | `Vector` | Same |
| `Normal + Normal` | `Vector` | Same; drops to Vector |
| `dot(Vector, Vector)` | scalar | Same as today |
| `dot(Direction, Direction)` | scalar | Returns cosine (∈[-1,1] for unit vectors) |
| `dot(Normal, Direction)` | scalar | The cosine that lighting integrands depend on |
| `cross(Vector, Vector)` | `Vector` | |
| `cross(Direction, Direction)` | `Vector` | Result is not unit-length |
| `Vector.toDirection()` | `Direction` | Normalizes and tags as a direction |
| `Vector.toNormal()` | `Normal` | Normalizes and tags as a surface normal |
| `Direction → Vector` | implicit | Direction is-a Vector with a stronger invariant; widening is safe |
| `Normal → Vector` | implicit | Same |
| `Vector → Direction` | **explicit only** | Must call `.toDirection()` |
| `Vector → Normal` | **explicit only** | Must call `.toNormal()` |
| `Direction ↔ Normal` | **explicit only** | Different transform semantics; never silently — use `.toNormal()` / `.toDirection()` on the source |

**Key design choice:** widening (Direction → Vector) is implicit;
narrowing (Vector → Direction) is explicit. The asymmetry mirrors
`const_cast` vs. ordinary copy in C++ — you can always weaken a
promise without saying so; strengthening requires you to assert it
intentionally.

The Direction ↔ Normal conversion being explicit-only is the most
important rule: it forces the caller to acknowledge they're switching
which transform semantics will apply. The `.toDirection()` /
`.toNormal()` naming carries this in the function name itself, where
a generic `.normalized()` would have been mathematically vague.

**No bare `.normalized()` on `Vector`.** If you want a unit-length
3-tuple, you want either a Direction or a Normal; call one. "I want a
unit-length thing that isn't either" is a code smell — what's its
geometric role? Force the answer at the call site.

---

## Matrix transforms by type

The transform overloads are where the types earn their keep. Because
`Point` stores its homogeneous `w` explicitly, the matrix type does
NOT need to encode the geometric semantic — same-size matrices apply
to any matching-size operand, and the operand's stored layout decides
what the operation means.

```cpp
// 3D transforms (the surface this plan ships first)
Matrix4<T>            * Point3<T>     → Point3<T>      // full affine, w preserved
Matrix3<T>            * Vector3<T>    → Vector3<T>     // linear-3D on a displacement
Matrix3<T>            * Direction3<T> → Direction3<T>  // linear-3D + optional renormalize
Matrix3<T>            * Normal3<T>    → illegal        // must go via .normalMatrix()
NormalMatrix<T, 3>    * Normal3<T>    → Normal3<T>     // inverse-transpose, computed once

// 2D transforms (lands in the follow-up phase; same template machinery)
Matrix3<T>            * Point2<T>     → Point2<T>      // affine-2D on the (x,y,1) homogeneous
Matrix2<T>            * Vector2<T>    → Vector2<T>     // linear-2D on a displacement
Matrix2<T>            * Direction2<T> → Direction2<T>  // linear-2D + optional renormalize
Matrix2<T>            * Normal2<T>    → illegal        // via NormalMatrix<T, 2>
NormalMatrix<T, 2>    * Normal2<T>    → Normal2<T>     // 2×2 inverse-transpose
```

Notice the `Matrix3 * Point2` row: same matrix size as `Matrix3 *
Vector3`, completely different operation. The disambiguation lives in
the operand: `Point2` stores `(x, y, w=1)` so `Matrix3 * Point2` is
the 3×3 affine-2D multiply (translation included via the bottom row);
`Vector3` stores `(x, y, z)` so `Matrix3 * Vector3` is the 3×3
linear-3D multiply. No matrix-naming-collision; the matrix is just
"a matrix of this size."

The `Matrix * Normal` operation does not exist. To transform a
normal, the caller must first explicitly construct the
`NormalMatrix<T, N>` via `.normalMatrix()`. This is the central
design choice that makes the wrong-matrix-on-normal bug a compile
error rather than a silent runtime hazard, and lets the
inverse-transpose cost be paid once per matrix rather than per call.
See the [Matrix companion types](#matrix-companion-types) section for
the full type set and operator-overload table.

For `Matrix * Direction`, the renormalize step is unnecessary for
orthonormal matrices. Two options:

- **Always renormalize.** Simple, safe, costs a sqrt per transform.
- **Skip renormalize when the matrix is known-orthonormal.** Tag
  matrices with an `isOrthonormal` bit at construction (rotations,
  reflections, identity); the transform path branches on the tag.

The second is faster but requires every matrix factory to track the
tag correctly. Default to always-renormalize in v1, add the tag in v2
once the call sites are stable.

The existing `Matrix4::transformPoint(Vector3)` /
`transformDirection(Vector3)` proposed in Phase 2.5 of the core-math
optimization plan become natural overloads of `operator*` once the
types exist — they don't go away, they become the canonical operator.

---

## Matrix companion types

The matrix types stay **named by size** (`Matrix2`, `Matrix3`,
`Matrix4`) — the geometric semantic comes from the operand, not the
matrix. There's one additional type for the inverse-transpose case
that *can't* be carried by matrix size alone: `NormalMatrix<T, N>`.

### `Matrix2<T>` / `Matrix3<T>` / `Matrix4<T>` — matrices by size

The existing `Matrix4<T>` stays as-is; `Matrix3<T>` already exists in
the codebase as a 3×3 matrix; `Matrix2<T>` is new for the 2D
follow-up. Each is just "an n×n matrix of T" — no embedded geometric
semantic.

Applies to anything whose operand size matches:

- `Matrix4 * Point3` — 4×4 multiply, full affine 3D.
- `Matrix3 * Vector3` — 3×3 multiply, linear-3D on a displacement.
- `Matrix3 * Point2` — 3×3 multiply, affine-2D on a homogeneous (x,y,1).
- `Matrix2 * Vector2` — 2×2 multiply, linear-2D on a displacement.

Construction stays as today: factories (`Matrix4::translate`,
`Matrix3::rotate`, etc.) for common transforms; component-wise ctor
for arbitrary matrices; `Matrix4::linearPart()` returns the
`Matrix3<T>` upper-3×3 (or `Matrix3::linearPart()` returns
`Matrix2<T>` for 2D).

### `NormalMatrix<T, N>` — inverse-transpose, distinct type

Distinct type for the inverse-transpose case. Stores the
already-inverse-transposed n×n matrix internally. Applied only to
`Normal<T, N>` via `NormalMatrix * Normal`. **Not** implicitly
convertible back to the corresponding `Matrix<T, N>` — that
conversion would defeat the entire point of the type. If a caller
really wants the raw matrix back (rare; usually wrong), there's an
explicit `.asMatrix()` escape hatch that loudly forces the caller to
acknowledge they're exiting the type-correct path.

Construction: `Matrix4::normalMatrix()` returns `NormalMatrix<T, 3>`,
performing the inverse-transpose of the 3×3 linear sub-block exactly
once at the call site. `Matrix3::normalMatrix()` returns
`NormalMatrix<T, 3>` directly (no 4D detour). For 2D:
`Matrix3::normalMatrix2()` returns `NormalMatrix<T, 2>` from the 2×2
linear sub-block, or `Matrix2::normalMatrix()` returns it directly.

Element-type typedefs:

```cpp
using NormalMatrix3f = NormalMatrix<float, 3>;
using NormalMatrix3d = NormalMatrix<double, 3>;
using NormalMatrix2f = NormalMatrix<float, 2>;
using NormalMatrix2d = NormalMatrix<double, 2>;
```

Subsequent applications of a constructed `NormalMatrix` are cheap
matrix-vector multiplies — no IT machinery on the per-multiply path.

### What this buys

**Compile-time bug prevention.** The wrong-matrix-on-normal bug
becomes a type error: `matrix4 * normal` doesn't compile (no
overload); the only legal path is `matrix4.normalMatrix() * normal`,
which forces the inverse-transpose to be explicit at the call site.

**Performance.** Inverse-transpose cost is paid once at the
`.normalMatrix()` call. Subsequent `NormalMatrix3 * Normal` operations
are cheap and don't carry IT machinery internally. For a scene that
transforms 10⁶ normals through a single transform, that's
10⁶ − 1 redundant IT computations saved versus a "cache inside
`Matrix * Normal`" strategy.

### Why no `DirectionMatrix`

Directions and Vectors share their transform rule (3×3 linear
sub-block, no translation). A separate `DirectionMatrix` would be
`Matrix3<T>` with a different name — solving a problem that doesn't
exist. The renormalize-on-non-orthonormal-transform concern is
either (a) a per-call decision (always renormalize, the v1 plan) or
(b) a tag on the matrix (`isOrthonormal` bit, the v2 plan); neither
requires a new type.

### Construction boundary, not operator-level specialization

Crucial design principle: `inverse()` and `transpose()` stay on the
general types and return the same general type.
`Matrix3::inverse() → Matrix3`. `Matrix4::transpose() → Matrix4`.
Specialization happens only at *explicit* construction points like
`matrix4.normalMatrix()`.

This avoids the combinatorial overload explosion that "every operator
returns a specialized result type" would create. Composition of normal
matrices still works because (M₁·M₂)⁻ᵀ = M₂⁻ᵀ · M₁⁻ᵀ:
`NormalMatrix3 * NormalMatrix3 → NormalMatrix3`. But you don't get
`NormalMatrix3` from `m1.inverse().transpose()` — you get a plain
`Matrix3` that you'd have to wrap explicitly. (You shouldn't want to;
you want `normalMatrix()`.)

### Operator-overload table for the matrix companions (3D)

| Operation | Result | Notes |
| --- | --- | --- |
| `Matrix4 * Point3` | `Point3` | Full affine 3D; w stored on Point preserves homogeneous semantics |
| `Matrix3 * Vector3` | `Vector3` | Linear-3D on a displacement |
| `Matrix3 * Direction3` | `Direction3` | Linear-3D + optional renormalize |
| `Matrix3 * Normal3` | **illegal** | Forces caller to use `.normalMatrix()` |
| `Matrix4 * Vector3` | `Vector3` | Convenience: ignores translation row, returns Vector3 |
| `Matrix4 * Direction3` | `Direction3` | Convenience: same |
| `Matrix4 * Normal3` | **illegal** | Same reason |
| `NormalMatrix<T,3> * Normal3` | `Normal3` | The whole point of the type |
| `NormalMatrix<T,3> * Vector3` | **illegal** | Mathematically defined; not the intent |
| `NormalMatrix<T,3> * Direction3` | **illegal** | Same |
| `NormalMatrix<T,3> * Point3` | **illegal** | Same |
| `Matrix4 * Matrix4` | `Matrix4` | Existing |
| `Matrix3 * Matrix3` | `Matrix3` | Existing |
| `NormalMatrix<T,3> * NormalMatrix<T,3>` | `NormalMatrix<T,3>` | (M₁·M₂)⁻ᵀ = M₂⁻ᵀ · M₁⁻ᵀ |
| `Matrix4.linearPart()` | `Matrix3` | Extract 3×3 sub-block |
| `Matrix4.normalMatrix()` | `NormalMatrix<T,3>` | IT of linear sub-block |
| `Matrix3.normalMatrix()` | `NormalMatrix<T,3>` | Same, no Matrix4 needed |
| `Matrix4.inverse()` | `Matrix4` | Stays general |
| `Matrix3.inverse()` | `Matrix3` | Stays general |
| `Matrix3.transpose()` | `Matrix3` | Stays general |
| `NormalMatrix<T,3>.asMatrix3()` | `Matrix3` | Explicit escape hatch; rarely useful |

The 2D table is the same shape with `N=2`, dimensions decremented by
one (Matrix3 in place of Matrix4 for affine, Matrix2 in place of
Matrix3 for linear). Lands in the 2D follow-up phase.

### The `Transform<T, N>` ergonomic wrapper

For scene-graph nodes that transform a mix of Points, Vectors,
Directions, and Normals through the *same* matrix, holding the
affine matrix, linear sub-block, and `NormalMatrix` separately is
awkward. An optional wrapper type — modeled on PBRT's `Transform` —
bundles them with lazy caching:

```cpp
// Shape for 3D; 2D analog has Matrix3/Matrix2 instead of Matrix4/Matrix3.
template<typename T>
class Transform3 {
  Matrix4<T> m_;
  mutable std::optional<Matrix3<T>>          linear_;
  mutable std::optional<NormalMatrix<T, 3>>  normal_;
public:
  Point3<T>     operator*(Point3<T> p)     const { return m_ * p; }
  Vector3<T>    operator*(Vector3<T> v)    const { return linearPart() * v; }
  Direction3<T> operator*(Direction3<T> d) const { return linearPart() * d; }
  Normal3<T>    operator*(Normal3<T> n)    const { return normalMatrix() * n; }

  const Matrix3<T>&         linearPart()   const; // fills linear_ on first call
  const NormalMatrix<T, 3>& normalMatrix() const; // fills normal_ on first call
};

using Transform3f = Transform3<float>;
using Transform3d = Transform3<double>;
// Transform2 lands in the 2D follow-up.
```

Scene-graph nodes hold a `Transform`, not a bare matrix. First call
to each `operator*` overload fills its cache; subsequent calls are
direct multiplies.

**Trade-off**: a fully-populated `Transform3<T>` is ~3× the storage
of a `Matrix4<T>`. For scene-graph nodes that handle the full
geometric type spectrum, that's exactly what you wanted anyway. For
one-shot matrices in tight loops (a temporary built per-call and
immediately applied to one point), use the raw `Matrix4` / `Matrix3`
/ `NormalMatrix` types directly.

`Transform` is **optional**, not required. The plan introduces the
three matrix types as the substrate; `Transform` is a convenience on
top.

---

## Debug invariant enforcement

CMake option:

```
option(RAYTRACER_STRICT_GEOM_INVARIANTS
       "Assert geometric invariants on Point/Direction/Normal" ${IS_DEBUG})
```

Defaults to `ON` in Debug builds, `OFF` in Release.

When `ON`, the policy object attached to each derived type's `Vec`
base (`HomogeneousPointPolicy`, `UnitLengthPolicy`, `VectorPolicy`)
is invoked from the base's `checkInvariant()` path, which the
constructors and mutating operators call. See
[Implementation machinery — Base templates](#base-templates-vecn-t-tag-policy-and-matr-c-t-tag)
for the policy definitions.

`RT_GEOM_ASSERT` is the macro the policies use; it compiles to
`assert(...)` when the option is on,
and to nothing (or `[[assume(...)]]` for optimizer hints) when off.
The macro path is important — bare `assert()` would not give us the
release-build optimizer-hint variant.

### Tolerances

- Point w-invariant: 1e-5 (looser; floating-point round-trip through
  homogeneous transforms accumulates error)
- Direction/Normal magnitude: |v|² within 1e-4 of 1 (squared, so we
  don't pay a sqrt for the assert)

Tolerances themselves should be `constexpr` constants in `Constants.h`
(see Phase 3.7 of the optimization plan).

### What the assertions catch — and don't

**Catches:** denormalized directions/normals coming out of arithmetic
that the caller forgot to renormalize; mistakenly using a Vector's raw
components to construct a Direction; points with w=0 silently treated as
positions.

**Doesn't catch:** explicit `Vector::toDirection()` calls that
produce a *correctly* unit-length result but where the caller meant
to be a Normal (different transform semantics, same invariant). The
typed-conversion API (`.toDirection()` vs `.toNormal()`) handles
this at the call site; assertions can't.

**Doesn't catch:** semantic misuse hidden behind explicit conversions
(the caller knew they were lying and the type system let them).
Nothing can catch that automatically — code review territory.

---

## Implementation machinery

### Base templates: `Vec<N, T, Tag, Policy>` and `Mat<R, C, T, Tag>`

Two base templates carry the shared machinery: `Vec` for everything
that's a vector at the storage level (Point, Vector, Direction,
Normal), and `Mat` for everything that's a matrix at the storage
level (Matrix2/3/4, NormalMatrix, future AffineMatrix). Names chosen
to read naturally at the `class X : public Vec<...>` boundary; the
phantom-tag mechanism is a parameter on the base, not the base's
name.

```cpp
namespace core::math::detail {
  // Vec: N-tuple storage + element-wise math + per-tag operator dispatch
  // + per-policy invariant enforcement. Houses CRTP plumbing,
  // construction, copy construction, debug printing, dot/length/
  // arithmetic that's tag-agnostic, and SIMD specializations keyed
  // on (N, T).
  template<typename T, int N, typename Tag, typename Policy>
  class Vec {
    std::array<T, N> v;
    // Invariant assertion (debug only):
    constexpr void checkInvariant() const { Policy::check(v); }
    // ... arithmetic, dot, length, accessors, debug printing
  };

  // Mat: R×C storage + matrix arithmetic + per-tag operator dispatch.
  // Houses construction, copy, debug printing, determinant/transpose/
  // inverse where they're tag-agnostic. SIMD specializations keyed
  // on (R, C, T).
  template<int R, int C, typename T, typename Tag>
  class Mat {
    std::array<T, R * C> v;
    // ...
  };
}
```

The invariant policies are small free-standing types:

```cpp
namespace core::math::detail {
  struct VectorPolicy {
    template<typename T, int N>
    static void check(const std::array<T, N>&) noexcept { /* no-op */ }
  };

  struct HomogeneousPointPolicy {
    // The last component is the homogeneous w; assert it ≈ 1.
    template<typename T, int N>
    static void check(const std::array<T, N>& v) noexcept {
      RT_GEOM_ASSERT(approx_equal(v[N - 1], T{1}, geom_tol));
    }
  };

  struct UnitLengthPolicy {
    // Sum of squares over the spatial (non-w) prefix ≈ 1.
    // For Vec<T, N, *, UnitLengthPolicy>, all N components are spatial
    // (Direction/Normal don't carry w; they're directional).
    template<typename T, int N>
    static void check(const std::array<T, N>& v) noexcept {
      T sumSq = T{0};
      for (int i = 0; i < N; ++i) sumSq += v[i] * v[i];
      RT_GEOM_ASSERT(approx_equal(sumSq, T{1}, geom_tol_squared));
    }
  };
}
```

The concrete types pick `(N_storage, Tag, Policy)` to express their
identity. **`Point<N, T>` derives from `Vec<N+1, T, ...>`** — its
storage dimension is one larger than its logical dimension, holding
the homogeneous `w` as the last component. Vector / Direction /
Normal use `N` for both:

```cpp
namespace core::math {
  struct PointTag {};
  struct VectorTag {};
  struct DirectionTag {};
  struct NormalTag {};

  template<typename T, int N>
  class Point : public detail::Vec<T, N + 1, PointTag, detail::HomogeneousPointPolicy> {
    // logical dimension N; storage dimension N+1 (the +1 is w).
    constexpr T w() const { return this->v[N]; }
  };

  template<typename T, int N>
  class Vector : public detail::Vec<T, N, VectorTag, detail::VectorPolicy> {};

  template<typename T, int N>
  class Direction : public detail::Vec<T, N, DirectionTag, detail::UnitLengthPolicy> {};

  template<typename T, int N>
  class Normal : public detail::Vec<T, N, NormalTag, detail::UnitLengthPolicy> {};

  // Dimension typedefs.
  template<typename T> using Point2 = Point<T, 2>;
  template<typename T> using Point3 = Point<T, 3>;
  template<typename T> using Vector2 = Vector<T, 2>;
  template<typename T> using Vector3 = Vector<T, 3>;
  // ... etc

  // Element-type typedefs.
  using Point2f = Point2<float>;
  using Point2d = Point2<double>;
  using Point3f = Point3<float>;
  using Point3d = Point3<double>;
  // ... 32 typedefs total for 8 types × 4 elementary types
}
```

**Three things this factoring buys**:

1. **`Vec` has no Point-specific specialization.** The "Point carries
   `w`" detail is captured in the *choice of base* (`Vec<N+1, ...>`),
   not in a storage specialization inside `Vec`. The base template
   is uniform across all four geometric types.
2. **Invariant policies are composable and explicit.** Direction and
   Normal share `UnitLengthPolicy` cleanly. Adding a new geometric
   type (say a future `Position2D` that requires positive components)
   means writing a one-method policy struct, not modifying `Vec`.
3. **Phase 6 spike starts further along.** When we benchmark
   full-policy-objects vs phantom-tags (Phase 6), invariant policies
   are already in place; the spike's remaining work is just porting
   *operator behavior* into policies. Half the migration is free.

**Concrete classes, not typedefs of `Vec`** — each derived class is
its own type, has its own constructors and member functions, shows up
distinctly in error messages and debuggers. The `Vec` base supplies
the shared machinery; the derived classes add the type identity and
the per-type interface (e.g., `Point::w()`, `Vector::toDirection()`).

### Operator overloads via phantom-tag `if constexpr`

The legal-op table is enforced in free-function operators that
inspect the tags via `if constexpr`. Because `Point<N>` and
`Vector<N>` have different storage dimensions (`Vec<N+1>` vs
`Vec<N>`), operators between them perform an explicit storage-shape
adjustment at the boundary:

```cpp
// Vector + Vector -> Vector
template<typename T, int N>
Vector<T, N> operator+(Vec<T, N, VectorTag, /*...*/> a,
                       Vec<T, N, VectorTag, /*...*/> b);

// Point - Point -> Vector (drops the w=0 result component)
template<typename T, int N>
Vector<T, N> operator-(Vec<T, N + 1, PointTag, /*...*/> a,
                       Vec<T, N + 1, PointTag, /*...*/> b);

// Point + Point -> illegal
template<typename T, int N>
void operator+(Vec<T, N + 1, PointTag, /*...*/>,
               Vec<T, N + 1, PointTag, /*...*/>) = delete;

// ... ~80 cases total for 4 types × ~20 ops
```

The `if constexpr` ladders from the earlier draft are replaced by
direct overload resolution on tag + storage-dimension combinations.
That's actually a small improvement over the ladders — most cases
resolve via simple overload matching; only ambiguous tag pairs
need explicit `if constexpr`.

Verbose but **centralized** — the whole interaction surface is
readable in one file.

### Why phantom tags now, policy objects later (maybe)

Policy-object design — each semantic type supplies a policy class
that defines its behavior, the main template delegates — is the
alternative considered. Cleaner per type, more modular, but more
boilerplate per policy and tricky cross-type result-type traits.

**The decision is benchmark-gated.** We ship phantom tags in Phase 1.
A planned spike (Phase N+1, after the type split has settled) ports
the same surface to policy objects on a throwaway branch and
benchmarks the two head-to-head on the macro render benchmark and
the targeted vector/matrix microbenchmarks from
`core-math-optimization.md`. If policy objects are equal-or-faster
*and* the stylistic gains justify the migration cost, we refactor; if
they regress performance, we stay with phantom tags. The public
surface (concrete classes, typedefs, operator overloads) doesn't
change either way — refactor risk is bounded to the internals.

Tracked as a follow-up issue once Phase 1 ships.

### Interaction with SSE3 specializations

The current `Vector3<double>` SSE3 specialization (resolved in Phase
2.3 of the optimization plan — the agent picked "delete") is going
away. The new SIMD path for `Vec<T, N>` storage lives on `Vec`
specializations keyed on `(T, N)`, and benefits all four geometric
types uniformly. Concretely: `Vec<double, 4, *, *>` (which is
`Point3<double>`'s storage) and `Vec<double, 3, *, *>` (which is
`Vector3<double>` / `Direction3<double>` / `Normal3<double>`'s
storage) each get one SIMD specialization; the tag and policy are
irrelevant for the bit-level math.

Implication: this plan is downstream of `core-math-optimization.md`
Phase 2.3. Don't start implementation until that phase has shipped.

---

## Migration plan

API churn through the entire renderer codebase. Phasing matters.

### Phase 0 — design lock

Resolve the remaining open questions above. Commit decisions to this
doc. **No code changes yet.** (Most of Phase 0 was completed in the
2026-05-11 design discussion that produced the current draft.)

### Phase 1 — introduce the 3D types alongside `Vector3<T>` / `Matrix4<T>`

Add the `Vec<T, N, Tag, Policy>` / `Mat<R, C, T, Tag>` template
machinery and the four `Point3<T>` / `Vector3<T>` / `Direction3<T>` /
`Normal3<T>` concrete
classes to `include/core/math/`. Add `NormalMatrix<T, 3>`. Add (or
not, per open question 5) `Transform3<T>`. The 2D specializations of
the template machinery exist from day one but no public 2D typedefs
are exposed yet — keeps the new types' surface narrow.

Implement arithmetic, the per-type matrix transforms, the
`.toDirection()` / `.toNormal()` conversions, and debug invariants.
Existing `Vector3<T>` (the current free-form type) and `Matrix4<T>`
remain in place and untouched, except `Matrix4` gains the new
per-type `operator*` overloads and the `.linearPart()` /
`.normalMatrix()` factories. `Matrix3<T>` similarly gains its own
`.normalMatrix()`. Unit tests for the new types only; no existing
call sites migrated yet.

### Phase 2 — migrate intersection code

`include/raytracer/primitives/` and friends. Ray now carries a
`Point3` origin and a `Direction3` direction. Hit records carry a
`Point3` position and a `Normal3` normal. Densest concentration of
vector use in the codebase; highest-value migration target.

### Phase 3 — migrate materials and shading

`include/raytracer/materials/`. Reflect, refract, cosine terms all
become `Direction3` / `Normal3`-typed. Phase 3.4 of the core-math
optimization plan (missing Vector ops: reflect, refract, lerp, …)
should land *after* this so those ops are written against the right
types from day one.

### Phase 4 — migrate cameras and rasterizer

The remaining 3D-heavy subsystems.

### Phase 5 — audit and cleanup

`grep` for remaining `Vector3<T>` in geometric contexts; convert
where appropriate. Some uses are genuinely free-form (e.g. RGB
color stored as Vector3 — flagged as a separate future sweep, see
[What this is not](#what-this-is-not)).

### Phase 6 — policy-object spike (optional)

Throwaway branch ports the same surface to policy objects;
benchmark head-to-head against phantom tags on the macro render +
microbenchmarks. Refactor only if policies are equal-or-faster and
the modularity gains justify the cost. See
[Implementation machinery — Why phantom tags now, policy objects
later (maybe)](#why-phantom-tags-now-policy-objects-later-maybe).

### Phase 7 — 2D follow-up

Public typedefs (`Point2f`, `Vector2d`, etc.) for the 2D types that
already exist as `Vec<T, 2-or-3, *, *>` instantiations from Phase 1
(Point2 uses storage 3 = 2+1; Vector2 / Direction2 / Normal2 use
storage 2).
`Matrix2<T>` added as a new sibling. `NormalMatrix<T, 2>` if not
already in place from Phase 1's generic machinery. Migration of any
2D call sites (UI overlays, image-processing helpers, future 2D
graphics work) onto the new types.

### Phase 8 — `AffineMatrix<T, N>` optional layer (deferred but intended)

Add `AffineMatrix<T, N>` as an additive layer alongside the existing
size-named `Matrix<N>` types. **Not part of v1, but the intended end
state.** Deferred for v1 scope discipline, not because the value is
in doubt — Thomas confirmed 2026-05-11 that `Matrix4 * Point3` reads
counterintuitively in code (the 4D framing isn't natural for 3D
geometry) and the eventual goal is to land `AffineMatrix3 * Point3`
as the canonical 3D affine transform call site.

What `AffineMatrix<T, N>` buys:

- **Type-level guarantee** that affine ∘ affine = affine. The
  compiler enforces composition correctness for scene-graph
  transforms, view matrices, and other affine operations.
- **Storage savings.** `AffineMatrix3<T>` stores 12 floats (3×4,
  bottom row implied `(0,0,0,1)`) vs `Matrix4<T>`'s 16.
  `AffineMatrix2<T>` stores 6 vs `Matrix3<T>`'s 9. Real memory
  reduction for scene-graph nodes holding thousands of transforms.
- **Cleaner inverse semantics.** Inverse of affine is affine:
  `AffineMatrix::inverse() → AffineMatrix`. The general
  `Matrix3::inverse() → Matrix3` doesn't carry that property.
- **API can drop nonsensical ops.** No `operator+` on
  `AffineMatrix` (the sum of two affine matrices isn't affine).
  Smaller, more correct surface.
- **Self-documenting at call sites.** A scene-graph node typed as
  `AffineMatrix3` is unambiguous; `Matrix4` is just "some 4×4
  matrix that happens to be affine."
- **Symmetry with `NormalMatrix`**. Both types encode a specific
  algebraic structure that the type tracks. Consistent design.

Why deferred:

- **Interacts with "operand decides".** v1's design says `Matrix3
  * Point2` (affine-2D) and `Matrix3 * Vector3` (linear-3D) are
  both legal and the operand's stored layout disambiguates. With
  `AffineMatrix2` in play, the cleaner story is that only
  `AffineMatrix2 * Point2` performs the affine-2D multiply; bare
  `Matrix3 * Point2` becomes illegal. We're partially undoing the
  operand-decides ergonomic. That's defensible but a real
  philosophical shift — better to land the operand-decides design
  first and see whether it produces bugs before reversing course.
- **Migration cost.** Adopting `AffineMatrix` means migrating
  scene-graph nodes, camera classes, and anything else holding
  `Matrix4` to the new type. That's a separate sweep on top of the
  PVN sweep — not v1 work.
- **The wins are real but the absence of the type isn't urgent.**
  No bugs land specifically because `Matrix4` is over-general for
  affine use. Storage savings exist but aren't on a hot path today.
  Better to ship v1, observe, then choose deliberately.

Implementation when it does land:

- `AffineMatrix<T, N>` stored as the compressed form (N+1 rows × N
  columns, bottom row implied).
- Factories migrate: `Matrix4::translate / rotate / scale / lookAt`
  → `AffineMatrix3<T>::translate / ...`. `Matrix4::perspective /
  frustum` stay on `Matrix4` (perspective isn't affine).
- `AffineMatrix3<T>::asMatrix4()` and `Matrix4<T>::asAffine()`
  (with a runtime check on the bottom row) for crossover.
- `Transform<T, N>` wrapper updated to hold `AffineMatrix<T, N>`
  instead of `Matrix<T, N+1>`. Lazy `linearPart()` / `normalMatrix()`
  caches unchanged in shape.
- `Matrix3` and `Matrix4` continue to exist for non-affine 3×3 /
  4×4 uses (perspective projection, arbitrary linear transforms).

Filed as a separate issue once v1 lands and we can evaluate based on
real call-site evidence.

Each phase is its own PR; each phase passes the test suite end-to-end
before the next starts.

---

## Risks

- **API churn is enormous.** Every existing `Vector3<T>` call site in
  geometric code needs a semantic decision. Mitigation: stage by
  subsystem (Phase 2-4 above); do not try a big-bang rename.
- **Conversion ergonomics.** If `Vector → Direction` conversion is
  painful, callers will fight it (e.g. cast to `Vector3` to avoid
  the conversion, defeating the type safety). Mitigation: keep the
  `.toDirection()` / `.toNormal()` API tight and well-documented;
  provide free functions for common patterns
  (`directionFromTo(Point, Point)`, `normalAt(...)`).
- **Debug-build performance.** Unit-length assertions on every
  Direction/Normal mutation are not free. A scene with millions of
  ray–hit pairs sees millions of `dot(d,d) - 1 < ε` checks. Mitigation:
  the option defaults off in Release; do not let the assertion
  machinery sneak into a hot path via a missed macro.
- **Interactions with serialization.** Loading a `Vector3` from a
  scene file — which type is it? The scene-format parser has to make
  the decision. Mitigation: scene-format schema declares the type per
  field; loader returns the right type.
- **Operator overload `if constexpr` ladders get unwieldy.** With
  4 types × ~20 ops = ~80 cases encoded in `if constexpr` chains, the
  free-function operator definitions can become hard to read.
  Mitigation: keep each operator's ladder in its own translation unit
  (or at least its own header section), grouped by op (one ladder for
  `operator+`, one for `operator-`, etc.). The post-Phase-1
  policy-object spike addresses this directly if needed.
- **The "color as Vector3" issue.** Some code uses `Vector3<float>`
  for RGB. Explicitly out of scope for this plan; flag as a separate
  future sweep.

---

## Resolved design decisions

These were open in the original draft and resolved in the 2026-05-11
design discussion. Captured here so the implementing agent doesn't
re-litigate.

1. **Storage layout / sharing model: phantom tags + invariant
   policies on `Vec<T, N, Tag, Policy>` and `Mat<R, C, T, Tag>`
   bases.** See [Implementation machinery](#implementation-machinery).
   Concrete classes inherit from the base; operator overloads use
   tag-keyed overload resolution + `if constexpr` for ambiguous
   pairs; invariants are enforced via small policy structs called
   in debug-only `checkInvariant()` paths. Point's homogeneous `w`
   is captured by `Point<N, T>` deriving from `Vec<N+1, T, ...>`,
   not by a storage specialization. Chosen for performance and for
   keeping the operator-dispatch path on direct overloads.
   Operator-behavior policies remain deferred to the Phase 6 spike.
2. **Concrete classes, not typedefs.** Each semantic type (`Point3`,
   `Vector3`, etc.) is its own `class` declaration inheriting from
   `Vec`. Typedefs are reserved for element-type aliases
   (`Point3f = Point3<float>`, etc.).
3. **Element-type typedefs in glm/Eigen style.**
   `Point3f` / `Point3d`, etc.
4. **Dimension as a template parameter.** `Vector<T, N>` with
   `N ∈ {2, 3}`. The public surface ships 3D only; 2D types drop in
   as a follow-up via the same template machinery.
5. **`Point` stores `w` explicitly.** `Point2` is 3-component
   `(x, y, w)`; `Point3` is 4-component `(x, y, z, w)`. This sidesteps
   the matrix naming collision — matrices stay named by size
   (`Matrix2`, `Matrix3`, `Matrix4`), the operand's storage
   disambiguates the geometric semantic. See
   [Why explicit `w`](#why-explicit-w-on-point).
6. **Inverse-transpose lives in `NormalMatrix<T, N>`.** Constructed
   via `Matrix4::normalMatrix()` / `Matrix3::normalMatrix()`. Distinct
   type, not implicitly convertible back to `Matrix<N>`.
7. **Naming for typed conversions: `.toDirection()` / `.toNormal()`.**
   Not `.normalized()`. The named-target form forces the caller to
   acknowledge which transform semantics will apply at the call site.
   Vector has *no* bare `.normalized()` — unit-length-without-semantic
   is a code smell.
8. **No `UnitVector<T>` separate from `Direction<T>`.** Direction is
   the only unit-length type for geometric use; non-geometric uses
   are rare enough to handle ad-hoc.
9. **No `AffineMatrix<T, N>` in v1.** Matrices stay named by size
   (`Matrix2`, `Matrix3`, `Matrix4`); the operand's stored layout
   disambiguates the geometric semantic. `AffineMatrix` is a real
   design option but explicitly deferred — see
   [Phase 8](#phase-8--affinematrixt-n-optional-layer-deferred) for
   the rationale and the planned future addition.

## Open questions

The remaining items the implementing agent needs guidance on:

1. **When to do the policy-object spike.** After Phase 1 ships and
   call sites stabilize. Ports the same surface to policies on a
   throwaway branch; benchmarks head-to-head. Refactor only if
   policies are equal-or-faster *and* the modularity gains justify
   the migration. File as a separate issue once Phase 1 lands.
2. **Backward-compatibility shim for the existing `Vector3<T>`.**
   Retain as an alias for `Vector<T, 3>` forever, or deprecate +
   eventually remove? Lean retain — many existing uses are
   semantically "Vector," not "ambiguous geometry," and the alias
   keeps churn contained. Final call belongs with the migration PRs.
3. **`float` vs `double` default per type.** Does `Normal` default to
   `Normal3d` (precision for grazing-angle lighting) or `Normal3f`
   (storage)? Codebase uses both today. Pick per-type or per-use-site
   based on what the migration phases find.
4. **Where the `isOrthonormal` tag lives** for the
   Direction-transform fast path. On `Matrix4`, on `Matrix3`, on
   both? Lean both, set by rotation/reflection/identity factories.
   v2 work; v1 always-renormalizes.
5. **Should `Transform<T, N>` ship in Phase 1, or be deferred?** The
   wrapper is optional and orthogonal to the three matrix types. Lean
   ship-in-Phase-1 — scene-graph nodes are the highest-leverage call
   site and they want the wrapper. Counter-argument: shipping it
   later lets us see whether the raw matrix types are ergonomic
   enough without it.

---

## What this is not

A few things this plan deliberately does **not** do, so they don't
creep in during implementation:

- **It does not introduce a Color type.** Color-as-`Vector3<float>`
  is a real problem with separate semantics (gamut, gamma, alpha).
  Out of scope; flag as future work.
- **It does not change the existing `Matrix4` / `Matrix3` factories,
  decompositions, or inversion algorithms.** Those remain on the
  optimization plan's path. What this plan adds is per-type
  `operator*` overloads, a new `NormalMatrix<T, N>` sibling type, the
  `.linearPart()` / `.normalMatrix()` factories, and the future
  `Matrix2<T>` (in the 2D follow-up). The existing matrix bodies stay
  put.
- **It does not introduce projective `Point4` distinctly from
  `Point3`.** `Point3` already stores its homogeneous `w`; perspective
  division is the natural `(x, y, z, w) / w` operation on its storage.
- **It does not unify SoA/batched ray ops** — that's Phase 4 of the
  optimization plan and depends on a stable scalar foundation
  (which this plan provides) but isn't part of this plan's scope.

---

## Working method

1. Resolve open questions above. Update this doc with the decisions.
2. Land `core-math-optimization.md` Phases 1 and 2 first — especially
   Phase 2.3 (Vector3<double> SSE3 resolution), which determines the
   storage foundation.
3. Phase the migration as described above; each phase its own PR.
4. Every PR updates `CHANGELOG.md` under `## Unreleased`.
5. Every PR runs the full test suite end-to-end. The whole-render
   macro benchmark (per `core-math-optimization.md`'s rule) must not
   regress on any phase — this is a *correctness* refactor, not a
   performance one, so the bar is "no regression," not "must improve."
6. Resist scope creep into the Color type and the Vector4 unification
   work; those are separate sweeps.
