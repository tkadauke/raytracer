# Sampling and anti-aliasing

A renderer that shoots one ray per pixel produces images with
visible jagged edges, shimmery thin features, and moiré patterns
on regular textures. Those artifacts are collectively called
**aliasing**, and the cure is to evaluate the rendering function
at *multiple* points within each pixel and average the results.
This chapter is about how the codebase generates those points,
why some patterns are better than others, and what stratification
guarantees the test suite pins.

By the end of this chapter you should know:

- the rendering function as an integral over the pixel area, and
  why averaging is the discrete approximation of that integral,
- the three sampler types the codebase ships (regular, jittered,
  random) and what each one trades off,
- the stratification invariant that the jittered sampler
  guarantees absolutely (not statistically),
- the dual API the `Sampler` class exposes — flat 2D sets and
  on-demand streams — and why the thin-lens camera needs the
  stream form,
- the named stream dimensions reserved for time, lens, BSDF,
  light, and continuation sampling, and why those dimensions
  must stay independent.

## <a id="aliasing-as-undersampling"></a>Aliasing as undersampling
Every pixel in a rendered image is the answer to a question:
*what is the average color of the rendering function over this
pixel's area?* Mathematically,

$$
C(x, y) = \int_{\text{pixel}(x, y)} L(\mathbf{u}) \, d\mathbf{u}
$$

where $L(\mathbf{u})$ is the light arriving at sub-pixel point
$\mathbf{u}$. A single-ray-per-pixel renderer evaluates $L$ at
exactly one point — usually the pixel center — and treats the
result as the integral. That works perfectly when $L$ is a
constant across the pixel (a uniform-color surface), tolerably
when $L$ varies smoothly (a slowly-shaded curved surface), and
catastrophically when $L$ varies rapidly (sharp edges, fine
texture detail, distant geometry).

The catastrophic case is *aliasing*. The edge of a sphere
projected onto pixels falls at some sub-pixel position; if the
sample at the pixel center is on the lit side, the whole pixel
is lit; if it's on the dark side, the whole pixel is dark. The
result is the staircased silhouette familiar from any
single-sample render.

The cure is to sample the integral at *multiple* points and
average:

$$
C(x, y) \approx \frac{1}{N} \sum_{i=1}^N L(\mathbf{u}_i)
$$

where the $\mathbf{u}_i$ are sub-pixel points distributed across
the pixel area. This is **Monte Carlo integration** of the pixel
function. The choice of *how* to distribute the $\mathbf{u}_i$
is what distinguishes one sampler from another.

## <a id="the-three-samplers"></a>The three samplers
The codebase ships three concrete samplers. Each takes
`numSamples` and `numSets` as constructor parameters: how many
sub-pixel samples per set, and how many independent sets to
pre-generate. The renderer picks one set at random per pixel and
uses its `numSamples` points for that pixel's evaluations.

The interactive widget compares all three on the same pixel
grid, with adjustable `numSamples` and an optional pixel /
lens / shutter-time dimension toggle:

<!-- widget: sampler_streams -->

[`RegularSampler`](../../../include/render/samplers/RegularSampler.h)
places samples on an evenly-spaced grid: the unit square is
divided into $\sqrt{N} \times \sqrt{N}$ cells, with one sample
at the center of each. This is the textbook regular grid. It
removes most aliasing but introduces *new* artifacts at certain
spatial frequencies — content whose frequency aligns with the
sample grid produces moiré patterns. Regular sampling is the
worst possible choice in the general case, and the best
possible choice for one specific case: when the integrand is a
constant over the pixel area, regular samples produce the same
answer every time. That makes `RegularSampler` the right pick
for the determinism-critical functional tests
(`SamplerDeterminismTest.RegularSamplerProducesBitIdenticalRenders`).

[`JitteredSampler`](../../../include/render/samplers/JitteredSampler.h)
divides the unit square into the same $\sqrt{N} \times \sqrt{N}$
grid as the regular sampler, but each sample's position within
its own grid cell is randomized. This is **stratified sampling**
in its simplest form. The sample in cell $(i, j)$ lives somewhere
in $[i/n, (i+1)/n] \times [j/n, (j+1)/n]$, but never crosses
into a neighbor cell. The randomization breaks the periodic
artifact pattern of the regular sampler while preserving the
even spatial distribution.

[`RandomSampler`](../../../include/render/samplers/RandomSampler.h)
draws all $N$ samples uniformly at random across the unit
square. There is no grid at all. This is the simplest possible
sampler and also the worst — random clumping produces local
oversampling of one region and undersampling of another, so
$N$ random samples produce a noisier estimate than $N$
jittered samples. Random sampling is mostly useful as a baseline
for understanding why stratification matters.

