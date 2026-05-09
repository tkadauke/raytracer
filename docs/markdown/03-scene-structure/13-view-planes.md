# 13. View planes

> **Status:** Stub. Narrative not written yet. Source anchors and
> embed plan are committed below; expect prose to fill in.

## Arc

What a view plane is, sample iteration order (row-major vs tiled
vs interlaced vs shuffled), and why shuffled orders matter for
progressive display.

## Source anchors

<!-- source-anchors -->
- `include/render/viewplanes/ViewPlane.h`
- `include/render/viewplanes/ViewPlaneFactory.h`
- `include/render/viewplanes/TiledViewPlane.h`
- `include/render/viewplanes/RowInterlacedViewPlane.h`
- `include/render/viewplanes/RowShuffledViewPlane.h`
- `include/render/viewplanes/PointInterlacedViewPlane.h`
- `include/render/viewplanes/PointShuffledViewPlane.h`
<!-- /source-anchors -->

## Planned embeds

<!-- widget: viewplane_iteration_order -->

## See also

- Volume index: [Volume III — Scene structure](README.md)
- Previous: [12. Tone mapping](../02-ray-rendering/12-tone-mapping.md)
- Next: [14. Constructive solid geometry](14-csg.md)
- Sampler partner: [10. Sampling and anti-aliasing](../02-ray-rendering/10-sampling-and-anti-aliasing.md)
