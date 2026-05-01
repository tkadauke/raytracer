# Feature Roadmap — `tkadauke/raytracer`

> **Scope:** vision, architecture, and feature roadmap for evolving this project from a Whitted-style raytracing library into a complete 3D content creation tool.
> **Date:** 2026-05-01
> **Status:** Draft — opened for iteration.
> **Companion doc:** [`modernize.md`](modernize.md) covers build/CI/Qt6/supply-chain modernization. This document covers feature, capability, and architectural direction. The two are independent and can advance in parallel.

---

## 1. Vision — the North Star

A renderer is the *core*, not the *whole product*. The goal is a working 3D content creation tool that:

- ships several rendering backends (wireframe, software raster, OpenGL preview, CPU raytracer, CPU path tracer) over a single scene representation,
- carries a rich material library with physically-based BSDFs, subsurface scattering, and volumetrics,
- imports and exports the formats the rest of the world uses (OBJ, glTF, USD, EXR, PLY, STL),
- offers a Qt-based modeling UI with multi-viewport editor, object outliner, gizmos, property panels, and a material browser,
- exposes the scene to an **AI agent** that can construct, modify, and critique scenes through natural-language chat — including writing parametric scripted objects (OpenSCAD-style),
- supports **animated rendering** with a timeline, keyframes, motion blur, and video output,
- can **distribute rendering across a Kubernetes cluster** for animation throughput.

The scope is "Blender-lite with an LLM in the side panel." The differentiator is the AI-native authoring loop on top of a clean, testable, physically-grounded renderer.

This document is a roadmap, not a commitment. Order and scope are negotiable; the prerequisites are not.

---

## 2. Current state — honest assessment

### What's already solid (keep, build on)

- **Pluggable strategy interfaces.** `Camera`, `Material`, `Primitive`, `BRDF/BTDF`, `Sampler`, `ViewPlane`, `Texture`, `Light` are all abstract bases. New shapes, materials, and cameras drop in additively.
- **Templated math + SSE3 specializations** under `core/math/vector/sse3/` and `core/color/sse3/`. Production-grade. Low-touch.
- **Constructive Solid Geometry** (`Composition`, `Union`, `Intersection`, `Difference`, `Instancing`) already works for implicit ray-traced primitives. Uncommon for a hobby project; reuse rather than rewrite.
- **Tile-based parallel rendering** via `QThreadPool` already gives clean horizontal scaling within a host. Per-tile dispatch makes distributed rendering a small extension.
- **Test discipline.** Unit + functional suites, LibFuzzer harness on the PLY parser, Google Benchmark on the SSE3 hot paths. CMake/Ninja, GitHub Actions CI, Doxygen pages, Dependabot, Dockerfile. Stronger engineering hygiene than most renderers.
- **Quartic intersector** for the torus is correct and numerically stable in the regimes tested.

### What needs reinforcement before features can stack

