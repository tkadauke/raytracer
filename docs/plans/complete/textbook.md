# Plan — long-form textbook in `docs/markdown/`

> **Companion docs:**
> - [`roadmap.md`](../roadmap.md) — feature pillars. Item §4.0 mandates an
>   educational deliverable; this plan is the long-form half of that.
> - [`modernize.md`](../modernize.md) — engineering-hygiene roadmap.
> - [`topics-backlog.md`](../topics-backlog.md) — out-of-scope topics that
>   haven't landed in the codebase yet, *not* covered by this book until
>   they do.
>
> **Status:** Decisions locked. Scaffolding is the next concrete step.

---

## 1. Mission

A markdown-based, wiki-style e-book under `docs/markdown/` that teaches the
algorithms and abstractions this codebase implements, written so a motivated
reader can learn rendering from scratch by reading top-to-bottom *or*
navigate sideways via cross-links to follow a single thread.

It complements — does not replace — the per-class Doxygen reference. Doxygen
is the lookup table; this book is the narrative.

### Three concrete promises

1. **Textbook quality.** Each chapter has a coherent narrative arc:
   motivation → math → algorithm → implementation → variants → exercises.
   It reads as prose, not as bullet lists of facts.

2. **Visual-first.** Every algorithm with a non-obvious geometric idea is
   accompanied by an interactive widget (reused from `scripts/docs/*.js`)
   or a rendered image (reused from `docs/images/`). New widgets only when
   the existing set leaves a gap.

3. **Source-grounded.** When the prose introduces a class, function, or
   test, it links to the exact file. The reader can always click through to
   the real implementation. Code references use a stable form (described
   in §3).

### Non-goals

- **Not a paper survey.** We cite the foundational papers (Pineda 1988,
  Whitted 1980, MacDonald & Booth 1990, etc.) but the explanations are
  written for a programmer who wants to build, not a researcher who wants
  to compare.
- **Not exhaustive.** Topics queued in
  [`docs/topics-backlog.md`](../topics-backlog.md) (path tracing, ReSTIR,
  photogrammetry, NeRF/3DGS, etc.) are out of scope until they exist in
  the codebase. The book documents what *is*, not what's planned.
- **Not a Doxygen replacement.** Class-by-class API walkthroughs stay in
  the per-header `@brief` / `@param` documentation. The book talks about
  *ideas*.

### Audience

Computer-graphics-curious software engineers. The book assumes:

- Comfort with C++ (classes, templates, STL containers, smart pointers).
- High-school + first-year-undergrad math (vectors, dot/cross products,
  basic trigonometry, solving quadratics).
- No prior graphics knowledge beyond knowing what a pixel is.

Things explained in-line as needed: barycentric coordinates, homogeneous
coordinates, Snell's law, BRDFs, signed distance, the rendering equation
(briefly). Things linked but not derived: the underlying calculus /
probability that backs Monte Carlo sampling (covered when path tracing
lands).

---

## 2. Volumes and chapters — outline

Eight volumes, twenty-eight chapters, plus an appendix. Each chapter lists:
**arc** (the narrative spine), **widgets reused**, **images reused**,
**source anchors** (the files the chapter is "about"), and **new
artifacts** (widgets/renders that don't exist yet and need to ship with
the chapter).

### Files and naming

```
docs/markdown/
  README.md                          ← top-level TOC + reading paths
  preface.md
  foundations/
    README.md                        ← volume TOC
    numbers-and-vectors.md
    matrices-and-transforms.md
    rays-and-geometry.md
    color-and-buffers.md
  ray-rendering/
    README.md
    the-whitted-pipeline.md
    cameras.md
    primitives-and-intersection.md
    materials-and-brdfs.md
    lights-and-shading.md
    sampling-and-anti-aliasing.md
    textures.md
    tone-mapping.md
  scene-structure/
    README.md
    view-planes.md
    csg.md
    spatial-acceleration.md
    instances-and-motion-blur.md
  rasterization/
    README.md
    tessellation.md
    the-rasterization-pipeline.md
    clipping-depth-stencil.md
    wireframe-rendering.md
    msaa-and-attribute-interpolation.md
  image-and-vision/
    README.md
    image-buffers-and-pixel-formats.md
    blob-analysis-and-silhouettes.md
    shape-classification.md
  tools-and-io/
    README.md
    ply-parsing.md
    tools-and-modeler.md
  render-graph/
    README.md
    render-plans-and-resources.md
  animation/
    README.md
    timelines-and-interpolation.md
  appendix/
    a-glossary.md
    b-bibliography.md
    c-source-map.md                  ← generated; see §4
```

