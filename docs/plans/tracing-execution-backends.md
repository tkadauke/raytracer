# Tracing execution backends plan - June 2026

> **Scope:** make the repository a playground for tracing algorithms that can
> run on CPU, hybrid CPU/GPU, and full GPU execution backends. The current
> GPU-assisted wavefront-intersection work becomes the first reusable backend
> service, not the final goal.
>
> **Status:** active parent plan. This supersedes GPU intersection as the
> top-level objective. The first implementation wave is mostly complete:
> intersection-service consolidation, cross-backend parity fixtures/helpers,
> compiled tracing scene records, deterministic GPU sample streams, CPU/Metal/
> Vulkan accumulation surfaces, and execution capability diagnostics have
> landed. `docs/plans/gpu-wavefront-intersection.md` remains active as the
> intersection-service slice. `docs/plans/wavefront-and-path-tracing.md`
> remains the CPU wavefront/path-tracing schedule plan.

---

## Vision

The repository should become a collection of rendering engines, tracing
algorithms, schedules, and execution strategies that can be selected,
compared, inspected, and extended. The important user-facing idea is not "use
the GPU when possible"; it is:

- the same scene can be rendered with different tracing algorithms;
- the same algorithm can use different schedules;
- the same schedule can run on different execution backends;
- hybrid backends stay useful and visible when a full GPU implementation is not
  available;
- every fallback is explicit in render graph trace, rendercli metrics, and
  Modeler inspection.

GPU intersection alone is valuable for real-time shadows, visibility queries,
ambient occlusion, picking, hybrid raster/tracing effects, and later full GPU
tracing. It should be preserved as a reusable capability. It should not be the
end state for GPU tracing.

---

## Current State

### Already Available

- CPU Whitted ray tracing through `engine::raytracer::Raytracer` and
  `render::WhittedIntegrator`.
- CPU scalar path tracing through `render::PathTracingIntegrator`.
- CPU wavefront/depth-major path tracing and Whitted batches through
  `engine::wavefront::WavefrontRaytracer`.
- Render intent and render graph state can choose ray-family engine,
  integrator, sampling, path-tracing schedule, convergence, adaptive sampling,
  denoising, and the wavefront intersection backend.
- `--wavefront_intersection_backend auto|cpu|gpu` is a graph-visible intent
  field.
- A restricted compiled intersection scene exists for supported geometry:
  triangle/mesh triangles, imported/static `MeshPrimitive` geometry that
  flattens to mesh triangles, finite-width `Curve` ribbon/tube tessellation,
  sphere, plane, rectangle, disk, OpenCylinder, Torus, and static transforms.
- Boolean/convex CSG primitives (`Difference`, `Intersection`, `Union`,
  `ConvexHull`, and `MinkowskiSum`) emit explicit unsupported intersection
  records instead of flattening supported children and losing CSG semantics.
- Compiled diffuse path-loop support diagnostics report a concrete unsupported
  primitive, material, texture, or light reason instead of a generic
  compiled-scene failure.
- The packed CPU intersection path uses the same GPU-style ray, hit, occlusion,
  BVH, primitive, payload, and transform records as the platform backend
  contract.
- Optional Metal and Vulkan presets exist:
  - `release-metal-wavefront`;
  - `benchmark-metal-wavefront`;
  - `release-vulkan-wavefront`;
  - `benchmark-vulkan-wavefront`.
- Metal/Vulkan basic closest-hit and any-hit render-path kernels exist for the
  restricted intersection subset when the platform backend is enabled and the
  scene is eligible.
- Direct-light visibility can be grouped into backend-owned any-hit batches in
  the wavefront path tracer.
- `render::IntersectionService` exposes the intersection backend as a
  standalone closest-hit/any-hit service with backend execution-path and
  fallback diagnostics for non-renderer callers.
- Closest-hit and any-hit frontiers are represented by backend-owned handles.
- `render::GpuTracingSceneSections` and diagnostics compile GPU-readable
  material, texture, light, environment, and debug-id records for the initial
  supported shading subset: Matte materials, Phong finite diffuse/glossy shading,
  Reflective mirror continuations, Transparent perfect reflection/refraction
  continuations, Portal delta redirection continuations, Emissive materials,
  ConstantColor, planar/UV CheckerBoard
  texture graphs, nearest-or-bilinear ImageTexture, UVColorTexture, and bounded
  Tinted wrapper chains over those supported base texture records, PointLight,
  DirectionalLight, and RectangularAreaLight.
- Raster-only material normal maps no longer reject GPU tracing material
  compilation. The compiled path-tracing subset ignores `normalTexture()` just
  like the scalar CPU path tracer currently does, so unsupported normal-map
  texture types do not poison otherwise supported path-tracing materials.
- `render::GpuSampleStream` provides the CPU reference for deterministic
  GPU-style sampling dimensions with fixed-vector coverage.
- Static `PinholeCamera`, `OrthographicCamera`, `ThinLensCamera`,
  `TiltShiftCamera`, `EquirectangularCamera`, `SphericalCamera`, and
  `FishEyeCamera` primary-ray generation can be represented by shader-facing
  GPU primary-path descriptors. The CPU reference generator uses the same
  descriptors and `GpuSampleStream` dimensions for parity, while Metal/Vulkan
  diffuse path-loop launches can skip uploading initial path-state records and
  let the kernel synthesize primary path records from the descriptor. Fish-eye
  descriptors preserve the circular image contract by generating active paths
  only for samples inside the unit disc; platform kernels synthesize terminated
  inactive records for discarded descriptor samples. Tilt-shift descriptors use
  the thin-lens lens-sample dimensions while packing shift and tilted
  focal-plane parameters into the descriptor. Trace-disabled full-GPU graph
  launches can also keep those primary records descriptor-only on the host,
  with platform kernels dispatching by descriptor path count instead of the host
  vector size; the platform backends size accumulation for descriptor-only
  pinhole, orthographic, thin-lens, tilt-shift, equirectangular, spherical, and
  fish-eye launches from the same descriptor metadata. Trace and CPU-reference
  paths still materialize records for inspection and parity.
  Trace-disabled platform full-GPU launches now also size path-state, step, and
  Metal closest-hit diagnostic storage to zero logical bytes. Simple LDR graph
  output can now use the platform-resolved display buffer without final HDR
  accumulation readback when the final tonemap is GPU-resolvable (Linear,
  Reinhard, or ACES); graph paths that inspect trace data, denoise, apply an
  unsupported tonemap, or feed postprocess/HDR consumers still retain the
  accumulation image readback. Retained-index and active-depth-count buffers
  remain resident because they are scheduling state, not trace readback
  artifacts. Render graph trace metadata reports the requested capture policy
  (`captureDiagnostics`, platform accumulation, platform display resolve, and
  display-resolve tonemap) so readback behavior is inspectable per pass.
  The Modeler final render dialog now leaves trace capture off by default and
  exposes it as an explicit `Capture graph trace` diagnostic toggle, so the
  normal final-render path can take the trace-disabled display-only full-GPU
  route. The live preview similarly leaves graph trace capture off by default
  and exposes `Capture Graph Trace` in the preview menu for users who need
  pass/resource snapshots.
  Direct Metal/Vulkan backend support checks reject unsupported
  display-resolve tonemaps when resolved display pixels are requested.
- Supported diffuse path-tracing scenes can route GPU execution requests
  through the compiled diffuse path-loop path from the live render graph path.
  rendercli, Modeler preview, and the render dialog report whether that
  execution used the CPU reference path, hybrid frontier compaction, or a
  supported platform full-GPU subset.
- Box denoising is now treated as a postprocess over the resolved compiled
  path-loop image, so it no longer forces a supported GPU path-tracing render
  back through the scalar CPU path. Feature-guided bilateral denoising can now
  run after compiled path-loop execution because the CPU-reference loop and the
  Metal/Vulkan platform full-GPU loops emit first-hit albedo, normal, and depth
  feature records when requested.
- Automatic tracing execution does not select that CPU-reference path-loop as
  a full GPU backend. `auto` stays on CPU/hybrid execution until scene analysis
  can prove that a platform path-loop kernel is available; explicit GPU
  requests can still exercise the compiled CPU-reference loop for diagnostics.
- The compiled path-loop backend interface now separates the diagnostic
  GPU-request backend from the platform full-GPU backend selection point:
  `defaultBackendForGpuRequest()` may return the CPU-reference or hybrid
  compaction path, while `defaultFullGpuBackendForGpuRequest()` returns a
  platform backend only when that build has a real path-loop backend hook. When
  a build contains multiple platform backends, the selector now prefers one
  whose launch path is currently available before falling back to a compiled
  backend for diagnostics. Metal-enabled builds can expose the restricted Metal
  backend for explicit GPU requests; ordinary builds still return no full-GPU
  backend.
- Metal diffuse path-loop probes now publish GPU-generated retained frontier
  indices through the shader-facing retained-index buffer. The ABI is
  count-prefixed so the same buffer can become a device-side compacted frontier
  handoff without a host scan of next path-state activity.
- The Metal full-GPU diffuse path-loop backend now uses that frontier handoff
  for the live platform path: a Metal initialization kernel builds the active
  path index frontier, each depth dispatch advances only indexed active paths,
  and the next frontier is compacted on-device into a second count-prefixed
  buffer. The host still encodes a bounded dispatch sequence for `maxDepth` and
  still reads final diagnostics/accumulation, but path records and retained
  frontier ownership stay in Metal buffers between depths. Each bounce dispatch
  is now indirectly sized from the current GPU-resident retained-frontier count,
  avoiding a full initial-sample dispatch after paths have terminated.
- The Vulkan full-GPU diffuse path-loop backend now follows the same
  depth-frontier shape: Vulkan initialization and advance compute shaders keep
  path records resident, compact retained path indices into current/next
  count-prefixed frontier buffers, and swap descriptor sets inside one command
  buffer instead of looping all bounces inside one shader invocation. Each
  bounce dispatch is prepared from the current GPU-resident retained-frontier
  count and launched through Vulkan indirect dispatch arguments instead of
  relaunching the full initial sample count at every depth.
  Vulkan now also retains its instance, device, descriptor layout, descriptor
  pool, descriptor sets, pipeline layout, queue, command pool, command buffer,
  storage buffers, and path-loop compute pipelines across launches, so steady
  renders no longer pay device, descriptor, command-pool, buffer allocation,
  and pipeline creation overhead for every image.
- GPU-requested compiled diffuse path-loop renders automatically use an
  available Metal or Vulkan frontier-compaction backend for the live
  `GpuDiffusePathStateRecord` frontier, reporting that middle step as hybrid
  execution while the overall loop remains the CPU reference implementation.
- The Modeler saved Render Settings item, final render dialog, and rendercli
  graph options expose the GPU sample stream as an explicit Path Tracer choice.
  Full-GPU compiled path-loop eligibility now treats ordinary sampler-backed
  Path Tracer requests as CPU/hybrid fallback cases instead of silently
  replacing the chosen sampler with the GPU stream. The final render dialog's
  managed Auto/GPU Path Tracer default now selects the GPU sample stream, while
  explicit CPU Path Tracer renders still default to Halton.
- CPU reference tracing accumulation, the current CPU wavefront tile
  accumulator, optional Metal accumulation buffers, and optional Vulkan
  synthetic accumulation results expose resource residency, byte,
  operation-count, and readback diagnostics.
- `render::TracingExecutionCapabilityRecords` groups tracing capabilities by
  intersection, scene records, sampling, direct lighting, BSDF, path state, and
  accumulation while preserving the older intersection metric aliases.
- Metrics expose backend request, selected backend, platform availability,
  execution path, fallback reason, transfer estimates, query counts, frontier
  residency, compiled tracing-scene counts, sample stream mode, accumulation
  diagnostics, host path-state bytes, compaction candidates, direct-light batch
  sizes, and future resident-frontier opportunity estimates.
- Modeler and rendercli expose much of that diagnostic state.

### Not Yet Available

- General platform GPU-owned path state. Static pinhole, orthographic,
  thin-lens, tilt-shift, equirectangular, spherical, and fish-eye primary paths
  can now be generated from shader-facing descriptors without host
  materialization for trace-disabled full-GPU graph launches and platform
  kernels dispatch those descriptor-only launches by descriptor path count, but
  other camera models without descriptors, animated cameras, later
  path-continuation records, and retained frontier ownership still need broader
  platform path-state support.
- A shared backend abstraction for selecting compacted wavefront versus
  megakernel schedules per platform, plus Vulkan shader validation and parity
  coverage on a Vulkan-enabled build for the new depth-frontier path-loop
  entry points.
- Broad platform full-GPU path-loop kernels for the normal render path. A
  restricted Metal path-loop kernel can advance empty-scene and
  optionally transformed triangle/`MeshPrimitive` mesh-triangle/finite-width
  `Curve` tessellated-triangle/sphere/plane/rectangle/disk/open-cylinder/torus
  paths with a depth-frontier Metal schedule and Matte, Phong finite
  diffuse/glossy, Reflective mirror,
  Transparent perfect reflection/refraction, Portal, or Emissive materials across
  multiple depths for backend tests and explicit GPU graph requests when the
  light set is empty or uses point, directional, or rectangular area lights. A
  restricted Vulkan path-loop backend can execute empty-scene all-miss paths and
  a multi-depth shaded static-transform
  triangle/`MeshPrimitive` mesh-triangle/finite-width `Curve`
  tessellated-triangle/sphere/plane/rectangle/disk/open-cylinder/torus subset with
  Matte/Phong-finite-glossy/Reflective-mirror/Transparent-refraction/Portal/Emissive
  materials, ConstantColor/planar-or-UV CheckerBoard texture graphs/
  nearest-or-bilinear ImageTexture/UVColor records, bounded Tinted wrapper
  chains over those records, and zero or more point, directional, or
  rectangular area lights when Vulkan is built and available, including
  sample-slot accumulation for duplicate active pixel targets, now through a
  depth-frontier Vulkan schedule.
  Graph auto-selection still waits for Vulkan shaded-path parity, broader scene
  support, and performance gates.
- Platform full-GPU path-loop backend selection beyond that restricted Metal
  subset and the restricted Vulkan multi-depth sphere subset. The factory hook
  can return Metal or Vulkan platform backends in platform-enabled builds, but
  ordinary builds and unsupported scenes still fall back to the CPU-reference or
  hybrid diagnostic backend.
- GPU material records beyond the current Matte, Emissive, Phong, Reflective,
  and Transparent subset.
- GPU texture records beyond ConstantColor, planar/UV CheckerBoard texture
  graphs, nearest-or-bilinear ImageTexture records, UVColorTexture, and bounded
  Tinted wrapper chains.
- GPU light records beyond PointLight, DirectionalLight, and
  RectangularAreaLight.
- Platform GPU BSDF evaluation.
- Platform GPU direct-light contribution evaluation.
- Platform GPU path continuation generation and Russian roulette.
- Integrated platform GPU accumulation/progressive sample buffers in the live
  render loop.
- Full platform GPU path-tracing loop.
- Full platform GPU Whitted loop.
- Hardware ray tracing backends.
- A render graph compiler model that can synthesize platform full-GPU tracing
  plans after those kernels exist; the current compiler can report explicit
  GPU requests, hybrid fallbacks, and compiled-reference diagnostics without
  treating the CPU-reference path-loop as automatic full GPU execution.

