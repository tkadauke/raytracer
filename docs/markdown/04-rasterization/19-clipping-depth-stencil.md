# 19. Clipping, depth, stencil

The rasterizer pipeline from
[chapter 18](18-the-rasterization-pipeline.md) glossed over
three steps that deserve their own chapter: clipping triangles
that straddle the visible region, deciding which fragment wins
when two triangles project to overlapping pixels, and the
optional **stencil** mask that lets the renderer mark and
filter pixels through additional per-pixel state. These three
pieces together are what graphics literature calls the
"fixed-function pipeline" — the family of configurable per-pixel
tests that GPU pipelines exposed before programmable shaders
took over.

By the end of this chapter you should know:

- the **[Sutherland-Hodgman](../appendix/a-glossary.md#s)** polygon-clipping algorithm, and
  why the rasterizer runs it in homogeneous clip space rather
  than in screen space,
- the depth test and the configurable depth functions,
- the stencil buffer as a per-pixel marker mechanism, and the
  three-action stencil op state machine,
- the face-culling state and how the rasterizer uses it.

## 19.1 Why clip at all

The rasterizer projects every triangle's vertices to clip space
in step 1 of the pipeline. After the projection, a triangle
might be:

- **Wholly inside** the visible region. Project, divide, and
  rasterize without trimming.
- **Wholly outside** the visible region (every vertex on the
  same side of one clip plane). Skip the triangle entirely; no
  pixels can be inside.
- **Straddling a clip plane.** Some vertices inside, some
  outside. The straddling case is what clipping handles.

The straddling case has two practical problems. First, a vertex
*behind* the camera projects through the perspective divide to
nonsensical screen coordinates — the divide by $w$ when $w$ is
zero or negative blows up to infinity or wraps to a wrong
quadrant. Second, a vertex *outside the viewport* but in front
of the camera projects to enormous (but finite) coordinates,
which stretch the rasterizer's bounding-box scan over millions
of pixels that all happen to be off-screen.

Clipping replaces the offending vertex with the *intersection*
of the triangle's edges with the clip plane, producing a
smaller polygon that is wholly inside the visible region.

## 19.2 Sutherland-Hodgman in homogeneous clip space

The classical algorithm is **Sutherland-Hodgman polygon
clipping** (1974). It clips a polygon against one half-space at
a time: walk the polygon's edges, and for each edge, emit zero,
one, or two output vertices depending on whether the edge's
endpoints are inside or outside the clip plane:

| Start | End | Emit |
|---|---|---|
| Inside | Inside | End |
| Inside | Outside | Intersection |
| Outside | Inside | Intersection, then End |
| Outside | Outside | Nothing |

Run this against six clip planes (near, far, left, right, top,
bottom) and the result is the polygon's intersection with the
viewing frustum. The output may have more vertices than the
input — a triangle clipped against the corner of the viewport
can produce a pentagon — but it's always a single convex
polygon, and the rasterizer fan-triangulates it back into
triangles.

The crucial design choice is **what space to clip in**. Two
options exist:

1. **Screen space.** Project every vertex through the
   perspective divide first, then clip in 2D pixel space
   against the rectangular viewport. Simple math; broken when
   any vertex is behind the camera.
2. **Homogeneous clip space.** Clip in 4D homogeneous
   coordinates *before* the perspective divide. The clip planes
   are linear in $(x, y, z, w)$; the perspective divide
   happens after clipping, when every surviving vertex has
   $w > 0$.

The codebase uses option 2. The clip planes live in
[`include/render/HomogeneousClipVolume.h`](../../../include/render/HomogeneousClipVolume.h)
as the `HomogeneousClipPlane` family — `Near`, `Left`,
`Right`, `Top`, `Bottom` — each defined by a linear inequality
over $(x, y, z, w)$. The clipping routine walks the polygon's
edges and tests each vertex's signed distance to the plane:
positive means inside, negative means outside. When the sign
changes between consecutive vertices, the routine computes the
intersection point by linearly interpolating along the edge.

The widget shows the clipping with attributes interpolated
along the way:

<!-- widget: rasterizer_clip_attributes -->

The interesting detail in the widget is that **all per-vertex
attributes** — UVs, normals, colors — get interpolated along
with the position when a clip generates an intersection
vertex. The intersection's [UV](../appendix/a-glossary.md#u) is the linear blend of the
edge's two endpoint UVs, with the same blend weight that
produced the position. The same for normals (re-normalized
after the blend, since linear-blended normals don't preserve
unit length).

The codebase precomputes, per vertex, a bitmask of which
clip planes the vertex is *outside*. A triangle whose three
vertices share a single "outside" bit means every vertex is
outside the same clip plane — the triangle gets dropped
entirely without running the per-edge clipping algorithm. A
triangle whose vertices' "outside" bits ANDed together is zero
— meaning some vertices inside, some outside, but no shared
"outside" — falls into the slow Sutherland-Hodgman path.
Triangles wholly inside (zero outside-bits everywhere) skip
the clipping algorithm and rasterize directly.

## 19.3 The depth test

When two triangles project to overlapping screen-space pixels,
the renderer needs to pick which one's color the pixel shows.
The standard mechanism is the **[Z-buffer](../appendix/a-glossary.md#z)**: a buffer the same
size as the framebuffer, holding one depth value per pixel.

Initially, every pixel of the Z-buffer is set to the
"farthest possible" value (positive infinity in this codebase).
When a fragment arrives at a pixel:

1. Compute the fragment's perspective-correct depth from
   [chapter 18 §18.4](18-the-rasterization-pipeline.md#18-4-the-depth-test).
2. Compare against the Z-buffer's current value at that pixel
   using the configured depth function.
3. If the comparison passes, the fragment writes both its
   color *and* its depth value (when depth writes are
   enabled).
4. If the comparison fails, the fragment is discarded; the
   pixel keeps whatever color and depth were there before.

The configurable depth function is one of:

| `DepthFunc` value | Pass when |
|---|---|
| `Always` | always (effectively disables the test) |
| `Never` | never |
| `Less` | fragment depth $<$ buffer depth |
| `Lequal` | fragment depth $\leq$ buffer depth |
| `Greater` | fragment depth $>$ buffer depth |
| `Gequal` | fragment depth $\geq$ buffer depth |
| `Equal` | fragment depth $=$ buffer depth |
| `NotEqual` | fragment depth $\neq$ buffer depth |

The default is `Less` with depth writes enabled — the textbook
"closest fragment wins" Z-buffer behavior. Other settings are
useful in specific cases:

- `Lequal` with depth writes off, for a second pass that
  reads the depth values from the first pass without
  modifying them (e.g. a "select fragments at the same depth
  as before" operation).
- `Greater` for back-face renders that want to write color
  only where the depth is *behind* the previous render — used
  by some shadow-volume techniques.
- `Always` with depth writes on, when the per-pixel order is
  guaranteed by other means (back-to-front sorting in
  software).

The widget covers these states alongside the cull mode:

<!-- widget: rasterizer_depth_stencil_cull -->

## 19.4 The stencil buffer

The Z-buffer answers *which fragment is closest at this pixel*.
A second per-pixel buffer, the **stencil buffer**, answers a
related question: *given some application-defined criterion,
should this pixel participate in the current draw at all?*

The stencil buffer holds one 8-bit value per pixel.
Applications use it for tasks the depth buffer can't express:

- **Mirror/reflection compositing**: stencil out the mirror's
  silhouette, render the reflected scene only inside that
  stencil region.
- **Decal rendering**: paint a decal mesh onto a primary
  surface only where the primary surface itself was drawn.
- **Stencil-shadow volumes**: count how many shadow boundaries
  a viewing ray crossed and shade pixels with non-zero count
  as in shadow.

The stencil API has three knobs:

1. **The stencil function** — a comparison between the
   fragment's incoming **reference value** and the buffer's
   current value, with an optional **mask** that selects which
   bits to compare. Same family of comparators as `DepthFunc`:
   `Always`, `Never`, `Less`, `Equal`, etc.
2. **Three actions** — what to do to the stencil buffer in
   three cases: **stencil-fail** (the stencil function failed),
   **depth-fail** (stencil passed but depth failed), **pass**
   (both passed). Each action is a `StencilOp`: `Keep`,
   `Zero`, `Replace`, `Increment`, `Decrement`, `Invert`.
3. **A write mask** — which bits of the stencil buffer are
   actually mutable.

A typical "draw inside the stencil region only" sequence:

1. Render the stencil region itself with `StencilFunc::Always`,
   `StencilOp::Replace`, reference value 1. The stencil buffer
   gets 1 wherever the region was rendered.
2. Render the actual content with `StencilFunc::Equal`,
   reference 1. Only pixels whose stencil buffer holds 1 pass
   the test; those are exactly the pixels inside the stencil
   region.

The codebase exposes the full state via
`Rasterizer::setStencilFunc(...)` and
`Rasterizer::setStencilOps(...)`, with the default being
`StencilFunc::Always` (every fragment passes the stencil test)
and all three ops set to `Keep` (the buffer is never written).
The default-disabled state means the stencil pass costs zero
unless an application opts in.

## 19.5 Face culling

Independent of clipping and depth/stencil, the rasterizer can
**skip back-facing triangles**. A back-facing triangle is one
whose vertices, projected to screen space, wind clockwise — the
opposite of the convention this codebase uses for front-facing
triangles.

The motivation: a closed convex mesh has every back-facing
triangle hidden behind a front-facing one. Skipping back-facers
cuts the triangle count roughly in half for closed meshes, with
no visual change.

The state machine is one enum:

| `CullMode` | What gets rendered |
|---|---|
| `Both` (default) | every triangle, both sides |
| `Back` | front-facing triangles only |
| `Front` | back-facing triangles only |

The default is `Both` because the codebase ships open meshes
(planes, disks, single triangles) where culling either side
would lose visible geometry. Closed-only scenes can switch to
`Back` for the speedup.

The face culling test uses the projected screen-space winding
order — the same signed area
[chapter 18 §18.2](18-the-rasterization-pipeline.md#18-2-the-edge-function-inside-test)
computes for the inside test. A negative signed area means
clockwise winding (back-facing in this codebase's
counter-clockwise convention); a positive signed area means
counter-clockwise (front-facing). The cull check is one sign
test before the rasterization-prep work begins.

## 19.6 The full state machine, default values

The configurable state at a glance:

| State | Default | Rasterizer setter |
|---|---|---|
| `CullMode` | `Both` | `setCullMode` |
| `DepthFunc` | `Less` | `setDepthFunc` |
| Depth writes | enabled | `setDepthWrite` |
| Depth clear value | `+∞` | `setDepthClearValue` |
| `StencilFunc` | `Always` | `setStencilFunc` |
| `StencilOp` (3) | all `Keep` | `setStencilOps` |
| Stencil clear value | `0` | `setStencilClearValue` |
| Stencil write mask | `0xFF` | `setStencilWriteMask` |

The defaults give back the textbook fixed-function pipeline
the chapter-18 walkthrough describes: `Less` depth test, depth
writes on, no stencil test, no culling, both sides rendered.
Applications that want custom behavior set the relevant state
before calling `render(...)`; the state persists across
renders, so a single configuration applies to the whole frame.

## 19.7 Where this connects to GPU pipelines

Real-time GPU rasterizers expose essentially the same state
machine — the Direct3D / OpenGL / Vulkan / Metal "depth-stencil
state" is the same configuration as the codebase's, modulo
spelling. The reason is that the state machine is the *minimum*
configurability needed for the graphics-research-vintage
techniques that pre-shader pipelines exposed: shadow volumes
(stencil counting), reflections (stencil masking), [CSG](../appendix/a-glossary.md#c) (depth
peeling), portal rendering (stencil regions).

When the codebase eventually grows a GPU rasterizer engine
(roadmap §4.1, second-order item), the GPU rasterizer will
take exactly these state knobs and translate them to the GPU's
native state objects. The fixed-function semantics carry over
unchanged.

## 19.8 Exercises

1. Predict what happens when a triangle has all three vertices
   *behind* the camera. Trace the clipping algorithm: what
   does it produce? What does the rasterizer do with the
   result?
2. The Sutherland-Hodgman algorithm clips against one plane
   at a time, producing a convex polygon. What's the maximum
   vertex count that a triangle can produce after clipping
   against the six clip planes (near, far, left, right, top,
   bottom)? Why?
3. Construct a stencil-buffer state that renders the *outside*
   of a stencil region (instead of the inside). Walk through
   the two-pass sequence: what's the stencil function, the
   three ops, and the reference value at each pass?
4. The default `CullMode::Both` renders both sides of every
   triangle. For a closed mesh, this is wasteful. For an
   *open* mesh (a plane, a half-cylinder), it's necessary.
   Write a check that decides at scene build time whether
   `CullMode::Back` is safe.

## See also

- Volume index: [Volume IV — Rasterization](README.md)
- Previous:
  [18. The rasterization pipeline](18-the-rasterization-pipeline.md)
- Next: [20. Wireframe rendering](20-wireframe-rendering.md)
- Camera-side clip-space projection:
  [6. Cameras §6.1](../02-ray-rendering/06-cameras.md#6-1-the-camera-interface)
- Vertex attributes that get interpolated through the clipper:
  [17. Tessellation](17-tessellation.md)
- Perspective-correct attribute interpolation in detail:
  [21. MSAA and attribute interpolation](21-msaa-and-attribute-interpolation.md)

## Source anchors

<!-- source-anchors -->
- `include/render/HomogeneousClipVolume.h`
- `include/engine/raster/Rasterizer.h`
- `src/engine/raster/Rasterizer.cpp`
<!-- /source-anchors -->