Filenames are stable kebab-case slugs without numeric prefixes. Reading order
lives in the top-level and per-volume `README.md` files. Volume folders carry
their own README so a visitor can drop in mid-book.

### Foundations

The math and data structures everything else stands on. Anything visible
in 2D — points, vectors, transforms, rays — gets a widget.

#### Numbers and vectors

- **Arc.** Why we use `double` (and when `float` is fine), `Vector<N, T>`
  as a uniform abstraction, the SSE3 `Vector3<double>` /
  `Vector4<double>` specializations, basic operators, dot/cross products,
  length and normalization. Closes with the *unit-length invariant* —
  most ray code assumes normalized directions.
- **Widgets reused.** None — vector ops are easier read than seen.
- **Images reused.** None.
- **Source anchors.**
  [`include/core/math/Vector.h`](../../include/core/math/Vector.h),
  [`include/core/math/vector/sse3/`](../../include/core/math/vector/sse3/).
- **New artifacts.** None. Optionally a tiny dot-product widget showing
  cosine for two draggable arrows.

#### Matrices and transforms

- **Arc.** 4×4 homogeneous matrices, the difference between a *point*
  and a *direction* under transformation, the inverse-transpose for
  normals, the composition order convention, and where the codebase
  keeps these (`Matrix.h` + `Transformable` mixins). Quaternions get a
  paragraph as the alternative for orientation-only.
- **Widgets reused.** `instance_transform_normals.js`.
- **Images reused.** None.
- **Source anchors.**
  [`include/core/math/Matrix.h`](../../include/core/math/Matrix.h),
  [`include/core/math/Quaternion.h`](../../include/core/math/Quaternion.h),
  [`include/render/primitives/Instance.h`](../../include/render/primitives/Instance.h).
- **New artifacts.** None.

#### Rays and geometry

- **Arc.** `Ray` as origin + direction + parametric `at(t)`, why we
  parameterize by `t` (the universal currency of ray–object
  intersection), `HitPoint` and `HitPointInterval` as the data flowing
  back from intersection tests, axis-aligned bounding boxes, ranges,
  the `Range<T>` helper for clipping intervals. Lays the groundwork
  for Primitives and intersection.
- **Widgets reused.** `ray_at.js`, `ray_class.js`, `ray_project.js`,
  `bounding_box_class.js`, `bounding_box_include.js`,
  `bounding_box_grown_by.js`, `bounding_box_moved_by.js`,
  `bounding_box_and.js`, `bounding_box_or.js`, `hitpoint_class.js`.
- **Images reused.** None — pure geometry chapter.
- **Source anchors.**
  [`include/core/math/Ray.h`](../../include/core/math/Ray.h),
  [`include/core/math/HitPoint.h`](../../include/core/math/HitPoint.h),
  [`include/core/math/HitPointInterval.h`](../../include/core/math/HitPointInterval.h),
  [`include/core/math/BoundingBox.h`](../../include/core/math/BoundingBox.h),
  [`include/core/math/Range.h`](../../include/core/math/Range.h).
- **New artifacts.** None.

#### Color and buffers

- **Arc.** `Colord` as four floats, the linear-RGB-vs-sRGB distinction,
  HDR vs LDR, `Buffer<T>` as the memory backing for a 2D image,
  packed-pixel conversion (`Colord::rgb()` to `unsigned int`). Sets up
  the framebuffer end of the rendering pipeline.
- **Widgets reused.** `color_model_conversions.js`.
- **Images reused.** None.
- **Source anchors.**
  [`include/core/Color.h`](../../include/core/Color.h),
  [`include/core/Buffer.h`](../../include/core/Buffer.h),
  [`include/core/color/sse3/`](../../include/core/color/sse3/).
- **New artifacts.** None.

### Ray rendering

The Whitted pipeline as it actually runs in `engine::raytracer::Raytracer`.
Each chapter ends with a "what this changes in the rendered image" section
that shows a reference image with and without the feature.

#### The Whitted pipeline

- **Arc.** A complete tracer in 200 words: cast primary rays, find
  closest hit, shade with material + lights, recurse for reflections /
  refractions, composite to the buffer. Then expand each step over the
  chapters that follow. Maps each step to the file that owns it.
- **Widgets reused.** None — this is an architecture chapter.
- **Images reused.** Several small "before/after" thumbnails to motivate
  later chapters.