---

## Terminology

### Algorithm

The light-transport estimator or deterministic ray policy:

- Whitted ray tracing;
- scalar path tracing;
- wavefront path tracing;
- bidirectional path tracing;
- photon mapping;
- progressive photon mapping;
- Metropolis light transport;
- ReSTIR-style direct or global illumination;
- volume path tracing;
- future algorithms.

The algorithm owns semantics. It defines what contribution is being estimated
or computed and what images are considered correct.

### Schedule

The order in which work is executed:

- recursive single-ray calls;
- scalar iterative path loop;
- depth-major wavefront;
- megakernel GPU loop;
- persistent GPU queues;
- tiled progressive batches;
- split kernels per phase;
- packet traversal.

The schedule should not change the algorithm's estimator. A scalar path tracer
and a wavefront path tracer can have different noise order and progress
behavior, but they should converge to the same image under the same algorithmic
settings.

### Execution Backend

The device and resource domain that executes work:

- CPU runtime scene traversal;
- CPU packed/compiled scene traversal;
- Metal compute;
- Vulkan compute;
- future hardware ray tracing;
- hybrid CPU/GPU plans.

The backend owns execution details, memory layout, kernels, uploads, readbacks,
and capability reporting. It does not own algorithm semantics.

### Backend Service

A reusable operation exposed by a backend:

- closest-hit intersection;
- any-hit/occlusion;
- shadow/visibility queries;
- material/BSDF evaluation;
- light sampling;
- direct-light contribution;
- path continuation generation;
- path-state compaction;
- accumulation;
- denoising;
- AOV production.

The current GPU-intersection work is one backend service.

### Intersection Service Contract

The current reusable intersection backend service is a backend service, not a
full GPU tracing implementation. CPU integrators and schedules still own path
state, material evaluation, BSDF sampling, light sampling, path continuation,
sample accumulation, denoising, tonemapping, and render graph scheduling. The
service answers ray-scene visibility questions for supported scene subsets and
reports when that narrow service cannot run on the requested backend.

The service exposes two query families:

- **Closest hit.** Camera, path-continuation, reflection, refraction, probe, and
  debug rays ask for the nearest positive surface intersection in the submitted
  `t` interval. A closest-hit query returns either a miss or one materializable
  hit record that CPU shading can consume.
- **Any hit.** Shadow, occlusion, visibility, and direct-light queries ask
  whether any supported primitive intersects the ray before `maxDistance`. An
  any-hit query returns one byte-sized occlusion flag per submitted ray:
  non-zero means occluded, zero means visible.

### Hybrid Visibility and Shadow Limitations

Hybrid visibility and ray-traced preview shadows are current users of the
intersection service. They are intentionally intersection-only features:

- `hybrid_visibility` submits primary closest-hit debug rays through
  `render::IntersectionService` and visualizes whether supported scene geometry
  was hit.
- `--shadow_mode ray_traced` builds a raster shadow-mask pass that submits
  shadow/occlusion any-hit rays through `render::IntersectionService` and
  composites the resulting mask over raster beauty output.

These modes may use a platform GPU backend for the submitted closest-hit or
any-hit batches when the build, device, scene subset, query family, and kernel
support allow it. They do not move the full renderer to the GPU. The CPU still
owns render-graph scheduling, camera/sample setup, material and texture
evaluation, light sampling, BSDF evaluation, path state, continuation
generation, accumulation, tonemapping, and final image ownership. A successful
GPU intersection dispatch therefore means "the visibility query ran on the GPU"
and not "this frame was path traced on the GPU."

The practical rendercli checks are:

```sh
rendercli --engine raster --shadow_mode ray_traced \
  --wavefront_intersection_backend gpu \
  --render_graph_trace_out trace.json \
  --wavefront_metrics_summary \
  test/fixtures/rendercli/raster_shadow_caster.json shadow.png

rendercli --engine raster \
  --render_graph_aov_out hybrid_visibility=visibility.png \
  --wavefront_intersection_backend gpu \
  --render_graph_trace_out trace.json \
  test/fixtures/rendercli/raster_shadow_caster.json raster.png
```

Inspect the trace or compact metrics for the requested backend, selected
backend, closest-hit and any-hit execution paths, compiled-scene support counts,
and fallback reason. Expected fallback families include CPU policy selection,
missing platform device or render-path kernels, unsupported scene leaves,
unsupported closest-hit or any-hit query-family kernels, platform dispatch
failure, malformed result counts, and runtime-only material or continuation
semantics. Unsupported scenes keep the runtime CPU path rather than producing a
partial GPU result.

In Modeler, use `Render -> Preview Engine -> Rasterizer`, enable preview
shadows, choose the ray-traced shadow mode when available, and select
`Render -> Preview View -> Hybrid visibility` to inspect the AOV path. The
Render Graph dock's selected pass details and trace messages show whether the
hybrid pass used a platform backend, packed CPU parity traversal, or the
runtime CPU fallback and why.

The CPU runtime backend supports the full existing `render::Scene` traversal
semantics. The compiled/packed and platform GPU service subset is deliberately
smaller and all-or-nothing for a render or service call. The supported subset is:

- triangle leaves and mesh triangles;
- sphere;
- plane;
- rectangle;
- disk;
- `OpenCylinder`;
- `Torus`;
- static transforms / instances whose payloads can be represented by stable
  compiled ids.

Unsupported CSG/boolean composites (`Difference`, `Intersection`, `Union`,
`ConvexHull`, and `MinkowskiSum`), moving transforms, unsupported exact
primitive payloads, and materials that require runtime-only continuation
semantics such as transparent/glass Whitted recursion must keep the service on
the runtime CPU path or produce an explicit fallback reason. Platform Metal and
Vulkan backends may also reject a scene that the packed CPU service can compile
until matching platform kernels exist.

Closest-hit output records must preserve CPU intersection semantics for the
supported subset:

- hit/miss state;
- primitive/object id and material lookup key;
- hit distance `t`;
- geometric hit point or enough data to reconstruct it;
- normal in the same orientation convention as the CPU path;
- UV, barycentric, or local coordinates when the primitive provides them;
- per-ray `State` bookkeeping needed by the CPU shading path.

Any-hit output records must preserve the CPU visibility contract:

- one result per submitted ray, in submission order;
- non-zero occlusion flag only when an intersection occurs before the query's
  maximum distance according to the same light-distance rule as
  `Scene::occludes(...)`;
- no material shading, alpha blending, or transparent continuation unless a
  future contract adds those semantics explicitly.

Diagnostics are part of the contract. Every backend selection or execution path
must expose:

- requested backend, selected backend, platform name, and availability;
- overall, closest-hit, and any-hit execution paths (`runtime_scene`,
  `packed_cpu`, `metal`, `vulkan`, or future names);
- compiled-scene counts: BVH nodes, primitive totals, supported primitive
  totals, transform count, upload bytes, and unsupported counts by reason;
- closest-hit and any-hit query counts, ray-upload byte estimates, readback byte
  estimates, total query-transfer estimates, and estimated round trips;
- frontier residency labels and frontier payload byte counts where frontier
  handles are used;
- observed upload/preparation, kernel/traversal, and readback worker seconds;
- explicit fallback reason when a requested GPU or packed path cannot run.

Fallback reasons should be stable enough for tests and diagnostics. The current
families are:

- user or auto policy selected CPU, including the auto-selected CPU threshold
  case before scene compilation;
- platform GPU device or render-path unavailable;
- scene could not compile to the intersection subset;
- scene compiled but is not eligible for the requested closest-hit or any-hit
  packed/platform kernel;
- unsupported primitive, transform, material, or runtime continuation
  requirement, counted by reason;
- platform preparation, dispatch, or kernel failure;
- backend returned malformed results, such as a closest-hit or any-hit batch
  whose result count differs from the submitted ray count.

---

## Target Architecture

### High-Level Shape

```text
Render intent
  -> render graph compiler
      -> tracing algorithm
      -> schedule
      -> execution backend selection
      -> graph pass state and trace metadata

Scene
  -> tracing scene compiler
      -> geometry records
      -> material records
      -> texture records
      -> light records
      -> camera/sample configuration
      -> backend-specific resource handles
```

The renderer should stop treating "wavefront intersection backend" as the
primary abstraction once more GPU work moves beyond visibility. The new parent
abstraction should be an execution backend or tracing backend with capability
flags.

### Capability Model

Backends should report structured capabilities:

```text
geometry.closest_hit
geometry.any_hit
geometry.supported_primitives
geometry.supported_transforms
scene.material_records
scene.texture_records
scene.light_records
sampling.gpu_rng
sampling.named_dimensions
shading.bsdf_eval
shading.bsdf_sample
shading.delta_branches
lighting.direct_light_sample
lighting.direct_light_visibility
lighting.direct_light_contribution
lighting.resident_direct_light_batches
state.path_state_residency
state.frontier_compaction
state.spawned_continuations
accumulation.sample_accumulation
accumulation.progressive_readback
debug.aovs
debug.trace_metadata
```

Capability values should include:

- supported;
- unsupported;
- supported for restricted subset;
- fallback reason;
- platform availability;
- kernel availability;
- expected transfer cost;
- observed execution path.

This turns "full GPU path tracer" into a composable set of capabilities rather
than a single binary switch.

### Execution Modes

The graph should eventually distinguish at least these modes:

1. **CPU reference**
   Everything runs through the CPU runtime scene and CPU integrator.

2. **CPU compiled**
   CPU uses compiled/packed scene records. This is useful for parity and for
   proving GPU ABI layout without requiring a GPU.

3. **Hybrid intersection**
   CPU owns scheduling, material evaluation, light sampling, path state, and
   accumulation. GPU answers closest-hit and any-hit frontiers.

4. **Hybrid resident frontiers**
   GPU owns ray frontiers and compaction, but CPU may still own shading and
   path semantics. Static primary-path descriptors are the first
   device-generated path-state inputs, but the mode still needs resident
   continuation frontiers to avoid CPU-owned path-state materialization.

5. **GPU shading subset**
   GPU evaluates a restricted material/light subset and returns accumulated
   contributions or continuation records. Unsupported scene features fall back
   visibly.

6. **Full GPU tracing subset**
   GPU owns path state, intersection, shading, sampling, direct lighting,
   continuation, compaction, and accumulation for supported scene subsets.

7. **Hardware RT backend**
   GPU tracing uses Metal/Vulkan hardware ray tracing acceleration structures
   while preserving the same algorithm/schedule semantics.

### User-Facing Execution Controls

Scene render intent and rendercli/Modeler overrides should expose a broad
tracing execution preference, not internal graph nodes or individual backend
services. The durable field is:

```json
{
  "renderIntent": {
    "engineOptions": {
      "raytracer": {
        "execution": {
          "tracingExecution": "auto"
        }
      }
    }
  }
}
```

Valid values are:

- `auto` - the compiler chooses the best supported tracing execution for the
  selected executor, integrator, scene support, device availability, and
  expected work size. This is the default when the field is omitted.
- `cpu` - request CPU execution. The compiler must not select GPU-owned
  tracing work for this request, though ordinary CPU graph passes and CPU
  resource transfers may still appear.
- `hybrid` - request a CPU-owned tracing algorithm/schedule that may use GPU
  backend services such as closest-hit/any-hit intersection, resident
  frontiers, compaction, or accumulation when those services are available.
- `gpu` - request a GPU-owned tracing loop for the selected algorithm and
  supported scene subset. Unsupported scenes or unavailable platform support
  must produce explicit fallback diagnostics instead of silently becoming CPU.

The field describes execution intent. It does not name graph passes, graph
nodes, Metal/Vulkan kernels, or service-level controls. During the transition
from the existing intersection-only control, `execution.intersectionBackend`
remains a narrower compatibility override for the closest-hit/any-hit backend
service. Valid combinations are:

- omitted `tracingExecution` plus any `intersectionBackend` value preserves
  existing behavior;
- `tracingExecution: "auto"` with `intersectionBackend: "auto"` or omitted is
  the preferred policy-selected spelling;
- `tracingExecution: "cpu"` is incompatible with `intersectionBackend: "gpu"`
  because the broad request forbids GPU tracing services;
- `tracingExecution: "hybrid"` may combine with `intersectionBackend: "auto"`
  or `"gpu"` to request GPU intersection inside a CPU-owned schedule;
- `tracingExecution: "gpu"` plus `intersectionBackend: "cpu"` is invalid once
  full GPU compilation is active because a GPU-owned tracing loop cannot force
  its core scene queries through the CPU service;
- `tracingExecution: "gpu"` plus `intersectionBackend: "auto"` or omitted lets
  the compiler pick the required platform service and report fallback when a
  full GPU plan cannot run.

Future rendercli and Modeler controls should label these as execution
preferences ("Auto", "CPU", "Hybrid", "GPU") and keep service-level controls
behind advanced diagnostics or compatibility affordances.

### GPU Accumulation Buffer Layout

`render::TracingAccumulationLayout` defines the v1 backend accumulation layout.
All planes are image-shaped with the same `width` and `height`:

- `colorSum`: required `rgba32_float`; RGB stores the linear HDR radiance sum,
  alpha is reserved and written as zero.
- `sampleCount`: required `uint32`; one counter per pixel.
- `moment`: optional `rgba32_float_second_raw_moment`; RGB stores the sum of
  squared linear sample values for variance/adaptive sampling, alpha is
  reserved and written as zero.
- `resolve`: required `rgba8_unorm_srgb`; LDR display/export output produced
  after dividing by the sample count and applying the selected resolve policy.

The resolve plane is separate from accumulation. Backends must not overwrite or
reinterpret the HDR color sum as the display target, and callers must size
memory from the individual plane byte counts rather than assuming one packed
struct per pixel. `render::TracingAccumulationBuffer` is the CPU reference for
clear, sample add, optional raw-second-moment accumulation, and LDR resolve.
Metal-enabled builds also expose optional clear, sample-add, and LDR resolve
kernels through `render::MetalTracingAccumulationBuffer`; Vulkan kernels and
resource diagnostics are follow-up jobs under the GPU accumulation-buffer epic.

---

## Plan Ownership Map

- `docs/plans/wavefront-and-path-tracing.md`
  - Owner: CPU schedule and path-tracing semantics.
  - Keep as the reference for scalar vs wavefront scheduling, convergence,
    adaptive sampling, denoising, and educational explanation.
  - It should not own full GPU execution.

- `docs/plans/gpu-wavefront-intersection.md`
  - Owner: reusable closest-hit/any-hit intersection service for wavefront and
    hybrid backends.
  - Keep active until the intersection service is correct, measured, and
    independently usable.
  - It is a child slice of this plan, not the top-level goal.

- `docs/plans/render-graph.md`
  - Owner: graph compilation, resource visibility, pass inspection, and graph
    trace. It should consume the tracing execution model once it exists.

- `docs/plans/opengl-gpu-residency.md`
  - Owner: raster/OpenGL resource residency. It is related by GPU resource
    concepts but not a tracing backend plan.

---

## Milestone Strategy

Milestones are deliberately sized so some can be delegated to Syrus while
interactive work can continue on the architectural slices that need judgment.
Each milestone has:

- a stable deliverable;
- dependency list;
- tests and visual/debug verification;
- a "Syrus-ready" note.

