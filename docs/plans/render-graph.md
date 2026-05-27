# Render graph plan - May 2026

> **Scope:** introduce an engine-agnostic render graph that can mix existing
> CPU renderers in one frame, expose the graph for teaching/debugging, and keep
> the design open to future GPU-backed resources and executors.
>
> **Status:** living design proposal. This plan captures the intended shape
> before implementation issues are filed. The first implementation should stay
> CPU-only, but the resource and pass contracts must not assume CPU buffers are
> the only possible storage.
>
> **Roadmap link:** implements the architecture sketched in
> `docs/roadmap.md` section 4.1.a, "Render-pass graph and hybrid execution."

---

## Goals

The project should stop treating `Raytracer`, `Rasterizer`, and `Wireframe` as
mutually exclusive whole-frame endpoints. A frame may need a raytraced room, a
rasterized computer-screen image inside the room, a wireframe diagnostic overlay,
and a postprocess or tonemap step at the end.

The render graph should provide:

- a compiled per-frame DAG of passes and typed resources;
- automatic expansion of scene features into backend-specific pass sequences;
- explicit user render intent for hybrid and diagnostic renders;
- graph inspection, export, and node enable/disable controls for education;
- a scheduling contract that can later run independent passes in parallel;
- CPU resource storage in the first version, with GPU storage as a future
  resource domain rather than a redesign.

The first version does not need to be a full production renderer. It should be
small enough to implement interactively, but strict enough that future parallel
scheduling, GPU acceleration, history resources, portals, mirrors, and AOV
exports fit the same model.

## Industry references

The design should follow the common frame/render-graph shape used by modern
renderers:

- **Frostbite FrameGraph** - a graph of render passes and resources that lets
  features stay decoupled while the frame remains efficient.
  <https://www.gdcvault.com/play/1024612/FrameGraph-Extensible-RenderingArc>
- **Unreal RDG** - passes declare graph resources through pass parameters; the
  graph derives dependencies, transient lifetimes, and validation.
  <https://dev.epicgames.com/documentation/de-de/unreal-engine/render-dependency-graph-in-unreal-engine>
- **Unity Render Graph** - SRP passes explicitly state resource use so the
  system can simplify pipeline setup and manage runtime performance.
  <https://docs.unity.cn/Packages/com.unity.render-pipelines.core%4014.0/manual/render-graph-system.html>
- **Filament FrameGraph** - a compact reference model with pass nodes, resource
  nodes, read/write edges, topological sort, unreachable-pass culling, resource
  lifetimes, usage bits, load/store inference, and import/export resources.
  <https://google.github.io/filament/notes/framegraph.html>
- **Vulkan synchronization model** - explicit APIs make clear that passes are
  not automatically synchronized; the graph should preserve enough information
  to reason about barriers and resource hazards when GPU executors arrive.
  <https://docs.vulkan.org/samples/latest/samples/performance/pipeline_barriers/README.html>
- **USD Hydra** - not a render graph, but an important reference for separating
  scene data from rendering backends through scene delegates, render indexes,
  and render delegates.
  <https://docs.nvidia.com/learn-openusd/latest/beyond-basics/hydra.html>

## Core distinction: intent vs. plan

The user should normally author **render intent**, not low-level passes. The
compiled graph is an implementation detail that can be inspected, disabled,
and overridden when teaching or debugging.

Examples of render intent:

- render this scene with the raytracer;
- render this scene as a raster preview with shadows;
- generally rasterize the frame, and later generally path trace once a path
  tracing engine exists;
- add a wireframe overlay;
- export `depth`, `normal`, and `object_id` AOVs;
- show portal or mirror effects where the selected executor supports them;
- render a nested scene into a screen material;
- use the raytracer for the main scene and raster or wireframe for a sub-view;
- render one tagged part of the scene as wireframe;
- render another tagged part with a toon shading profile.

Examples of compiled plan details:

- a raster portal may compile to stencil marking, a portal-camera render pass,
  and a composite pass;
- a raytraced portal may compile to no extra pass because the material can trace
  the redirected ray directly;
- a wireframe overlay may compile to a pass that reads depth and writes over the
  color target;
- a screen material may compile to an offscreen render target pass before the
  main scene pass consumes that texture.

This split gives us both automation and control: users can ask for high-level
effects, while advanced users and teaching tools can inspect or modify the
resulting graph.

## Compilation is a first-class step

Graph compilation must be independent of graph execution. For education and
debugging, callers need to compile the graph from a scene, inspect it, disable
or override nodes, then render the manipulated plan later.

The lifecycle should be:

```text
world scene + camera + render intent
  -> RenderGraphCompiler
  -> RenderPlan
  -> inspection / export / node overrides / validation
  -> GraphRenderEngine executes the RenderPlan
  -> output resources
```

This is not just a render implementation detail. It is an explicit workflow:

- rendercli can print or export the graph without rendering;
- Modeler can show the graph before the first frame is rendered;
- documentation tools can generate graph diagrams from scenes;
- tests can validate compiled plans without comparing pixels;
- users can disable nodes before execution to observe the effect.

`GraphRenderEngine` may compile internally for simple callers, but the compiler
and plan must also be usable directly.

## Public architecture

### Module placement

The graph renderer should live under a new module:

```text
include/engine/graph/
src/engine/graph/
```

This module owns `GraphRenderEngine`, `RenderGraphCompiler`, graph pass
payloads, render-intent types, plan validation, plan serialization, and graph
execution. It may use shared `render::` scene/camera/resource concepts, but the
graph itself is an engine module.

### Default through the graph, direct engines still usable

The current direct engines should remain valid public APIs:

- `engine::raytracer::Raytracer`
- `engine::raster::Rasterizer`
- `engine::wireframe::Wireframe`

They are useful for focused tests, simple callers, benchmarking, and teaching
single-engine behavior.

The normal render path goes through the graph because that is where AOVs,
feature expansion, overlays, render-to-texture surfaces, and educational
inspection live. Direct engines remain available when the caller explicitly
asks for them, such as rendercli's `--direct_engine` mode or a focused unit
test.

Graph-backed rendering should be able to produce the same output as a direct
engine when the compiled plan contains only one beauty pass plus final output.

### Add a graph compiler

Add a standalone compiler, tentatively:

```cpp
namespace engine::graph {
  class RenderGraphCompiler {
  public:
    RenderPlan compile(const world::Scene& scene,
                       const render::Camera& camera,
                       const RenderIntent& intent) const;
  };
}
```

The compiler should:

- snapshot the editable world intent needed for planning;
- discover automatic render features from scene elements, materials, lights, and
  UI intent;
- apply global defaults and per-scene/per-subtree overrides;
- produce a validated, inspectable `RenderPlan` without allocating every render
  resource or drawing pixels;
- preserve stable node/resource ids across recompiles when the scene structure
  has not changed.

### Add a graph engine facade

Add a new sibling `RenderEngine`, tentatively:

```cpp
namespace engine::graph {
  class GraphRenderEngine : public RenderEngine {
  public:
    void setIntent(RenderIntent intent);
    void setPlan(RenderPlan plan);
    RenderPlan compilePlan() const;
    const RenderPlan& lastPlan() const;
    void render(Buffer<Colord>& buffer) override;
  };
}
```

`GraphRenderEngine` should:

- accept high-level `RenderIntent`;
- compile a `RenderPlan` from the current scene, camera, and intent when a
  caller wants the simple one-call path;
- accept a precompiled and possibly user-manipulated `RenderPlan`;
- execute the selected plan into the requested output buffer;
- keep the last compiled plan available for inspection by rendercli, Modeler,
  tests, and documentation tools.

This keeps `RenderEngine` as the simple entry point while allowing
`RenderPlan` to become a first-class inspectable object.

## Core model

