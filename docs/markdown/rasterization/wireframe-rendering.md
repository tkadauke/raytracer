# Wireframe rendering

The two engines so far in Rasterization
filled rasterizer — produce *shaded* output: every visible
pixel carries a color derived from the surface material, the
lights, the geometry. This chapter covers a third engine that
deliberately produces nothing of the kind. The
[`engine::wireframe::Wireframe`](../../../include/engine/wireframe/Wireframe.h)
engine draws every mesh face's *edges* as thin lines on an
otherwise-blank framebuffer. No fill, no shading, no depth
test, no [Z-buffer](../appendix/a-glossary.md#z).

It is the simplest rasterization engine the codebase ships,
and it earns its keep in two specific cases:

- as an interactive *preview* in editor scenarios, where its
  per-frame cost is much lower than either shaded engine,
- as a *debug* view of the underlying geometry, where the
  absence of fill and shading lets the reader see exactly
  what the tessellation looks like.

By the end of this chapter you should know:

- what the wireframe engine actually draws, and what it
  doesn't,
- the **[Bresenham](../appendix/a-glossary.md#b) line algorithm** that does the per-edge
  rasterization,
- the [LOD](../appendix/a-glossary.md#l)-driven progression from "obviously polygonal" to
  "saturated silhouette,"
- where wireframe renders earn their keep in the development
  workflow.

## <a id="what-the-engine-does"></a>What the engine does
[`Wireframe`](../../../include/engine/wireframe/Wireframe.h)'s
`render` routine is structurally simpler than the filled
rasterizer's. Walking the algorithm:

1. **Tessellate.** Call `Primitive::tessellate(lod)` on every
   primitive in the scene; the same path
   [Tessellation](tessellation.md) describes.
2. **Clip near-plane crossings.** For every edge, compare both
   endpoints' eye-relative depths against
   `Wireframe::nearClipDepth()` (default `0.1`). If both endpoints
   are closer than that, skip the edge. If exactly one endpoint is
   closer, linearly shorten the world-space edge to the near plane.
3. **Project.** For every surviving edge endpoint, call
   `Camera::projectPoint(...)` to get screen-space coordinates.
4. **Walk edges.** For each face, iterate the consecutive
   vertex pairs around the polygon (vertex 0 → vertex 1, vertex
   1 → vertex 2, …, vertex N → vertex 0).
5. **Rasterize each edge.** Call `core::drawLine(a, b)` —
   Bresenham's algorithm — to plot pixels along the line from
   the first vertex's projected coordinates to the second.
6. Repeat for every face of every primitive's mesh.

That is the whole engine. Notable absences:

- **No depth test.** Edges that should be hidden behind closer
  geometry get drawn anyway, in the order primitives appear in
  the scene. The user sees both the front of a sphere's
  wireframe *and* the back through it. This is sometimes
  called the "see-through" wireframe and is the textbook V1
  behavior.
- **No shading.** Every drawn pixel is the same color (white
  by default). The geometry's material is irrelevant.
- **No fill.** Triangles' interiors are blank framebuffer.
  Only the edges show.
- **No viewport clipping.** Projected coordinates outside the
  framebuffer are still passed to Bresenham; the pixel write is
  bounds-checked. The explicit clipping step is only the near-plane
  edge trim needed before perspective projection.

Each absence is deliberate. The engine is *minimum-viable*: it
demonstrates the tessellate-and-project pipeline without any of
the additional state machinery the filled rasterizer needs. A
reader walking the source for the first time can absorb the
whole engine in one sitting.

## <a id="bresenhams-line-algorithm"></a>Bresenham's line algorithm
A line from screen-space point $(x_0, y_0)$ to screen-space
point $(x_1, y_1)$ has to be approximated by a sequence of
integer pixel coordinates on the screen. The naive approach —
*at every step, advance by $\Delta x = 1$ and $\Delta y =
\text{slope}$, round to the nearest pixel* — works but uses
floating-point arithmetic and produces visible artifacts when
the slope is greater than 1 (the line ends up missing pixels).

**Bresenham's algorithm** (1965) is the textbook integer-only
solution. The math idea: pick the longer axis as the *fast*
axis (one pixel step per iteration), and accumulate an
*error term* that triggers a step on the slow axis exactly
when the slope demands one. The error term is updated entirely
in integers using the classic accumulator $2\,\Delta y -
\Delta x$.

The codebase's implementation in
[`include/core/geometry/Bresenham.h`](../../../include/core/geometry/Bresenham.h)
is `core::drawLine(x0, y0, x1, y1, plot)` — a templated
function that calls a caller-supplied `plot(x, y)` for each
pixel along the line. The wireframe engine's `plot` lambda
just writes the line color into the framebuffer at that
coordinate.

Three properties of the algorithm are worth pinning:

1. **Integer arithmetic in the inner loop.** No divisions, no
   floating-point. On a 1980s CPU this was a measurable
   speedup; on a modern CPU it's still slightly faster, and
   the integer-only output is bit-identical across architectures.
2. **Order-independent output.** `drawLine(a, b, ...)` plots
   the same pixel set as `drawLine(b, a, ...)`, modulo
   iteration order. The wireframe engine relies on this — it
   walks polygon vertices in mesh order, but the edges plot
   the same pixels regardless.
3. **Single-pixel correctness.** If $(x_0, y_0) = (x_1, y_1)$,
   the algorithm plots exactly one pixel. Edge cases like
   degenerate single-point edges (which happen at sphere poles
   in the lat-long tessellation from
   [Tessellation: Sphere: lat-long grid](tessellation.md#sphere-lat-long-grid))
   handle naturally.

## <a id="the-lod-progression"></a>The LOD progression
The wireframe engine is the easiest place to see the LOD
progression from
[Tessellation: Picking a level of detail](tessellation.md#picking-a-level-of-detail).
Increasing the LOD doubles the segment count per dimension on
the parametric primitives, so the visible edge count grows
roughly $4 \times$ per LOD step on a sphere or torus.

The progression goes through four phases:

- **LOD 0** — the silhouette is obviously polygonal. A sphere
  at LOD 0 is a 16-segment lat-long grid; the silhouette is
  visibly hexadecagonal at any reasonable render size.
- **LOD 1–2** — the silhouette is recognizable as the intended
  shape, but the faceting is still visible.
- **LOD 3** — most renders look smooth at typical render
  resolutions.
- **LOD 4 and up** — the wireframe **saturates**. Past a
  certain edge density every visible pixel falls on an edge
  somewhere, and the wireframe degenerates into an opaque
  silhouette. That's the LOD at which switching to a shaded
  engine — for which higher density just means smoother
  shading — becomes the natural next step.

The doc-render gallery in the
[`Wireframe`](../../../include/engine/wireframe/Wireframe.h)
header shows this explicitly: `wireframe_engine_lod_0.png`
through `wireframe_engine_lod_4.png` sweep the same sphere at
five LODs, with the saturation point visible at the high end.

## <a id="where-wireframe-earns-its-keep"></a>Where wireframe earns its keep
The engine pays its rent in three places.

**Editor previews.** The interactive
`src/modeler` lets the user toggle the engine
between Raytracer, Rasterizer, and [Wireframe](../appendix/a-glossary.md#w). For "I want to
position my camera and lights, then bake a final render,"
the wireframe gives instant feedback (every edge is one
Bresenham call; no shading, no shadow rays, no recursion)
while the user spins the camera around. Once the framing is
right, switch to the raytracer for the final render.

**[Tessellation](../appendix/a-glossary.md#t) debugging.** Mistakes in
[Tessellation](tessellation.md)'s tessellation code — wrong
vertex order, missing vertices, off-by-one in the seam
duplication — are immediately visible in a wireframe render.
The shaded engines hide these errors with shading; the
wireframe shows you exactly what triangles the tessellation
produced.

**LOD tuning.** Picking the right LOD for a scene is partly
visual judgment. The wireframe makes the visual judgment
explicit: the polygon density is right when the wireframe
looks "smooth enough" but not yet saturated. Once the
wireframe is approaching saturation, additional LOD bumps
just add work without visible improvement.

## <a id="current-limits"></a>Current limits
Wireframe is intentionally small. Its current limits are:

- **No hidden-line removal.** Real CAD wireframes hide edges
  occluded by closer geometry. This requires per-edge depth
  testing (a different problem from per-pixel; the canonical
  algorithms are Appel's hidden-line algorithm and the
  hidden-line variants of the painter's algorithm).
- **No shaded wireframes.** Edges colored by their face's
  shading or by a per-vertex attribute. Useful for displaying
  vertex normals, [UV](../appendix/a-glossary.md#u) layouts, or per-face material assignments
  as a debug overlay.
- **No anti-aliased lines.** Bresenham draws crisp 1-pixel
  lines; an anti-aliased variant (Wu's algorithm or the
  bilinear-filter form) is a different line-rasterization model.

## <a id="exercises"></a>Exercises
1. Predict the wireframe rendering cost (in pixels written) of
   a sphere at LOD 2, in a 1024×1024 viewport. Then predict the
   filled rasterizer's cost on the same sphere. Where is the
   crossover point as the LOD climbs?
2. Read `core::drawLine`. The implementation handles all eight
   line octants (each combination of $\Delta x \gtrless 0$,
   $\Delta y \gtrless 0$, and $|\Delta x| \gtrless |\Delta y|$).
   Pick one octant and confirm the algorithm produces
   bit-identical output to the canonical Bresenham
   pseudocode.
3. The wireframe engine has no Z-buffer. What rendered artifact
   appears when two solid spheres overlap on screen? When a
   sphere is partially occluded by a plane?
4. Find `Wireframe::nearClipDepth()` and
   `Camera::eyeRelativeDepth(...)`. What happens to an edge when
   both endpoints are closer than the near depth? What happens when
   exactly one endpoint is closer?

## See also

- Volume index: [Rasterization](README.md)
- Previous:
  [Clipping, depth, stencil](clipping-depth-stencil.md)
- Next:
  [MSAA and attribute interpolation](msaa-and-attribute-interpolation.md)
- Tessellation source: [Tessellation](tessellation.md)
- Camera projection: [Cameras](../ray-rendering/cameras.md)
- Shaded counterpart:
  [The rasterization pipeline](the-rasterization-pipeline.md)

## Source anchors

<!-- source-anchors -->
- `include/core/geometry/Bresenham.h`
- `include/engine/wireframe/Wireframe.h`
- `src/engine/wireframe/Wireframe.cpp`
- `test/functional/engine/wireframe/WireframeTest.cpp`
- `test/functional/steps/WireframeSteps.cpp`
- `test/unit/render/WireframeTest.cpp`
<!-- /source-anchors -->
