# 5. The Whitted pipeline

> **Status:** Stub. Narrative not written yet. Source anchors and
> embed plan are committed below; expect prose to fill in.

## Arc

A complete tracer in 200 words: cast primary rays, find closest
hit, shade with material + lights, recurse for reflections /
refractions, composite to the buffer. Then expand each step over
the chapters that follow. Maps each step to the file that owns it.

This chapter is the road map for the rest of Volume II.

## Source anchors

<!-- source-anchors -->
- `include/engine/raytracer/Raytracer.h`
- `src/engine/raytracer/Raytracer.cpp`
- `include/render/RenderEngine.h`
- `include/render/State.h`
- `include/render/RayCaster.h`
<!-- /source-anchors -->

## Planned embeds

- Architecture-flow SVG (static, *new artifact*) — TBD whether the
  prose actually needs it. Several thumbnail before/after pairs
  pulled from existing `docs/images/` to motivate later chapters.

## See also

- Volume index: [Volume II — Ray rendering](README.md)
- Previous: [4. Color and buffers](../01-foundations/04-color-and-buffers.md)
- Next: [6. Cameras](06-cameras.md)
- The other engine: [18. The rasterization pipeline](../04-rasterization/18-the-rasterization-pipeline.md)
