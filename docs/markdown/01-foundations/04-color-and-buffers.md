# 4. Color and buffers

> **Status:** Stub. Narrative not written yet. Source anchors and
> embed plan are committed below; expect prose to fill in.

## Arc

`Colord` as four floats, the linear-RGB-vs-sRGB distinction, HDR vs
LDR, `Buffer<T>` as the memory backing for a 2D image, packed-pixel
conversion (`Colord::rgb()` to `unsigned int`). Sets up the
framebuffer end of the rendering pipeline.

## Source anchors

<!-- source-anchors -->
- `include/core/Color.h`
- `include/core/Buffer.h`
- `include/core/color/sse3/`
<!-- /source-anchors -->

## Planned embeds

<!-- widget: color_model_conversions -->

## See also

- Volume index: [Volume I — Foundations](README.md)
- Previous: [3. Rays and geometry](03-rays-and-geometry.md)
- Next: [5. The Whitted pipeline](../02-ray-rendering/05-the-whitted-pipeline.md)
- Picked up by: [12. Tone mapping](../02-ray-rendering/12-tone-mapping.md), [22. Image buffers and pixel formats](../05-image-and-vision/22-image-buffers-and-pixel-formats.md)
