# 5. The Whitted pipeline

A renderer is a function from a scene description to an image.
Volume II is about how this codebase computes that function using
the **Whitted** algorithm — the recursive ray-tracing method
introduced by Turner Whitted in 1980 and still the textbook entry
point to ray-based rendering.

This chapter is the volume's road map. It walks the entire
pipeline at a high level, names the file that owns each step, and
points to the chapter that elaborates on it. Subsequent chapters
expand each step in turn. The aim of reading chapter 5 first is to
have a mental model of where each subsequent chapter fits before
you dive into the math of any one piece.

By the end you should know:

- the seven steps the Whitted algorithm performs for every primary
  ray,
- which file in the codebase owns each step,
- the `RenderEngine` / `Raytracer` split and why the abstraction
  exists,
- the recursive structure that gives [Whitted](../appendix/a-glossary.md#w) its name, and the
  reason the recursion has a depth cap.

## 5.1 The algorithm in one paragraph

The renderer casts a ray from the camera through every pixel,
finds the closest intersection with any object in the scene, and
asks the surface material at the hit point what color it produces.
For direct lighting it shoots one shadow ray per light to figure
out which lights are visible. For reflection or refraction it
recursively traces a secondary ray and combines the returned color
with the local shading. The algorithm repeats this across the
pixel grid, accumulates into the float framebuffer, tonemaps to
[LDR](../appendix/a-glossary.md#l), and writes to the display buffer.

That is the entire algorithm. Every chapter in Volume II takes one
of those clauses and unpacks it.

## 5.2 The seven steps, in detail

It helps to enumerate the steps with the file that owns each one.

1. **Camera ray generation.** For each pixel $(x, y)$, the camera
   produces a `Rayd` whose origin is the camera position (or a
   sampled point on the lens, for thin-lens cameras) and whose
   direction points through the pixel's location on the view
   plane.
   - Owner: [`include/render/cameras/`](../../../include/render/cameras/).
   - Detail: [chapter 6](06-cameras.md).
2. **Scene intersection.** The renderer walks the scene's spatial
   structure (typically a [BVH](../appendix/a-glossary.md#b), sometimes a flat list) and tests
   the ray against each primitive's bounding box, then against the
   primitive's actual geometry for every box that survives. The
   closest valid hit is returned.
   - Owner:
     [`include/render/primitives/Scene.h`](../../../include/render/primitives/Scene.h)
     and every concrete primitive's `intersect`.
   - Detail: [chapter 7](07-primitives-and-intersection.md);
     spatial structure in
     [chapter 15](../03-scene-structure/15-spatial-acceleration.md).
3. **Material shading.** The hit point is handed to the
   primitive's `Material::shade(...)` method. The material
   consults its texture for the surface color, applies the [BRDF](../appendix/a-glossary.md#b)
   appropriate to its type ([Lambertian](../appendix/a-glossary.md#l) for matte, glossy for
   [Phong](../appendix/a-glossary.md#p), perfect specular for mirror, perfect transmitter for
   glass), and asks the renderer to recurse on any secondary rays
   it needs.
   - Owner:
     [`include/render/materials/`](../../../include/render/materials/).
   - Detail: [chapter 8](08-materials-and-brdfs.md).
4. **Direct lighting.** For each light in the scene, a shadow ray
   is cast from the hit point toward the light. If the shadow ray
   reaches the light unobstructed, the light contributes directly
   to the surface; if not, the light is blocked and contributes
   nothing.
   - Owner:
     [`include/render/lights/`](../../../include/render/lights/).
   - Detail: [chapter 9](09-lights-and-shading.md).
5. **Recursive reflection and refraction.** A reflective material
   computes a mirror-direction secondary ray and calls
   `rayColor(secondaryRay, state)` to obtain its color. A
   transparent material does the same with the refraction
   direction. The recursive call may go several levels deep before
   bottoming out.
   - Owner: same materials as step 3, plus the
     `Raytracer::rayColor` entry point.
6. **Anti-aliasing accumulation.** When the camera's view plane
   has a sampler with more than one sample per pixel, the steps
   above run multiple times per pixel — once per subpixel sample —
   and the results are averaged. This is the Monte Carlo integral
   over the pixel area.
   - Owner: the camera's view plane and sampler.
   - Detail: [chapter 10](10-sampling-and-anti-aliasing.md).
7. **[Tonemap](../appendix/a-glossary.md#t).** Once every pixel of the [HDR](../appendix/a-glossary.md#h) float framebuffer is
   filled, a tonemap operator runs over the buffer to compress the
   high-dynamic-range floats into the $[0, 1]$ display range, and
   the result is packed into `unsigned int`s for the LDR display
   buffer. Render complete.
   - Owner:
     [`include/render/tonemap/`](../../../include/render/tonemap/).
   - Detail: [chapter 12](12-tone-mapping.md).

That covers all of Volume II in seven items. The remaining
chapters fill in the math and the code at each step.

## 5.3 The `RenderEngine` abstraction

The codebase factors the rendering algorithm behind an abstract
base class,
[`render::RenderEngine`](../../../include/render/RenderEngine.h).
Anything that takes a scene and a camera and produces an image is
a `RenderEngine`. The Whitted raytracer is one concrete subclass;
the wireframe engine
([chapter 20](../04-rasterization/20-wireframe-rendering.md)) and
the software rasterizer
([chapter 18](../04-rasterization/18-the-rasterization-pipeline.md))
are the two others currently shipped.

The base class owns the scene pointer, the camera shared pointer,
the tonemap pointer, the cancellation hook, and the
`render(buffer)` overload that calls into the subclass-specific
algorithm. The subclass-specific bits are the algorithm itself
plus any controls that only make sense for that algorithm — in
the raytracer's case, the maximum recursion depth, the
worker-thread count, and the single-ray probe methods used by the
interactive picking path.

The
[`engine::raytracer::Raytracer`](../../../include/engine/raytracer/Raytracer.h)
header tells the same story, with comment annotations explaining
what is engine-shared and what is raytracer-specific:

```cpp
// include/engine/raytracer/Raytracer.h
class Raytracer : public render::RenderEngine, public render::RayCaster {
public:
  // ... constructors ...
  virtual void render(Buffer<Colord>&        buffer) override;
  virtual void render(Buffer<unsigned int>&  buffer) override;

  // single-ray probes — raytracer-specific
  const render::Primitive* primitiveForRay(const Rayd& ray) const;
  render::State            rayState(const Rayd& ray) const;
  Colord                   rayColor(const Rayd& ray, render::State& state) const override;

  void setMaximumRecursionDepth(int depth);
  void setMaximumThreads(int threads);
  // ...
};
```

The two `render` overloads are both required by `RenderEngine`.
The `Buffer<Colord>&` overload writes the HDR float framebuffer.
The `Buffer<unsigned int>&` overload tile-tonemaps as it goes so
that an interactive viewer polling the buffer mid-render sees
partial output progressively, rather than waiting for the entire
HDR pass to finish before any tonemapped pixels appear.

`Raytracer` also inherits from `RayCaster`, which exposes the
single-ray `rayColor` entry point as a virtual interface separate
from the threaded multi-ray render. That separation lets material
shading call back into the renderer without dragging in the
threading machinery — a `Material::shade` implementation that
needs to recurse on a reflection ray takes a `RayCaster*` and
calls `rayColor` on it.

## 5.4 The recursive heart

Step 5 above — recursive reflection and refraction — is where
Whitted's name comes from and is also the only step with a real
recursive structure. The other six steps run once per pixel; this
one calls itself.

The implementation is short. From
[`src/engine/raytracer/Raytracer.cpp`](../../../src/engine/raytracer/Raytracer.cpp):

```cpp
// src/engine/raytracer/Raytracer.cpp:144
Colord Raytracer::rayColor(const Rayd& ray, render::State& state) const {
  state.recurseIn();
  ScopeExit sx([&] { state.recurseOut(); });

  if (state.recursionDepth == p->maximumRecursionDepth) {
    return m_scene->background();
  }

  HitPointInterval hitPoints;
  auto primitive = m_scene->intersect(ray, hitPoints, state);
  if (primitive) {
    auto hitPoint = hitPoints.minWithPositiveDistance();
    if (primitive->material()) {
      return primitive->material()->shade(this, *m_scene, ray, hitPoint, state);
    } else {
      return Colord::black();
    }
  } else {
    return m_scene->background();
  }
}
```

The routine has three branches.

The first branch is the **depth cap**. The recursion depth is
bumped on entry and decremented on exit (via the `ScopeExit`
guard). Once it reaches the configured maximum, the renderer
bottoms out by returning the scene background instead of recursing
further. The choice of "background, not black" is deliberate: at
the depth cap, the ray is conceptually a long distance from the
camera, so returning the scene's environment color produces an
image consistent with what a non-recursive miss would produce.

The second branch is the **hit case**. The scene's `intersect`
returns the closest primitive the ray hits, with the corresponding
`HitPointInterval` populated. The renderer asks the primitive for
its material, hands the `RayCaster*` (`this`) to the material's
`shade` method, and lets the material drive the next step — which,
for a reflective or transparent material, includes calling back
into `rayColor` on a secondary ray. The recursive call then
re-enters this same routine one level deeper.

The third branch is the **miss case**. Nothing was hit, so the
routine returns the scene's background color.

The depth cap matters because Whitted recursion can be unbounded.
A pair of facing mirrors traps a ray bouncing between them
indefinitely; without a cap the renderer would loop forever (or
exhaust the call stack). The default cap is 10, chosen to handle
glass-torus scenes — four surface crossings (ray enters glass, ray
exits glass on the far side) times reflection branches per hit —
without truncating any visibly significant energy. Scenes that
specifically want deeper recursion (a hall of mirrors, a thick
stack of glass plates) can crank the limit; scenes that need only
a few bounces can crank it down to save time.

## 5.5 The per-ray `State`

One detail in the code above is worth its own paragraph. Every
call to `rayColor` takes a `render::State&` argument, mutated as
the recursion descends and unwinds.
[`render::State`](../../../include/render/State.h) carries:

- The recursion depth and the depth cap.
- A counter of rays cast (used by the stats overlay).
- The hit point at the *primary* recursion level — the ray that
  came from the camera, not any secondary ray. The interactive
  picking path reads this to figure out which primitive the user
  clicked on.
- An optional event log for debug visualization (the
  `RefractingRayTracer` example uses it to draw the actual ray
  path through a refractive scene).

A fresh `State` is constructed once per primary ray — at the start
of rendering each pixel — and threaded by mutable reference
through every recursive call. The mutable-reference pattern keeps
the data flow visible at the call site (you can see exactly which
calls touch state) while avoiding the cost of copying the state
record at every recursion.

## 5.6 The full data flow, with types

In the vocabulary of Volumes I and II so far:

```
Scene + Camera + Sampler
      │
      ▼
for each pixel (x, y) in the view plane's iteration order:
   for each subpixel sample s of the sampler:
      Rayd primaryRay = camera->rayForPixel(x, y, s);
      State state;
      Colord pixel = raytracer->rayColor(primaryRay, state);
      buffer[y][x] += pixel / sampleCount;     // accumulate
      │
      │       (rayColor recurses on secondary rays as needed)
      ▼
Buffer<Colord>          ← HDR float framebuffer, all pixels filled
      │
      │  tonemap operator (Linear / Reinhard / ACES)
      ▼
Buffer<unsigned int>    ← LDR display framebuffer
      │
      ▼
display / PNG file
```

This is the
[chapter 4 §4.6](../01-foundations/04-color-and-buffers.md#4-6-putting-it-together)
data-flow diagram with the inner loop expanded. The arrows that
were single steps there are the chapters of Volume II.

## 5.7 What this volume does *not* cover

Two engines share scene + camera abstractions with the raytracer
but produce pixels through entirely different algorithms:

- The **wireframe engine**
  ([chapter 20](../04-rasterization/20-wireframe-rendering.md))
  edge-projects tessellated geometry into screen space and rasters
  the edges with [Bresenham](../appendix/a-glossary.md#b)'s line algorithm. No ray casting at all.
- The **software rasterizer**
  ([chapter 18](../04-rasterization/18-the-rasterization-pipeline.md))
  projects triangles into screen space and runs the textbook
  edge-function algorithm to fill them. Also no ray casting.

Both share the camera and scene abstractions developed in this
volume; they simply consume them differently. Once you understand
how the raytracer turns a `Camera` and a `Scene` into pixels,
Volume IV reuses both inputs in a different algorithmic frame.

The other intentional omission is **path tracing** and the broader
Monte Carlo integration territory. Path tracing is queued under
roadmap §4.1 and `docs/topics-backlog.md` §A; once it lands as a
new `RenderEngine` subclass, this volume will gain a chapter
covering its differences from Whitted (importance sampling,
Russian-roulette termination, multi-bounce indirect illumination).
Until then, "ray rendering" in this book means Whitted.

## 5.8 Exercises

1. The `rayColor` routine returns `Colord::black()` when the
   primitive has no material. Why? What rendered output would you
   expect from a primitive with no material attached?
2. Read `Raytracer::primitiveForRay`. It calls into the same
   scene `intersect` as `rayColor`, but skips the material
   shading. What is that operation used for in the editor, and
   why is it cheap enough for interactive picking on a mouse
   click?
3. Set the maximum recursion depth to 0, render a scene with a
   reflective sphere, and predict what you see. Set it to 1.
   Predict again. Verify with a real run.
4. The recursion uses the `ScopeExit` guard to decrement the
   depth on exit even if the call returns early. Why a guard, not
   just a single decrement before each `return`? What invariant
   does the guard preserve?

## See also

- Volume index: [Volume II — Ray rendering](README.md)
- Previous: [4. Color and buffers](../01-foundations/04-color-and-buffers.md)
- Next: [6. Cameras](06-cameras.md)
- The other engines: [Volume IV — Rasterization](../04-rasterization/README.md)
- Each step in the seven-step list links to its own chapter above.

## Source anchors

<!-- source-anchors -->
- `include/engine/raytracer/Raytracer.h`
- `src/engine/raytracer/Raytracer.cpp`
- `include/render/RenderEngine.h`
- `include/render/State.h`
- `include/render/RayCaster.h`
<!-- /source-anchors -->
