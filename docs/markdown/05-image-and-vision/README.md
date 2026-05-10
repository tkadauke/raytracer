# Volume V — Image processing & computer vision

The codebase's image-processing surface today is a small,
test-side toolkit: `Blob`, `Silhouette`, and `ShapeClassifier`
under [`test/helpers/`](../../../test/helpers/). Per
[`docs/roadmap.md` §4.11](../../roadmap.md), the planned
direction is a full classical-CV library shared between tests
and renderer (denoising, edge detection, blob analysis, shape
descriptors, Hough transforms, image quality metrics). This
volume documents what exists; new chapters land as §4.11 ships.

## Chapters

22. [Image buffers and pixel formats](22-image-buffers-and-pixel-formats.md)
    — `Buffer<T>` revisited as the lingua franca of image
    processing. The hand-off from rendering output to CV input.
23. [Blob analysis and silhouettes](23-blob-analysis-and-silhouettes.md)
    — connected-component flood-fill; 4 vs 8 neighborhoods;
    outer-extreme silhouette extraction.
24. [Shape classification](24-shape-classification.md) —
    silhouette descriptors (radial variance, BB aspect ratio);
    the `isCircle` / `isRectangle` predicates the test suite
    consumes.

## Why test helpers deserve textbook chapters

The helpers under `test/helpers/` encode classical-CV
decisions (BFS flood-fill, radial variance, aspect-ratio
gating) with documented thresholds. The decisions are the same
ones a production CV library would make on the same inputs,
and they are concrete enough — exact thresholds, exact
predicates — to teach the underlying technique honestly. That
they currently live on the test side is an implementation
detail; the algorithms transfer unchanged when the §4.11
library lands.

## See also

- Previous: [Volume IV — Rasterization](../04-rasterization/README.md)
- Next: [Volume VI — Tools & I/O](../06-tools-and-io/README.md)
