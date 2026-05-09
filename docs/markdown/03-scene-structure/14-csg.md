# 14. Constructive solid geometry

> **Status:** Stub. Narrative not written yet. Source anchors and
> embed plan are committed below; expect prose to fill in.

## Arc

Hit intervals as the unifying abstraction, union / intersection /
difference set operations on intervals, Minkowski-sum and
convex-hull as the support-mapping family, GJK as the algorithm
that links them all. Picks up the `HitPointInterval` vocabulary
from chapter 3 and the support-mapping previews from chapter 7.

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

## Planned embeds

<!-- widget: csg_hit_intervals -->
<!-- widget: support_mapping_gjk -->

Plus CSG doc renders (`difference`, `union`, `intersection`,
`minkowski_sum`, `convex_hull`).

## See also

- Volume index: [Volume III — Scene structure](README.md)
- Previous: [13. View planes](13-view-planes.md)
- Next: [15. Spatial acceleration](15-spatial-acceleration.md)
- Vocabulary from: [3. Rays and geometry](../01-foundations/03-rays-and-geometry.md), [7. Primitives and intersection](../02-ray-rendering/07-primitives-and-intersection.md)
