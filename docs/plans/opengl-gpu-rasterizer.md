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
  state requests MSAA.
- ~~Bind graph raster pass state for color write masks.~~ ✅ **Done.** OpenGL
  raster draw calls now apply graph-derived RGB color write masks after the
  background clear.
- Bind remaining graph raster pass state for other fixed-function controls.
- ~~Render into an FBO color attachment and depth attachment.~~ ✅ **Done.**
  The backend clears the offscreen framebuffer, depth-tests triangles, and
  draws material-albedo color.
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

- Upload image textures used by imported glTF/LDraw/OpenSCAD mesh materials.
- Implement base-color texture sampling with nearest/linear and wrap/clamp
  modes where supported by the existing material model.
- Preserve current material fallback warnings.
- Add a textured glTF fixture or generated smoke asset for rendercli coverage.

Acceptance:

- The sloth-style glTF path renders textured geometry through GPU raster.
- Missing or unsupported material features remain visible as diagnostics.

## Phase 3 - graph resources without eager readback

Tasks:

- Let graph resource storage keep OpenGL textures/renderbuffers resident across
  compatible GPU passes.
- Add explicit readback pass/operation for final image output, trace, and AOV
  export.
- Record readback cost in graph trace metadata.
- Validate CPU/GPU resource domain transitions.

Acceptance:

- Multi-pass GPU-compatible plans avoid readback between every pass.
- Trace/export still work when requested.

## Phase 4 - fixed-function state coverage

Tasks:

- ~~Report unsupported fixed-function state before execution.~~ ✅ **Done.**
  OpenGL raster passes now reject unsupported raster pass postprocess AA,
  and depth bias with explicit diagnostics instead of silently ignoring the
  compiled graph state.
- ~~Add fixed-function blending support.~~ ✅ **Done.** OpenGL raster draw calls
  now apply graph-derived blend enable, factors, operation, and constant color
  state.
- ~~Add fixed-function alpha-test support.~~ ✅ **Done.** OpenGL raster draw
  calls now carry material alpha into the vertex stream and discard fragments
  through graph-derived alpha-test enable/function/reference state.
- ~~Add stencil AOV support where it maps cleanly to OpenGL state.~~ ✅
  **Done.** OpenGL raster draw calls now apply the stencil state used by the
  graph stencil AOV pass and read the stencil attachment back into the
  graph-visible CPU resource.
- Add generalized stencil pass state once high-level render intents need
  explicit stencil tests beyond diagnostic/AOV generation.
- ~~Add object/material ID output with integer attachments or a fallback
  path.~~ ✅ **Done.** Object/material ID graph AOVs selected with the OpenGL
  raster backend now use the software raster diagnostic fallback so IDs stay
  exact until the GPU path has integer attachment support; trace messages
  identify that mixed execution path.
- ~~Allow graph shadow-map plans to execute while OpenGL shadow sampling is
  pending.~~ ✅ **Done.** The CPU graph shadow pass still materializes and
  caches shadow-map resources for OpenGL-selected raster plans, and OpenGL
  beauty records a trace message when it renders without consuming the shadow
  resource.
- Add OpenGL shadow-map sampling after the GPU raster path has explicit shadow
  texture binding and shader lighting state.

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
