# Scene structure

Cross-cutting abstractions that make the renderer fast and the scene
expressive. None of these chapters are strictly required for "a
renderer that renders" — but every non-trivial scene uses at least
two of them, and Rasterization's rasterizer needs the same vocabulary.

## Chapters

- [View planes](view-planes.md) — the iteration policy over
    pixels: row-major, tiled, interlaced, shuffled. Why the choice
    matters for progressive display.
- [Constructive solid geometry](csg.md) — hit intervals as the
    unifying abstraction; union / intersection / difference;
    Minkowski-sum and convex-hull as the support-mapping family;
    GJK as the algorithm that links them.
- [Spatial acceleration](spatial-acceleration.md) — bounding
    volume hierarchies with the Surface Area Heuristic, uniform
    grids with DDA traversal. Why O(N) per ray is unacceptable past
    a few hundred primitives.
- [Instances and motion blur](instances-and-motion-blur.md) —
    one mesh, N transforms; why normals need the inverse-transpose;
    motion blur as a velocity per instance integrated over shutter
    time.

## When to read this volume

Right after Ray rendering if you're doing the linear top-to-bottom read.

If you're navigating sideways: read [Constructive solid geometry](csg.md) if
you've hit CSG in the codebase, [Spatial acceleration](spatial-acceleration.md)
if you're chasing a perf regression, and
[Instances and motion blur](instances-and-motion-blur.md) if `Instance`
confuses you. Each chapter stands on its own once you have the Foundations
vocabulary.

## See also

- Previous: [Ray rendering](../ray-rendering/README.md)
- Next: [Rasterization](../rasterization/README.md)