- **Source anchors.**
  [`include/engine/raytracer/Raytracer.h`](../../include/engine/raytracer/Raytracer.h),
  [`src/engine/raytracer/Raytracer.cpp`](../../src/engine/raytracer/Raytracer.cpp).
- **New artifacts.** Possibly an architecture-flow SVG (static).

#### Cameras

- **Arc.** The pinhole as the canonical model, then the family —
  orthographic, spherical, fisheye, equirectangular, tilt-shift,
  thin-lens. For each: what it physically models, what it costs, and
  how the codebase wires it in via the `Camera` interface +
  `CameraFactory`. Depth-of-field via thin-lens + sampler interaction
  is the centerpiece.
- **Widgets reused.** `camera_forward_projection.js`,
  `wide_angle_camera_mappings.js`, `thin_lens_camera_disc_sampling.js`,
  `thin_lens_camera_convergence.js`, `tilt_shift_camera_scheimpflug.js`.
- **Images reused.** Doc renders for each camera class
  (`docs/images/pinhole_camera_*.png`,
  `docs/images/thin_lens_camera_*.png`, etc.).
- **Source anchors.** [`include/render/cameras/`](../../include/render/cameras/).
- **New artifacts.** None.

#### Primitives and intersection

- **Arc.** The primitive interface, then per-shape: sphere (analytic
  quadric), plane, box (slab method), triangle (Möller-Trumbore), disk
  and open cylinder, torus (quartic root finding). Each gets the math,
  the hit-point construction, and the place where UVs get filled in.
- **Widgets reused.** `mesh_triangle_interpolation.js`,
  `sphere_farthest_point.js`, `box_farthest_point.js`,
  `convex_hull_farthest_point.js`, `support_mapping_gjk.js` (forward
  reference to ch. 14).
- **Images reused.** Per-primitive doc renders.
- **Source anchors.**
  [`include/render/primitives/Primitive.h`](../../include/render/primitives/Primitive.h)
  + every concrete primitive.
- **New artifacts.** Possibly a sphere-ray-intersection widget showing
  the discriminant geometry — only if needed; the Möller-Trumbore
  widget already exists and the sphere math is short.

#### Materials and BRDFs

- **Arc.** What a material is (a `shade()` function over a hit), what a
  BRDF is (the directional reflectance distribution), how Matte / Phong
  / Reflective / Transparent / Portal compose primitive BRDFs / BTDFs.
  Pins the new `BSDF` interface as the container abstraction. The
  material parameter space (ambient/diffuse/specular coefficients) gets
  concrete with sweep tables.
- **Widgets reused.** `phong_lambertian_lobes.js`,
  `reflective_material_recursion.js`,
  `transparent_material_refraction.js`,
  `portal_material_ray_redirection.js`.
- **Images reused.** Material doc renders
  (`docs/images/matte_material_*.png`, `docs/images/phong_material_*.png`,
  reflective / transparent sweeps).
- **Source anchors.**
  [`include/render/materials/`](../../include/render/materials/),
  [`include/render/brdf/`](../../include/render/brdf/),
  [`include/render/bsdf/BSDF.h`](../../include/render/bsdf/BSDF.h).
- **New artifacts.** None.

#### Lights and shading

- **Arc.** Point lights, directional lights, the shadow ray, ambient as
  the cheap GI hack, why no area lights yet (stochastic shadow sampling
  lands with path tracing). Pins the geometric shadow-boundary contract
  via the PointLight functional test.
- **Widgets reused.** None directly (Phong widget covers shading-side).
- **Images reused.** Point-light and directional-light doc renders.
- **Source anchors.** [`include/render/lights/`](../../include/render/lights/).
- **New artifacts.** Maybe a shadow-ray geometry diagram (static SVG)
  showing the umbra/penumbra of a point source.

#### Sampling and anti-aliasing

- **Arc.** Why one ray per pixel produces aliasing, regular vs jittered
  vs random samplers as the three classical answers, the stratification
  guarantee (pinned by the new
  `JitteredSampler.EachStratumGetsExactlyOneSamplePerSet` unit test),
  multi-sample-per-pixel as a Monte Carlo integral over pixel area.
  Connects to Cameras via the lens sampler reuse.
- **Widgets reused.** `sampler_streams.js`.
- **Images reused.** Sampler doc renders (low-vs-high-spp comparison).
- **Source anchors.**
  [`include/render/samplers/`](../../include/render/samplers/),
  [`test/unit/render/samplers/JitteredSamplerTest.cpp`](../../test/unit/render/samplers/JitteredSamplerTest.cpp).
