# Blob analysis and silhouettes

The renderer produces images. The test suite asks geometric
questions of those images: *is this a circle? is the camera
pointing at the right object? is the rendered output even
visible at all?* This chapter covers the two CV primitives the
codebase uses to answer those questions:
[`Blob`](../../../test/helpers/Blob.h) for connected-component
flood fill, and
[`Silhouette`](../../../test/helpers/Silhouette.h) for outer-
edge extraction.

The two primitives serve different needs. `Blob` cares about
**interior fill** — given a region of pixels of some target
color, what is the connected mass that contains them, and what
geometric properties does it have? `Silhouette` cares only
about the **outer edge** — what does the shape look like *from
the outside*, ignoring whether the interior is filled in.

By the end of this chapter you should know:

- the connected-components flood-fill algorithm `Blob` uses,
- the geometric descriptors `Blob` exposes (area, perimeter,
  centroid, bounding box, aspect ratio, circularity,
  radial variance, extent),
- the outer-extreme sampling that `Silhouette` uses,
- why the codebase has both, and which to pick when.

## <a id="connected-components-flood-fill"></a>Connected components: flood fill
A **blob** is a maximally connected region of pixels of a
target color. *Connected* means: each pixel is reachable from
every other pixel via a sequence of adjacent target-colored
pixels. The codebase uses **4-connectivity** — only the four
orthogonal neighbors (up, down, left, right) count as adjacent;
diagonal neighbors do not.

The standard algorithm is **breadth-first flood fill**: starting
at any unvisited target-colored pixel, explore outward through
its target-colored neighbors until no more reachable target
pixels remain. The result is one blob; repeat from any
remaining unvisited target-colored pixel to find the next.

The pseudocode:

```
visited = bitmask of size width × height, all false
blobs = []

for each (x, y) in buffer:
  if buffer[y][x] == target and !visited[y][x]:
    queue = [(x, y)]
    blob = []
    while queue is not empty:
      p = queue.pop_front()
      if !visited[p] and buffer[p] == target:
        visited[p] = true
        blob.add(p)
        queue.push_back(p + (-1, 0))  // left
        queue.push_back(p + (+1, 0))  // right
        queue.push_back(p + (0, -1))  // up
        queue.push_back(p + (0, +1))  // down
    blobs.add(Blob(blob))
```

The cost is $\mathcal{O}(\text{pixel count})$ — every pixel is
visited at most twice (once as the queue input, once when
popped). The auxiliary `visited` bitmask is the same size as
the buffer.

The codebase's
[`findAllBlobs(buffer, color)`](../../../test/helpers/Blob.h)
returns a `std::vector<Blob>`, one entry per connected region.
A convenience wrapper, `findLargestBlob`, returns the
single biggest blob by area or `std::nullopt` if no
target-colored pixels exist.

## <a id="watching-the-algorithm-run"></a>Watching the algorithm run
A still picture of a finished blob doesn't show the
*algorithm*. The figure below — backed by
`scripts/docs/connected_components.js` — animates the BFS step
by step. The example raster contains three disconnected
components; the slider advances the BFS one cell-pop at a
time, and each component gets its own color as it is
discovered:

<!-- widget: connected_components -->

Drag the slider to the left and watch the BFS replay from a
fresh start; drag to the right and the algorithm fast-forwards
through every cell. The caption at the bottom counts the cells
visited so far and the number of distinct components
discovered.

## <a id="what-a-blob-carries"></a>What a blob carries
Beyond the raw pixel list, a `Blob` precomputes several
**descriptors** — geometric properties that summarize the
blob's shape:

- **Area** — total pixel count.
- **Perimeter** — boundary pixel count. The boundary is the
  subset of blob pixels with at least one non-blob 4-neighbor
  (or a buffer-edge neighbor).
