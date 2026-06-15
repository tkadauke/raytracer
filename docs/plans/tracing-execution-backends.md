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
  triangle/mesh triangles, sphere, plane, rectangle, disk, OpenCylinder,
  Torus, and static transforms.
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
  supported shading subset: Matte and Emissive materials, ConstantColor
  textures, PointLight, DirectionalLight, and RectangularAreaLight.
- `render::GpuSampleStream` provides the CPU reference for deterministic
  GPU-style sampling dimensions with fixed-vector coverage.
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

- GPU-owned path state.
- GPU-side path/frontier compaction for scheduler-owned path records.
- GPU material records beyond the initial Matte and Emissive subset.
- GPU texture records beyond ConstantColor.
- GPU light records beyond PointLight, DirectionalLight, and
  RectangularAreaLight, and GPU-side light sampling/contribution kernels.
- GPU BSDF evaluation.
- GPU direct-light contribution evaluation.
- GPU path continuation generation and Russian roulette.
- Integrated GPU accumulation/progressive sample buffers in the render loop.
- Full GPU path-tracing loop.
- Full GPU Whitted loop.
- Hardware ray tracing backends.
- A render graph compiler model that can synthesize CPU, hybrid, and full GPU
  tracing plans from render intent, scene support, and user overrides.

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

Unsupported CSG/boolean composites, moving transforms, unsupported exact
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
lighting.direct_light_contribution
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
   path semantics.

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
- Explicit fallback test for transparent/glass scene.
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
  - transparent/glass explicitly unsupported for first GPU path-tracing subset
    unless the continuation contract is ready.
- Start with a restricted texture subset:
  - constant color;
  - checkerboard if UV/local-coordinate payloads are ready;
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
- No transparent/glass.
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

Transparent/glass is intentionally out of GPU Whitted v1. Unsupported glass
must produce an explicit capability/fallback reason and run through the CPU
Whitted path instead of approximating refraction as opacity, alpha blending, or
mirror reflection. Glass can be reconsidered only after hit metadata,
inside/outside medium state, nested IOR handling, and transparent any-hit
semantics are represented in the shared tracing records.

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
- Fallback tests for TransparentMaterial until supported.
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
   triangle, mesh triangle, sphere, plane, rectangle, disk, OpenCylinder, Torus,
   and static transforms, and keeps mesh triangles represented in optional
   Metal/Vulkan triangle smoke parity scenes.
   - Depends on: job 1.
   - Output: CPU runtime, packed CPU, Metal, and Vulkan closest-hit/any-hit
     parity tests for triangle, mesh triangle, sphere, plane, rectangle, disk,
     OpenCylinder, Torus, and static transforms.

3. ~~**Stabilize explicit fallback behavior.**~~ ✅ **Done.** Issue #571 pins
   deterministic GPU-intersection unsupported-scene fallback reasons for
   transparent/glass and generic unsupported scenes in backend and metrics tests.
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
   `render::compileGpuTracingMaterials` now packs Matte and Emissive materials
   into GPU tracing records keyed by runtime material id, with unsupported
   material reasons counted for tracing scene diagnostics. Closes #580.
   - Depends on: job 1.
   - Output: records for Matte and Emissive; explicit unsupported reasons for
     all other materials.

3. ~~**Compile texture records.**~~ ✅ **Done.**
   `render::GpuTracingTextureCompilation` now packs ConstantColor textures into
   GPU tracing records keyed by material-referenced texture ids, with
   unsupported texture reasons counted for tracing scene diagnostics. Closes
   #581.
   - Depends on: job 1.
   - Output: records for ConstantColor; optional CheckerBoard only if required
     coordinates are already represented; unsupported reasons otherwise.

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
   preserving existing intersection backend fields.
   - Depends on: job 2.
   - Output: JSON/rendercli output with compatibility aliases for existing
     consumers.

4. **~~Group Modeler graph metadata.~~** ✅ **Done.** Modeler selected-pass
   rows now summarize tracing execution as CPU, hybrid, GPU, fallback, and
   unsupported capability groups before the legacy intersection detail aliases.
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

1. **Define direct-light work records.**
   - Depends on: none.
   - Output: hit point/reconstructible hit data, normal, material id, incoming
     direction, throughput, sample state, and light-selection inputs.

2. **Add CPU reference direct-light batch.**
   - Depends on: job 1.
   - Output: record-based CPU implementation matching current path-tracer
     direct-light semantics.