Commits should be reasonably sized: one backend capability, one testable UI
surface, one documentation slice, or one coherent refactor per commit. Avoid
one-line metric/documentation commits unless they fix a broken build or a
small isolated defect.

The **Syrus Epic Graph** section near the end of this file is the concrete
delegation graph. The milestones below explain the technical destination; the
epic graph says how to file the work as containers with jobs and dependencies.

---

## Milestone 0 - Current-State Audit and Naming Reset

**Goal:** establish that the active top-level goal is tracing execution
backends, while preserving the existing GPU-intersection plan as a child.

**Dependencies:** none.

**Deliverables:**

- Add this parent plan.
- Update `gpu-wavefront-intersection.md` status to say it is retired as a
  top-level goal and remains active as the intersection-service slice.
- Update `wavefront-and-path-tracing.md` Phase 7+ to point to this plan as the
  parent for GPU tracing.
- Optionally add a short textbook note that GPU intersection is a hybrid mode,
  not full GPU path tracing.

**Tests/gates:**

- `rake docs:textbook:check`
- Search confirms old plans point to this parent plan.

**Syrus-ready:** yes, after the desired wording is approved. Low risk.

---

## Milestone 1 - Tracing Backend Capability Model

**Goal:** replace ad hoc GPU-intersection capability reporting with a general
backend capability object that can grow into full tracing execution.

**Dependencies:** Milestone 0.

**Deliverables:**

- Add a `TracingBackendCapabilities` or similarly named type.
- Include substructures for:
  - intersection;
  - scene compilation;
  - material records;
  - texture records;
  - light records;
  - sampling/RNG;
  - shading;
  - path state residency;
  - compaction;
  - accumulation;
  - diagnostics.
- Adapt existing `WavefrontIntersectionBackend` metrics to populate the new
  capability shape without changing user-facing behavior.
- Keep compatibility aliases in existing JSON metrics until consumers migrate.
- Add rendercli metrics fields that make sense outside intersection, for
  example:
  - `tracing_backend`;
  - `tracing_backend_mode`;
  - `tracing_backend_capabilities`;
  - `tracing_backend_fallback`.
- Update Modeler selected-pass metadata to group capabilities instead of only
  listing flat intersection fields.

**Tests/gates:**

- Unit tests for capability serialization.
- rendercli functional test for CPU, explicit GPU fallback, and supported GPU
  request metadata.
- Modeler widget tests for capability grouping.
- Existing wavefront metrics tests still pass.

**Syrus-ready:** partially. Serialization and rendercli tests can be delegated.
Modeler grouping is better done interactively because UX judgment matters.

---

## Milestone 2 - Intersection Service Consolidation

**Goal:** finish the current GPU-intersection work as a stable backend service
that can be reused by shadows, visibility, hybrid tracing, and future full GPU
tracing.

**Dependencies:** Milestone 1 can happen before or after this, but final
diagnostics should use the capability model.

**Deliverables:**

- Keep `WavefrontIntersectionBackend` or rename it to an intersection service
  only if the call sites become clearer.
- Preserve:
  - CPU runtime backend;
  - CPU compiled/packed backend;
  - Metal backend;
  - Vulkan backend;
  - explicit fallback reasons.
- Finish platform parity for the existing supported primitive subset:
  - triangle;
  - mesh triangles;
  - sphere;
  - plane;
  - rectangle;
  - disk;
  - OpenCylinder;
  - Torus;
  - static transforms.
- Add a compact intersection-only API usable outside wavefront path tracing:
  - shadow/visibility service for raster hybrids;
  - graph AOV/debug passes;
  - picking/probing later if useful.
- Make auto-selection conservative and evidence-based:
  - availability;
  - scene eligibility;
  - expected query count;
  - transfer estimate;
  - measured performance threshold.

**Tests/gates:**

- CPU/packed/GPU closest-hit record parity.
- CPU/packed/GPU any-hit parity.
- Rendered image RMS parity for deterministic supported scenes.
- Transparent/glass packed-intersection eligibility, plus explicit fallback
  tests for genuinely unsupported scene features.
- rendercli summary shows the actual execution path.
- Benchmarks capture large supported scenes before enabling `auto` GPU
  selection by default.

**Syrus-ready:** yes for parity coverage and fallback tests. Performance
threshold tuning should remain interactive.

---

## Milestone 3 - CPU Reference and Cross-Backend Parity Harness

**Goal:** make every future backend prove it matches the CPU algorithm on
well-defined scenes before it is user-selectable.

**Dependencies:** Milestone 0.

**Deliverables:**

- Create a reusable parity harness for:
  - hit records;
  - occlusion records;
  - direct-light samples;
  - sample accumulation;
  - final image RMS/SSIM-style comparison;
  - stochastic deterministic-seed comparisons.
- Define canonical test scenes:
  - supported matte scene;
  - direct area-light scene;
  - indirect bounce scene;
  - mirror/delta branch scene;
  - glass/transparent fallback scene;
  - imported mesh scene;
  - large visibility-heavy scene.
- Provide rendercli test helpers that can run CPU, packed CPU, Metal, Vulkan,
  and fallback paths with the same seed.
- Make tests skip platform GPU checks cleanly when unavailable.

**Tests/gates:**

- Harness itself covered by unit tests with synthetic images/records.
- ~~rendercli functional tests use the harness for at least one CPU vs GPU
  request path.~~ ✅ **Done.** `rendercli_tracing_parity` compares the
  canonical matte direct-light fixture across CPU and GPU-requested paths and
  checks the metrics path metadata for issue #578.

**Syrus-ready:** yes. This is a good independent Syrus epic because it can be
implemented without changing algorithm semantics.

---

## Milestone 4 - Compiled Tracing Scene Records

**Goal:** compile more than geometry. Full GPU tracing needs stable material,
texture, and light records, not only primitive ids.

**Dependencies:** Milestone 1 recommended. Milestone 2 useful but not required
for material/light records.

**Deliverables:**

- Add a `CompiledTracingScene` or extend the existing compiled scene with
  separate sections:
  - geometry/intersection scene;
  - material table;
  - texture table;
  - light table;
  - environment/background records;
  - object/material name/id debug table.
- Start with a restricted material subset:
  - matte diffuse;
  - emissive;
  - ~~mirror/delta reflection if straightforward~~ ✅ **Done.** `GpuTracingMaterialRecord`
    now carries `ReflectiveMaterial` local Phong coefficients plus mirror
    reflection color/coefficient continuation parameters for GPU Whitted v1
    record consumers.
  - ~~transparent/glass explicitly unsupported for first GPU path-tracing subset
    unless the continuation contract is ready~~ ✅ **Done.** The restricted
    compiled path-loop subset now lowers `TransparentMaterial` local Phong
    state plus reflection/transmission continuation coefficients and samples one
    perfect delta branch per path state.
- Start with a restricted texture subset:
  - constant color;
  - ~~checkerboard if UV/local-coordinate payloads are ready~~ ✅ **Done.**
    Compiled texture records now cover planar/UV checker graphs, UV color,
    nearest/bilinear image records, and bounded tinted wrapper chains for the
    path-loop subset. The direct-light CPU-reference work record also carries
    surface UVs and shares the compiled texture evaluator with the diffuse
    path-step reference, keeping fallback/direct-light diagnostics aligned with
    those texture records.
  - finite diffuse/Phong BSDF evaluation is now shared between the compiled
    diffuse path-step reference and direct-light CPU-reference contribution
    records, so Phong, Reflective, and Transparent local surface terms remain
    aligned while platform direct-light contribution kernels are still being
    filled in.
  - unsupported fallback for procedural or complex textures.
- Start with a restricted light subset:
  - point light;
  - directional light;
  - rectangular area light;
  - environment/miss color.
- Record first unsupported reason and counts by reason.

**Tests/gates:**

- Unit tests for material/texture/light record compilation.
- Unsupported scene fallback tests.
- Id round-trip tests from runtime material/object to compiled records.
- JSON trace contains compiled tracing-scene counts.

**Syrus-ready:** yes. This is a strong delegation candidate because it has
clear inputs and testable output records.

---

## Milestone 5 - GPU Sample Streams and RNG

**Goal:** support deterministic stochastic dimensions on GPU so CPU/GPU path
tracing can be compared and debugged.

**Dependencies:** Milestone 1. Can run in parallel with Milestone 4.

**Deliverables:**

- Define GPU sample stream contract:
  - pixel sample dimension;
  - lens/time dimensions;
  - BSDF dimension;
  - light selection dimension;
  - Russian roulette dimension;
  - continuation dimensions.
- Choose an initial RNG/sequence:
  - deterministic hash-based generator;
  - or PCG/Xoroshiro-style per-sample state;
  - Halton/Sobol later if desired.
- Add CPU implementation of the exact same GPU sample stream for parity.
- Add seed and sample-index mapping to compiled tracing execution settings.
- Add debug AOV or metrics for generated samples if useful.

**Tests/gates:**

- CPU/GPU sample generation parity for fixed seeds.
- Distribution smoke tests.
- Rendercli option/trace records the seed and stream mode.

**Syrus-ready:** yes, after RNG choice is agreed. This can be isolated from
scene compilation.

---

## Milestone 6 - GPU Accumulation and Progressive Readback

**Goal:** move sample accumulation to GPU before full GPU shading. This reduces
readback pressure and creates the output surface full GPU tracing will need.

**Dependencies:** Milestone 1. Can proceed before GPU BSDF if fed with test
colors.

**Deliverables:**

- Add backend-owned accumulation buffer:
  - HDR color sum;
  - sample count;
  - optional variance/stddev moments;
  - optional albedo/normal AOVs later.
- Define `render::TracingAccumulationLayout` as the v1 backend accumulation
  layout:
  - all planes are image-shaped with the same width and height;
  - `colorSum` is required `rgba32_float`; RGB stores the linear HDR radiance
    sum, and alpha is reserved and written as zero;
  - `sampleCount` is required `uint32`; one counter is stored per pixel;
  - `moment` is optional `rgba32_float_second_raw_moment`; RGB stores the sum
    of squared linear sample values for variance/adaptive sampling, and alpha
    is reserved and written as zero;
  - `resolve` is required `rgba8_unorm_srgb`; this is the LDR display/export
    output produced after dividing by the sample count and applying the
    selected resolve policy;
  - the resolve plane remains separate from accumulation. Backends must not
    overwrite or reinterpret the HDR color sum as the display target, and
    callers must size memory from individual plane byte counts rather than
    assuming one packed struct per pixel.
- Add CPU reference implementation with the same layout.
- Add Metal/Vulkan kernels for:
  - clear;
  - add sample contribution;
  - resolve to display/readback buffer.
- Integrate with graph trace as a resource:
  - residency;
  - bytes;
  - readback count;
  - progressive snapshot count.
- Keep current CPU final image path as fallback.

**Tests/gates:**

- Accumulation parity for synthetic contributions.
- Rendercli writes the same resolved image from CPU and GPU accumulation on a
  deterministic test buffer.
- Modeler progressive display still updates.

**Syrus-ready:** yes for CPU/packed and shader parity. Modeler progressive UX
should be reviewed interactively.

---

## Milestone 7 - GPU Direct-Lighting Visibility and Contribution

**Goal:** move next-event-estimation work beyond "GPU answers shadow rays" by
letting the backend compute direct-light contributions for a restricted subset.

**Dependencies:** Milestones 2, 4, and 5.

**Deliverables:**

- Define a direct-light work item:
  - hit point or reconstructible hit data;
  - normal;
  - material id;
  - incoming direction;
  - path throughput;
  - sample dimension/state;
  - light selection result or seed.
- First implementation can split:
  - GPU light selection and visibility;
  - CPU contribution evaluation;
  - then GPU contribution evaluation once material/light records are ready.
- Support initial lights:
  - point;
  - directional;
  - rectangular area light.
- Support initial materials:
  - matte diffuse;
  - emissive direct hit handling.
- Keep MIS contract explicit.
- Unsupported material/light combos fall back or terminate visibly according to
  algorithm policy.

**Tests/gates:**

- Analytic diffuse direct-light tests match CPU.
- MIS tests match CPU for non-delta area-light samples.
- Visibility batching parity.
- Image parity on `pathtracer_area_light_demo.json`.
- Metrics distinguish:
  - visibility-only GPU;
  - contribution GPU;
  - CPU fallback.

**Syrus-ready:** partially. Analytic tests and data records are delegateable;
MIS and estimator semantics should be reviewed interactively.

---

## Milestone 8 - GPU BSDF Evaluation and Path Continuation

**Goal:** move the core path-transport step to GPU for a small material subset.

**Dependencies:** Milestones 4 and 5. Milestone 6 recommended.

**Deliverables:**

- Add GPU BSDF eval for matte diffuse.
- Add GPU BSDF sampling for matte diffuse.
- Add optional mirror/delta reflection once branch spawning is ready.
- Add Russian roulette on GPU.
- Define continuation record:
  - ray;
  - throughput;
  - sample index;
  - depth;
  - pixel/sample id;
  - flags;
  - previous BSDF pdf and delta flag for MIS.
- Add CPU implementation of the same continuation kernel for parity.

**Tests/gates:**

- BSDF eval/sample parity for deterministic sample inputs.
- Continuation record parity for diffuse hits.
- Russian roulette deterministic parity.
- Unsupported materials produce explicit fallback/termination metrics.

**Syrus-ready:** yes for diffuse-only record/kernel tests. Delta branching
should be interactive.

---

## Milestone 9 - GPU-Resident Path State and Compaction

**Goal:** remove the largest current hybrid limitation: CPU-owned `BatchPath`
frontiers and host compaction.

**Dependencies:** Milestones 1, 5, and 8. Milestone 6 recommended.

**Deliverables:**

- Define GPU path-state layout:
  - active ray;
  - throughput;
  - accumulated contribution or accumulation target;
  - sample/pixel id;
  - depth;
  - RNG/sample stream state;
  - flags;
  - material/light MIS metadata.
- Add device frontier buffers:
  - active paths;
  - next paths;
  - spawned delta continuations;
  - retained index buffer;
  - compaction output.
- Add Metal/Vulkan compaction kernels.
- Preserve CPU reference compaction contract.
- Update metrics:
  - resident path-state bytes;
  - retained path-state bytes;
  - removed path-state bytes;
  - compaction kernel time;
  - host readback avoided;
  - resident-frontier round trips actually saved.
- Keep host debug readback available for graph trace inspection.

**Tests/gates:**

- Compaction parity for synthetic path states.
- Path frontier after each depth matches CPU for deterministic diffuse scene.
- Metrics change from estimate-only to actual resident execution path.
- No graph/Modeler flicker regression when trace updates include resident
  frontiers.

**Syrus-ready:** partially. Synthetic compaction tests and CPU reference layout
are delegateable. Platform shader work and scheduler integration should be
interactive.

---

## Milestone 10 - First Full GPU Path Tracer v1

**Goal:** implement the first complete GPU tracing algorithm for a restricted
scene subset.

**Dependencies:** Milestones 2, 4, 5, 6, 7, 8, and 9.

**Initial supported subset:**

