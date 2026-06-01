# Feature Roadmap — `tkadauke/raytracer`

> **Scope:** vision, architecture, and feature roadmap for evolving this project from a Whitted-style raytracing library into a complete 3D content creation tool.
> **Date:** 2026-05-01
> **Status:** Draft — opened for iteration.
> **Companion docs:**
> - [`modernize.md`](modernize.md) covers build/CI/Qt6/supply-chain modernization. Independent of this doc; advances in parallel.
> - [`topics-backlog.md`](topics-backlog.md) catalogues the wider field — algorithms and disciplines not yet promoted to a roadmap pillar but worth implementing and documenting eventually. Items there graduate to this roadmap when they get picked up.

---

## 1. Vision — the North Star

> **This project exists for my own education and entertainment, not for any production use.** The goal is to build, by hand, a content creation pipeline that touches every interesting algorithm in computer graphics — and to keep that pipeline navigable through clear, comprehensively documented code accompanied by **interactive educational diagrams** that make the math and the data flow visible. Where multiple algorithms exist for the same problem, the project should implement *all* the worthwhile variants (or at least leave space for them). Efficiency matters; pedagogy matters more.

A renderer is the *core*, not the *whole product*. The goal is a working 3D content creation tool that:

- ships several rendering backends (wireframe, software raster, OpenGL viewport, **WebGL/WebGPU preview**, CPU raytracer, CPU path tracer) over a single scene representation, with a render-pass graph that can mix engines in one frame, plus web-based previews for static scenes — and, as a stretch goal, animations,
- carries a rich material library with physically-based BSDFs, NPR variants, subsurface scattering, volumetrics, and a complete shading-normal pipeline (smooth normals → bump → normal map → displacement),
- grounds all mesh geometry in a **comprehensive computational geometry library** with first-class spatial acceleration (BVH, octrees, kd-trees, uniform grids — selected per workload),
- imports and exports the formats the rest of the world uses (OBJ, glTF, USD, EXR, PLY, STL, FBX, OpenVDB), reading and writing where reasonable,
- offers a Qt-based modeling UI with multi-viewport editor, outliner, gizmos, property panels, **modifier stack**, **node graph**, **sculpt/UV/retopology/grease-pencil** tools, and a material/asset browser,
- exposes the scene to an **AI agent** that can construct, modify, and critique scenes through natural-language chat — including writing parametric scripted objects (OpenSCAD-style),
- supports **animated rendering** with a timeline + keyframes + motion blur, plus a **video-editor-style sequencer** for stitching rendered scenes together with transitions, titles, and audio,
- runs the resulting frames through a **postprocessing & compositing stack** (tonemap, bloom, DoF, motion blur, color grading, AOV-driven compositing),
- can **distribute rendering across a Kubernetes cluster** for animation throughput,
- leaves room for **industry-inspired extras** (NeRF, Gaussian splatting, custom physics, etc.) at the far end of the roadmap, once the foundations support them.

The scope is "Blender-lite + DaVinci-Resolve-lite + an LLM in the side panel." The differentiator is the AI-native authoring loop on top of a clean, testable, physically-grounded renderer that *teaches* while it runs.

This document is a roadmap, not a commitment, and there are no deadlines. Order and scope are negotiable; the prerequisites and the pedagogical invariant (§4.0) are not.

---

## 2. Current state — honest assessment

### What's already solid (keep, build on)

- **Pluggable strategy interfaces.** `Camera`, `Material`, `Primitive`, `BRDF/BTDF`, `Sampler`, `ViewPlane`, `Texture`, `Light` are all abstract bases. New shapes, materials, and cameras drop in additively.
- **Templated math + SIMD specializations** under `core/math/vector/sse3/`, `core/color/sse3/`, and the shared `core/simd/` packet backend. The x86 SSE/SSE2/SSE3 paths remain production-grade and ARM NEON now supports the four-wide packet traversal surface.
- **Constructive Solid Geometry** (`Composition`, `Union`, `Intersection`, `Difference`, `Instancing`) already works for implicit ray-traced primitives. Uncommon for a hobby project; reuse rather than rewrite.
- **Tile-based parallel rendering** via `QThreadPool` already gives clean horizontal scaling within a host. Per-tile dispatch makes distributed rendering a small extension.
- **Test discipline.** Unit + functional suites, LibFuzzer harness on the PLY parser, Google Benchmark on hot paths, and Syrus-native build/test/textbook graders. CMake/Ninja, Doxygen pages, parked GitHub Actions/Dependabot configs, and Dockerfile coverage give stronger engineering hygiene than most renderers.
- **Quartic intersector** for the torus is correct and numerically stable in the regimes tested.

### What needs reinforcement before features can stack