### RenderIntent

`RenderIntent` is the user-facing request. It is serializable in scene JSON so
a scene can describe its intended final render, educational views, and preview
strategies. `RenderGraphRequest` is the shared front-end resolver that
rendercli, Modeler, tests, and direct API callers use to layer temporary
overrides on top of the scene intent without mutating the scene file.

The effective intent is built in layers:

```text
scene JSON RenderIntent
  -> application defaults
  -> RenderGraphRequest overrides from rendercli / Modeler / API
  + RenderSceneAnalysis from the current scene snapshot
  -> compiled RenderPlan
```

This lets Modeler render a lower-fidelity realtime-ish preview of the final
image while preserving the final render description in the scene.
`RenderSceneAnalysis` is the scene-content side of compilation: intent says
what the user wants, while analysis records facts such as visible surfaces and
lights so feature planners can decide whether requested nodes like preview
shadow maps are meaningful for this scene.

```cpp
enum class RenderExecutorPreference {
  Raytracer,
  Rasterizer,
  Wireframe,
  // Add PathTracer once a concrete path tracing engine exists.
};

enum class RenderViewMode {
  Default,
  Beauty,
  Wireframe,
  Depth,
  Normal,
  ObjectId,
  MaterialId,
  WorldPosition
};

struct ShadingProfileRef {
  std::string name;
  ShadingProfileParameters parameters;
};

struct RenderCameraRef {
  std::optional<std::string> sceneCameraId;
  std::optional<CameraSnapshot> snapshot;
};

struct RenderViewOverride {
  SceneSelector selector;
  std::optional<RenderExecutorPreference> executor;
  std::optional<RenderViewMode> viewMode;
  std::optional<ShadingProfileRef> shadingProfile;
  std::optional<RenderCameraRef> camera;
  std::optional<bool> inheritEngineOptions;
  RenderEngineOptions engineOptions;
};

struct RenderSubviewIntent {
  std::string name;
  RenderViewOverride view;
};

struct RenderIntent {
  RenderExecutorPreference defaultExecutor;
  RenderViewMode defaultViewMode = RenderViewMode::Beauty;
  ShadingProfileRef defaultShadingProfile{"default", {}};
  std::optional<RenderCameraRef> defaultCamera;
  bool enableAutomaticFeatures = true;
  bool enableWireframeOverlay = false;
  bool enablePreviewShadows = false;
  RenderPostProcessAA postProcessAA = RenderPostProcessAA::None;
  RenderEngineOptions engineOptions;
  std::vector<RenderViewMode> exportedAOVs;
  std::vector<RenderViewOverride> viewOverrides;
  std::vector<RenderSubviewIntent> subviews;
};
```

The exact API can evolve, but it should stay high-level. It should not require
the caller to manually describe "stencil first, then reflected camera, then
composite" for a planar mirror.
Whole-frame (`selector: all`) overrides are applied to the default frame intent
before the compiler synthesizes nodes. More specific selector overrides remain
intent for later scene-partitioning planners; users should not author pass
nodes directly as the normal API.
Render tools that attach executor-specific pass state should use this effective
frame intent too, so a scene-authored whole-frame raster override receives the
same raster MSAA, shadow, and postprocess state as an explicit raster default.
Engine-specific advanced controls are intent, not node requests: raytracer
sampler/samples/view-plane/recursion settings, raster sampling/framebuffer/
shadow settings, and wireframe LOD are stored as typed `RenderEngineOptions`.
The compiler resolves those options into typed payload state on the nodes it
synthesizes. Subviews use the same view-override option fields to inherit the
global engine options or provide their own override block, which keeps future
render-to-texture quality controls in the same model without asking users to
directly author graph nodes.
The effective default camera is carried on synthesized scene-rendering pass
`SceneView` records and serialized in exported plan JSON, even though current
executors still render with the engine's active camera until alternate-camera
execution is wired in.
Tools derive that default camera from the active editable-scene camera when the
scene intent does not name one explicitly, so graph inspection can still report
the camera used by ordinary scene renders.

The important requirement is that intent can express both:

- broad defaults, such as "generally rasterize this frame" now and "generally
  path trace this frame" once the path tracer exists; and
- local overrides, such as "render this tagged subtree as wireframe" or "use
  a toon shading profile for these objects."

The planner decides how to satisfy those requests for the selected executors.
For example, a subtree using the named shading profile `"toon"` may become a
separate raster pass plus a composite edge, while a wireframe subtree may become
an overlay pass reading main depth.

`Cartoon` should not be a fixed core enum value. It is a specific shading
technique or style profile, and the graph should not need a new enum value for
every future look such as clay, x-ray, technical illustration, watercolor, or
pixel art. The core intent carries a view mode plus an optional named shading
profile; registered planners and executors decide how to implement that profile.

Camera overrides are part of intent. This matters for Modeler and teaching
views: a pass can render the scene from an inspection camera while drawing some
other camera's frustum as visible geometry, or render a selected subtree through
an alternate camera into a texture.

### RenderPlan

`RenderPlan` is the compiled DAG:

```cpp
class RenderPlan {
public:
  const std::vector<RenderPassNode>& passes() const;
  const std::vector<RenderResourceDescriptor>& resources() const;

  RenderPlanValidation validate() const;
  std::string toText() const;
  std::string toDot() const;
  QJsonObject toJson() const;

  void connectProducerToConsumer(RenderPassNode producer,
                                 RenderResourceDescriptor resource,
                                 RenderPassId consumerPassId);
  std::size_t routeResourceThroughPass(RenderResourceId sourceResource,
                                       RenderResourceDescriptor routedResource,
                                       RenderPassNode pass);
  RenderPlan withOverrides(RenderGraphOverrides overrides) const;
};
```

The plan should be deterministic for a given scene snapshot and intent. Stable
node ids matter because UI controls, rendercli filters, tests, and docs should
be able to reference specific nodes.

Plan manipulation should operate on the compiled plan without requiring a render
to happen. Disabling a node, switching a pass to a fallback view mode, or
changing a node's enabled state should be reflected in validation and
visualization before execution. Producer/resource construction and
producer/consumer rewrites belong on `RenderPlan`, so compiler code can connect
passes through resource edges rather than hand-maintaining dependency reads and
writes around serial list insertion.

### RenderPassNode

A pass node declares its resource access and execution constraints:

```cpp
enum class RenderExecutorKind {
  Raytracer,
  Rasterizer,
  Wireframe,
  // Add PathTracer once a concrete path tracing engine exists.
  Composite,
  PostProcess
};

enum class DisabledBehavior {
  Error,
  CullDependents,
  SubstituteDefault,
  Passthrough
};

struct RenderPassNode {
  RenderPassId id;
  std::string name;
  RenderPassKind kind;
  RenderExecutorKind executor;
  std::vector<RenderFeatureKind> features;
  std::vector<ResourceRead> reads;
  std::vector<ResourceWrite> writes;
  SceneView sceneView;
  std::shared_ptr<const RenderPassState> state;
  DisabledBehavior disabledBehavior;
  bool enabled = true;
  bool hasExternalSideEffects = false;
  bool canRunConcurrently = true;
};
```

The first implementation can execute pass nodes serially in resource dependency
order. The declaration must still be strong enough for a later scheduler to run
independent nodes in parallel.

### Pass payloads

Pass execution payloads should be virtual. A pass node is the declarative graph
record; a payload is the executor-specific implementation behind that node.

```cpp
class RenderPassPayload {
public:
  virtual ~RenderPassPayload() = default;
  virtual void execute(RenderExecutionContext& context) = 0;
};
```

This keeps the graph open to specialized pass implementations without forcing a
large tagged union into the core plan model. Built-in payloads should still have
serializable descriptors where possible so compiled plans can be exported,
inspected, and replayed for built-in pass types.