## <a id="the-stratification-invariant"></a>The stratification invariant
The jittered sampler's defining property is one a stratified
sampler can guarantee absolutely: every grid cell receives
*exactly* one sample per set. Not statistically close to one,
not on average one — exactly one. The randomization happens
*within* the cell, never between cells.

This invariant is pinned by the unit test
[`JitteredSampler.EachStratumGetsExactlyOneSamplePerSet`](../../../test/unit/render/samplers/JitteredSamplerTest.cpp).
The test builds a 5×5 stratified sampler with 200 sets, bucketizes
every sample by its grid cell, and asserts that every bucket
holds exactly 200 samples (one per set, summed across all sets).
The exact-equality assertion is what makes the test meaningful:
a "statistically close to 200" assertion would pass for a random
sampler too, because 200 samples drawn uniformly across 25
buckets is 8 expected per bucket per set, with high variance
that random gets very close to over enough sets.

The rendering benefit of this guarantee is concrete: jittered
sampling produces $\mathcal{O}(1/N)$ variance reduction in the
integrated pixel value, where random sampling produces only
$\mathcal{O}(1/\sqrt{N})$. Going from 4 samples to 16 samples
reduces jittered noise by 4×; the same change reduces random
noise by only 2×. The cost is the same — 16 ray casts — and the
output quality differs visibly.

The doc-render sweep on the
[`JitteredSampler`](../../../include/render/samplers/JitteredSampler.h)
class shows the convergence: 1, 4, 9, 16, 25 samples per pixel
on the same scene. The 1-sample render has visible aliasing on
sphere silhouettes; the 16-sample render is essentially clean.

## <a id="two-access-patterns-sets-and-streams"></a>Two access patterns: sets and streams
A `Sampler` exposes its samples through two complementary APIs.

The **set-based** API is `sampleSet()`: it returns one of the
pre-generated `numSets` flat 2D sample sets, picked at random.
The renderer's primary-ray loop calls this once per pixel and
walks the set, calling `rayForPixel(x + sample.x(), y +
sample.y())` for each. Pinhole, orthographic, and the wide-angle
cameras only need a single 2D dimension (the sub-pixel offset),
and the set-based API is enough for them.

