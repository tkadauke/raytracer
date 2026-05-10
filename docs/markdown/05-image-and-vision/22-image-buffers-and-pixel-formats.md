# 22. Image buffers and pixel formats

A renderer's output is an image, and an image is — in the
codebase's vocabulary — a `Buffer<T>` over some pixel type.
Volumes II, III, and IV all spend their time *filling* those
buffers. Volume V is about *consuming* them: classical
computer-vision algorithms that take a rendered image and pull
geometric meaning out of it.

This is a short opening chapter. It re-introduces the buffer
abstraction with a CV-consumer's mindset and frames the small
toolkit of helpers that the test suite uses today. The next two
chapters get into the actual algorithms (connected components,
shape classifiers); this one lays the data-side groundwork.

By the end of this chapter you should know:

- the pixel-type conventions Volume V works with,
- the standard iteration patterns over `Buffer<T>`,
- the contract between rendering output and CV input,
- the small set of CV helpers the codebase ships in
  `test/helpers/`, and what each one is for.

## 22.1 The pixel formats Volume V consumes

[Chapter 4 §4.5](../01-foundations/04-color-and-buffers.md#4-5-buffer-t-a-fixed-size-2d-array)
introduced two `Buffer<T>` instantiations for the renderer:

- `Buffer<Colord>` — the [HDR](../appendix/a-glossary.md#h) float framebuffer the renderer
  writes into during the shading pass.
- `Buffer<unsigned int>` — the [LDR](../appendix/a-glossary.md#l) display buffer with packed
  `0x00RRGGBB` pixels, what tonemap and the GUI consume.

Volume V works exclusively with `Buffer<unsigned int>`. The
reasoning: a CV algorithm cares about *what color* a pixel is,
not the dynamic range it came from. By the time the buffer
reaches a CV consumer it has already been tonemapped, clipped
to $[0, 1]$, and packed to 8-bit per channel. Working in that
form keeps the CV algorithms display-faithful — they see the
same pixels the user sees on screen.

The packed-pixel layout matters for one specific reason: the
**target color comparison**. Most CV algorithms in the codebase
work by comparing pixels against a target color. That
comparison is on the packed `unsigned int` form, so it's a
single integer-equality test rather than three floating-point
near-equalities. Cheap and exact.

The pixel-type conversion API from chapter 4 is the bridge.
A `Colord` becomes its packed form via `.rgb()`; a packed pixel
gets read out as `.r()`, `.g()`, `.b()` integer accessors when
needed.

## 22.2 The standard iteration pattern

Every CV helper in `test/helpers/` shares the same
canonical iteration pattern:

```cpp
for (int y = 0; y < buffer.height(); ++y) {
  for (int x = 0; x < buffer.width(); ++x) {
    unsigned int pixel = buffer[y][x];
    if (pixel == targetColor.rgb()) {
      // hit: do something
    }
  }
}
```

Three things make this canonical. First, the outer loop walks
*rows* and the inner loop walks *columns*. The `Buffer<T>` is
a `T**`
([chapter 4 §4.5](../01-foundations/04-color-and-buffers.md#4-5-buffer-t-a-fixed-size-2d-array)),
and `buffer[y]` returns the `y`-th row pointer. Iterating that
row in the inner loop is sequential memory access; iterating
columns in the inner loop would jump rows on every iteration
and pay the cache miss.

Second, the target-color comparison happens *inside* the inner
loop. A CV pass typically only cares about pixels matching some
target — silhouette extraction is "where is the red?", blob
detection is "where are the connected red regions?", shape
classification is "what shape do the red pixels make?" The
inner loop's hot path is the comparison, not the pixel read.

Third, the `unsigned int` form makes the comparison atomic. A
floating-point per-channel comparison would have to choose an
epsilon; the integer comparison is exact, and tonemap rounding
guarantees that two pixels of "the same" color compare equal.

## 22.3 The rendering-to-CV handoff

In the test suite, the typical flow is:

1. Build a small scene: a sphere, a few lights, a default
   camera.
2. Render through the appropriate engine
   (`Raytracer`, `Wireframe`, `Rasterizer`).
3. Take the resulting `Buffer<unsigned int>` as the CV input.
4. Run a classifier or descriptor over the buffer.

The rendering-to-CV contract is implicit but real: the renderer
produces pixels in $\{0, ..., 255\}$ per channel, with
predictable colors based on the scene's materials, and the CV
helpers consume those exact integer values. There is no
intermediate normalization, no resampling, no JPEG round-trip.

This matters because *real* CV — applied to camera-captured
photographs — has to deal with sensor noise, JPEG compression
artifacts, color calibration variations, exposure shifts. The
test-side CV in this codebase has none of those problems. The
algorithms work on synthetic, perfectly-rendered, perfectly-
quantized output. That makes the algorithms *simpler* than
their real-world counterparts (no de-noising, no robust color
matching), but the price is that they wouldn't survive being
fed a real photograph without significant preprocessing.

The roadmap §4.11 work — when it lands — will include the
real-world preprocessing pipeline (denoising, gamma decoding,
white-balance correction). Until then, Volume V documents the
synthetic-output algorithms the test suite actually uses.

## 22.4 The CV helpers shipped today

Three helpers under
[`test/helpers/`](../../../test/helpers/) cover the current
classifier-based test cases:

[`Blob`](../../../test/helpers/Blob.h) — connected-components
flood fill. Given a buffer and a target color, returns a list
of connected regions: each region is the set of pixels with
the target color that are reachable from one another through a
sequence of adjacent (4-neighbor) target-colored pixels.
Useful for *counting objects* in a scene and for measuring
*per-object area*.

[`Silhouette`](../../../test/helpers/Silhouette.h) — outer-edge
sampling. Given a buffer and a target color, returns the
extreme pixels: the leftmost and rightmost target pixel in each
row, plus the topmost and bottommost target pixel in each
column. The point of `Silhouette` is that it works
*identically* for a Raytracer-rendered solid disk (interior
filled with target color) and a [Wireframe](../appendix/a-glossary.md#w)-rendered circle
outline (only the boundary is target color). The interior is
ignored either way; only the outer extremes contribute.

[`ShapeClassifier`](../../../test/helpers/ShapeClassifier.h) —
predicate-style shape recognition. Given a buffer and a target
color, returns boolean answers like `isCircle(...)` and
`isRectangle(...)` based on geometric descriptors of the
silhouette. The descriptors — radial variance and bounding-box
aspect ratio — fall out of the silhouette sample positions
without needing the interior. This is the classifier the
codebase's functional tests
([chapter 7 §7.5](../02-ray-rendering/07-primitives-and-intersection.md#7-5-triangle-moller-trumbore)
mentions one such test) actually use.

Chapter 23 covers `Blob` and `Silhouette` in depth; chapter 24
covers `ShapeClassifier`.

## 22.5 What this chapter does *not* cover

The full classical-CV pillar from
`docs/topics-backlog.md` covers a much wider range of
techniques than the three helpers above:

- **Filtering and convolution.** Box filters, Gaussian
  filters, Sobel and Scharr edge filters, morphological
  operators (erode, dilate, open, close).
- **Edge detection.** Canny, structured-forests, learned-edge.
- **Hough transforms.** Line and circle Hough, gradient
  voting variants.
- **Feature detection and description.** SIFT, SURF, ORB,
  R2D2, learned-descriptor variants.
- **Segmentation.** Region growing, watershed, graph-cut,
  GrabCut.
- **Image quality metrics.** SSIM, PSNR, perceptual metrics.

None of these ship today. They are queued for the §4.11 work,
and Volume V will gain new chapters as they land. For now the
volume covers what the test suite consumes.

## 22.6 Exercises

1. Sketch the loop body for "count the number of red pixels in
   this buffer." Then sketch the loop for "find the bounding
   box of red pixels." Compare the two — what's the same, what
   differs?
2. Predict the cache behavior of the canonical iteration
   pattern (row-major) versus the wrong order (column-major)
   on a 1024×1024 buffer. How many cache misses does each
   pattern incur per "screenful" of pixels, on a CPU with a
   typical 32-byte cache line and `unsigned int` pixels?
3. Suppose you're debugging a CV algorithm that reports a
   silhouette doesn't match the expected shape. What's the
   first thing you'd check about the *renderer*, before
   suspecting the CV algorithm?
4. The `Buffer<unsigned int>` packing is `0x00RRGGBB`. Write
   the integer literal for "pure red" and "pure green". Now
   write a target-color check that succeeds if the pixel is
   *either* red or green. What's the trade-off between writing
   it as one integer comparison vs. two?

## See also

- Volume index:
  [Volume V — Image processing & computer vision](README.md)
- Previous:
  [21. MSAA and attribute interpolation](../04-rasterization/21-msaa-and-attribute-interpolation.md)
- Next:
  [23. Blob analysis and silhouettes](23-blob-analysis-and-silhouettes.md)
- Buffer foundation:
  [4. Color and buffers](../01-foundations/04-color-and-buffers.md)

## Source anchors

<!-- source-anchors -->
- `include/core/Buffer.h`
- `include/core/Color.h`
- `test/helpers/Blob.h`
- `test/helpers/Silhouette.h`
<!-- /source-anchors -->