### Render resources

The graph should not store `Buffer<T>` directly in pass nodes. Use handles and
descriptors:

```cpp
enum class RenderResourceType {
  Color,
  Depth,
  Stencil,
  ObjectId,
  MaterialId,
  Normal,
  WorldPosition,
  MotionVector,
  ShadowMap,
  ShadowMask,
  CustomTexture
  // Future additions: EnvironmentMap, IrradianceCache, PhotonMap,
  // AccelerationData, LookupTable, ScalarBuffer, Geometry, Volume, and
  // ProceduralField once the corresponding pass/storage paths exist.
};

enum class RenderResourceDomain {
  CPU,
  GPU
};

enum class RenderResourceLifetime {
  Transient,
  Imported,
  Exported,
  History,
  PersistentCache
};

struct RenderResourceDescriptor {
  RenderResourceId id;
  std::string name;
  RenderResourceType type;
  RenderFormat format;
  int width;
  int height;
  int sampleCount;
  RenderResourceDomain domain;
  RenderResourceLifetime lifetime;
};
```

For CPU v1, storage can wrap typed buffers:

```cpp
class RenderResourceStorage {
public:
  Buffer<Colord>& color(RenderResourceId id);
  Buffer<double>& depth(RenderResourceId id);
  Buffer<std::uint8_t>& stencil(RenderResourceId id);
  Buffer<std::uint32_t>& objectId(RenderResourceId id);
};
```

Future GPU support should add GPU storage behind the same descriptors. The graph
should know "this is a color resource with these dimensions and access modes,"
not "this is always a `Buffer<Colord>`."

Not every future graph resource will be a final-frame image. The first
implementation supports only image-like CPU resources; later graph slices should
add scene-derived data products such as photon maps, reflection probes,
irradiance caches, denoiser feature buffers, lookup tables, acceleration data,
geometry, volumes, and procedural fields when those pass/storage paths exist.

### Generated scene data vs. graph resources

Procedural generators such as terrain or cloud generators are usually scene or
asset concepts, not render-graph concepts. The generator definition belongs
before graph compilation: it is part of the world being rendered.

The graph should only model generator outputs when they are render-dependent
products or caches. Examples:

- a stable terrain mesh generated as part of loading the scene is ordinary scene
  geometry and should exist before graph compilation;
- view-dependent terrain LOD tiles, clipmaps, or impostors may become future
  geometry/procedural-field graph resources with `PersistentCache` lifetime;
- a cloud material's procedural density function is scene/material data, but a
  froxel grid, shadow volume, or cached density field generated for the current
  view can become a future volume or procedural-field graph resource;
- photon maps, irradiance caches, path-guiding data, reflection probes, and
  acceleration data are render-derived resources even when they survive across
  frames.

The practical rule: if data exists independent of the chosen renderer, camera,
quality level, or frame, create it before graph compilation. If data is produced
because a render pass needs it, has renderer-specific fidelity, or must be
invalidated with frame/view/quality changes, model it as a graph resource.

### Persistent graph artifacts and invalidation

Some graph resources are expensive to rebuild and should be reusable across
frames. Shadow maps are the first useful example, but the same model should
also cover future reflection probes, irradiance caches, photon maps,
path-guiding data, acceleration data, generated terrain LOD tiles, and
denoiser/history feature buffers.

Transient frame resources and persistent artifacts should remain distinct:

- `RenderResourceStorage` owns the resources used by one graph execution.
- `RenderGraphArtifactCache` lives beside `GraphRenderEngine` and survives
  across frames and cloned preview render snapshots. ✅ **Done.** The first
  cache slice provides clone-shared storage for immutable
  `RenderGraphCachedArtifact` instances keyed by producer pass, resource
  descriptor, typed pass-state fingerprint, and render-input fingerprint.
- `RenderResourceLifetime::PersistentCache` marks a resource as cacheable, but
  cache reuse still depends on the resource descriptor, producer pass state, and
  an invalidation fingerprint.
- Cached artifacts should be immutable once published so an old preview render
  can safely keep using an artifact while a newer render replaces it.

The cache key should be typed, not a loose JSON string. A cache entry needs at
least:

- producing pass id and resource id;
- resource descriptor shape, domain, and type;
- serialized or typed pass state relevant to the artifact;
- target/camera/executor settings that affect the artifact;
- scene dependency fingerprints for the objects, lights, materials, and
  transforms the producer pass actually reads.

Initial invalidation can be conservative. For shadow maps, it is acceptable for
the first cache slice to invalidate on any scene, camera, target, LOD, culling,
or shadow-setting change. Later slices should refine this into domains such as
`geometry`, `transform`, `light`, `camera`, and `material`. Modeler currently
has a coarse scene `changed` flag, so fine-grained invalidation will require
stable scene/object revision counters or fingerprints.

Shadow maps now have a graph-owned artifact path. The current
`raster_preview_shadows` graph node publishes shadow-map request state, builds
the full directional/cascade collection through `RasterShadowMapBuilder`, stores
that collection as an immutable artifact, and exposes the first cascade as an
inspectable depth preview. `raster_beauty` consumes the artifact when present,
so cache hits skip the shadow-map build that used to be hidden inside beauty
execution. The completed architectural slice was:

1. ~~extract the raster shadow-map builder from `Rasterizer` into a graph-usable
   service or payload helper~~ ✅ **Done.** `RasterShadowMapBuilder` now owns
   full directional/cascade map construction and first-cascade depth previews;
2. ~~introduce a typed `ShadowMapArtifact` resource that owns directional light
   cascades, depth buffers, and sampling/filter metadata~~ ✅ **Done.**
   `RasterShadowMapArtifact` owns the full raster shadow-map collection and a
   depth preview;
3. ~~make the graph shadow pass produce the full artifact collection~~ ✅
   **Done.** `raster_preview_shadows` materializes and caches the collection;
4. ~~make `raster_beauty` consume the artifact instead of triggering an
   internal shadow build~~ ✅ **Done.** The rasterizer accepts graph-supplied
   external shadow maps for beauty shading;
5. ~~extend cache hit/miss metadata from the current trace-visible first-cascade
   artifact to the full beauty-consumed shadow-map collection~~ ✅ **Done.**
   Cache status now describes the full artifact consumed by raster beauty.

One nuance: the current cascaded directional shadow maps are view-camera
dependent because cascade fitting uses the main camera's visible depth range.
If the desired behavior is "do not invalidate when the camera moves, only when
the light or occluders move," the shadow-map fitting strategy must change to a
more stable light-space cache, or the cache must tolerate camera movement by
reusing only compatible cascades.

## Pass taxonomy and stress-test examples

Most examples in this section are out of scope for the first render-graph
implementation. They exist to keep the architecture honest: if the model cannot
describe these passes, it is probably too narrow.

### Common passes

These are executor-agnostic or appear at the end of almost every frame:

- **Tonemap pass** - convert HDR scene-linear color into display/export color.
- **Color-management pass** - apply output transform, gamma/OETF, display gamut,
  or a look/LUT.
- **Exposure pass** - fixed exposure or auto-exposure based on scene luminance.
- **AOV visualization pass** - convert depth, normals, object ids, material ids,
  UVs, or motion vectors into inspectable color.
- **Compositor pass** - combine color, alpha, masks, depth, object ids, or
  explicit layers into the final image. ✅ **Partial.** Built-in `Composite`
  executor passes tagged `depth_composite` or `stencil_composite` can now choose
  foreground color over base color through graph-visible depth and stencil
  resources.
- **Postprocess anti-aliasing pass** - FXAA, SMAA, or other image-space AA.
- **Temporal accumulation pass** - TAA, temporal denoising, progressive sample
  accumulation, or history blending.
