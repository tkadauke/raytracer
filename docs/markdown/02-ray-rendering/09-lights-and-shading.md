# 9. Lights and shading

> **Status:** Stub. Narrative not written yet. Source anchors and
> embed plan are committed below; expect prose to fill in.

## Arc

Point lights, directional lights, the shadow ray, ambient as the
cheap GI hack, why no area lights yet (stochastic shadow sampling
lands with path tracing). Pins the geometric shadow-boundary
contract via the PointLight functional test under
[`test/functional/render/lights/`](../../../test/functional/render/lights/).

## Source anchors

<!-- source-anchors -->
- `include/render/lights/Light.h`
- `include/render/lights/PointLight.h`
- `include/render/lights/DirectionalLight.h`
- `test/functional/render/lights/PointLightTest.cpp`
<!-- /source-anchors -->

## Planned embeds

The Phong lobe widget covers the shading side from chapter 8. New
artifact for this chapter (optional): a static SVG of the
shadow-ray geometry showing umbra / penumbra of a point source.

Plus point-light and directional-light doc renders.

## See also

- Volume index: [Volume II — Ray rendering](README.md)
- Previous: [8. Materials and BRDFs](08-materials-and-brdfs.md)
- Next: [10. Sampling and anti-aliasing](10-sampling-and-anti-aliasing.md)
- Shading function input: [8. Materials and BRDFs](08-materials-and-brdfs.md)
