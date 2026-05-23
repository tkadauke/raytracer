# Rasterization

The other rendering family the codebase ships. Where Ray rendering
shoots rays at implicit surfaces, Rasterization projects explicit
triangles onto a pixel grid. Same scene; different math; different
performance envelope.

This volume mostly stands alone after Foundations. If you came here to
understand the rasterizer, you can skip Ray rendering and Scene structure with
only one detour: [The Whitted pipeline](../ray-rendering/the-whitted-pipeline.md)
introduces the engine abstraction that both rendering paths share.

## Chapters

- [Tessellation](tessellation.md) — `Primitive::tessellate(int
    lod) → Mesh`. Per-shape strategies: subdivided icosahedron,
    triangle fan, quad strip with seam-duplicate UVs, etc. How
    `Composite` and `Instance` recurse.
- [The rasterization pipeline](the-rasterization-pipeline.md) —
    end-to-end edge-function rasterization (Pineda 1988): vertex
    transform → clip-space culling → rasterize → depth test →
    fragment shade → write framebuffer.
- [Clipping, depth, stencil](clipping-depth-stencil.md) —
    Sutherland-Hodgman in homogeneous clip space; the z-buffer as
    the visibility solver; stencil as the per-pixel mask;
    configurable depth-func, depth-write, stencil-op, cull-mode
    state.
- [Wireframe rendering](wireframe-rendering.md) — Bresenham in
    screen space; the wireframe engine as the simplest rasterizer;
    editor-preview and structural-debugging use cases.
- [MSAA and attribute interpolation](msaa-and-attribute-interpolation.md)
    — single-tile vs N-sample-resolved framebuffer; perspective-
    correct UV interpolation (Heckbert-Moreton 1/z trick); coverage
    sampling vs shading sampling.

## Why a software rasterizer?

Three reasons:

1. **Pedagogy.** A rasterizer that runs on the CPU is *readable*. Real
   GPU pipelines disappear into hardware specs and driver black
   boxes; this one fits in a single source file.
2. **Headless.** When the GL stack is unavailable (CI, remote, no
   GPU), `rendercli --engine raster` still produces images.
3. **Comparison.** Rendering the same scene through the raytracer
   and the rasterizer side-by-side is the fastest way to see what
   each algorithm gets right (and wrong).

## See also

- Previous: [Scene structure](../scene-structure/README.md)
- The ray side: [Ray rendering](../ray-rendering/README.md)
- Next: [Image processing & computer vision](../image-and-vision/README.md)