- **New artifacts.** None.

#### Textures

- **Arc.** `Texture` as a function `(s, t) → Colord`, constant / checker
  / UV-color / image (when it lands), mappings (planar, UV-direct), and
  how textures plug into materials.
- **Widgets reused.** `texture_coordinate_mapping.js`.
- **Images reused.** Checker-texture doc renders.
- **Source anchors.** [`include/render/textures/`](../../include/render/textures/).
- **New artifacts.** None.

#### Tone mapping

- **Arc.** Why HDR exists, the float framebuffer pipeline (post-R1),
  Linear / Reinhard / ACES (Narkowicz fit) as a sweep across compression
  strengths. Pins the cross-operator monotonicity contract via
  `TonemapMonotonicityTest`.
- **Widgets reused.** `tonemap_curves.js`.
- **Images reused.** Tonemap sweep doc renders.
- **Source anchors.** [`include/render/tonemap/`](../../include/render/tonemap/).
- **New artifacts.** None.

### Scene structure

#### View planes

- **Arc.** What a view plane is, sample iteration order (row-major vs
  tiled vs interlaced vs shuffled), and why shuffled orders matter for
  progressive display.
- **Widgets reused.** `viewplane_iteration_order.js`.
- **Images reused.** None directly — the iteration order is a temporal
  property.
- **Source anchors.** [`include/render/viewplanes/`](../../include/render/viewplanes/).
- **New artifacts.** None.

#### Constructive solid geometry (CSG)

- **Arc.** Hit intervals as the unifying abstraction, union /
  intersection / difference set operations on intervals, Minkowski-sum
  and convex-hull as the support-mapping family, GJK as the algorithm
  that links them all.
- **Widgets reused.** `csg_hit_intervals.js`, `support_mapping_gjk.js`.
- **Images reused.** CSG doc renders (`difference`, `union`,
  `intersection`, `minkowski_sum`, `convex_hull`).
- **Source anchors.** CSG primitives in
  [`include/render/primitives/`](../../include/render/primitives/).
- **New artifacts.** None.

#### Spatial acceleration

- **Arc.** Why a flat list of primitives is O(N) per ray, the bounding
  volume hierarchy as the divide-and-conquer answer, the Surface Area
  Heuristic for split selection, the uniform grid via DDA traversal as
  the "fixed cost per axis" alternative.
- **Widgets reused.** `bvh_sah_traversal.js`, `grid_dda_traversal.js`.
- **Images reused.** None — acceleration is invisible at the pixel
  level (modulo timing).
- **Source anchors.**
  [`include/render/primitives/BVH.h`](../../include/render/primitives/BVH.h),
  [`include/render/primitives/Grid.h`](../../include/render/primitives/Grid.h),
  [`test/unit/render/primitives/BVHPerformanceTest.cpp`](../../test/unit/render/primitives/BVHPerformanceTest.cpp).
- **New artifacts.** None.

#### Instances and motion blur

- **Arc.** How `Instance` lets one mesh appear with N transforms (the
  classic wins: trees, asteroid fields), why normals need the
  inverse-transpose, motion blur as a velocity per instance integrated
  over shutter time.
- **Widgets reused.** `instance_transform_normals.js`,
  `motion_blur_time_sampling.js`.
- **Images reused.** Motion-blur doc renders.
- **Source anchors.**
  [`include/render/primitives/Instance.h`](../../include/render/primitives/Instance.h).
- **New artifacts.** None.

### Rasterization

#### Tessellation

- **Arc.** Why a rasterizer needs triangles where the raytracer needs
  implicit surfaces, the `tessellate(int lod) → Mesh` contract, the
  per-primitive strategies (sphere ↦ subdivided icosahedron / lat-long
  grid, disk ↦ triangle fan, cylinder ↦ quad strip with seam-duplicate
  UVs, etc.), how `Composite` and `Instance` recurse.
- **Widgets reused.** `sphere_tessellate.js`, `disk_tessellate.js`,
  `open_cylinder_tessellate.js`, `torus_tessellate.js`.
- **Images reused.** Per-primitive tessellation doc renders at multiple
  LODs.
- **Source anchors.**
  [`include/core/geometry/Mesh.h`](../../include/core/geometry/Mesh.h),
  every primitive's `tessellate` override.
- **New artifacts.** None.