- Pinhole camera.
- Static geometry from the compiled intersection subset.
- Matte diffuse material.
- Emissive material or rectangular area light.
- Constant-color texture.
- Point/directional/rectangular area lights.
- Environment/miss color.
- Transparent perfect reflection/refraction delta continuations.
- No procedural textures except explicit first subset.
- No motion blur.
- No DOF until lens sampling is included.

**Execution options:**

- Metal full GPU path tracer on macOS.
- Vulkan full GPU path tracer on Linux.
- CPU compiled backend with the same records for parity.

**Deliverables:**

- New backend mode:
  - `tracing_backend_mode=full_gpu_subset`;
  - or equivalent graph-visible state.
- Render graph compiler can select this mode only when scene support and user
  intent allow it.
- rendercli can request it explicitly.
- Modeler Render Settings can request it under an advanced tracing backend
  control.
- Fallback path is explicit:
  - unsupported material;
  - unsupported texture;
  - unsupported light;
  - unsupported camera;
  - platform unavailable;
  - kernel unavailable.
- Progressive preview works through resolved accumulation readback.

**Tests/gates:**

- CPU scalar path tracer vs GPU path tracer image comparison at fixed seed.
- CPU wavefront path tracer vs GPU path tracer statistical comparison.
- Golden scenes at low resolution for:
  - direct light;
  - indirect bounce;
  - environment miss;
  - shadows;
  - unsupported fallback.
- Metrics prove GPU-owned:
  - path state;
  - direct lighting;
  - BSDF sampling;
  - accumulation.

**Syrus-ready:** no as one whole task. This should be split after Milestones
4-9 land. Individual shaders/tests can be delegated.

---

## Milestone 11 - GPU Whitted v1

**Goal:** add a GPU Whitted-style renderer for deterministic reflection/shadow
scenes once the full GPU infrastructure exists.

**Dependencies:** Milestones 2, 4, 6, 8, and 9.

**Rationale:**

Whitted ray tracing is educationally important and easier to compare against
existing deterministic output, but recursive reflection/refraction and
material-specific branching can make it less clean than diffuse-first path
tracing. It should follow the shared backend infrastructure rather than being a
separate one-off GPU renderer.

### GPU Whitted v1 Supported Subset

GPU Whitted v1 is a deterministic, hard-shadow Whitted branch for the scene
subset already expressible through shared tracing backend services. It is not a
path tracer, not a soft-shadow renderer, and not a separate GPU scene compiler.
Selection must require the common tracing capability records to report support
for the needed scene records, closest-hit queries, any-hit visibility queries,
direct-light contribution, reflection continuations, path-state/frontier
residency, and accumulation.

The supported v1 scene subset is:

- the compiled intersection-service geometry subset from this plan's
  **Intersection Service Contract**;
- exact material records for `MatteMaterial`, `PhongMaterial`, and
  `ReflectiveMaterial`, plus `EmissiveMaterial` for visible emitter surfaces;
- `ConstantColorTexture` inputs only;
- `PointLight`, `DirectionalLight`, and `RectangularAreaLight`, evaluated with
  deterministic Whitted direct-light semantics and hard any-hit shadow tests;
- scene ambient, background miss color, and tonemap/resolve behavior matching
  the CPU Whitted path;
- mirror reflection continuations for `ReflectiveMaterial` only, using the same
  maximum recursion depth and throughput cutoff as `render::WhittedIntegrator`.

The explicit unsupported list for v1 is:

- `TransparentMaterial`, glass/refraction, total-internal-reflection-only glass
  shortcuts, Fresnel splitting, nested-medium state, and alpha/transparency
  visibility;
- `PortalMaterial`, custom `Material::shade()` callbacks, and any material that
  requires CPU runtime continuation semantics;
- image, checker, procedural, normal, bump, displacement, or otherwise
  non-constant textures;
- stochastic area-light sampling, soft shadows, MIS, environment-light sampling,
  BSDF sampling, Russian roulette, indirect diffuse/glossy transport, and
  caustics;
- animated transforms, moving primitives, unsupported primitive payloads, and
  any geometry the shared intersection service cannot compile for the selected
  backend;
- partial per-object GPU fallback inside one render. A scene is GPU Whitted
  eligible only when every required record and backend service is supported.

Transparent/glass full-GPU Whitted shading is intentionally out of GPU Whitted
v1. Transparent scenes may still use the shared compiled/packed intersection
service, but continuation and refraction shading stay CPU-owned instead of
being approximated as opacity, alpha blending, or mirror reflection. Full GPU
glass shading can be reconsidered only after hit metadata, inside/outside
medium state, nested IOR handling, and transparent any-hit semantics are
represented in the shared tracing records.

The recursion policy is iterative rather than device call-stack recursive. The
GPU schedule may be wavefront/depth-major or use an explicit bounded stack, but
it must publish the same depth limit, spawned-continuation counts, terminated
continuation counts, and throughput-cutoff behavior that the CPU Whitted
diagnostics expose. Reflection rays are the only v1 secondary continuation
kind; shadow rays remain any-hit visibility queries owned by the direct-light
stage.

GPU Whitted v1 must reuse these shared backend services:

- `render::GpuTracingScene` material, texture, light, and geometry records;
- `render::IntersectionService` or the same closest-hit/any-hit backend
  contract used by wavefront intersection batches;
- tracing backend capability records and fallback summaries;
- backend-owned frontier handles and path-state residency diagnostics;
- tracing accumulation clear/add/resolve services where GPU accumulation is
  selected.

No GPU Whitted job should add a private scene-record layout, private primitive
compiler, private fallback vocabulary, or private device-selection policy.
Unsupported scenes fall back visibly through the shared capability/fallback
surface.

**Deliverables:**

- Deterministic GPU direct lighting and shadows.
- Mirror reflection for supported material subset.
- Transparent/refraction only after hit metadata and nested-medium semantics
  are explicitly represented.
- Optional wavefront or iterative stack schedule.

**Tests/gates:**

- CPU Whitted vs GPU Whitted image parity on supported deterministic scenes.
- Packed-intersection eligibility tests for `TransparentMaterial`, plus
  full-GPU Whitted fallback tests until transparent shading is supported there.
- Reflection depth behavior matches CPU.

**Syrus-ready:** future. Not first.

---

## Milestone 12 - Hybrid GPU Visibility for Raster and Render Graph Effects

**Goal:** preserve and expose GPU intersection-only as a useful standalone
feature.

**Dependencies:** Milestone 2.

**Use cases:**

- Realistic real-time shadows.
- Hybrid raster ray-traced shadows.
- Ambient occlusion.
- Reflection/visibility probes.
- Portal/mirror visibility queries.
- Graph debug AOVs.

**Deliverables:**

- Render graph pass type or executor capability for visibility queries.
- Raster/OpenGL pass can request GPU any-hit or closest-hit visibility when
  available.
- Clear fallback to CPU/raster approximations when unavailable.
- Trace resources show visibility buffers separately from full tracing.

### Hybrid Visibility Graph Pass Contract

The hybrid visibility pass is a render-graph consumer of the intersection
service, not a tracing algorithm and not a full GPU path tracer. CPU-side graph
compilation, scene selection, camera/sample setup, light selection, material
evaluation, BSDF sampling, path continuation, contribution accumulation, and
tonemapping remain owned by the existing graph/integrator passes. The
visibility pass answers bounded ray-scene questions and writes graph-visible
visibility resources that later passes may consume.

Inputs are explicit graph resources or pass parameters:

- A compiled intersection scene handle plus diagnostics from the tracing scene
  compiler, or the runtime `render::Scene` fallback when the compiled subset is
  unavailable.
- One or more ray buffers. Each ray carries origin, direction, minimum distance,
  maximum distance, sample/pixel id, and an optional query id that lets callers
  map results back to lights, probes, tiles, or debug pixels.
- A query family: `any_hit`, `closest_hit`, or `mixed`. `any_hit` is the normal
  shadow/occlusion/visibility path. `closest_hit` is reserved for debug AOVs,
  probes, picking-style graph effects, or later passes that need the nearest
  supported surface record before CPU-side interpretation.
- Optional query tags such as `shadow`, `ambient_occlusion`,
  `visibility_probe`, or `debug_aov`. Tags are diagnostics and scheduling
  hints; they do not change intersection semantics.
- Backend request and fallback policy: `auto`, `cpu`, or `gpu`, plus whether
  the caller accepts CPU intersection fallback, raster approximation fallback,
  or disabled-output fallback.

Outputs are typed visibility resources, not shaded color:

- `visibility_mask`: one byte per submitted any-hit ray, in submission order;
  non-zero means occluded before `maxDistance`, zero means visible. This is the
  default output for ray-traced shadows, ambient occlusion, and occlusion AOVs.
- `visibility_hit_records`: optional closest-hit records for closest-hit or
  mixed queries. Records contain hit/miss state, primitive/material lookup id,
  distance, reconstructable hit position, normal, and primitive coordinates
  where available. They are suitable for CPU interpretation or debug display,
  but they are not material-shaded radiance.
- `visibility_metadata`: query counts, query-family split, output dimensions or
  query-list shape, execution path per family, backend/platform names, fallback
  reason, unsupported-scene counts, transfer estimates, and observed
  preparation/kernel/readback timing when available.
- Optional debug previews derived from the typed outputs, such as a grayscale
  occlusion mask or false-color closest-hit distance image. These previews are
  trace artifacts; downstream passes consume the typed resources above.

Resource domains follow the normal render graph rules. CPU visibility resources
use host buffers. GPU visibility resources may be produced by Metal, Vulkan, or
future hardware-ray-tracing kernels, but they must either expose an explicit
GPU-to-CPU readback edge before CPU consumers run or remain GPU-resident only
when every downstream consumer declares compatible GPU residency. The pass must
not hide uploads, readbacks, or CPU fallback behind an image-like resource name;
trace metadata records the real residency and transfer boundary.

Fallback behavior is part of the contract:

- `auto` may select CPU before scene compilation when the expected query count
  is too small to amortize GPU work.
- A requested GPU path may fall back to CPU when no platform device/render path
  is available, the scene cannot compile to the intersection subset, the query
  family lacks a platform kernel, preparation/dispatch fails, or returned result
  counts are malformed.
- A raster shadow caller may choose a raster approximation fallback when
  ray-query visibility is unavailable, but the trace must report that no
  hybrid visibility query ran.
- A debug/AOV caller may choose disabled-output fallback, producing a typed
  empty/default resource plus an explicit fallback reason instead of silently
  substituting a shaded render.

The pass preserves the intersection service's any-hit and closest-hit
semantics. Any-hit queries only answer occluded/visible for bounded rays and do
not evaluate materials, transparency, alpha blending, or contribution. Closest
hit queries only return the nearest supported surface record and do not shade,
spawn continuation rays, sample lights, accumulate radiance, or claim ownership
of path state. A graph or UI label may call this "GPU visibility" or "hybrid
ray-traced shadows"; it must not report "GPU path tracing" unless the shading,
sampling, continuation, and accumulation capabilities are also GPU-owned by a
separate tracing pass.

**Tests/gates:**

- AOV/visibility pass renders expected occlusion.
- Raster/hybrid scene visibly changes when ray-traced shadows are enabled.
- Metrics show GPU intersection service usage without claiming full GPU
  tracing.

**Syrus-ready:** yes after the intersection service API is stable.

---

## Milestone 13 - Render Graph and Modeler Selection Model

**Goal:** make CPU, hybrid, and GPU execution selectable through intent without
letting users manually prescribe internal graph nodes.

**Dependencies:** Milestone 1. Can run in parallel with backend work.

**Render intent should express:**

- algorithm preference:
  - Whitted;
  - path tracing;
  - future algorithms;
- schedule preference:
  - scalar;
  - wavefront;
  - backend default;
- execution preference:
  - CPU;
  - auto;
  - hybrid;
  - GPU;
  - platform-specific only in advanced/debug UI;
- quality:
  - samples per pixel;
  - max depth;
  - direct-light samples;
  - denoiser;
  - adaptive sampling;
  - future light/path sampling controls.

**Compiler should decide:**

- actual graph passes;
- backend mode;
- fallback mode;
- resource residency;
- trace fields.

**Deliverables:**

- Replace "intersection backend" UI prominence with a broader "Tracing
  backend" or "Execution" group.
- Keep intersection backend visible in advanced diagnostics.
- Render dialog graph tab shows predicted execution before render.
- After render, graph trace shows actual execution and fallback.
- Property editor groups capability metadata cleanly.

**Tests/gates:**

- Intent round-trip tests.
- Render graph compiler tests for CPU/hybrid/GPU/fallback decisions.
- Modeler widget tests for visibility of relevant controls.
- No invalid settings can be selected.

**Syrus-ready:** partially. Compiler tests are delegateable; UI naming and
interaction should be interactive.

---

## Milestone 14 - Performance and Auto-Selection Gates

**Goal:** do not make any GPU path automatic until correctness and performance
are proven on representative scenes.

**Dependencies:** Milestones 2, 3, and any backend mode being considered for
auto.

**Deliverables:**

- Benchmark scenes:
  - small educational primitive scene;
  - large imported mesh;
  - visibility-heavy direct-light scene;
  - indirect path-tracing scene;
  - unsupported fallback scene.
- Metrics:
  - total render time;
  - kernel time;
  - upload/readback time;
  - scene compile/upload time;
  - rays per second by query family;
  - resident path-state bytes;
  - saved round trips;
  - fallback rate.
- Auto policy:
  - platform available;
  - scene supported;
  - expected work clears threshold;
  - recent benchmark evidence supports the threshold.

**Tests/gates:**

- Benchmarks are buildable under CPU, Metal, Vulkan presets.
- Functional tests only assert conservative behavior, not machine-specific
  speedups.
- Performance claims are documented with captured numbers.

**Syrus-ready:** yes for benchmark harness expansion. Threshold decision should
be interactive.

---

## Milestone 15 - Hardware Ray Tracing Backends

**Goal:** add Vulkan RT and Metal ray tracing after compute backends prove the
algorithm/backends split.

**Dependencies:** Milestones 1, 3, 4, and at least one full or hybrid compute
backend mode.

**Deliverables:**

- Hardware acceleration structure compiler.
- Capability reporting separate from compute BVH traversal.
- Same algorithm/schedule interfaces as compute backend.
- Explicit support/fallback by platform.

**Tests/gates:**

- Hit/occlusion parity with CPU and compute backend.
- Scene support/fallback parity.
- Performance capture on supported hardware.

**Syrus-ready:** future. Do not delegate until compute backend architecture is
stable.

---

## Milestone 16 - Additional Algorithms

**Goal:** grow the repository into the intended rendering-algorithm playground.

**Dependencies:** CPU/GPU backend architecture stable enough for algorithm
plugins.

**Future algorithms:**

- bidirectional path tracing;
- photon mapping;
- progressive photon mapping;
- Metropolis light transport;
- ReSTIR direct lighting;
- ReSTIR GI;
- volume path tracing;
- spectral rendering;
- participating media;
- differentiable rendering experiments.

Each new algorithm should define:

- CPU reference implementation first, unless it is inherently GPU-first;
- supported schedule(s);
- backend capability requirements;
- debug AOVs and metrics;
- textbook explanation;
- rendercli and Modeler controls;
- rendered examples.

**Syrus-ready:** future. Each algorithm should become its own plan/epic.

---

## Testing Matrix

### Always-on CPU Tests

