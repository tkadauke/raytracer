# Volume III — Scene structure

Cross-cutting abstractions that make the renderer fast and the scene
expressive. None of these chapters are strictly required for "a
renderer that renders" — but every non-trivial scene uses at least
two of them, and Volume IV's rasterizer needs the same vocabulary.

## Chapters

13. [View planes](13-view-planes.md) — the iteration policy over
    pixels: row-major, tiled, interlaced, shuffled. Why the choice
    matters for progressive display.
14. [Constructive solid geometry](14-csg.md) — hit intervals as the
    unifying abstraction; union / intersection / difference;
    Minkowski-sum and convex-hull as the support-mapping family;
    GJK as the algorithm that links them.
15. [Spatial acceleration](15-spatial-acceleration.md) — bounding
    volume hierarchies with the Surface Area Heuristic, uniform
    grids with DDA traversal. Why O(N) per ray is unacceptable past
    a few hundred primitives.
16. [Instances and motion blur](16-instances-and-motion-blur.md) —
    one mesh, N transforms; why normals need the inverse-transpose;
    motion blur as a velocity per instance integrated over shutter
    time.

## When to read this volume

Right after Volume II if you're doing the linear top-to-bottom read.

If you're navigating sideways: read chapter 14 if you've hit CSG in
the codebase, chapter 15 if you're chasing a perf regression,
chapter 16 if `Instance` confuses you. Each chapter stands on its
own once you have Volume I's vocabulary.

## See also

- Previous: [Volume II — Ray rendering](../02-ray-rendering/README.md)
- Next: [Volume IV — Rasterization](../04-rasterization/README.md)