#### The rasterization pipeline

- **Arc.** End-to-end edge-function rasterization (Pineda 1988):
  vertex transform → clip-space culling → rasterize triangle → depth
  test → fragment shade → write framebuffer. Each stage maps to its
  function in `Rasterizer.cpp`. The fixed-point edge stepping
  (`PreparedRasterTriangle`) gets its own subsection.
- **Widgets reused.** `rasterizer_pipeline.js`,
  `rasterizer_perspective_uv.js`, `rasterizer_clip_attributes.js`.
- **Images reused.** Rasterizer-vs-raytracer doc renders.
- **Source anchors.**
  [`include/core/geometry/Rasterize.h`](../../include/core/geometry/Rasterize.h),
  [`include/engine/raster/Rasterizer.h`](../../include/engine/raster/Rasterizer.h),
  [`src/engine/raster/Rasterizer.cpp`](../../src/engine/raster/Rasterizer.cpp).
- **New artifacts.** None.

#### Clipping, depth, stencil

- **Arc.** Sutherland-Hodgman in homogeneous clip space (and why), the
  z-buffer as the visibility solver, stencil as the per-pixel mask /
  marker, configurable depth-func / depth-write / stencil-op /
  cull-mode state. Tied to the textbook "fixed-function pipeline" the
  rasterizer is teaching.
- **Widgets reused.** `rasterizer_depth_stencil_cull.js`,
  `rasterizer_clip_attributes.js`.
- **Images reused.** Stencil + cull doc renders.
- **Source anchors.**
  [`include/render/HomogeneousClipVolume.h`](../../include/render/HomogeneousClipVolume.h),
  [`include/engine/raster/Rasterizer.h`](../../include/engine/raster/Rasterizer.h).
- **New artifacts.** None.

#### Wireframe rendering

- **Arc.** A second rasterization engine, this time edge-only:
  Bresenham in screen space after the same projection step the filled
  rasterizer uses. Frames the wireframe engine as the simplest
  rasterizer, useful for editor previews + structural debugging.
- **Widgets reused.** None directly.
- **Images reused.** Wireframe doc renders.
- **Source anchors.**
  [`include/core/geometry/Bresenham.h`](../../include/core/geometry/Bresenham.h),
  [`include/engine/wireframe/Wireframe.h`](../../include/engine/wireframe/Wireframe.h),
  [`test/functional/engine/wireframe/WireframeTest.cpp`](../../test/functional/engine/wireframe/WireframeTest.cpp).
- **New artifacts.** None.

#### MSAA and attribute interpolation

- **Arc.** The single-tile vs N-sample-resolved framebuffer split,
  perspective-correct UV interpolation (Heckbert-Moreton 1/z trick),
  why MSAA samples coverage but not shading.
- **Widgets reused.** `rasterizer_msaa_coverage.js`,
  `rasterizer_perspective_uv.js`.
- **Images reused.** 1×-vs-4× MSAA doc renders.
- **Source anchors.**
  [`src/engine/raster/Rasterizer.cpp`](../../src/engine/raster/Rasterizer.cpp)
  (the MSAA resolve loop).
- **New artifacts.** None.

### Image processing & computer vision

The new pillar (`docs/roadmap.md` §4.11). Currently small; will grow as
the §4.11 backlog lands.

#### Image buffers and pixel formats

- **Arc.** `Buffer<T>` revisited as the lingua franca of image
  processing, pixel iteration patterns, the hand-off from rendering
  output to CV input. Sets up vol V's contract with vol IV.
- **Widgets reused.** None (revisit `color_model_conversions.js`).
- **Images reused.** None.
- **Source anchors.**
  [`include/core/Buffer.h`](../../include/core/Buffer.h),
  [`test/helpers/Blob.h`](../../test/helpers/Blob.h) (the consumer).
- **New artifacts.** None.

#### Blob analysis and silhouettes

- **Arc.** Connected-component flood-fill, what counts as "connected"
  (4 vs 8 neighborhood, color-equivalence), silhouette extraction as
  outer-extreme sampling. Concrete consumer: the test-side `Blob` /
  `Silhouette` helpers replacing the old `ShapeRecognition`.
- **Widgets reused.** None yet; new artifact below.
- **Images reused.** Could repurpose any rendered sphere image as the
  input.
- **Source anchors.**
  [`test/helpers/Blob.h`](../../test/helpers/Blob.h),
  [`test/helpers/Silhouette.h`](../../test/helpers/Silhouette.h).