- Unit tests for scene compilation records.
- CPU reference integrator tests.
- CPU vs packed CPU intersection parity.
- Unsupported-scene fallback reasons.
- Capability serialization.
- rendercli option validation.
- graph trace metadata validation.

### Optional Platform Tests

- Metal tests under `release-metal-wavefront`.
- Vulkan tests under `release-vulkan-wavefront`.
- Tests skip cleanly if the build preset or runtime device is unavailable.

### Visual/Functional Tests

- Low-resolution deterministic image parity for supported scenes.
- Statistical RMS comparisons for path-tracing scenes with fixed seeds.
- Fallback scenes that must render correctly on CPU when GPU support is
  incomplete.
- Modeler graph inspection for backend/fallback metadata.

### Performance Tests

- Benchmarks are not required to pass on absolute timing in CI.
- Speed claims require captured benchmark numbers.
- Auto-selection thresholds must be justified by benchmark evidence.

---

## Documentation Requirements

Every user-visible tracing backend milestone should update:

- rendercli help/behavior docs;
- Modeler/render settings docs;
- render graph trace docs;
- wavefront/path-tracing textbook chapter or a new GPU tracing chapter;
- rendered example images where output is visible;
- changelog for behavior-affecting changes.

The textbook should be explicit about:

- algorithm vs schedule vs backend;
- hybrid GPU intersection vs full GPU tracing;
- why matching images are success for backend changes;
- which metrics prove the backend path was used.

---

## Syrus Epic Graph

This is the concrete graph to file in Syrus. Epics are containers. Epic
dependencies should be used for coarse ordering. Jobs should mostly depend on
jobs inside the same epic. If a job seems to need a job inside another epic, the
preferred fix is to split the producing work into its own epic or add an epic
dependency.

The graph is a DAG:

```text
Root implementation epics:
  E1 Intersection service consolidation
  E2 Cross-backend parity harness
  E3 GPU tracing scene data
  E4 GPU sample stream
  E5 GPU accumulation buffer
  E6 Execution diagnostics and capability reporting

E1 + E2 + E3 + E4 + E6
  -> E7 GPU diffuse direct lighting

E1 + E2 + E3 + E4 + E5 + E7
  -> E8 GPU diffuse path step

E2 + E5 + E6 + E8
  -> E9 GPU resident path loop v1

E6 + E9
  -> E10 Render graph, rendercli, and Modeler execution integration

E9 + E10
  -> E11 Textbook, examples, and rendered comparisons
  -> E12 Performance gates and automatic selection

E1 + E6
  -> E13 Hybrid visibility and ray-traced shadows

E1 + E2 + E3 + E5 + E8 + E10
  -> E14 GPU Whitted v1
```

This plan is the alignment artifact. Syrus epics begin where coding begins, so
there is no planning/meta epic.

### E1 - Intersection Service Consolidation

**Epic dependencies:** none.

**Purpose:** finish GPU intersection as a reusable closest-hit/any-hit service
for tracing, shadows, visibility, and graph passes.

**Jobs:**

1. ~~**Write the service contract.**~~ ✅ **Done.** The Intersection Service
   Contract section documents query families, supported primitive subset,
   output records, timing diagnostics, fallback reasons, and the fact that this
   is a backend service rather than full GPU tracing.
   - Depends on: none.
   - Output: document/code comments identifying supported query families,
     supported primitive subset, fallback reasons, timing fields, and output
     records.

2. ~~**Close parity gaps for existing supported primitives.**~~ ✅ **Done.**
   Issue #570 adds explicit packed CPU closest-hit/any-hit parity coverage for
   triangle, mesh triangle, finite-width Curve tessellation, sphere, plane,
   rectangle, disk, OpenCylinder, Torus, and static transforms, and keeps mesh
   triangles represented in optional Metal/Vulkan triangle smoke parity scenes.
   - Depends on: job 1.
   - Output: CPU runtime, packed CPU, Metal, and Vulkan closest-hit/any-hit
     parity tests for triangle, mesh triangle, finite-width Curve tessellation,
     sphere, plane, rectangle, disk, OpenCylinder, Torus, and static transforms.

3. ~~**Stabilize explicit fallback behavior.**~~ ✅ **Done.** Issue #571 pinned
   deterministic GPU-intersection fallback reasons; transparent/glass now has
   packed-intersection eligibility tests, while generic unsupported scenes still
   prove fallback behavior in backend and metrics tests.
   - Depends on: job 1.
   - Output: ~~tests for transparent/glass and unsupported scene features
     proving they fall back before rendering or report why they cannot use the
     service.~~

4. ~~**Expose an intersection-only service entry point.**~~
   - Depends on: jobs 1 and 2.
   - Output: API usable by graph visibility/AOV passes and future raster hybrid
     shadows without pretending to run a full tracing algorithm on the GPU. ✅
     **Done.** `render::IntersectionService` prepares a selected backend for a
     scene, submits closest-hit and any-hit queries, and retains execution-path
     plus fallback diagnostics for callers.

5. ~~**Add service benchmarks and metrics capture.**~~ ✅ **Done.** Issue #573
   adds `IntersectionService` benchmark rows for closest-hit, any-hit, and
   mixed query-family workloads over small supported, mesh-heavy supported, and
   visibility-heavy supported scenes, with transfer, timing, execution-path,
   and query-family counters.
   - Depends on: jobs 2 and 4.
   - Output: benchmarks for closest-hit and any-hit on supported small and large
     scenes, with upload/kernel/readback timing and query-family counts.

**Gate:** a user can request GPU intersection, see whether it ran on Metal,
Vulkan, packed CPU, or runtime CPU, and trust that supported queries match CPU
semantics.

### E2 - Cross-Backend Parity Harness

**Epic dependencies:** none.

**Purpose:** create the shared verification surface every backend slice must
use before it can be called correct.

**Jobs:**

1. ~~**Define canonical parity scenes.**~~ ✅ **Done.** The
   `test/fixtures/tracing_parity/` manifest now tracks matte direct-light,
   indirect-bounce, imported-mesh, transparent-fallback, and visibility-heavy
   scenes for backend parity.
   - Depends on: none.
   - Output: scene list covering supported matte/direct-light, indirect bounce,
     mesh import, transparent fallback, and visibility-heavy workloads.

2. ~~**Add record comparison helpers.**~~ ✅ **Done.** Issue #575 adds
   reusable GoogleTest helpers for GPU hit records, GPU occlusion records,
   compiled CPU hits versus GPU hits, and wavefront closest-hit results.
   - Depends on: job 1.
   - Output: helpers for closest-hit records, occlusion records, material/light
     ids, hit distance, normals, UV/barycentric/local coordinates, and miss
     records.

3. ~~**Add image comparison helpers.**~~ ✅ **Done.** `TracingImageComparison`
   helpers report normalized RMS/channel deltas for HDR/RGB buffers and cover
   identical, within-threshold, over-threshold, and dimension-mismatch cases.
   - Depends on: job 1.
   - Output: deterministic RMS checks and stochastic fixed-seed comparison
     helpers with clear tolerances.

4. ~~**Add platform-skip behavior.**~~ ✅ **Done.** Optional Metal/Vulkan parity
   and accumulation tests now skip cleanly when the build preset or runtime
   device/path is unavailable.
   - Depends on: jobs 2 and 3.
   - Output: Metal/Vulkan tests skip cleanly when the build preset or runtime
     device is unavailable.

5. ~~**Wire rendercli functional parity tests.**~~ ✅ **Done.**
   `rendercli_tracing_parity` compares canonical fixtures across CPU and
   GPU-requested paths and checks execution-path metadata.
   - Depends on: jobs 2, 3, and 4.
   - Output: rendercli tests comparing CPU vs packed/GPU requests and checking
     trace metadata for the path that actually ran.

**Gate:** new backend work can prove correctness without inventing one-off
comparison logic.

### E3 - GPU Tracing Scene Data

**Epic dependencies:** none.

**Purpose:** compile the scene data the GPU needs to shade, not just intersect.

**Jobs:**

1. ~~**Define compiled tracing-scene sections.**~~ ✅ **Done.**
   `render::GpuTracingSceneSections` now defines version-1 geometry,
   material, texture, light, environment, and debug-id sections for GPU
   tracing scene data, with a separate shading work record from the
   intersection hit record. Closes #579.
   - Depends on: none.
   - Output: geometry, material, texture, light, environment, and debug-id
     sections with versioned layout expectations.

2. ~~**Compile material records.**~~ ✅ **Done.**
   `render::compileGpuTracingMaterials` now packs Matte, Phong, Reflective,
   and Emissive materials into GPU tracing records keyed by runtime material
   id, with unsupported material reasons counted for tracing scene diagnostics.
   Closes #580.
   - Depends on: job 1.
   - Output: records for Matte, Phong, Reflective, and Emissive; explicit
     unsupported reasons for all other materials.

3. ~~**Compile texture records.**~~ ✅ **Done.**
   `render::GpuTracingTextureCompilation` now packs ConstantColor textures,
   planar/UV CheckerBoard texture references, nearest/bilinear ImageTexture
   records, base-level bilinear records for mipmapped image textures,
   UVColorTexture records, and bounded Tinted wrapper chains into GPU tracing
   records keyed by material-referenced texture ids, with unsupported texture
   reasons counted for tracing scene diagnostics. Closes #581.
   - Depends on: job 1.
   - Output: records for ConstantColor, planar/UV CheckerBoard texture graphs,
     nearest/bilinear ImageTexture records, UVColorTexture, and bounded Tinted
     wrapper chains; unsupported reasons otherwise.

4. ~~**Compile light records.**~~ ✅ **Done.**
   `render::GpuTracingLightCompilation` now packs PointLight,
   DirectionalLight, and RectangularAreaLight into stable GPU tracing light
   records and counts unsupported light types by reason. Closes #582.
   - Depends on: job 1.
   - Output: records for PointLight, DirectionalLight, and RectangularAreaLight;
     unsupported reasons for the rest.

5. ~~**Add record round-trip and unsupported-reason tests.**~~ ✅ **Done.**
   GPU tracing scene tests now prove material, texture, and light record counts,
   runtime-id mappings, first unsupported reasons, and grouped reason counts.
   Closes #583.
   - Depends on: jobs 2, 3, and 4.
   - Output: tests proving runtime ids map to compiled records and unsupported
     features are counted by reason.

6. ~~**Expose compiled tracing-scene diagnostics.**~~ ✅ **Done.**
   Metrics JSON, rendercli summaries, and graph traces now report compiled GPU
   tracing material, texture, light, environment, debug-id, upload-byte, and
   unsupported reason counts. Closes #584.
   - Depends on: job 5.
   - Output: trace/metrics counts for compiled materials, textures, lights, and
     unsupported reasons.

**Gate:** a supported diffuse path-tracing scene can be represented by stable
GPU-readable records for geometry, materials, textures, and lights.

### E4 - GPU Sample Stream

**Epic dependencies:** none.

**Purpose:** let CPU and GPU generate deterministic stochastic samples from the
same seed/sample-index contract.

The first GPU tracing sample generator is a stateless 32-bit PCG hash
(`pcg_hash32`) evaluated from an explicit sample coordinate. The generator is a
deterministic building block, not a new renderer:

```cpp
uint32_t pcg_hash32(uint32_t input) {
  uint32_t state = input * 747796405u + 2891336453u;
  uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
  return (word >> 22u) ^ word;
}
```

Each stochastic request is addressed by the same logical coordinate already
reserved by `SampleStream`: `(seed, pixelIndex, primarySampleIndex, dimension,
component)`. The dimension comes from `sampleDimensionIndex(name, slot)`.
Pixel, time, and lens own dimensions 0, 1, and 2; path-tracing dimensions repeat
in four-slot groups for BSDF, light-surface, direct-light selection, and
Russian-roulette continuation. The coordinate is folded through fixed unsigned
32-bit integer mixes before the final `pcg_hash32` call; future fixed-vector
tests must pin the exact packing/mixing constants before production code
depends on this stream.

Floating-point samples are produced by taking the high 24 bits of the hash and
multiplying by `2^-24`, yielding a value in `[0, 1)`. That rule is intentional:
it avoids platform `uniform_real_distribution` behavior, avoids CPU extended
precision differences, and maps cleanly to `float` precision on GPU shaders.
The CPU reference implementation lives in `render::GpuSampleStream` and is
kept independent of the existing stratified sampler set path.

### Rationale

- **Schedule-independent.** A sample is a pure function of its coordinate, so
  scalar CPU, wavefront CPU, and GPU queues can request dimensions in different
  orders without changing the stream.
- **GPU-friendly.** The hash uses only unsigned 32-bit multiply, add, xor, and
  shift operations. Those are widely available in C++, GLSL, MSL, WGSL, and
  SPIR-V without relying on 64-bit integer support.
- **No hidden RNG state.** The contract forbids `std::random_device`,
  `std::mt19937`, `std::uniform_real_distribution`, Qt random helpers,
  shader `fract(sin(...))` tricks, hardware RNGs, or any per-thread/per-platform
  generator for this stream.
- **Easy parity testing.** Fixed input coordinates produce fixed `uint32_t` and
  `[0, 1)` values that can be checked in ordinary unit tests and mirrored in
  shader conformance tests.

### Limitations

- This is a pseudorandom white-noise baseline, not a low-discrepancy sequence.
  Sobol, Owen-scrambled Sobol, blue-noise tiles, and stratified reconstruction
  remain future sampling-quality work.
- It is not cryptographic and must not be used for secrets, randomized file
  formats, or adversarial input handling.
- The initial 24-bit float mapping is sufficient for renderer sampling parity,
  but not for APIs that require full 32-bit integer entropy as a float.
- The exact coordinate packing is part of the ABI once fixed-vector tests land;
  changing it is a behavior change and must be versioned or intentionally
  migrated.

**Jobs:**

1. ~~**Define sample dimensions and indices.**~~ ✅ **Done.**
   `SampleDimension` and `sampleDimensionIndex(...)` reserve stable pixel,
   time, lens, BSDF, light, light-selection, and continuation dimensions.
   - Depends on: none.
   - Output: pixel, lens/time, BSDF, light, Russian-roulette, and continuation
     dimensions with sample-index mapping.

2. **~~Choose and document the first generator.~~** ✅ **Done.** The initial
   parity stream is a stateless 32-bit PCG hash over explicit sample
   coordinates, with platform RNGs excluded from the contract.
   - Depends on: job 1.
   - Output: deterministic generator choice with rationale and limits.

3. **~~Add CPU reference implementation.~~** ✅ **Done.** `render::GpuSampleStream`
   provides the CPU reference generator with explicit seed, pixel,
   primary-sample, dimension, and component coordinates.
   - Depends on: job 2.
   - Output: generator callable from tests and future GPU parity code.

4. ~~**Add tests for fixed seeds and dimensions.**~~ ✅ **Done.**
   `GpuSampleStreamTest` pins the PCG hash, coordinate purity, named-dimension
   mapping, fixed vectors, sequential reads, primary-sample behavior, and
   half-open interval bounds.
   - Depends on: job 3.
   - Output: stable test vectors for the named sample dimensions.

