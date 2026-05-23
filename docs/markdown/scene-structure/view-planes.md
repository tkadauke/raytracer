# View planes

A view plane is a small abstraction with a big practical
consequence: it controls *what order* the renderer fills the
output framebuffer in. For a fully-rendered final image, the order
doesn't matter. For an interactive preview being drawn over time,
it changes whether the user sees a half-rendered top half of the
scene or a low-resolution version of the entire scene.

By the end you should know:

- what a `ViewPlane` actually represents,
- the five concrete iteration policies the codebase ships, and
  what each one looks like as the render progresses,
- why `RowShuffledViewPlane` and friends matter for the
  interactive editor.

## <a id="what-a-view-plane-is"></a>What a view plane is
The
[`ViewPlane`](../../../include/render/viewplanes/ViewPlane.h)
class bundles two things:

1. **A plane in 3D space**, at unit distance in front of the
   camera, oriented by the camera's matrix. Given a pixel
   coordinate $(x, y)$, `pixelAt(x, y)` returns the world-space
   point on that plane the camera should fire a ray *through*.
   This is the "screen" the camera projects onto.
2. **An iteration order** over the pixel grid. `begin(rect)`
   returns an iterator that walks the pixels in some order; the
   renderer's primary-ray loop calls it for each pixel.

Part 1 is shared across all the subclasses — the math of
"convert pixel coords to a 3D ray-target point" is dictated by
the camera and doesn't depend on iteration order. Part 2 is what
the subclasses override.

Both live in the same class because the iteration order needs
access to the same pixel-grid bounds the projection uses;
bundling them avoids a separate constructor argument. You can
reasonably read every `ViewPlane` subclass as "an iteration
policy that knows the pixel-grid size."

## <a id="the-default-row-major"></a>The default: row-major
The base `ViewPlane`'s iterator walks the pixels in textbook
row-major order: every column of row 0, then every column of row
1, and so on. Render the whole frame and the framebuffer fills in
top-to-bottom, left-to-right.

For an offline render — "produce a PNG file, then exit" — this is
fine. For an interactive preview, it's exactly the wrong order.
After two seconds of rendering you see a thin band at the top of
the image and nothing below; after ten seconds you see the top
third. You can't tell whether the camera is pointed in the right
direction until the render is most of the way done.

## <a id="the-four-alternatives"></a>The four alternatives
Each of these subclasses overrides only the iterator. The
projection math is unchanged.

`RowInterlacedViewPlane` walks rows in a stride-2 pattern: rows 0,
2, 4, ..., then rows 1, 3, 5, .... After the first pass you have
half the rows filled and a recognizable picture of the whole
scene; after the second pass, every row is done.

`RowShuffledViewPlane` walks rows in a deterministic random
order. After 10% of the render, you have 10% of the rows filled
*evenly distributed across the image*. Coverage is uniform from
the start; what improves is density.

`PointInterlacedViewPlane` does the same but per-pixel: it visits
every $N$th pixel of every row first, then fills in the gaps. The
first pass gives you a low-resolution version of the entire
image.

`PointShuffledViewPlane` walks pixels in a deterministic random
order. After 10% of the render, you have 10% of the pixels filled
evenly distributed across the entire image — same uniform
coverage as the row-shuffled variant, just at a finer grain.

`TiledViewPlane` walks the image in fixed-size tiles. This is the
choice you want when render work is parallelized across threads,
each owning a tile: you want spatial locality (so the camera /
material caches stay warm) and balanced work units. The
rasterizer's `queueSize > 1` parallel mode uses this.

The `viewplane_iteration_order` widget below lets you scrub
through each policy on a small grid and see what the framebuffer
looks like at any partial-completion percentage:

<!-- widget: viewplane_iteration_order -->

The widget makes one observation visible that's worth calling
out: row-major and tiled produce *spatially clustered* partial
renders (you see one part of the image, the rest is blank).
Shuffled and interlaced produce *spatially distributed* partial
renders (you see a faded version of the whole image, sharpening
over time). The two categories serve different purposes — one
optimizes for batch throughput, the other for interactive
feedback — and the right policy depends on which one you're
doing.

## <a id="what-an-iteration-order-is-not"></a>What an iteration order is *not*
A common confusion when reading the code: the iteration order is
not the sample-within-pixel order, and it's not the choice of
sampler. Those live one layer up, in
[Sampling and anti-aliasing](../ray-rendering/sampling-and-anti-aliasing.md).

The view plane decides *which pixel* the renderer is working on
right now. The sampler decides *which subpixel offsets* to evaluate
inside that pixel. The two are orthogonal: you can have a tiled
view plane with a jittered sampler, or a row-shuffled view plane
with a regular sampler, or any other combination, and the math
just composes.

## <a id="picking-a-view-plane"></a>Picking a view plane
The defaults the codebase makes:

- `rendercli` (offline batch) → row-major. No interactive consumer
  to please; the simplest order is fine.
- `src/modeler` →
  `RowShuffledViewPlane`. Editor previews need uniform coverage
  early so the camera position is observable a second or two into
  the render.
- Rasterizer with `queueSize > 1` → `TiledViewPlane`. Tiles are
  the parallel-work unit.

If you're adding a new front end, picking the view plane is one of
the small but visible decisions you'll make. The rule of thumb:
batch outputs use row-major (cheapest); interactive previews use
shuffled (best feedback); parallel renders use tiled.

## <a id="exercises"></a>Exercises
1. Write a `ViewPlane` subclass that walks pixels in a Hilbert-
   curve order. What advantage might that have over the
   row-shuffled variant for a renderer that caches per-tile
   geometry?
2. The `RowShuffledViewPlane` uses a deterministic shuffle so two
   renders of the same scene visit pixels in the same order. Find
   the seed in the source. What changes if you randomize per
   render? When would each be the right choice?
3. Suppose you have an 8-thread render of a 1920×1080 image. The
   tile size is 64×64. How many tiles total? On average, how many
   tiles per thread? What's the worst-case load imbalance if some
   tiles take 10× longer than others?
4. Read `Camera::render` and locate the call into
   `ViewPlane::begin(...)`. What changes if you swap in a different
   subclass mid-render? (Don't try this in production — it's not
   designed for it — but trace what would actually happen.)

## See also

- Volume index: [Scene structure](README.md)
- Previous: [Tone mapping](../ray-rendering/tone-mapping.md)
- Next: [Constructive solid geometry](csg.md)
- Sampler partner: [Sampling and anti-aliasing](../ray-rendering/sampling-and-anti-aliasing.md)
- Tiled mode is the parallel-work unit for the rasterizer:
  [MSAA and attribute interpolation](../rasterization/msaa-and-attribute-interpolation.md)

## Source anchors

<!-- source-anchors -->
- `include/render/viewplanes/ViewPlane.h`
- `include/render/viewplanes/ViewPlaneFactory.h`
- `include/render/viewplanes/TiledViewPlane.h`
- `include/render/viewplanes/RowInterlacedViewPlane.h`
- `include/render/viewplanes/RowShuffledViewPlane.h`
- `include/render/viewplanes/PointInterlacedViewPlane.h`
- `include/render/viewplanes/PointShuffledViewPlane.h`
<!-- /source-anchors -->
