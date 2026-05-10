# 11. Textures

A material wants to know what color a surface is at the hit point.
For a uniform red plane the answer is `Colord(1, 0, 0)`
everywhere, but most surfaces in the real world aren't uniform —
they have wood grain, brick patterns, photograph-like detail. A
**texture** is the abstraction that lets a material answer "what
color am I right here?" without baking the answer into geometry.

By the end of this chapter you should know:

- the `Texture<T>` interface and the contract it places on
  subclasses,
- the three concrete textures the codebase ships
  (`ConstantColorTexture`, `CheckerBoardTexture`,
  `UVColorTexture`) and what each is for,
- the **mapping** abstraction that turns a hit point into 2D
  texture coordinates, and the difference between planar and
  UV-direct,
- how a material plugs a texture in.

## 11.1 The `Texture<T>` interface

The interface is small. From
[`include/render/textures/Texture.h`](../../../include/render/textures/Texture.h):

```cpp
// include/render/textures/Texture.h
template<class T>
class Texture : public render::Object {
public:
  virtual ~Texture() {}
  virtual T evaluate(const Rayd& ray, const HitPoint& hitPoint) const = 0;
};

typedef Texture<Colord> Texturec;
```

The interface declares one pure-virtual method: given a ray and
the hit point that ray produced, return a value of type `T`. For
the color textures we care about, `T` is `Colord` and the typedef
`Texturec` is what materials accept.

The reason both the ray *and* the hit point are passed: most
textures only need the hit point's surface position or its
interpolated UV coordinates, but a ray-aware texture (think:
distance-fade, view-direction shimmer, or anything that wants to
know how the surface is being looked at) can ask. None of the
shipped textures use the ray; the parameter is there for future
extensions and to keep the interface honest.

## 11.2 The three concrete textures

### `ConstantColorTexture`

This is the trivial case. The class stores a single `Colord` and
`evaluate` returns it unchanged. This is how a "plain red sphere"
actually works under the hood — its material wraps a
`ConstantColorTexture(Colord(1, 0, 0))`.

```cpp
// concept, not the literal source
class ConstantColorTexture : public Texturec {
  Colord m_color;
public:
  Colord evaluate(const Rayd&, const HitPoint&) const override {
    return m_color;
  }
};
```

You'll see this everywhere in the codebase. Every functional test
that adds a "red sphere" or a "blue plane" is constructing a
`ConstantColorTexture` and handing it to a material constructor.

### `CheckerBoardTexture`

The classic two-color checker pattern. Two child textures (a
"bright" and a "dark") plus a 2D mapping that turns the hit point
into texture coordinates $(s, t)$. The lookup rule is one line:
when $\lfloor s \rfloor + \lfloor t \rfloor$ is even, return the
bright texture's value; when odd, return the dark texture's value.

That arithmetic is the whole pattern. It's recursive: the
"bright" and "dark" children are themselves `Texturec`s, so you
can nest checker patterns to produce hierarchical patterns
(checker of checkers of solid colors). In practice the children
are constant colors and the recursion is one level deep.

The interesting choice is the mapping. The same checker math
produces wildly different results depending on whether $(s, t)$ is
derived from the hit point's *world position* or from the hit
point's *interpolated UV coordinates*. That's the next section.

### `UVColorTexture`

A diagnostic texture. It returns a color whose components are the
hit point's $(u, v)$ coordinates: red = $u$, green = $v$, blue =
$0$. Render a textured surface with a `UVColorTexture` and you can
literally see the UV layout — red increasing toward $u = 1$, green
increasing toward $v = 1$. Invaluable for catching UV-mapping
bugs in tessellation code (see
[chapter 17](../04-rasterization/17-tessellation.md)) and for
verifying perspective-correct interpolation in the rasterizer
([chapter 21](../04-rasterization/21-msaa-and-attribute-interpolation.md)).

## 11.3 Mappings: from hit point to $(s, t)$

A 2D mapping is a function `(HitPoint) → (s, t)`. The codebase
ships two:

`PlanarMapping2D` derives $(s, t)$ from the hit point's world-
space position by reading off two axes. The natural reading: take
$x$ as $s$, $z$ as $t$, ignore $y$ (or the other obvious
permutation depending on which face is being mapped). Useful for
ground planes and infinite floors where there's no "intrinsic" UV
parameterization — the geometry is just an infinite plane and you
want a checker pattern that tiles forever.

`UVMapping2D` reads $(s, t)$ directly out of the hit point's
stored UV coordinates. Every primitive that has a meaningful UV
parameterization (sphere, cylinder, disk, mesh triangles) fills
those in during intersection, so this mapping just passes them
through. This is what you want for textures applied to a curved
surface — the UV coords follow the curvature.

