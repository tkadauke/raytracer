# 10. Sampling and anti-aliasing

> **Status:** Stub. Narrative not written yet. Source anchors and
> embed plan are committed below; expect prose to fill in.

## Arc

Why one ray per pixel produces aliasing, regular vs jittered vs
random samplers as the three classical answers, the stratification
guarantee (pinned by the
`JitteredSampler.EachStratumGetsExactlyOneSamplePerSet` unit test),
multi-sample-per-pixel as a Monte Carlo integral over pixel area.
Connects to chapter 6 via the lens sampler reuse.

## Source anchors

<!-- source-anchors -->
- `include/render/samplers/Sampler.h`
- `include/render/samplers/SamplerFactory.h`
- `include/render/samplers/RegularSampler.h`
- `include/render/samplers/JitteredSampler.h`
- `include/render/samplers/RandomSampler.h`
- `include/render/samplers/SampleStream.h`
- `test/unit/render/samplers/JitteredSamplerTest.cpp`
- `test/unit/render/samplers/RandomSamplerTest.cpp`
- `test/functional/render/samplers/SamplerDeterminismTest.cpp`
<!-- /source-anchors -->

## Planned embeds

<!-- widget: sampler_streams -->

Plus sampler doc renders (low-vs-high-spp comparison).

## See also

- Volume index: [Volume II — Ray rendering](README.md)
- Previous: [9. Lights and shading](09-lights-and-shading.md)
- Next: [11. Textures](11-textures.md)
- Sampler consumer: [6. Cameras](06-cameras.md) (thin-lens disc sampling)
- Sampler consumer: [21. MSAA and attribute interpolation](../04-rasterization/21-msaa-and-attribute-interpolation.md)
