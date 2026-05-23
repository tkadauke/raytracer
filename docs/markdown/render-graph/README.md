# Render graph

The render graph layer describes a frame as named passes connected by named
resources. It is the shared vocabulary for inspecting a render before pixels
are produced: which pass writes the color image, which pass reads it, what can
be disabled, and whether the declared graph is internally consistent.

## Chapters

- [Render plans and resources](render-plans-and-resources.md) —
    render intent data, scene selectors, typed graph resources, pass nodes,
    validation, graph exports, and CPU-backed resource storage.

## See also

- Previous: [Tools & I/O](../tools-and-io/README.md)
- Next: [Animation](../animation/README.md)
- Rasterizer internals:
  [The rasterization pipeline](../rasterization/the-rasterization-pipeline.md)
- Raytracer internals:
  [The Whitted pipeline](../ray-rendering/the-whitted-pipeline.md)
- [Top-level TOC](../README.md)
- [Appendix](../appendix/a-glossary.md)
