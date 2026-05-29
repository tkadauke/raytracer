# The rasterization pipeline

The raytracer in Ray rendering asks, for each pixel, *which
primitive does this pixel's ray hit?* The rasterizer asks the
inverse question: *for each triangle in the scene, which
pixels are inside its projection?* The two algorithms produce
the same image of the same scene, but the loops are inverted —
ray-over-pixels versus triangle-over-pixels — and the
mathematical machinery is correspondingly different.

This chapter walks the rasterizer's machinery end to end. By
the end you should know:

- the six stages a triangle goes through from "mesh face" to
  "shaded pixels in the framebuffer",
- the **edge-function** algorithm that decides which pixels
  are inside a triangle ([Pineda](../appendix/a-glossary.md#p) 1988),
- how the prepared-triangle structure precomputes the
  edge-function increments so the inner pixel loop is a few
  adds per pixel,
- the perspective-correct interpolation trick that makes
  per-vertex attributes (depth, normal, [UV](../appendix/a-glossary.md#u)) survive projection,
- and the per-primitive material-lookup path that gives the
  rasterizer's shaded output something to look at.

## <a id="the-pipeline-at-a-glance"></a>The pipeline at a glance
Reading the codebase's
[`engine::raster::Rasterizer`](../../../include/engine/raster/Rasterizer.h)
header in order, a triangle from the scene's tessellated mesh
goes through six stages:

1. **Vertex transform.** Every vertex's homogeneous clip-space
   coordinates are computed via `Camera::projectPointToClipSpace`
   ([Cameras: The `Camera` interface](../ray-rendering/cameras.md#the-camera-interface)).
   Vertices already inside the clip volume get a cached screen-
   space position so step 4 can skip the divide.
2. **Triangulation.** Mesh faces are polygons; the rasterizer
   triangulates them by fan-splitting from vertex 0. The
   per-primitive tessellate implementations in
   [Tessellation](tessellation.md) guarantee convex faces, so
   fan-splitting is always valid.
3. **Clipping.** Each triangle gets clipped in homogeneous
   space against the near plane and the four viewport edges.
   Triangles wholly outside any plane are skipped; triangles
   straddling a plane are split, as
   [Clipping, depth, stencil](clipping-depth-stencil.md) covers in detail.
4. **Rasterization.** The clipped triangle's screen-space
   coordinates feed `core::rasterizeTriangle`, which iterates
   the pixels inside the triangle's bounding box and emits a
   per-pixel callback for every pixel inside the triangle's
   projected outline. The math is the **edge-function**
   algorithm described in [The prepared triangle](#the-prepared-triangle-incremental-edge-stepping).
5. **Depth test.** Each emitted pixel's perspective-correct
   depth is compared against the per-pixel [Z-buffer](../appendix/a-glossary.md#z). Failing
   pixels are dropped. The full depth-and-stencil state machine
   is in [Clipping, depth, stencil](clipping-depth-stencil.md);
   [The depth test](#the-depth-test) sketches the default `Less` test.
6. **Fragment shading.** Surviving pixels get their normal,
   world position, and UV interpolated, the material is sampled
   for albedo and direct-lighting coefficients, and local material
   shading produces the final color.
   The result is written to the float framebuffer; tonemap
   ([Tone mapping](../ray-rendering/tone-mapping.md)) does
   the rest.

Step 4 is the heart of the algorithm and the namesake of the
chapter; the rest is plumbing.

When shadow maps are enabled, an extra light-space depth pass
runs *before* step 1 — once per directional light. The fragment
shader at step 6 then projects each shaded point into the
stored depth image to decide whether the surface receives
direct light. The shadow-map pipeline is documented in
[Lights and shading: Shadow maps for the rasterizer](../ray-rendering/lights-and-shading.md#shadow-maps-for-the-rasterizer).

## <a id="the-edge-function-inside-test"></a>The edge-function inside-test
A triangle in 2D has three edges. The **edge function** for an
edge from $\mathbf{a}$ to $\mathbf{b}$ evaluated at a point
$\mathbf{p}$ is

$$
E_{\mathbf{a}\mathbf{b}}(\mathbf{p}) = (b_x - a_x)(p_y - a_y) - (b_y - a_y)(p_x - a_x)
$$

This is the $z$-component of the cross product
$(\mathbf{b} - \mathbf{a}) \times (\mathbf{p} - \mathbf{a})$
from
[Numbers and vectors: Cross product](../foundations/numbers-and-vectors.md#cross-product),
which is twice the signed area of the triangle
$\mathbf{a}\mathbf{b}\mathbf{p}$. Sign matters: positive when
$\mathbf{p}$ is on one side of the edge, negative on the other,
zero on the edge.

A triangle's three edge functions agree on the sign for any
$\mathbf{p}$ inside the triangle. They disagree (one positive,
two negative, or vice versa) for any $\mathbf{p}$ outside. The
**Pineda 1988** algorithm uses this directly: a pixel is inside
the triangle iff all three edge functions have the same sign as
the parent triangle's signed area.

Samples exactly on an edge need a tie break. This rasterizer uses
the standard **top-left fill rule**: top or left edges include
edge samples, while bottom or right edges exclude them. Two
adjacent triangles sharing an edge therefore cannot both claim the
same pixel, which matters for stencil operations, object-id
passes, and any diagnostic pass that counts fragments.

The widget shows this hands-on with three draggable triangle
vertices, the bounding-box scan region the algorithm walks, and
the per-pixel inside test plus barycentric weights:

<!-- widget: rasterizer_pipeline -->

The pixel coloring in the widget makes a useful auxiliary
property visible: the three edge-function values, divided by
the parent area, are exactly the **barycentric coordinates**
$(w_0, w_1, w_2)$ of the pixel — the same coordinates from
[Primitives and intersection: Triangle: Möller-Trumbore](../ray-rendering/primitives-and-intersection.md#triangle-moller-trumbore).
The inside-test and the attribute-interpolation weights fall
out of the same computation, which is half of why this
algorithm dominates rasterization in practice.

The other half is that edge-function rasterization
**parallelizes trivially**. Every pixel's inside-test is
independent of every other pixel's; a GPU can fire 32 pixels'
worth of inside-tests in parallel and they don't interact.
Hardware GPUs use variants of this algorithm; the codebase's
software rasterizer follows the same recipe, just running on
the CPU.

## <a id="the-prepared-triangle-incremental-edge-stepping"></a>The prepared triangle: incremental edge stepping
The naive implementation evaluates all three edge functions at
every pixel of the bounding box. That's three multiplies and
three subtractions per pixel per edge, six edge values for the
inside test — about 30 floating-point operations per pixel.

The optimization, also in
[`include/core/geometry/Rasterize.h`](../../../include/core/geometry/Rasterize.h),
exploits the linearity of the edge function. Stepping
$\mathbf{p}$ by $(1, 0)$ in screen space changes
$E_{\mathbf{a}\mathbf{b}}(\mathbf{p})$ by exactly $-(b_y -
a_y)$ — a constant, computable once at triangle setup. Stepping
by $(0, 1)$ changes it by $(b_x - a_x)$, also constant. The
edge-function value across a row is therefore an arithmetic
sequence with a fixed step, and across rows is the same with
a different fixed step.

The codebase's implementation precomputes the three edges'
starting values at the bounding-box corner, plus the row and
column step deltas, into a `PreparedRasterTriangle` struct.
The inner pixel loop is then three adds per pixel — one per
edge — plus the sign comparisons and the top-left equality tests.
The inside-test cost drops from ~30 ops per pixel to a handful of
integer comparisons.

The struct also holds the parent triangle's signed area and
its inverse (the latter precomputed once so the per-pixel
barycentric-weight conversion is a multiply instead of a
divide). The fixed-point representation — `kRasterSubpixelScale
= 256` — keeps fractional projected vertices and MSAA sample
offsets in the same coordinate system while storing edge values
in `int64_t` instead of `double`.

The integer fixed-point form has two benefits. First, small
camera or object movements can shift an edge by a fraction of a
pixel without forcing all vertices to round to new integer
locations at once. Second, the inside-test reduces to integer
comparisons, including exact zero-on-edge checks for the top-left
rule.

## <a id="the-depth-test"></a>The depth test
When two triangles project to overlapping screen-space pixels,
some pixels need to come from the front triangle and some from
the back triangle. The mechanism is the **Z-buffer**: a
per-pixel depth value, initialized to `+infinity`, that
records the closest-so-far depth at each pixel. A new pixel
writes its color *and* its depth if its depth is closer than
the buffer's current value, and is discarded otherwise.

This works as long as the depth values are perspective-correct.
A naive implementation that linearly interpolates depth across
a triangle in screen space gets it wrong: depth doesn't vary
linearly in screen space under perspective projection. Instead,
**$1/z$ varies linearly in screen space**. The trick is to
interpolate $1/z$ using the rasterizer's barycentric weights,
then invert at sample time:

$$
z_{\text{interp}} = \frac{1}{w_0 / z_0 + w_1 / z_1 + w_2 / z_2}
$$

This is the **[Heckbert-Moreton](../appendix/a-glossary.md#h)** trick (1991), and
[MSAA and attribute interpolation](msaa-and-attribute-interpolation.md) walks it
through in detail. The same trick applies to every per-vertex
attribute (normals, UVs, world positions) — interpolate each
attribute *divided by z* in screen space, then divide by the
interpolated $1/z$ at sample time to recover the
perspective-correct value.

The default depth test is `DepthFunc::Less` (write only when
the new pixel's depth is strictly less than the buffer's), with
depth writes enabled. The full configurability — `Less`,
`Lequal`, `Greater`, etc., plus stencil — is in
[Clipping, depth, stencil](clipping-depth-stencil.md).

## <a id="fragment-shading"></a>Fragment shading
A pixel that survives the depth test has known barycentric
weights, and from those, perspective-correct interpolated
attributes:

- **Normal** — interpolated unit-length surface normal, used
  for direct diffuse and specular shading.
- **World position** — used by texture mappings that depend on
  world coordinates and (when shadow maps are enabled) by the
  shadow-map projection step from
  [Lights and shading: Shadow maps for the rasterizer](../ray-rendering/lights-and-shading.md#shadow-maps-for-the-rasterizer).
- **UV** — used by texture sampling.

The fragment shader is a small function that takes those
interpolated attributes plus the primitive's material and
produces the final pixel color. The default behavior handles the
local direct-lighting subset of
[`MatteMaterial`](../../../include/render/materials/MatteMaterial.h)
and [`PhongMaterial`](../../../include/render/materials/PhongMaterial.h)
from [Materials and BRDFs: The five shipped materials](../ray-rendering/materials-and-brdfs.md#the-five-shipped-materials):

$$
L = k_a \, \mathbf{albedo} \, L_{\text{ambient}} + \sum_{\text{lights}} (k_d \, \mathbf{albedo} + k_s \, \mathbf{specular} \, (\mathbf{r} \cdot \mathbf{v})^e) \, L_{\text{light}} \, \max(0, \mathbf{n} \cdot \mathbf{l})
$$

The `albedo` comes from the primitive's material — sampled at
the interpolated UV when the material has a texture, otherwise
the constant material color. Primitives without a usable
material fall back to a stable per-face hash so missing
materials remain *visible* (a colored mesh) rather than
*invisible* (a black mesh).

For Phong-family materials, the rasterizer also reads the specular
color, coefficient, and exponent and evaluates the same local lobe as
the raytracer. Recursive reflection and refraction remain raytracer-only;
reflective materials preview only their local Phong base, while
transparent materials preview that same local base with transient source
alpha computed from `1 - transmissionCoefficient`. `rendercli --engine raster`
prints a fallback warning when a scene uses either material so this
non-recursive preview is explicit.

![Raster material preview: two Matte spheres followed by broad and tight Phong highlights](../../images/rasterizer_material_preview.png)

The implementation prepares this material path while it emits
triangles: each leaf primitive is classified once, and each emitted
triangle carries cached material terms plus a raster albedo evaluator.
Exact `ConstantColorTexture` values are stored directly. Exact
`UVColorTexture` objects and UV-mapped `CheckerBoardTexture` objects sample
from the interpolated UV without fabricating a ray-hit context. Arbitrary
texture objects still use the virtual `Texture::evaluate(...)` path with a
synthesized `HitPoint`, preserving custom texture behavior.

Normal maps use the same UV stream but feed the lighting normal rather than the
albedo. For each rasterized triangle, the emitter derives a tangent and
bitangent from the triangle's world-space edges and UV edges. A sampled normal
map color is decoded from RGB into tangent space, then transformed through that
frame before the diffuse and Phong terms evaluate `n dot l`. If the UVs are
degenerate and no tangent frame can be derived, the rasterizer keeps the
interpolated geometric normal.

| Flat geometric normal | Tangent-space normal map |
| --- | --- |
| ![Flat raster rectangle lit by its geometric normal](../../images/rasterizer_normal_map_flat.png) | ![Raster rectangle with checker normal map changing the lighting](../../images/rasterizer_normal_map_mapped.png) |

This is one place where the rasterizer takes a shortcut
compared to the raytracer. The [Whitted](../appendix/a-glossary.md#w) raytracer in
[The Whitted pipeline: The recursive heart](../ray-rendering/the-whitted-pipeline.md#the-recursive-heart)
calls the material's `shade(...)` method, which can recurse on
secondary rays for reflection and refraction. The rasterizer
runs only the direct-shading path. Reflection and refraction
in a software rasterizer would require a hybrid raytrace-
rasterize approach — *render the scene, then for each
reflective pixel, ray-trace the reflection, then composite*.

For diffuse surfaces, point lights, and directional lights, the
rasterizer produces the same image as the raytracer. For
mirror, glass, and refraction, only the raytracer can render
them.

The reusable scene file
[`scenes/raster_material_preview.json`](../../../scenes/raster_material_preview.json)
matches the preview image's intent: the left pair isolates Matte
ambient/diffuse coefficients, and the right pair isolates broad versus
tight Phong specular response.

## <a id="cpu-and-opengl-raster-backends"></a>CPU and OpenGL raster backends
The software rasterizer remains the reference implementation because it
is deterministic, inspectable, and works without a graphics context. The
render graph can also compile raster beauty passes with the experimental
OpenGL backend. That backend still uses the same scene, camera,
tessellation, graph intent, and graph-owned shadow-map resources, but it
submits the final mesh pass to an offscreen OpenGL framebuffer and reads
the result back into the graph resource.

The comparison below uses graph-backed renders for both columns. The
first row exercises local material lighting plus planar and UV checker
textures. The second row enables graph preview shadow maps so both
backends consume the same shadow-map intent.

| CPU raster | OpenGL raster |
| --- | --- |
| ![Graph-backed CPU raster render with directional lighting and checker textures](../../images/rasterizer_backend_lit_cpu.png) | ![Graph-backed OpenGL raster render with directional lighting and checker textures](../../images/rasterizer_backend_lit_opengl.png) |
| ![Graph-backed CPU raster render with graph preview shadow maps](../../images/rasterizer_backend_shadow_cpu.png) | ![Graph-backed OpenGL raster render with graph preview shadow maps](../../images/rasterizer_backend_shadow_opengl.png) |

Differences in those images are useful. The CPU column is the
correctness reference for the software path and for headless docs/CI
runs. The OpenGL column shows which pieces of the same compiled raster
state are already implemented in the GPU backend: directional and point
diffuse/Phong lighting, direct checker/image texture sampling,
depth bias, depth and stencil attachments, MSAA framebuffers, and the currently supported
shader-side shadow-map subset. Unsupported raster state should fail or
fall back through the graph with trace metadata instead of silently
changing the render.

## <a id="the-render-method-end-to-end"></a>The render method, end to end
The full
[`Rasterizer::render`](../../../src/engine/raster/Rasterizer.cpp)
is large, but the structure follows [the pipeline overview](#the-pipeline-at-a-glance) with a
seventh (the float-to-[LDR](../appendix/a-glossary.md#l) conversion) bolted on the end.
Reading the algorithmic spine:

```
for each leaf primitive in scene:
    if finite bounds are wholly outside one clip plane:
        continue
    mesh = primitive.tessellate(lod)
    project each vertex to clip space
    for each face (fan-triangulated to triangles):
        for each triangle:
            clip in homogeneous space against near + viewport
            for each clipped sub-triangle:
                rasterizeTriangle(...) -> per-pixel callback:
                    interpolate attributes (perspective-correct)
                    depth test
                    fragment shade
                    write color and depth
```

Three important details. First, the rasterizer **walks leaf
primitives directly** rather than tessellating the entire
scene into one mega-mesh. The motivation is per-primitive
material lookup: each leaf carries its own material, and
collapsing everything into one mesh would lose that
information. Finite primitive bounds are tested before
tessellation with the same homogeneous outcode planes used for
triangle clipping; a primitive is skipped only when every
bounding-box corner is outside the same plane. Second, the
default 1x single-tile path streams
those projected and clipped triangles directly into the pass
buffers. The retained `RasterTriangleSet` appears only when
the renderer needs reuse: tiled rendering bins triangles by
tile, and MSAA replays the same projected triangles across
multiple sample offsets. Third, the inner loops are written for
straight-line CPU performance — the `core::rasterizeTriangle`
inline expansion gets the per-pixel callback inlined into the
prepared-triangle inner loop, so the entire `rasterize +
depth-test + interpolate + shade + write` cycle runs as a
single tight pixel loop with minimal indirect calls.

## <a id="why-the-rasterizer-exists"></a>Why the rasterizer exists
The chapter's introduction said the rasterizer and the
raytracer produce the same image. That's almost true — for
diffuse-only scenes with point and directional lights, the two
match. Three reasons the rasterizer is worth shipping anyway:

1. **Pedagogical clarity.** The textbook fixed-function
   pipeline maps directly onto code you can read in one
   sitting. There is no equivalent for the raytracer's
   recursive integrator.
2. **Headless rendering.** The rasterizer runs on the CPU with
   no GL stack, so it works on remote machines, in CI, in
   minimal Docker containers, anywhere a GPU might not be
   available.
3. **Side-by-side debugging.** Rendering the same scene through
   both engines and comparing the output catches bugs in
   either side. A primitive whose tessellation has a normal
   error renders weirdly under the rasterizer but correctly
   under the raytracer; a primitive whose intersection has a
   bug renders weirdly under the raytracer but correctly under
   the rasterizer. The two engines triangulate each other's
   bugs.

The third point is the practical reason a serious renderer
ships both. The roadmap envisions adding more engines
(WebGL preview, GPU rasterizer, full path tracer); each
one's debugging gets easier as the catalog of "render through
all engines, look for differences" grows.

## <a id="exercises"></a>Exercises
1. The edge function is twice the signed area of a triangle.
   Why "twice"? What's the constant factor doing in the
   formula, and what would the inside-test look like if it
   were divided away?
2. Predict the cost of the edge-function inside-test (in CPU
   adds and compares per pixel) for a triangle whose bounding
   box is 100×100 pixels. Compare to the cost of the same
   triangle's *area in pixels* (inside the triangle). What
   fraction of the work is "outside the triangle"?
3. The rasterizer interpolates $1/z$ linearly in screen space
   and inverts at sample time. What goes wrong if you
   interpolate $z$ directly? Construct a specific example with
   two visible artifacts.
4. Read the `Rasterizer::render` implementation and find the
   inner per-pixel callback. Identify exactly where the
   barycentric weights from [The edge-function inside-test](#the-edge-function-inside-test),
   the perspective-correct inversion from [The depth test](#the-depth-test),
   and the direct-material shading sum from [Fragment shading](#fragment-shading)
   happen. How many CPU operations is the entire chain,
   per pixel, in the default state?

## See also

- Volume index: [Rasterization](README.md)
- Previous: [Tessellation](tessellation.md)
- Next: [Clipping, depth, stencil](clipping-depth-stencil.md)
- Engine abstraction:
  [The `RenderEngine` abstraction](../ray-rendering/the-whitted-pipeline.md#the-renderengine-abstraction)
- Per-vertex attributes from tessellation:
  [Tessellation](tessellation.md)
- Material shading shared with the raytracer:
  [The five shipped materials](../ray-rendering/materials-and-brdfs.md#the-five-shipped-materials)
- Perspective-correct interpolation in detail:
  [MSAA and attribute interpolation](msaa-and-attribute-interpolation.md)
- Clipping detail:
  [Clipping, depth, stencil](clipping-depth-stencil.md)

## Source anchors

<!-- source-anchors -->
- `include/core/geometry/Rasterize.h`
- `include/engine/raster/Rasterizer.h`
- `src/engine/raster/Rasterizer.cpp`
- `include/engine/raster/detail/RasterMaterial.h`
- `include/engine/raster/detail/RasterTriangleEmitter.h`
- `include/render/HomogeneousClipVolume.h`
- `include/render/TilePlan.h`
<!-- /source-anchors -->