- **Denoise pass** - read noisy color plus albedo/normal/depth feature buffers
  and produce filtered color.
- **Blur/filter pass** - Gaussian blur, bilateral blur, Kawase blur, box blur,
  shadow-mask blur, or separable horizontal/vertical image filters.
- **Bloom/glare pass** - threshold bright regions, blur, then composite.
- **Depth-of-field post pass** - image-space circle-of-confusion blur from depth.
- **Motion-blur post pass** - velocity-buffer blur or stochastic accumulation.
- **Sharpen/upscale pass** - image sharpening, pixel-art scaling, or future
  learned/super-resolution upscaling.
- **Debug overlay pass** - draw tiles, bounding boxes, selected objects, camera
  frusta, light volumes, graph-node labels, or profiling heatmaps.

Cross-check: these passes require image resources, history resources, imported
view settings, and simple pass chaining. The current plan covers this through
typed resources, `PostProcess` / `Composite` executors, imported/history
lifetimes, and node disabling.

As the graph grows, postprocess stages that currently live inside individual
engines should migrate into explicit nodes. ~~Image-space FXAA/SMAA~~ ✅
**Done.** `RenderIntent::postProcessAA` now compiles FXAA/SMAA into
`post_fxaa` / `post_smaa` postprocess nodes with typed `post_process_aa` state
between beauty and overlay/tonemap for raytracer, wireframe, and rasterizer
graphs; rendercli and the Modeler preview both use that shared compiler path.
TAA remains deferred until graph-owned history, depth, and jitter resources
exist.

Raster preview shadow requests now also carry typed pass state: ✅ **Done.**
`raster_preview_shadows` serializes `parameters.shadows`, execution publishes
that state on the `preview_shadow_map` resource, materializes the full
directional/cascade shadow-map artifact, and `raster_beauty` consumes it when
the shadow node is enabled. Splitting individual directional/cascade depth
passes remains future raster graph work.

### Rasterizer passes

Raster rendering is where graph-based decomposition is most immediately useful:

- **Depth pre-pass** - render depth only before a color pass.
- **Main forward raster pass** - shade visible triangles directly into color.
- **G-buffer pass** - write depth, normal, material id, albedo, roughness,
  motion vectors, and object ids for deferred shading or post effects.
- **Deferred lighting pass** - read G-buffer outputs and accumulate direct
  lighting in image space.
- **Shadow-map pass** - render light-space depth for directional, spot, or point
  lights.
- **Cascaded shadow-map passes** - multiple directional-light depth passes with
  split fitting and stabilization.
- **Shadow-filter pass** - PCF, PCSS, variance-shadow-map filtering, or blur of
  a shadow mask.
- **Environment-map pass** - render or sample sky/environment lighting.
- **Reflection-probe pass** - render cubemaps or cached probes for curved or
  rough reflective objects.
- **Irradiance-probe pass** - prefilter diffuse environment lighting.
- **Planar-reflection pass** - reflected camera for flat mirrors or water.
- **Curved-reflection fallback pass** - environment/probe reflection for curved
  reflective objects where a planar pass is invalid.
- **Screen-space reflection pass** - trace in depth/color buffers for local
  glossy reflection approximations.
- **Stencil-marking pass** - mark portals, mirrors, masks, outlines, or selected
  regions.
- **Portal-view pass** - render through a transformed camera only where a portal
  mask/stencil applies.
- **Decal pass** - project decals into G-buffer or forward color/depth.
- **Transparent sorted pass** - draw alpha-blended surfaces after opaque depth.
- **Order-independent transparency pass** - weighted blended OIT or per-pixel
  linked-list style transparency in a future GPU path.
- **Particle/sprite pass** - billboarded primitives, impostors, and effects.
- **Toon shading pass** - quantized lighting plus optional outline.
- **Outline/silhouette pass** - edge detection from normals/depth/object ids or
  geometry-expanded silhouettes.
- **Object-id/picking pass** - write stable selection ids for Modeler picking.
- **Occlusion/visibility pass** - produce object visibility, queries, or masks.

Cross-check: raster passes need depth/stencil resources, scene selectors,
alternate cameras, per-selector view-mode/shading-profile overrides, default
resources when disabled such as "no shadow," and later resource aliasing. The
current plan covers the declarations, but the first slice should avoid claiming
real portal/mirror support until stencil/depth-aware composition exists.

### Raytracer and path-tracer passes

Ray-style renderers also benefit from graph decomposition, especially for caches,
hybrid previews, and progressive/denoised output:

- **Primary-ray beauty pass** - trace camera rays and write HDR color.
- **Raytraced shadow-mask pass** - compute visibility from surface points to
  lights for a raster or deferred shading pass.
- **Reflection/refraction pass** - trace secondary rays as a separate resource
  for compositing or debugging.
- **Photon-emission pass** - shoot photons from lights and store a photon map.
- **Caustic photon-map pass** - build a focused photon map for specular caustics.
- **Photon-gather pass** - estimate indirect/caustic lighting from photon maps.
- **Final-gather pass** - shoot secondary diffuse rays from visible points.
- **Irradiance-cache pass** - compute and reuse sparse indirect-light samples.
- **Subsurface-scattering pass** - diffusion/profile gather or random-walk
  transmission for translucent materials.
- **Volume-integration pass** - integrate participating media, fog, smoke, or
  volumetric shadows.
- **Path-sample accumulation pass** - accumulate many stochastic samples per
  pixel over time or batches.
- **Variance/statistics pass** - write sample variance, convergence, or adaptive
  sampling masks.
- **Adaptive-sampling scheduler pass** - choose which pixels need more samples.
- **Denoiser feature pass** - output albedo, normal, depth, motion vectors, and
  variance for a denoiser.
- **Path-guiding data pass** - build or update directional sampling data.
- **Light-sampling/reservoir pass** - future ReSTIR-style candidate or reservoir
  data for direct/indirect lighting.
- **Debug ray-event pass** - output recursion depth, hit kind, material branch,
  or path contribution diagnostics.

Cross-check: these require future non-image resources such as photon maps,
irradiance caches, and path-guiding data, plus history resources,
imported/exported progressive state, and passes that may not write the final
framebuffer at all. The resource model must therefore be able to grow beyond
textures without adding those unimplemented resource types prematurely.

### Wireframe and modeling passes

Wireframe is not just a whole-frame engine; it is often an overlay or diagnostic
mode:

- **Wireframe beauty pass** - draw all visible edges over a background.
- **Depth-aware wireframe overlay pass** - draw edges against an existing depth
  buffer.
- **Hidden-line pass** - render visible and hidden edges with different styles.
- **Silhouette/crease pass** - draw only important contour or feature edges.
- **Selection-outline pass** - draw selected objects or hovered objects.
- **Bounding-volume pass** - boxes, spheres, BVHs, grids, tile bins, and light
  volumes.
- **Frustum/clip-volume pass** - show cameras, cascades, view frusta, portals,
  and clipping planes.
- **Manipulator/gizmo pass** - draw translation/rotation/scale handles in
  Modeler.

Cross-check: these passes need scene selectors, depth reads, style state, and
often UI-only scene data. That supports keeping graph intent connected to
`world::Scene` and Modeler view state rather than only runtime
`render::Scene`.

### User-specified composition examples

The graph should support explicit creative and educational compositions, not
only automatic feature expansion:

- **Wireframe inside a computer screen** - render a nested scene with the
  wireframe executor into `screen_color`, then use it as a screen texture in the
  main scene.
- **Toon inset monitor in a path-traced room** - rasterize a tagged nested
  scene with toon shading, then path trace the room that contains the screen.
- **Raytraced hero object over raster background** - rasterize the full scene,
  raytrace one tagged object with higher-quality reflection/refraction, then
  depth/object-id composite.
