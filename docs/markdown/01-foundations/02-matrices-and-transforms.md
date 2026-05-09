# 2. Matrices and transforms

> **Status:** Stub. Narrative not written yet. Source anchors and
> embed plan are committed below; expect prose to fill in.

## Arc

4×4 homogeneous matrices, the difference between a *point* and a
*direction* under transformation, the inverse-transpose for
normals, the composition order convention, and where the codebase
keeps these (`Matrix.h` plus `Transformable` mixins). Quaternions
get a paragraph as the alternative for orientation-only.

## Source anchors

<!-- source-anchors -->
- `include/core/math/Matrix.h`
- `include/core/math/Quaternion.h`
- `include/render/primitives/Instance.h`
<!-- /source-anchors -->

## Planned embeds

<!-- widget: instance_transform_normals -->

## See also

- Volume index: [Volume I — Foundations](README.md)
- Previous: [1. Numbers and vectors](01-numbers-and-vectors.md)
- Next: [3. Rays and geometry](03-rays-and-geometry.md)
- Picked up by: [16. Instances and motion blur](../03-scene-structure/16-instances-and-motion-blur.md)
