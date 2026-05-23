# Volume VII — Render graph

The render graph layer describes a frame as named passes connected by named
resources. It is the shared vocabulary for inspecting a render before pixels
are produced: which pass writes the color image, which pass reads it, what can
be disabled, and whether the declared graph is internally consistent.

## Chapters

27. [Render plans and resources](27-render-plans-and-resources.md) —
    render intent data, scene selectors, typed graph resources, pass nodes,
    validation, graph exports, and CPU-backed resource storage.

## See also

- Previous: [Volume VI — Tools & I/O](../06-tools-and-io/README.md)
- Next: [Volume VIII — Animation](../08-animation/README.md)
- Rasterizer internals:
  [18. The rasterization pipeline](../04-rasterization/18-the-rasterization-pipeline.md)
- Raytracer internals:
  [5. The Whitted pipeline](../02-ray-rendering/05-the-whitted-pipeline.md)
- [Top-level TOC](../README.md)
- [Appendix](../appendix/a-glossary.md)