The **stream-based** API is `stream()`: it returns a
`SampleStream` that the consumer pulls dimensions from on
demand. `next2D()` returns the next 2D sample and `next1D()`
returns the next 1D sample; subsequent calls within the same
pixel pull additional dimensions from *independent* sample sets,
so the stream's outputs don't correlate. Thin-lens cameras
([Cameras: Thin-lens: depth of field](cameras.md#thin-lens-depth-of-field))
need this — they consume one dimension for the pixel offset and
*another* dimension for the lens-disc sample, and using
correlated samples for both would produce visible artifacts in
defocus blur. Future path tracers will pull even more
dimensions per ray ([BRDF](../appendix/a-glossary.md#b) importance sampling, light sampling,
Russian-roulette decisions); the stream API scales to that
without forcing every consumer to know about every other
consumer's dimensional needs. `sharedStream()` produces the same
sequence as `stream()` for render paths that retain primary-ray
sample streams until a later batch integrator runs; wavefront uses
that retained form to avoid wrapping a freshly allocated unique
stream in a second shared allocation.

## <a id="named-stream-dimensions"></a>Named stream dimensions
Sequential `next1D()` / `next2D()` pulls are useful for cameras:
the camera can ask for the next stochastic input in the order its
ray-construction code naturally needs it. Recursive integrators
need a stricter ownership rule. A path tracer should not get a
different BSDF sample merely because the implementation asked the
light sampler first, skipped light sampling for a delta lobe, or
added Russian roulette later.

[`SampleStream`](../../../include/render/samplers/SampleStream.h)
therefore exposes named dimensions:

| Dimension | Numeric ownership | Intended consumer |
|---|---:|---|
| `SampleDimension::Pixel` | 0 | sub-pixel sample |
| `SampleDimension::Time` | 1 | shutter-time sample for motion blur |
| `SampleDimension::Lens` | 2 | aperture sample for thin-lens cameras |
| `SampleDimension::BSDF` | `3 + bounce * 3` | direction sample for BSDF importance sampling |
| `SampleDimension::Light` | `4 + bounce * 3` | light-surface or light-selection sample |
| `SampleDimension::Continuation` | `5 + bounce * 3` | path-continuation / Russian-roulette sample |

The `sample2D(dimension, index)` and `sample1D(dimension, index)`
methods read those dimensions without advancing the sequential
cursor. The index is normally the path bounce. Bounce 0's BSDF and
light samples occupy dimensions 3 and 4; bounce 1's occupy 6 and 7.
The unit tests pin this exact mapping in
[`SamplerStream.NamedDimensionsMatchLegacyCameraDimensionOrder`](../../../test/unit/render/samplers/SamplerTest.cpp)
and
[`SamplerStream.PathTracingDimensionsDoNotReuseTheSamePattern`](../../../test/unit/render/samplers/SamplerTest.cpp).

The independence requirement is not bookkeeping neatness; it is a
Monte Carlo correctness issue. If two estimators reuse the same 2D
pattern, their errors become correlated. A glossy BSDF sample and a
light sample might both prefer the same corner of their domains, or
a lens sample might line up with a pixel-offset sample and draw a
structured blur pattern. The default `Sampler::stream(sampleIndex,
pixelHash)` implementation avoids that by looking up dimension `d`
in pre-baked set `(pixelHash + d) mod numSets` at the same
`sampleIndex`. For jittered sampling, that means every named
dimension still receives a stratified point, but it receives it from
a different set. The `pixelHash` term shifts those set choices per
pixel so the whole image does not share one visible set pattern.

This is foundation API, not a completed path tracer. The shipped
renderer currently consumes pixel, time, and lens dimensions for
primary rays, motion blur, and thin-lens depth of field. BSDF,
light, and continuation dimensions are reserved and tested so a
future path-tracing integrator can use them without redefining the
sampler contract.

## <a id="the-thin-lens-sampler-interaction"></a>The thin-lens / sampler interaction
[Cameras](cameras.md) introduced the thin-lens camera and noted that it
*requires* a multi-sample sampler to produce visible defocus
blur. This is the chapter where that requirement gets explained.

A thin-lens camera draws a fresh point on its aperture disc per
ray, and combines that with the pixel offset to construct the
ray. With one sample per pixel, there is one aperture point and
one pixel offset per pixel — the camera produces a *single ray*
per pixel, just as a pinhole would, but with the ray's origin
shifted slightly off the camera position. The result is a
shifted-but-sharp pinhole render. No averaging happens, so no
defocus blur appears.

With 16 samples per pixel, the camera draws 16 different
aperture points per pixel, each combined with a different
pixel offset. A surface point at the focal distance receives
contributions from all 16 rays that all converge to the same
world-space point, so the surface is sharp. A surface *off*
the focal plane receives contributions from 16 rays that hit
16 different world-space points, and the average is the
defocus blur.

The lens-vs-pixel-offset independence is what the stream API
provides. If the camera used `sampleSet()` for both
dimensions, the lens samples and the pixel offsets would be
correlated within a single pixel, and the defocus blur would
look weirdly patterned at high samples.

## <a id="determinism"></a>Determinism
Sampler determinism shows up in two places where it actually
matters.

Test reproducibility — the
[`SamplerDeterminismTest.RegularSamplerProducesBitIdenticalRenders`](../../../test/functional/render/samplers/SamplerDeterminismTest.cpp)
test renders the same scene twice and asserts byte-for-byte
identical output. This works because the regular sampler's
`generateSet()` is purely deterministic — no randomness — and
the renderer's iteration is deterministic given a fixed view
plane, so the same scene produces the same pixels every run.
Useful for catching regressions that subtly perturb shading
behavior.

Random sampler reproducibility — the
[`RandomSampler.ExplicitSetupSeedProducesIdenticalSets`](../../../test/unit/render/samplers/RandomSamplerTest.cpp)
test builds two random samplers with the same explicit setup seed
and asserts that every generated set matches. This scoped setup
seed restores the caller's random stream afterward, so a
test-only reproducibility hook does not perturb unrelated code.
The sampler's reproducibility hinges on the thread-local PCG32
stream in
[`Number.h`](../../../include/core/math/Number.h); `std::srand`
does not affect renderer sampling. Outside controlled tests, each
thread owns its own entropy-seeded default stream, so render
workers do not contend on a shared global generator and production
noise is not accidentally locked to one process-wide pattern.

Render reproducibility — the raytracer can opt into a root sampling
seed for tests that need stochastic samples but still require
byte-identical output. [`SamplingSeed`](../../../include/render/SamplingSeed.h)
derives ownership hierarchically: a render seed produces a tile
seed, the tile seed produces per-pixel seeds, and per-pixel seeds
produce per-sample seeds for future path-tracing dimensions. The
current camera path uses the seeded pixel value to choose stable
sample-stream sets; the sample-level hook is present so future BSDF,
light, and continuation samples have the same deterministic root.

Determinism interacts with the cancellation hook from
[The Whitted pipeline: The `RenderEngine` abstraction](the-whitted-pipeline.md#the-renderengine-abstraction):
a cancelled-and-restarted render of the same scene produces the
same pixels (regular) or the same noise pattern (random with the
same seed) as the first attempt would have, because the
sampler's per-pixel set choice is deterministic given the
sampler's `numSets`.

## <a id="picking-a-sampler"></a>Picking a sampler
For the final-render output of a [Whitted](../appendix/a-glossary.md#w) scene, **jittered**
is the right default. The stratification guarantee gives the
$\mathcal{O}(1/N)$ variance reduction over random; the
randomization inside cells avoids the regular sampler's moiré
patterns; and with 16 samples per pixel the visible noise is
below the threshold of perception for most scenes.

For test scenes that need byte-identical reproducibility,
**regular** is the right pick. The functional-test
`SamplerDeterminismTest` documents this convention; deviating
from it forces tests to use looser comparisons (color counts,
bounding boxes) when exact comparison would be sufficient and
faster.

For experiments and side-by-side baselines, **random** is the
educational choice — it shows what an *unstratified* sampler
looks like, which makes the stratification benefit visible in
contrast.

## <a id="what-this-chapter-does-not-cover"></a>What this chapter does *not* cover
Many advanced sampling topics are queued under
`docs/topics-backlog.md` §A. Notably absent here:

- **Low-discrepancy sequences** (Halton, Sobol, Niederreiter).
  These are deterministic sample distributions that beat
  jittered's variance reduction in moderate dimensions but are
  much trickier to implement well. A path tracer would benefit
  from them.
- **Blue-noise sampling**. Spatial blue-noise distributions
  produce visually pleasing noise patterns where individual
  noise grains are distributed evenly without forming patterns
  the eye picks up. Useful at very low sample counts where
  jittered's grid pattern is itself visible.
- **Russian roulette and splitting**. Path-termination
  heuristics that don't apply to the bounded-recursion Whitted
  renderer.

All of these land with the path tracer. For a Whitted renderer
the three samplers above are sufficient.

## <a id="exercises"></a>Exercises
1. Predict what happens to the variance of an integration
   estimator if you run a regular sampler at 16 samples per
   pixel on a textured surface whose checker frequency exactly
   matches the sample grid. Now predict the same for a jittered
   sampler.
2. The thin-lens camera, with a single-sample sampler, produces
   a "shifted-but-sharp" pinhole render. What does *shifted*
   mean here, and how does the shift change between two
   single-sample renders of the same scene?
3. Read `JitteredSampler::generateSet`. The implementation
   randomizes a sample's position within its grid cell, but
   the cells are visited in deterministic order. What happens if
   you also randomize the order? Does the stratification
   guarantee survive?
4. The `SampleStream` API was introduced specifically for
   thin-lens cameras. What other consumers (current or future)
   would benefit from independent sample dimensions?

## See also

- Volume index: [Ray rendering](README.md)
- Previous: [Lights and shading](lights-and-shading.md)
- Next: [Textures](textures.md)
- Stream-API consumer:
  [Thin-lens: depth of field](cameras.md#thin-lens-depth-of-field)
- Set-API consumer:
  [MSAA and attribute interpolation](../rasterization/msaa-and-attribute-interpolation.md)
- Stratification unit test:
  [`test/unit/render/samplers/JitteredSamplerTest.cpp`](../../../test/unit/render/samplers/JitteredSamplerTest.cpp)
- Determinism functional test:
  [`test/functional/render/samplers/SamplerDeterminismTest.cpp`](../../../test/functional/render/samplers/SamplerDeterminismTest.cpp)

## Source anchors

<!-- source-anchors -->
- `include/render/samplers/Sampler.h`
- `include/render/SamplingSeed.h`
- `include/render/samplers/SamplerFactory.h`
- `include/render/samplers/RegularSampler.h`
- `include/render/samplers/JitteredSampler.h`
- `include/render/samplers/RandomSampler.h`
- `include/render/samplers/SampleStream.h`
- `test/unit/render/samplers/SamplerTest.cpp`
- `test/unit/render/samplers/JitteredSamplerTest.cpp`
- `test/unit/render/samplers/RandomSamplerTest.cpp`
- `test/functional/render/samplers/SamplerDeterminismTest.cpp`
<!-- /source-anchors -->
