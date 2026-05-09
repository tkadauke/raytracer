# 23. Blob analysis and silhouettes

> **Status:** Stub. Narrative not written yet. Source anchors and
> embed plan are committed below; expect prose to fill in.

## Arc

Connected-component flood-fill, what counts as "connected" (4 vs 8
neighborhood, color-equivalence), silhouette extraction as
outer-extreme sampling. Concrete consumer: the test-side `Blob` /
`Silhouette` helpers replacing the old `ShapeRecognition`.

## Source anchors

<!-- source-anchors -->
- `test/helpers/Blob.h`
- `test/helpers/Blob.cpp`
- `test/helpers/Silhouette.h`
- `test/helpers/Silhouette.cpp`
<!-- /source-anchors -->

## Planned embeds

**New artifact:** `scripts/docs/connected_components.js` — load a
small example raster, step through BFS flood-fill with a speed
slider, color components by ID. *(Lands with this chapter.)*

`<!-- widget: connected_components -->` once the widget exists.

## See also

- Volume index: [Volume V — Image processing & computer vision](README.md)
- Previous: [22. Image buffers and pixel formats](22-image-buffers-and-pixel-formats.md)
- Next: [24. Shape classification](24-shape-classification.md)
- Buffer vocabulary: [22. Image buffers and pixel formats](22-image-buffers-and-pixel-formats.md)
