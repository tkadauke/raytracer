# Lights and shading

A material knows the surface color at a hit point, but it
doesn't know how brightly that surface is lit. The lights in
the scene supply the missing piece. This chapter describes the
three light types the codebase ships, the shadow-ray test that
makes shadow boundaries visible, and the ambient-light hack
that approximates global illumination cheaply.

By the end of this chapter you should know:

- the abstract `Light` interface and the two virtual methods
  every concrete light implements,
- the sampling/PDF metadata a Monte Carlo integrator can ask from
  a light without changing the current Whitted direct-lighting path,
- how finite light sources can publish derived emitter geometry while
  keeping light sampling on the light interface,
- how a point light differs physically and computationally from
  a directional light,
- how a rectangular area light becomes a non-delta soft-shadow source
  for the path tracer,
- the shadow ray as the test for "is this light visible from
  this surface point?",
- the geometry that places a shadow boundary precisely where it
  lands,
- and the ambient-light convention that gives every surface a
  baseline brightness even where direct light doesn't reach.

## <a id="the-light-interface"></a>The `Light` interface
The base class is
[`render::Light`](../../../include/render/lights/Light.h). It
exposes two virtuals:

```cpp
// include/render/lights/Light.h
virtual Vector3d direction(const Vector3d& point) const = 0;
virtual Colord   radiance() const = 0;
```