5. ~~**Expose seed/stream diagnostics.**~~ ✅ **Done.** Raytracer pass state,
   render engine options, wavefront metrics input, and rendercli summaries now
   carry `sampleStreamMode` and sampling seed where relevant.
   Structured tracing capabilities also use that mode: ordinary sampler renders
   report that GPU sampling was not requested, while `gpu_sample_stream` renders
   report the CPU reference implementation as `gpu_sample_stream_cpu_reference`.
   - Depends on: job 4.
   - Output: rendercli/trace fields showing seed and stream mode where relevant.

**Gate:** future GPU kernels can sample the same dimensions as the CPU reference
without hidden RNG choices.

### E5 - GPU Accumulation Buffer

**Epic dependencies:** none.

**Purpose:** let the backend own HDR color sums and sample counts before full
GPU shading lands.

**Jobs:**

1. ~~**Define accumulation layout.**~~ ✅ **Done.**
   `render::TracingAccumulationLayout` defines HDR color sums, sample counts,
   optional second raw moments, resolve format, byte accounting, and stable
   format names.
   - Depends on: none.
   - Output: color sum, sample count, optional variance moments, dimensions,
     and resolve format.

2. ~~**Add CPU reference clear/add/resolve.**~~ ✅ **Done.**
   `render::TracingAccumulationBuffer` is the CPU reference implementation with
   coverage for clear, add, multi-sample averaging, optional moments, tonemapped
   resolve, diagnostics, shape validation, bounds checks, and overflow.
   - Depends on: job 1.
   - Output: deterministic reference implementation and tests.

3. ~~**Add Metal clear/add/resolve kernels.**~~ ✅ **Done.** Optional
   `render::MetalTracingAccumulationBuffer` kernels clear/add/resolve the v1
   accumulation planes and compare against the CPU reference on synthetic
   inputs when Metal is enabled and available.
   - Depends on: job 2.
   - Output: optional-platform kernels with skip behavior when unavailable.

4. ~~**Add Vulkan clear/add/resolve kernels.**~~ ✅ **Done.** Optional
   Vulkan compute kernels now clear accumulation planes, add synthetic
   full-frame sample colors, and resolve to the LDR output plane, with CPU
   reference parity tests that skip when Vulkan is disabled or unavailable.
   - Depends on: job 2.
   - Output: optional-platform kernels with skip behavior when unavailable.

5. ~~**Expose accumulation resource diagnostics.**~~ ✅ **Done.** Wavefront
   metrics now expose accumulation backend, residency, bytes, layout formats,
   clear/add/resolve/readback operation counts, and readback bytes.
   - Depends on: jobs 2, 3, and 4.
   - Output: residency, bytes, clears, adds, resolves, and readback counts in
     trace/metrics.

**Gate:** synthetic sample colors resolve identically through CPU reference and
available GPU accumulation paths.

### E6 - Execution Diagnostics and Capability Reporting

**Epic dependencies:** none.

**Purpose:** make CPU, hybrid, and GPU execution visible without centering the
architecture on "intersection backend".

**Jobs:**

1. ~~**Inventory current metric fields.**~~ ✅ **Done.** Archived in
   `docs/plans/complete/tracing-execution-diagnostics-inventory.md`.
   - Depends on: none.
   - Output: mapping from existing wavefront/intersection metrics to broader
     tracing execution concepts.

2. ~~**Add tracing execution capability records.**~~ ✅ **Done.**
   `render::TracingExecutionCapabilityRecords` now models intersection, scene
   records, sampling, direct lighting, BSDF, path state, accumulation, and
   fallback state with unit coverage.
   - Depends on: job 1.
   - Output: C++ data structures for intersection, scene records, sampling,
     direct lighting, BSDF, path state, accumulation, and fallback status.

3. **~~Serialize capabilities and preserve aliases.~~** ✅ **Done.** Wavefront
   metrics now serialize tracing capability records and rendercli aliases while
   preserving existing intersection backend fields. Compact rendercli summaries
   print fallback and restricted capability lists separately, so CPU-reference
   contracts stay visible without being mislabeled as fallback.
   - Depends on: job 2.
   - Output: JSON/rendercli output with compatibility aliases for existing
     consumers.

4. **~~Group Modeler graph metadata.~~** ✅ **Done.** Modeler selected-pass
   rows now summarize tracing execution as CPU, hybrid, GPU, fallback,
   restricted, and unsupported capability groups before the legacy intersection
   detail aliases.
   - Depends on: job 3.
   - Output: selected-pass properties present execution capabilities as grouped
     CPU/hybrid/GPU state, not a flat dump of intersection fields.

5. ~~**Add tests for CPU, GPU request, and fallback summaries.**~~ ✅ **Done.**
   Wavefront metrics tests cover serialized capability arrays and fallback
   summaries; the Modeler graph inspector test pins the grouped tracing backend
   row before legacy intersection details.
   - Depends on: jobs 3 and 4.
   - Output: unit/rendercli/widget tests proving the diagnostics are stable.

**Gate:** users can tell what ran where, what fell back, and why, even when the
image intentionally matches CPU output.

### E7 - GPU Diffuse Direct Lighting

**Epic dependencies:** E1, E2, E3, E4, E6.

**Purpose:** move the first real shading work to the GPU: diffuse direct light
for the supported scene subset.

**Jobs:**

1. ~~**Define direct-light work records.**~~ ✅ **Done.** Issue #600 adds
   documented GPU diffuse direct-light surface, sample-state, light-selection,
   and visibility records with unit tests for layout and sample-dimension
   addressing.
   - Depends on: none.
   - Output: hit point/reconstructible hit data, normal, material id, incoming
     direction, throughput, sample state, and light-selection inputs.

2. ~~**Add CPU reference direct-light batch.**~~ ✅ **Done.** Issue #601 adds
   record-based CPU reference visibility and contribution batches over packed
   tracing scene records for GPU parity tests.
   - Depends on: job 1.
   - Output: record-based CPU implementation matching current path-tracer
     direct-light semantics.

3. ~~**Implement supported light sampling.**~~ ✅ **Done.** Issue #602 adds
   compiled-record light selection and sampling for point, directional, and
   rectangular area lights, with fixed-sample parity against runtime
   `LightSampler` / `Light::sample` and explicit unsupported-record fallback
   status.
   - Depends on: jobs 1 and 2.
   - ~~Output: point, directional, and rectangular area light sampling for the
     compiled light records.~~

4. ~~**Implement diffuse contribution evaluation.**~~ ✅ **Done.** Issue #603
   pins matte diffuse direct-light contribution evaluation with analytic
   point-light, occlusion, invalid-PDF, and non-delta area-light MIS tests over
   the packed CPU/GPU work records.
   - Depends on: jobs 2 and 3.
   - ~~Output: Matte diffuse direct-light contribution with MIS behavior matching
     CPU reference.~~

5. ~~**Connect visibility through the intersection service.**~~ ✅ **Done.** Issue #604
   routes packed direct-light visibility records and path-tracer direct-light
   any-hit frontiers through `render::IntersectionService`, preserving
   occlusion flags for contribution evaluation and capability metrics that
   distinguish visibility GPU from CPU contribution.
   - Depends on: jobs 3 and 4.
   - ~~Output: any-hit visibility queries use E1's service and produce occluded
     flags for contribution evaluation.~~

6. ~~**Add parity tests and metrics.**~~ ✅ **Done.** Issue #605 adds
   CPU-vs-GPU-requested matte direct-light image parity assertions and direct
   light contribution execution/fallback fields in metrics JSON, rendercli
   summaries, and tracing capability records.
   - Depends on: job 5.
   - ~~Output: analytic diffuse tests, MIS tests, image/record parity, and trace
     fields distinguishing visibility-only GPU from contribution GPU.~~

**Gate:** a supported matte direct-light scene can compute direct-light
contributions through the GPU path and match the CPU estimator.

### E8 - GPU Diffuse Path Step

**Epic dependencies:** E1, E2, E3, E4, E5, E7.

**Purpose:** execute one GPU path-tracing bounce for supported diffuse paths.

**Jobs:**

1. ~~**Define path-state record.**~~ ✅ **Done.** `render::GpuDiffusePathStateRecord`
   defines the v1 diffuse path-step state with ray, throughput, accumulated
   radiance, pixel/sample ids, depth, sample cursor, active/terminated flags,
   and previous-event MIS metadata. Closes #606.
   - Depends on: none.
   - Output: ray, throughput, accumulated contribution target or sample id,
     depth, RNG/sample state, flags, and MIS metadata.

2. ~~**Add CPU reference path-step kernel.**~~ ✅ **Done.**
   `render::GpuDiffusePathStepReference` advances record-based diffuse path
   states for hit, miss, emission, direct-light, and diffuse-continuation
   cases with fixed-seed coverage. Closes #607.
   - Depends on: job 1.
   - Output: record-based one-bounce implementation for comparison.

3. ~~**Run closest-hit and material lookup for a path step.**~~ ✅ **Done.**
   `render::GpuDiffusePathStep` now submits active path rays through the packed
   closest-hit intersector, forwards the resulting hit/miss records into the
   diffuse path-step material lookup, and reports unsupported compiled material
   hits as explicit terminated path records. Closes #608.
   - Depends on: jobs 1 and 2.
   - Output: path states intersect through E1 and resolve supported material
     records from E3.

4. ~~**Add emission and direct-light contribution.**~~ ✅ **Done.**
   `render::GpuDiffusePathStep` now carries supported emissive hits and matte
   direct-light samples into the path-state accumulated radiance record, while
   path-step metrics report packed closest-hit/visibility paths and CPU record
   contribution-evaluation paths. Closes #609.
   - Depends on: jobs 2 and 3.
   - Output: supported emission/direct-light contribution feeds E5
     accumulation or the CPU reference equivalent.

5. ~~**Sample diffuse continuation.**~~ ✅ **Done.** `render::GpuDiffusePathStep`
   now emits a compact next path-state frontier only for surviving diffuse
   continuations, using the GPU sample-stream BSDF and continuation dimensions
   to populate direction, PDF, throughput, previous-event flags, and
   Russian-roulette termination behavior. Closes #610.
   - Depends on: jobs 2, 3, and 4.
   - Output: next path-state records using E4 sample dimensions.

6. ~~**Add one-bounce parity tests.**~~ ✅ **Done.** `GpuDiffusePathStep`
   now has wrapper-vs-reference one-bounce parity tests for miss, diffuse
   continuation, direct-light contribution, and explicit unsupported-material
   fallback records. Closes #611.
   - Depends on: job 5.
   - Output: CPU/GPU path-step records and accumulated contribution match for
     fixed seeds on supported diffuse scenes.

**Gate:** one supported diffuse bounce can run as a backend operation, producing
the same continuation and contribution records as the CPU reference.

### E9 - GPU Resident Path Loop v1

**Epic dependencies:** E2, E5, E6, E8.

**Purpose:** turn the one-bounce path step into a multi-bounce GPU-resident
diffuse path tracer for the first supported subset.

**Jobs:**

1. ~~**Add active/next path buffers.**~~ ✅ **Done.** Added
   `render::TracingPathStateLayout` and CPU reference ping-pong buffers for
   issue #612.
   - Depends on: none.
   - Output: ping-pong buffers for current and next path states.

2. **~~Add GPU/CPU compaction contract.~~**
   - Depends on: job 1.
   - Output: ~~retained-index, removed-count, moved-count, and execution-path
     contract matching existing host compaction metrics.~~ ✅ **Done.**
     `ResidentPathCompactionContract` mirrors the host compaction counters with
     32-bit retained indices and resident path-state byte accounting for
     issue #613, while the host path-state frontier rejects compaction results
     whose declared input path count does not match the active frontier.
     `ResidentPathCompactionBackend` now gives the resident path loop an
     explicit compaction dispatch point, with the default CPU backend reporting
     `cpu_resident_path_compaction` so metrics no longer classify host
     compaction as GPU execution. Metal-enabled builds now also have a
     standalone `MetalResidentPathCompactionBackend` that compacts retained
     `GpuPathStateRecord` entries with a Metal compute kernel for the same
     retained-index contract. The live compiled diffuse path loop now has its
     own `GpuDiffusePathFrontierCompactionBackend` seam for the
     `GpuDiffusePathStateRecord` representation actually used by graph-backed
     renders, and trace metadata reports that compaction execution separately
     from the overall CPU-reference path-loop execution. Metal-enabled builds
     now also have `MetalGpuDiffusePathFrontierCompactionBackend`, a compute
     kernel that compacts the live diffuse path-loop state-record layout.
     Vulkan-enabled Linux builds now also have
     `VulkanGpuDiffusePathFrontierCompactionBackend`, using the same retained
     index contract and live diffuse path-loop state-record layout.
     `CompactingGpuDiffusePathLoopBackend` can now run the compiled
     CPU-reference loop through an injected frontier-compaction service, so
     graph traces classify that middle state as hybrid execution when a Metal
     or Vulkan compaction path participates, while still reporting the overall
     path loop as `compiled_cpu_reference` until a full platform path-loop
     kernel exists. The graph engine now selects that platform compaction path
     automatically for GPU-requested compiled diffuse path-loop renders when
     Metal or Vulkan support is built and the compaction backend is available,
     falling back to the plain CPU-reference loop otherwise.

3. **~~Loop over depth with max-depth and Russian roulette.~~** ✅ **Done.**
   Added `loopResidentDiffusePaths` as the CPU-reference resident path-state
   loop with active/next ping-pong, max-depth draining, GPU sample-stream
   Russian roulette, and fixed-seed retained-record parity diagnostics for
   issue #614. Resolved records now clear their active flag before image
   resolve so terminal CPU-reference path states are distinct from live
   frontier state, image resolve rejects active records at its boundary, and
   active frontiers reject inactive records instead of silently dropping them.
   - Depends on: jobs 1 and 2.
   - Output: ~~supported diffuse paths execute multiple bounces and terminate
     according to the same policy as CPU.~~

4. ~~**Resolve accumulated image.**~~ ✅ **Done.**
   - Depends on: jobs 1 and 3.
   - Output: ~~E5 accumulation resolves to rendercli/Modeler-visible image
     data.~~ Supported live graph renders now route GPU-requested diffuse
     path-tracing scenes through the compiled CPU-reference diffuse path loop,
     resolve terminal records through E5 accumulation diagnostics, and expose
     the result in rendercli, Modeler preview, and the render dialog trace as
     `compiled_cpu_reference`. Capability records now keep that overall
     missing-platform-kernel fallback separate from the direct-light, BSDF,
     continuation, path-state, compaction, and accumulation subsystems still
     owned by the CPU reference loop.

5. ~~**Add end-to-end parity scenes.**~~ ✅ **Done.** Added fixed-seed
   rendercli CPU vs GPU-requested parity coverage for direct, indirect,
   environment, and explicit unsupported fallback scenes for issue #616.
   - Depends on: jobs 3 and 4.
   - Output: ~~fixed-seed CPU vs GPU diffuse path-tracing comparisons for
     direct, indirect, environment, and unsupported fallback scenes.~~

6. **~~Update residency metrics from estimates to actuals.~~** ✅ **Done.**
   Added resident path-loop actual execution counters for issue #617, surfaced
   through wavefront JSON, rendercli summaries, and Modeler graph details.
   - Depends on: jobs 2, 3, and 5.
   - Output: ~~trace shows resident path-state bytes, actual compaction execution,
     actual round trips, and actual saved host readbacks.~~

