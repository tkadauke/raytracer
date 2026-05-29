# OpenGL GPU rasterizer plan - May 2026

> **Scope:** add an OpenGL-backed raster executor that runs through the render
> graph, writes graph resources, and can accelerate raster passes without
> becoming a separate viewport/rendering architecture.
>
> **Status:** planning. This plan is independent of CPU culling work. CPU
> culling can improve what the GPU receives, but the first GPU executor should
> be useful without it.
>
> **Related plans:** `docs/plans/render-graph.md` owns graph compilation,
> resources, trace, and inspection. `docs/plans/cpu-culling-graph-pass.md`
> tracks optional CPU visibility preprocessing.

---

## Goals

The GPU rasterizer should be another graph executor, not a replacement for the
render graph. The graph still decides which passes exist, which viewpoints are
needed, how portals/mirrors/render-to-texture views are expanded, and how
intermediate resources connect. OpenGL only executes pass payloads that map
cleanly to draw calls, framebuffer objects, textures, shaders, and GPU state.

The first version should:

- execute the existing raster beauty pass through OpenGL where possible;
- consume explicit graph/raster pass state rather than private UI state;
- write graph-visible color and depth resources;
- support rendercli and Modeler through the same graph path;
- keep CPU readback available for final image output, graph trace, AOV export,
  and tests;
- fall back to CPU raster or fail clearly when no OpenGL context is available;
- preserve the CPU rasterizer as the reference, teaching, and headless backend.

## Non-goals

- Do not create a separate OpenGL viewport that bypasses render intent or graph
  compilation.
- Do not attempt to support every rasterizer feature in the first slice.
- Do not require GPU resources for every graph resource type. CPU storage stays
  valid.
- Do not make OpenGL responsible for portals, mirrors, recursive views, or
  culling policy. The graph/compiler owns those decisions.
- Do not depend on CPU culling being present.

## Architecture

The intended path is:

```text
world scene + render intent
  -> RenderGraphCompiler
  -> RenderPlan with raster pass state
  -> GraphRenderEngine
  -> OpenGL raster executor for supported passes
  -> graph color/depth resources
```

Add an executor, tentatively:

```cpp
namespace engine::raster {
  class OpenGLRasterizer;
}
```

It should implement the same high-level rendering contract as the CPU
`Rasterizer`, but receive its work from graph pass state. A pass can select the
GPU executor explicitly or through an automatic backend preference such as
`raster_backend=gpu`.

The graph resource layer should distinguish logical resources from storage:

- color resource: CPU `Buffer<Colord>` or OpenGL texture/renderbuffer;
- depth resource: CPU depth buffer or OpenGL depth texture/renderbuffer;
- stencil resource: CPU stencil buffer or OpenGL stencil attachment;
- object/material ID resources: CPU integer buffers or GPU integer textures;
- imported/readback resources: explicit transfer boundary, not implicit state.

The initial implementation can read back after each GPU pass if that keeps the
integration small. A later optimization can keep GPU resources resident across
passes and read back only when final output, trace, or rendercli export needs
CPU data.

## Graph responsibilities

The graph remains responsible for features OpenGL does not model by itself:

- **Portals:** compile portal masks/stencil, portal-camera passes, and
  composites.
- **Flat mirrors:** compile reflected-camera render passes and mirror surface
  sampling/compositing.
- **Curved mirrors/environment mapping:** compile probe/cubemap passes and feed
  them to reflective material/pass state.
- **Recursive effects:** expand bounded recursion into explicit pass nodes.
- **Hybrid frames:** mix raytracer, CPU raster, GPU raster, wireframe,
  postprocess, and composite executors in one plan.
- **Inspection:** expose pass state, resources, timings, trace images, and
  readback decisions.

OpenGL executes draw work for the pass it is given. It must not invent hidden
scene behavior that the graph cannot inspect.

## Capability model

Each raster pass payload should advertise whether it can run on the OpenGL
executor. The first supported set should be narrow and explicit:

- opaque mesh triangles;
- vertex positions, normals, UVs;
- matte/base-color material factor;
- base-color texture sampling where image textures are available;
- ambient and directional light shading;
- color/depth output;
- viewport/scissor and face culling;
- MSAA if FBO support is straightforward.

Unsupported initial features should fall back to CPU raster or report a clear
diagnostic:

- alpha test/blend/stencil write/read;
- object ID and material ID AOVs;
- shadow maps;
- postprocess AA;
- line/curve overlays;
- unusual texture mappings;
- render graph trace snapshots without readback.

As features land, the capability surface should be tested by pass state, not by
ad hoc scene type checks.

