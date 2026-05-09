# 20. Wireframe rendering

> **Status:** Stub. Narrative not written yet. Source anchors and
> embed plan are committed below; expect prose to fill in.

## Arc

A second rasterization engine, this time edge-only: Bresenham in
screen space after the same projection step the filled rasterizer
uses. Frames the wireframe engine as the simplest rasterizer,
useful for editor previews + structural debugging.

## Source anchors

<!-- source-anchors -->
- `include/core/geometry/Bresenham.h`
- `include/engine/wireframe/Wireframe.h`
- `test/functional/engine/wireframe/WireframeTest.cpp`
- `test/functional/steps/WireframeSteps.cpp`
<!-- /source-anchors -->

## Planned embeds

(No widgets specifically for wireframe today; the screen-space
projection details are shared with the filled rasterizer chapter.)

Plus wireframe doc renders.

## See also

- Volume index: [Volume IV — Rasterization](README.md)
- Previous: [19. Clipping, depth, stencil](19-clipping-depth-stencil.md)
- Next: [21. MSAA and attribute interpolation](21-msaa-and-attribute-interpolation.md)
- Tessellation source: [17. Tessellation](17-tessellation.md)
