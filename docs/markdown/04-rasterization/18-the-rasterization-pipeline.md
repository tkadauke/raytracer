# 18. The rasterization pipeline

> **Status:** Stub. Narrative not written yet. Source anchors and
> embed plan are committed below; expect prose to fill in.

## Arc

End-to-end edge-function rasterization (Pineda 1988): vertex
transform → clip-space culling → rasterize triangle → depth test →
fragment shade → write framebuffer. Each stage maps to its
function in `Rasterizer.cpp`. The fixed-point edge stepping
(`PreparedRasterTriangle`) gets its own subsection.

## Source anchors

<!-- source-anchors -->
- `include/core/geometry/Rasterize.h`
- `include/engine/raster/Rasterizer.h`
- `src/engine/raster/Rasterizer.cpp`
- `include/render/HomogeneousClipVolume.h`
- `include/render/TilePlan.h`
<!-- /source-anchors -->

## Planned embeds

<!-- widget: rasterizer_pipeline -->
<!-- widget: rasterizer_perspective_uv -->
<!-- widget: rasterizer_clip_attributes -->

Plus rasterizer-vs-raytracer doc renders.

## See also

- Volume index: [Volume IV — Rasterization](README.md)
- Previous: [17. Tessellation](17-tessellation.md)
- Next: [19. Clipping, depth, stencil](19-clipping-depth-stencil.md)
- Engine abstraction: [5. The Whitted pipeline](../02-ray-rendering/05-the-whitted-pipeline.md)