**Gate:** a supported diffuse scene can render end-to-end through the GPU
path-loop subset and compare against the CPU path tracer.

### E10 - Render Graph, rendercli, and Modeler Execution Integration

**Epic dependencies:** E6, E9.

**Purpose:** make the new execution modes usable without asking users to
manually request internal graph nodes.

**Jobs:**

1. **Define user-facing execution controls.**
   - Depends on: none.
   - Output: ~~render intent fields and valid values for CPU, hybrid, GPU, and
     auto execution preferences.~~ ✅ **Done.** `tracingExecution` now defines
     the durable intent values and documents transition rules for the existing
     intersection-backend service control in issue #618.

2. **Teach the graph compiler to synthesize execution modes.**
   - Depends on: job 1.
   - Output: ~~graph compiler picks CPU, hybrid intersection, or full GPU
     subset from intent, scene support, and backend availability.~~ ✅
     **Done.** Issue #619 records predicted CPU/hybrid/GPU modes and fallback
     reasons in compiled tracing pass state, then adds actual mode metadata to
     graph traces.

3. **Update rendercli flags and validation.**
   - Depends on: jobs 1 and 2.
   - Output: ~~rendercli can request execution preference and rejects invalid
     combinations with useful errors.~~ ✅ **Done.** Issue #620 adds
     `--tracing_execution auto|cpu|hybrid|gpu`, exports it in graph pass state,
     and validates unsupported or conflicting CLI combinations.

4. **Update Modeler Render Settings.**
   - Depends on: jobs 1, 2, and 3.
   - Output: ~~grouped controls expose tracing execution without making internal
     backend services the primary user model.~~ ✅ **Done.** Issue #621 adds
     Modeler Render Settings controls for `auto`, `cpu`, `hybrid`, and `gpu`
     tracing execution, while keeping the intersection backend as a
     hybrid-only advanced override.

5. **Show predicted and actual graph execution.**
   - Depends on: jobs 2 and 4.
   - Output: ~~render dialog graph tab shows predicted mode; executed trace
     shows actual mode and fallback.~~ ✅ **Done.** Issue #622 adds predicted
     tracing execution rows to the Modeler render dialog graph tab and feeds the
     completed graph trace back into the dialog with actual mode/fallback rows.

6. **Dispatch compiled path loops through a backend seam.**
   - Depends on: jobs 2 and 5.
   - Output: ~~graph payloads call a path-loop execution backend instead of
     constructing the CPU-reference diffuse loop directly.~~ ✅ **Done.**
     `render::GpuDiffusePathLoopBackend` now owns the compiled diffuse
     path-loop dispatch point, `GraphRenderEngine` carries the selected backend
     through render clones, and graph metadata can now reflect an injected
     full-GPU path-loop result without CPU-reference fallback text. Injected
     GPU frontier-compaction services are also visible as hybrid actual
     execution with their own timing and capability rows, without claiming the
     path loop itself is full GPU.

**Gate:** users can request a broad execution intent and inspect what graph was
compiled and what actually ran.

### E11 - Textbook, Examples, and Rendered Comparisons

**Epic dependencies:** E9, E10.

**Purpose:** make the CPU/hybrid/GPU tracing work understandable and visible.

**Jobs:**

1. ~~**Add textbook section for algorithm/schedule/backend.**~~ ✅ **Done.**
   The wavefront/path-tracing textbook chapter now separates transport
   algorithm, frame schedule, and execution backend, and explains why
   CPU/hybrid/GPU image parity proves backend-record compatibility rather than
   full GPU shading ownership.
   - Depends on: none.
   - Output: ~~explanation of why matching images can still prove backend work.~~

2. ~~**Add rendered examples for supported subset.**~~ ✅ **Done.** Issue
   #624 adds scripted CPU, auto-policy, and GPU-requested wavefront
   path-tracing comparison renders plus metrics sidecars for one supported
   matte scene.
   - Depends on: job 1.
   - Output: CPU, hybrid, and GPU comparison images for the same scene.

3. ~~**Add Modeler example scene.**~~ ✅ **Done.** Issue #625 adds
   `scenes/tracing_execution_inspection_demo.json`, a Modeler-loadable
   GPU-requested wavefront path-tracing scene with supported compiled
   intersection primitives and graph-trace backend metadata coverage.
   - Depends on: job 2.
   - Output: scene that loads in Modeler and makes execution-mode inspection
     easy.

4. ~~**Add rendercli examples and troubleshooting notes.**~~ ✅ **Done.**
   Issue #626 documents release and platform preset builds, CPU/auto/GPU
   rendercli requests, compact and JSON metrics fields, and common fallback
   reasons in the Tools and Modeler textbook chapter.
   - Depends on: jobs 2 and 3.
   - Output: commands showing CPU/hybrid/GPU requests, fallback reasons, and
     metrics to inspect.

5. ~~**Run textbook/source-map checks.**~~ ✅ **Done.** Issue #627 verifies
   `rake docs:textbook:check` and regenerates
   `docs/markdown/appendix/c-source-map.md`; the generated source-map appendix
   was already current.
   - Depends on: jobs 1 through 4.
   - Output: `rake docs:textbook:check`; source-map regeneration if anchors
     changed.

**Gate:** a reader can learn what changed, render examples, and verify which
execution path was used.

### E12 - Performance Gates and Automatic Selection

**Epic dependencies:** E9, E10.

**Purpose:** only make GPU or hybrid paths automatic after correctness and
performance are proven.

**Jobs:**

1. ~~**Define benchmark scenes.**~~ ✅ **Done.**
   - Depends on: none.
   - Output: documented in
     `docs/perf/tracing-backend-benchmark-scenes-2026-06-15.md`, runnable
     through `benchmarks/tracing_backend_capture.sh`, and mirrored by
     `benchmarks/WavefrontIntersectionBackendBenchmark.cpp` rows for small
     primitive, large mesh, visibility-heavy, indirect diffuse, and unsupported
     fallback workloads.

2. ~~**Capture CPU, hybrid, and GPU metrics.**~~ ✅ **Done.**
   - Depends on: job 1.
   - Output: `benchmarks/WavefrontIntersectionBackendBenchmark.cpp` now emits
     comparable `tracing_*` counters for render time, upload/readback time,
     kernel time, scene compile time, rays/sec, resident bytes, and fallback
     rate across CPU, hybrid/packed, GPU, and fallback rows; the counter
     contract is documented in
     `docs/perf/tracing-backend-benchmark-scenes-2026-06-15.md`.

3. ~~**Define auto-selection policy.**~~ ✅ **Done.**
   - Depends on: job 2.
   - Output: `auto` remains CPU-first. It rejects workloads below the fixed
     expected-ray floor before platform probing or scene compilation, then
     requires a platform GPU device, a render-path-capable platform backend, a
     compiled scene with no unsupported leaves, platform basic-hit eligibility,
     packed closest-hit eligibility, and packed any-hit eligibility before GPU
     can be considered. The final GPU gate compares the effective expected-ray
     count against the larger of the fixed floor and
     `ceil(scene_upload_bytes / 1024) * minimumGpuRaysPerSceneUploadKiB`.
     Selection uses closest-hit plus any-hit expected rays when query-family
     estimates are present, otherwise the legacy total expected ray count.
     Metrics that explain decisions are
     `intersectionBackendExpectedRays`,
     `intersectionBackendExpectedClosestHitRays`,
     `intersectionBackendExpectedAnyHitRays`,
     `intersectionBackendAutoMinimumGpuRays`,
     `intersectionBackendAutoEstimatedQueryTransferBytes`,
     `intersectionSceneUploadBytes`,
     `intersectionSceneUnsupportedReasons`,
     `intersection_backend_gpu_device`,
     `intersection_backend_gpu_render_path`, selected execution path,
     fallback reason, frontier residency/byte counters, and backend
     upload/kernel/readback timing. Residency is diagnostic-only for this
     intersection-service policy; GPU is automatic only after the transfer and
     scene gates above pass.

4. ~~**Add conservative policy tests.**~~ ✅ **Done.**
   `WavefrontIntersectionAutoSelection` functional tests pin small supported
   scenes staying on CPU before compilation, large supported scenes becoming GPU
   candidates or explicit CPU fallbacks, direct policy GPU selection for a
   supported large candidate, and unsupported-scene fallback.
   - Depends on: job 3.
   - Output: ~~functional tests pin decisions for small, supported large, and
     unsupported scenes without relying on absolute timing.~~

5. ~~**Document measured thresholds.**~~ ✅ **Done.**
   - Depends on: jobs 2, 3, and 4.
   - Output: `docs/perf/tracing-backend-benchmark-scenes-2026-06-15.md`
     documents the 65,536-ray fixed floor, the 64 rays-per-scene-upload-KiB
     amortization gate, captured benchmark rows for the current CPU-resolved
     automatic policy, and the explicit caveat that those rows do not support a
     GPU speedup claim.

**Gate:** `auto` has evidence and tests. It does not silently route to GPU just
because a GPU exists.

### E13 - Hybrid Visibility and Ray-Traced Shadows

**Epic dependencies:** E1, E6.

**Purpose:** preserve GPU intersection-only as a useful standalone feature for
real-time-ish shadows and graph visibility work.

**Jobs:**

1. ~~**Define graph visibility pass contract.**~~ ✅ **Done.** The Milestone 12
   contract now defines hybrid visibility pass inputs, any-hit/closest-hit
   typed outputs, resource residency/readback rules, and explicit CPU/raster/
   disabled-output fallback behavior for issue #633.
   - Depends on: none.
   - Output: pass inputs, outputs, resource types, and fallback behavior.

2. ~~**Implement a visibility/AOV graph pass.**~~ ✅ **Done.** The
   `hybrid_visibility` AOV pass submits primary closest-hit debug rays through
   `render::IntersectionService`, writes a graph-visible preview resource, and
   records backend execution diagnostics for issue #634.
   - Depends on: job 1.
   - Output: pass that submits any-hit/closest-hit work through E1 and writes a
     usable debug/visibility resource.

3. ~~**Integrate hybrid ray-traced shadow option.**~~ ✅ **Done.** Raster
   shadow mode `ray_traced` now compiles a `hybrid_ray_traced_shadows` graph
   pass, submits primary closest-hit plus shadow any-hit queries through
   `render::IntersectionService`, writes `hybrid_shadow_mask`, and composites
   the mask over raster beauty output.
   - Depends on: job 2.
   - Output: ~~raster or graph beauty pass can opt into GPU visibility shadows
     where scene/backend support exists.~~

4. ~~**Add rendercli and Modeler tests.**~~ ✅ **Done.** GraphRenderEngine tests
   prove the hybrid shadow pass darkens occluded raster receivers and records
   intersection-service metadata; rendercli functional coverage now proves
   `--shadow_mode ray_traced` changes raster output and writes trace metadata
   for closest-hit plus any-hit shadow queries without claiming full GPU
   tracing.
   - Depends on: jobs 2 and 3.
   - Output: ~~rendered/AOV tests and graph trace assertions for GPU
     intersection service usage without claiming full GPU tracing.~~

5. ~~**Document limitations.**~~ ✅ **Done.** The intersection-service
   contract and user-facing rendercli/Modeler docs now call out that
   `hybrid_visibility` and `--shadow_mode ray_traced` use GPU intersection
   service queries when eligible, while scheduling, shading, path state, and
   accumulation remain CPU-owned for issue #637.
   - Depends on: job 4.
   - Output: docs explain this is hybrid visibility, not full GPU path tracing.

**Gate:** GPU intersection remains independently valuable even before or after
full GPU path tracing exists.

### E14 - GPU Whitted v1

**Epic dependencies:** E1, E2, E3, E5, E8, E10.

**Purpose:** add a deterministic GPU Whitted branch after the shared GPU tracing
infrastructure exists.

**Jobs:**

1. ~~**Define supported Whitted subset.**~~ ✅ **Done.** The GPU Whitted v1
   supported materials, lights, recursion policy, full-shading
   transparent/glass exclusion, and shared-service requirements are documented
   in Milestone 11 for issue #638.
   - Depends on: none.
   - Output: supported materials, lights, recursion/iteration policy, and
     explicit transparent/glass decision.

2. ~~**Add deterministic direct lighting and shadows.**~~ ✅ **Done.** The
   Whitted depth-major batch path now evaluates supported non-recursive local
   BSDF direct lighting with backend any-hit shadow batches and CPU parity
   tests for lit and shadowed cases for issue #639.
   - Depends on: job 1.
   - Output: GPU path for supported non-recursive Whitted lighting.

3. ~~**Add mirror reflection continuation.**~~ ✅ **Done.**
   `ReflectiveMaterial::shadeWhitted` emits explicit mirror
   `WhittedContinuation` records, the Whitted depth-major scheduler queues
   them through the same closest-hit frontier path as primary rays, and unit
   coverage compares scalar and batched reflective continuation output while
   keeping built-in reflective packet hits on the packet path.
   - Depends on: job 2.
   - Output: ~~iterative reflection depth behavior matching CPU for supported
     materials.~~

4. ~~**Add CPU/GPU parity scenes.**~~ ✅ **Done.** `rendercli_tracing_parity`
   now compares CPU Whitted renders with GPU-requested Whitted renders for
   deterministic direct-light and mirror-reflection fixture scenes, while
   keeping transparent Whitted fallback coverage for issue #641.
   - Depends on: jobs 2 and 3.
   - Output: deterministic image parity for supported Whitted scenes.

5. ~~**Expose through graph/rendercli/Modeler.**~~ ✅ **Done.**
   Render intent and graph pass state expose broad tracing execution controls,
   rendercli accepts `--tracing_execution`, Modeler Render Settings exposes
   Auto/CPU/Hybrid/GPU tracing execution controls for ray-family renders, and
   graph traces show requested, predicted, actual, and fallback execution
   fields for GPU-requested Whitted renders.
   - Depends on: job 4.
   - Output: ~~user-facing controls and trace fields for GPU Whitted
     execution.~~

**Gate:** GPU Whitted is a separate algorithm branch that reuses shared tracing
backend services instead of creating a one-off renderer.

### E15 - Platform Full-GPU Diffuse Path Loop v1

**Epic dependencies:** E2, E3, E4, E5, E7, E8, E9, E10, E12.

**Purpose:** replace the current compiled CPU-reference diffuse path loop with
the first Metal/Vulkan-owned path-loop backend for the supported diffuse subset.
This is the milestone that should produce a real GPU speedup claim when the
scene is large enough to amortize upload/readback costs.

**Jobs:**

1. **Define the platform path-loop backend contract.**
   - Depends on: none.
   - Output: backend availability, platform label, unsupported reason, required
     buffers, readback policy, and result metadata are explicit on the
     `GpuDiffusePathLoopBackend` boundary. ✅ **Started.** The interface now has
     a separate full-GPU backend selection hook so automatic graph compilation
     cannot confuse the CPU-reference diagnostic backend with a platform-owned
     path-loop backend. `GpuDiffusePathLoopLaunchPlanner` also defines the
     shader-facing launch parameters, serialized compiled-scene upload payload,
     section offsets, geometry subrange offsets/counts, and byte accounting for
     scene upload, ping-pong path-state buffers, per-depth step records,
     retained indices, and accumulation storage. The backend boundary now also
     exposes scene-specific full-GPU support so a platform backend can be
     available but reject a narrower compiled-scene subset without making graph
     analysis overclaim full GPU execution. Metal and Vulkan now share the same
     scene-section support policy for geometry, material, texture, light, and
     max-depth validation, while keeping platform-specific fallback wording at
     the backend boundary.