- **[Centroid](../appendix/a-glossary.md#c)** — pixel-mass-weighted center. The average of
  every blob pixel's coordinates, rounded to integer.
- **Bounding box** — the axis-aligned `Recti` tight to the
  blob's pixel extents.
- **Aspect ratio** — bounding-box height over width.
- **Circularity** ([Polsby-Popper](../appendix/a-glossary.md#p)) — $4\pi \cdot \text{area} /
  \text{perimeter}^2$, normalized to $[0, 1]$ where $1.0$ is
  a perfect circle.
- **Radial variance** — the standard deviation of boundary-
  point distance from the centroid, normalized by the mean. A
  circle has all boundary points at the same radius, so the
  variance is $0$. A square has corners poking out, so the
  variance is larger.
- **Extent** — area divided by bounding-box area; how full
  the shape is relative to its enclosing rectangle.

These eight descriptors form a feature vector. The next
chapter covers the
[`ShapeClassifier`](../../../test/helpers/ShapeClassifier.h)
that takes that vector and decides whether the shape *is* a
circle, square, etc.

A specific design property is worth pinning. The
**`circularity` descriptor** is sensible only for *filled*
blobs — for outline-only shapes ([Wireframe](../appendix/a-glossary.md#w)-rendered
silhouettes) the area equals the perimeter and the value
collapses to a useless function of perimeter alone. The
**`radialVariance` descriptor**, by contrast, depends only on
boundary points, and the boundary of a filled circle and the
boundary of an outline circle are the same set of pixels. So
`radialVariance` gives the *same* answer for the
Raytracer-rendered solid disk and the Wireframe-rendered
circle outline of the same shape.

That property — engine-agnostic descriptors — is what makes
the classifier in [Shape classification](shape-classification.md) work uniformly across all three
rendering engines.

## <a id="the-silhouette-shortcut"></a>The `Silhouette` shortcut
`Blob` does too much work for the common case of "I just
want to know the *shape* of the rendered object." A
connected-components flood fill is
$\mathcal{O}(\text{pixel count})$ with constant work per
pixel, but for shape classification you only need the outer
extremes — the boundary contour, sampled at the extreme
points. The interior is irrelevant.

`Silhouette` is the simpler primitive that builds only that.
The algorithm is two linear scans:

```
points = []

# Row-major scan: leftmost and rightmost target pixel per row.
for y in 0..height:
  left = first x where buffer[y][x] == target
  right = last x where buffer[y][x] == target
  if found:
    points.add((left, y))
    points.add((right, y))

# Column-major scan: topmost and bottommost target pixel per column.
for x in 0..width:
  top = first y where buffer[y][x] == target
  bottom = last y where buffer[y][x] == target
  if found:
    points.add((x, top))
    points.add((x, bottom))
```

Two passes over the buffer, $\mathcal{O}(\text{pixel count})$
total cost, no auxiliary bitmask. The output is a
`Silhouette` carrying the extreme-point set, plus the
centroid and bounding box derived from it.

The crucial property is that `Silhouette` produces *the same
points* whether the input is a filled blob (every interior
pixel is target-colored) or an outline (only the boundary is
target-colored). The leftmost target pixel of row $y$ is the
boundary pixel of the leftmost edge — the interior pixels in
between are irrelevant to the leftmost-rightmost-per-row
extraction.

`Silhouette::aspectRatio` and `Silhouette::radialVariance`
work on the same descriptor formulas as `Blob`'s, just over
the silhouette point set instead of the boundary subset of
the blob's pixels. The two metrics produce the same answers
for shape classification.

## <a id="picking-between-blob-and-silhouette"></a>Picking between Blob and Silhouette
Reach for `Blob` when:

- You need to **count discrete objects** in a scene (each
  blob is one object).
- You need the blob's **interior area** (a measure of how
  big the rendered object actually is).
- You need the **circularity** descriptor or any other
  area-and-perimeter-based metric.

Reach for `Silhouette` when:

- You only need the *shape* of one object — its outline.
- You're working with a Wireframe render where the interior
  is background.
- You want the cheaper construction (two linear scans, no
  flood fill).
- You want descriptors that are **engine-agnostic**: same
  answer regardless of whether the test ran through
  Raytracer, Rasterizer, or Wireframe.

The third point is the most practical reason. The codebase's
functional tests, designed to run uniformly across engines,
use `Silhouette` (via `ShapeClassifier`) almost exclusively.
`Blob` shows up when the test specifically wants to assert
"exactly $N$ red regions" or "the red region's interior area
is at least $M$ pixels."

## <a id="what-this-chapter-does-not-cover"></a>What this chapter does *not* cover
Several connected-components extensions are not implemented:

- **8-connectivity.** Considering diagonal neighbors as
  adjacent. Slightly different blob splits at corner cases.
- **Run-length-encoded blob detection.** Faster than BFS for
  scenes with very long horizontal runs (which 4-connectivity
  visits cell-by-cell).
- **Two-pass label-equivalence algorithm.** A different
  connected-components algorithm with $\mathcal{O}(1)$
  amortized per-pixel cost. Used in production OpenCV.
- **Hierarchical / recursive blob analysis.** Trees of nested
  blobs (a hole inside a region, with a child region inside
  the hole, etc.).
- **Color-tolerant matching.** Comparing pixel colors with
  perceptual distance instead of exact equality.

None ship today. The current 4-connectivity exact-color BFS
is sufficient for the synthetic-renderer-output use case the
test suite lives in.

## <a id="exercises"></a>Exercises
1. Predict the BFS visit count for a scene with two non-
   overlapping red disks of radius 100 pixels each, on a
   black background. Compare to the visit count for one red
   disk of the same total area.
2. The `Silhouette::extractSilhouette` function does two
   scans: row-major and column-major. Why both? Construct an
   example where row-major alone would miss a critical
   extreme point.
3. The `circularity` descriptor returns about $0.785$ for a
   solid square (a value of $\pi/4$). Why? Derive the
   formula's value algebraically for a square of side $n$
   pixels.
4. Suppose you render a Wireframe scene of a torus from
   side-on. The silhouette is the outer-edge of the torus
   from that viewpoint. What does
   `Silhouette::radialVariance` return on it? Compare to the
   value for a side-on silhouette of a circle. Why are the
   two different?

## See also

- Volume index:
  [Image processing & computer vision](README.md)
- Previous:
  [Image buffers and pixel formats](image-buffers-and-pixel-formats.md)
- Next: [Shape classification](shape-classification.md)
- Buffer iteration:
  [The standard iteration pattern](image-buffers-and-pixel-formats.md#the-standard-iteration-pattern)
- The classifier consumer:
  [Shape classification](shape-classification.md)

## Source anchors

<!-- source-anchors -->
- `test/helpers/Blob.h`
- `test/helpers/Blob.cpp`
- `test/helpers/Silhouette.h`
- `test/helpers/Silhouette.cpp`
<!-- /source-anchors -->