- **Raster preview with raytraced shadows** - rasterize a G-buffer, raytrace a
  shadow mask, then shade/composite.
- **Wireframe diagnostic over final render** - render normal beauty, then draw
  selected edges or BVH boxes over it.
- **Depth-only fog composite** - render depth, run an image-space fog pass, then
  composite with color.
- **Object-id matte extraction** - render object ids, isolate a selected object,
  blur or color-grade it, then composite.
- **Portal preview** - mark portal pixels, render the destination view, and
  composite through the portal mask.
- **Mirror comparison** - render a planar reflection pass and an environment
  probe approximation, then show either one or a split-screen comparison.
- **AOV contact sheet** - render beauty, depth, normals, albedo, object id, and
  motion vectors into a tiled debug image.
- **Render graph teaching sequence** - execute the same compiled plan multiple
  times with one node disabled each time, producing a set of images that show
  what every pass contributes.
- **Stylized multi-layer frame** - path trace the background, rasterize
  toon-shaded characters, draw wireframe props, bloom emissive surfaces, and
  tonemap at the end.

Cross-check: user-specified composition needs stable scene selectors,
render-to-texture resources, per-selector executor/view-mode/shading-profile
overrides, graph JSON export/import, and a way to validate manipulated plans
before execution. These are now explicit requirements in the plan.

## Automatic feature expansion

Automatic graph generation should be driven by explicit feature intent in the
scene and materials, not by fragile class-name checks alone.

Add a feature-planning layer:

```cpp
class RenderFeaturePlanner {
public:
  virtual void contribute(RenderPlanBuilder& builder,
                          const RenderFeatureContext& context) const = 0;
};
```

Feature planners can be attached to materials, surfaces, lights, scene elements,
or UI/render intent.

Feature expansion must see the default executor/view mode/shading profile and
any local view overrides. The same scene can therefore compile different plans
for different intents:

- "generally rasterize" may expand mirrors to stencil and reflected-view raster
  passes;
- "generally raytrace" may keep reflective materials inside the raytraced beauty
  pass;
- "this subtree is wireframe" may add a depth-aware overlay or route that
  subtree to a wireframe pass;
- "this subtree uses the `toon` shading profile" may choose a quantized material
  evaluator, outline pass, or separate composited target depending on the
  executor.

### Examples

#### Raster preview shadows

Input intent:

```text
primary = Rasterizer
enablePreviewShadows = true
```

Compiled plan:

```text
DirectionalShadowMapPass -> light_depth
MainRasterPass reads light_depth -> main_color, main_depth
TonemapPass reads main_color -> display
```

#### Raytraced main scene with wireframe overlay

Input intent:

```text
primary = Raytracer
enableWireframeOverlay = true
```

Compiled plan:

```text
RaytraceBeautyPass -> beauty_color
WireframeOverlayPass reads beauty_color -> overlay_color
TonemapPass reads overlay_color -> display
```

#### Per-subtree wireframe view mode

Input intent:

```text
primary = Rasterizer
override tag "rig" -> view mode Wireframe
```

Compiled plan:

```text
MainRasterPass scene excludes tag "rig" -> main_color, main_depth
WireframePass scene includes tag "rig", reads main_depth -> composed_color
TonemapPass reads composed_color -> display
```

This is not merely a global overlay. It is a scene-part routing request: the
selected objects are drawn with a different executor/view mode from the rest of
the frame.

#### Per-subtree toon shading profile

Input intent:

```text
primary = Raytracer
override tag "screen_content" -> executor Rasterizer, shading profile "toon"
```

Compiled plan:

```text
ToonRasterPass scene includes tag "screen_content" -> toon_color
MainRaytracePass reads toon_color -> main_color
TonemapPass reads main_color -> display
```

The exact composition depends on how the selected objects appear in the scene.
For a screen material, the toon pass may produce a texture consumed by the main
pass. For ordinary world geometry, the planner may need depth/object-id
composition before this becomes correct.

#### Screen material

A screen surface that displays a nested scene should compile to an offscreen
render before the containing scene consumes that texture:

```text
NestedSceneRasterPass -> screen_color
MainRaytraceOrRasterPass reads screen_color -> main_color
TonemapPass -> display
```

This is the simplest first hybrid demo because the composition can be handled as
texture sampling on the screen surface.

#### Portal material

For the raytracer:

```text
RaytraceBeautyPass -> main_color
```

The material can redirect rays during shading, so no extra pass is required.

For the rasterizer:

```text
PortalStencilPass -> portal_stencil
PortalViewRasterPass reads portal_stencil -> portal_color
MainRasterPass -> main_color, main_depth
PortalCompositePass reads main_color, portal_color, portal_stencil -> composed_color
```

Exact ordering may change once implementation starts, but the key requirement is
that the user says "this is a portal" and the raster planner supplies the
stencil and alternate-camera passes.

#### Planar mirror

A reflective material alone is not enough information. A reflective sphere does
not imply a planar reflection pass. The scene needs an explicit planar mirror
feature, either through a future `MirrorMaterial`, a material flag, or a surface
role.

For the rasterizer:

```text
MirrorStencilPass -> mirror_stencil
ReflectedViewRasterPass reads mirror_stencil -> reflected_color
MainRasterPass -> main_color, main_depth
MirrorCompositePass reads main_color, reflected_color, mirror_stencil -> composed_color
```

For the raytracer, ordinary reflective materials can continue to trace
reflection rays directly.

## Scene partitioning and render views

The graph needs more than a flat runtime `render::Scene`. It needs a way to
describe which subset of the world a pass sees and how that subset is viewed.

Minimum concepts:

- surface visibility;
- light visibility;
- tags or layers;
- stable scene selectors that can address one object, a subtree, a tag/layer, or
  a material role;
- pass-specific include/exclude masks;
- material/surface feature roles such as screen, portal, mirror, overlay-only;
- per-selector executor, view-mode, shading-profile, and camera overrides;
- nested scene references for render-to-texture surfaces;
- alternate cameras such as reflected cameras and portal-transformed cameras;
- inspection cameras that can render one camera's frustum from another
  perspective;
- recursion limits for mirrors, portals, and screens that show scenes containing
  themselves.

The existing `world::Scene` is a good place to discover editable intent. The
runtime `render::Scene` is a good executor input after the plan has decided what
each pass should see.

## Node disabling and defaults

Compiled nodes should be user-visible and individually disable-able:

```cpp
struct RenderGraphOverrides {
  std::set<RenderPassId> disabledPasses;
  std::set<RenderPassKind> disabledPassKinds;
  std::set<RenderExecutorKind> disabledExecutors;
  std::set<RenderFeatureKind> disabledFeatures;
};
```

Disabling a pass cannot always mean "skip this node and keep going." The graph
must know what to do with resources the disabled node would have produced.

Supported disabled behaviors:

- `Error` - required pass; disabling makes the plan invalid.
- `CullDependents` - disable this pass and all passes that require its outputs.
- `SubstituteDefault` - produce a default resource instead, such as a white
  shadow mask, black reflection texture, empty stencil, or empty overlay.
- `Passthrough` - for postprocess/composite passes, return the main input
  unchanged.

For teaching, prefer `SubstituteDefault` and `Passthrough` where the result is
meaningful. Disabling a shadow-map pass should show a render without shadows,
not make the frame disappear.

## Graph inspection and UI

The graph is an educational surface, not just backend plumbing.

### Rendercli

Rendercli should expose both high-level and graph-level controls:

```text
--render-graph
--compile-render-graph
--render-graph-only
--render-graph-format text|dot|json
--render-graph-out graph.dot
--render-graph-in graph.json
--disable-pass ShadowMapPass
--disable-pass-kind shadow
--disable-executor wireframe
--disable-feature portal
--enable-aov depth,normal,object_id
--default-engine raytracer|rasterizer|wireframe
--direct-engine raytracer|rasterizer|wireframe
--view-mode selector=wireframe
--shading-profile selector=toon
--executor selector=wireframe
--camera selector=camera_id
--wireframe-overlay
```

