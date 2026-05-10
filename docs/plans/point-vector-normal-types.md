# Point / Vector / Direction / Normal type split

> **Scope:** introduce semantic geometric types — `Point`,
> `Vector`, `Direction`, `Normal` — alongside the existing
> `Vector3<T>` / `Vector4<T>` in `include/core/math/`. Each type
> carries different transform semantics and (in debug builds) a
> different runtime invariant. Companion doc to
> `core-math-optimization.md`. Captured 2026-05-10 from the
> conversation that motivated the idea.
>
> **Status:** Living document — design proposal, not yet committed.
> Open questions in the section of the same name need decisions
> before any implementation issues fan out.
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

All parametrized by element type `T` (float / double) like the existing
`Vector3<T>` / `Vector4<T>`. Storage layout discussed in
[Storage and SIMD](#storage-and-simd).

### `Point<T>` — a position in space

- Homogeneous w = 1 (logically; storage may or may not include w).
- Transforms via the full 4×4 affine path (includes translation).
- Invariant (debug): w ≈ 1 within tolerance.
- Conceptually a *coordinate*, not a magnitude.

### `Vector<T>` — a free-form displacement

- Homogeneous w = 0.
- Transforms via the 3×3 linear sub-block (no translation).
- No magnitude invariant; this is the "anything else" bucket.
- The closest type to the existing `Vector3<T>`. Most current uses of
  `Vector3` are semantically Vectors.

### `Direction<T>` — a unit-length displacement

- Homogeneous w = 0.
- Transforms via the 3×3 linear sub-block, **renormalized** if the
  matrix is non-orthonormal (or if the caller opts in to skip the
  renormalize for an orthonormal matrix — see open questions).
- Invariant (debug): |d| ≈ 1.
- Use cases: ray directions, light directions, view directions.

### `Normal<T>` — a unit-length surface normal

- Homogeneous w = 0.
- Transforms via the **inverse-transpose** of the matrix's 3×3
  sub-block. This is the load-bearing differentiator from Direction.
- Invariant (debug): |n| ≈ 1.
- Use cases: shading normals, geometric normals, anything used in a
  cosine-weighted lighting integrand.

Direction and Normal share an invariant (unit length) but differ in
*how they transform*. That difference is the entire reason for the
two-type split rather than one `UnitVector`.

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
| `Vector.normalized()` | `Direction` | Explicit conversion |
| `Vector.normalizedAsNormal()` | `Normal` | Explicit conversion (different name to force semantic choice) |
| `Direction → Vector` | implicit | Direction is-a Vector with a stronger invariant; widening is safe |
| `Normal → Vector` | implicit | Same |
| `Vector → Direction` | **explicit only** | Must call `.normalized()` |
| `Vector → Normal` | **explicit only** | Must call `.normalizedAsNormal()` |
| `Direction ↔ Normal` | **explicit only** | Different transform semantics; never silently |

**Key design choice:** widening (Direction → Vector) is implicit;
narrowing (Vector → Direction) is explicit. The asymmetry mirrors
`const_cast` vs. ordinary copy in C++ — you can always weaken a
promise without saying so; strengthening requires you to assert it
intentionally.

The Direction ↔ Normal conversion being explicit-only is the most
important rule: it forces the caller to acknowledge they're switching
which transform semantics will apply.

---

## Matrix transforms by type

The transform overloads are where the types earn their keep:

```cpp
Matrix4<T>::operator*(Point<T>)      // full affine, w=1 row
Matrix4<T>::operator*(Vector<T>)     // 3×3 sub-block, w=0 row
Matrix4<T>::operator*(Direction<T>)  // 3×3 sub-block + optional renormalize
Matrix4<T>::operator*(Normal<T>)     // inverse-transpose 3×3 sub-block + renormalize
```

For `Matrix4 * Normal`, the matrix's inverse-transpose is a
property of the matrix, not of any individual call. Cache it on the
matrix when first needed (or precompute it eagerly for matrices that
will see many normal transforms — the scene-graph transforms qualify).

For `Matrix4 * Direction`, the renormalize step is unnecessary for
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

## Debug invariant enforcement

CMake option:

```
option(RAYTRACER_STRICT_GEOM_INVARIANTS
       "Assert geometric invariants on Point/Direction/Normal" ${IS_DEBUG})
```

Defaults to `ON` in Debug builds, `OFF` in Release.

When `ON`, the constructors and mutating operators of `Point`,
`Direction`, `Normal` assert their invariants:

```cpp
Point(T x, T y, T z, T w = 1) {
  RT_GEOM_ASSERT(approx_equal(w, 1, geom_tol));
  ...
}

Direction(T x, T y, T z) {
  T len_sq = x*x + y*y + z*z;
  RT_GEOM_ASSERT(approx_equal(len_sq, 1, geom_tol_squared));
  ...
}
```

`RT_GEOM_ASSERT` compiles to `assert(...)` when the option is on,
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

**Doesn't catch:** explicit Vector → Direction conversions via
`.normalized()` that produce a *correctly* unit-length result but that
the caller meant to be a Normal (different transform semantics, same
invariant). Type-system rules handle this; assertions can't.

**Doesn't catch:** semantic misuse hidden behind explicit conversions
(the caller knew they were lying and the type system let them).
Nothing can catch that automatically — code review territory.

---

## Storage and SIMD

Three viable layouts for the underlying data. The choice has
performance implications and interacts with the existing SSE3
specializations in `Vector.h`.

### A. Each type stores its own 3 components

```cpp
template<typename T> class Point  { T x, y, z; };
template<typename T> class Vector { T x, y, z; };
template<typename T> class Direction { T x, y, z; };
template<typename T> class Normal { T x, y, z; };
```

- Pro: smallest storage, simplest to reason about.
- Con: transforms re-add the implicit w (1 for Point, 0 for everyone
  else) on every multiply; SIMD specializations need to be replicated
  across all four types or shared via CRTP base.

### B. Shared `Vec3<T>` base; types are thin wrappers

```cpp
template<typename T> class Vec3 { T x, y, z; /* arithmetic, dot, etc. */ };
template<typename T> class Point     : private Vec3<T> { /* exposes affine ops */ };
template<typename T> class Vector    : public Vec3<T>  { /* re-exposes raw ops */ };
template<typename T> class Direction : private Vec3<T> { /* invariant-guarded */ };
template<typename T> class Normal    : private Vec3<T> { /* invariant + IT transform */ };
```

- Pro: SIMD specializations live on `Vec3<T>`; types share them.
- Pro: minimal code duplication.
- Con: have to be careful that `Point` does NOT implicitly convert to
  `Vec3` or `Vector` — private inheritance plus selectively `using`
  the right operators.

### C. Tagged storage: one underlying class with a phantom type

```cpp
enum class GeomKind { Point, Vector, Direction, Normal };
template<typename T, GeomKind K> class Tagged3 { T x, y, z; };
using Point     = Tagged3<T, GeomKind::Point>;
using Vector    = Tagged3<T, GeomKind::Vector>;
using Direction = Tagged3<T, GeomKind::Direction>;
using Normal    = Tagged3<T, GeomKind::Normal>;
```

- Pro: single template, single SIMD specialization, operators
  constrained via `if constexpr` on the tag.
- Pro: type identity comes for free from the phantom parameter.
- Con: implementation of operator overloads becomes a chain of
  `if constexpr` (legal-op table from above, encoded as predicates).
  More dense but harder to read.

**Recommendation:** start with **B**. CRTP-style shared base is the
idiom most familiar to C++ readers, the SIMD path lives in one place,
and the type-safety story is clear in the public interface. Revisit if
B's verbosity becomes painful.

### Interaction with SSE3 specializations

The current `Vector3<double>` SSE3 specialization (slated for
resolution in Phase 2.3 of the optimization plan) is structurally
broken and partly UB. The type split should land **after** Phase 2.3
resolves the storage question. Whatever Phase 2.3 picks (delete the
specialization, replace with AVX2 `__m256d`, fix-in-place) becomes the
`Vec3<T>` base for the type split.

Implication: this plan is downstream of `core-math-optimization.md`
Phase 2.3. Do not start implementation until that phase has shipped.

---

## Migration plan

API churn through the entire renderer codebase. Phasing matters.

### Phase 0 — design lock

Resolve the open questions below. Pick storage layout (A / B / C).
Decide whether `Direction` is always-renormalized or matrix-tagged.
Commit the design to this doc. **No code changes yet.**

### Phase 1 — introduce the types alongside `Vector3<T>`

Add `Point`, `Vector`, `Direction`, `Normal` to
`include/core/math/`. Implement arithmetic + transforms + debug
invariants. Existing `Vector3<T>` remains in place and untouched.
Unit tests for the new types only; no existing call sites migrated
yet.

### Phase 2 — migrate intersection code

`include/raytracer/primitives/` and friends. Ray now carries a
`Point` origin and a `Direction` direction. Hit records carry a
`Point` position and a `Normal` normal. This is the densest
concentration of vector use in the codebase and the highest-value
migration target.

### Phase 3 — migrate materials and shading

`include/raytracer/materials/`. Reflect, refract, cosine terms all
become `Direction`/`Normal`-typed. Phase 3.4 of the core-math
optimization plan (missing Vector ops: reflect, refract, lerp, …)
should land *after* this so those ops are written against the right
types from day one.

### Phase 4 — migrate cameras and rasterizer

The remaining geometry-heavy subsystems.

### Phase 5 — audit and cleanup

`grep` for remaining `Vector3<T>` in geometric contexts; convert
where appropriate. Some uses are genuinely free-form (e.g. RGB
color stored as Vector3 — this should arguably be a separate type,
but that's a *different* sweep, not this one).

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
  `.normalized()` API tight and well-documented; provide free
  functions for common patterns (`directionFromTo(Point, Point)`,
  `normalAt(...)`).
- **Debug-build performance.** Unit-length assertions on every
  Direction/Normal mutation are not free. A scene with millions of
  ray–hit pairs sees millions of `dot(d,d) - 1 < ε` checks. Mitigation:
  the option defaults off in Release; do not let the assertion
  machinery sneak into a hot path via a missed macro.
- **Interactions with serialization.** Loading a `Vector3` from a
  scene file — which type is it? The scene-format parser has to make
  the decision. Mitigation: scene-format schema declares the type per
  field; loader returns the right type.
- **Operator overload explosion.** Layout B with private inheritance
  needs careful `using` declarations to expose the right subset of
  base ops on each derived type. Mitigation: hide everything by
  default, `using` only what's in the legal-op table.
- **The "color as Vector3" issue.** Some code uses `Vector3<float>`
  for RGB. Explicitly out of scope for this plan; flag as a separate
  future sweep.

---

## Open questions

These need decisions before Phase 1 of the migration plan starts.

1. **Storage layout: A, B, or C?** Recommendation B; not locked.
2. **Direction transform: always renormalize, or matrix-orthonormal
   tag?** Recommendation always-renormalize for v1; tag in v2.
3. **Does `Point` store w explicitly, or imply w=1?** Storage savings
   vs. transform-path simplicity. Probably imply w=1 and add it at
   transform time; revisit if Matrix×Point becomes a hot spot.
4. **Should there be a `UnitVector<T>` alongside `Direction<T>` for
   non-geometric unit vectors?** Probably not — Direction is the
   geometric "unit vector that lives in space"; non-geometric uses
   are rare enough to handle ad-hoc. Defer.
5. **What's the name of the conversion: `.normalized()` returning
   `Direction`, or `.toDirection()`?** Style preference. Lean
   `.normalized()` — matches `.normalize()` (mutating) and is the
   common idiom.
6. **Where does the inverse-transpose live?** Computed on demand vs.
   cached on the matrix. Cached is faster but doubles matrix memory
   for transforms that never see a normal.
7. **Backward compatibility shim?** Should `Vector3<T>` remain a
   public alias forever, or be deprecated and eventually removed?
   Lean toward retain-as-alias; some uses (free-form 3-vectors that
   aren't geometric) are legitimate.
8. **`float` vs `double` defaults per type.** Does `Normal` default
   to `Normal<double>` (precision matters for grazing-angle lighting)
   or `Normal<float>` (storage)? Currently the codebase uses both;
   pick a default per type or per use-site.

---

## What this is not

A few things this plan deliberately does **not** do, so they don't
creep in during implementation:

- **It does not introduce a Color type.** Color-as-`Vector3<float>`
  is a real problem with separate semantics (gamut, gamma, alpha).
  Out of scope; flag as future work.
- **It does not change the `Matrix` interface beyond adding the
  per-type `operator*` overloads.** Matrix factories, decompositions,
  inversion remain on the optimization plan's path.
- **It does not introduce projective `Point4` distinctly from
  `Point3`.** The homogeneous w=1 case is implicit in Point;
  perspective division at the projection boundary stays a free
  function operating on Vector4-like data.
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
