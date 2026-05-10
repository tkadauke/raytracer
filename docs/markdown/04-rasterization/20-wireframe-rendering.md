# 20. Wireframe rendering

The two engines so far in Volume IV — the raytracer and the
filled rasterizer — produce *shaded* output: every visible
pixel carries a color derived from the surface material, the
lights, the geometry. This chapter covers a third engine that
deliberately produces nothing of the kind. The
[`engine::wireframe::Wireframe`](../../../include/engine/wireframe/Wireframe.h)
engine draws every mesh face's *edges* as thin lines on an
otherwise-blank framebuffer. No fill, no shading, no depth
test, no Z-buffer.

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
- the **Bresenham line algorithm** that does the per-edge
  rasterization,
- the LOD-driven progression from "obviously polygonal" to
  "saturated silhouette,"
- where wireframe renders earn their keep in the development
  workflow.

## 20.1 What the engine does

[`Wireframe`](../../../include/engine/wireframe/Wireframe.h)'s
`render` routine is structurally simpler than the filled
rasterizer's. Walking the algorithm:

1. **Tessellate.** Call `Primitive::tessellate(lod)` on every
   primitive in the scene; the same path
   [chapter 17](17-tessellation.md) describes.
2. **Project.** For every face vertex, call
   `Camera::projectPoint(...)` to get screen-space coords.
3. **Walk edges.** For each face, iterate the consecutive
   vertex pairs around the polygon (vertex 0 → vertex 1, vertex
   1 → vertex 2, …, vertex N → vertex 0).
4. **Rasterize each edge.** Call `core::drawLine(a, b)` —
   Bresenham's algorithm — to plot pixels along the line from
   the first vertex's projected coordinates to the second.
5. Repeat for every face of every primitive's mesh.

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
- **No clipping** beyond the implicit "if the projected
  coordinates are off-screen, the pixel write is bounds-
  checked." Edges that pass behind the camera produce
  nonsense projection, and the engine relies on the camera's
  `projectPoint` returning an "undefined" sentinel for
  behind-camera vertices.

Each absence is deliberate. The engine is *minimum-viable*: it
demonstrates the tessellate-and-project pipeline without any of
the additional state machinery the filled rasterizer needs. A
reader walking the source for the first time can absorb the
whole engine in one sitting.

## 20.2 Bresenham's line algorithm

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
   [chapter 17 §17.3](17-tessellation.md#17-3-sphere-lat-long-grid))
   handle naturally.

## 20.3 The LOD progression

The wireframe engine is the easiest place to see the LOD
progression from
[chapter 17 §17.10](17-tessellation.md#17-10-picking-a-level-of-detail).
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

## 20.4 Where wireframe earns its keep

The engine pays its rent in three places.

**Editor previews.** The interactive
`examples/GeneratedRayTracer` lets the user toggle the engine
between Raytracer, Rasterizer, and Wireframe. For "I want to
position my camera and lights, then bake a final render,"
the wireframe gives instant feedback (every edge is one
Bresenham call; no shading, no shadow rays, no recursion)
while the user spins the camera around. Once the framing is
right, switch to the raytracer for the final render.

**Tessellation debugging.** Mistakes in
[chapter 17](17-tessellation.md)'s tessellation code — wrong
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

## 20.5 What this chapter does *not* cover

A few wireframe-related extensions are queued under roadmap
§4.1.b:

- **Hidden-line removal.** Real CAD wireframes hide edges
  occluded by closer geometry. This requires per-edge depth
  testing (a different problem from per-pixel; the canonical
  algorithms are Appel's hidden-line algorithm and the
  hidden-line variants of the painter's algorithm). Not yet
  implemented.
- **Shaded wireframes.** Edges colored by their face's
  shading or by a per-vertex attribute. Useful for displaying
  vertex normals, UV layouts, or per-face material assignments
  as a debug overlay.
- **Anti-aliased lines.** Bresenham draws crisp 1-pixel
  lines; an anti-aliased variant (Wu's algorithm or the
  bilinear-filter form) would produce smoother edges at the
  cost of one floating-point division per pixel. Currently
  out of scope for the simplest-engine target.

All three are reasonable future additions. None ship today;
the simplest-engine framing is what the chapter is teaching,
and adding hidden-line or anti-aliasing pulls in machinery
that distracts from the core "tessellate and Bresenham"
recipe.

## 20.6 Exercises

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
4. The engine relies on `Camera::projectPoint` returning an
   "undefined" sentinel for vertices behind the camera. Find
   that sentinel in the `Camera` interface. What does the
   wireframe engine do with edges where one endpoint projects
   to undefined?

## See also

- Volume index: [Volume IV — Rasterization](README.md)
- Previous:
  [19. Clipping, depth, stencil](19-clipping-depth-stencil.md)
- Next:
  [21. MSAA and attribute interpolation](21-msaa-and-attribute-interpolation.md)
- Tessellation source: [17. Tessellation](17-tessellation.md)
- Camera projection: [6. Cameras](../02-ray-rendering/06-cameras.md)
- Shaded counterpart:
  [18. The rasterization pipeline](18-the-rasterization-pipeline.md)

## Source anchors

<!-- source-anchors -->
- `include/core/geometry/Bresenham.h`
- `include/engine/wireframe/Wireframe.h`
- `test/functional/engine/wireframe/WireframeTest.cpp`
- `test/functional/steps/WireframeSteps.cpp`
<!-- /source-anchors -->
