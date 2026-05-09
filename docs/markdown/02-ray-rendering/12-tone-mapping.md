# 12. Tone mapping

> **Status:** Stub. Narrative not written yet. Source anchors and
> embed plan are committed below; expect prose to fill in.

## Arc

Why HDR exists, the float framebuffer pipeline (post-R1), Linear /
Reinhard / ACES (Narkowicz fit) as a sweep across compression
strengths. Pins the cross-operator monotonicity contract via
`TonemapMonotonicityTest`.

## Source anchors

<!-- source-anchors -->
- `include/render/tonemap/Tonemap.h`
- `include/render/tonemap/TonemapFactory.h`
- `include/render/tonemap/LinearTonemap.h`
- `include/render/tonemap/ReinhardTonemap.h`
- `include/render/tonemap/AcesTonemap.h`
- `test/functional/render/tonemap/TonemapMonotonicityTest.cpp`
<!-- /source-anchors -->

## Planned embeds

<!-- widget: tonemap_curves -->

Plus tonemap sweep doc renders.

## See also

- Volume index: [Volume II — Ray rendering](README.md)
- Previous: [11. Textures](11-textures.md)
- Next volume: [Volume III — Scene structure](../03-scene-structure/README.md)
- Buffer foundation: [4. Color and buffers](../01-foundations/04-color-and-buffers.md)