`direction(point)` returns the unit vector from the surface
point toward the light. For a point light this varies with the
surface position; for a directional light it is the same vector
everywhere in the scene. The return value is unit-length, in
keeping with the
[Numbers and vectors: Length, normalization, and the unit-length invariant](../foundations/numbers-and-vectors.md#length-normalization-and-the-unit-length-invariant)
convention.

`radiance()` returns the color and intensity arriving along
that direction. The convention is that intensity is folded into
the color components — a "100 W bulb" is a `Colord(100, 100,
100)`, not a separate brightness scalar. Materials simply
multiply their [BRDF](../appendix/a-glossary.md#b) result by the radiance and the geometric
cosine, with no further scaling.

The same base class now also exposes a sampling contract for
future Monte Carlo direct-lighting estimators:

```cpp
// include/render/lights/Light.h
struct LightSample {
  Vector3d direction;
  Colord   radiance;
  double   distance;
  double   pdf;
  bool     delta;
};

virtual LightSample sample(const Vector3d& point) const;
virtual double pdf(const Vector3d& point, const Vector3d& direction) const;
virtual bool isDelta() const;
virtual Colord emission() const;
virtual std::optional<Colord> power() const;
virtual std::shared_ptr<render::Primitive> emitterPrimitive() const;
```

`sample(point)` returns one direction from the shaded point toward
the light, the radiance arriving from that draw, the distance to the
sampled emitter when finite, the PDF of the draw, and whether the
sample is a delta event. `pdf(point, direction)` evaluates the
ordinary solid-angle density for a direction chosen by some other
sampler, such as a BSDF sample in an MIS estimator. `emission()` and
`power()` are metadata for light-selection heuristics and future
many-light sampling; `radiance()` remains the value used by the
current material loops. `emitterPrimitive()` is optional derived
geometry for finite lights that should be visible to camera,
reflection, and refraction rays. Delta and infinite lights return
null.

Lights live on a separate list off
[`Scene::lights()`](../../../include/render/primitives/Scene.h),
not in the geometric primitive tree. The shading pipeline
iterates the light list independently of the [BVH](../appendix/a-glossary.md#b) traversal that
finds the hit. Finite emitters complicate that clean separation:
they are still lights for sampling, but they may also publish a
geometric proxy. [`Scene::addLight(...)`](../../../src/render/primitives/Scene.cpp)
attaches that proxy to the primitive tree and keeps the original
light on the light list. This makes a rectangular area light visible
in primary rays and specular paths without making materials or
integrators switch on concrete light types.

## <a id="point-lights"></a>Point lights
[`PointLight`](../../../include/render/lights/PointLight.h)
models an infinitesimal light source at a fixed world-space
position — a single point in space radiating equally in every
direction. The `direction(point)` implementation is the
straight-line vector from the surface point to the light's
position, normalized:

$$
\mathbf{l} = \frac{\mathbf{p}_L - \mathbf{p}_S}{\lVert \mathbf{p}_L - \mathbf{p}_S \rVert}
$$

where $\mathbf{p}_L$ is the light's position and $\mathbf{p}_S$
is the surface point.

`radiance()` returns the constructor's color regardless of
where the surface is. This is *not* physically accurate — a
real point source's intensity at a given surface falls off as
$1/r^2$ with distance, the inverse-square law. The codebase
deliberately omits the falloff because it makes scene authoring
much harder: a point light bright enough to illuminate the far
side of a room would burn out the near side, and tuning that
balance pixel-by-pixel is a path-tracing problem, not a
[Whitted](../appendix/a-glossary.md#w)-renderer problem. This renderer leaves distance falloff out.

## <a id="directional-lights"></a>Directional lights
[`DirectionalLight`](../../../include/render/lights/DirectionalLight.h)
models a light source infinitely far away, radiating in one
fixed direction throughout the scene. The sun is the canonical
example: the sun is so far away that, for a small Earth-bound
scene, every shadow ray pointing toward it is *parallel* to
every other shadow ray pointing toward it. There is no
position; there is only a direction.

The `direction(point)` implementation returns the same unit
vector for every input position — the constructor's stored
direction (negated, since the convention is "direction from
surface to light," not "direction the light is shining"). This
makes directional-light shading effectively free compared to
point-light shading: no normalization per surface point, no
position lookup.

Visually, directional lights produce parallel shadow edges and
even illumination across the scene. Point lights produce
shadows that radiate outward from the light's position.

## <a id="rectangular-area-lights"></a>Rectangular area lights
[`RectangularAreaLight`](../../../include/render/lights/RectangularAreaLight.h)
models a one-sided rectangular emitter with constant radiance. The rectangle is
defined by its center and two full edge vectors:

$$
\mathbf{p}_L(u, v) =
\mathbf{c} + (u - 0.5)\mathbf{e}_u + (v - 0.5)\mathbf{e}_v
$$

with $u$ and $v$ in $[0, 1)$. The emitting side is the side pointed to by
$\mathbf{e}_u \times \mathbf{e}_v$. Samples from the back side return black
radiance and a zero PDF because the light is one-sided.

The runtime light also publishes a matching
[`Rectangle`](../../../include/render/primitives/Rectangle.h) through
`emitterPrimitive()`. That rectangle carries an
[`EmissiveMaterial`](../../../include/render/materials/EmissiveMaterial.h):
`shade(...)` returns the emitter radiance for Whitted-style camera rays, and
`emittedRadiance(...)` gives path tracers the hit-light term before BSDF
sampling. The material emits only in the normal-facing hemisphere, matching the
light sampler's one-sided convention.

Unlike point and directional lights, a rectangular area light is not a delta
distribution. The path tracer passes a sampler-owned `SampleDimension::Light`
value to `sample(point, sample2D)`, maps that sample to a point on the
rectangle, and converts the uniform area PDF to a solid-angle PDF:

$$
p_\omega = \frac{r^2}{\cos\theta_L A}
$$

where $r$ is the distance from the shaded point to the sampled point on the
emitter, $\theta_L$ is the angle between the light normal and the direction
back toward the shaded point, and $A$ is the rectangle area. That finite extent
is what produces soft shadow penumbrae in the path tracer.

## <a id="light-sampling"></a>Light sampling
The light sampling API separates **delta lights** from finite emitters.

A point light is infinitesimal in position. From a shaded point,
there is exactly one direction that reaches it. A directional light
is infinitesimal in direction. From every shaded point, there is
again exactly one light direction. Their `sample(point)` and
`sample(point, sample2D)` methods therefore return deterministic
`LightSample` values with `pdf == 1` and `delta == true`; the 2D
sample is accepted for API uniformity and ignored by delta lights.
Their ordinary `pdf(point, direction)` methods return zero, because
a delta distribution is not an ordinary solid-angle density.

That distinction is what the path tracer's direct-light estimator
uses. [`LightSampler`](../../../include/render/lights/LightSampler.h)
first selects one light from the scene using virtual light metadata:
bounded emitters publish `power()`, unbounded emitters fall back to
`emission()`, and all-zero metadata falls back to uniform selection.
The estimator divides the selected light's contribution by that
selection PDF, so the result stays unbiased while avoiding a shadow ray
to every light at every hit.

When the selected light is a delta light, the integrator uses the
sampled event directly and gives it MIS weight 1; the competing BSDF
sampler cannot hit an infinitely small point or direction with nonzero
probability. For non-delta lights — the shipped rectangular area light
today, and later spheres, environment maps, and mesh emitters —
`sample(point, sample2D)` maps the caller-owned 2D sample to an emitter
location or direction and returns a non-delta PDF. The path tracer
passes the selected light's `SampleDimension::Light` slot into this
overload during next-event estimation, then combines the light PDF with
the material's `bsdfPdf(...)` through the MIS helper. The
integrator uses the same rule in the other direction: when a non-delta
BSDF continuation ray hits visible emitter geometry, the emitted radiance
is weighted against `LightSampler::pdf(...)`, which includes both the
selected light's PDF and the light-selection probability. Camera-visible
emitters and specular/delta continuations keep weight 1 because the
competing continuous estimator cannot sample those events. This
integrator-side weighting and sample-ownership contract is shared by all
non-delta emitters.

The important boundary is capability: these methods document how
lights can be sampled, not that the Whitted renderer already performs
soft shadows. The shipped Whitted materials still iterate
`Scene::lights()`, sample each light, and cast the hard shadow ray
described below; stochastic light integration belongs to the path
tracer.

## <a id="the-shadow-ray"></a>The shadow ray
The shading routine in
[Materials and BRDFs: The five shipped materials](materials-and-brdfs.md#the-five-shipped-materials)
tests whether each light is *visible* from the hit point before
adding its contribution. The test is a single boolean ray cast
from the surface point toward the light:

```cpp
LightSample sample = light->sample(hitPoint.point());
Rayd shadowRay = Rayd(hitPoint.point(), sample.direction).epsilonShifted();

if (scene.occludes(shadowRay, state, sample.distance)) {
  // shadow: this light is blocked by an occluder
} else {
  // visible: add the light's contribution to the surface color
}
```

The ray is `epsilonShifted` — its origin is pushed forward
along the direction by `Ray::epsilon` to prevent the ray from
spuriously self-intersecting the surface it just bounced off of
(see
[Rays and geometry: The `Ray` class, all of it](../foundations/rays-and-geometry.md#the-ray-class-all-of-it)).
`scene.occludes(...)` keeps the shadow test bounded for finite point
lights, so geometry behind the light does not accidentally shadow the
surface. Directional lights pass an infinite distance and keep the old
"any hit along the ray" behavior. Internally, the finite-distance path uses
`scene.intersect(...)` to compare the nearest hit distance against the light
sample's distance; the infinite path uses `scene.intersects(...)`, the cheaper
boolean form from
[Primitives and intersection: The `Primitive` interface](primitives-and-intersection.md#the-primitive-interface),
because an unbounded directional shadow ray only needs to know *whether* it
hits anything, not *where*.

When a finite light has visible emitter geometry, the bounded check deliberately
leaves a small tolerance at the far endpoint. A shadow ray to an area-light
sample is epsilon-shifted away from the shaded surface and therefore reaches
the light-card geometry just before the original unshifted sample distance.
That terminal hit is the light itself, not an occluder, so it must not shadow
its own direct-light contribution.

The shadow ray is the single biggest cost the shading pipeline
pays. A scene with $L$ lights spawns $L$ shadow rays per ray
hit; with $N$ primary rays per pixel and $D$ recursion levels
per primary ray, the total ray-cast count is approximately $N
\cdot D \cdot (1 + L)$ rays per pixel. This is why the
boolean form exists — knowing only "is anything in the way?"
is enough information for shadow visibility, and a routine that
returns at the first hit it finds (rather than the closest one)
is markedly faster than the full geometric form.

## <a id="the-geometric-shadow-boundary"></a>The geometric shadow boundary
Every surface point is either fully lit by a given light or
fully unlit, with no in-between. There is no soft shadow,
because a point source has no extent — it either reaches the
surface point in a straight line or it doesn't. The boundary
between lit and unlit regions is the **shadow boundary**, and
it lives exactly where the line from the light through the edge
of the occluder lands on the receiving surface.

For a unit sphere at $(0, 1, 0)$ occluding a point light at
$(0, 5, 0)$ over a floor plane at $y = -3$, the shadow
boundary is computed from the tangent line from the light to
the sphere:

- The light-to-sphere-center distance is 4 (from $(0, 5, 0)$ to
  $(0, 1, 0)$).
- The sphere radius is 1.
- The tangent angle $\theta$ from the light satisfies $\sin
  \theta = 1/4$.
- The tangent line intersects the floor at $y = -3$, eight
  units below the light.
- The boundary radius on the floor is $8 \tan(\arcsin(1/4)) =
  8 / \sqrt{15} \approx 2.07$.

This contract is pinned by
[`PointLightTest.ShadowBoundaryFallsAtGeometricallyPredictedLocation`](../../../test/functional/render/lights/PointLightTest.cpp),
which samples just inside ($x = 1.8$) and just outside
($x = 2.4$) the predicted boundary and asserts the inside
pixel is fully dark while the outside pixel carries
non-zero red from the lit floor. The test is a useful
mental model: the shadow boundary is a geometric prediction
the renderer must reproduce exactly, not an approximation.

The same logic applied to a directional light produces a
parallel-edged shadow: the tangent rays are all parallel to the
light direction, and the boundary is the silhouette of the
occluder projected onto the receiving surface along the light
direction. Long, sharp shadows from a sun-style light follow
directly from this.

## <a id="ambient-as-cheap-gi"></a>Ambient as cheap GI
A surface in *true* shadow — completely blocked from every
light source — is fully dark in the math from [The shadow ray](#the-shadow-ray).
In real
life, surfaces in shadow aren't fully dark: bounced light from
nearby surfaces still reaches them, producing a soft fill that
keeps shadows readable.

The "right" way to compute this is **global illumination**:
trace many bounces of light, accumulate the indirect
contributions, get the soft shadow fill for free. That's what
a path tracer does. The Whitted renderer doesn't do that, and
adding it would require a different rendering algorithm.

The Whitted renderer's substitute is the **scene ambient**.
Every surface receives a uniform background contribution from
`Scene::ambient()`, multiplied by the material's ambient
coefficient (which scales with the surface's response, like its
diffuse color). This produces a baseline brightness that fills
the otherwise-black shadow regions. The math is the term in
[Materials and BRDFs: The five shipped materials](materials-and-brdfs.md#the-five-shipped-materials):

$$
L_{\text{ambient}} = k_a \, f_d \, L_{\text{scene-ambient}}
$$

where $k_a$ is the material's ambient coefficient, $f_d$ is the
diffuse BRDF (reduces to the diffuse color divided by $\pi$
for [Lambertian](../appendix/a-glossary.md#l)), and $L_{\text{scene-ambient}}$ is the
scene-level ambient color.

This is not physically correct. A real scene's indirect light
varies per surface point — a corner of a room receives less
indirect light than the middle of an open floor, and a surface
facing a bright wall receives more than a surface facing a
dark one. The ambient term ignores all of that variation and
applies the same fill to every surface.

That said, the cost is exactly zero — one multiply per shaded
pixel — and the result is plausible enough for almost every
scene the renderer is likely to render. When a scene's
ambient is set to black (as it is in the dark-room functional
tests), shadows are properly black; when it's nonzero, shadows
get the cheap fill.

## <a id="shadow-maps-for-the-rasterizer"></a>Shadow maps for the rasterizer
The shadow ray from [The shadow ray](#the-shadow-ray) is the
raytracer's mechanism. The rasterizer's fast path avoids that
per-fragment ray query: rasterization runs over screen-space
triangles, so its scalable direct-light shadow alternative is the
**shadow map**, an opt-in feature on
[`engine::raster::Rasterizer`](../../../include/engine/raster/Rasterizer.h).

The algorithm is a two-pass render. The first pass renders the
scene from each directional light's point of view into a
depth-only buffer — the **shadow map**. Each texel of that
map records the closest surface to the light along that
texel's direction. The second (camera) pass renders the
scene normally, and at each shaded fragment, projects the
fragment's world-space position into the light's coordinate
frame. The fragment's projected depth gets compared against
the stored depth in the shadow map. If the projected depth is
farther than what the map recorded, something else is closer
to the light along that direction — the fragment is in shadow.
If the fragment projects outside its selected shadow-map image,
the rasterizer treats that lookup as lit. PCF taps outside the map
also contribute lit samples, and PCSS taps outside the map do not
count as blockers.

Six knobs control quality versus cost:

- **Map resolution** — how finely the light-space depth
  image samples the scene. Low values quantize shadow edges
  visibly; higher values cost proportionally more raster work
  and memory.
- **Cascade count** — how many camera-depth slices each
  directional light receives. A single map covers the whole
  scene bounds; multiple cascades build tighter light-space
  maps for near and middle view-depth ranges, trading extra
  depth passes for more usable shadow-map detail. Each map is
  fit around its slice in the directional light's basis, and the
  rasterizer snaps each cascade center to the corresponding
  light-space texel grid. That keeps small camera moves from
  shifting the shadow projection by fractional texels.
- **Cascade split blend** — how the cascade boundaries are
  distributed between near and far camera depths. A blend of
  0 uses linear splits, so every cascade receives the same
  amount of depth. A blend of 1 uses logarithmic splits, so
  more cascade resolution stays near the camera. The default
  practical blend is 0.5.

<!-- widget: rasterizer_shadow_cascades -->

- **Bias** — additive depth tolerance for the comparison.
  Constant bias applies the same light-space depth offset to
  every receiver. Slope-scaled bias adds angle-dependent tolerance
  for receivers that turn away from the light, where one shadow-map
  texel covers more depth. Too little bias lets a surface shadow
  itself due to depth quantization (the classic *shadow acne*);
  too much bias detaches the shadow from the casting object (the
  classic *Peter Panning*).
- **PCF radius** — percentage-closer filtering. The
  comparison runs over a $(2r + 1) \times (2r + 1)$ kernel of
  neighboring texels and averages the results, producing a
  soft penumbra around hard shadow edges. Radius 0 is the
  exact nearest-texel comparison; radius 1 uses a 3×3 kernel,
  radius 4 a 9×9 kernel.
- **Filter mode** — fixed PCF or blocker-search
  [PCSS](../appendix/a-glossary.md#p). PCSS first searches the
  configured radius for blockers, estimates how far the shaded
  receiver is behind the average blocker in light space, and then
  runs PCF with a receiver-local radius clamped by the configured
  maximum. The result is still a shadow-map approximation, but it
  distinguishes near-contact hard edges from farther soft edges.

The widget below visualizes the two passes side by side. The
left panel is the world: a draggable caster, a draggable
receiver, a directional light. The right panels show the
stored depth map and the receiver's projected-depth-vs-stored-
depth comparison. Move the caster, lower the map resolution,
or crank the bias to see shadow acne and Peter Panning
appear:

<!-- widget: rasterizer_shadow_map -->

The default state ships shadow maps **disabled**, both
because they multiply raster work and because not every scene
benefits — a scene with only ambient lighting has nothing to
cast shadows. Application code that wants shadows turns them on
through the rasterizer's setter API.

Directional lights use the shadow-map path described above. When
shadows are enabled but a light has no directional shadow-map
resource, the rasterizer falls back to a boolean scene-visibility
query for that light. That keeps point-light scenes visually
shadowed in the software preview while point/spot shadow-map
resources remain future graph work.

## <a id="what-this-chapter-does-not-cover"></a>What this chapter does *not* cover
Two important light-related topics are out of scope until
later:

**Area lights.** A real-world light has spatial extent — a
fluorescent tube, a window into a brighter room, a softbox in a
photo studio. The shadow boundary from an area light is *soft*:
some surface points see a fraction of the light source past the
occluder, producing the gradient between fully-lit and
fully-unlit. Computing this requires sampling many points on
the area light per surface hit and averaging the results, which
is a Monte Carlo problem and lives with the path tracer.

**Distance falloff.** The inverse-square law at point lights,
mentioned in [Point lights](#point-lights). Same story — adding it interacts with light
intensity tuning in ways that are easier to handle in a
fully-Monte-Carlo path tracer.

Neither is missing because of an architectural problem; they
just aren't needed for a Whitted renderer that targets
educational scenes. When path tracing lands they ship
together.

## <a id="exercises"></a>Exercises
1. Construct a scene with two point lights at the same color
   and intensity but opposite positions. Place a single sphere
   between them. Predict the shading at the sphere's surface
   point closest to each light. Now place the same sphere
   between two directional lights pointing in opposite
   directions. What's different?
2. Directional-light shadow rays can use `scene.intersects(...)`
   because any hit along the ray blocks the light. Point-light
   shadow rays need `scene.occludes(...)` so they can ignore
   geometry behind the light. Construct a case where the two
   answers differ. Why is the bounded query more expensive in a
   flat primitive list? How does the saving change with the BVH from
   [Spatial acceleration](../scene-structure/spatial-acceleration.md)
   in place?
3. Set `Scene::ambient()` to pure white and render a sphere on
   a black floor with a single directional light. The sphere
   has visibly distinct lit and shadowed regions. Describe what
   the shadowed region looks like, and explain in terms of the
   [Ambient as cheap GI](#ambient-as-cheap-gi) formula.
4. Read the `epsilonShifted` step in the shadow-ray
   construction. Suppose you remove it. Predict the visual
   artifacts in a render of a sphere on a plane with a point
   light directly above. Why those particular artifacts?

## See also

- Volume index: [Ray rendering](README.md)
- Previous: [Materials and BRDFs](materials-and-brdfs.md)
- Next:
  [Sampling and anti-aliasing](sampling-and-anti-aliasing.md)
- Materials consume light contributions:
  [The five shipped materials](materials-and-brdfs.md#the-five-shipped-materials)
- BSDF and MIS contracts:
  [Materials and BRDFs: MIS helper contracts](materials-and-brdfs.md#mis-helper-contracts)
- Shadow-boundary functional contract:
  [`test/functional/render/lights/PointLightTest.cpp`](../../../test/functional/render/lights/PointLightTest.cpp)

## Source anchors

<!-- source-anchors -->
- `include/render/lights/Light.h`
- `include/render/lights/LightSampler.h`
- `include/render/lights/PointLight.h`
- `include/render/lights/DirectionalLight.h`
- `include/render/lights/RectangularAreaLight.h`
- `include/render/materials/EmissiveMaterial.h`
- `include/render/primitives/Scene.h`
- `src/render/primitives/Scene.cpp`
- `test/unit/render/materials/EmissiveMaterialTest.cpp`
- `test/unit/render/lights/PointLightTest.cpp`
- `test/unit/render/lights/DirectionalLightTest.cpp`
- `test/unit/render/lights/LightSamplerTest.cpp`
- `test/unit/render/lights/RectangularAreaLightTest.cpp`
- `test/unit/render/primitives/SceneTest.cpp`
- `test/functional/render/lights/PointLightTest.cpp`
<!-- /source-anchors -->
