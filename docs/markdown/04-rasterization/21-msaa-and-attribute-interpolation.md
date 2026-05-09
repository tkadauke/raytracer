# 21. MSAA and attribute interpolation

> **Status:** Stub. Narrative not written yet. Source anchors and
> embed plan are committed below; expect prose to fill in.

## Arc

The single-tile vs N-sample-resolved framebuffer split,
perspective-correct UV interpolation (Heckbert-Moreton 1/z trick),
why MSAA samples coverage but not shading.

## Source anchors

<!-- source-anchors -->
- `src/engine/raster/Rasterizer.cpp`
- `include/engine/raster/Rasterizer.h`
- `include/core/geometry/Rasterize.h`
<!-- /source-anchors -->

## Planned embeds

<!-- widget: rasterizer_msaa_coverage -->
<!-- widget: rasterizer_perspective_uv -->

Plus 1×-vs-4× MSAA doc renders.

## See also

- Volume index: [Volume IV — Rasterization](README.md)
- Previous: [20. Wireframe rendering](20-wireframe-rendering.md)
- Next volume: [Volume V — Image processing & computer vision](../05-image-and-vision/README.md)
- Sampler partner: [10. Sampling and anti-aliasing](../02-ray-rendering/10-sampling-and-anti-aliasing.md)
- Texture interpolation: [11. Textures](../02-ray-rendering/11-textures.md)
