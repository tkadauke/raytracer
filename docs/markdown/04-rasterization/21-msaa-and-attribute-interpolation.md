# 21. MSAA and attribute interpolation

Two topics share this chapter because they share the same
technical territory: the rasterizer's per-pixel computation
of *what value should be displayed at this pixel?* MSAA
(multi-sample anti-aliasing) is about the **coverage** answer —
"how much of this pixel does the triangle cover?" Attribute
interpolation is about the **content** answer — "given that the
triangle does cover this pixel, what depth, normal, UV, and
color does it have here?" Both questions get answered using the
same barycentric coordinates the edge-function rasterizer
produced in [chapter 18](18-the-rasterization-pipeline.md), but
in different ways.

By the end of this chapter you should know:

- the **screen-space barycentric weights** the rasterizer
  produces, and why they are *not* the perspective-correct
  attribute weights,
- the **Heckbert-Moreton** $1/z$ trick that recovers
  perspective-correct interpolation from screen-space weights,
- what MSAA samples (coverage) and what it doesn't (shading),
- the codebase's single-tile-vs-N-sample-resolved framebuffer
  split, and the per-sample buffers MSAA needs for the resolve.

## 21.1 Screen-space barycentric weights aren't perspective-correct

Recall from
[chapter 18 §18.2](18-the-rasterization-pipeline.md#18-2-the-edge-function-inside-test):
the rasterizer's edge-function inside-test produces, as a
side effect, three barycentric coordinates $(w_0, w_1, w_2)$
for every pixel inside the triangle. These weights are
*screen-space* barycentric coordinates — they are computed
from the triangle's *projected* (post-perspective-divide)
vertex positions.

Suppose you naively use those weights to interpolate a
per-vertex attribute. For each pixel inside the triangle:

$$
\text{attr}_{\text{interp}} = w_0 \, \text{attr}_0 + w_1 \, \text{attr}_1 + w_2 \, \text{attr}_2
$$

This works perfectly *in the absence of perspective*. For
orthographic projection, screen-space barycentric weights are
identical to world-space barycentric weights, and the
interpolation is correct. For pinhole projection, it isn't:
perspective foreshortening makes the same world-space step
along a triangle's edge correspond to a *shorter* screen-space
step near a far vertex than near a near vertex.

The visible effect is that linearly interpolating a per-vertex
attribute in screen space produces *non-linear* variation in
world space. The artifact is most obvious with textures: a
checker texture rendered onto a tilted floor with naive
interpolation looks visibly *warped* — the pattern spacing is
wrong on the parts of the floor far from the camera. The
classic example is the floor in early-90s console games, where
the texture warping was sometimes the giveaway that the engine
used affine (non-perspective-correct) texture mapping.

## 21.2 The Heckbert-Moreton $1/z$ trick

The fix, due to Heckbert and Moreton (1991), exploits a
mathematical identity: while world-space attributes do not
vary linearly in screen space, the quantity $1/z$ does. So
does any attribute *divided by* $z$. The recipe:

1. **Setup time** (per vertex): precompute $\text{attr}_i /
   z_i$ for every per-vertex attribute, along with $1 / z_i$.
2. **Per-pixel** (with screen-space barycentric weights $w_0,
   w_1, w_2$):

$$
\frac{1}{z_{\text{interp}}} = w_0 \, \frac{1}{z_0} + w_1 \, \frac{1}{z_1} + w_2 \, \frac{1}{z_2}
$$

$$
\frac{\text{attr}_{\text{interp}}}{z_{\text{interp}}} = w_0 \, \frac{\text{attr}_0}{z_0} + w_1 \, \frac{\text{attr}_1}{z_1} + w_2 \, \frac{\text{attr}_2}{z_2}
$$

3. **Recover** the attribute by dividing the second result by
   the first:

$$
\text{attr}_{\text{interp}} = \frac{(\text{attr}_{\text{interp}} / z_{\text{interp}})}{(1 / z_{\text{interp}})}
$$

The resulting `attr` is perspective-correct: it's the value the
attribute would have at the world-space point under the
projected pixel. The cost is two extra multiplies (one for the
attribute, one for $1/z$) per pixel per attribute, plus one
divide to recover the value. For a typical fragment with a
depth, a normal, a world-position, and a UV — five attributes —
the cost is around 10 multiplies and 1 divide per pixel for the
correction.

The widget shows the perspective-correct UV interpolation
side-by-side with the affine version, so the difference is
visible:

<!-- widget: rasterizer_perspective_uv -->

The codebase's
[`Rasterizer.cpp`](../../../src/engine/raster/Rasterizer.cpp)
applies this trick consistently: depth interpolation
(through the Z-buffer), normal interpolation (for shading),
world-position interpolation (for shadow rays), and UV
interpolation (for texture sampling) all go through the
$1/z$-divided form. The result is that surfaces look correct
under perspective projection at any angle, with no texture
warping.

## 21.3 MSAA: coverage sampling, not shading sampling

With one ray per pixel, the raytracer in
[chapter 10 §10.1](../02-ray-rendering/10-sampling-and-anti-aliasing.md#10-1-aliasing-as-undersampling)
produces aliasing on edges and small features. The fix there
was Monte Carlo integration — many sub-pixel samples of the
shading function, averaged.

The rasterizer's analogous fix is **multi-sample anti-aliasing
(MSAA)**. The trick is that, for the rasterizer, *most* of the
visible aliasing comes from triangle *coverage* at edges
rather than from rapid variation of the shading function across
the pixel area. A pixel near the silhouette of a sphere is
either fully covered by the silhouette triangle or fully
uncovered; the underlying shading function is nearly constant
across the pixel. So sampling the *coverage* finely, while
shading only once at the centroid of the covered samples,
captures most of the anti-aliasing benefit at a fraction of the
cost.

The `rasterizer_msaa_coverage` widget shows the effect with a
high-contrast diagonal triangle:

<!-- widget: rasterizer_msaa_coverage -->

At 1× sampling, edge pixels are either fully red or fully
blank, producing the staircase. At 4× sampling, edge pixels
get the right *fraction* of red — three of four samples
covered means 0.75 red — and the staircase smooths into a
gradient.

The crucial property is that **MSAA samples coverage, not
shading**. The four samples per pixel all evaluate to the same
shading function (because the pixel center is what gets
shaded), but each one tests *coverage* at a different sub-pixel
position. The averaged result is "0.75 red" for an edge pixel,
not "the average of four shading evaluations" — it's "the
shading evaluation, weighted by the per-sample coverage."

This is the difference from the raytracer's per-pixel
super-sampling. The raytracer does run the full shading
function at each sub-pixel sample (because it has no separate
notion of coverage). The rasterizer's MSAA cuts the shading
work to once per pixel, even at 4× MSAA — the multiplication
is in coverage and depth tests, not shading.

## 21.4 The MSAA implementation

The
[`Rasterizer`](../../../include/engine/raster/Rasterizer.h)
exposes `setMSAASamples(int)` with valid values $1$, $2$, $4$,
$8$. The implementation has two paths:

- **Single-tile path** (`m_msaaSamples == 1`): one float
  framebuffer, one Z-buffer, one stencil buffer. Identical to
  the [chapter 18](18-the-rasterization-pipeline.md) pipeline.
  This is the default and the codepath optimized for the most
  common case.
- **N-sample resolved path** (`m_msaaSamples > 1`): N
  per-sample color buffers, N per-sample Z-buffers, N
  per-sample stencil buffers. Each sample is rasterized
  independently against its own buffers, then a final
  *resolve* pass averages the N samples per pixel into the
  single output framebuffer.

The per-sample subpixel offsets follow a fixed pattern:
$2$-sample uses $(0, 0)$ and $(0.5, 0.5)$; $4$-sample uses a
diagonal-rotated grid; $8$-sample uses an 8-rook pattern.
Each pattern covers the pixel area more uniformly than the
naive regular grid, with the same anti-correlation properties
that make jittered sampling
([chapter 10 §10.2](../02-ray-rendering/10-sampling-and-anti-aliasing.md#10-2-the-three-samplers))
beat regular sampling.

The single-tile vs N-sample resolved split is *deliberate* —
the single-tile path runs the inner pixel loop without any
per-sample dispatch overhead, which on benchmarks runs about
3-4× faster than the resolved path at $1$× MSAA. For
non-MSAA renders this matters; for MSAA-on renders the user
has already opted into the higher cost.

## 21.5 The cost of MSAA

The doc-render benchmark sweep in the
[`Rasterizer`](../../../include/engine/raster/Rasterizer.h)
header shows the per-MSAA-level cost on three reference
scenes:

- Dense 640×480 LOD-8 sphere: 1× → 1035 ms median; 4× →
  3779 ms (about 3.6× slower).
- 1920×1080 offscreen-floor: 1× → 102 ms; 4× → 443 ms (about
  4.3× slower).
- 640×480 LOD-3 materials baseline: 1× → 14 ms; 4× → 56 ms
  (about 4× slower).

The 4× number is the dominant cost. The raster work
quadruples (4× per-sample triangle binning, 4× per-sample
depth tests, 4× per-sample writes). The shading work stays
constant (one fragment shader per pixel). The resolve pass
adds a small amortized cost.

The verdict is that MSAA is *expensive but worth it* for final
output. For interactive editing, single-sample is the right
choice because the cost-vs-benefit favors raw frame rate. For
the final-output render, a 4× MSAA pass cleans up the visible
aliasing at four times the per-frame cost.

## 21.6 Where the perspective-correct path matters most

The widgets above show the visible effect on UV mapping. The
same machinery has implications for every other interpolated
attribute:

- **Depth.** Z-buffer correctness depends on perspective-correct
  depth interpolation — the wrong interpolation produces
  z-fighting at angles and incorrect occlusion at edges. The
  default `Less` test from
  [chapter 19 §19.3](19-clipping-depth-stencil.md#19-3-the-depth-test)
  assumes the depth values are perspective-correct.
- **Normals.** Smooth-shaded primitives interpolate per-vertex
  normals across the triangle. Affine interpolation produces
  visible "shading discontinuities" at high triangle density
  on curved surfaces; perspective-correct interpolation
  produces the smooth shading transitions you'd expect.
- **World position.** Shadow rays from the rasterizer use the
  interpolated world position as the ray origin. Affine
  interpolation puts the ray origin at the wrong world-space
  point, and shadow lookups produce visibly wrong results.
- **UVs.** As covered. The most dramatic visible effect.

All four interpolations go through the same codebase
machinery, with the per-vertex precomputation amortized once at
triangle setup and the per-pixel cost paid uniformly across
all attributes.

## 21.7 What this chapter does *not* cover

Several anti-aliasing variants are queued under roadmap
§4.1.b:

- **SSAA (super-sample anti-aliasing).** Render the entire
  framebuffer at 2× or 4× the target resolution, then
  downsample. More expensive than MSAA (every pixel of the
  high-res framebuffer pays full shading cost), but produces
  better quality on shading-driven aliasing (highlights, fine
  geometry). Currently not implemented.
- **TAA (temporal anti-aliasing).** Accumulate samples across
  multiple frames, jittering the camera slightly each frame.
  Cheap per-frame, expensive in motion (ghosting artifacts).
  Belongs to a future real-time engine, not a static renderer.
- **FXAA / SMAA (post-process anti-aliasing).** Run an
  edge-detection pass on the rendered framebuffer and
  selectively blur edge pixels. Cheap; lower quality than
  MSAA. Useful for engines that can't afford MSAA's per-pixel
  cost.

All three are reasonable future additions; none are needed for
the current educational target. MSAA is the canonical
hardware-vintage anti-aliasing technique, which makes it the
right one to teach first.

## 21.8 Exercises

1. The Heckbert-Moreton trick interpolates `attr / z` and
   `1 / z` linearly, then divides at sample time. Verify
   algebraically that this produces the perspective-correct
   `attr` at each interior point of the triangle. Hint: write
   the world-space attribute as a linear function of barycentric
   weights, project to screen space, and chase the algebra.
2. Predict the rendering cost ratio between 1× and 8× MSAA for a
   scene whose triangles are mostly axis-aligned (so there are
   few edge pixels). Then predict the ratio for a scene with
   many diagonal silhouettes. Why are they different?
3. The codebase has separate per-sample Z-buffers for MSAA
   greater than 1. Why not share one Z-buffer across all
   samples, the way some GPU pipelines do? What artifact would
   that cause?
4. Construct a scene where the rasterizer's MSAA produces a
   visibly smoother result than the same scene rendered through
   the raytracer at the same total ray count. Construct a
   scene where the raytracer wins instead. What property
   distinguishes them?

## See also

- Volume index: [Volume IV — Rasterization](README.md)
- Previous: [20. Wireframe rendering](20-wireframe-rendering.md)
- Next volume:
  [Volume V — Image processing & computer vision](../05-image-and-vision/README.md)
- Sampler partner:
  [10. Sampling and anti-aliasing](../02-ray-rendering/10-sampling-and-anti-aliasing.md)
- Texture interpolation context:
  [11. Textures](../02-ray-rendering/11-textures.md)
- Edge-function barycentric weights:
  [18. The rasterization pipeline §18.2](18-the-rasterization-pipeline.md#18-2-the-edge-function-inside-test)
- Z-buffer:
  [19. Clipping, depth, stencil §19.3](19-clipping-depth-stencil.md#19-3-the-depth-test)

## Source anchors

<!-- source-anchors -->
- `src/engine/raster/Rasterizer.cpp`
- `include/engine/raster/Rasterizer.h`
- `include/core/geometry/Rasterize.h`
<!-- /source-anchors -->