Why two? Because the same checker texture can produce two very
different visual results depending on which mapping it gets. A
floor plane with a `PlanarMapping2D` shows a regular grid of
squares aligned to the world axes. A sphere with a
`PlanarMapping2D` shows squares that look correct on the
near-equator face and increasingly stretched / pinched toward the
poles, because the planar mapping treats each world-space coord
independently of the surface it lies on. A sphere with a
`UVMapping2D` shows squares that follow the sphere's lat-long
parameterization, with the natural pinching at the poles you'd
expect from a globe.

The interactive `texture_coordinate_mapping` widget below makes
this hands-on:

<!-- widget: texture_coordinate_mapping -->

Drag the hit point around the surface, switch the mapping
dropdown between planar and UV, and watch $(\lfloor s \rfloor +
\lfloor t \rfloor)$ change parity to give the bright/dark
checker output.

## 11.4 How a material plugs a texture in

Materials don't store colors directly. They store a
`std::shared_ptr<Texturec>` and call `evaluate` whenever they need
to know the surface color at the hit point. From a typical Matte
material constructor:

```cpp
auto sphere = std::make_shared<Sphere>(Vector3d::null(), 1.0);
sphere->setMaterial(std::make_shared<MatteMaterial>(
  std::make_shared<ConstantColorTexture>(Colord(1, 0, 0))));
```

The three nested `make_shared` calls track the three layers: the
outer one is the material, the middle one is the texture pointer,
and the innermost is the actual color. Replace the inner
`ConstantColorTexture` with a `CheckerBoardTexture(...)` and the
same sphere now has a checkered surface — no other code changes.

This is also why the Matte material's albedo lookup in the
rasterizer
([chapter 18](../04-rasterization/18-the-rasterization-pipeline.md))
is a tiny indirect call into `texture->evaluate(...)` rather than
a direct field read. The rasterizer doesn't know whether the
texture is a constant, a checker, or anything else; it just hands
the hit point over and gets a color back.

## 11.5 What's missing — image textures

One thing is notably absent from this list: an `ImageTexture`
that reads a PNG or EXR off disk and samples it. That's a queued
item under `docs/topics-backlog.md` (and roadmap §4.3.b). When it
lands, it will look like:

- Constructor takes a path or a `Buffer<Colord>`.
- `evaluate` reads $(s, t)$ via the configured mapping (probably
  `UVMapping2D` by default), wraps or clamps to $[0, 1]$, and
  samples the buffer with bilinear interpolation.
- Tone-mapping and gamma-decoding the source image is the
  consumer's job; the texture itself works in linear RGB like
  every other `Texturec`.

This is the natural next chapter to revisit. The book will pick
up the change in the same spot when it ships.

## 11.6 Exercises

1. Predict what a sphere with a `CheckerBoardTexture` and a
   `PlanarMapping2D` looks like *if you rotate the sphere*. Does
   the checker pattern rotate with the sphere or stay fixed in
   world space? Verify with a hand-edited scene.
2. Implement a `StripeTexture` that alternates two child textures
   based on $\lfloor s \rfloor$ alone (ignoring $t$). What's the
   visual difference vs a `CheckerBoardTexture` whose dark child
   is also a `StripeTexture`?
3. The `UVColorTexture` returns $(u, v, 0)$. Why $0$ for blue and
   not, say, $1 - u - v$? What information would you lose by
   choosing the latter?
4. Read the `PlanarMapping2D` source. Which pair of world-space
   axes does it use for $(s, t)$? Is that choice configurable, or
   hard-coded? What changes if a primitive lies in the wrong
   plane for the default?

## See also

- Volume index: [Volume II — Ray rendering](README.md)
- Previous: [10. Sampling and anti-aliasing](10-sampling-and-anti-aliasing.md)
- Next: [12. Tone mapping](12-tone-mapping.md)
- Consumer: [8. Materials and BRDFs](08-materials-and-brdfs.md) —
  every shipped material wraps a `Texturec`
- Rasterizer use:
  [21. MSAA and attribute interpolation](../04-rasterization/21-msaa-and-attribute-interpolation.md)
  — `UVMapping2D` is what makes perspective-correct UV
  interpolation visible to the material
- UV provenance: [17. Tessellation](../04-rasterization/17-tessellation.md)
  — where each primitive's UV coords come from

## Source anchors

<!-- source-anchors -->
- `include/render/textures/Texture.h`
- `include/render/textures/ConstantColorTexture.h`
- `include/render/textures/CheckerBoardTexture.h`
- `include/render/textures/UVColorTexture.h`
- `include/render/textures/mappings/`
<!-- /source-anchors -->
