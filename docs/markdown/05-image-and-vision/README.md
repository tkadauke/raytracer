# Volume V — Image processing & computer vision

The newer pillar of the codebase. Today it's a small, test-side
toolkit (`Blob`, `Silhouette`, `ShapeClassifier` under
[`test/helpers/`](../../../test/helpers/)) that replaced an earlier
hand-rolled "is this rendered output a circle?" heuristic. Tomorrow
— per
[`docs/roadmap.md` §4.11](../../roadmap.md) — it grows into a full
classical-CV library used by both tests and renderer (denoising, edge
detection, blob analysis, shape descriptors, Hough transforms, image
quality metrics).

This volume documents what exists today; new chapters land as
§4.11 ships.

## Chapters

22. [Image buffers and pixel formats](22-image-buffers-and-pixel-formats.md)
    — `Buffer<T>` revisited as the lingua franca of image
    processing. The hand-off from rendering output to CV input.
23. [Blob analysis and silhouettes](23-blob-analysis-and-silhouettes.md)
    — connected-component flood-fill; 4 vs 8 neighborhoods; outer-
    extreme silhouette extraction. Includes a new interactive
    flood-fill widget.
24. [Shape classification](24-shape-classification.md) — silhouette
    descriptors (radial variance, BB aspect ratio); the `isCircle`
    / `isRectangle` predicates the test suite actually uses.
    Includes a new draggable-polygon descriptor widget.

## Why "test helpers" deserve textbook chapters

The test side is where the codebase first grew CV pretty seriously.
The helpers under `test/helpers/` aren't toys — they replaced a
heuristic that was silently saying "yes this is a circle" for
lemons, diamonds, and torus side-views, and the misclassifications
were hidden from the test reporter for a long time. The new helpers
encode actual classical-CV decisions (BFS flood-fill, radial
variance, aspect-ratio gating) and pin them with documented
thresholds. That makes them legitimate teaching surfaces, even
though they only exist on the test side today.

When the §4.11 library lands, much of this volume's content will be
shared — production code on one side, test helpers on the other.

## See also

- Previous: [Volume IV — Rasterization](../04-rasterization/README.md)
- Next: [Volume VI — Tools & I/O](../06-tools-and-io/README.md)
