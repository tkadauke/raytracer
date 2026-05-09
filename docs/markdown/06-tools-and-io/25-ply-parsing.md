# 25. PLY parsing

> **Status:** Stub. Narrative not written yet. Source anchors and
> embed plan are committed below; expect prose to fill in.

## Arc

ASCII vs binary PLY, element / property declarations, why this is
the project's only untrusted-input surface and how the LibFuzzer
harness keeps it honest.

## Source anchors

<!-- source-anchors -->
- `src/core/formats/ply/PlyFile.cpp`
- `src/core/formats/ply/PlyElement.cpp`
- `src/core/formats/ply/PlyProperty.cpp`
- `test/unit/core/formats/ply/PlyFileTest.cpp`
- `fuzz/`
<!-- /source-anchors -->

## Planned embeds

(No widgets. PLY is a parser story, not a geometric one.)

## See also

- Volume index: [Volume VI — Tools & I/O](README.md)
- Previous: [24. Shape classification](../05-image-and-vision/24-shape-classification.md)
- Next: [26. The example apps](26-the-example-apps.md)
- Mesh consumer: [17. Tessellation](../04-rasterization/17-tessellation.md)
