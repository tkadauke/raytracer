# Volume II — Ray rendering

The Whitted pipeline as it actually runs in
[`engine::raytracer::Raytracer`](../../../include/engine/raytracer/Raytracer.h).
The longest volume in the book: every chapter expands one step of
the pipeline introduced in chapter 5.

## Chapters

5. [The Whitted pipeline](05-the-whitted-pipeline.md) — the whole
   tracer in 200 words, then the road map for the rest of the
   volume.
6. [Cameras](06-cameras.md) — pinhole, orthographic, spherical,
   fisheye, equirectangular, tilt-shift, thin-lens. Each one's
   physical model and its place in the codebase.
7. [Primitives and intersection](07-primitives-and-intersection.md) —
   sphere, plane, box, triangle, disk, cylinder, torus. Per-shape
   math; how UVs get filled in.
8. [Materials and BRDFs](08-materials-and-brdfs.md) — Matte, Phong,
   Reflective, Transparent, Portal. The new `BSDF` interface as the
   container abstraction.
9. [Lights and shading](09-lights-and-shading.md) — point lights,
   directional lights, the shadow ray, ambient as cheap GI.
10. [Sampling and anti-aliasing](10-sampling-and-anti-aliasing.md) —
    regular vs jittered vs random, multi-sample-per-pixel as Monte
    Carlo over pixel area, the lens-sampler shared with the
    thin-lens camera.
11. [Textures](11-textures.md) — `Texture` as `(s, t) → Colord`,
    constant / checker / UV-color, planar vs UV-direct mapping.
12. [Tone mapping](12-tone-mapping.md) — why HDR exists, the float
    framebuffer, Linear / Reinhard / ACES as a sweep across
    compression strengths.

## Where the pipeline runs

```
include/engine/raytracer/Raytracer.h    ← interface
src/engine/raytracer/Raytracer.cpp      ← implementation
```

That class threads through everything in this volume. Camera is its
shared pointer. The scene is what `render(buffer)` walks. Materials
get called from `rayColor()`. Tonemap runs at the end, on the float
framebuffer, before the LDR conversion that hits the
`Buffer<unsigned int>` you actually display.

## See also

- Previous: [Volume I — Foundations](../01-foundations/README.md)
- Next: [Volume III — Scene structure](../03-scene-structure/README.md)
- The other engine: [Volume IV — Rasterization](../04-rasterization/README.md)