3. **Implement supported light sampling.**
   - Depends on: jobs 1 and 2.
   - Output: point, directional, and rectangular area light sampling for the
     compiled light records.

4. **Implement diffuse contribution evaluation.**
   - Depends on: jobs 2 and 3.
   - Output: Matte diffuse direct-light contribution with MIS behavior matching
     CPU reference.

5. **Connect visibility through the intersection service.**
   - Depends on: jobs 3 and 4.
   - Output: any-hit visibility queries use E1's service and produce occluded
     flags for contribution evaluation.

6. **Add parity tests and metrics.**
   - Depends on: job 5.
   - Output: analytic diffuse tests, MIS tests, image/record parity, and trace
     fields distinguishing visibility-only GPU from contribution GPU.

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

4. **Add emission and direct-light contribution.**
   - Depends on: jobs 2 and 3.
   - Output: supported emission/direct-light contribution feeds E5
     accumulation or the CPU reference equivalent.

5. **Sample diffuse continuation.**
   - Depends on: jobs 2, 3, and 4.
   - Output: next path-state records using E4 sample dimensions.

6. **Add one-bounce parity tests.**
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
     issue #613.

3. **~~Loop over depth with max-depth and Russian roulette.~~** ✅ **Done.**
   Added `loopResidentDiffusePaths` as the CPU-reference resident path-state
   loop with active/next ping-pong, max-depth draining, GPU sample-stream
   Russian roulette, and fixed-seed retained-record parity diagnostics for
   issue #614.
   - Depends on: jobs 1 and 2.
   - Output: ~~supported diffuse paths execute multiple bounces and terminate
     according to the same policy as CPU.~~

4. **Resolve accumulated image.**
   - Depends on: jobs 1 and 3.
   - Output: E5 accumulation resolves to rendercli/Modeler-visible image data.

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

**Gate:** users can request a broad execution intent and inspect what graph was
compiled and what actually ran.

### E11 - Textbook, Examples, and Rendered Comparisons

**Epic dependencies:** E9, E10.

**Purpose:** make the CPU/hybrid/GPU tracing work understandable and visible.

**Jobs:**

1. **Add textbook section for algorithm/schedule/backend.**
   - Depends on: none.
   - Output: explanation of why matching images can still prove backend work.

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

4. **Add conservative policy tests.**
   - Depends on: job 3.
   - Output: functional tests pin decisions for small, supported large, and
     unsupported scenes without relying on absolute timing.

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

3. **Integrate hybrid ray-traced shadow option.**
   - Depends on: job 2.
   - Output: raster or graph beauty pass can opt into GPU visibility shadows
     where scene/backend support exists.

4. **Add rendercli and Modeler tests.**
   - Depends on: jobs 2 and 3.
   - Output: rendered/AOV tests and graph trace assertions for GPU intersection
     service usage without claiming full GPU tracing.

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
   supported materials, lights, recursion policy, transparent/glass exclusion,
   and shared-service requirements are documented in Milestone 11 for issue
   #638.
   - Depends on: none.
   - Output: supported materials, lights, recursion/iteration policy, and
     explicit transparent/glass decision.

2. ~~**Add deterministic direct lighting and shadows.**~~ ✅ **Done.** The
   Whitted depth-major batch path now evaluates supported non-recursive local
   BSDF direct lighting with backend any-hit shadow batches and CPU parity
   tests for lit and shadowed cases for issue #639.
   - Depends on: job 1.
   - Output: GPU path for supported non-recursive Whitted lighting.

3. **Add mirror reflection continuation.**
   - Depends on: job 2.
   - Output: iterative reflection depth behavior matching CPU for supported
     materials.

4. ~~**Add CPU/GPU parity scenes.**~~ ✅ **Done.** `rendercli_tracing_parity`
   now compares CPU Whitted renders with GPU-requested Whitted renders for
   deterministic direct-light and mirror-reflection fixture scenes, while
   keeping transparent Whitted fallback coverage for issue #641.
   - Depends on: jobs 2 and 3.
   - Output: deterministic image parity for supported Whitted scenes.

5. **Expose through graph/rendercli/Modeler.**
   - Depends on: job 4.
   - Output: user-facing controls and trace fields for GPU Whitted execution.

**Gate:** GPU Whitted is a separate algorithm branch that reuses shared tracing
backend services instead of creating a one-off renderer.

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
