# 1. Numbers and vectors

> **Status:** Stub. Narrative not written yet. Source anchors and
> embed plan are committed below; expect prose to fill in.

## Arc

Why we use `double` (and when `float` is fine), `Vector<N, T>` as a
uniform abstraction, the SSE3 `Vector3<double>` / `Vector4<double>`
specializations, basic operators, dot and cross products, length
and normalization. Closes with the *unit-length invariant* — most
ray code assumes normalized directions.

## Source anchors

<!-- source-anchors -->
- `include/core/math/Vector.h`
- `include/core/math/vector/sse3/`
- `include/core/math/Number.h`
- `include/core/math/Constants.h`
<!-- /source-anchors -->

## Planned embeds

- *Optional new widget:* a tiny dot-product widget showing cosine
  for two draggable arrows. Skipping unless prose calls for it.

## See also

- Volume index: [Volume I — Foundations](README.md)
- Next chapter: [2. Matrices and transforms](02-matrices-and-transforms.md)
- Used in: [3. Rays and geometry](03-rays-and-geometry.md), every
  chapter in [Volume II](../02-ray-rendering/README.md).
