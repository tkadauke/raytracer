# 15. Spatial acceleration

> **Status:** Stub. Narrative not written yet. Source anchors and
> embed plan are committed below; expect prose to fill in.

## Arc

Why a flat list of primitives is O(N) per ray, the bounding volume
hierarchy as the divide-and-conquer answer, the Surface Area
Heuristic for split selection, the uniform grid via DDA traversal
as the "fixed cost per axis" alternative.

## Source anchors

<!-- source-anchors -->
- `include/render/primitives/BVH.h`
- `include/render/primitives/Grid.h`
- `include/core/math/BoundingBox.h`
- `test/unit/render/primitives/BVHPerformanceTest.cpp`
<!-- /source-anchors -->

## Planned embeds

<!-- widget: bvh_sah_traversal -->
<!-- widget: grid_dda_traversal -->

(Acceleration is invisible at the pixel level; no rendered images
to embed here.)

## See also

- Volume index: [Volume III — Scene structure](README.md)
- Previous: [14. Constructive solid geometry](14-csg.md)
- Next: [16. Instances and motion blur](16-instances-and-motion-blur.md)
- Bounding-box vocabulary: [3. Rays and geometry](../01-foundations/03-rays-and-geometry.md)