## Phase 0 - capability and context skeleton

Tasks:

- ~~Add an OpenGL capability probe that can run in Modeler and rendercli.~~ ✅
  **Done.** `OpenGLOffscreenContext::probe()` reports either context details or
  one actionable capability/bootstrap error.
- ~~Create a reusable offscreen Qt/OpenGL context and FBO owner.~~ ✅ **Done.**
  The OpenGL raster shell now attempts a Qt offscreen context, surface, and
  depth/stencil FBO before stopping at the missing draw path.
- ~~Add a small `OpenGLRasterizer` executor shell with deterministic errors when
  context creation fails.~~ ✅ **Done.** The first shell is graph-selectable and
  reports one actionable unavailable-backend error until mesh drawing lands.
- ~~Add render intent / rendercli spelling for selecting GPU raster backend,
  without changing the default backend yet.~~ ✅ **Done.** `--raster_backend
  opengl` and `gpu` serialize as typed raster pass execution state; `cpu`
  remains the default.
- ~~Teach rendercli to bootstrap a `QGuiApplication` or offscreen platform mode
  when GPU raster is requested, so command-line GPU renders can create the
  context instead of reporting the current application-bootstrap error.~~ ✅
  **Done.** rendercli pre-scans `--raster_backend opengl|gpu`, starts a
  `QGuiApplication` only for that path, and defaults the command-line GPU run
  to Qt's offscreen platform unless the caller has already selected a platform.
- ~~Add Modeler UI to select GPU raster preview backend.~~ ✅ **Done.** The
  scene Render Settings property editor, final Render window, and preview
  override menu can all compile Rasterizer passes with the OpenGL backend.
- ~~Gate or annotate the Modeler OpenGL choice with the capability probe and
  missing-draw-path status once the first draw path is close enough to exercise.~~ ✅
  **Done.** The final Render window shows a backend-status row for OpenGL,
  Render Settings explains that OpenGL is experimental, and the preview menu
  exposes the same shared OpenGL raster status. Headless Cocoa context probes
  are still rejected before entering the Qt call that can crash on macOS;
  Modeler is allowed to attempt the Cocoa path so the visible preview/render
  UI can exercise the backend.

Acceptance:

- `rendercli --engine raster --raster_backend gpu` either renders a trivial
  scene or reports one actionable context/capability error.
- Modeler can show whether the GPU backend is available.
- CPU raster remains the default.

## Phase 1 - color/depth mesh beauty pass

Tasks:

- ~~Traverse the runtime scene leaves and prepare mesh geometry for VBO/IBO
  buffers.~~ ✅ **Done.** The OpenGL raster path now reuses the software raster
  front end to produce a GPU-ready screen-space vertex/index buffer with the
  same camera setup, clipping, culling, material albedo, cancellation, and LOD
  behavior.
- ~~Upload prepared mesh geometry to VBO/IBO buffers.~~ ✅ **Done.** The
  initial OpenGL pass allocates Qt OpenGL vertex and index buffers from the
  prepared mesh.
- ~~Compile a minimal shader for matte/base-color shading.~~ ✅ **Done.** The
  first shader draws interpolated material albedo so visible geometry appears
  before direct lighting lands.
- ~~Bind graph raster pass state for camera and LOD, and enable depth test.~~ ✅
  **Done.** Camera setup and LOD are shared with the software front end, and
  the OpenGL pass enables depth testing.
- ~~Bind graph raster pass state for viewport/scissor and culling
  overrides.~~ ✅ **Done.** The OpenGL raster backend now carries the same
  graph-derived viewport, scissor, and culling state as the CPU rasterizer for
  the initial mesh beauty path.
- ~~Bind graph raster pass state for MSAA sample count.~~ ✅ **Done.** The
  OpenGL raster backend now allocates a multisample framebuffer when graph pass
  state requests MSAA, and OpenGL raster plans default MSAA shading state to
  `per_fragment` so graph state matches the current GPU shading path.
- ~~Bind graph raster pass state for color write masks.~~ ✅ **Done.** OpenGL
  raster draw calls now apply graph-derived RGB color write masks after the
  background clear.
- ~~Bind graph raster pass state for color attachment controls.~~ ✅ **Done.**
  Raster framebuffer state now serializes color load/store operations, OpenGL
  honors color-store discard during readback, and color-load requests fail
  clearly until GPU resource residency can seed the attachment.
- ~~Bind graph raster pass state for depth compare/write controls.~~ ✅
  **Done.** Raster framebuffer state now serializes depth compare, clear,
  load/store, and write controls; OpenGL binds compare/write/clear/store state
  and rejects depth attachment load until graph resource residency exists.