- **New artifacts.** **`scripts/docs/connected_components.js`** — load
  a small example raster, step through BFS flood-fill with a speed
  slider, color components by ID. *(New widget.)*

#### Shape classification

- **Arc.** From silhouette descriptors (radial variance, bounding-box
  aspect ratio) to predicates (`isCircle`, `isRectangle`). The
  decision boundaries the codebase actually ships with, why those
  thresholds, what fails them.
- **Widgets reused.** None yet; new artifact below.
- **Images reused.** None.
- **Source anchors.**
  [`test/helpers/ShapeClassifier.h`](../../test/helpers/ShapeClassifier.h).
- **New artifacts.** **`scripts/docs/shape_descriptors.js`** —
  draggable polygon → live readout of radial variance, BB aspect
  ratio, classifier output. *(New widget.)*

### Tools & I/O (light coverage)

#### PLY parsing

- **Arc.** ASCII vs binary PLY, element/property declarations, why
  this is the project's only untrusted-input surface and how the
  fuzzer keeps it honest.
- **Widgets reused.** None.
- **Images reused.** None.
- **Source anchors.**
  [`src/core/formats/ply/PlyFile.cpp`](../../src/core/formats/ply/PlyFile.cpp),
  [`fuzz/`](../../fuzz/).
- **New artifacts.** None.

#### Tools and the Modeler

- **Arc.** Tour of `rendercli` (headless), `src/modeler`
  (interactive, full editor surface), reusable scene JSON, and how the
  engine selector wires in.
- **Widgets reused.** None.
- **Images reused.** None.
- **Source anchors.**
  [`tools/rendercli/rendercli.cpp`](../../tools/rendercli/rendercli.cpp),
  [`src/modeler/`](../../src/modeler/), [`scenes/`](../../scenes/).
- **New artifacts.** None.

### Appendix

- **A. Glossary.** ~50 terms (BRDF, BTDF, BSDF, Whitted, Pineda, Möller-
  Trumbore, MSAA, SAH, DDA, etc.) with one-line definitions and forward
  links to the chapter that introduces them. Hand-written.
- **B. Bibliography.** Numbered citations for the foundational papers
  (Whitted 1980, Pineda 1988, Möller-Trumbore 1997, MacDonald & Booth
  1990, Heckbert-Moreton 1991, Narkowicz 2015 ACES, etc.). Hand-written.
- **C. Source map.** A reverse index from chapters → source files —
  tells a reader who lands on `Rasterizer.cpp` which chapters are about
  it. **Generated** (see §4).

---

## 3. Conventions

### Cross-linking

- Plain markdown relative links:
  `[Vectors](../foundations/numbers-and-vectors.md#dot-product)`.
- Each chapter ends with a **See also** section pointing at sibling
  chapters.
