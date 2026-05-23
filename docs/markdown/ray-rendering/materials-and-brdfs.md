# Materials and BRDFs

A primitive answers the question *where does this ray hit?* A
material answers *what color comes back?* The two halves
together drive every pixel of every render the codebase
produces, and the boundary between them is one of the most
useful abstractions in graphics: separating *geometry* from
*appearance*.

By the end of this chapter you should know:

- the `Material` interface and its single `shade(...)` virtual,
- what a [BRDF](../appendix/a-glossary.md#b) is, mathematically and at the function-pointer
  level,
- the four BRDFs the codebase ships ([Lambertian](../appendix/a-glossary.md#l), GlossySpecular,
  PerfectSpecular, PerfectTransmitter), and the matching
  materials that compose them,
- the `BSDF` abstraction that unifies BRDF and [BTDF](../appendix/a-glossary.md#b) behind
  one interface,
- and how reflection, transmission, and the portal effect fit
  the same shading frame.

## <a id="the-material-interface"></a>The `Material` interface
Every material derives from
[`render::Material`](../../../include/render/materials/Material.h)
and overrides one virtual method:

```cpp
// include/render/materials/Material.h
virtual Colord shade(
    const render::RayCaster*  raycaster,
    const render::Scene&      scene,
    const Rayd&               ray,
    const HitPoint&           hitPoint,
    render::State&            state) const = 0;
```

The signature carries five arguments and one return value. The `Colord` is the radiance
leaving the surface in the direction of the incoming ray —
exactly the quantity the recursive `rayColor` from
[The Whitted pipeline: The recursive heart](the-whitted-pipeline.md#the-recursive-heart)
expects to add to its accumulator.

The `RayCaster*` is the back-pointer the material uses to recurse
on secondary rays. A `ReflectiveMaterial` calls
`raycaster->rayColor(reflectedRay, state)` and gets the color the
recursion produces back; a `TransparentMaterial` does the same
with the refracted ray. The double-pointer interface keeps the
threading machinery from
[The Whitted pipeline: The `RenderEngine` abstraction](the-whitted-pipeline.md#the-renderengine-abstraction)
out of the material's view: a material doesn't know the engine
has a thread pool or a queue size — it just calls `rayColor`.

The `Scene&` is the scene reference the shader needs for two
specific operations: iterating the lights for direct illumination
through [the `BSDF` interface](#the-bsdf-interface), and casting shadow rays via `scene.intersects(...)`. The
shadow-ray cost is half of what a raytraced render spends per
hit, which is why `intersects` is a separate cheap-boolean form
on every primitive
([Primitives and intersection: The `Primitive` interface](primitives-and-intersection.md#the-primitive-interface)).

The `Rayd&` is the incoming ray; the `HitPoint&` is the
intersection record from
[Rays and geometry: `HitPoint`: what comes back](../foundations/rays-and-geometry.md#hitpoint-what-comes-back).
The `State&` is the per-ray state from
[The Whitted pipeline: The per-ray `State`](the-whitted-pipeline.md#the-per-ray-state),
mutated as the material recurses.

## <a id="what-a-brdf-is"></a>What a BRDF is
A **BRDF** — Bidirectional Reflectance Distribution Function —
is the continuous function that answers, for a single point on a
surface, *for every incoming direction $\mathbf{w}_i$ and every
outgoing direction $\mathbf{w}_o$, what fraction of the incoming
radiance reflects out along $\mathbf{w}_o$?* In symbols,

$$
f_r(\mathbf{w}_i, \mathbf{w}_o)
$$

with units of $\text{sr}^{-1}$ (inverse steradians, since it
distributes radiance per unit solid angle). The shading equation
that drives every direct-lighting computation is the BRDF
multiplied by the cosine of the incident angle:

$$
L_o(\mathbf{w}_o) = \int f_r(\mathbf{w}_i, \mathbf{w}_o) \, L_i(\mathbf{w}_i) \, (\mathbf{n} \cdot \mathbf{w}_i) \, d\mathbf{w}_i
$$

A [Whitted](../appendix/a-glossary.md#w) raytracer reduces that integral to a sum over the
scene's lights — one BRDF evaluation per light, multiplied by
the light's radiance and the geometric cosine.

In code, the BRDF interface is
[`render::BRDF`](../../../include/render/brdf/BRDF.h). Its main
method is `operator()(hitPoint, in, out)`, which returns the
reflectance value at the hit point for the given direction
pair. The associated `BTDF` (transmittance — the same idea, but
for light passing *through* the surface) lives in
[`include/render/brdf/BTDF.h`](../../../include/render/brdf/BTDF.h).

## <a id="the-four-shipped-brdfs"></a>The four shipped BRDFs
[`Lambertian`](../../../include/render/brdf/Lambertian.h) is the
view-independent diffuse reflection model. The BRDF value is a
constant — the diffuse color divided by $\pi$ — for every
incoming and outgoing direction pair. The direct-lighting code
multiplies the BRDF value by the cosine $(\mathbf{n} \cdot
\mathbf{w}_i)$, which is what produces the visible brightness
falloff at grazing angles. Lambertian is the cheapest BRDF in
the catalog and the right default for matte surfaces.

[`GlossySpecular`](../../../include/render/brdf/GlossySpecular.h)
is the [Phong](../appendix/a-glossary.md#p) specular lobe. It returns nonzero only when the
outgoing direction $\mathbf{w}_o$ is close to the *mirror
reflection* of the incoming direction; the lobe's tightness is
controlled by an exponent. Higher exponents produce sharper
highlights; lower exponents spread the highlight into a soft
glow. The BRDF value falls off as $\cos^n \theta$ where $\theta$
is the angle between $\mathbf{w}_o$ and the mirror direction, so
the highlight rolls off smoothly with angle.

The two BRDFs above sit on opposite ends of the
view-dependence spectrum. Lambertian is fully view-independent —
the surface looks the same from every angle. Phong is fully
view-dependent — the highlight tracks where the eye is.
The widget shows both lobes with draggable view and light
directions plus a slider for the Phong exponent:

<!-- widget: phong_lambertian_lobes -->

[`PerfectSpecular`](../../../include/render/brdf/PerfectSpecular.h)
is the BRDF for a perfect mirror. It returns nonzero only at
exactly one direction — the mirror reflection of the incoming
ray. As a continuous function it is a Dirac delta; as a
practical matter, the implementation handles it by
*sampling* the BRDF (returning the sampled mirror direction and
its reflectance) rather than evaluating it for an arbitrary
direction. This is why `BRDF::sample(...)` exists alongside
`operator()`.

[`PerfectTransmitter`](../../../include/render/brdf/PerfectTransmitter.h)
is the analogous BTDF for a perfect refractor — light passing
through the surface, redirected by Snell's law:

$$
n_1 \sin\theta_1 = n_2 \sin\theta_2
$$

where $n_1$ and $n_2$ are the indices of refraction on each side
of the interface. When the light hits at too steep an angle —
above the *critical angle* for the medium — the refraction
formula produces no real solution and the light reflects back
into the original medium instead of refracting. This is **total
internal reflection**, the reason a glass of water has bright
spots near the bottom edge.

## <a id="the-five-shipped-materials"></a>The five shipped materials
A **material** composes one or more BRDFs (and optionally a
texture for the surface color) into a `shade` routine the
renderer calls.

[`MatteMaterial`](../../../include/render/materials/MatteMaterial.h)
wraps a Lambertian BRDF. The `shade` routine reads the texture
at the hit point, builds two Lambertian instances (one for
ambient, one for diffuse), and combines them with the scene's
lights:

```cpp
// src/render/materials/MatteMaterial.cpp:19
auto texColor = diffuseTexture()
    ? diffuseTexture()->evaluate(ray, hitPoint)
    : Colord::black();

Lambertian ambientBRDF(texColor, ambientCoefficient());
Lambertian diffuseBRDF(texColor, diffuseCoefficient());

auto color = ambientBRDF.reflectance(hitPoint, Vector3d::null())
           * scene.ambient();

for (const auto& light : scene.lights()) {
  Vector3d in = light->direction(hitPoint.point());

  if (scene.intersects(Rayd(hitPoint.point(), in).epsilonShifted(), state)) {
    // shadow: this light is blocked
  } else {
    double normalDotIn = hitPoint.normal() * in;
    if (normalDotIn > 0.0)
      color += diffuseBRDF(hitPoint, ...) * light->radiance() * normalDotIn;
  }
}

return color;
```

Three things are worth noting in this routine. First, the
scene-ambient-times-ambient-BRDF term gives every surface a
non-zero base brightness even in shadow, which is the cheap
"global illumination" hack from
[The Whitted pipeline: The algorithm in one paragraph](the-whitted-pipeline.md#the-algorithm-in-one-paragraph).
Second, the per-light loop checks shadow visibility *before*
evaluating the diffuse BRDF, so a blocked light contributes
zero work, not just zero color. Third, the shadow ray is
`epsilonShifted` to avoid the ray-spawned-at-its-own-surface
self-intersection from
[Rays and geometry: The `Ray` class, all of it](../foundations/rays-and-geometry.md#the-ray-class-all-of-it).

The material's behavioral contracts (texture passthrough, the
ambient-coefficient linearity, the no-illumination invariant)
are pinned by
[`MatteMaterialTest`](../../../test/functional/render/materials/MatteMaterialTest.cpp).

[`PhongMaterial`](../../../include/render/materials/PhongMaterial.h)
adds a `GlossySpecular` BRDF on top of the same Lambertian
diffuse term. The shading routine is the same per-light loop as
Matte, with the specular contribution added:

$$
L_o = L_{\text{ambient}} + \sum_i \big(\, k_d \, f_d \, (\mathbf{n} \cdot \mathbf{w}_i) + k_s \, f_s(\mathbf{w}_i, \mathbf{w}_o) \, (\mathbf{n} \cdot \mathbf{w}_i) \,\big) L_i
$$

The diffuse term is what Matte produces; the specular term is
the new contribution. Phong-with-specular-zero degrades to
Matte exactly. The contrast between Phong and Matte under the
same dark scene + head-on light is pinned by
[`PhongMaterialTest`](../../../test/functional/render/materials/PhongMaterialTest.cpp).
The software rasterizer previews this same local Phong term; the
difference between the engines starts with the recursive materials below.

[`ReflectiveMaterial`](../../../include/render/materials/ReflectiveMaterial.h)
adds recursive reflection on top of Phong. The shade routine
delegates to Phong for direct lighting, then samples the
`PerfectSpecular` BRDF to get the mirror direction, calls
`raycaster->rayColor(mirrorRay, state)` to recurse, and
combines the recursive result with the local shading. The
recursion bottoms out at the depth cap from
[The Whitted pipeline: The recursive heart](the-whitted-pipeline.md#the-recursive-heart);
between depth 0 and depth $N$, the material produces the
classic "reflective sphere on a checkered floor" Whitted look.

The `reflective_material_recursion` widget shows the recursion
explicitly with a draggable incoming ray, draggable surface
normal, and a slider for the recursion depth:

<!-- widget: reflective_material_recursion -->

[`TransparentMaterial`](../../../include/render/materials/TransparentMaterial.h)
adds refraction. The shade routine delegates to Phong for direct
lighting, samples both `PerfectSpecular` (the partial mirror
reflection that any refractive surface still has) and
`PerfectTransmitter` (the refracted ray through the surface),
recurses on both, and combines using [Fresnel](../appendix/a-glossary.md#f) weighting — more
reflection at glancing angles, more transmission near
perpendicular. When the angle exceeds the critical angle for
the medium, the transmission contribution drops to zero and the
material behaves as a perfect mirror (total internal
reflection). The `transparent_material_refraction` widget shows
Snell's law and the Fresnel + total-internal-reflection
behavior with draggable ray direction and IOR sliders:

<!-- widget: transparent_material_refraction -->

[`PortalMaterial`](../../../include/render/materials/PortalMaterial.h)
is the unconventional one. The shade routine inverse-transforms
the incoming ray's origin and direction by a configurable
matrix, recurses on the transformed ray, and returns the result
optionally tinted by a color filter. The intended effect: a
"portal" surface that, when looked through, shows whatever the
scene looks like from a different position and orientation —
useful for impossible architecture, mirror-like effects with
asymmetric optics, and pure visual experiments. The widget
shows the input ray, the inverse-transformed query ray, and the
filter swatch:

<!-- widget: portal_material_ray_redirection -->

## <a id="the-bsdf-interface"></a>The `BSDF` interface
The umbrella abstraction that unifies the four BRDF/BTDF flavors
is
[`render::BSDF`](../../../include/render/bsdf/BSDF.h)
("**B**idirectional **S**cattering **D**istribution
**F**unction"). A `BSDF` answers three questions:

```cpp
// include/render/bsdf/BSDF.h
virtual Colord eval(const Vector3d& wi, const Vector3d& wo) const;
virtual Colord sample(const Vector3d& wi, Vector3d& wo, double& pdf) const;
virtual double pdf(const Vector3d& wi, const Vector3d& wo) const;
```

`eval(wi, wo)` returns the [BSDF](../appendix/a-glossary.md#b) value for a fully-specified
direction pair. Used by direct-lighting loops (which know the
light's direction and the eye's direction) and by importance-
sampling integrators that need a probability for a draw.

`sample(wi, wo, pdf)` generates an outgoing direction by
importance-sampling the lobe — picks a $\mathbf{w}_o$, writes it
to the out-parameter, writes the probability of having drawn it
into `pdf`, and returns the BSDF value at that draw. Used by
recursive integrators (Whitted, future path tracer) to spawn
the next ray.

`pdf(wi, wo)` returns just the probability density without a
new draw. Used by multiple-importance-sampling weights.

The interface unifies BRDF and BTDF — a `BSDF` may scatter into
the reflection lobe, the transmission lobe, or both, with the
caller agnostic to which one fired. This abstraction is what
the eventual path tracer will use; for the Whitted renderer
shipped today, `BSDF` is mostly the unifying parent class for
the existing BRDFs and BTDFs.

## <a id="the-four-brdfs-and-four-plus-one-materials-summarized"></a>The four BRDFs and four (plus one) materials, summarized
| BRDF / BTDF | What it computes | Used by |
|---|---|---|
| Lambertian | view-independent diffuse | Matte, Phong, Reflective, Transparent |
| GlossySpecular | Phong specular lobe | Phong, Reflective, Transparent |
| PerfectSpecular | mirror direction | Reflective, Transparent (Fresnel reflection) |
| PerfectTransmitter | refraction (Snell's law) | Transparent |

| Material | Composes | What it produces |
|---|---|---|
| MatteMaterial | Lambertian × 2 (ambient + diffuse) | matte / chalk / cloth |
| PhongMaterial | Matte + GlossySpecular | plastic / painted metal |
| ReflectiveMaterial | Phong + PerfectSpecular | mirror, polished metal |
| TransparentMaterial | Phong + PerfectSpecular + PerfectTransmitter | glass, water, gem |
| PortalMaterial | matrix-transformed recursion | non-physical effect |

The four materials in the first four rows compose for a clean
"phototgraphy-style" hierarchy: Phong is Matte plus highlights,
Reflective is Phong plus mirror recursion, Transparent is
Reflective plus refraction. Each step adds one BRDF / BTDF
contribution and one optional recursive call.

## <a id="exercises"></a>Exercises
1. Build a `MatteMaterial` with a black diffuse texture and
   render. The result is everywhere black. Now flip the
   `ambientCoefficient` to 1.0 and the scene's ambient color to
   pure white. Predict the rendered output.
2. Read the `ReflectiveMaterial::shade` source. The reflection
   contribution is added to the Phong result. What would happen
   if it were *replaced* with the Phong result instead — i.e.,
   mirror surfaces wouldn't carry their own diffuse / specular
   shading? Picture a chrome sphere on a white floor in your
   head; what would the change make different?
3. The Lambertian BRDF returns the diffuse color *divided by
   $\pi$*. Why? Where does that division show up in the radiance
   formula above, and what would happen if you forgot it?
4. The `PortalMaterial` allows an arbitrary 4×4 transform matrix
   between the incoming-ray space and the recursion-ray space.
   What constraint does the matrix have to satisfy for the
   portal to look like a "real" mirror? What constraint for it
   to look like a magnifying glass?

## See also

- Volume index: [Ray rendering](README.md)
- Previous: [Primitives and intersection](primitives-and-intersection.md)
- Next: [Lights and shading](lights-and-shading.md)
- Texture inputs: [Textures](textures.md)
- Recursive call back into the renderer:
  [The recursive heart](the-whitted-pipeline.md#the-recursive-heart)
- Functional contract tests:
  [`MatteMaterialTest`](../../../test/functional/render/materials/MatteMaterialTest.cpp),
  [`PhongMaterialTest`](../../../test/functional/render/materials/PhongMaterialTest.cpp)

## Source anchors

<!-- source-anchors -->
- `include/render/materials/Material.h`
- `include/render/materials/MatteMaterial.h`
- `include/render/materials/PhongMaterial.h`
- `include/render/materials/ReflectiveMaterial.h`
- `include/render/materials/TransparentMaterial.h`
- `include/render/materials/PortalMaterial.h`
- `include/render/brdf/BRDF.h`
- `include/render/brdf/BTDF.h`
- `include/render/brdf/Lambertian.h`
- `include/render/brdf/GlossySpecular.h`
- `include/render/brdf/PerfectSpecular.h`
- `include/render/brdf/PerfectTransmitter.h`
- `include/render/bsdf/BSDF.h`
- `test/functional/render/materials/MatteMaterialTest.cpp`
- `test/functional/render/materials/PhongMaterialTest.cpp`
<!-- /source-anchors -->