- **`Buffer<unsigned int>` LDR pipeline.** Fine for Whitted; fatal for path tracing. PT needs a float accumulator, with tonemapping at the end.
- **Whitted-only integrator hard-coded into `Raytracer::rayColor`.** No Monte Carlo integration. No Multiple Importance Sampling. No proper area-light shadow rays. No motion blur. The integrator and the engine are conflated.
- **Phong-only direct lighting, no Fresnel.** Reflection and transmission coefficients are scalar constants set on the material — physically wrong everywhere except at normal incidence. PT-class materials need Fresnel at every interface.
- **No nested-medium tracking.** `PerfectTransmitter` hardcodes "the other side is air, IOR = 1." Glass-inside-glass scenes will be wrong.
- **No UV / tangent frame on `HitPoint`.** Blocks textures with non-trivial mapping, normal maps, anisotropic BRDFs.
- **Default recursion depth and black-on-truncation** — being addressed by the in-flight TIR / truncation PRs (#35, #36).

### Existing seams that just need extension

- New `Primitive`s: drop in, register, done.
- New `Material`s / `BRDF`s / `BTDF`s: same.
- New `Camera`s: same.
- The `widgets/` module is cleanly separated from the renderer — UI grows without touching the math.
- `examples/` show the scene-construction pattern; a parametric DSL slots into the same shape.

### Missing seams (new abstractions)

These don't exist and are blockers for entire feature classes:

- **No `RenderEngine` abstraction.** `Raytracer` *is* the engine. Adding wireframe/software/GL needs a level above.
- **No `Mesh` interface that all primitives can produce.** `Primitive::tessellate(LOD) → Mesh` does not exist. Blocks the GL viewport, mesh export, and any rasterization-based engine.
- **`Light` is not first-class enough.** Lights exist but aren't sampled for soft shadows or MIS — no `Light::sample(point) → (direction, pdf, intensity)`.
- **No scene serialization.** Scenes are constructed in C++ or via scripts. Nothing round-trips through a file. Blocks UI save/load, file import/export workflows, distributed rendering, and AI-agent state persistence.
- **No scene-object handle/ID system.** UI selection, undo, AI agent tool calls all need stable references. Today, objects are bare pointers.
- **No time dimension** on the scene graph. Blocks animation.
- **`BSDF` is not separated from `Material::shade`.** Today shading is `Material::shade(raytracer, ray, hit, state) → Color` — tightly coupled to the recursive raytracer's call style. Path tracing wants `BSDF::eval(wi, wo)`, `BSDF::sample(wi)`, `BSDF::pdf(wi, wo)` with the integrator owning the recursion.

---

## 3. Foundational refactors — the six prerequisites

These six refactors unlock roughly 80% of the north-star features. Done as small focused PRs, each independently shippable.

### R1. Float HDR framebuffer + tonemap stage

`Buffer<Colord>` accumulator; existing `Buffer<unsigned int>` becomes a display target produced by a tonemap pass (Reinhard or ACES, configurable).

- Estimated effort: ~2 days.
- Unblocks: path tracing, motion blur, EXR output, distributed-tile aggregation.

### R2. Stable scene-object IDs

Every node in the scene graph gets a typed `SceneObjectId` (integer or UUID). `Scene::find(id) → Object*` lookup. Optional `Scene::name(id)` for debug.

- Estimated effort: ~1 day.
- Unblocks: UI selection, undo/redo, AI agent tool calls, save-to-disk references.

### R3. Scene serialization (YAML or JSON, v1)

Round-trip primitives + materials + camera + lights through a file format. Defer animation/time and CSG composition to v2 if it makes v1 simpler.

- Estimated effort: ~3 days.
- Unblocks: UI save/load, file-format imports built on top of it, distributed rendering, AI agent state, regression test golden files for renders.

### R4. `Primitive::tessellate(LOD) → Mesh`

Add a `Mesh` type with positions, normals, UVs, and indices. Implement `tessellate()` on every primitive class:

- Sphere → icosphere or UV sphere.
- Box → 12 triangles.
- Cylinder, cone, capsule (new primitives) → parameterized strips.
- Torus → ring × ring grid.
- Plane / disk / rectangle → trivial.
- Triangle / Mesh → identity.
- CSG operations → mesh booleans (separate epic; libigl or BSP-based for v1).
- Implicit / SDF (future) → marching cubes.

Estimated effort: ~3 days for the basic shapes, plus separate work for CSG mesh booleans.

Unblocks: GL viewport, wireframe engine, software rasterizer, OBJ/STL/glTF export, mesh-based BVH acceleration paths.

### R5. `RenderEngine` abstraction

Introduce an abstract `RenderEngine` above `Raytracer`. Move `render(buffer)` and the threading loop into the base; subclass for each backend:

- `WireframeEngine`
- `SoftwareRasterEngine`
- `OpenGLEngine`
- `RaytracerEngine` (refactored from the existing `Raytracer`)
- Future: `PathTracerEngine`, `VulkanRTEngine`

- Estimated effort: ~2 days for the abstraction; backends are independent.
- Unblocks: every other engine.

### R6. `BSDF` split from `Material::shade`

Introduce a `BSDF` interface with `eval(wi, wo) → spectrum`, `sample(wi) → (wo, pdf)`, `pdf(wi, wo) → density`. Existing materials wrap their current logic in a `BSDF` implementation while keeping `Material::shade` as a thin compatibility layer for the Whitted integrator.

- Estimated effort: ~3-5 days.
- Unblocks: path tracing, MIS, the modern material library, Fresnel everywhere.

### Total foundation work

Roughly **two calendar weeks** of focused refactor work. After that, every feature on the brainstorm becomes a clean, scoped PR.

---

## 4. Feature pillars

### 4.1 Rendering engines

Beyond the existing `Raytracer`, factor in (in suggested order):

- **Wireframe.** Edge projection from `Mesh`. Cheap, useful for editor preview and mesh debugging.
- **Software rasterizer.** Scanline + Z-buffer; runs on CPU; no GL dependency. Great for headless preview when the GL stack is unavailable.
- **OpenGL viewport.** Real-time editor view. Tessellated meshes feed VBOs; GLSL shaders mirror the material library for live preview parity. Also unlocks gizmo rendering.
- **Path tracer.** Monte Carlo integrator over the same scene graph. Multiple Importance Sampling between BSDF sampling and light sampling. Stratified or Sobol QMC sampling. Adaptive sampling per tile.
- **GPU backends, eventually.** Vulkan compute, OptiX, or Metal. Templated math primitives port reasonably to CUDA. Massive undertaking; not a near-term priority.

### 4.2 Primitives & geometry

Easy wins that fill obvious gaps:

- Cylinder
- Cone
- Capsule (cylinder with hemispherical caps)
- Heightfield / terrain
- Signed Distance Field primitive base, with sphere-tracing intersector. Opens up procedural geometry: mandelbulbs, gyroids, fractal terrain. Trendy and rewarding.
- Bezier patches / NURBS — niche, defer.
- Subdivision surfaces (Catmull-Clark) — depends on `Mesh` infrastructure.
- Particles / billboards — for sprites or fast vegetation.

Once tessellation lands (R4), the geometry engine can also do:

- CSG mesh booleans (libigl, Carve, or BSP).
- Marching cubes for implicit/SDF visualization.

### 4.3 Materials

Layered upgrade path:

- **Bump and normal maps.** Perturb `hitPoint.normal()` from a texture before shading. Needs UVs on `HitPoint` (R6 prerequisite).
- **Procedural textures** (Perlin, Worley, checker, marble, wood). Templated `ProceduralTexture<F>` on a noise functor.
- **GGX / Cook-Torrance microfacet BRDF.** Physically-based replacement for Phong. Roughness + IOR knobs. Foundational for modern materials.
- **Fresnel everywhere** — Schlick approximation for dielectrics and metals.
- **Anisotropic GGX.** Brushed metal.
- **Layered materials** (clear coat over base). Car paint, varnished wood.
- **Disney Principled BSDF.** Artist-friendly über-shader; layered metal/specular/clearcoat/sheen/SSS over a diffuse base.
- **Subsurface scattering.** Random-walk in a volumetric medium with Henyey-Greenstein phase function — natural inside path tracing. Christensen-Burley approximation for fast preview. Skin, jade, milk, wax.
- **Volumetric participating media.** Fog, smoke, clouds. Same machinery as SSS, just operating between surface hits.
- **Iridescence / thin-film interference.** Soap bubbles, oil slicks, peacock feathers.
- **Spectral rendering.** Replace RGB with wavelength sampling. Correct dispersion, fluorescence, polarization. Big architectural change; defer.

#### Material library (separate from material types)

A bundled catalog of preset materials calibrated against measured data (MERL BRDF database, Disney measured materials, Filament reference values). Gold, copper, jade, glass, rubber, brushed steel, marble, skin, pearl, silk, etc.

- Stored as YAML/JSON, hot-reloadable.
- Auto-render thumbnails (preview sphere on a checkerboard) for the UI library browser.
- Naming scheme that an LLM can pattern-match (e.g., `metal/copper/polished`, `glass/crown/optical`, `organic/jade/imperial`).

### 4.4 Lights

- **Point** (already exists in some form).
- **Directional** (sun).
- **Spot.**
- **Area** (rectangular and disk emitters with proper sampling).
- **Mesh emitter** (any triangle mesh, sampled by area).
- **HDRI environment** (image-based lighting from an EXR/HDR sky map; importance-sampled by luminance).

All require a `Light::sample(shadingPoint) → (wi, pdf, Le)` API for proper integration in MC integrators.

### 4.5 File I/O

Reads where useful, writes where useful, both directions where it makes sense:

- **OBJ** — ubiquitous, mesh-only, ~1 day. Read first, write second.
- **STL** — 3D printing, mesh-only, ~half day.
- **PLY** — already supported (read).
- **glTF 2.0** — meshes + materials + textures + skeletal animation. Modern, well-specified. Use `cgltf` or `tinygltf`.
- **USD** — Pixar's industry-standard scene description. Heavy dependency but the right long-term home for everything (geometry, materials, animation, layered overrides, references).
- **FBX** — Autodesk; via OpenFBX (avoid Autodesk SDK if possible).
- **OpenVDB** — for volumetric data, once volumetrics land.
- **EXR / HDR** — environment maps and HDR output. Pairs with the float framebuffer (R1) and tonemap stage.
- **OpenSCAD `.scad`** — see modeling UI.
- **Native scene JSON/YAML** (see R3) — round-trip with full fidelity.

### 4.6 Modeling UI

Scope = "Blender-lite." Qt is already the toolkit, so this is reachable.

#### Traditional modeling

- Multi-viewport (top / front / side / perspective). Each viewport picks its own engine — typically perspective uses GL, others use wireframe.
- Object hierarchy / outliner with selection, naming, grouping, parenting.
- Property editor for the selected object's transform, material, primitive parameters.
- Move / rotate / scale gizmos with snapping.
- Primitive creation toolbar; CSG operation buttons.
- Material editor with library browser, parameter sliders, live preview.
- Undo/redo (depends on R2 — stable IDs — and R3 — diffable serialization).

#### AI-native side panel

A chat interface in the editor with an LLM agent that has tool calls into the scene graph. Tool surface includes:

- `add_primitive(type, position, params) → id`
- `transform(id, matrix)`
- `apply_material(id, name_or_inline_definition)`
- `csg_union(a, b) → id`, `csg_intersect(a, b) → id`, `csg_difference(a, b) → id`
- `import_file(path) → id`
- `query_scene() → JSON` (the agent's read-side: list objects, materials, hierarchy, current selection)
- `select(id)`, `delete(id)`
- `set_camera(position, target, fov)`
- `set_environment(hdri_path)`

The agent observes user edits via the same scene graph, so the loop is bidirectional: user moves an object, the agent sees it on the next `query_scene`. The agent can describe the scene, refactor it ("group these into an assembly"), critique it ("the lighting is too uniform — want a key/fill setup?"), or build it from scratch ("a brass lamp on a marble table").

Implementation notes:

- Anthropic's Claude with tool use is well-suited; the green-acres infrastructure already has Claude API auth.
- Tool calls go through the scene serialization layer (R3) so they can be undone, replayed, and audited.
- The chat transcript is part of the scene file (project-level), so users can resume an AI-assisted session days later.

#### Scripted parametric objects (OpenSCAD-style)

A scene-script DSL for parametric/procedural geometry. Two mutually compatible options:

- **Python or JavaScript** for ergonomics — most users already know one or both. Existing `scripts/` directory shows the project already uses scripted scenes.
- **OpenSCAD-style functional language** for parametric/precision use cases — geometric primitives composed via union/difference, parameterized over input variables.

Scripts produce primitives that are then dropped into the scene graph as regular objects. The AI agent can write scripts when asked for parametric parts ("a 24-tooth involute gear", "a 30-step spiral staircase", "a hex-wrench at 5mm A/F"), then add the resulting object to the scene.

### 4.7 Animation & timeline

- Scene timeline with keyframes on any animatable parameter — transforms, material parameters, camera pose, light intensity, scene-level globals (time of day, weather).
- Interpolation curves: linear, Bezier, ease-in/out, hold.
- Time-sampled rendering for motion blur (multiple time samples per frame within shutter-open).
- Output: image sequence or piped to ffmpeg for video (configurable codec).
- Eventually (defer): rigid body simulation, particle systems, simple IK skeletons.

Prerequisite: time dimension on the scene graph — every transform and parameter needs to be evaluable at a time `t`.

### 4.8 Distributed rendering

Master/worker architecture, in two flavors that share infrastructure:

- **Tile-level distribution** for stills. Master partitions image into tiles, dispatches them, aggregates the float framebuffer back. Best for high-sample-count single images.
- **Frame-level distribution** for animation. Each worker renders whole frames in parallel. Better throughput, simpler scheduling, no aggregation logic within a frame. Best for animation throughput.

Tech stack:

- Transport: gRPC or NATS. For Kubernetes-native deployments, each frame as a `Job` with a worker pod.
- Fault tolerance: worker death → re-dispatch.
- Shared storage: NFS or S3 for textures/scenes; S3 (or any object store) for output frames.
- Authentication: re-use the existing green-acres auth pattern (token-mode gateway).

A "render to k8s" button in the UI is genuinely cool — the green-acres cluster is right there.

---

## 5. Non-goals (explicit)

To keep scope honest, this roadmap is *not* trying to:

- Compete with Blender on modeling tooling. The UI is "Blender-lite"; sculpting, retopology, UV unwrapping are out of scope.
- Compete with production renderers (Arnold, RenderMan, Cycles, Manuka) on absolute image quality. The renderer should be physically grounded and pretty enough for portfolio shots, not VFX-final-frame.
- Be a real-time game engine. The OpenGL viewport is for editing, not for shipping playable experiences.
- Implement BDPT, MLT, or photon mapping in the near term. Path tracing handles 95% of the cases that need them; the remaining 5% are not worth the architectural complexity at this stage.
- Build a node-based shader graph. Not before there's clear demand.
- Replace OpenSCAD or CadQuery for CAD workflows. The scripted DSL is for *artistic* parametric content, not engineering-grade CAD.

These can all be reconsidered later. The point of this list is to keep PRs focused while the foundations are still being laid.

---

## 6. Suggested phasing

These are milestones, not deadlines. Each milestone is independently shippable and visibly improves the project.

### Milestone A — foundations (next 2-3 weeks)

The six refactors from §3:

- R1: float HDR framebuffer + tonemap.
- R2: stable scene-object IDs.
- R3: scene serialization v1.
- R4: `Primitive::tessellate()` for the basic primitives.
- R5: `RenderEngine` abstraction.
- R6: `BSDF` split from `Material::shade`.

Plus the in-flight bug fixes: TIR/truncation (PRs #35, #36), and the small tidy-ups (`PerfectTransmitter` IOR default, `PerfectSpecular` typo, missing `setTransmissionCoefficient` in textured ctor).

### Milestone B — quick visual wins (1-2 weeks after A)

- Cylinder, cone, capsule primitives.
- Bump/normal maps + procedural textures.
- Area lights with proper sampling (now possible thanks to R6).
- HDRI environment lighting.
- OBJ import + export.

### Milestone C — second engine (2-3 weeks after B)

- Wireframe engine (cheapest).
- OpenGL viewport (most useful).
- Tessellation polish: higher-LOD options, smooth normals, proper UV layout.

### Milestone D — path tracer (3-4 weeks after C)

- `PathTracerEngine` — same scene API, MC integration.
- Multiple Importance Sampling.
- GGX / Cook-Torrance BSDF.
- Adaptive sampling per tile.
- OIDN denoiser integration (Intel Open Image Denoise — single library, drop-in, transforms 32-spp output to 1024-spp quality).

### Milestone E — modeling UI shell (4-6 weeks after D)

- Multi-viewport editor.
- Outliner, property editor, gizmos.
- Material editor with library browser.
- Save/load via R3.
- Undo/redo.

### Milestone F — AI chat side panel (1-2 weeks after E)

- Tool-using LLM agent over the scene graph.
- Scripted parametric DSL (Python/JS first; OpenSCAD-style later).
- Chat transcript persisted with the scene.

This is the milestone that makes the project uniquely *yours* — get here as efficiently as possible, then iterate.

### Milestone G — animation (3-4 weeks after F)

- Time dimension on the scene graph.
- Keyframe timeline UI.
- Motion blur (multi-sample within shutter-open).
- Image-sequence and video output.

### Milestone H — distributed rendering (2-3 weeks after G)

- Tile-level distribution for stills.
- Frame-level distribution for animation.
- "Render to k8s" button in the UI.

### Milestone I — advanced materials (open-ended, after H)

- Disney Principled BSDF.
- Subsurface scattering (random-walk in PT).
- Volumetric participating media.
- Anisotropic GGX, layered materials, iridescence.
- Spectral rendering (architectural change — its own milestone).

---

## 7. Open questions

These need decisions before specific work starts. Calling them out so they don't ambush a PR mid-flight.

- **Scene serialization format: YAML or JSON?** YAML is friendlier for hand-editing, JSON is friendlier for tooling. USD-on-disk is a separate question (it's binary-ish via Crate or text via `.usda`).
- **Scripted DSL: which language first?** Python is more popular but adds a Python embedding dependency. JavaScript via QuickJS is small and self-contained. An OpenSCAD-style language is the most opinionated and the most work.
- **Material library distribution model.** Bundled in-tree, downloaded on first use, or referenced from a community repo (like Blender's asset library)?
- **UI undo/redo granularity.** Per-property change, per-tool action, or per-scene-mutation? Affects the serialization design.
- **AI agent: cloud LLM or local?** Cloud (Claude via API) is more capable today; local (llama.cpp, Ollama) is private and free at idle. Both are viable; the tool-call surface is the same. Probably default to cloud with a local-fallback knob.
- **GPU backend: Vulkan compute, OptiX, or Metal?** Vulkan is portable but verbose. OptiX is NVIDIA-only but mature. Metal is macOS-only. This is a far-future decision; flagged here so it doesn't get re-litigated every quarter.
- **Compatibility: maintain the existing C++ scene-construction API forever, or sunset it once UI/serialization land?** The examples directory currently uses it heavily.

---

## 8. Relationship to `modernize.md`

[`modernize.md`](modernize.md) is the engineering-hygiene roadmap: build system, CI, test framework, Qt 6 migration, supply-chain hardening. This document is the feature/architecture roadmap.

The two are largely independent. Recommended order:

- Modernization items §3.1 (CMake migration, done), §3.2 (CI, in progress), §3.5 (test framework upgrade) should land before any of the foundational refactors here, since they make the refactors safer to ship.
- Modernization §3.10 (Qt 6 migration) should ideally land before Milestone E (modeling UI) so the new UI code targets a supported toolkit from the start.
- Everything else can interleave freely.

---

## 9. How to use this document

This is a living roadmap. PRs that move items forward should reference the roadmap section they advance (e.g., `[roadmap §4.2]` in the PR title or body). Items completed get crossed off here. New items get added here before they get implemented.

Open questions in §7 should be resolved through discussion (issues, this doc, or just a chat session) before the affected work starts.

The "north star" in §1 is the scope ambition. The non-goals in §5 keep that ambition honest. The phasing in §6 is one possible order — reorder freely as priorities shift.

---

*End of roadmap. Open for iteration.*
