# 24. Shape classification

> **Status:** Stub. Narrative not written yet. Source anchors and
> embed plan are committed below; expect prose to fill in.

## Arc

From silhouette descriptors (radial variance, bounding-box aspect
ratio) to predicates (`isCircle`, `isRectangle`). The decision
boundaries the codebase actually ships with, why those thresholds,
what fails them.

## Source anchors

<!-- source-anchors -->
- `test/helpers/ShapeClassifier.h`
- `test/helpers/ShapeClassifier.cpp`
- `test/helpers/Silhouette.h`
<!-- /source-anchors -->

## Planned embeds

**New artifact:** `scripts/docs/shape_descriptors.js` — draggable
polygon → live readout of radial variance, BB aspect ratio, and
classifier output. *(Lands with this chapter.)*

`<!-- widget: shape_descriptors -->` once the widget exists.

## See also

- Volume index: [Volume V — Image processing & computer vision](README.md)
- Previous: [23. Blob analysis and silhouettes](23-blob-analysis-and-silhouettes.md)
- Next volume: [Volume VI — Tools & I/O](../06-tools-and-io/README.md)
- Silhouette source: [23. Blob analysis and silhouettes](23-blob-analysis-and-silhouettes.md)