- Bind remaining graph raster pass state for depth/stencil attachment-load
  controls that still need explicit GPU resource-domain support.
- ~~Render into an FBO color attachment and depth attachment.~~ ✅ **Done.**
  The backend clears the offscreen framebuffer, depth-tests triangles, and
  draws material-albedo color.
- ~~Apply ambient and direct diffuse lighting for the initial mesh path.~~ ✅
  **Done.** OpenGL raster mesh vertices now carry the same scene ambient term
  and per-light diffuse factor used by the CPU raster material path, leaving
  shadow masking for a later slice.
- ~~Apply local Phong specular highlights for the initial mesh path.~~ ✅
  **Done.** OpenGL raster mesh vertices now carry view-dependent specular
  contributions for materials with specular color/coefficient/exponent state,
  leaving shadow masking for a later slice.
- ~~Move directional diffuse/specular lighting from vertex interpolation to
  fragment shading.~~ ✅ **Done.** OpenGL raster batches now upload directional
  light radiance/direction and per-material lighting coefficients so smooth
  assets use fragment-normal lighting instead of Gouraud-style interpolation.
- ~~Move point diffuse/specular lighting from vertex interpolation to fragment
  shading.~~ ✅ **Done.** Runtime lights now expose shader-compatible
  positional light state through the `Light` hierarchy, and OpenGL raster
  batches upload point-light position/radiance for fragment-normal lighting
  when shadow-map fallback is not required.
- ~~Read back color for final output~~ ✅ **Done.** Color readback now fills
  the render target.
- ~~Read back depth for the raster depth AOV path.~~ ✅ **Done.** The OpenGL
  backend can now materialize graph-visible CPU depth previews for depth AOV
  renders.
- Cache GPU buffers per immutable mesh payload where possible.

Acceptance:

- A simple mesh scene renders through the graph using the GPU backend.
- CPU raster and GPU raster produce the same dimensions and broadly similar
  output for opaque matte fixtures.
- The graph inspector still shows the same pass node and resource edges.

## Phase 2 - materials and texture parity

Tasks:

- ~~Carry UVs and perspective interpolation data through the OpenGL vertex
  stream for shader-side texture evaluation.~~ ✅ **Done.** OpenGL raster
  vertices now retain clip `w`, UV coordinates, and a shader albedo mode so
  supported procedural texture modes can be evaluated per fragment instead of
  pre-sampled as vertex colors.
- ~~Upload direct UV-mapped `ImageTexture` sources used by imported
  glTF/LDraw/OpenSCAD mesh materials.~~ ✅ **Done.** OpenGL raster batches now
  upload direct `ImageTexture` sources, including their generated mip levels,
  and reuse one GL texture per image within the draw pass. Other wrapped
  texture stacks still use the vertex-color fallback until material texture
  graphs have GPU descriptors.
- ~~Preserve glTF base-color texture tints without falling back to vertex-color
  sampling.~~ ✅ **Done.** glTF tinting now uses a shared `TintedTexture`, and
  OpenGL raster albedo state carries the tint beside supported direct image
  sources.
- ~~Implement base-color texture sampling with nearest/linear and wrap/clamp
  modes where supported by the existing material model.~~ ✅ **Done.** The
  OpenGL shader now samples UV-mapped image albedo with the runtime
  nearest/bilinear/mipmap filter and repeat/clamp wrap policy carried by
  `ImageTexture`.
- ~~Evaluate direct UV checker textures in the shader where the existing
  material model exposes constant checker colors.~~ ✅ **Done.** OpenGL raster
  batches now carry UV checker scale and child colors so checkerboard albedo is
  selected per fragment instead of pre-sampled at vertices.
- ~~Evaluate direct planar checker textures in the shader where the existing
  material model exposes constant checker colors.~~ ✅ **Done.** OpenGL raster
  batches now carry planar checker child colors so large floor checkers are
  evaluated from fragment world position instead of interpolated vertex colors.
- ~~Preserve current material fallback warnings.~~ ✅ **Done.** rendercli now
  checks that OpenGL-selected raster graph plans still emit recursive-material
  fallback warnings before execution, even on hosts where the OpenGL pass later
  reports an unavailable offscreen context.
- ~~Add a textured glTF fixture or generated smoke asset for rendercli
  coverage.~~ ✅ **Done.** `rendercli_raster` now configures a tiny textured
  glTF triangle and exercises it with `--raster_backend gpu`, accepting the
  normal OpenGL capability error on hosts without an offscreen context.

Acceptance:

