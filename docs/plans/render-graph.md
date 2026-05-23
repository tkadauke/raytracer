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

Long term, the normal render path should go through the graph because that is
where AOVs, feature expansion, overlays, render-to-texture surfaces, and
educational inspection live. Direct engines remain available when the caller
explicitly asks for them, such as rendercli's "only use raytracer" mode or a
focused unit test.

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

`RenderIntent` is the user-facing request. It should be serializable in scene
JSON so a scene can describe its intended final render, educational views, and
preview strategies. Rendercli, Modeler, tests, and direct API callers can layer
temporary overrides on top of the scene intent without mutating the scene file.

The effective intent is built in layers:

```text
scene JSON RenderIntent
  -> application defaults
  -> rendercli / Modeler / API overrides
  -> compiled RenderPlan
```

This lets Modeler render a lower-fidelity realtime-ish preview of the final
image while preserving the final render description in the scene.

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
  ObjectId
};

struct ShadingProfileRef {
  std::string name;
  QJsonObject parameters;
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
};

struct RenderIntent {
  RenderExecutorPreference defaultExecutor;
  RenderViewMode defaultViewMode = RenderViewMode::Beauty;
  ShadingProfileRef defaultShadingProfile{"default", {}};
  std::optional<RenderCameraRef> defaultCamera;
  bool enableAutomaticFeatures = true;
  bool enableWireframeOverlay = false;
  bool enablePreviewShadows = true;
  std::vector<RenderViewOverride> viewOverrides;
  std::vector<RenderAOV> requestedAOVs;
  RenderGraphOverrides overrides;
};
```

The exact API can evolve, but it should stay high-level. It should not require
the caller to manually describe "stencil first, then reflected camera, then
composite" for a planar mirror.

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

  RenderPlan withOverrides(RenderGraphOverrides overrides) const;
};
```

The plan should be deterministic for a given scene snapshot and intent. Stable
node ids matter because UI controls, rendercli filters, tests, and docs should
be able to reference specific nodes.

Plan manipulation should operate on the compiled plan without requiring a render
to happen. Disabling a node, switching a pass to a fallback view mode, or
changing a node's enabled state should be reflected in validation and
visualization before execution.

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
  std::vector<ResourceRead> reads;
  std::vector<ResourceWrite> writes;
  SceneView sceneView;
  std::shared_ptr<render::Camera> camera;
  DisabledBehavior disabledBehavior;
  bool enabled = true;
  bool hasExternalSideEffects = false;
  bool canRunConcurrently = true;
};
```

The first implementation can execute pass nodes serially. The declaration must
still be strong enough for a later scheduler to run independent nodes in
parallel.

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
  explicit layers into the final image.
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
RaytraceBeautyPass -> main_color, main_depth
WireframeOverlayPass reads main_color, main_depth -> overlay_color
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
- pass details: executor, reads, writes, scene view, camera, disabled behavior;
- resource details: type, size, format, producer, consumers;
- click a resource to preview it where possible;
- validate the manipulated graph before rendering;
- export graph as DOT/JSON/text.

The UI should not require deep backend mutation. It should pass graph overrides
to the planner or manipulate a compiled plan, then re-render. This supports the
educational workflow where a user compiles the graph, disables or changes nodes,
and only then executes the frame.

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
- executor/resource-domain mismatch;
- pass disabled with no valid default or dependent-culling path;
- imported resource not provided;
- exported resource not produced;
- mirror/portal/screen recursion over the configured limit.

Validation errors should be human-readable because the graph is a teaching
object. Rendercli and Modeler should show why a plan is invalid.

## First implementation slice

Status: started. The initial foundation now lives in `include/engine/graph/`
and `src/engine/graph/`: render intent, scene selectors, resource descriptors,
CPU resource storage, pass declarations, virtual pass payloads, plan validation,
graph override disabling, text/DOT/JSON plan export, a minimal compiler that
emits one whole-frame beauty pass, a graph engine facade that can execute that
single pass through Raytracer/Rasterizer/Wireframe, and the textbook's
render-graph volume. Multi-pass compilation, postprocess/composite execution,
scene JSON intent, and real graph scheduling remain TODO.

Implement the smallest graph that proves the architecture:

1. Add `RenderResourceDescriptor`, `RenderResourceId`, and CPU storage for color
   and depth. ✅ Done for initial CPU color/depth/stencil/object-id storage.
2. Add `RenderPassNode`, `RenderPlan`, plan validation, plan overrides, and
   text/DOT/JSON dumps. ✅ Done for the initial declarative graph model.
3. Add JSON-serializable `RenderIntent`, including default executor, view mode,
   shading profile, camera, and per-selector overrides for the same fields.
   ✅ Core intent data is defined; scene JSON read/write is TODO.
4. Add `RenderGraphCompiler` so plans can be compiled, inspected, exported, and
   manipulated without rendering. ✅ Done for the first whole-frame beauty
   pass compiler; scene-feature expansion remains TODO.
5. Add `GraphRenderEngine` that can either compile from intent or execute a
   precompiled plan. ✅ Done for the first execution slice: exactly one enabled
   beauty pass backed by Raytracer, Rasterizer, or Wireframe.
6. Wrap existing whole-frame engines as pass executors:
   - `RaytraceBeautyPass`;
   - `RasterBeautyPass`;
   - `WireframeOverlayPass`;
   - `TonemapPass` or final copy/tonemap stage.
   ✅ Partial: whole-frame raytracer, rasterizer, and wireframe beauty passes
   are selected by `GraphRenderEngine`; named payload classes and tonemap
   execution remain TODO.
7. Add node disabling with `Passthrough` and `SubstituteDefault` for the first
   supported pass kinds.
8. Add rendercli graph inspection, graph-only compilation, graph JSON input,
   runtime intent overrides, and node-disabling flags.
9. Add a Modeler graph inspector that compiles the plan before rendering and
   toggles nodes.
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
graph-level shadow-map resources and passes.

### Stencil/depth-aware composition

Add stencil and depth resources to the graph, then implement portal and planar
mirror raster previews through generated stencil, alternate-camera, and
composite passes.

### AOV exports

Add `depth`, `normal`, `world_position`, `object_id`, `material_id`, and
`motion_vector` resources as graph-visible outputs.

### Parallel scheduler

Replace serial graph execution with dependency-ready scheduling. Keep executor
concurrency limits explicit.

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
- How should Modeler present automatic nodes versus user-requested nodes?
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