The exact flag names can change, but rendercli should be able to:

- print the compiled plan without rendering;
- write a compiled plan that can be inspected or edited before rendering;
- read a previously exported plan and execute it;
- render with selected nodes disabled;
- export DOT/JSON for docs and debugging;
- specify global executor, view-mode, shading-profile, and camera defaults;
- specify per-selector executor, view-mode, shading-profile, and camera
  overrides;
- bypass the graph only when explicitly requested;
- render selected AOV resources to images where the resource format supports it.

### Modeler

The Modeler render view should grow a graph inspector:

- compile the graph before rendering and show it immediately;
- node list or DAG visualization;
- checkbox per node;
- grouped toggles by feature, pass kind, and executor;
- global controls for default executor, view mode, shading profile, and camera;
- per-selection controls for executor, view-mode, shading-profile, and camera
  overrides;
- pass details: executor, reads, writes, scene view, camera, shading profile,
  disabled behavior;
- resource details: type, size, format, producer, consumers;
- click a resource to preview it where possible;
- after a render, select a node to inspect supported input snapshots, output
  snapshots, and difference images from the last execution trace;
- while a render is running, highlight currently executing nodes in the graph
  view;
- expose the scene's saved render intent as a generated scene item in the
  Elements dock so the property editor can update durable intent without
  treating intent as a renderable graph pass;
- present that generated scene item as `Render Settings` in the UI, with
  grouped fields, dropdowns for enumerated choices, and backend-specific
  properties hidden unless their backend is selected;
- keep Modeler-facing intent controls focused on user-visible render quality:
  constrain numeric values in the editor, use discrete controls for discrete
  choices, and keep low-level view-plane/thread/queue controls out of the
  Modeler UI while preserving those fields for JSON/rendercli paths;
- keep preview controls and final-render controls as temporary request
  overrides layered over the saved intent;
- show the final render graph in the Render window before execution starts and
  execute that displayed graph.
- validate the manipulated graph before rendering;
- export graph as DOT/JSON/text.

The UI should not require deep backend mutation. It should pass graph overrides
to the planner or manipulate a compiled plan, then re-render. This supports the
educational workflow where a user compiles the graph, disables or changes nodes,
and only then executes the frame.

### Execution traces and per-node resource inspection

After a render, Modeler should be able to inspect what each graph node read,
wrote, and changed. This should be modeled as an execution trace rather than as
state on `RenderPlan`: the plan is the declarative graph, while a trace is the
result of one concrete execution.

Add a trace model, tentatively:

```cpp
class RenderGraphExecutionTrace {
public:
  const std::vector<RenderPassTrace>& passes() const;
  const RenderPassTrace* findPass(RenderPassId id) const;
};

struct RenderPassTrace {
  RenderPassId passId;
  RenderPassExecutionStatus status;
  std::vector<RenderResourceSnapshot> inputs;
  std::vector<RenderResourceSnapshot> outputs;
  std::vector<RenderResourceDiff> diffs;
  std::chrono::nanoseconds elapsed;
  std::string message;
};
```

The first trace implementation should support inspectable image-like CPU
resources:

- capture color inputs before a pass executes and color outputs after it
  executes;
- keep trace capture opt-in so ordinary graph renders, rendercli runs, and
  future render-farm/movie renders do not retain per-pass artifacts by default;
- store full-resolution color snapshots when tracing is enabled, so inspection
  views do not downsample and then scale images back up;
- compute a difference image for simple one-input/one-output color passes with
  matching dimensions;
- provide both absolute RGB difference and a boosted or heatmap visualization
  for subtle filters such as FXAA/SMAA;
- preview color, depth, stencil, and integer-id resources, and mark non-image
  resources such as shadow maps, motion vectors, and future cache artifacts as
  "metadata only" until a specialized viewer exists.
- attach cache metadata to resource snapshots so tools can distinguish
  non-cacheable resources from persistent-cache resources that were not served
  by a concrete cached artifact.

Shadow maps can be skipped for the first inspection UI. Later, a shadow-map
viewer can show depth as normalized grayscale, cascade coverage, texel snapping,
and per-light metadata. Until then, shadow resources should still appear in the
node details with descriptor, producer/consumer, cache status, and reason why no
image preview is available.

The trace must be tied to a specific executed plan. If the user changes graph
overrides, recompiles, resizes the target, or changes the scene after a render,
Modeler should mark the last trace as stale rather than showing old snapshots as
if they belonged to the new graph. The first compatibility check should compare
the effective execution plan: resources, pass ids, read/write edges, enabled
state, disabled behavior, scene/camera selection, and typed pass state. Scene
content changes that recompile to the same plan are now covered by a
conservative render-input fingerprint on the trace: camera pose/aspect, runtime
scene identity and coarse scene shape, background, and tonemap selection. Later
cache work can refine this into stable per-object and per-light revision
domains.

Preview rendering uses cloned render engines on worker threads, so the trace
cannot live only on a short-lived clone. Use a shared, thread-safe
`RenderGraphExecutionRecorder` or similar sink that cloned `GraphRenderEngine`
instances can write to, then publish the completed trace back to the UI thread
when the render finishes.

### Live execution state

Long-running renders should make the graph view show which node or nodes are
currently executing. This is separate from final trace inspection: it is a live
event stream used while the worker thread is still rendering.

Add an execution observer, tentatively:

```cpp
class RenderGraphExecutionObserver {
public:
  virtual void passStarted(RenderPassId id) = 0;
  virtual void passFinished(RenderPassId id) = 0;
  virtual void passFailed(RenderPassId id, std::string message) = 0;
};
```

The observer should use a set of running pass ids, not a single current node.
The first executor is mostly serial, but the UI and trace model should already
handle future parallel passes.

Modeler graph-view states:

- idle: normal node styling;
- running: highlighted outline/fill while the pass is executing;
- completed in the current render: optional muted success styling;
- skipped/disabled: disabled styling;
- failed: error styling with the pass failure message.

The inspector should clear live state on render start, cancellation, plan
changes, and failed renders. Updates from worker threads must be delivered to
Qt through queued UI-thread calls.

The same observer stream should feed future timing/profiling data and the
`RenderGraphExecutionTrace`, so avoid a UI-specific callback name such as
"highlight node."

## Parallel execution contract

The first implementation may execute serially. The pass contract should still
make future parallel execution straightforward.

Each pass must declare:

- all resource reads;
- all resource writes;
- side effects, if any;
- whether the executor can run concurrently with another pass on the same
  executor type;
- whether the pass has internal tiling or sample-level parallelism;
- whether it requires main-thread or GUI-thread execution;
- whether it imports or exports resources outside the frame.

A later scheduler can then run a pass when:

- all resource reads are available;
- no other running pass writes the same resource;
- executor concurrency limits permit it;
- imported resources are ready and exported-resource ownership is clear.

Natural parallelism examples:

- independent shadow maps;
- independent screen/render-texture sub-scenes;
- diagnostic AOV passes that read the same stable scene snapshot;
- postprocess branches that read the same input and write different outputs.

Pass-level parallelism remains inside executors: raytracer tiles, raster tiles,
path-tracing samples, shadow-map cascades, and image postprocess tiles can keep
their own worker strategies.

## Validation

Plan validation should catch:

- missing producer for a read;
- multiple writers to the same resource without an explicit resolve/composite;
- read/write cycles;
- resource dimension or sample-count mismatches;
- executor/resource-domain mismatch; ✅ **Done.** Current CPU-backed pass nodes
  reject `GPU` resources during plan validation until GPU-capable executors
  exist.