- The sloth-style glTF path renders textured geometry through GPU raster.
- Missing or unsupported material features remain visible as diagnostics.

## Phase 3 - graph resources without eager readback

Tasks:

- Let graph resource storage keep OpenGL textures/renderbuffers resident across
  compatible GPU passes. The CPU resource storage layer now tracks
  backend-provided GPU residency metadata for descriptor-only resources and
  surfaces that metadata in graph trace snapshots, providing the first storage
  hook for resident OpenGL textures/renderbuffers before concrete pass reuse
  lands. Render pass nodes now also declare their supported resource domains,
  so GPU-compatible pass chains can validate explicitly instead of relying on a
  hard-coded CPU-only rule; text and DOT graph exports surface that pass-domain
  support alongside resource domains for inspection.
- Add explicit readback pass/operation for final image output, trace, and AOV
  export. ✅ **Partial.** `RenderPassKind::Readback` now exposes the transfer
  boundary as a graph node and executes CPU-materialized copies through
  `RenderResource` instance methods. Descriptor-only GPU inputs still fail
  clearly until concrete OpenGL texture/renderbuffer residency can perform the
  real transfer. OpenGL raster beauty compilation now inserts `beauty_readback`
  before tonemap, so the final-output transfer boundary is visible in graph
  exports and the Modeler graph view even while the current OpenGL pass still
  eagerly materializes CPU color. OpenGL-backed raster AOV view compilation
  now inserts the same kind of readback node before visualization, and exported
  AOV side branches route their transient producer resource through a readback
  node before publishing the exported resource. OpenGL-backed stencil-composite
  plans also route their internal raster base color and stencil mask through
  readback nodes before the composite pass consumes them, keeping exported
  stencil AOV side branches independent from the structural mask.
- ~~Record readback cost in graph trace metadata.~~ ✅ **Done.** OpenGL raster
  beauty/depth/stencil executions now append trace messages that report which
  attachments were copied back to CPU buffers and how long that eager readback
  took.
- ~~Record mesh-preparation cost in graph trace metadata.~~ ✅ **Done.**
  OpenGL raster traces now include how many triangles were prepared for the GPU
  mesh stream and how long that CPU preparation took.
- ~~Record OpenGL setup/draw submission cost in graph trace metadata.~~ ✅
  **Done.** OpenGL raster traces now report how long the offscreen draw block
  spent clearing state, compiling/binding shaders, uploading buffers/textures,
  and submitting draw calls before CPU readback begins.
- ~~Validate CPU/GPU resource domain transitions.~~ ✅ **Done.** Render plan
  validation rejects CPU-only passes that read or write GPU-domain resources,
  and resource storage keeps GPU descriptors as metadata-only until explicit
  readback or GPU residency exists.

Acceptance:

- Multi-pass GPU-compatible plans avoid readback between every pass.
- Trace/export still work when requested.

## Phase 4 - fixed-function state coverage

Tasks:

- ~~Report unsupported fixed-function state before execution.~~ ✅ **Done.**
  OpenGL raster passes now reject unsupported raster pass postprocess AA and
  attachment-load state with explicit diagnostics instead of silently ignoring
  the compiled graph state.
- ~~Add fixed-function blending support.~~ ✅ **Done.** OpenGL raster draw calls
  now apply graph-derived blend enable, factors, operation, and constant color
  state.
- ~~Add fixed-function alpha-test support.~~ ✅ **Done.** OpenGL raster draw
  calls now carry material alpha into the vertex stream and discard fragments
  through graph-derived alpha-test enable/function/reference state.
- ~~Add depth-bias support.~~ ✅ **Done.** OpenGL raster mesh preparation now
  applies graph-derived signed depth bias to device depth, matching the CPU
  rasterizer's pass-state contract for depth comparisons and writes.
- ~~Add stencil AOV support where it maps cleanly to OpenGL state.~~ ✅
  **Done.** OpenGL raster draw calls now apply the stencil state used by the
  graph stencil AOV pass and read the stencil attachment back into the
  graph-visible CPU resource.
- ~~Add generalized stencil pass state once high-level render intents need
  explicit stencil tests beyond diagnostic/AOV generation.~~ ✅ **Done.**
  Raster framebuffer state now serializes stencil test, reference/masks,
  load/store, and operations; the stencil AOV definition configures that typed
  state on its producer pass instead of hard-coding it inside the payload.
- ~~Add object/material ID output with integer attachments or a fallback
  path.~~ ✅ **Done.** Object/material ID graph AOVs selected with the OpenGL
  raster backend now use the software raster diagnostic fallback so IDs stay
  exact until the GPU path has integer attachment support; trace messages
  identify that mixed execution path.
