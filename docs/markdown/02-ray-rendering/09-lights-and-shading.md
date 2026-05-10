# 9. Lights and shading

A material knows the surface color at a hit point, but it
doesn't know how brightly that surface is lit. The lights in
the scene supply the missing piece. This chapter describes the
two light types the codebase ships, the shadow-ray test that
makes shadow boundaries visible, and the ambient-light hack
that approximates global illumination cheaply.

By the end of this chapter you should know:

- the abstract `Light` interface and the two virtual methods
  every concrete light implements,
- how a point light differs physically and computationally from
  a directional light,
- the shadow ray as the test for "is this light visible from
  this surface point?",
- the geometry that places a shadow boundary precisely where it
  lands,
- and the ambient-light convention that gives every surface a
  baseline brightness even where direct light doesn't reach.

## 9.1 The `Light` interface

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
[chapter 1 §1.6](../01-foundations/01-numbers-and-vectors.md#1-6-length-normalization-and-the-unit-length-invariant)
convention.

`radiance()` returns the color and intensity arriving along
that direction. The convention is that intensity is folded into
the color components — a "100 W bulb" is a `Colord(100, 100,
100)`, not a separate brightness scalar. Materials simply
multiply their [BRDF](../appendix/a-glossary.md#b) result by the radiance and the geometric
cosine, with no further scaling.

Lights live on a separate list off
[`Scene::lights()`](../../../include/render/primitives/Scene.h),
not in the geometric primitive tree. The shading pipeline
iterates the light list independently of the [BVH](../appendix/a-glossary.md#b) traversal that
finds the hit. This separation matters because lights aren't
geometry: they don't intersect rays, they don't need bounding
boxes, they don't get tessellated. A light is just a function
that says *here is where I am, and here is what I emit*.

## 9.2 Point lights

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
[Whitted](../appendix/a-glossary.md#w)-renderer problem. Distance falloff is queued under
roadmap §4.4.b and lands when stochastic shadow sampling does.

## 9.3 Directional lights

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

## 9.4 The shadow ray

The shading routine in
[chapter 8 §8.4](08-materials-and-brdfs.md#8-4-the-five-shipped-materials)
tests whether each light is *visible* from the hit point before
adding its contribution. The test is a single boolean ray cast
from the surface point toward the light:

```cpp
if (scene.intersects(Rayd(hitPoint.point(), in).epsilonShifted(), state)) {
  // shadow: this light is blocked by an occluder
} else {
  // visible: add the light's contribution to the surface color
}
```

The ray is `epsilonShifted` — its origin is pushed forward
along the direction by `Ray::epsilon` to prevent the ray from
spuriously self-intersecting the surface it just bounced off of
(see
[chapter 3 §3.3](../01-foundations/03-rays-and-geometry.md#3-3-the-ray-class-all-of-it)).
It calls `scene.intersects(...)` rather than
`scene.intersect(...)`: the cheaper boolean form from
[chapter 7 §7.1](07-primitives-and-intersection.md#7-1-the-primitive-interface),
since a shadow ray only needs to know *whether* it hits
anything, not *where*.

The shadow ray is the single biggest cost the shading pipeline
pays. A scene with $L$ lights spawns $L$ shadow rays per ray
hit; with $N$ primary rays per pixel and $D$ recursion levels
per primary ray, the total ray-cast count is approximately $N
\cdot D \cdot (1 + L)$ rays per pixel. This is why the
boolean form exists — knowing only "is anything in the way?"
is enough information for shadow visibility, and a routine that
returns at the first hit it finds (rather than the closest one)
is markedly faster than the full geometric form.

## 9.5 The geometric shadow boundary

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

## 9.6 Ambient as cheap GI

A surface in *true* shadow — completely blocked from every
light source — is fully dark in the math from §9.4. In real
life, surfaces in shadow aren't fully dark: bounced light from
nearby surfaces still reaches them, producing a soft fill that
keeps shadows readable.

The "right" way to compute this is **global illumination**:
trace many bounces of light, accumulate the indirect
contributions, get the soft shadow fill for free. That's what
a path tracer does. The Whitted renderer doesn't do that, and
adding it would be a multi-week project (queued under roadmap
§4.1).

The Whitted renderer's substitute is the **scene ambient**.
Every surface receives a uniform background contribution from
`Scene::ambient()`, multiplied by the material's ambient
coefficient (which scales with the surface's response, like its
diffuse color). This produces a baseline brightness that fills
the otherwise-black shadow regions. The math is the term in
[chapter 8 §8.4](08-materials-and-brdfs.md#8-4-the-five-shipped-materials):

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

## 9.7 Shadow maps for the rasterizer

The shadow ray from §9.4 is the raytracer's mechanism. The
rasterizer cannot use it: rasterization runs over screen-space
triangles, with no ray-cast primitive available at fragment
shading time. Its alternative is the **shadow map**, an
opt-in feature on
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

Three knobs control quality versus cost:

- **Map resolution** — how finely the light-space depth
  image samples the scene. Low values quantize shadow edges
  visibly; higher values cost proportionally more raster work
  and memory.
- **Bias** — an additive depth tolerance for the comparison.
  Too little bias lets a surface shadow itself due to depth
  quantization (the classic *shadow acne*); too much bias
  detaches the shadow from the casting object (the classic
  *Peter Panning*).
- **PCF radius** — percentage-closer filtering. The
  comparison runs over a $(2r + 1) \times (2r + 1)$ kernel of
  neighboring texels and averages the results, producing a
  soft penumbra around hard shadow edges. Radius 0 is the
  exact nearest-texel comparison; radius 1 uses a 3×3 kernel,
  radius 4 a 9×9 kernel.

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
cast shadows. Application code that wants shadows turns them
on per-light through the rasterizer's setter API.

## 9.8 What this chapter does *not* cover

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
mentioned in §9.2. Same story — adding it interacts with light
intensity tuning in ways that are easier to handle in a
fully-Monte-Carlo path tracer.

Neither is missing because of an architectural problem; they
just aren't needed for a Whitted renderer that targets
educational scenes. When path tracing lands they ship
together.

## 9.8 Exercises

1. Construct a scene with two point lights at the same color
   and intensity but opposite positions. Place a single sphere
   between them. Predict the shading at the sphere's surface
   point closest to each light. Now place the same sphere
   between two directional lights pointing in opposite
   directions. What's different?
2. The shadow ray uses `scene.intersects(...)` (the boolean
   form) instead of `scene.intersect(...)` (the
   hit-point-emitting form). For a shadow ray that has to
   traverse 100 primitives in a complex scene to find the
   nearest occluder, how much work does the boolean form save
   over the geometric form? How does the saving change with the
   BVH from
   [chapter 15](../03-scene-structure/15-spatial-acceleration.md)
   in place?
3. Set `Scene::ambient()` to pure white and render a sphere on
   a black floor with a single directional light. The sphere
   has visibly distinct lit and shadowed regions. Describe what
   the shadowed region looks like, and explain in terms of the
   §9.6 ambient formula.
4. Read the `epsilonShifted` step in the shadow-ray
   construction. Suppose you remove it. Predict the visual
   artifacts in a render of a sphere on a plane with a point
   light directly above. Why those particular artifacts?

## See also

- Volume index: [Volume II — Ray rendering](README.md)
- Previous: [8. Materials and BRDFs](08-materials-and-brdfs.md)
- Next:
  [10. Sampling and anti-aliasing](10-sampling-and-anti-aliasing.md)
- Materials consume light contributions:
  [8. Materials and BRDFs §8.4](08-materials-and-brdfs.md#8-4-the-five-shipped-materials)
- Shadow-boundary functional contract:
  [`test/functional/render/lights/PointLightTest.cpp`](../../../test/functional/render/lights/PointLightTest.cpp)

## Source anchors

<!-- source-anchors -->
- `include/render/lights/Light.h`
- `include/render/lights/PointLight.h`
- `include/render/lights/DirectionalLight.h`
- `test/functional/render/lights/PointLightTest.cpp`
<!-- /source-anchors -->
