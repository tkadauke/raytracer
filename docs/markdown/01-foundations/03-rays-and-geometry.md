# 3. Rays and geometry

> **Status:** Stub. Narrative not written yet. Source anchors and
> embed plan are committed below; expect prose to fill in.

## Arc

`Ray` as origin + direction + parametric `at(t)`, why we
parameterize by `t` (the universal currency of ray–object
intersection), `HitPoint` and `HitPointInterval` as the data
flowing back from intersection tests, axis-aligned bounding boxes,
ranges, the `Range<T>` helper for clipping intervals. Lays the
groundwork for [chapter 7](../02-ray-rendering/07-primitives-and-intersection.md).

## Source anchors

<!-- source-anchors -->
- `include/core/math/Ray.h`
- `include/core/math/HitPoint.h`
- `include/core/math/HitPointInterval.h`
- `include/core/math/BoundingBox.h`
- `include/core/math/Range.h`
- `include/core/math/Rect.h`
<!-- /source-anchors -->

## Planned embeds

<!-- widget: ray_at -->
<!-- widget: ray_class -->
<!-- widget: ray_project -->
<!-- widget: bounding_box_class -->
<!-- widget: bounding_box_include -->
<!-- widget: bounding_box_grown_by -->
<!-- widget: bounding_box_moved_by -->
<!-- widget: bounding_box_and -->
<!-- widget: bounding_box_or -->
<!-- widget: hitpoint_class -->

## See also

- Volume index: [Volume I — Foundations](README.md)
- Previous: [2. Matrices and transforms](02-matrices-and-transforms.md)
- Next: [4. Color and buffers](04-color-and-buffers.md)
- Picked up by: [7. Primitives and intersection](../02-ray-rendering/07-primitives-and-intersection.md), [14. CSG](../03-scene-structure/14-csg.md)