- ~~**`Buffer<unsigned int>` LDR pipeline.** Fine for Whitted; fatal for path tracing. PT needs a float accumulator, with tonemapping at the end.~~ ✅ **Done.** See §3.R1: engines can render into `Buffer<Colord>` and the LDR buffer is now a tonemapped display target.
- **Whitted-only integrator.** ~~Hard-coded into `Raytracer::rayColor`.~~ ✅ **Done.** The existing recursive policy now lives in `render::WhittedIntegrator`, with `Raytracer::rayColor` delegating through the `Integrator` boundary. Future path tracers plug into that single-ray radiance contract, not into the frame scheduler or material callback directly. No Monte Carlo integration, no Multiple Importance Sampling, and no proper area-light shadow rays ship yet. The renderer has velocity-based motion-blur sampling and a `RenderEngine` split, but Whitted remains the only shipped transport policy.
- **Phong-only direct lighting, no Fresnel.** Reflection and transmission coefficients are scalar constants set on the material — physically wrong everywhere except at normal incidence. PT-class materials need Fresnel at every interface.
- **No nested-medium tracking.** `PerfectTransmitter` hardcodes "the other side is air, IOR = 1." Glass-inside-glass scenes will be wrong.
- **No complete tangent frame on `HitPoint`.** `HitPoint` can now carry UV coordinates for raster fragments and other callers that provide them, but tangent/bitangent data and full ray-intersection UV coverage are still missing. This blocks normal maps, anisotropic BRDFs, and a complete shading-normal pipeline.
- **Texturing is still incomplete.** Procedural and image-backed textures exist as colour sources; raster albedo now supports UV-driven image sampling, mip selection, and tangent-space normal maps. Ray/path-tracing texture evaluation still lacks bump/displacement, triplanar projection, cross-engine shading-normal slots, and a material-graph authoring surface.
- **Bump and normal mapping: not implemented.** Distinct from texturing — needs a tangent frame on `HitPoint` and a shading-normal pass before BSDF eval.
- **Camera library is growing but still incomplete.** Pinhole, fish-eye, orthographic, spherical, equirectangular, thin-lens DoF, and tilt-shift cameras exist. The roster is still missing Kolb realistic, Panini, omnidirectional/cubemap, light-field, and stereo (parallel-frustum and toed-in) cameras.
- **Spatial acceleration is reinforced for the main raytracing path.** `render::BVH` now provides SAH-built acceleration and Ray4/Ray8 packet traversal, while `Grid` remains available as the uniform-grid path. `render::SpatialIndex` and `render::SpatialIndexFactory` let callers select the plain linear fallback, uniform grid, or BVH through one abstraction, and the automatic policy now chooses Linear for empty/single-leaf scenes and BVH for multi-leaf scenes based on measured benchmarks. This supersedes stale issue #18's "only Grid, no BVH or abstraction" framing; the remaining work is octree/kd-tree variants, TLAS/BLAS instancing, and deeper policy tuning for workload-specific engine backends.
- ~~**Default recursion depth and black-on-truncation** — being addressed by the in-flight TIR / truncation PRs (#35, #36).~~ ✅ **Done.** #35 bumped the default `maximumRecursionDepth` 5→10 and switched truncation to return the scene background; #36 made the TIR branch of `TransparentMaterial::shade` retain Phong direct lighting.
- ~~**No throughput-based adaptive recursion cutoff.** The Whitted ray policy used to recurse to the hard depth limit regardless of how attenuated the path had become, wasting work on paths carrying < ε energy.~~ ✅ **Done.** `render::State` gains a `throughput` field (default 1.0); `ReflectiveMaterial`, `TransparentMaterial`, and `PortalMaterial` multiply it by the local attenuation before each recursive call; `render::WhittedIntegrator` short-circuits to the scene background when `throughput < RAYTRACER_THROUGHPUT_CUTOFF` (1e-4 in `Constants.h`). The biased cutoff is a prerequisite for the unbiased Russian-Roulette variant planned for the path-tracing integrator (§4.1).

### Existing seams that just need extension

- New `Primitive`s: drop in, register, done.
- New `Material`s / `BRDF`s / `BTDF`s: same.
- New `Camera`s: same.
- The `widgets/` module is cleanly separated from the renderer — UI grows without touching the math.
- `scenes/` shows the serialized scene-graph pattern; a parametric DSL slots into the same shape.

### Missing seams (new abstractions)

These don't exist and are blockers for entire feature classes:

- ~~**No `RenderEngine` abstraction.**~~ ✅ Resolved by §3.R5 — `RenderEngine` base class lifted out of `Raytracer`; engines plug in via subclassing.
- ~~**No `Mesh` interface that all primitives can produce.**~~ ✅ Resolved by §3.R4 — `Primitive::tessellate(int lod = 0) → shared_ptr<Mesh>` implemented across the standard primitive set.
- ~~**`Light` is not first-class enough.** Lights exist but aren't sampled for soft shadows or MIS — no `Light::sample(point) → (direction, pdf, intensity)`.~~ ✅ **Partially done.** Runtime lights now expose `Light::sample(point)`, `Light::pdf(point, direction)`, delta-light metadata, `emission()`, and `power()` for Epic #358. The shipped light classes are still point and directional delta lights, and the Whitted renderer still uses hard-shadow direct lighting; area/environment/mesh lights and the integrator that consumes these APIs remain future work.
- ~~**No scene serialization.** Scenes are constructed in C++ or via scripts. Nothing round-trips through a file. Blocks UI save/load, file import/export workflows, distributed rendering, and AI-agent state persistence.~~ ✅ **Done for native editable scenes.** `world::Scene` now loads/saves JSON through `ElementFactory`, preserves render intent and animation blocks, and underpins `rendercli` / Modeler file workflows. External import/export breadth remains tracked under §4.4.
- ~~**No scene-object handle/ID system.** UI selection, undo, AI agent tool calls all need stable references. Today, objects are bare pointers.~~ ✅ **Mostly done.** `world::Element` owns a stable string id, auto-generates UUID-style ids, serializes ids, and supports recursive `findById`. A typed `SceneObjectId` wrapper and render-runtime handle layer remain future cleanup.
- ~~**No time dimension** on the scene graph. Blocks animation.~~ ✅ **Partially done.** Native scene JSON now has a top-level `animation` timeline, world-side keyframe tracks, frame evaluation, `rendercli --frame`, `rendercli --animation`, and read-only Modeler timeline preview. Render-side continuous track compilation and shutter-time sampling from arbitrary keyframes remain §4.7 follow-up work.
- ~~**`BSDF` is not separated from `Material::shade`.** Today shading is `Material::shade(raytracer, ray, hit, state) → Color` — tightly coupled to the recursive raytracer's call style.~~ ✅ **Partially done.** `render::BSDF` exists with `eval`, `sample`, `pdf`, `reflectance`, explicit delta metadata, and lobe flags; `BRDF` / `BTDF` adapt the legacy lobes to that interface, and representative diffuse/glossy/delta lobes now have sample/PDF contract coverage. `Material::shade` is still the Whitted compatibility layer, material-owned BSDF composition remains future work, and the path-tracing integrator still has to own recursion.

---

## 3. Foundational refactors — the prerequisites

These refactors unlock roughly 80% of the north-star features. Done as small focused PRs, each independently shippable. R0 is the prerequisite to the prerequisites: without behavioural test coverage on the integrator and materials, every refactor in R5 and R6 is risky.

### R0. Behavioural test coverage of integrator + materials

The test suite was broad on construction and properties but thin on *behaviour*. A test-coverage audit (2026-05-01) found that no unit test called `Material::shade()` on any subclass, Whitted recursion was exercised only via golden-image functional tests, `TransparentMaterial` Snell / TIR / nested-IOR logic was untested, `PerfectTransmitter` had no test file, and the texture-mapping classes (`Texture`, `TextureMapping2D`, `PlanarMapping2D`) had zero coverage. Refactor confidence required closing these gaps first.

Specific work:

- ✅ **Whitted single-ray evaluation** — depth-limit boundary (== vs ≥), no-hit→background, `state.recursionDepth` tracking on reflection/refraction recursion, behaviour with no material assigned. ~~Belongs alongside the in-flight #35 fix.~~ Landed via #35 and now lives behind `render::WhittedIntegrator` with `Raytracer::rayColor` delegating through `render::Integrator`.
- ✅ **`TransparentMaterial`** — Snell's law refraction angle, critical-angle TIR boundary, ~~nested-medium IOR stacking~~ (still TODO — see §2 "no nested-medium tracking"), normal-incidence pass-through. ~~Belongs alongside the in-flight #36 fix.~~ Most of this landed via #36's `ShadeFixture`-based behavioural tests in `TransparentMaterialTest.cpp`.
- ✅ **`PerfectTransmitter`** — first-ever test file. BTDF subdir generally under-tested. **Done in #36** (`test/unit/raytracer/brdf/PerfectTransmitterTest.cpp`).
- ✅ **All `Material::shade()` subclasses** — at least one behavioural test per material that calls `shade()` and verifies the output colour against an expected value, not just constructed properties. **Done in #22** (`MatteMaterial`, `PhongMaterial`, `ReflectiveMaterial`, `TransparentMaterial`).
- **`Texture` and texture mappings** — coordinate projection correctness for `PlanarMapping2D`; texture-pattern alternation for `CheckerBoardTexture`; constant-colour shading for `ConstantColorTexture`. *(World-side `Texture` family covered in #24; raytracer-side mappings still uncovered.)*
- **Empty test body** — `PlaneTest.cpp::ShouldReturnBoundingBox` is a `// TODO` stub. Fill in.
- ~~**`world/` layer** — 32 headers, 31 impls, zero tests. Either bring under test (it's the Qt-side scene description and likely live), or formally sunset and remove.~~ ✅ **Done.** All 39 world-layer classes now under test: `Element`, `Transformable`, `Scene`, `Light` family, `Surface` family, `Material` family, `Texture` family, `Camera` family (including `ThinLensCamera`, `TiltShiftCamera`, `EquirectangularCamera`), `CSGSurface` family, `Disk`, `OpenCylinder`, `Rectangle`, `Torus`, `Triangle`, `ScriptedSurface` — closes #24.
- **Sampler distribution properties** — verify jittered sampler actually jitters, regular sampler stratifies, random sampler is uniform. Currently only iterator mechanics are tested.
- **Factory methods** — `CameraFactory`, `SamplerFactory`, `ViewPlaneFactory` have no test files.

Estimated effort: ~3-4 days. Most of the gap is in materials + integrator + texture-mapping unit tests; the `world/` decision (test vs sunset) is its own conversation.

Unblocks: every other refactor below. Specifically, R5 (`RenderEngine` abstraction) and R6 (`BSDF` split from `Material::shade`) cannot be confidently refactored without behavioural tests on the current integrator and materials.

### R1. Float HDR framebuffer + tonemap stage

~~`Buffer<Colord>` accumulator; existing `Buffer<unsigned int>` becomes a display target produced by a tonemap pass (Reinhard or ACES, configurable).~~ ✅ **Done.** `Camera::render` writes into `Buffer<Colord>`; `Raytracer::render(Buffer<unsigned int>&)` allocates the float buffer, runs the tile workers into it, and applies the configured `Tonemap` (default `LinearTonemap` — pass-through) to produce 8-bit display output. Three operators ship: `LinearTonemap`, `ReinhardTonemap`, and `AcesTonemap` (Narkowicz fit), all registered with `TonemapFactory` and selectable via `rendercli --tonemap`. The `Buffer<Colord>` overload is also exposed publicly so future EXR writers and path-tracing accumulators can skip tonemapping entirely. See §4.9.a for the larger tonemap-operator catalog this unblocks.

- Unblocks: path tracing, motion blur, EXR output, distributed-tile aggregation.

### R2. Stable scene-object IDs

~~Every node in the scene graph gets a typed `SceneObjectId` (integer or UUID). `Scene::find(id) → Object*` lookup. Optional `Scene::name(id)` for debug.~~ ✅ **Mostly done.** Editable `world::Element` nodes have stable string ids, UUID-style defaults, JSON round-tripping, and recursive `findById`. Remaining cleanup: introduce a typed id wrapper and decide whether runtime `render::Object` needs persistent handles beyond metadata.

- Unblocks: UI selection, undo/redo, AI agent tool calls, save-to-disk references.

### R3. Scene serialization (JSON, v1)

~~Round-trip primitives + materials + camera + lights through a file format. Defer animation/time and CSG composition to v2 if it makes v1 simpler.~~ ✅ **Done.** `world::Scene::load/save` round-trips native JSON through `ElementFactory`, preserves child object ids/properties/references, imports, render intent, and the top-level animation block. Tests cover static scenes, invalid files, missing references, render-intent JSON, and animation round-trips.

- Unblocks: UI save/load, file-format imports built on top of it, distributed rendering, AI agent state, regression test golden files for renders.

### R4. `Primitive::tessellate(LOD) → Mesh`

~~Add a `Mesh` type with positions, normals, UVs, and indices. Implement `tessellate()` on every primitive class.~~ ✅ **Done** for the standard primitives (PRs #47 batch A, #46 batch B, #45 batch C). `Mesh::Vertex` carries position + normal + UV; `Primitive::tessellate(int lod = 0)` is the contract. Per-primitive coverage:

- ✅ Sphere → UV sphere on a `lonSegs × latBands` grid (`16 << lod` × `8 << lod`), pole vertices duplicated for pinch-free seam.
- ✅ Box → 6 quad faces × 4 verts each (24 total), CCW from outside, per-face normals + UVs.
- ✅ OpenCylinder → quad strip wrapping Y, `2 × (segments + 1)` verts with seam-duplication so wrapped textures don't smear.
- ✅ Torus → `majorSegs × minorSegs` grid, both seams closed by duplicated final column/row.
- ✅ Plane / Disk / Rectangle / Triangle / FlatMeshTriangle / SmoothMeshTriangle → trivial fans / single triangles / empty mesh as appropriate.
- ✅ Composite / Instance / Grid / Scene → child-mesh concatenation with face-index remapping; Instance applies the point + normal matrices, normals re-normalised under non-uniform scale; Grid / Scene inherit unchanged.
- 🟡 CSG operations (`Difference`, `Union`, `Intersection`, `MinkowskiSum`, `ConvexHull`) → return empty mesh + `qWarning`; mesh-boolean impls queued under §4.2.a.
- Future: cylinder caps, cone, capsule (new primitives); implicit / SDF → marching cubes.

Four interactive Doxygen widgets (`disk_tessellate.js`, `open_cylinder_tessellate.js`, `sphere_tessellate.js`, `torus_tessellate.js`) visualise the LOD-driven segment count growth with live sliders. 89 unit tests pin counts, normals, UVs, and LOD-scaling invariants across every concrete impl.

Unblocks: GL viewport, wireframe engine, software rasterizer, OBJ/STL/glTF export, mesh-based BVH acceleration paths.

### R5. `RenderEngine` abstraction

~~Introduce an abstract `RenderEngine` above `Raytracer`. Move `render(buffer)` and the threading loop into the base; subclass for each backend.~~ ✅ **Done.** `RenderEngine` base class added; owns camera / scene / tonemap / cancellation hooks plus the `render(Buffer<unsigned int>&)` tonemap-wrapper. `Raytracer` is now a concrete subclass holding what's actually raytracer-specific: the QThreadPool tile dispatch, the single-ray probes (`rayColor` / `rayState` / `primitiveForRay`), and the recursion-depth limit. Threading lives on the concrete engine because each engine picks its own strategy (raytracer tiles pixels; future wireframe will parallelize edges; future GL submits to the driver). The split surfaces what's actually shared, which the namespace cleanup below uses to decide what moves out of `raytracer::`.

- ~~`WireframeEngine`~~ ✅ **Done** (single-threaded, no HLR). Edge projection from `Mesh` via `Camera::projectPoint` (`PinholeCamera`, `OrthographicCamera`, `ThinLensCamera` / `TiltShiftCamera` pinhole fallback, and other cameras with closed-form projection), near-plane edge clipping, and Bresenham rasterization. `Primitive::tessellate(lod)` per primitive walked; every face edge drawn. CSG operations show empty (their tessellate returns empty meshes); fish-eye and other non-rectilinear cameras render empty (no closed-form `projectPoint` inverse). V2 wins queued: hidden-line removal and edge-parallel threading.
- `SoftwareRasterEngine`
- `OpenGLEngine`
- ✅ `RaytracerEngine` — kept as `Raytracer` since it predates the abstraction; the type rename was deemed not worth the churn.
- Future: `PathTracerEngine`, `VulkanRTEngine`

- Unblocks: every other engine.

### R5b. Namespace + directory cleanup — `raytracer::` → `render::`

~~Lift the shared types out of `raytracer::` into a new top-level `render::` namespace + `include/render/` directory.~~ ✅ **Done** in 10 phased commits (`0cdcfd3` through `fff5ea8`). All engine-shared types now live in `render::`. The `raytracer::` namespace is reduced to `Raytracer` (the engine), `State` (per-ray recursion state), and `stats::Counters` (the raytracer-specific stats namespace).

**Stays in `raytracer::` / `include/raytracer/`**:

- `Raytracer` (the engine).
- `State` — per-ray recursion state, raytracer-specific.

**Moves to `render::` / `include/render/`**:

- `RenderEngine`, `Object`, `Stats`.
- `Camera` + all subclasses + factory.
- `Primitive` + all subclasses (the largest subdirectory, hence the timing — see below).
- `Light` + subclasses.
- `Material` + subclasses (note: `Material::shade` will continue to take a `Raytracer*` until §3.R6 splits it onto a `BSDF` interface; cross-namespace forward-declaration is fine until then).
- `BRDF` / `BTDF` + subclasses.
- `Sampler`, `SampleStream`, factory.
- `ViewPlane` + subclasses + factory.
- `Texture` + subclasses + texture mappings.
- `Tonemap` + subclasses + factory.

**Timing**: queued behind R4's per-primitive tessellate batches (issues #42, #43, #44). The R4 batch agents work in `raytracer::primitives/`; doing the namespace move before they land would force every agent PR through a rebase against a renamed primitives/ tree. Doing it after avoids that conflict and lets the cleanup grab the agents' new tessellate code in one mechanical pass.

- Estimated effort: ~1 day for the mechanical work (file moves + sed for namespace + sed for include paths) + a few hours for fixup (places that use `using namespace raytracer;` and reference the moved types unqualified).
- Unblocks: `WireframeEngine` and every subsequent engine — they reference `render::Camera` / `render::Light` / etc. without the awkward implication that they're "raytracer-specific."

### R6. `BSDF` split from `Material::shade`

~~Introduce a `BSDF` interface with `eval(wi, wo) → spectrum`, `sample(wi) → (wo, pdf)`, `pdf(wi, wo) → density`.~~ ✅ **Partially done.** `render::BSDF` now exists and the legacy `BRDF` / `BTDF` hierarchy inherits from it, with lobe flags, explicit delta metadata, caller-owned 2D sampling, and adapters for `eval`, `sample`, `pdf`, and `reflectance`. Lambertian diffuse and Phong glossy lobes now provide analytic sample/PDF contracts, and perfect reflection/transmission expose delta contracts. Remaining work: make `Material::shade` a thin Whitted compatibility layer over material-owned BSDF composition and move recursion ownership into a path-tracing integrator.

- Unblocks: path tracing, MIS, the modern material library, Fresnel everywhere.

### R7. Spatial acceleration framework

Replace the current hierarchical container with a proper acceleration tree, behind a `SpatialIndex` (or `Accelerator`) interface so multiple structures can coexist:

- ✅ **BVH** (binary, with SAH build) — the workhorse for general scenes. Lives at `include/render/primitives/BVH.h`; SAH (Surface Area Heuristic) builder, ray-AABB cull at every node, falls back to linear scan if `setup()` is skipped. Drop-in replacement for `Composite` / `Grid`.
- **Octree** — natural fit for unbounded/heterogeneous scenes and volumetrics.
- **kd-tree** — best for static, geometry-heavy scenes; teaches a different traversal style.
- ✅ **Uniform / hashed grid** — fast build, good for animated/dynamic scenes. Pre-existing as `render::Grid`.
- **Two-level (TLAS / BLAS) layout** — stable per-mesh BLASes referenced by transformed instances; matches GPU RT API patterns and supports instancing-heavy scenes.

~~The `SpatialIndex` interface itself isn't yet extracted~~ ✅ **Done.** `render::SpatialIndex` now names the shared add/setup/bounds/intersect contract implemented by `Composite`, `BVH`, and `Grid` for Epic #360. `render::SpatialIndexFactory` also constructs the linear fallback, uniform grid, and BVH behind that contract so callers do not need to name the concrete accelerator. `render::AccelerationPolicy` and `world::Scene::accelerationMode` make Auto/Linear/Grid/BVH selection explicit and diagnostic-observable; ~~the old Grid-preserving Auto default awaited benchmark evidence~~ ✅ **Done.** Auto now uses scene-conversion analysis for Epic #360, choosing Linear for empty/single-leaf scenes and BVH for multi-leaf imported or procedural geometry. This closes the abstraction/BVH/policy portions of issue #18; kd-tree remains a separate future accelerator.

Per-engine backends pick the right structure (raytracer/path tracer want BVH; rasterizers want frustum-cull-friendly trees). Scenes can request a specific structure for benchmarking, since "compare them all" *is* the educational point.

- Estimated effort: ~5 days for BVH+octree+grid behind the interface; kd-tree as a follow-up; TLAS/BLAS once instancing matters.
- Unblocks: any non-trivial scene size, animation (rebuilds-per-frame), volumetrics, and meaningful benchmarks.

### Total foundation work

Roughly **three calendar weeks** of focused work — R0 (test coverage) plus R1-R7 (the actual refactors). After that, every feature on the brainstorm becomes a clean, scoped PR.

---

## 4. Feature pillars

### 4.0 Documentation & interactive education *(hard invariant)*

This section gates every other section. The project's primary deliverable is *understanding*, and the codebase needs to be readable as a textbook as well as runnable as a tool.

**Always-on requirements** — every feature PR must satisfy these to merge:

- **Doxygen-grade source comments** on every public class, every algorithm, and every non-obvious snippet. "What this does" is fine; "why, with a citation to the relevant chapter / paper" is the actual bar.
- **A topical doc page** under `docs/` for each subsystem (one per pillar in §§4.1-4.10), kept in lockstep with the code. New algorithm → new doc section, same PR.
- **An interactive diagram** for any non-trivial algorithm, embedded in the doc page. Targets in priority order:
  1. **WebGL/WebGPU live scenes** (preferred — same engine work that powers §4.1's web preview, reused for explainers).
  2. **Animated SVG** for 2D math (sampling distributions, BVH traversal, Bezier curves, etc.).
  3. **MathJax / KaTeX** for the equations.
  4. **`<canvas>` with vanilla JS** for the simplest interactive cases.
- **Reproducible "from-zero" runs.** Each doc page should include a minimal scene that exercises the algorithm and a one-liner to render it, so a reader can poke at the values and see what changes.
- **Cross-linking.** Every PR that touches a subsystem updates the relevant doc page's algorithm catalog and "implementations of variant X" lists. The educational invariant *is* the multi-variant invariant — we're documenting *all* the ways to do a thing, not just the one we ship by default.

**Hosting.** The existing GitHub Pages workflow (Doxygen output) is the deployment target. Long-form docs live in `docs/topics/`; interactive demos live in `docs/interactive/` and ship as static HTML+JS so they work on the public Pages site without a server.

**Why this is in §4.0 and not §8.** Because if it's an afterthought it never happens, and the project loses its reason to exist. PRs without docs are draft PRs — full stop.

### 4.1 Rendering engines

Beyond the existing `Raytracer`, factor in (in suggested order):

- **Wireframe.** Edge projection from `Mesh`. Cheap, useful for editor preview and mesh debugging.
- **Software rasterizer.** Scanline + Z-buffer; runs on CPU; no GL dependency. Great for headless preview when the GL stack is unavailable. The educational version implements the textbook pipeline end-to-end: clipping (Sutherland-Hodgman), perspective-correct attribute interpolation, depth and stencil, vertex/fragment shading hooks. Current state ships an edge-function rasterizer (Pineda 1988) with homogeneous clip-space Sutherland-Hodgman against configurable near/far clip depths and viewport planes before perspective divide, configurable depth/stencil/alpha/blend/load/store/viewport/scissor state, tiny vertex/fragment shader hooks, perspective-correct depth + normal + worldPos + UV interpolation, local Matte/Phong material preview with direct UV/checker/image texture paths, mipmapped image textures, tangent-space normal maps, recursive-material fallback diagnostics, material-sidedness-driven default culling, diagnostic depth/normal/id/stencil outputs, opt-in 2x/4x/8x MSAA with per-sample or per-fragment shading, FXAA/SMAA/TAA post-process modes, directional-light shadow maps with PCF/PCSS filtering and texel-grid-stabilized 1-4 cascades, and scene-aware automatic tile-parallel queue selection that preserves explicit caller overrides. Follow-up work:
  - ~~**Far-plane clip policy** — add far-depth semantics deliberately before clipping against the far plane, and make the policy explicit for both raytracer parity and rasterizer teaching.~~ ✅ **Done.** `Rasterizer` now exposes configurable eye-relative near and far clip depths; the far plane defaults to infinity and finite far depths clip geometry before perspective divide.
  - ~~**Geometric polygon-clipping teaching path** — add a Sutherland-Hodgman clip of an already-projected triangle against the four viewport edges, producing an on-screen polygon that can be fan-triangulated. This is the 2D teaching counterpart to the current homogeneous clipper: "here's the canonical screen-space polygon clipper, here's the unified clip-space variant." Real GPUs prefer the scissor-rect we have today (one bbox-scan vs N rasterizer calls), so this is purely a teaching artefact, not a perf win.~~ ✅ **Done.** `core::clipTriangleToRect(...)` and `core::fanTriangulateRasterClipPolygon(...)` provide the reference helper; the runtime rasterizer still uses homogeneous clipping before perspective divide plus the existing scissor/bounding-box path.
  - ~~**Material-sidedness follow-up** — connect material intent to the rasterizer's default cull mode instead of relying only on caller configuration.~~ ✅ **Done.** Front-sided materials default to backface culling, back-sided materials default to frontface culling, and two-sided materials keep both faces unless the caller sets an explicit cull mode.
  - ~~**Imported sidedness safety** — preserve source winding confidence through imported/generated meshes and use it to avoid inferred raster culling when the importer had unknown or corrected winding.~~ ✅ **Done.** `MeshFaceMetadata` carries reliable/unknown/corrected winding through LDraw mesh construction, `MeshPrimitive` leaves, and generated mesh composition; raster metrics report culling and winding/degeneracy triangle rejects for Epic #356.
  - **Texture/material input expansion** — ~~expand the material input path toward image textures, tangent frames, normal maps, and MIP-mapping~~ ✅ **Done.** Raster material evaluation now supports direct image-texture sampling with nearest/bilinear/mipmapped filtering, UV-gradient mip selection, and tangent-space normal maps derived from triangle UVs. Still TODO: bump/displacement, anisotropic filtering, cross-engine shading-normal slots, and material-graph inputs shared with the ray/path tracers.
  - ~~**Tile-parallel performance follow-up** — decide whether the engine needs an automatic tiling heuristic, coarser per-tile work, tile-local depth/color storage with a final stitch, a GPU path, or a scene where shading cost dwarfs tessellation/binning/task/cache overhead.~~ ✅ **Done for the CPU default policy.** Benchmarks now expose projected triangle count, projected bounds, tile-list duplication, framebuffer size, worker count, queue size, and MSAA samples; the default rasterizer uses those signals to select tiled rendering only for measured win cases while preserving explicit `setQueueSize(...)` / `rendercli --queue_size` overrides. Future work remains under broader performance/GPU planning.
  - ~~**Screen-space tessellation LOD for dense previews** — choose lower-detail raster mesh variants for small projected parts, expose preview/balanced/final render intent, keep final-quality overrides visible, and cache repeated source-part variants.~~ ✅ **Done.** Raster pass geometry state now carries quality plus maximum screen-space error, the raster front end measures projected primitive extent and caches `(source primitive, effective LOD)` meshes, and tests cover preview/final selection plus repeated instance reuse for Epic #356 Phase 2.
  - **Rasterizer anti-aliasing follow-up** — MSAA now exists for coverage/depth and resolve. ~~Next, decide whether to add centroid/per-fragment shading for cheaper MSAA previews~~ ✅ **Done.** `MSAAShadingMode::PerFragment` caches the first passing shaded color per triangle/pixel while leaving coverage/depth/stencil per sample. Next, move into the shared sample-pattern/filter API, post-process AA, TAA, alpha-to-coverage, and conservative rasterization from §4.1.c.
  - **Frustum-culling integration** — ~~feed the rasterizer from a frustum-cull-friendly view of the current composite scene rather than traversing every tessellated mesh every frame~~ ✅ **Done.** `Primitive::forEachLeafInBounds(...)` lets the rasterizer reject bounded composite groups before leaf flattening and tessellation. A future §R7 `SpatialIndex` can replace the composite-backed view with a real acceleration tree.
  - **Rendered-doc engine parity backlog** — keep class-level rendered docs honest about which engines can support each scene. Remaining deferred coverage is explicit: `Ring`, `Union`, `Intersection`, `Difference`, `MinkowskiSum`, `ConvexHull`, and the CSG-heavy `ScriptedSurface` scenes stay raytracer-only until mesh booleans / useful scripted tessellation exist; `FishEyeCamera`, `SphericalCamera`, and `EquirectangularCamera` stay raytracer-only until they expose forward projection APIs for Rasterizer and Wireframe; ~~`ThinLensCamera` and `TiltShiftCamera` stay raytracer-only until they expose forward projection APIs for Rasterizer and Wireframe~~ ✅ **Done.** They now expose pinhole-compatible projection fallback for Rasterizer and Wireframe previews while DOF/tilt effects remain raytracer-only; material docs skip Wireframe because it ignores material shading.
  - **Rasterized shadows** — ~~add a shadow-map renderer on top of the depth/stencil path: directional-light shadow maps first~~ ✅ **Done.** `Rasterizer` now has opt-in directional-light shadow maps with `rendercli`, Modeler render-dialog controls, a Modeler live-preview shadow toggle, PCF/PCSS filtering, slope-scaled bias, practical cascade split blending, light-space cascade fitting, texel-grid-stabilized 1-4 cascades, rendered comparison sweeps, an interactive light-depth-map widget, and an interactive cascade-split widget. Cascade split diagnostics stay in the docs/widget surface until Modeler has a broader debug-overlay or multi-view framework.
  - **Planar reflections and portals** — classic stencil/pass-graph use case: mark a mirror/portal surface, render a reflected or redirected view only through that mask, clip against the portal/mirror plane, then composite. This gives raster/GPU previews good parity for flat mirrors, water planes, polished floors, and portal screens without pretending arbitrary recursive reflection is a raster strength. Water is the motivating natural-material case here: calm planar water can preview through reflection/refraction passes, while waves, caustics, foam, spray, and absorption belong to the natural-phenomena backlog.
- **OpenGL viewport.** Real-time editor view. Tessellated meshes feed VBOs; GLSL shaders mirror the material library for live preview parity. Also unlocks gizmo rendering.
- **WebGL / WebGPU preview.** The same scene rendered in a browser, served alongside the GitHub Pages docs. WebGL first (broadest support, simpler), WebGPU as a follow-up. Static-scene preview is the v1 target; web preview of *animations* (timeline scrubbing in the browser) is a stretch goal that lands after §4.7. This engine doubles as the canvas for the §4.0 interactive diagrams — the rendering engine and the explainer engine are the same code path.
- **Path tracer.** Monte Carlo integrator over the same scene graph. Multiple Importance Sampling between BSDF sampling and light sampling. Stratified or Sobol QMC sampling. Adaptive sampling per tile. The implementation should explicitly compare **megakernel**, **wavefront/queue-based**, and **material-sorted** architectures: a simple megakernel is the clearest CPU baseline, while wavefront queues expose GPU divergence, occupancy, compaction, and BSDF/light sorting tradeoffs. ~~SoA ray packets (`Ray4`/`Ray8`), primitive packet entry points, and block-batched BVH traversal for CPU packet traversal.~~ ✅ **Done.** Added aligned packet transport, scalar-fallback packet primitive entry points, shared SIMD packet kernels for core primitives, `BoundingBox::intersects4`, and `BVH::intersectPacket(Ray4/Ray8)` active-mask descent with `BVHPacketBenchmark.cpp` coverage for Epic #141 Phases 4.1 and 4.3. `Ray4` is supported through SSE or NEON where available; `Ray8` remains AVX-only.
  ~~Dimensioned sampler streams for pixel, time, lens, BSDF, light, and continuation ownership.~~ ✅ **Done.** `SampleStream` now exposes explicit named dimensions so the future path tracer can request independent per-bounce samples without call-order coupling for Epic #358.
  ~~BSDF sample/PDF contracts for diffuse, glossy, specular, and transmission lobes.~~ ✅ **Done.** `render::BSDF` now has explicit delta metadata and caller-owned 2D sampling; Lambertian, Phong glossy, perfect reflection, and perfect transmission tests pin sample/PDF consistency for Epic #358.
  ~~Light sampling, PDF, delta-light, emission, and power metadata APIs for direct-light estimators and MIS.~~ ✅ **Done.** `Light::sample(point)`, `Light::pdf(point, direction)`, `Light::isDelta()`, `Light::emission()`, and `Light::power()` now expose the runtime sampling contract for Epic #358 while preserving the current direct-lighting path.
  ~~Deterministic sampler seeding for sampling-heavy regression tests.~~ ✅ **Done.** Sampler setup and raytracer frame scheduling now expose explicit seed hooks with render/tile/pixel/sample derivation, while production defaults remain unseeded unless callers opt in for Epic #358.
  ~~Multiple-importance-sampling arithmetic helpers for future direct-lighting estimators.~~ ✅ **Done.** `render::mis` now provides balance/power heuristics and small direct-lighting estimate helpers that combine BSDF values, light radiance, cosines, PDFs, and delta-sample handling for Epic #358.
  ~~Scalar megakernel integrator over the BSDF/light sampling substrate.~~ ✅ **Done.** `render::PathTracingIntegrator` now walks paths iteratively, consumes `SampleStream`, samples lights for next-event estimation, applies Russian roulette, and remains selectable as ray-family graph state while Whitted stays the default transport policy.
  ~~Initial wavefront executor surface.~~ ✅ **Done.** `engine::wavefront::WavefrontRaytracer` now ships as a sibling `RenderEngine`, with render-intent, graph, rendercli, Modeler render-settings selection, and graph trace metrics for tiles, samples, active samples per depth, radiance deltas per depth, batch mode, queue choice, and timing. It owns primary-ray tile sampling and submits samples through `Integrator::radianceBatch`; `PathTracingIntegrator` already processes those batches depth-major, while Whitted compatibility still falls back to scalar recursive material callbacks until explicit legacy ray queues land.
  These completed bullets are **path-tracing foundations**, not a finished production path tracer: material BSDF coverage is still partial, no soft area-light shadows ship, wavefront scheduling still needs explicit queue ownership, and the default transport policy remains `render::WhittedIntegrator`.
- **GPU backends, eventually.** Vulkan compute, OptiX, or Metal. Templated math primitives port reasonably to CUDA. Massive undertaking; not a near-term priority. When this graduates from aspiration to plan, include mesh/task shaders, bindless resource models, shader-language targets (GLSL/HLSL/WGSL/SPIR-V), and CPU/GPU residency rules for out-of-core scenes rather than treating "GPU" as only a faster execution device.

The "all engines over one scene" property is itself the pedagogical payoff — being able to render a single test scene through wireframe, software raster, OpenGL, WebGL, raytracer, and path tracer side-by-side teaches more about the rendering equation than any single engine ever could.

#### 4.1.a Render-pass graph and hybrid execution

The long-term renderer should not treat "Rasterizer", "Raytracer", "Wireframe", and "PathTracer" as mutually exclusive whole-frame endpoints. The state-of-the-art shape is a render graph / frame graph: a per-frame DAG of passes and typed resources, compiled from the scene, camera, and render intent. Unreal's RDG, Unity's Render Graph, Frostbite's FrameGraph, and USD Hydra's scene/render delegate split are the industry reference points; the project version should stay smaller and more explicit, but aim at the same separation of concerns.

Proposed architecture:

- **`RenderPlan` DAG** — produced per frame from scene features plus explicit user overrides. Nodes are passes; edges are resource dependencies.
- **Typed resources / AOVs** — colour, depth, stencil, object id, material id, normal, world position, motion vectors, shadow masks, reflection targets, user render textures, and the final display/export target.
- **`RenderPass` contract** — each pass declares inputs, outputs, required scene subset, camera/view state, clear/load/store behaviour, and the executor it needs (`raster`, `raytracer`, `wireframe`, `pathtracer`, compositor, future GPU rasterizer, future GPU ray backend).
- **Pass executors instead of monoliths** — engines become implementations of pass kinds. A `RasterDrawPass`, `RayShadowMaskPass`, `WireframeOverlayPass`, `PathTraceBeautyPass`, `CompositePass`, and `TonemapPass` can all contribute to one frame.
- **Planner + explicit override** — automatic discovery handles common cases; advanced users can still pin or reorder passes for experiments and teaching.

Feature discovery should start conservative and grow by material / object intent:

- Lights that request preview shadows add a `ShadowMapPass` or `RayShadowMaskPass`.
- Materials that request planar reflection add a stencil mask, reflected-view pass, clip plane, and composite step.
- Objects marked as screens, portals, scopes, mirrors, minimaps, or render-texture receivers produce offscreen render targets and dependency edges before the containing scene pass.
- Overlays and diagnostics add wireframe, normal, depth, object-id, or bounding-volume passes on top of the beauty pass.
- Postprocess effects request the AOVs they need: depth for DoF/SSAO, motion vectors for motion blur/TAA, normals/world positions for denoisers and relighting.

Canonical hybrid examples:

- **Computer screen inside a photoreal scene** — render a nested cartoon scene to `screen_color`, render wireframe diagnostics over that texture, then render the main environment with `screen_color` bound as the screen material.
- **Rasterized preview with raytraced shadows** — rasterize a G-buffer (`world_position`, `normal`, `material_id`, `depth`), run a raytraced `shadow_mask` pass against the scene BVH, then shade/composite in raster or image space.
- **Planar mirror / water plane** — mark the mirror pixels in stencil, render the scene from a reflected camera only where stencil matches, clip reflected geometry against the mirror plane, then blend with the mirror material.
- **Mixed-style frame** — rasterize the base scene, path-trace a hero object or inset panel into a separate target, draw wireframe overlays where requested, then depth/object-id composite the pieces.

Hard problems to design for up front:

- **Shared semantics** — every executor must agree on camera projection, clip/depth conventions, colour space, alpha, normals, object/material IDs, material intent, and AOV names.
- **Scene partitioning** — the planner needs layers, tags, material features, visibility masks, render-target ownership, and "this surface shows that scene" relationships. Geometry alone is not enough.
- **Cycles and recursion limits** — mirrors reflecting mirrors, portals showing portals, and screens displaying scenes that include the same screen create graph cycles. Break them with explicit recursion limits, previous-frame history, or diagnostic errors.
- **CPU/GPU resource boundaries** — the current CPU engines can share `Buffer<T>` easily; future OpenGL/WebGPU/Vulkan paths must avoid readbacks by keeping resources resident on the device until the final display/export boundary.
- **Parity limits** — flat reflections, shadow masks, environment probes, and overlays can preview well in raster. Arbitrary recursive reflection/refraction, caustics, and multi-bounce glossy transport remain ray/path tracer territory.

Implementation order:

1. Define ~~render-resource descriptors~~ and AOV handles (`Color`, `Depth`, `Stencil`, `ObjectId`, `Normal`, `WorldPosition`, `MotionVector`, custom texture). ✅ **Partially done.** Initial `engine::graph::RenderResourceDescriptor` covers typed CPU/GPU-open image-resource declarations, with execution-time `RenderResource` objects and CPU storage for color/depth/stencil/object-id buffers; concrete AOV handles and future non-image resource categories remain TODO.
2. Add a minimal `RenderPass` interface with ~~declared reads/writes~~, clear/load/store operations, and ~~an executor enum~~. ✅ **Partially done.** Initial `engine::graph::RenderPassNode` declares read/write resources, executor, pass kind, disabled behavior, graph validation/export, disabled default substitution, and color passthrough; load/store operations and broader graph execution remain TODO.
3. Split current monolithic engines into first passes without changing output: ~~graph-backed whole-frame raytracer, rasterizer, and wireframe beauty execution~~ ✅ **Partially done.** `RenderGraphCompiler` now emits a single whole-frame beauty plan and `GraphRenderEngine` executes that plan through the existing Raytracer/Rasterizer/Wireframe engines; simple enabled `Tonemap` passes can transform color resources, while named pass payload classes and wireframe overlay composition remain TODO.
4. Add `CompositePass` plus offscreen render-to-texture so nested scenes can be rendered before the materials that consume them.
5. Ship the first hybrid demo: photoreal main scene with a cartoon/wireframe render-target screen inside it.
6. Add planar reflections and raster shadow maps as pass-graph clients.
7. Add raytraced shadow masks and path-traced inset/hero passes once AOV resource sharing is solid.
8. Build a plan visualizer: ~~dump the DAG as text/Graphviz~~ and show resources, lifetimes, and executor choices in docs. ✅ **Partially done.** `RenderPlan::toText()` and `RenderPlan::toDot()` expose inspectable plan dumps, `rendercli --render_graph_only --render_graph_format text|dot|json` exports compiled plans with disable filters and default intent overrides, `rendercli --render_graph_in` replays saved JSON plans, and the textbook now has a render-graph volume; interactive plan visualization remains TODO.

This pass graph is the bridge between renderer parity and composability. It lets the rasterizer and future GPU rasterizer preview the parts they can approximate, while still delegating specific expensive or truth-critical work to raytracing/path tracing.

#### 4.1.b Renderer architecture patterns

Engine implementations should be compared as algorithms in their own right, not hidden behind a single `render()` call:

- **Megakernel path tracing** — one straightforward recursive/iterative kernel owns most path state; best for readability and CPU-first pedagogy, worst for GPU divergence.
- **Wavefront path tracing** — queues for ray generation, intersections, material evaluation, shadow rays, medium sampling, and compaction; teaches GPU scheduling, occupancy, and data-oriented transport.
- **Material/light sorted queues** — batch shading work by BSDF, texture set, or light to improve coherence; useful on both CPU SIMD and GPU.
- **Tiled, scanline, packet, and streaming execution** — contrast image-space decomposition with ray/path queues and ray packets.
- **Out-of-core rendering** — scene, texture, and acceleration-structure streaming for data sets larger than memory; important once OpenVDB, USD, film-resolution textures, and large meshes enter the roadmap.

These are implementation strategies, not user-facing engines, so they belong under the same `RenderEngine` / render-graph umbrella as PathTracer, Rasterizer, and future GPU backends.

#### 4.1.c Anti-aliasing and raster quality

Anti-aliasing should be a cross-engine feature rather than a one-off rasterizer trick. The shared API is "sample pattern + reconstruction filter + resolve", with each backend choosing the parts it can implement:

- **Camera supersampling baseline** — already natural for raytraced engines via samples-per-pixel; formalise regular, jittered, stratified, blue-noise, and Sobol sample patterns so visual comparisons use the same scenes and seeds.
- **Reconstruction filters** — box, tent, Gaussian, Mitchell-Netravali, Lanczos, and Blackman-Harris, with weight accumulation in the float framebuffer and side-by-side renders that show blur, sharpness, and ringing.
- **Rasterizer MSAA** — the software rasterizer now has fixed 2x/4x/8x subpixel coverage with independent per-sample color/depth/stencil buffers and a resolve into the float framebuffer. `MSAAShadingMode::PerSample` shades covered samples directly for correctness; `MSAAShadingMode::PerFragment` is the cheaper preview mode and reuses the first passing shaded color per prepared triangle/pixel.
- **Post-process AA** — ~~FXAA for the software rasterizer preview path~~ ✅ **Done.** `render::postprocess::applyFxaa` now runs as an opt-in `Rasterizer::PostProcessAA::FXAA` pass, exposed through `rendercli --post_aa fxaa` and the Modeler render dialog. ~~Add a first CPU SMAA-style image-space follow-up for sharper raster previews.~~ ✅ **Done.** `render::postprocess::applySmaa` adds a luminance-edge detection and directional blend pass exposed as `Rasterizer::PostProcessAA::SMAA`, `rendercli --post_aa smaa`, and the Modeler render dialog for Epic #167.
- **Temporal AA / TAAU** — ~~software-rasterizer TAA with jittered frames, depth-validated history reprojection, lifecycle reset, and rendercli/Modeler selection~~ ✅ **Done.** `Rasterizer::PostProcessAA::TAA` now accumulates repeated raster frames through managed color/depth history and resets on first frame, resize, or explicit invalidation (#167). Still TODO: neighborhood clamping, richer motion-vector policy for moving scenes, TAAU, and shared cross-engine AOV integration.
- **Transparency edge cases** — alpha-to-coverage, stochastic alpha, and stochastic transparency so foliage, hair, sprites, and partially transparent surfaces have an AA story.
- **Conservative rasterization** — useful for voxelization, visibility masks, and procedural geometry; implement as its own rasterizer mode rather than folding it into ordinary MSAA.

#### 4.1.d Interactive display buffers

`RenderWidget` / Modeler currently expose progress by letting worker threads mutate a display buffer that the UI thread periodically copies into a `QImage`. That is simple and useful, but it mixes "render target" and "paintable snapshot" in one object. The long-term display pipeline should be explicit:

- ~~**Front/back display buffers** — workers render into a back buffer or tile-local staging buffer; the UI paints an immutable front buffer. Completed tiles or frames publish by swap, avoiding partial reads while a worker is writing.~~ ✅ **Done.** `RenderWidget` now owns a render-thread back buffer and UI-thread front `QImage`; `paintEvent` only draws the immutable front image.
- ~~**Dirty-tile publication** — engines report completed tiles; the widget only converts/copies dirty tiles into the paintable `QImage` instead of rebuilding the full image every paint event.~~ ✅ **Done.** `RenderEngine::completedTiles()` lets the raytracer publish completed LDR tiles progressively; non-progressive engines publish the full frame on completion.
- ~~**Progressive mode as a policy** — keep live partial rendering for raytracer previews, but let final-frame and screenshot paths require complete tile publication only.~~ ✅ **Done.** `RenderWidget::DisplayMode` exposes periodic whole-buffer updates, completed-tile publication, and final-frame double buffering; Modeler's render dialog exposes all modes, and its central preview keeps the previous image visible while reusing the raytracer back buffer for 16 ms whole-buffer point-interlaced updates plus non-canceling double-buffered Rasterizer/Wireframe updates. Point-interlaced view planes now derive their first coarse pass from the full view plane instead of per-tile dimensions, and raytracer cancellation uses an atomic camera flag with additional sample-loop checks.
- **HDR/display split** — preserve the `Buffer<Colord>` accumulator for offline output while the display path owns LDR snapshots, tonemap timing, and optional progress overlays. Current baseline: LDR display snapshots are separate from HDR engine output.
- **Resize/cancel safety** — buffer swaps, render cancellation, and widget resize need a clear ownership protocol so Modeler can change scenes/cameras while long renders are stopping.

### 4.2 Primitives, meshes & computational geometry

Mesh objects need to be grounded in a comprehensive computational geometry library — both because the modelling and rendering engines depend on the same primitives, and because the educational scope (§4.0) is unbounded here in the best way: most of computer graphics *is* computational geometry.

#### 4.2.a New primitives

Easy wins that fill obvious gaps:

- Cylinder, cone, capsule (cylinder with hemispherical caps).
- Heightfield / terrain.
- Signed Distance Field primitive base, with sphere-tracing / distance-field ray marching intersectors. Opens up procedural geometry: mandelbulbs, gyroids, smooth-blend CSG, metaballs / blobby implicit surfaces, neural or scripted SDFs, and fractal terrain.
- Bezier patches / NURBS — for the "all variants" pedagogy, though niche in modern pipelines.
- Subdivision surfaces (Catmull-Clark, Loop, Doo-Sabin) — depends on `Mesh` infrastructure.
- Particles / billboards — for sprites or fast vegetation.
- Curve primitives — ~~polyline curves rendered as ribbons or tubes for mesh-consuming engines, with overlay and attribute-color documentation plus reusable path fixtures~~ ✅ **Done.** Runtime `render::Curve` tessellates `core::Polyline` paths into ribbon/tube meshes for raster and wireframe render paths, exposes center-line overlays, and documents importer-facing fixture coverage for Epic #231. Bezier/B-spline/NURBS curves and true curve intersections remain TODO; this is the geometry side of hair/fur and stroke rendering, separate from hair shading or physics.
- Point clouds / surfels — direct point rendering, EWA splatting, and conversion paths into meshes, SDFs, or Gaussian splats.

Once tessellation lands (R4), the geometry engine can also do:

- CSG mesh booleans (libigl, Carve, or BSP — implement at least two for comparison).
- Marching cubes for implicit/SDF visualization.

#### 4.2.b Spatial acceleration structures

Behind the §3 R7 interface, with each variant documented and benchmarked side-by-side:

- BVH (binary, SAH-built; also try equal-counts, mid-split, and binned SAH for the writeup).
- ~~BVH block-batched packet traversal (Ray4/Ray8 active-mask descent; coherent primary-ray cache-reuse path)~~ ✅ **Done.** `BVH::intersectPacket(Ray4/Ray8)` added with active-mask descent; `BVHPacketBenchmark.cpp` measures coherent vs incoherent throughput; closes Epic #141 Phase 4.3.
- Octree (loose and tight variants).
- kd-tree (with and without SAH).
- Uniform grid and hashed grid.
- Two-level TLAS / BLAS for instancing.
- Brute-force list (kept as the reference baseline — the "before" in every benchmark).

Each structure ships with an interactive WebGL diagram in `docs/interactive/` showing build, traversal, and a query overlay.

#### 4.2.c Computational geometry library

A reusable `geometry/` module that the modelling UI, the engines, and any scripts can call into:

- Predicate kernels: orientation, in-circle/in-sphere (Shewchuk-style adaptive precision).
- Convex hull (Graham scan, gift wrapping, quickhull, Chan's algorithm — multiple variants for the textbook value).
- Triangulation: ear clipping, monotone polygon, Delaunay (Bowyer-Watson, divide-and-conquer, incremental), constrained Delaunay.
- Voronoi diagrams (dual of Delaunay).
- Polygon Boolean operations (Vatti, Greiner-Hormann).
- Minkowski sums.
- Mesh repair: hole filling, manifold check, T-junction welding, normal recomputation.
- Mesh simplification (quadric error metrics) and remeshing.
- UV unwrapping (LSCM, ABF, ABF++).
- Geodesics on meshes.
- Skeletonization, medial axis transforms.
- Curves and surfaces: Bezier, B-spline, NURBS evaluation; tessellation; intersection.

This is intentionally over-scoped — the *library* exists to learn from. We add to it lazily as the modelling UI and engines need pieces, but the design intent is "every algorithm we cared enough about to read a chapter on goes here."

### 4.3 Materials, textures & shading

The "all variants" rule applies hardest here. The job isn't to ship one PBR shader; it's to ship a stable framework with *every interesting BSDF, every interesting texture pipeline, and every interesting shading-normal trick* hanging off it.

#### 4.3.a BSDFs (all the variants)

Layered upgrade path, but the destination is the full list:

- **Lambertian** (already there).
- **Phong / Blinn-Phong** (already there) — kept as the legacy baseline and as the NPR entry point.
- **Oren-Nayar rough diffuse** — the first "Lambert is an approximation" step; cheap, intuitive, and good for chalk, clay, unpolished stone, and fabric bases.
- **GGX / Trowbridge-Reitz microfacet** (isotropic and anisotropic) — the modern workhorse.
- **Cook-Torrance, Beckmann, GGX-VNDF** — comparative implementations, all behind the same BSDF interface.
- **Disney Principled BSDF** — artist-friendly über-shader over a diffuse base.
- **Smooth and rough dielectric** with proper Fresnel and nested-medium tracking.
- **Conductors** (with full Fresnel from complex IOR — copper, gold, silver, aluminium, iron).
- **Emission materials** — constant, textured, and mesh-emitter-backed surfaces, with the same material usable by the Whitted, raster, and path-tracing engines even if only the latter two can exploit every path.
- **Thin surfaces** — thin glass, thin translucent sheets, leaves, paper, water films, wet surfaces, and two-sided materials; important because not every transparent object deserves full nested-medium volume handling.
- **Layered materials** (clear coat over base; iridescence as a thin-film layer).
- **Iridescence / thin-film interference** — proper interference, not a colour ramp.
- **Car paint / metallic flakes** — base + flake + clearcoat model; a good stress test for layered BSDFs and material presets.
- **Sheen / fabric BRDFs** (Charlie / ASM cloth).
- **Hair / fur** (Marschner, Chiang).
- **Subsurface scattering** — random-walk SSS in PT, Christensen-Burley for fast preview, dipole / multipole diffusion, and BSSRDF for textbook completeness; skin/jade/milk/wax tests.
- **Volumetric participating media** (homogeneous + heterogeneous) — fog, smoke, clouds; Henyey-Greenstein and Mie phase functions. Treat the transport algorithms as first-class: ray marching for the teaching baseline, distance sampling for homogeneous media, delta / Woodcock tracking, ratio and residual-ratio tracking, null-collision estimators, spectral/decomposition tracking, transmittance estimators, and acceleration structures for heterogeneous grids. Natural phenomena presets — fog banks, smoke plumes, clouds, flame volumes, mist, dust, spray — should be built on this layer rather than one-off materials.
- **NPR shaders.** Toon/cel, Gooch (warm/cool), hatching, contour/silhouette extraction, halftone, Kuwahara. NPR is its own family — not "make the PBR pipeline look stylised" but a parallel pipeline with its own shading-normal logic.
- **Spectral rendering** — replace RGB with wavelength sampling for correct dispersion, fluorescence, and (eventually) polarization. Big architectural change; lives at the end of this pillar.

Pragmatic first wave: GGX conductor/dielectric, Oren-Nayar, emission, and a metallic/roughness material that maps cleanly to glTF. That gives the renderer modern-looking surfaces before the full Disney/layered/spectral catalog lands.

#### 4.3.b Textures (a four-layer stack)

Textures are not just colour sources — they are the input to every shading parameter, and the framework should make that explicit. The four layers, from most concrete to most abstract:

1. **Image textures** with proper sampling: ~~nearest / bilinear / trilinear; MIP-mapping; clamp/repeat~~ ✅ **Done.** CPU `ImageTexture` now exposes explicit nearest, bilinear, and mipmapped filtering with clamp/repeat wrap and raster UV-gradient mip selection (#167). Still TODO: anisotropic filtering, mirror wrap, sRGB vs linear, and HDR (EXR / Radiance HDR).
2. **Procedural textures** (Perlin, simplex, Worley, checker, marble, wood, brick, gabor, value noise, fractal noise variants). Templated `ProceduralTexture<F>` on a noise functor.
3. **Calculated / data-driven textures** — functions of position, normal, UV, view, hit attributes, and previous shading state. Used for AO bake-in, curvature shading, world-space gradients, etc.
4. **Scripted textures.** Same DSL/scripting layer as §4.6's parametric objects. The user (or the AI agent) can write `(u, v, p, n) → colour` in a small language and drop it on a material slot. Hot-reload at edit time. This is where most of the "I want to try X" experiments happen; making it first-class is what keeps them from leaking into the C++ codebase.

Texture inputs feed *any* material parameter — albedo, roughness, metallic, IOR, normal, displacement, emission, subsurface radius, sheen colour, etc. — through a common `Sampler2D<T>` interface.

The texture-mapping roadmap is separate from the texture-source roadmap:

- **UV mapping first** — carry stable UVs from `Mesh::Vertex` through `HitPoint`, the rasterizer, and every material evaluation path. Tests should pin seam duplication and primitive-generated UV conventions.
- **Derivative and tangent data** — expose `dPdu`, `dPdv`, ~~`dUVdx`, and `dUVdy` where an engine can provide them; needed for MIP level selection~~ ✅ **Done.** The software rasterizer now computes per-triangle UV gradients for image-texture mip selection (#167). ~~Derive raster tangent/bitangent frames for tangent-space normal maps.~~ ✅ **Done.** Raster material evaluation derives per-triangle tangent frames from world-space and UV edges for normal-map preview (#167). Still TODO: full surface derivatives for bump mapping and anisotropic filtering.
- **Projection modes** — planar, spherical, cylindrical, cube/box, triplanar, object-space, world-space, and camera/projector mapping. These should be explicit mapping objects, not special cases inside individual textures.
- **Environment mapping** — equirectangular and cubemap lookup for backgrounds, reflection/refraction probes, and HDRI lighting. This overlaps §4.4.b's HDRI light, but the material side needs its own sampler, transform, and roughness-filtered lookup path.
- **Texture cache and filtering** — image decode, colour-space conversion, MIP generation, tile/cache lifetime, and sampling policy belong below materials so all engines see the same texture values.
- **Material slots** — albedo, roughness, metallic, specular, IOR, alpha, emission, normal, bump height, displacement, ambient occlusion, clearcoat, sheen, and subsurface slots should all accept the same `Sampler2D<T>` machinery.

#### 4.3.c Shading-normal pipeline

A first-class pass that runs between intersection and BSDF eval, with all four standard modes implemented and selectable per material:

1. **Geometric normal** (the raw triangle normal — the baseline).
2. **Smooth / interpolated normal** (vertex normals, tangent-frame interpolation).
3. **Bump mapping** (height-derivative perturbation; needs `dPdu`/`dPdv` on the hit).
4. **Normal mapping** (~~tangent-space normal sampled from a texture for raster previews~~ ✅ **Done.** The software rasterizer now samples Matte/Phong normal textures through UV-derived tangent frames (#167); still TODO: cross-engine shading-normal slots and Mikktspace tangents across non-planar UV seams).
5. **Parallax / steep parallax / parallax occlusion** (visual depth without geometry).
6. **Displacement mapping** (real geometry — pre-tessellation displacement on `Mesh`, or adaptive subdivision in PT).

Each mode gets its own doc page and interactive WebGL diagram showing the difference from the previous mode on the same scene.

#### 4.3.d AOV (Arbitrary Output Variable) pipeline

Every renderer writes more than a beauty pass. AOVs are first-class outputs of the integrator, typed resources in the render-pass graph (§4.1.a), and inputs to the postprocessing/compositing stage (§4.9):

- Beauty (RGB).
- Depth (Z) and normal.
- Albedo / diffuse / specular separation.
- Direct vs indirect; per-light contribution; per-material-id mask.
- Object-id and material-id (for compositing and selection).
- Cryptomatte (for proper anti-aliased ID masks).
- Motion vectors (for temporal denoise and motion blur compositing).
- World-space position, UV.
- Roughness, metallic, emission.
- Sample variance (for adaptive sampling and denoise).

#### 4.3.e Volumetric transport details

Volumetrics are large enough to deserve an implementation plan adjacent to the BSDF work rather than a single material bullet. The minimum educational ladder:

- **Homogeneous media** — analytic transmittance, exponential free-flight sampling, single scattering, and a constant-density fog scene.
- **Heterogeneous media** — ray marching baseline, majorant grids, delta / Woodcock tracking, ratio tracking, residual-ratio tracking, and null-collision estimators compared on the same density fields.
- **Phase functions** — isotropic, Henyey-Greenstein, Rayleigh, Mie, and artist-facing approximations for clouds/smoke.
- **Grid acceleration** — dense grids first, then sparse/OpenVDB-style trees, brick maps, macro-cell majorants, and octree/BVH integration from §R7.
- **Volume light transport** — next-event estimation inside media, equiangular sampling near lights, volumetric MIS, photon beams / beam radiance estimates, and volumetric photon mapping as the historical comparison.
- **Authoring and diagnostics** — transfer-function previews, density/majorant overlays, step-count AOVs, transmittance visualizers, and rendered comparisons of bias/variance tradeoffs.

#### 4.3.f Material library (separate from material types)

A bundled catalog of preset materials calibrated against measured data (MERL BRDF database, Disney measured materials, Filament reference values). Gold, copper, jade, glass, rubber, brushed steel, marble, skin, pearl, silk, etc.

- Stored as JSON, hot-reloadable.
- Auto-render thumbnails (preview sphere on a checkerboard) for the UI library browser.
- Naming scheme that an LLM can pattern-match (e.g., `metal/copper/polished`, `glass/crown/optical`, `organic/jade/imperial`).

### 4.4 Cameras and lights

#### 4.4.a Cameras (the completionist set)

The current four (pinhole, fish-eye, orthographic, spherical) become the core; the roster expands to cover the cameras that actual cinematographers and researchers care about. Each gets a doc page comparing it to the others on the same test scene.

- **Pinhole** (existing) — the textbook entry point.
- **Orthographic** (existing) — for engineering views.
- **Fish-eye** (existing) — equidistant projection, with stereographic and equisolid variants added.
- **Spherical** (existing) — partial-sphere projection with tunable horizontal/vertical FOV. ✅ **Equirectangular** added as a separate dedicated full-360°×180° camera (`raytracer::EquirectangularCamera`, 35ac267) — for HDRI authoring, since `SphericalCamera` doesn't quite cover the canonical full-sphere case.
- ✅ **Thin-lens** — depth of field, ~~bokeh shape (circular, polygonal, custom texture for cat-eye/anamorphic)~~. Basic DoF + circular bokeh done in 3e42f4d (`raytracer::ThinLensCamera`); polygonal/custom-texture bokeh shapes still to come.
- **Kolb realistic camera** — full multi-element lens stack with chromatic aberration, vignetting, distortion. The pedagogical centrepiece for "how do real cameras work."
- **Panini projection** — wide angle without the fish-eye distortion.
- **Cylindrical / panoramic.**
- **Omnidirectional / cubemap** — six pinholes glued together.
- **Light-field camera** — multi-perspective rendering for refocusable output.
- **Stereo cameras** — both **parallel-frustum** (off-axis, no keystone) and **toed-in / converged** (camera angle) variants, with explicit IPD and convergence distance controls. Anaglyph/side-by-side/top-bottom output formats for compatibility with viewers.
- ✅ **Tilt-shift / Scheimpflug** — ~~independent control of sensor and lens planes.~~ Done as `raytracer::TiltShiftCamera` (subclass of `ThinLensCamera`). Rotates the focal plane around the camera's local right axis (`tilt`) and supports a lens-shift offset (`shiftX` / `shiftY`) for converging-vertical correction. Not full Scheimpflug — only the focal plane rotates; image and lens planes stay perpendicular. The full physical Scheimpflug condition is deferred to the future Kolb camera.

All cameras share a `Camera::generateRay(sample) → Ray` API; lens-based cameras additionally implement aperture sampling for DoF.

#### 4.4.b Lights

- **Point** (already exists in some form).
- **Directional** (sun).
- **Spot** (with inner/outer cone falloff).
- **Area** — rectangular, disk, sphere, cylinder emitters with proper sampling (uniform, importance, projected solid angle).
- **Mesh emitter** — any triangle mesh, sampled by area; importance-sampled by triangle area for big meshes.
- **HDRI environment** — image-based lighting from an EXR/HDR sky map; importance-sampled by luminance (with hierarchical / Sobol-distributed sampling for comparison).
- **Sun + sky model** (Hosek-Wilkie or Preetham analytical sky). Time-of-day and turbidity controls.
- **Volumetric lights / god rays** — emerge naturally from §4.3's volumetrics; called out so they aren't forgotten.
- **IES profiles** — real-world photometric data for architectural lighting.
- **Portal lights** — sampling helper for indirect daylight through windows.
- **Many-light sampling systems** — light trees, alias tables, RIS / reservoir sampling, Lightcuts, virtual point lights / instant radiosity, ReGIR, and RTXDI-style direct-light reservoirs. These are the bridge between a few hand-authored lights and production scenes with thousands or millions of emitters.

All require ~~a `Light::sample(shadingPoint) → (wi, pdf, Le)` API for proper integration in MC integrators, plus a `Light::pdf(wi)` for MIS and `Light::power()` / bounded-emission metadata for light-tree construction and many-light sampling~~ ✅ **Done.** Runtime lights now expose the sampling, PDF, delta-light, emission, and power metadata contract for Epic #358. Remaining work is to add non-delta area/environment/mesh emitters and the integrators that consume the API.

### 4.5 File I/O

Read and write wherever it's reasonable. The guiding rule: if a format is used as a *delivery* format (PLY, STL, EXR), write-support is "nice to have." If a format is used as an *interchange* format (OBJ, glTF, USD, FBX), read+write is the target.

- **OBJ + MTL** — ubiquitous, mesh-only. Read first (~1 day); write second (~half day). The universal "just load this mesh" fallback.
- **STL** — 3D printing. ~~Read ASCII and binary variants.~~ ✅ **Done.**
  `core/formats/stl` parses STL triangles into `Mesh`, and
  `world::StlSceneImporter` wraps them as flat imported `MeshPrimitive`
  geometry with unit/material diagnostics and rendercli smoke fixtures for Epic
  #235. Write support remains TODO.
- **3MF** — 3D printing interchange. ~~Core package import: ZIP container,
  model XML, object meshes, build items, transforms, units, and base material
  display colors.~~ ✅ **Done.** `core/formats/threemf` parses core package
  parts and `world::ThreeMfSceneImporter` maps build items to transformed
  grouped imported meshes with rendercli smoke coverage for Epic #235;
  production extensions, textures, and write support remain TODO.
- **PLY** — already read (with LibFuzzer harness). Add write.
- **glTF 2.0** — ~~low-level `.gltf` / `.glb` parsing for buffers,
  bufferViews, accessors, and image references~~ ✅ **Done.**
  `core::gltf::Reader` validates JSON/GLB containers and resolves external
  and embedded payloads for Epic #233. ~~Scene and node hierarchy import as
  editable `Group` nodes with names, transforms, parent-child structure, and
  source provenance.~~ ✅ **Done.** `GltfSceneImporter` registers `.gltf` /
  `.glb` files and maps source scenes/nodes into `Group` metadata for Epic
  #233. ~~Initial animation sampler/channel import for node transform
  timelines.~~ ✅ **Done.** Simple translation, rotation, and scale channels
  become world timeline tracks while unsupported targets produce diagnostics
  for Epic #233. ~~Triangle mesh primitives with POSITION, indices, NORMAL,
  TEXCOORD_0, computed-normal/zero-UV fallbacks, and base-color material
  references.~~ ✅ **Done.** `GltfSceneImporter` compiles node mesh primitives
  into shared `MeshPrimitive` geometry with rendercli smoke coverage and
  supported-subset docs for Epic #233. ~~PBR base-color factors and base-color
  textures map to diffuse renderer materials with diagnostics for unsupported
  metallic/roughness, alpha, double-sided, and extension features.~~ ✅ **Done.**
  `GltfSceneImporter` builds `MatteMaterial` diffuse textures from glTF material
  data for Epic #233. Remaining material features, skeletal animation, morph
  targets, compression extensions, and write support remain TODO; the natural
  default for web interop (feeds the §4.1 WebGL viewer directly).
- **USD / OpenUSD** — Pixar's industry-standard scene description. Heavy dependency but the right long-term home for everything (geometry, materials, animation, layered overrides, references). Read-only first; write is aspirational.
- **FBX** — Autodesk; via OpenFBX. Read + limited write.
- **OpenVDB** — volumetric grids, once §4.3 volumetrics land. Read-only initially.
- **EXR** — float HDR output from the framebuffer (R1) and environment map input. Read + write via OpenEXR or tinyexr.
- **HDR (Radiance `.hdr`)** — environment maps. Read.
- **OpenSCAD `.scad`** — ~~external CLI compile adapter with missing-tool
  diagnostics and generated mesh caching~~ ✅ **Done.** `OpenScadSceneImporter`
  compiles `.scad` sources through an optional OpenSCAD executable into cached
  STL/PLY mesh output, routes generated meshes through the imported mesh path,
  records source provenance, and now has checked-in primitive/transform/boolean
  fixtures plus rendercli smoke coverage for Epic #234; native scripted-object
  authoring remains in §4.6.
- **G-code `.gcode`** — 3D-printer toolpaths. ~~Core visualization parser for
  common movement, extrusion, feed-rate, layer comments, temperatures, tool
  changes, absolute/relative axes, absolute/relative extrusion, and ignored
  dialect commands with diagnostics.~~ ✅ **Done.** `core/formats/gcode`
  preserves printer path segments and metadata for Epic #235; ~~scene importer
  conversion to curves/layers with print visualization modes~~ ✅ **Done.**
  World import and rendercli now expose layer/tool/speed/temperature/move-type
  coloring, layer filtering, cumulative layers, and travel hiding for Epic #235.
- **LDraw `.dat` / `.ldr`** — LEGO part and model text format. ~~Core line
  parser for command types 0 through 5.~~ ✅ **Done.** `core/formats/ldraw`
  preserves meta commands, subfile filenames, and geometry records for #210;
  ~~referenced-file resolution~~ ✅ **Done.** `LDrawLibraryResolver` resolves
  and caches subfiles from model-relative and library roots for #210;
  ~~LDConfig color records, current/edge color inheritance, direct RGB color
  codes, and material approximation.~~ ✅ **Done.** `LDrawColorTable` parses
  color definitions and maps them to renderer materials for #210.
  ~~Inline type 3/4 polygon conversion to renderable mesh geometry.~~ ✅
  **Done.** `LDrawGeometryCompiler` builds lit `MeshPrimitive` geometry with
  command-color material assignment for #210.
  ~~Referenced-file resolution for type-1 subfile references with transform and
  color inheritance.~~ ✅ **Done.** `LDrawGeometryCompiler` resolves cached
  subfiles through `LDrawFileResolver` and instances them with LDraw affine
  transforms for #210. ~~MPD scene assembly with `0 FILE` / `0 NOFILE`
  virtual submodels.~~ ✅ **Done.** MPD-local files are parsed as named blocks
  and resolved before external library roots for #210.
  ~~BFC winding, clipping sidedness, and inverted subfile references.~~ ✅
  **Done.** `LDrawGeometryCompiler` tracks BFC certification, winding, clipping,
  and `INVERTNEXT` state so imported mesh winding, normals, and raster material
  sidedness follow certified LDraw geometry for #210.
  ~~Compiled part reuse for repeated references.~~ ✅ **Done.**
  `LDrawGeometryCompiler` caches compiled subfile primitives by resolved file
  identity, inherited color context, and winding inversion, then wraps repeated
  references in `render::Instance`; repeated-brick fixture tests pin cache hits,
  shared geometry, bounds, and inherited-material behavior for #210.
  ~~Structured import diagnostics for missing parts, unsupported commands,
  skipped geometry, color fallbacks, BFC treatment, and fatal parse failures.~~
  ✅ **Done.** `LDrawDiagnostics` gives parser/resolver/compiler paths
  machine-checkable warnings and errors for #210.
  ~~Type-2 edge/detail line overlay import.~~ ✅ **Done.**
  `LDrawGeometryCompiler` carries edge lines as zero-width curve-overlay
  segments, including color 24 resolution from the active part edge color;
  type-5 conditional-line visibility remains a deferred importer follow-up for
  #210.
  ~~World scene and rendercli integration.~~ ✅ **Done.** `Collection`
  authoring metadata references LDraw source files; rendercli resolves those
  references through the import pipeline into ordinary primitive geometry and
  accepts scene-level library roots plus direct LDraw input for #210.
  ~~Preserve `0 STEP` and MPD authoring structure as scene groups.~~ ✅
  **Done.** `LDrawSceneImporter` maps build steps and submodel references onto
  generic `Group` metadata nodes while keeping flattened import available for
  #210.
  ~~Fixture, render-smoke, and documentation surface.~~ ✅ **Done.** A tiny
  checked-in MPD plus mini-library fixtures cover inline geometry, library part
  resolution, nested subfiles, inherited colors, BFC, and MPD; rendercli CTest
  renders the fixture without the official parts library, and the textbook
  documents library-path configuration plus supported limitations for #210.
  ~~Explicit render-affecting LDraw import options.~~ ✅ **Done.** World scene
  metadata and direct `rendercli --ldraw_input` expose library roots, import
  scale, coordinate conversion, hierarchy preservation, normal mode, edge
  overlays, recursion limits, and missing-part policy for #210.
- **PDB / PDBx/mmCIF molecular coordinates** — ~~parse atom-site records into
  atoms, residues, chains, and models; preserve imported molecule hierarchy as
  scene groups; generate protein CA-trace backbone curves as overlay, ribbon,
  or tube representations; add tiny PDB/mmCIF fixtures, render smoke coverage,
  and documentation for supported records, representations, and limitations.~~
  ✅ **Done.** `MoleculeParser` and
  `MoleculeSceneImporter` load molecular coordinate files into grouped atom
  spheres, inferred or explicit bond cylinders, and per-chain CA backbone curves
  with residue metadata, backed by `test/fixtures/molecules`, rendercli smoke
  tests, and the molecule import textbook page for Epic #236 and Epic #408;
  molecule SourceAsset docs and smoke coverage now pin default styled atoms plus
  bond generation for Epic #408; full secondary-structure ribbons remain TODO.
  ~~Shared sidecar asset resolver for importer search paths and cache keys.~~ ✅
  **Done.** `core::AssetResolver` resolves current-file-relative and configured
  search-root assets with explicit case-sensitivity behavior for Epic #230.
  ~~Shared scene-importer interface.~~ ✅ **Done.** `world::SceneImporter`,
  `ImportOptions`, `ImportResult`, and `ImportDiagnostic` define the
  format-neutral importer contract for Epic #230; wiring concrete LDraw import
  into that contract remains TODO. ~~Shared imported mesh ownership and
  runtime primitive support.~~ ✅ **Done.** `core::MeshAsset` and
  `render::MeshPrimitive` provide shared mesh lifetime, per-face material
  assignment, Grid/BVH traversal, and raster/wireframe compatibility for Epic
  #230.
  ~~Shared import provenance metadata.~~ ✅ **Done.** `Element` metadata and
  `ImportProvenance` helpers let importers attach source file/entity,
  line-range or record, original-units, and category provenance without custom
  subclasses for Epic #230.
  ~~Shared importer fixture harness and documentation.~~ ✅ **Done.**
  `ImporterTestHelper`, `test/fixtures/importers`, and the importer lifecycle
  textbook chapter cover diagnostics assertions, group-tree expectations,
  sidecar asset fixture layout, options, provenance, and render smoke patterns
  for Epic #230.
- **Native scene format: JSON** (see R3) — round-trip with full fidelity. JSON chosen over YAML because it's more tooling-friendly, parses without ambiguity, and works natively in the §4.1 WebGL viewer without a YAML→JSON conversion step. The R3 format is the authoritative scene description; all other loaders convert into it on read and out of it on write.

### 4.6 Modeling UI

Scope = "Blender-lite." Qt is already the toolkit. The UI's modality structure mirrors Blender's: Object mode → Edit mode → Sculpt mode → UV mode, with a unified timeline at the bottom and a chat side panel at the right.

#### 4.6.a Core editor

- Multi-viewport (top / front / side / perspective / final camera). Each viewport picks its own engine — perspective uses GL, orthographic views use wireframe, and one view can follow the active render camera for final-output preview.
- Object hierarchy / outliner with selection, naming, grouping, parenting.
- Property editor for the selected object's transform, material, primitive parameters.
- Move / rotate / scale gizmos with snapping (unit, grid, surface, increment).
- Primitive creation toolbar; CSG operation buttons.
- Material editor with library browser, parameter sliders, live preview.
- Undo/redo (depends on R2 stable IDs + R3 diffable serialization).

#### 4.6.b Modifier stack

A non-destructive modifier stack on each object (Blender-style: apply all, apply one, collapse to mesh):

- Transform modifiers: mirror, array, curve deform, lattice.
- Mesh modifiers: subdivision (Catmull-Clark), smooth, decimate, solidify, weld, triangulate.
- Generation modifiers: screw, skin, wireframe.
- Deform modifiers: displace (texture-driven), wave, simple deform (bend/taper/twist/stretch), cast.
- Physics modifiers (placeholder slots for when §4.10 physics lands).

The modifier stack is serialized as a list of operations in the scene JSON, and the AI agent can read and rewrite it.

#### 4.6.c Node graph editor

A node-based editor with two modes, sharing the same UI widget:

- **Geometry nodes** — procedural mesh generation and modification via a composable DAG. Each node maps to a function in the §4.2.c geometry library.
- **Material/shader nodes** — wire-up of the §4.3 texture/BSDF layers. This is the primary authoring surface for complex materials; not the same as a full OSL shading language, but structurally similar.

Both modes compile down to the same scene JSON; the node graph is just a visual editing tool, not a runtime VM.

#### 4.6.d Sculpt mode

- Multi-resolution sculpting on `Mesh` objects (Catmull-Clark subdivide + sculpt at each level).
- Brush toolkit: draw, smooth, flatten, crease, inflate, pinch, snake hook, grab, clay, clay strips.
- Dynamic topology (Dyntopo): real-time remesh under the brush for unbounded detail.
- Masking, face sets, and visibility for isolating sculpt regions.
- Normal map bake-down from high-poly sculpt to low-poly via ray-cast.

#### 4.6.e UV editor

- UV unwrapping: LSCM, Angle-Based Flattening (ABF++), Project-from-view, Smart UV Project.
- UV packing (bin-packing of UV islands into the [0,1]² square).
- Seam paint and pinning.
- Texture paint in UV space (direct 2D painting that updates the §4.3 image texture).
- 3D texture paint (project-paint directly on the surface, updates the UV-mapped texture).

#### 4.6.f Retopology tools

- RetopoFlow-style surface projection: draw new quad topology snapped to a high-poly surface.
- Shrinkwrap modifier (project any mesh to the surface of another).
- Quadriflow / Instant Meshes-style automatic remesh (produces clean quad topology for rigging or export).

#### 4.6.g Grease pencil

A 2D draw-in-3D tool: strokes live at world positions, can be animated on the timeline, rendered as flat colour overlays or as actual volumetric ribbons by a dedicated stroke-renderer backend. Useful for annotation, storyboarding, and stylised animation in the same file as the 3D scene.

#### 4.6.h Asset library

- A project-local and user-global asset library (materials, meshes, node groups, modifier presets, HDRI environments).
- Thumbnail auto-generation for all asset types.
- Searchable, tag-filterable catalog; drag-and-drop into the scene.
- Naming scheme compatible with the AI agent's tool calls (`metal/copper/polished` etc.).
- Community/shared library import is out of scope for now, but the storage format (JSON + loose files, zip-packaged as `.rcxpkg`) should be open enough that it's possible later.

#### 4.6.i AI-native side panel

A chat interface in the editor with an LLM agent that has tool calls into the scene graph. Tool surface includes:

- `add_primitive(type, position, params) → id`
- `transform(id, matrix)`
- `apply_material(id, name_or_inline_definition)`
- `csg_union(a, b) → id`, `csg_intersect(a, b) → id`, `csg_difference(a, b) → id`
- `import_file(path) → id`
- `query_scene() → JSON`
- `select(id)`, `delete(id)`
- `set_camera(position, target, fov)`
- `set_environment(hdri_path)`
- `run_script(code) → id_list` — evaluate a parametric script and drop the result into the scene.
- `get_modifier_stack(id) → JSON`, `set_modifier_stack(id, JSON)`

The agent observes user edits via the same scene graph, so the loop is bidirectional. The chat transcript is persisted in the scene file so AI-assisted sessions can be resumed.

Implementation notes:

- Anthropic's Claude with tool use is the default; the green-acres infrastructure has Claude API auth already.
- Tool calls go through R3 serialization, so they can be undone, replayed, and audited.

#### 4.6.j Scripted parametric objects

A scene-script DSL for parametric/procedural geometry. See §7 open question on scripting language. Scripts produce primitives that drop into the scene graph as regular objects. The AI agent can write scripts for parametric parts ("a 24-tooth involute gear", "a 30-step spiral staircase").

### 4.7 Animation & timeline

- Scene timeline with keyframes on any animatable parameter — transforms, material parameters, camera pose, light intensity, scene-level globals (time of day, weather).
- ✅ **Timeline math foundation.** Shared Qt-free timeline/keyframe primitives now live under `include/core/animation/`, with reusable interpolation policies under `include/core/math/interpolation/`.
- ✅ **World scene timeline loading and evaluation.** Editable scenes now preserve a top-level JSON `animation` block and evaluate id-targeted direct `Q_PROPERTY` tracks through `world::Timeline` / `world::AnimationTrack`.
- ✅ **`rendercli --frame`.** The command-line renderer now evaluates an optional animation frame before runtime scene conversion, with a checked-in animation-frame demo scene and CTest smoke coverage.
- ✅ **`rendercli --animation`.** The command-line renderer now emits timeline image sequences from a printf-style output pattern, with range/fps overrides, progress output, and CTest smoke coverage.
- ✅ **Modeler timeline preview.** Animated scene files now enable a read-only Timeline dock; the frame control evaluates copied scenes for preview/final renders while property editing stays on the base authoring scene.
- Interpolation curves: linear, Bezier, ease-in/out, hold.
- ✅ **Time-sampled rendering for motion blur** (multiple time samples per frame within shutter-open). First pass landed in 7c81d11 — `State::timeSample` drawn from `SampleStream::next1D` (dim 1 in the renderer's stream allocation), `world::Surface::velocity` Q_PROPERTY, `Instance` interpolates linear translation. Rotation/scale animation, full timeline, and keyframe interpolation curves still TODO.
- Output: image sequence or piped to ffmpeg for video (configurable codec).
- Eventually (defer): rigid body simulation, particle systems, simple IK skeletons.

Prerequisite: time dimension on the scene graph — every transform and parameter needs to be evaluable at a time `t`.

### 4.7.b Video-editor sequencer (NLE layer)

Once individual animated scenes render, they need to be assembled. A non-linear editing layer sits above the renderer and composes rendered scenes (and imported video/image clips) into a final output:

- **Clip timeline.** Each clip is a rendered image sequence, a live scene, or an imported video. Clips sit on tracks; tracks compose vertically with blend modes.
- **Transitions.** Cut, dissolve, wipe (clock, linear, radial, box), cross-zoom, RGB split. The transition framework is a shader pipeline over two framebuffers (A and B at blend factor `t`) so custom transitions are just GLSL/WebGL fragments. Collect *all* the classic transitions from the broadcast and post-production repertoire; the "all variants" rule applies.
- **Titles and slates.** Text overlay with font, size, colour, position, animation (slide-in, fade, typewriter, etc.). SVG import for logo stings.
- **Audio tracks.** Import audio (WAV, MP3, AAC); sync to timeline; basic mix (gain, fade, trim). No DSP — that's a DAW's job. Just enough to assemble a watchable render reel.
- **Speed ramp / time remapping.** Per-clip speed curves (linear retiming, ease-in/ease-out slow-motion).
- **Colour grading per clip** — feeds the §4.9 postprocessing stack, but overridable per clip.
- **Export.** Pipe to ffmpeg; configurable codec (H.264, H.265, ProRes, lossless), container (MP4, MOV, MKV), and resolution/bitrate. Still-frame export at any frame.

The sequencer stores its state in the same project JSON as the 3D scenes — one file, the full production.

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

### 4.9 Postprocessing & compositing

Every frame — from any engine — passes through a configurable postprocessing stack before display or export. This is both a production-quality feature and an educational playground: postprocessing is where you learn what the beauty pass is *missing*.

#### 4.9.a Tone-mapping & colour science

The tonemap operator catalog. The `Tonemap::apply(hdr) → ldr` interface from R1 carries the stateless single-pixel ones; operators that need a buffer pre-pass (Drago, auto-exposure) require lifting the API to a two-phase analyse-then-apply flow — flagged as the architectural change they need.

Stateless single-pixel operators (current `Tonemap::apply` interface):

- ✅ **Linear** — pass-through; LDR clamp via `Colord::rgb()`. Default and regression baseline.
- ✅ **Reinhard** — `c / (1 + c)` per channel.
- ✅ **ACES** (Narkowicz polynomial fit). Per-channel; carries the well-known chromatic skew (saturated reds shift to orange under heavy compression).
- **AgX** (Troy Sobotka, 2021) — the modern replacement for ACES; default in Blender 4.0+. Operates in log space then through a sigmoid; fixes ACES's per-channel skew. Most-impactful next add — both pedagogically (clear ACES contrast) and aesthetically.
- **Hable / Uncharted 2** (John Hable, 2010) — game-industry classic, polynomial-fit shoulder/toe with explicit white-point control. Simpler than AgX; nice "what game engines used pre-ACES" data point.
- **Khronos PBR Neutral** (2024) — designed to preserve PBR material colours across viewers. Important for any future glTF / asset-library work.
- **Extended Reinhard** — `c · (1 + c/Cwhite²) / (1 + c)` — Reinhard with a `whitePoint` parameter. Trivial change; demonstrates parameterised operators.
- **Filmic (Blender)** — Sobotka's pre-AgX work, wider-gamut filmic curve. Historical interest.
- **AP0/AP1 ACES variants** — full-pipeline ACES with proper colour-space matrices (the matrices that the Narkowicz fit skips). Educational completeness.

Operators that need a per-render pre-pass (would lift the API to `Tonemap::analyze(buffer); Tonemap::apply(pixel)`):

- **Drago / adaptive logarithmic** (2003) — log-base picked from scene maximum luminance.
- **Auto-exposure / Reinhard with key-value adaptation** — average / median luminance pre-pass to set a reference exposure.
- **Mantiuk** (2008) — perceptually motivated; needs the buffer.

Adjacent colour-science features:

- Exposure, white balance (colour temperature + tint), contrast, lift/gamma/gain.
- LUT (Look-Up Table) input in `.cube` format for arbitrary colour grade.
- **Tony McMapface** (Tomasz Stachowiak) and other LUT-based operators — falls out of the .cube LUT support above.

#### 4.9.b Image-space effects

All implemented as full-screen fragment shaders (or compute passes), over the float framebuffer from R1:

- Bloom (Gaussian, physically-based high-pass threshold + bilateral; also: glare/star filter for light sources).
- Depth-of-field (post-process CoC approximation from depth AOV; separable hexagonal/circular blur; bokeh shape texture).
- Motion blur (per-pixel accumulation from motion-vector AOV; fast approximate and ground-truth compare).
- Screen-space ambient occlusion (SSAO, HBAO, GTAO — implement all three for comparison).
- Chromatic aberration (radial RGB channel offset).
- Vignetting (gain falloff toward corners, optionally with lens-profile data).
- Grain / film noise (analytic or sampled from a real film profile).
- Lens flares and light shafts (screen-space, not ray-traced).
- Halation (highlight fringing on bright edges — film characteristic).
- Sharpen / unsharp mask, diffusion filter.
- Pixel art / dither downscale (for stylised output).

#### 4.9.c AOV-based compositing

Using the §4.3.d AOV outputs and any intermediate render-graph resources that a pass chooses to publish:

- Per-pass multiply/add compositing (diffuse+specular+emission+indirect = beauty, manually verifiable).
- Object/material-id mask extraction for selective colour grading or motion-blur exclusion.
- Cryptomatte-based selection (anti-aliased ID masks from the Cryptomatte AOV).
- Z-composite for inserting CG into photo backgrounds.
- Render-graph composites for hybrid frames: raster base + raytraced shadow mask, offscreen cartoon scene inside a screen material, planar reflection target masked by stencil, wireframe diagnostics over arbitrary passes.
- Denoising: OIDN (Intel Open Image Denoise) as the drop-in; optionally OptiX denoiser for NVIDIA hardware.

#### 4.9.d Compositing node graph

A node-based compositor (Nuke/Blender Compositor-style) that wires AOV inputs, render-target resources, colour-science nodes, effect nodes, and mask nodes into a final output. The same node-graph widget from §4.6.c is reused with a different node palette. This is the natural endpoint for all of §4.9, while §4.1.a's render-pass graph is the lower-level execution plan that produces the resources the compositor consumes. Not a priority for v1, but the architecture should accommodate it from the start.

### 4.11 Image processing & computer vision

The raytracer produces images; computer vision *consumes* them. This pillar collects the classical (non-ML, or barely-ML) image processing and computer vision algorithms — same "all variants for the textbook value" principle as §4.2.c (computational geometry).

Concrete near-term consumers in this codebase: shape-classification helpers for the functional test suite (replacing the broken `ShapeRecognition` heuristic — see §4.11.d), reference-image regression in §3.4.a item D (PSNR / SSIM diff with tolerance — see §4.11.h), and path-tracer denoising once §4.1 path tracing lands (NLM / bilateral — see §4.11.i). Long-term: every camera + tonemap + postprocess change is a CV pipeline in disguise.

The goal isn't to ship a CV library that competes with OpenCV. It's to ship a self-contained, beautifully-documented teaching collection — like the comp-geo library — where each algorithm is implemented from scratch, comes with an interactive Doxygen widget, and is benchmarkable side-by-side against its siblings.

**Where it lives.** The current `Blob` / `Silhouette` / `ShapeClassifier` primitives sit under `test/helpers/` because their first consumer is the functional test suite. The library should graduate to a first-class location (`include/cv/` + `src/cv/`) once a *non-test* consumer materialises — the renderer using silhouette extraction for NPR engines, edge-aware denoising in the path tracer, or feature detection / matching for any future image-based workflow. Educational documentation (per-algorithm Doxygen pages with the math, interactive JS widgets, side-by-side comparison renders) lands at the same time as the graduation, not before — keeping it in `test/helpers/` is the lazy correct default until the renderer needs it.

#### 4.11.a Pixel and colour foundations

- **Colour space conversions** — sRGB ⇄ linear, RGB ⇄ HSL/HSV/HCL, RGB ⇄ XYZ ⇄ LAB ⇄ LCH, RGB ⇄ YUV/YCbCr (used in JPEG / video codecs), RGB ⇄ CMYK. Each conversion is ~10 lines and worth a Doxygen page on the colour theory.
- **Histograms** — single-channel, multi-channel (RGB), 2D joint histograms (HS plane). Computation, visualisation as a JS widget, plot-style rendering for the docs.
- **Histogram equalization** — global, AHE (Adaptive HE), CLAHE (Contrast-Limited AHE).
- **Gamma correction, white balance, exposure compensation** — primitive operations on raster buffers.
- **Tone mapping ↔ this pillar** — the existing `Tonemap` family (§3.R1) IS image processing; this is the natural home for new operators (Drago, Mantiuk, Reinhard global vs local, Filmic). Cross-listed in §4.9.a.

#### 4.11.b Filtering, convolution, morphology

- **Linear filters** — Box blur (separable), Gaussian (separable), Sobel / Scharr / Prewitt gradients, Laplacian, generic NxN convolution kernel.
- **Non-linear filters** — Median (rank), bilateral (edge-preserving smoothing), joint bilateral, NLM (non-local means — denoising classic). Anisotropic diffusion (Perona-Malik) for the textbook value.
- **Morphology** — Erosion, dilation, opening, closing, hit-or-miss, top-hat, black-hat. Greyscale and binary variants. Useful for blob cleanup before classification.
- **Frequency-domain filtering** — FFT, ideal / Butterworth / Gaussian band-pass, deconvolution. Pulls in an FFT impl (Cooley-Tukey, Bluestein for non-power-of-two sizes).

#### 4.11.c Edge detection

- **Sobel / Scharr / Prewitt** — first-order gradient operators.
- **Laplacian / Marr-Hildreth** — second-order; zero-crossing detection.
- **Difference of Gaussians (DoG)** — fast Marr-Hildreth approximation; foundational for SIFT.
- **Canny** — full pipeline (Gaussian blur → Sobel gradient → non-max suppression → double threshold → hysteresis edge linking). The pedagogical centrepiece.
- **Structured edges, edge boxes** — modern variants, lighter-weight than Canny in some applications.

#### 4.11.d Connected components & blob analysis *(immediate consumer: tests)*

- **Connected components labelling** — BFS flood fill (simple), Hoshen-Kopelman / two-pass with union-find (fast), Spaghetti / BBDT (state of the art). Each variant gets a benchmark; `ShapeClassifier` (the `ShapeRecognition` successor) starts on BFS and can swap in faster variants without API changes.
- **Geometric blob descriptors** — area, perimeter, centroid, bounding box, oriented bounding box, convex hull (links to §4.2.c), solidity (`area / hull_area`), extent (`area / bbox_area`), aspect ratio, equivalent diameter, eccentricity, **circularity** (Polsby-Popper: `4π·area/perimeter²`, the core metric the test classifier uses).
- **Radial / angular distribution** — std-dev of boundary-point distance from centroid, normalised by mean. The metric that distinguishes circles from polygons regardless of fill. Histogram-of-radii for unimodal/multimodal classification.
- **Hu moments** (1962) — seven translation/scale/rotation-invariant central moments. Each shape gets a 7-dimensional signature; classification by nearest neighbour in moment space. Classic shape-classification paper, ~50 LOC.
- **Zernike moments** — orthogonal-polynomial moments; rotation-invariant by construction. Better numerical stability than Hu's at high orders.
- **Fourier descriptors** — boundary parameterised in the complex plane; Fourier coefficients become rotation/translation invariants. Truncated descriptors give a low-pass shape signature.
- **Chain codes** (Freeman 1961) — boundary as a sequence of 4- or 8-direction codes. Compression / matching-via-cyclic-shift.
- **Polygonal approximation** — Douglas-Peucker, Visvalingam-Whyatt. Reduce a boundary to a few corners. Already on the §4.2.c list; cross-referenced here for shape-recognition use.

#### 4.11.e Hough transforms & robust fitting

- **Standard Hough Lines** — vote-based line detection in (ρ, θ) parameter space. The diagrammable accumulator is begging for a JS widget.
- **Probabilistic Hough Lines** — random subset sampling; faster.
- **Hough Circle Transform** — three-parameter (cx, cy, r) variant; the natural pairing with the shape classifier.
- **Generalised Hough Transform** (Ballard 1981) — vote for arbitrary shapes via R-table lookup.
- **RANSAC** — RANdom SAmple Consensus. Iterative outlier-rejecting line / circle / plane fit. Foundational for everything below.
- **MSAC, MLESAC, PROSAC** — RANSAC variants. Educational comparison.
- **Total Least Squares, Levenberg-Marquardt** — optimisation tools that fitting routines reach for.

#### 4.11.f Feature detection & description

This is the corner-and-keypoint zoo. Classical, all pre-deep-learning, all elegant.

- **Corner detectors** — Harris (eigenvalues of structure tensor), Shi-Tomasi (Good Features to Track), FAST (Features from Accelerated Segment Test — the speed champion), SUSAN (Smallest Univalue Segment Assimilating Nucleus).
- **Blob detectors** — DoG (Difference of Gaussians), LoG (Laplacian of Gaussian), MSER (Maximally Stable Extremal Regions).
- **Descriptors** — SIFT (Scale-Invariant Feature Transform; the towering classic), SURF (faster SIFT cousin), ORB (Oriented FAST + Rotated BRIEF — the modern free-to-use default), BRIEF, BRISK, FREAK.
- **Holistic descriptors** — HOG (Histogram of Oriented Gradients — the pedestrian-detection classic), LBP (Local Binary Patterns — texture).

Each comes with the standard "find features in image A, match against image B, draw correspondences" demo as an interactive widget.

#### 4.11.g Segmentation

- **Thresholding** — global (Otsu's method, the maximum between-class variance classic), Niblack, Sauvola, adaptive thresholding.
- **Clustering-based** — K-means in colour space, mean-shift segmentation.
- **Region growing** — seed-and-expand, split-and-merge.
- **Watershed** (Vincent-Soille 1991) — topographic-flooding segmentation.
- **Graph cuts** — max-flow / min-cut on a pixel graph (Boykov-Kolmogorov). Implementation paired with the max-flow algorithm itself.
- **GrabCut** — iterated graph cuts with Gaussian Mixture Models. The classic "draw a rectangle around the object" interactive segmenter.
- **SLIC superpixels** — Simple Linear Iterative Clustering. Local k-means in 5D (LAB + xy).
- **Active contours** (snakes — Kass-Witkin-Terzopoulos 1988), level-set methods (Chan-Vese).

#### 4.11.h Image quality metrics *(immediate consumer: §3.4.a item D)*

- **Pixel metrics** — MSE, MAE, RMSE, PSNR.
- **Structural metrics** — SSIM (Structural SIMilarity index — the classic), MS-SSIM (multi-scale), CW-SSIM (complex-wavelet, robust to small geometric distortions).
- **Perceptual metrics** — Δ E (CIEDE2000), VMAF (Video Multi-method Assessment Fusion). LPIPS is the deep-learning sibling — included for completeness, optional dep.
- **No-reference quality** — BRISQUE, NIQE, PIQE — quality scores from a single image, no reference needed.

The §3.4.a reference-image-diff regression suite uses SSIM with a tolerance band; failures emit the diff visualisation alongside the diagnostic.

#### 4.11.i Restoration & denoising *(immediate consumer: future path tracer)*

- **NLM (Non-Local Means)** — the modern path-tracer denoising default. Bilateral filter generalisation that finds similar patches anywhere in the image.
- **Bilateral filter** — joint domain-and-range Gaussian. The lightweight denoiser.
- **Total Variation denoising** (Rudin-Osher-Fatemi 1992) — convex optimisation; preserves edges.
- **Wiener filter** — minimum-MSE optimal linear filter; the textbook deconvolution.
- **BM3D** (Block-Matching 3D) — the SOTA classical denoiser; collaborative filtering of similar patches in transform domain.
- **Inpainting** — Bertalmio diffusion-based, exemplar-based (Criminisi), patch-match (Barnes 2009 — the famous Photoshop Content-Aware Fill engine).
- **Deblurring** — Wiener, Richardson-Lucy, blind deconvolution.

The path tracer, once it lands, will need at least bilateral or NLM as the post-render cleanup pass; this list is pre-built infrastructure for that.

#### 4.11.j Geometric transformations

- **Affine, projective transforms** — closed-form forward and inverse mapping. The math underlies §4.2.c instancing too.
- **Image warping** — forward warping (splatting), inverse warping (with bilinear / bicubic / Lanczos resampling).
- **Image registration** — feature-based (RANSAC + descriptor matching from §4.11.f) and intensity-based (Lucas-Kanade).
- **Homography estimation** — Direct Linear Transform (DLT), normalised DLT, robust estimation under RANSAC. The "rectify a planar object from an oblique view" demo.
- **Rectification** — for stereo image pairs, line up scanlines.
- **Lens distortion correction** — radial (Brown-Conrady), tangential. Inverse mapping with iterative Newton refinement.

#### 4.11.k Compression primitives

- **Lossless** — RLE (Run-Length Encoding), Huffman coding, arithmetic coding, LZW, LZ77 / LZ78, ANS (Asymmetric Numeral Systems — the modern entropy coder behind Zstd).
- **DCT** (Discrete Cosine Transform) — JPEG core. Block-wise, with a JS widget showing block reconstruction at varying quantisation levels.
- **DWT** (Discrete Wavelet Transform) — JPEG2000 core. Educational alternative to DCT.
- **Quantisation matrices** — JPEG luma/chroma quant tables; impact on artefacts.
- **Chroma subsampling** — 4:4:4, 4:2:2, 4:2:0 — and the visible-vs-invisible artefacts of each.

The endgame is a from-scratch JPEG encoder/decoder — JPEG-as-textbook is a classic CG-curriculum project.

#### 4.11.l Multi-view geometry & video *(long tail)*

- **Stereo matching** — block matching, semi-global matching (Hirschmüller), graph cuts. Disparity → depth.
- **Optical flow** — Lucas-Kanade (sparse), Horn-Schunck (dense, variational), Farnebäck polynomial expansion (dense, fast).
- **Structure from Motion (SfM)** — incremental and global pipelines. Bundle adjustment via Levenberg-Marquardt.
- **Visual odometry, SLAM primitives** — keyframe selection, loop closure detection, pose graph optimisation.
- **Video stabilisation** — feature-tracked + motion-model fitting + smoothed-trajectory warping.
- **Tracking** — Kalman filter, particle filter, KLT (Kanade-Lucas-Tomasi), CSRT, mean-shift tracking.

These are far-future and depend on §4.7 animation infrastructure for any video aspect. Listed for completeness — every one is an interactive Doxygen widget waiting to be written.

#### 4.11.m Pattern recognition (classical)

The pre-deep-learning side of the field. Most are simple to implement and pair beautifully with the §4.11.f feature extractors.

- **k-Nearest Neighbours**, **Naive Bayes**, **Decision trees** (ID3, C4.5).
- **Linear classifiers** — perceptron, logistic regression, Fisher's Linear Discriminant Analysis.
- **Support Vector Machines** — linear, kernel-trick (RBF, polynomial). The optimisation (SMO — Sequential Minimal Optimisation) is the educationally interesting bit.
- **Eigenfaces / Eigenobjects** — PCA on aligned image patches; the classic face-recognition demo from the 90s.
- **Bag-of-Visual-Words** — quantise SIFT descriptors via k-means, treat images as histograms over the codebook, classify with k-NN or SVM.

LLMs and CNNs are explicitly *out* of this pillar. They go in §4.10 (long tail) if at all — the goal here is the classical canon, before deep learning ate the field.



Features that belong in a complete CG curriculum but sit at the end of the roadmap because they depend on most everything above:

- **Custom physics engine.** A separate, self-contained simulation library (rigid body, soft body, cloth, fluid SPH, smoke/fire) that plugs into the animation timeline via the same keyframe interface. Not a dependency on Bullet or Havok — the point is to implement it from scratch for the learning value. Rendering-side natural phenomena (water, smoke, fog, clouds, fire, precipitation, dust, lava) are tracked in `topics-backlog.md`; simulated fluids feed those renderers once §4.7 animation infrastructure is solid.
- **NeRF / Neural Radiance Fields.** Volume rendering from a trained MLP over (direction, position) → (colour, density). Academic, slow, and fascinating. Implemented as a `RenderEngine` subclass that takes a pre-trained `.nerf` or `.ingp` checkpoint. CUDA or libtorch dependency; treat as optional.
- **3D Gaussian Splatting.** The faster, more practical sibling of NeRF. Splatted onto the rasterizer backend. Same dependency constraints as NeRF.
- **Photon mapping** (progressive, stored-photon, volumetric). Historically important; teaches caustics and subsurface light transport in a way path tracing alone doesn't.
- **Bidirectional path tracing (BDPT) and Metropolis Light Transport (MLT).** Deep in the weeds; tackle after vanilla PT is proven. The educational payoff is high (understanding why PT struggles with caustics, and how BDPT/MLT fix it), so they belong in the roadmap rather than the non-goals list.
- **Polarization-aware spectral rendering.** The ultimate extension of §4.3.a's spectral mode.
- **Deep image compositing.** EXR deep data (multiple samples per pixel with per-sample depth and coverage) for complex transparency compositing. Niche, but completes the EXR story.
- **Light field rendering / plenoptic cameras.** Natural extension of §4.4.a's light-field camera.
- **Procedural animation (rigs, IK, blend shapes).** Armatures, inverse kinematics solvers (CCD, FABRIK, Jacobian), blend shape / shape-key morphing. Separate from the physics simulation; this is character animation.

Each of these is worth a deep-dive document (§4.0 invariant applies). None of them should block anything in §§4.1–4.9.

## 5. Non-goals (explicit)

This list is intentionally short, because the scope is intentionally large. The two things this project will not become:

- **A real-time game engine.** The OpenGL and WebGL viewports are for editing and previewing, not for shipping interactive, playable experiences. Frame rate matters but never at the expense of correctness.
- **A CAD / engineering tool.** The scripted DSL and geometry library are for *artistic* parametric content. Dimensional tolerance, constraint solvers, export to STEP/IGES, and anything that needs to be machined are out of scope. Use FreeCAD.

Everything else that appeared in previous versions of this list — sculpting, BDPT/MLT, node graphs, UV unwrapping, production render quality — has been moved *into* the roadmap proper (§§4.2–4.10), because the educational scope makes them worth implementing.

---

## 6. Themes (not milestones, not deadlines)

There are no deadlines on this project. Work proceeds by *theme* — a theme is a cluster of related features that can be tackled in any order and can stall without blocking other themes. The prerequisite chain (§3) is the only hard ordering; everything else can interleave.

Pick the theme that's most interesting today; stop when it stops being interesting; pick the next one. The "all variants" principle means themes never fully close — there is always another algorithm to implement and document.

### T1. Foundations *(prerequisite — do first)*

The eight refactors from §3 (R0–R7). Gatekeeping nothing else runs well without these. R0 (behavioural tests on the integrator and materials) precedes everything. ~~Plus in-flight fixes: PRs #35/#36~~ ✅ #35 (default depth + truncation) and #36 (TIR direct lighting) are merged. ~~and~~ The small tidy-ups (`PerfectTransmitter` IOR default, `PerfectSpecular` normalDotIn typo, missing `setTransmissionCoefficient` in textured ctor) are still TODO.

### T2. More engines

Wireframe → Software rasterizer → render-pass graph → OpenGL viewport → Path tracer → WebGL preview. Each engine starts self-contained once R4 (tessellate) and R5 (`RenderEngine` abstraction) land, then graduates into pass executors behind the §4.1.a `RenderPlan`. Suggested order: wireframe (cheapest), software raster (most educational about the pipeline and already in progress), render-pass graph (the composability bridge), OpenGL (most immediately useful for the UI once pass resources exist), path tracer (the pedagogical centrepiece), WebGL (most shareable). GPU backends (Vulkan/OptiX/Metal) are their own long-tail item.

### T3. Better shading

The full §4.3 progression: shading-normal pipeline → GGX/Cook-Torrance → all BSDF variants → procedural + scripted textures → four-layer texture stack → AOV outputs → NPR shaders → spectral rendering. Feeds every other theme with better visuals immediately.

### T4. Geometry completionism

§4.2 end-to-end: new primitives → BVH + alternate acceleration structures → computational geometry library algorithms (convex hull, triangulation, Voronoi, Booleans, …). Benchmarks comparing acceleration structures are a natural output of this theme.

### T5. Cameras & lights completionism

§4.4 end-to-end: stereo cameras → thin-lens DoF → Kolb realistic → Panini/panoramic → light-field; area lights → HDRI → sun-sky → IES. Each camera/light variant gets a side-by-side comparison render.

### T6. File I/O

OBJ read/write → STL → glTF 2.0 → EXR → FBX → USD. As each format lands, the scene JSON round-trip baseline expands.

### T7. Modeling UI

§4.6 progression: core editor shell → modifier stack → UV editor → sculpt mode → node graph → retopology → grease pencil → asset library. Also the AI chat side panel and scripted parametric objects.

### T8. Animation & sequencer

Timeline + keyframes on the scene graph → motion blur → image/video output → NLE sequencer → transitions → titles/audio → speed ramp. Unlocks the distributed rendering farm (T10) for animation throughput.

### T9. Postprocessing & compositing

§4.9 end-to-end: tone mapping science → image-space effects → AOV compositing → compositor node graph. Satisfying to iterate on because every new effect is immediately visible on any scene.

### T11. Image processing & computer vision

§4.11 end-to-end: pixel/colour foundations → filtering & morphology → edge detection → connected-components & shape descriptors → Hough & RANSAC → feature detection → segmentation → image-quality metrics → restoration & denoising → geometric transforms → compression primitives → multi-view geometry → classical pattern recognition. Naturally interleaves with T9 (postprocessing) since both work in image space; powers tests (shape classifier, golden-image regression) and the future path tracer's denoising pass. Pre-deep-learning canon — beautifully self-contained algorithms with interactive widget demos.

### T10. Long tail & industry extras

§4.10: physics engine → photon mapping → BDPT/MLT → NeRF/3DGS → polarization-aware spectral → deep EXR → procedural rigs/IK. No ordering constraints among these; each is its own extended project.

---

## 7. Open questions

These need decisions before specific work starts. Calling them out so they don't ambush a PR mid-flight.

- **Scripted DSL: which language first?** Python is more popular but adds a Python embedding dependency. JavaScript via QuickJS is small and self-contained. An OpenSCAD-style language is the most opinionated and the most work. The same DSL choice drives §4.3.b layer 4 (scripted textures) and §4.6.j (parametric objects), so picking once unlocks both.
- **Material library distribution model.** Bundled in-tree, downloaded on first use, or referenced from a community repo (like Blender's asset library)?
- **UI undo/redo granularity.** Per-property change, per-tool action, or per-scene-mutation? Affects the serialization design.
- **AI agent: cloud LLM or local?** Cloud (Claude via API) is more capable today; local (llama.cpp, Ollama) is private and free at idle. Both are viable; the tool-call surface is the same. Probably default to cloud with a local-fallback knob.
- **GPU backend: Vulkan compute, OptiX, or Metal?** Vulkan is portable but verbose. OptiX is NVIDIA-only but mature. Metal is macOS-only. This is a far-future decision; flagged here so it doesn't get re-litigated every quarter.
- **Compatibility: maintain the existing C++ scene-construction API forever, or sunset it once UI/serialization land?** It still backs several tests and scene-construction helpers, but the Modeler and JSON scenes are now the primary interactive path.

---

## 8. Relationship to `modernize.md`

[`modernize.md`](modernize.md) is the engineering-hygiene roadmap: build system, CI, test framework, Qt 6 migration, supply-chain hardening. This document is the feature/architecture roadmap.

The two are largely independent. Recommended order:

- Modernization items §3.1 (CMake migration, done), §3.2 (CI, in progress), §3.5 (test framework upgrade) should land before any of the foundational refactors here, since they make the refactors safer to ship.
- Modernization §3.10 (Qt 6 migration) should ideally land before T7 (modeling UI) so the new UI code targets a supported toolkit from the start.
- Everything else can interleave freely.

---

## 9. How to use this document

This is a living roadmap. PRs that move items forward should reference the roadmap section they advance (e.g., `[roadmap §4.2]` in the PR title or body). Items completed get crossed off here. New items get added here before they get implemented.

The wider field — algorithms and topics that don't yet have a pillar but should eventually be implemented and documented — lives in [`topics-backlog.md`](topics-backlog.md). When a backlog item gets picked up, it either graduates into a roadmap section or gets checked off in the backlog.

Open questions in §7 should be resolved through discussion (issues, this doc, or just a chat session) before the affected work starts.

The "north star" in §1 is the scope ambition. The non-goals in §5 keep that ambition honest. The themes in §6 are clusters that can interleave freely; only the §3 prerequisite chain has a hard ordering. There are no deadlines — pick the theme that's most interesting today.

---

*End of roadmap. Open for iteration.*
