# 16. Instances and motion blur

> **Status:** Stub. Narrative not written yet. Source anchors and
> embed plan are committed below; expect prose to fill in.

## Arc

How `Instance` lets one mesh appear with N transforms (the classic
wins: trees, asteroid fields), why normals need the
inverse-transpose, motion blur as a velocity per instance integrated
over shutter time.

## Source anchors

<!-- source-anchors -->
- `include/render/primitives/Instance.h`
- `include/core/math/Matrix.h`
<!-- /source-anchors -->

## Planned embeds

<!-- widget: instance_transform_normals -->
<!-- widget: motion_blur_time_sampling -->

Plus motion-blur doc renders.

## See also

- Volume index: [Volume III — Scene structure](README.md)
- Previous: [15. Spatial acceleration](15-spatial-acceleration.md)
- Next volume: [Volume IV — Rasterization](../04-rasterization/README.md)
- Transform math: [2. Matrices and transforms](../01-foundations/02-matrices-and-transforms.md)