- Concepts mentioned out-of-order get inline forward references with a
  short parenthetical: "...applies the *Möller-Trumbore* test ([covered in
  Primitives and intersection](../ray-rendering/primitives-and-intersection.md#triangle-moller-trumbore))".
- Anchor IDs are explicit (`<a id="dot-product"></a>`) for stability —
  a later edit that retitles a heading shouldn't break inbound links.

### Source references

Two forms, used together:

- **Inline pointers** in prose: `Vector<3, double>` lives in
  [`include/core/math/Vector.h`](../../include/core/math/Vector.h).
- **Code blocks lifted from the codebase** carry a fenced caption:

  ```cpp
  // include/core/math/Ray.h:42
  Vector3d at(double t) const { return m_origin + m_direction * t; }
  ```

  No copy-pasting full classes — pull the 3-10 lines that illustrate
  the point and link to the file for the rest.

- **Test references** when a contract is pinned by a specific test,
  e.g.  *"The bit-identical render contract is pinned by
  [`SamplerDeterminismTest.RegularSamplerProducesBitIdenticalRenders`](../../test/functional/render/samplers/SamplerDeterminismTest.cpp)."*

### Prose style

The book is written for serious readers, so the prose should be
written in serious sentences. Default to complete sentences with a
subject and a verb; avoid noun-only fragments
(*"Tiny class, used everywhere."*), verb-only fragments
(*"Stores a `Colord`."*), and label-style one-word lead-ins
(*"Code:"*) for anything more substantial than a one-line caption.
Short, declarative sentences are welcome; truncated ones are not.
The tone should sit a notch below an academic textbook — direct,
no jargon hedging, no false-friendly contractions — but well above
a stream of bullet-point notes.

### Math notation

KaTeX-style fenced math (`$$ ... $$` for display, `$...$` inline). The
publish flow (§4) renders these to HTML; the GitHub native renderer also
supports the syntax for in-repo viewing. Equations are numbered only
when later text refers back to them — bare equations stay clean.

### Embedded widgets

Widgets must render inline in the published book — readers shouldn't
have to follow a "live demo" link to interact. The publish flow
(§4) is responsible for turning each widget reference into actual
running JavaScript:

```markdown
<!-- widget: rasterizer_pipeline -->
```

The build step replaces that comment with an iframe (or a
script-embedded container) that loads `figure.js` plus
`scripts/docs/<name>.js`. The exact mechanism (iframe per widget vs
shared host iframe vs inline `<script>`) is a publish-flow detail; the
book source uses the comment marker so the markdown stays portable.

Until the publish flow exists, chapters can use a static fallback —
embedded screenshot + `<!-- widget: ... -->` comment — and the
build step substitutes the live version when it's wired up.

**The widget must match the surrounding prose exactly.** A widget
that demonstrates more than the prose introduces is a distraction
— the reader stops to figure out what the extra controls do, then
comes back wondering whether they missed something. Read each
widget's `scripts/docs/*.js` source before placing it: if the
widget covers concepts A, B, and C but the chapter section only
introduces concept A, embed it in the section that covers all
three (or skip the embed and let the prose carry the section
unaided). When in doubt, no widget is better than a too-complex
one.

### Rendered images

Images live in `docs/images/` and are rendered via `rake docs:render`.
The book references them by relative path:

```markdown
![Pinhole vs orthographic](../../docs/images/pinhole_vs_orthographic.png)
```

When the book introduces a *new* educational image, the corresponding
`scripts/docs/<topic>.rb` driver gets a new `class_doc` block and
`rake docs:render` regenerates. The book PR ships the driver change
and the `@image html` reference; the rendered PNG lands separately
via the docs render flow.

---

## 4. Tooling and publish flow

### `rake docs:textbook:check` *(static checks; lands with scaffolding)*

A Ruby walker over `docs/markdown/` that validates:

- **Link resolution.** Every `[text](path)` resolves to an existing
  file; every anchor `(...#anchor)` resolves to an explicit `<a id>`
  or a heading slug.
- **Source-anchor freshness.** Every chapter's "Source anchors"
  section lists files that exist; renamed/deleted files surface as
  errors.
- **Chapter graph consistency.** The top-level Contents chapter order must
  match the concatenated per-volume `## Chapters` lists, and every chapter file
  must be reachable from both surfaces exactly once.
- **Widget references.** Every `<!-- widget: foo -->` comment has a
  matching `scripts/docs/foo.js`.
- **Image references.** Every `![](docs/images/foo.png)` has an
  `@image html foo.png` somewhere in `include/` or
  `scripts/docs/`. (Reuses the existing `rake check:doc-images` plumbing.)

Failures are listed; the task exits non-zero. Wired into CI alongside
`rake check:cpp`.

### `rake docs:textbook:source-map` *(generates appendix C)*

Walks `docs/markdown/` collecting every `Source anchors:` entry,
inverts the mapping, and writes
`docs/markdown/appendix/c-source-map.md` as a sorted table:

```markdown
| Source file                                  | Chapters           |
|---------------------------------------------|--------------------|
| include/core/math/Ray.h                     | 3, 5, 7            |
| include/render/cameras/PinholeCamera.h      | 6                  |
| ...                                         | ...                |
```

The output is committed (so it shows up on GitHub Pages) but
generated; `docs:textbook:check` flags drift between source and
generated form.

### `rake docs:textbook:html` *(publishes the book)*

The publish flow that turns `docs/markdown/` into a static site:

1. Run a markdown → HTML pass (existing toolchain TBD —
   `redcarpet`-style Ruby gem, or pandoc, or `mkdocs`; pick during
   the publish-flow PR).
2. Resolve `<!-- widget: foo -->` markers into iframe / script
   embeds that load `scripts/docs/foo.js` from the same site.
3. Render KaTeX math (server-side via `katex` CLI, or client-side
   via the standard auto-render JS — pick during the same PR).
4. Copy referenced images from `docs/images/` into the output tree.
5. Drop the result under `docs/html/textbook/` so the existing
   GitHub Pages workflow picks it up.

The publish flow lives behind its own task because the book stays
useful (and reviewable) as plain markdown for as long as we don't
need it embedded in the public Pages site. Wiring it up is a
follow-up to the scaffolding PR, not a blocker for chapter work.

### `rake docs:textbook` *(convenience wrapper)*

Runs `docs:textbook:check`, `docs:textbook:source-map`, and
`docs:render` (for the embedded images). Used by both CI and local
authors before pushing.

---

## 5. Workflow & sequencing

### Order of writing

The book is written in a sensible reading order, **not** all at once.
Suggested sequencing (1 chapter ≈ 1 PR):

1. **Scaffolding first.** Land `docs/markdown/README.md`,
   `docs/markdown/preface.md`, the per-volume `README.md` files —
   empty TOCs the chapters slot into — plus
   `rake docs:textbook:check` and `:source-map`.
2. **Foundations** — short, high-density, sets the notation. Done before
   anything that depends on it.
3. **Ray rendering** — the canonical Whitted pipeline. Heaviest
   cross-linking happens here.
4. **Scene structure** — uses the ray-rendering vocabulary.
5. **Rasterization** — could ship in parallel with Scene structure since the
   rasterizer mostly stands alone after Foundations.
6. **Image processing and computer vision** — adds the two image-analysis
   widgets.
7. **Tools and I/O** — light coverage; last because they're the
   lowest-leverage.
8. **Render graph and animation** — added after the initial textbook plan to
   document implemented graph planning and timeline features.
9. **Publish flow.** `rake docs:textbook:html` lands when the
   reader experience needs the embedded widgets / KaTeX math (and
   not before — keeps the markdown reviewable on GitHub natively
   in the meantime).
9. **Appendix.** Glossary + bibliography + (auto-regenerated)
   source map.

Each chapter PR ships:

- The chapter `.md` file.
- Any new `scripts/docs/*.js` widget required (with its JS-side
  test).
- The `class_doc` driver entry if a new rendered image is needed.
- A CHANGELOG entry under "Added".
- Crosslinks added to neighbour chapters' "See also" sections.
- An `appendix/c-source-map.md` regeneration via
  `rake docs:textbook:source-map`.

---

## 6. Keeping the book in sync

A textbook that lags the codebase loses its trust. Two complementary
mechanisms keep them aligned.

### Mechanism 1 — `CLAUDE.md` invariant

A new section in [`CLAUDE.md`](../../CLAUDE.md), under the existing
"Adding a new visible-output feature" block: **"Keeping the textbook
in sync."** It codifies that an agent (or human) editing the codebase
must consider chapter impact in the same PR. Specifically:

- **Renamed / moved / deleted source files** that appear under a
  chapter's `Source anchors` list → the chapter's anchors get updated
  in the same PR. `rake docs:textbook:check` catches regressions.
- **New public class, algorithm, or visible behaviour** — if it fits
  inside an existing chapter's narrative arc, the chapter gets a
  paragraph + source pointer. If it opens a new topic, the editor
  files an issue (or adds a `## TODO: chapter ...` line at the end
  of the closest-matching chapter) for follow-up.
- **New `scripts/docs/*.js` widget** → either embed it in an
  existing chapter or note it under "candidate embeds" in the
  closest-matching chapter; never let a new widget land without a
  pointer from the book.
- **Removed widget or rendered image** referenced from the book →
  PR removes the reference. `rake docs:textbook:check` flags
  dangling references.

The rule lives next to the rest of the agent-facing conventions so
it's loaded into context for every code-touching session.

### Mechanism 2 — periodic sweep

A scheduled task, if added, should run `rake docs:textbook:check` plus a
broader scan that:

- Lists `include/**/*.h` files that aren't in any chapter's
  `Source anchors` list.
- Lists `scripts/docs/*.js` widgets that aren't referenced from any
  chapter.
- Lists `*Test.cpp` files added since the last sweep that pin
  contracts mentioned by chapters but aren't linked.
- Diffs the publicly-exported types in each chapter's anchors
  against what they were when the chapter last saw an edit
  (heuristic: `git log -- <file>` since the chapter's last commit
  modifies it).

Output is a punch list the agent (or human) can work through. The
sweep doesn't auto-edit the book; it surfaces drift. The
agent-facing rule from Mechanism 1 stays the primary defence — the
sweep is the safety net.

There is no dedicated audit task today. The implemented surfaces are
`rake docs:textbook:check`, `rake docs:textbook:source-map`, and the wrapper
`rake docs:textbook`.
