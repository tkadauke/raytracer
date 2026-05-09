# 11. Textures

> **Status:** Stub. Narrative not written yet. Source anchors and
> embed plan are committed below; expect prose to fill in.

## Arc

`Texture` as a function `(s, t) → Colord`, constant / checker /
UV-color / image (when it lands), mappings (planar, UV-direct), and
how textures plug into materials.

## Source anchors

<!-- source-anchors -->
- `include/render/textures/Texture.h`
- `include/render/textures/ConstantColorTexture.h`
- `include/render/textures/CheckerBoardTexture.h`
- `include/render/textures/UVColorTexture.h`
- `include/render/textures/mappings/`
<!-- /source-anchors -->

## Planned embeds

<!-- widget: texture_coordinate_mapping -->

Plus checker-texture doc renders.

## See also

- Volume index: [Volume II — Ray rendering](README.md)
- Previous: [10. Sampling and anti-aliasing](10-sampling-and-anti-aliasing.md)
- Next: [12. Tone mapping](12-tone-mapping.md)
- Consumer: [8. Materials and BRDFs](08-materials-and-brdfs.md)
- Rasterizer use: [21. MSAA and attribute interpolation](../04-rasterization/21-msaa-and-attribute-interpolation.md)
