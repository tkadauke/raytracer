# Foundations

The math and data structures everything else stands on. Read this
volume linearly; later chapters assume the vocabulary it sets up.

## Chapters

- [Numbers and vectors](numbers-and-vectors.md) — `double` vs
   `float`, the `Vector<N, T>` template, SIMD specializations, dot
   and cross products, the unit-length invariant.
- [Matrices and transforms](matrices-and-transforms.md) — 4×4
   homogeneous matrices, point-vs-direction transformation, the
   inverse-transpose for normals, quaternions on the side.
- [Rays and geometry](rays-and-geometry.md) — `Ray`, `HitPoint`,
   `HitPointInterval`, axis-aligned bounding boxes, ranges. The
   parametric `t` value as the universal currency.
- [Color and buffers](color-and-buffers.md) — `Colord` as four
   floats, linear-RGB vs sRGB, HDR vs LDR, `Buffer<T>` as the 2D
   image abstraction, the framebuffer end of the pipeline.

## Why these four, in this order

A renderer is a function from scene → image. The scene is a
collection of geometry expressed in vectors and matrices. The image
is a buffer of colors. The function in the middle, in any rendering
algorithm we ship, ultimately reduces to "shoot a ray, see what it
hits." So Foundations covers, in order:

1. Vectors — the unit of geometry.
2. Matrices — how that geometry moves around.
3. Rays — the probe.
4. Colors and buffers — what comes out.

Once you have those four primitives, every later chapter is about
algorithms over them.

## See also

- [Top-level TOC](../README.md)
- [Preface](../preface.md)
- Next: [Ray rendering](../ray-rendering/README.md)
