# Image processing & computer vision

The codebase's image-processing surface is a small, test-side toolkit:
`Blob`, `Silhouette`, and `ShapeClassifier` under
[`test/helpers/`](../../../test/helpers/). This volume documents those
helpers and the classical-CV ideas they implement.

## Chapters

- [Image buffers and pixel formats](image-buffers-and-pixel-formats.md)
    — `Buffer<T>` revisited as the lingua franca of image
    processing. The hand-off from rendering output to CV input.
- [Blob analysis and silhouettes](blob-analysis-and-silhouettes.md)
    — connected-component flood-fill; 4 vs 8 neighborhoods;
    outer-extreme silhouette extraction.
- [Shape classification](shape-classification.md) —
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
they live on the test side is an implementation detail.

## See also

- Previous: [Rasterization](../rasterization/README.md)
- Next: [Tools & I/O](../tools-and-io/README.md)