- pass disabled with no valid default or dependent-culling path;
- imported resource not provided; ✅ **Done for CPU color/depth/stencil/id
  inputs.** `GraphRenderEngine` now accepts bound imported/history color, depth,
  stencil, and integer-id resources and rejects unbound or unsupported external
  inputs before execution. rendercli can bind imported/history color, depth,
  stencil, object-id, and material-id resources from image files with
  `--render_graph_color_in`, `--render_graph_depth_in`,
  `--render_graph_stencil_in`, `--render_graph_object_id_in`, and
  `--render_graph_material_id_in` using `resource=file` syntax.
- exported resource not produced; ✅ **Done.** Plan validation now rejects
  exported resources with no declared producer.
- mirror/portal/screen recursion over the configured limit.

Validation errors should be human-readable because the graph is a teaching
object. Rendercli and Modeler should show why a plan is invalid.

## First implementation slice

Status: started. The initial foundation now lives in `include/engine/graph/`
and `src/engine/graph/`: render intent, scene selectors, resource descriptors,
CPU resource storage, pass declarations, virtual pass payloads with per-pass
execution context, plan validation, graph override disabling, text/DOT/JSON
plan export, a minimal compiler that emits a whole-frame beauty pass, optional
wireframe overlay pass, and tonemap/export pass, a graph engine facade that can
execute that dependency-ordered color chain through Raytracer/Rasterizer/Wireframe plus
PostProcess tonemapping, graph-visible typed pass state for replaying raster
beauty-pass controls, a display-buffer fast path for progressive simple previews,
dual HDR/display raytracer beauty output for progressive previews with
postprocess passes, typed raster preview shadow-pass state, optional scene JSON
render intent, and the textbook's render-graph volume.
Scene-feature expansion, arbitrary postprocess/composite execution, and real
parallel graph scheduling remain TODO.

Implement the smallest graph that proves the architecture:

1. Add `RenderResourceDescriptor`, `RenderResourceId`, and CPU storage for color
   and depth. ✅ Done for initial CPU color/depth/stencil/object-id storage,
   with execution-time `RenderResource` objects behind the serializable
   descriptors.
2. Add `RenderPassNode`, `RenderPlan`, plan validation, plan overrides, and
   text/DOT/JSON dumps. ✅ Done for the initial declarative graph model.
3. Add JSON-serializable `RenderIntent`, including default executor, view mode,
   shading profile, camera, and per-selector overrides for the same fields.
   ✅ Done. `RenderIntent::toJson()` / `fromJson(...)` own the scene JSON
   shape, `world::Scene` persists an optional top-level `renderIntent` block,
   and rendercli/Modeler layer temporary preview overrides on top.
4. Add `RenderGraphCompiler` so plans can be compiled, inspected, exported, and
   manipulated without rendering. ✅ Done for the first whole-frame beauty,
   optional wireframe overlay, and tonemap/export compiler; scene-feature
   expansion remains TODO.
5. Add `GraphRenderEngine` that can either compile from intent or execute a
   precompiled plan. ✅ Done for the first execution slice: whole-frame beauty
   passes backed by Raytracer, Rasterizer, or Wireframe, the first wireframe
   overlay pass, plus simple dependency-ordered color-resource chains. Simple beauty plus
   optional tonemap LDR output uses the wrapped engine's display-buffer render
   path, and raytracer beauty can write HDR graph color plus packed display
   pixels in one pass so postprocess graphs keep progressive preview updates.
6. Wrap existing whole-frame engines as pass executors:
   - `RaytraceBeautyPass`;
   - `RasterBeautyPass`;
   - `WireframeOverlayPass`;
   - `TonemapPass` or final copy/tonemap stage.
   ✅ Done for the current supported pass set: whole-frame raytracer,
   rasterizer, and wireframe beauty passes, a non-depth-aware wireframe overlay
   pass, plus tonemap now execute through virtual payload classes. A
   depth-aware overlay payload remains for the later depth/composite slice.
7. Add node disabling with `Passthrough` and `SubstituteDefault` for the first
   supported pass kinds. ✅ Done for disabled default substitution and color
   passthrough in the dependency-ordered graph engine.
8. Add rendercli graph inspection, graph-only compilation, graph JSON input,
   runtime intent overrides, and node-disabling flags. ✅ Partial: rendercli
   can compile/export text, DOT, and JSON plans, render through the graph by
   default, bypass the graph with `--direct_engine`, load and replay JSON
   plans, use scene JSON render intent, override the compiled default
   executor/view mode/shading profile/shading parameters/camera, append
   command-line view overrides with selectors, request the wireframe overlay
   intent, apply pass id/kind/executor/feature disable
   filters, serialize graph-backed raster
   beauty pass state for MSAA/post-AA/fixed-function/shadow controls,
   serialize graph-backed wireframe pass state for LOD, compile graph-visible
   depth, stencil, normal, object-id, material-id, world-position, and raster
   counter AOV views, and
   validate the manipulated plan.
   Selector-specific scene and command-line intent now fail compilation clearly
   instead of being silently ignored until scene-partitioning planners exist.
9. Add a Modeler graph inspector that compiles the plan before rendering and
   toggles nodes. ✅ Partial: Modeler now has a Render Graph dock that compiles
   the current live-preview plan before preview renders, lists the default
   beauty + tonemap pass chain and resource details, validates per-pass
   checkbox overrides, and feeds the effective valid plan back into the central
   graph-backed preview. The preview menu can request the wireframe overlay
   intent. The Graph tab renders a left-to-right graph view with selectable
   nodes and double-click pass toggles, preview renders highlight the currently
   executing graph node, post-render traces expose pass/resource previews, and
   the dock exports the effective graph as text, DOT, or JSON. The Groups tab
   can disable all passes matching a present kind, executor, or feature. The
   Preview View menu can compile the live preview as beauty, depth, stencil,
   normal, object-id, material-id, world-position, or raster counter AOV graphs. Per-selector intent
   controls remain TODO.
10. Ship one hybrid demo: raytraced room containing a rasterized or wireframe
   render-texture screen.

This first slice should not implement planar mirrors, portals, TAA, GPU
resources, or a parallel scheduler. It should create the stable substrate those
features need.

## Follow-up slices

### Render-to-texture surfaces

Add explicit screen/render-texture scene features and support nested scene
rendering into a texture consumed by a surface material.

### Per-scene and per-subtree style routing

Add selectors, tags/layers, and planner rules that can route a subset of the
scene through a different executor, view mode, shading profile, or camera, such
as wireframe diagnostic geometry, toon-shaded objects, or rasterized inset
content inside a
path-traced frame.

### Raster shadow maps as graph clients

Move raster preview shadows from internal rasterizer-only orchestration into
graph-level shadow-map resources and passes. ✅ **Done.** Graph-backed preview
renders include a `raster_preview_shadows` node and `preview_shadow_map`
resource that control whether raster beauty enables preview shadows. The shadow
node materializes a graph-visible CPU depth preview for the first
directional-light cascade and owns the full raster shadow-map collection as a
typed artifact. Raster beauty consumes that artifact instead of triggering its
own internal shadow build; lights without a directional map use a rasterizer
visibility fallback so the graph shadow toggle still affects point-lit previews.

### Persistent artifact cache

Add a graph-owned cache for `PersistentCache` resources. Start with shadow-map
artifacts once concrete shadow maps are externalized from `Rasterizer`, then
extend the same cache to reflection probes, irradiance caches, photon maps,
path-guiding data, terrain/volume intermediates, and acceleration data. ✅
**Partial.** `RenderGraphArtifactCache` now provides the clone-shared,
thread-safe cache container and typed cache keys; the raster shadow node now
stores and reuses a full directional shadow-map artifact with trace-visible
hit/stored metadata, and raster beauty consumes cache hits directly. Remaining
cache clients include reflection probes, irradiance caches, photon maps,
path-guiding data, terrain/volume intermediates, and acceleration data.