2. **Add a minimal Metal path-loop kernel.**
   - Depends on: job 1.
   - Output: supported initial path states, compiled scene records, fixed GPU
     sample stream dimensions, diffuse continuation, direct light sampling,
     any-hit visibility, path-state compaction, and accumulation execute inside
     one Metal backend for Matte, Phong finite diffuse/glossy, Reflective
     mirror, Transparent perfect reflection/refraction, Portal, or Emissive scenes
     using ConstantColor, planar/UV CheckerBoard texture graphs,
     nearest/bilinear ImageTexture, base-level bilinear mipmapped ImageTexture
     records, UVColorTexture, or bounded Tinted wrapper chains over those
     texture records. ✅ **Started.**
     `MetalGpuDiffusePathLoopKernel` now compiles and dispatches a Metal
     launch-probe kernel that binds the shader-facing path-loop descriptor plus
     scene, initial/active/next path-state, step-record, retained-index, and
     accumulation buffers, uploads the serialized compiled-scene payload with
     descriptor-visible section offsets, and copies initial path-state records
     into GPU active/next path-state buffers while writing probe
     `GpuDiffusePathStepRecord` rows. A restricted all-miss Metal probe now
     also reads the environment section from the serialized scene upload and
     resolves empty-scene background/environment misses into terminated
     GPU-owned path-state records while clearing and writing the matching
     accumulation color/count planes for unique active pixel targets. A
     restricted closest-hit probe now traverses packed BVH/primitive/sphere
     records from the same resident scene upload for untransformed sphere
     geometry and writes `GpuIntersectionHitRecord` rows for active path
     states. A restricted Matte/ConstantColor shading probe now reads material
     and texture records from that upload and computes the diffuse continuation
     throughput for sphere hits. A restricted continuation probe now samples a
     GPU diffuse direction, applies Russian roulette, and writes the next
     path-state record for that same sphere/Matte/ConstantColor subset. That
     continuation probe also evaluates restricted
     point/directional/rectangular-area direct-light contribution with GPU
     light-selection and light-surface dimensions plus sphere any-hit shadow
     rejection, and it can terminate restricted Emissive/ConstantColor sphere
     hits while preserving emitted radiance in the GPU path-state contract.
     The full Metal loop now also samples planar/UV CheckerBoard texture graphs
     whose children can be supported texture records, so common wrapped-color
     matte/emissive scenes can stay in the GPU-owned path loop. OpenCylinder
     and Torus payload traversal are now in the same full Metal loop.
     Terminal outcomes from that continuation probe now clear and write the
     accumulation color/count planes for unique active pixel targets, including
     miss termination, emissive hits, unsupported hits, and diffuse paths that
     fail to spawn a continuation. The Metal probes now also append active
     surviving path indices into a count-prefixed retained-index buffer on the
     GPU, giving the future multi-depth loop a shader-generated frontier
     handoff instead of requiring the host to scan next path-state activity.
     This proves the command-buffer, scene-upload, geometry, material-record,
     texture-record, light-record, environment-record, path-state, step-record,
     continuation, direct-light-contribution, emissive-hit-termination, and
     terminal accumulation ABI plus the retained-frontier ABI for the future
     path-loop kernel. Compiled diffuse path-loop continuations now also carry
     weighted rectangular-area-light PDFs in the CPU-reference, Metal, and
     Vulkan paths instead of dropping that emitter-MIS input, and
     BSDF-sampled emissive hits consume those PDFs with the scalar path
     tracer's power-heuristic MIS weight. A restricted
     `MetalGpuDiffusePathLoopBackend` now wraps
     the empty-scene and optionally transformed
     triangle/finite-width Curve tessellated-triangle/sphere/plane/rectangle/
     disk/open-cylinder/torus Matte, Phong finite diffuse/glossy,
     Reflective-mirror, Transparent refraction, Portal, and Emissive paths with
     empty, point-light, directional-light, or rectangular-area-light scenes
     behind the platform
     backend interface, including scene/settings support rejection and
     full-GPU result metadata for backend tests. Its first real path-loop
     dispatch can advance supported paths across multiple depths inside one
     Metal command buffer for that narrow subset, and matching platform
     accumulation planes now flow through the shared diffuse path-loop resolver
     and rendercli metrics instead of requiring host-side image reconstruction
     from terminal path states. Multi-sample explicit GPU renders can now stay
     on the Metal path-loop kernel by switching duplicate output-pixel samples
     to sample-slot platform accumulation and resolving the averaged final
     image from the Metal accumulation planes. The launch contract now also
     carries an explicit diagnostic-capture flag so render graph execution can
     request image-only platform readback when graph trace capture is disabled,
     while backend tests and trace-enabled runs keep step/path-state
     diagnostics available. The full path-loop kernels also skip those
     diagnostic buffer writes and zero the logical diagnostic buffer residency
     on the trace-disabled final-image path. The platform full-GPU kernels now
     keep tiny device-written retained-frontier and per-depth active-path
     counters resident even when trace diagnostics are disabled, so future
     device-side scheduling, compact execution metrics, and
     `activePathsPerDepth` do not depend on diagnostic step-record readback.
     Trace-disabled Metal display-only path-loop renders with GPU-resolvable
     tonemapping (Linear, Reinhard, or ACES) can also resolve packed display
     pixels on the GPU and skip HDR accumulation-plane readback; HDR, denoiser,
     graph trace, and unsupported tonemap paths still keep the accumulation
     planes available for CPU consumers. When the live render graph receives
     such a platform-resolved display buffer, compatible tonemap passes can
     propagate that display resource as current without immediately
     CPU-repacking the HDR graph resource. Simple trace-disabled LDR graph
     renders now go further: when the full-GPU path-tracer beauty pass feeds
     only that compatible final tonemap, the graph executes the beauty pass as
     display-only, skips the CPU tonemap pass, marks the final output edge
     produced, and does not ask the backend to read back HDR accumulation
     planes. Any denoiser, graph trace, unsupported tonemap, or postprocess/HDR
     consumer still forces accumulation materialization, but downstream
     postprocess consumers no longer request an unused platform display resolve
     for the intermediate beauty preview; the
     final render dialog keeps graph trace capture off by default and exposes a
     diagnostic checkbox for users who need trace images/metadata. The
     compiled path loop also
     carries `ReflectiveMaterial`, `TransparentMaterial`, and `PortalMaterial`
     delta continuations through the CPU reference evaluator and the Metal
     full-GPU subset, and the
     compiled environment records now carry scene ambient separately from
     visible background and bounced environment radiance so supported surface
     hits match the scalar path tracer's base ambient term. The Metal path-loop
     dispatch now uses pipeline-derived threadgroup widths instead of one
     thread per threadgroup, moving the explicit full-GPU path tracer closer to
     the E15 performance gate without changing its supported-scene contract.
     The real Metal wavefront path-loop launch now also reuses its shared
     buffers across renders, so steady launches no longer allocate the full
     parameter, scene, path-state, frontier, accumulation, diagnostic, and
     resolve buffer set for every image. Metal and Vulkan now also keep an
     exact host-side copy of the last serialized scene upload and skip rewriting
     the retained platform scene buffer when those bytes are unchanged across
     repeated renders.
     Graph background-color overrides now update the compiled visible-background
     record before backend dispatch instead of forcing scalar fallback.
     Raster-only material normal maps no longer make those materials
     unsupported by the compiled path-loop subset; they remain ignored by path
     tracing, matching the scalar CPU integrator.
     Box denoising now runs after compiled path-loop image resolve instead of
     rejecting the path-loop route. The compiled CPU-reference path loop and the
     Metal/Vulkan platform full-GPU path loops now emit first-hit albedo,
     normal, and depth feature records when requested, so bilateral denoising
     can stay on the compiled route for supported scenes.
     Broader
     primitive traversal, full material shading, full direct-light coverage,
     device-side compacted wavefront scheduling, Vulkan parity, and performance
     gates still need to land before it can be called a general full GPU
     tracing path.

3. **Add a minimal Vulkan path-loop kernel.**
   - Depends on: job 1.
   - Output: Linux Vulkan implements the same supported subset and result
     contract as the Metal backend, with skip behavior when Vulkan is not built
     or no device is available. ✅ **Started.** Vulkan-enabled builds now
     compile an embedded diffuse path-loop compute shader and expose a
     restricted `VulkanGpuDiffusePathLoopBackend` that can execute empty
     all-miss paths and a multi-depth shaded static-transform
     triangle/finite-width Curve tessellated-triangle/sphere/plane/rectangle/
     disk/open-cylinder/torus subset with
     Matte/Phong-finite-glossy/Reflective-mirror/Transparent-refraction/Portal/Emissive
     materials,
     ConstantColor/planar-or-UV CheckerBoard texture graphs/
     nearest-or-bilinear ImageTexture records, UVColorTexture, plus bounded
     Tinted wrapper chains over those records, and zero or more point,
     directional, or rectangular area lights. It writes a Vulkan-owned resident
     path-state buffer, current/next retained-frontier index buffers,
     step-record, and accumulation buffers, reports platform path-state
     residency, and now advances the path loop through depth-frontier dispatches
     instead of one all-bounces shader invocation. It also uses platform sample-slot/path
     accumulation when duplicate active pixel targets would otherwise collide
     and evaluates direct-light sampling plus supported geometry visibility in
     the shader. It also carries Reflective mirror and Transparent refraction
     materials as exact delta continuations and can intersect transformed or
     untransformed Triangle, Plane, Rectangle, Disk, OpenCylinder, and Torus
     records, plus finite-width Curve tessellation lowered through triangle
     records, in addition to spheres. Portal materials now carry transformed
     delta continuations through the same shader-side path loop. Trace-disabled
     Vulkan path-loop renders
     with GPU-resolvable tonemapping (Linear, Reinhard, or ACES) can now resolve
     packed display pixels in a second compute dispatch after the path-loop
     dispatch. Display-only callers can
     skip HDR accumulation-plane readback, while graph renders that still need
     HDR resources can request the packed display buffer and accumulation
     readback together. The Vulkan runtime also keeps its device, queue,
     descriptor layout, descriptor pool, descriptor sets, pipeline layout,
     command pool, command buffer, and path-loop compute pipelines alive across
     launches instead of rebuilding them for every render. It
     still cleanly rejects broader compiled geometry until the platform subset
     reaches parity with Metal.

4. **Wire graph auto-selection to platform backend availability.**
   - Depends on: jobs 2 and 3.
   - Output: `auto` can select full GPU only when scene support, platform
     backend availability, and render-path capability all pass; explicit GPU
     still reports precise fallback reasons. ✅ **Started.**
     Explicit GPU graph requests now ask the platform full-GPU backend hook for
     a scene/settings-supported backend and prefer it over the diagnostic
     CPU-reference backend when available. Caller-injected test backends still
     win. Eligible automatic path-tracer beauty passes now also predict full
     GPU execution when the scene analysis proves the compiled full-GPU subset
     and a platform backend are both available; incompatible settings such as
     denoising, adaptive sampling, convergence, and non-GPU sample streams
     remain CPU/hybrid until those paths have platform support.
     Execution metadata now preserves the full-GPU selection fallback reason,
     so graph traces distinguish missing platform builds, unavailable
     Metal/Vulkan devices or kernels, unsupported scene/settings subsets, and
     explicit caller-selected CPU/hybrid backends. The Modeler render graph
     inspector also shows auto-selected tracing execution predictions before a
     render runs, with `Auto` request, predicted CPU/hybrid/GPU execution, and
     `none` when there is no fallback reason. The default macOS release build
     now enables the Metal wavefront/full-GPU path-loop backend so the normal
     Modeler/rendercli build can exercise platform GPU execution without using
     a separate preset. Graph-backed `pathtracer` executor shortcuts now fill
     in GPU tracing execution and the GPU sample stream when the scene/caller
     did not explicitly choose CPU/hybrid tracing or a sampler-backed path, so
     the default user-facing path-tracer route enters the compiled GPU path-loop
     eligibility path.

5. **Add parity and performance gates.**
   - Depends on: jobs 2, 3, and 4.
   - Output: CPU/path-loop image parity for fixed seeds, compact trace metrics
     proving `full_gpu_subset`, and benchmark rows that compare CPU-reference,
     hybrid-compaction, Metal, and Vulkan path-loop execution. ✅ **Started.**
     Platform-enabled benchmark builds now include an explicit requested-GPU
     compiled diffuse path-loop row with diagnostics disabled, using the same
     workload matrix as the CPU-reference compiled path-loop row. The rendercli
     capture script now also sets `--tracing_execution` alongside
     `--wavefront_intersection_backend` so its CPU/auto/GPU modes measure the
     broader tracing execution path rather than only intersection-backend
     selection. Full-GPU path-loop benchmark rows now also publish the platform
     backend's reported upload, kernel, readback, total reported, and host
     overhead timings so performance captures can distinguish shader work from
     setup/readback costs. Benchmark counters, render graph metadata, and the
     Modeler graph inspector now also show whether the platform scene-upload
     buffer was reused and how many serialized scene bytes were written for a
     launch, so repeated-render captures can separate scene-upload churn from
     path-loop shader cost. Platform-enabled benchmark builds now also include
     a warmed requested-GPU compiled diffuse path-loop row that pre-runs one
     platform launch before timing, making steady-state scene-upload cache reuse
     visible beside the cold requested-GPU row. The tracing backend capture
     script can now opt into those compiled diffuse path-loop benchmark rows
     beside its rendercli metrics, so one capture directory can hold the
     CPU-reference, cold requested-GPU, and warmed requested-GPU comparison data
     needed for the E15 speedup gate.

6. **Update user-facing docs and examples.**
   - Depends on: job 5.
   - Output: rendercli/modeler instructions and textbook examples distinguish
     CPU, hybrid intersection/compaction, and full platform GPU path-loop
     execution with performance caveats.

**Gate:** a supported diffuse path-tracing scene can render end-to-end through
Metal or Vulkan with path state, shading, direct lighting, compaction, and
accumulation owned by the platform backend, and the result is faster than the
CPU-reference path loop on at least one documented benchmark workload.

---

## Filing Guidance

File all epics up front if desired. Epics with unmet dependencies should remain
blocked; that is useful because it keeps the graph visible. Do not start with
E9 or E10 just because they are exciting: they depend on scene data, sampling,
accumulation, parity, and direct-light/path-step services being real.

If a Syrus agent finds that an epic dependency is too broad, split the upstream
epic rather than adding many cross-epic job dependencies. Cross-epic job
dependencies are allowed but should be rare.

The first practical implementation wave is:

1. E1 Intersection Service Consolidation.
2. E2 Cross-Backend Parity Harness.
3. E3 GPU Tracing Scene Data.
4. E4 GPU Sample Stream.
5. E5 GPU Accumulation Buffer.
6. E6 Execution Diagnostics and Capability Reporting.

Once those are complete, E7 and E8 are the first places where the GPU does
non-trivial tracing work beyond intersection.
