# Volume IV — Rasterization

The other rendering family the codebase ships. Where Volume II
shoots rays at implicit surfaces, Volume IV projects explicit
triangles onto a pixel grid. Same scene; different math; different
performance envelope.

This volume mostly stands alone after Volume I. If you came here to
understand the rasterizer, you can skip Volumes II and III with
only one detour: [chapter 5](../02-ray-rendering/05-the-whitted-pipeline.md)
introduces the engine abstraction that both rendering paths share.

## Chapters

17. [Tessellation](17-tessellation.md) — `Primitive::tessellate(int
    lod) → Mesh`. Per-shape strategies: subdivided icosahedron,
    triangle fan, quad strip with seam-duplicate UVs, etc. How
    `Composite` and `Instance` recurse.
18. [The rasterization pipeline](18-the-rasterization-pipeline.md) —
    end-to-end edge-function rasterization (Pineda 1988): vertex
    transform → clip-space culling → rasterize → depth test →
    fragment shade → write framebuffer.
19. [Clipping, depth, stencil](19-clipping-depth-stencil.md) —
    Sutherland-Hodgman in homogeneous clip space; the z-buffer as
    the visibility solver; stencil as the per-pixel mask;
    configurable depth-func, depth-write, stencil-op, cull-mode
    state.
20. [Wireframe rendering](20-wireframe-rendering.md) — Bresenham in
    screen space; the wireframe engine as the simplest rasterizer;
    editor-preview and structural-debugging use cases.
21. [MSAA and attribute interpolation](21-msaa-and-attribute-interpolation.md)
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

- Previous: [Volume III — Scene structure](../03-scene-structure/README.md)
- The ray side: [Volume II — Ray rendering](../02-ray-rendering/README.md)
- Next: [Volume V — Image processing & computer vision](../05-image-and-vision/README.md)
