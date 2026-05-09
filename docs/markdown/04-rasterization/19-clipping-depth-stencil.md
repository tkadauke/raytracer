# 19. Clipping, depth, stencil

> **Status:** Stub. Narrative not written yet. Source anchors and
> embed plan are committed below; expect prose to fill in.

## Arc

Sutherland-Hodgman in homogeneous clip space (and why), the
z-buffer as the visibility solver, stencil as the per-pixel mask /
marker, configurable depth-func / depth-write / stencil-op /
cull-mode state. Tied to the textbook "fixed-function pipeline" the
rasterizer is teaching.

## Source anchors

<!-- source-anchors -->
- `include/render/HomogeneousClipVolume.h`
- `include/engine/raster/Rasterizer.h`
- `src/engine/raster/Rasterizer.cpp`
<!-- /source-anchors -->

## Planned embeds

<!-- widget: rasterizer_depth_stencil_cull -->
<!-- widget: rasterizer_clip_attributes -->

Plus stencil + cull doc renders.

## See also

- Volume index: [Volume IV — Rasterization](README.md)
- Previous: [18. The rasterization pipeline](18-the-rasterization-pipeline.md)
- Next: [20. Wireframe rendering](20-wireframe-rendering.md)