The first implementation may use conservative invalidation. ✅ **Partial.**
Raster preview shadow artifacts now use a pass-specific cache fingerprint, so
display-only changes such as tonemap swaps do not invalidate the cached depth
artifact. A later refinement should add scene/object revision domains so
light/occluder changes can be distinguished from unrelated scene edits.

### Execution trace and resource inspection

Record a per-render execution trace containing pass status, timings, supported
input/output resource snapshots, and per-pass difference images. Modeler should
let the user select a graph node after rendering and inspect `Input`, `Output`,
`Difference`, and `Metadata` tabs. CPU color, depth, stencil, and integer-id
snapshots should be previewable; shadow maps and other specialized resources
can remain metadata-only until custom viewers exist.

✅ **Partial.** `RenderGraphExecutionTrace` now records the executed plan, pass
status, elapsed time, full-resolution CPU color/depth/stencil/id snapshots, and
absolute plus boosted difference previews for simple one-input/one-output color
passes. `GraphRenderEngine` makes trace capture opt-in and shares the recorder
with render clones, so worker-thread preview renders can publish the completed
trace back to the original engine when the Modeler inspector enables tracing.
rendercli enables the same trace capture only for `--render_graph_trace_out`.
The shared recorder uses per-render sessions so a retired worker cannot
overwrite the latest trace after a newer render starts. The Modeler accepts a
completed trace only when its executed plan and render-input fingerprint match
the current effective plan, covering stale traces from resize, graph overrides,
compiled pass-state changes, camera edits, scene swaps, and tonemap changes.
Trace-owned resource lookup helpers feed the central trace preview for
resource-node selections. Resource snapshots also carry cache metadata;
rendercli trace JSON and the Modeler resource property editor show whether a
snapshot was not cacheable or was a persistent-cache resource that executed
without artifact reuse.

### Live graph execution highlighting

Expose pass-start/pass-finish/pass-fail events from graph execution so the
Modeler graph view can highlight the node or nodes currently running during
long renders. Keep the model as a set of active pass ids so the UI survives the
future parallel scheduler. ✅ **Done.** `GraphRenderEngine` exposes a live
execution observer copied into render snapshots, and the Modeler Render Graph
dock highlights running/completed/failed pass nodes during preview renders. Live
events carry render generations so retired preview workers do not update the
graph after a replacement render starts.

### Stencil/depth-aware composition

Add stencil and depth resources to the graph, then implement portal and planar
mirror raster previews through generated stencil, alternate-camera, and
composite passes. ✅ **Partial.** CPU graph storage already owns depth and
stencil resources, and `GraphRenderEngine` can now execute built-in
depth/stencil composite passes. A composite pass reads base color, foreground
color, an optional base/foreground depth pair, and an optional stencil mask,
then writes a color output using nearest finite foreground depth and nonzero
stencil coverage. Stencil AOV view mode also synthesizes graph-visible stencil
masks for primary-hit coverage, including a single-sample raster
stencil-marking payload that uses tessellated raster geometry. The compiler can
also synthesize a `stencil_composite` structural view that renders raster
beauty, wireframe foreground, a raster stencil AOV, a stencil composite pass,
and tonemap from scene intent, and the Modeler ships with a loadable scene for
that path. Portal/mirror pass synthesis, alternate-camera rendering, and
selector-derived stencil masks remain TODO.

### AOV exports

Add `depth`, `stencil`, `normal`, `world_position`, `object_id`,
`material_id`, and `motion_vector` resources as graph-visible outputs. ✅
**Partial.** The default view mode can now compile graph-visible `depth_aov`,
`stencil_aov`, `normal_aov`, `object_id_aov`, `material_id_aov`, and
`world_position_aov` resources with visualization passes, and rendercli accepts
`--render_graph_view depth`, `--render_graph_view stencil`,
`--render_graph_view stencil_composite`, `--render_graph_view normal`,
`--render_graph_view object_id`, `--render_graph_view material_id`,
`--render_graph_view world_position`, `--render_graph_view
raster_coverage_count`, `--render_graph_view raster_depth_test_count`,
`--render_graph_view raster_depth_pass_count`, `--render_graph_view
raster_shade_count`, and `--render_graph_view raster_color_write_count`.
Render intents now carry an `exportedAOVs` list, the compiler adds requested
AOV side branches through `RenderAOVDefinition` objects, and rendercli writes
multiple opt-in AOV preview files with repeated `--render_graph_aov_out
view=file` options. Rasterizer-backed depth, stencil, normal, object-id,
material-id, and world-position AOV payloads now use rasterizer diagnostic
buffers or a raster stencil-marking pass, so they reflect tessellated raster
geometry and raster pass state instead of analytic primary-ray intersections.
Raster counter AOV payloads extend that diagnostic path with graph-visible
heatmaps for per-pixel coverage, depth tests, depth passes, shading calls, and
color writes.
rendercli can also provide imported/history color, depth, stencil, object-id,
and material-id resources to replayed graph JSON with
`--render_graph_color_in`, `--render_graph_depth_in`,
`--render_graph_stencil_in`, `--render_graph_object_id_in`, and
`--render_graph_material_id_in` using `resource=file` syntax.
Motion vector resources remain TODO until graph history and previous-frame
inputs exist.

### Parallel scheduler

Replace single-threaded dependency execution with parallel dependency-ready
scheduling. Keep executor concurrency limits explicit. ✅ **Partial.**
`RenderPlan::executionStages()` now groups dependency-ready passes into stable
layers used by text exports and the Modeler graph layout; execution remains
serial until executor concurrency limits and worker scheduling are added.

### History resources

Add imported previous-frame resources for TAA, temporal denoising, motion blur,
and reprojection experiments.

### GPU resource domains

Introduce GPU-backed resource storage and executor adapters after the CPU graph
contracts are stable. GPU support should preserve the same high-level resource
descriptors and pass dependencies.

## Open design questions

These need review before implementation:

- What is the right first representation for tags/layers/scene views?
- What selector syntax should rendercli use for object ids, names, tags, layers,
  material roles, and subtrees?
- Should graph node ids be human-readable stable paths, numeric ids, or both?
- Should disabled-node overrides be persisted in the scene, the view settings,
  or only the current render session?
- How editable should exported graph JSON be before we risk treating it as a
  stable public scene format?

Resolved decisions:

- `GraphRenderEngine`, `RenderGraphCompiler`, graph pass payloads, and graph
  support types live in `include/engine/graph/` and `src/engine/graph/`.
- Pass execution payloads are virtual.
- `RenderIntent` is serializable in scene JSON, with rendercli/Modeler/API
  overrides layered on top for previews, education, and one-off renders.
- Named shading profiles, not fixed enum values like `Cartoon`, represent
  specific looks such as toon shading.
- Users author render intent, not graph topology. All normal render graph nodes
  are synthesized by the compiler from scene intent, scene analysis,
  preview/tool intent, and automatic feature expansion. Exported/replayed graph
  JSON and node disabling remain debugging and teaching surfaces, not the
  primary scene-authoring API.

## Documentation and testing expectations

Every implementation slice should consider:

- API docs for new public graph types;
- textbook coverage once graph behavior exists;
- a graph visualization widget or Modeler screenshot once the inspector exists;
- rendercli examples that dump text/DOT/JSON plans;
- rendered examples for hybrid frames and disabled-node comparisons;
- unit tests for plan validation and dependency ordering;
- integration tests for graph-rendered output matching existing direct engines
  when the graph contains only one beauty pass;
- changelog and roadmap updates when behavior lands.