- ~~Keep graph diagnostic AOVs available while GPU attachments are incomplete.~~
  ✅ **Done.** Normal, world-position, and raster counter AOVs selected with
  the OpenGL backend now use the software raster diagnostic fallback and record
  the mixed path in graph traces.
- ~~Allow graph shadow-map plans to execute while OpenGL shadow sampling is
  pending.~~ ✅ **Done.** The CPU graph shadow pass still materializes and
  caches shadow-map resources for OpenGL-selected raster plans, and OpenGL
  beauty records a trace message when it renders without consuming the shadow
  resource.
- ~~Consume graph shadow-map artifacts in the current OpenGL mesh-preparation
  path.~~ ✅ **Done.** OpenGL raster beauty now receives graph-owned shadow-map
  artifacts and applies their visibility while preparing the lit mesh stream, so
  shadow-enabled raster plans no longer render as fully unshadowed on the GPU
  backend.
- ~~Separate ambient and direct lighting terms in the OpenGL mesh stream.~~ ✅
  **Done.** OpenGL vertices now carry ambient and direct-light factors
  separately, leaving the shader with a direct-light channel for later
  shadow-texture sampling without changing the current image equation.
- ~~Add shader-side OpenGL shadow-map sampling after the GPU raster path has
  explicit shadow texture binding and shader lighting state.~~ ✅ **Done.** The
  first supported OpenGL shadow subset samples one hard/PCF-filtered
  directional cascade in the fragment shader when that shadow map owns the
  scene's single direct light; unsupported shadow configurations stay on
  CPU-prepared visibility. The shadow-map
  collection now exposes its directional maps directly so the GPU binding path
  can enumerate cascades without per-light lookup, and the OpenGL mesh stream
  now carries world position plus lighting normal for per-fragment
  shadow projection. Directional shadow cameras also expose their fitted
  origin, basis, and half-extent so shader uniforms can project those fragments
  into light-space; directional shadow maps expose bias and filter policy so
  the GPU path can decide when shader sampling matches the graph state. An
  `OpenGLShadowSamplingPlan` now captures the first supported subset: one
  directional cascade with constant bias and hard shadows or PCF radius up to
  4. Graph traces record
  whether a shadow-enabled OpenGL pass matches that subset or still falls back
  to CPU-prepared shadow visibility. `OpenGLShadowTextureData` now converts the
  eligible shadow depth buffer into normalized RGBA float texels with a
  no-occluder sentinel and carries the matching directional-light projection
  constants. Eligible OpenGL shadow passes now upload that payload as a
  nearest-filtered shadow texture and record the prepared texture dimensions in
  graph traces. The sampling plan also models the current lighting-channel
  limitation explicitly: shader-side shadowing is only allowed when the
  eligible shadow map owns the scene's single direct light. That guarded path
  now disables CPU-prepared shadow visibility for the matching mesh stream and
  lets the fragment shader sample the uploaded shadow texture per fragment.

Acceptance:

- Existing raster output-state rendercli tests can opt into GPU backend where
  the platform supports it.
- Unsupported state is reported by pass capability checks, not by undefined
  image differences.

## Phase 5 - portals, mirrors, and render-to-texture

Tasks:

- Add graph compiler cases for flat mirror and portal render-to-texture passes.
- Use OpenGL executor only for the raster passes inside those compiled
  sequences.
- Add graph resources for offscreen color/depth inputs sampled by later passes.
- Bound recursion explicitly in render intent/graph compilation.

Acceptance:

- A portal or flat mirror scene compiles to multiple graph nodes.
- The graph view exposes offscreen resources and dependencies.
- CPU and GPU raster backends can execute the same high-level plan where their
  capabilities overlap.

## Testing and diagnostics

Required checks for each phase:

- unit tests for capability selection and resource-domain validation;
- rendercli functional tests that skip cleanly when OpenGL is unavailable;
- Modeler UI smoke where practical;
- graph export tests showing executor/backend state in compiled pass payloads;
- image smoke tests for dimensions, nonempty output, and coarse visual
  differences only where deterministic;
- trace metadata for GPU timing, readback timing, and fallback reason.

## Open questions

- Should the first public spelling be `--raster_backend gpu` or
  `--render_graph_executor opengl_rasterizer`?
- Should OpenGL be a direct engine option, a graph executor option, or both?
- How much platform-specific context code belongs in `engine/raster` versus a
  small `engine/gpu` utility module?
- Which CI environment can reliably exercise OpenGL, if any?
