# GPU-assisted wavefront intersection plan - June 2026

> **Scope:** add an optional GPU intersection backend for the wavefront/path
> tracing engines. The first version is hybrid: wavefront scheduling, material
> evaluation, BSDF sampling, direct lighting, denoising, tonemapping, and graph
> execution stay on the CPU; the GPU backend answers "which primitive does this
> ray hit?" for a batch/frontier of active rays.
>
> **Status:** implementation in progress. Phase 1 has the CPU backend seam,
> render intent/graph state selection, rendercli and Modeler controls, and
> fallback metrics in place. Phase 2 now has a diagnostic CPU-side compiled
> intersection scene for supported leaves, ids, transforms, bounds, and
> unsupported reasons. Scene-created GPU fallback stubs retain supported
> compiled scenes and packed upload buffers; exact closest-hit, packet
> closest-hit, and bounded any-hit queries for triangle, sphere, plane,
> rectangle, disk, exact OpenCylinder, and static-transform payloads can run
> through the packed CPU kernel contract and the platform basic-kernel contract.
> Metal-only smoke kernels now prove optional compute dispatch
> outside the render path, and the first render-path Metal basic closest-hit
> and any-hit kernels can execute for prepared triangle, sphere, plane,
> rectangle, disk, and OpenCylinder scenes, including static transform payloads, when a Metal
> device is available. Vulkan-enabled builds can now run basic closest-hit and
> any-hit kernels for prepared triangle, sphere, plane, rectangle, disk, and OpenCylinder
> scenes, including static transform payloads. This is a
> follow-up to
> `docs/plans/wavefront-and-path-tracing.md` Phase 7+. It should not replace
> the CPU wavefront renderer, and it should not attempt a full GPU path tracer
> in the first slice.

---

## Goals

- Keep one renderer semantics path. CPU and GPU intersection backends should
  feed the same wavefront scheduler and the same CPU shading/integrator code.
- Support macOS and Linux. macOS should use Metal; Linux should use Vulkan
  compute. CPU remains the fallback everywhere.
- Make the backend boundary explicit: a backend owns ray/scene intersection, not
  material interpretation or path transport.
- Start with a restricted primitive subset and clear fallback reasons rather
  than a partial, silently wrong mixed renderer.
- Add measurements and parity tests before making any GPU backend automatic.
- Keep the render graph visible: backend choice and fallback reasons should show
  up in trace metadata and rendercli/Modeler diagnostics.

## Non-goals

- Do not make OpenGL the compute backend. macOS OpenGL has no compute shaders
  and is deprecated; it remains a raster preview/backend technology only.
- Do not adopt CUDA as the project GPU path. It is useful for NVIDIA Linux, but
  it does not satisfy macOS support.
- Do not build on OpenCL. It is deprecated on macOS and not a good long-term
  foundation.
- Do not use hardware ray tracing APIs first. Vulkan RT and Metal ray tracing
  have different acceleration-structure models; first prove flat-BVH compute
  traversal so CPU/GPU results are directly comparable.
- Do not merge CPU and GPU closest-hit results from two separate acceleration
  structures in v1. If a GPU backend cannot compile the scene, fall back to the
  CPU backend for the whole render.
- Do not move materials, BSDF sampling, light sampling, or denoising to the GPU
  in this plan.

---

## Backend responsibility

The backend contract is **ray-scene intersection**:

```cpp
class WavefrontIntersectionBackend {
public:
  virtual std::string name() const = 0;
  virtual BackendAvailability availability() const = 0;
  virtual BackendCapabilities capabilities() const = 0;

  virtual IntersectionSceneHandle compileScene(const render::Scene& scene) = 0;

  virtual void intersectClosest(const IntersectionSceneHandle& scene,
                                Span<const RayWorkItem> rays,
                                Span<HitRecord> hits) = 0;

  virtual void intersectAny(const IntersectionSceneHandle& scene,
                            Span<const RayWorkItem> rays,
                            Span<OcclusionRecord> occlusion) = 0;
};
```

The backend receives:

- a compiled intersection scene;
- a frontier of rays: origin, direction, min/max distance, time sample, and ray
  flags;
- a query mode: closest hit for camera/path rays, any hit for shadow/occlusion
  rays.

The backend returns:

- miss or closest-hit state;
- primitive/object id;
- material id or material handle lookup key;
- hit distance `t`;
- geometric hit data needed by CPU shading: normal, point or reconstructible
  point, UV/barycentric/local coordinates when available;
- backend diagnostics: rays submitted, hits, misses, kernel time, upload time,
  fallback reason.

The backend does **not** own:

- material evaluation;
- BSDF sampling;
- next-event estimation;
- path continuation or Russian roulette;
- tonemapping;
- denoising;
- graph pass scheduling;
- final pixel/sample accumulation.

The BVH is an implementation detail of a backend. Backends may use the current
CPU scene/BVH, a CPU packet BVH, a GPU flattened BVH, or future hardware
acceleration structures, but wavefront should only ask the backend to intersect
the current frontier.

---

## Cross-platform GPU API policy

The project should expose one engine-level backend interface and multiple
platform backends:

- `CpuWavefrontIntersectionBackend`: wraps the current CPU path and remains the
  default/canonical implementation.
- `MetalWavefrontIntersectionBackend`: macOS compute backend.
- `VulkanWavefrontIntersectionBackend`: Linux compute backend.

The shader/kernel source can initially be duplicated in small, backend-native
kernels:

- Metal Shading Language for macOS;
- GLSL/SPIR-V or Vulkan GLSL compiled through the existing CMake toolchain for
  Linux.

Shader portability layers are a later decision, not a prerequisite. Slang,
wgpu/Dawn, or a shared shader IR may become attractive once the kernel set is
large, but the first slice should be small enough that native kernels are easier
to debug.

MoltenVK is a possible future unification path, but it should not be assumed for
v1. The macOS path should be able to use Metal directly so it matches the native
platform GPU stack and avoids relying on Vulkan-over-Metal behavior for core
renderer correctness.

---

## Intersection scene representation

GPU backends should not consume arbitrary C++ `Primitive` subclasses directly.
They need a compiled, stable representation:

```cpp
struct IntersectionPrimitiveRecord {
  PrimitiveKind kind;
  MaterialId material;
  ObjectId object;
  TransformId transform;
  Bounds bounds;
  std::uint32_t payloadOffset;
  std::uint32_t payloadCount;
};

struct FlatBvhNode {
  Bounds bounds;
  std::uint32_t leftOrFirstPrimitive;
  std::uint32_t primitiveCount;
  std::uint32_t flags;
};

struct GpuIntersectionScene {
  std::vector<FlatBvhNode> bvh;
  std::vector<IntersectionPrimitiveRecord> primitives;
  std::vector<TrianglePayload> triangles;
  std::vector<SpherePayload> spheres;
  std::vector<TransformPayload> transforms;
};
```

The compiler maps the runtime `render::Scene` into this representation and
declares whether the target backend can support it. Unsupported primitives,
unsupported transforms, or unsupported motion semantics should produce a
specific fallback reason.

Initial GPU v1 should be all-or-nothing per render:

- if every leaf compiles to the supported intersection scene, use GPU
  intersection;
- otherwise use CPU intersection and record the first unsupported reason plus
  counts by reason.

Mixed CPU/GPU intersection can come later, after we have a correct global
closest-hit merge strategy and tests for overlapping supported/unsupported
geometry.

---

## Primitive support policy

The CPU backend supports everything the existing raytracer supports.

GPU v1 should support:

1. **Triangle and mesh leaves.** This covers glTF, STL, 3MF, OpenSCAD, LDraw,
   and most imported model workflows after tessellation/import.
2. **Common exact primitives.** Sphere, plane, triangle, rectangle, disk, and OpenCylinder are
   useful early because many educational scenes use them and they avoid
   raster-style tessellation drift.
3. **Static instances.** Static object transforms are important for imported
   assets. Moving instances and shutter-time transforms can wait until the
   ray/time contract is explicit in GPU payloads.

GPU v1 should reject:

- CSG/boolean composites;
- torus, curve, convex operation, and other exact primitives not yet ported;
- moving instances;
- scenes with primitive/material references that cannot be represented by
  stable ids.

Later phases can expand this list incrementally, each with CPU/GPU parity tests.

---

## Phase 0 - API decision spike and baseline capture

Tasks:

- Add this plan and link it from `docs/plans/wavefront-and-path-tracing.md`
  Phase 7+.
- Record baseline CPU wavefront metrics for:
  - BVH-heavy primary rays;
  - imported mesh scene;
  - path-tracing indirect-bounce scene;
  - transparent/glass scene that is expected to stay CPU-only initially.
- Prototype one tiny compute kernel per platform outside the render path:
  - Metal: upload a ray buffer and write back deterministic hit/miss dummy data.
  - Vulkan: same shape on Linux.
- Decide build flags and optional dependencies:
  - `RAYTRACER_ENABLE_METAL_WAVEFRONT`;
  - `RAYTRACER_ENABLE_VULKAN_WAVEFRONT`;
  - both off when toolchains are unavailable.

Gate:

- CPU behavior unchanged.
- GPU backend unavailability is reported cleanly.
- The chosen API/toolchain path can be configured on macOS and Linux without
  breaking the default build.

Progress:

- `RAYTRACER_ENABLE_METAL_WAVEFRONT` and
  `RAYTRACER_ENABLE_VULKAN_WAVEFRONT` are now explicit CMake options. They
  default off, enforce the intended host platform, and publish compile
  definitions to the library and dependents.
- Platform-conditional CMake presets now make those options one-command
  build/test/benchmark paths: `release-metal-wavefront` and
  `benchmark-metal-wavefront` on macOS, plus `release-vulkan-wavefront` and
  `benchmark-vulkan-wavefront` on Linux. The default presets remain CPU-only.
- `RAYTRACER_ENABLE_METAL_WAVEFRONT` now also builds an Objective-C++/Metal
  smoke wrapper. It uploads a uint buffer for a deterministic dispatch/readback
  check, and it can run render-path basic closest-hit/any-hit kernels for
  triangle, sphere, plane, rectangle, disk, and OpenCylinder scenes, including
  static transform payloads, against the packed BVH/primitive/payload/ray ABI. The
  smoke path remains a
  platform-plumbing proof; the basic hit kernels are now selected only for
  eligible prepared scenes.
- Platform diagnostics now distinguish disabled plumbing, enabled-without-device,
  prepared-scene ineligibility, and active platform execution.
- Vulkan-enabled builds now also probe the loader for a physical device with a
  compute queue and can run a tiny deterministic compute dispatch/readback
  smoke kernel. The render backend still reports CPU fallback until a Vulkan
  closest-hit kernel exists, but the fallback reason and `auto` selection path
  can distinguish missing Vulkan compute support from missing render-path
  kernel work.
- Vulkan-enabled builds now compile the smoke compute shader from GLSL into
  generated SPIR-V at build time, replacing the hand-written C++ word array and
  creating the native shader pipeline needed for Vulkan render-path kernels.
- Vulkan-enabled builds now compile and expose direct basic closest-hit and
  any-hit compute dispatches against the packed BVH/primitive/exact-payload/ray
  ABI. Prepared triangle, sphere, plane, rectangle, disk, and OpenCylinder scenes
  can now execute wavefront closest-hit and any-hit batches through Vulkan,
  including static transform payloads.
- Platform GPU device availability is now structured backend trace data instead
  of only fallback text. Wavefront metrics JSON, rendercli summaries, and the
  Modeler graph tooltip expose the selected platform backend id and whether
  that platform backend saw a GPU device during the render.
- Platform GPU render-path availability is now separate structured trace data.
  Auto selection requires both a detected platform GPU device and a backend that
  can run render-path hit kernels, so Vulkan compute-smoke availability no
  longer makes `auto` treat the Vulkan backend as render-capable before Vulkan
  closest-hit and any-hit kernels exist.
- Metal render-path availability now verifies the basic closest-hit and any-hit
  compute pipelines can be constructed, instead of treating device availability
  alone as proof that the Metal render-path kernels are usable. Prepared Metal
  backend execution and direct prepared-scene construction are gated on the
  same render-path probe, so explicit GPU requests do not report `metal`
  execution when dispatch would fall back to the packed CPU contract.

## Phase 1 - backend interface and CPU refactor

Tasks:

- Add `WavefrontIntersectionBackend` and related data types.
- Implement `CpuWavefrontIntersectionBackend` by wrapping the current
  `intersectActiveFrontier(...)` behavior.
- Route Whitted and path-tracing wavefront batches through the CPU backend
  without changing output.
- Add metrics:
  - backend name;
  - backend availability;
  - rays submitted;
  - closest-hit queries;
  - any-hit queries;
  - intersection time;
  - fallback reason.
- Expose backend selection through render intent / render graph state:
  `auto`, `cpu`, `gpu`.

Gate:

- Existing wavefront parity tests pass unchanged with the CPU backend.
- rendercli graph JSON and trace metadata show the selected intersection
  backend.

## Phase 2 - compiled intersection scene

Tasks:

- Add a CPU-side `IntersectionSceneCompiler` that emits flat primitive records,
  payload arrays, material/object lookup tables, and a flat-array BVH.
- Start with triangle/mesh, sphere, plane, rectangle, disk, and static instance
  records; extend the host/packed CPU contract to exact OpenCylinder before
  enabling platform kernels for it.
- Add explicit unsupported-reason collection.
- Add unit tests for:
  - supported primitive compilation;
  - unsupported primitive fallback reasons;
  - material/object id round-tripping;
  - static instance transform payloads;
  - BVH bounds and bounded leaf ranges matching runtime bounds.

Gate:

- CPU backend can optionally consume the compiled scene for diagnostics, but
  default CPU rendering remains behaviorally identical.
- Unsupported scenes fall back to CPU before render work starts.

Progress:

- `IntersectionSceneCompiler` now emits records and payload arrays for
  triangle/mesh-triangle, sphere, plane, rectangle, disk, and OpenCylinder leaves.
- Static instances are captured as transform payloads through the existing
  transformed-leaf traversal hook.
- Moving instances are rejected before child leaves are flattened, preserving
  the all-or-nothing fallback contract.
- Unsupported leaves are represented explicitly with primitive names, object
  ids, and fallback reasons.
- The compiled BVH now emits a deterministic bounded-leaf tree over primitive
  records using a centroid-axis Surface Area Heuristic sweep when it improves
  expected traversal cost, with median fallback to preserve the four-record
  leaf bound for overlapping or degenerate inputs.

## Phase 3 - GPU backend stubs and UI/rendercli plumbing

Tasks:

- Add Metal/Vulkan backend classes that implement availability checks and return
  "unavailable" or "scene unsupported" without rendering.
- Add rendercli option:
  `--wavefront_intersection_backend auto|cpu|gpu`.
- Add Modeler Render Settings advanced control for wavefront/path-tracer
  intersection backend, hidden for engines that do not use wavefront batches.
- Add graph trace/properties display for selected backend and fallback reason.
- Keep `auto` selecting CPU until a GPU backend passes parity gates.

Gate:

- Users can request GPU and get a clear fallback, not a crash.
- Tests cover invalid option combinations and trace fallback metadata.

Progress:

- `MetalWavefrontIntersectionBackend` and `VulkanWavefrontIntersectionBackend`
  are now explicit platform stubs. They report availability/fallback metadata,
  name the host platform for a `gpu` request, and still report CPU fallback
  until real kernels land.
- Scene-created GPU stubs now retain the supported
  `CompiledIntersectionScene` they were prepared from. Static platform
  singletons and unsupported-scene fallbacks remain stateless, but the supported
  `gpu` path now has an owned compiled artifact ready for future upload handles
  instead of discarding the diagnostic compile result.
- Wavefront metrics now expose whether an intersection scene was compiled plus
  BVH node, primitive, supported payload, transform, and unsupported-leaf
  counts. rendercli compact summaries and the Modeler render graph pass tooltip
  surface the same counts beside the backend fallback reason, making the future
  upload workload visible before kernels exist.
- Supported scene-created GPU stubs now answer closest-hit, packet closest-hit,
  and any-hit queries from their retained compiled scene through the CPU parity
  intersector. They still report a CPU fallback because no Metal/Vulkan
  render-path intersection kernel has executed, but they no longer re-enter the
  runtime `Scene` for supported query shapes.
- Backend metrics now also name the actual query execution path
  (`runtime_scene` or `compiled_cpu`) separately from the requested and resolved
  backend ids. This keeps the diagnostic honest while GPU requests still resolve
  to CPU fallback, and gives the future Metal/Vulkan kernels a clear parity
  gate when the label changes to a platform execution path.

## Phase 4 - triangle GPU closest-hit kernel

Tasks:

- Upload flat-array BVH nodes, triangle payloads, material/object ids, and ray
  frontiers.
- Implement iterative closest-hit BVH traversal in Metal and Vulkan compute.
- Return hit/miss records with primitive id, material id, object id, `t`,
  barycentric coordinates, and geometric normal.
- Read back hit records and let CPU wavefront shading continue as today.
- Keep the backend opt-in.

Gate:

- GPU and CPU hit records match for mesh-only scenes within explicit tolerances.
- Rendered output matches CPU wavefront for deterministic mesh scenes.
- Metrics separate upload, kernel, and readback time.

Progress:

- Vulkan-enabled builds now compile GLSL basic closest-hit and any-hit compute
  shaders into embedded SPIR-V, expose direct dispatch wrappers for CPU
  packed-intersector parity tests, and route prepared triangle, sphere, plane,
  rectangle, and disk scenes through the Vulkan wavefront backend when a Vulkan
  compute device can construct both pipelines, including static transform
  payloads. Unsupported Vulkan scenes continue to use packed CPU traversal with
  explicit fallback diagnostics.
- Metal-enabled builds now include opt-in basic closest-hit and any-hit wrappers
  that consume `GpuIntersectionScenePacker`'s triangle, sphere, plane,
  rectangle, disk, and static-transform packed scene buffers and write
  `GpuIntersectionHitRecord` and `GpuIntersectionOcclusionRecord` results.
  Focused tests compare the Metal results to `GpuIntersectionIntersector` for
  hit, miss, and bounded miss rays when a Metal device is present, while
  unsupported basic-kernel scene rejection is tested without requiring a
  device. Prepared exact-primitive and static-transform GPU requests can now
  route closest-hit and any-hit queries through that Metal path when a device is
  available.
- A CPU `CompiledIntersectionSceneIntersector` now traverses the compiled
  flat-array BVH and produces GPU-style closest-hit records for triangle
  payloads, including object/material ids, distance, point, normal, UVs, and
  barycentric coordinates. This is a parity harness for the upcoming
  Metal/Vulkan hit kernels; it is not selected as a render backend yet.
  Its traversal now also uses bounded node/primitive ray-box tests, nearest-hit
  pruning, and near-first BVH child visitation so the CPU parity harness follows
  the same pruning contract as the packed upload-buffer traversal.
- `GpuIntersectionScenePacker` now converts compiled BVH nodes, primitive
  records, triangle/sphere/plane/rectangle/disk payloads, exact OpenCylinder
  payloads, static transform payloads, ray work items, and miss records into
  16-byte-aligned POD buffers. It marks whether a compiled scene is eligible for
  the first basic hit kernel: all primitive records must be triangle, sphere,
  plane, rectangle, disk, or OpenCylinder records with either no transform or a valid static
  transform payload. This keeps the next kernel work focused on traversal and
  hit-record parity instead of ad hoc per-backend layout decisions.
- Scene-created GPU fallback stubs now retain those packed upload buffers next
  to the compiled scene. Wavefront metrics, rendercli compact summaries, and
  Modeler graph tooltips expose the retained upload byte count plus
  triangle-kernel, basic-kernel, and packed closest-hit eligibility, so each
  backend slice can switch from `compiled_cpu` to a platform execution path with
  visible parity gates.
- `GpuIntersectionIntersector` now executes iterative closest-hit BVH
  traversal directly against the packed upload buffers and writes GPU-style
  hit/miss records. Triangle, sphere, plane, rectangle, disk, exact
  OpenCylinder, and static instance prepared GPU fallbacks route closest-hit,
  packet closest-hit, and bounded any-hit queries through this packed CPU
  kernel contract.
  Packed CPU and Metal traversal now also test primitive-record bounds inside
  BVH leaves before running payload intersection, keeping host parity and the
  platform kernel aligned while reducing wasted per-payload tests.
  Closest-hit traversal now clamps node and primitive bound tests to the
  current nearest hit distance, so both packed CPU and Metal paths can skip
  farther BVH work after a closer hit is known.
  Those closest-hit paths also visit nearer BVH children first, making the
  distance cutoff useful earlier in front-to-back traversal.
  Metrics now record closest-hit and any-hit execution paths separately, so a
  Metal basic closest-hit/any-hit render can report `metal`, CPU packed fallback
  reports `packed_cpu`, and `compiled_cpu` plus `mixed` remain available for
  unsupported packed payloads or query paths that diverge. Packed closest-hit
  and packed any-hit eligibility are now separate diagnostics, and both require
  valid one-payload primitive records before a prepared backend can advertise
  that query path.
- Metal render-path closest-hit and any-hit queries now record backend timing
  buckets for host upload/setup, kernel dispatch/wait, and result readback.
  Those timings flow into wavefront metrics, rendercli summaries, and Modeler
  graph trace tooltips when a platform kernel actually runs; CPU fallback paths
  leave the buckets at zero so total intersection worker time remains
  comparable with and without GPU dispatch.
- Closest-hit and any-hit queries now carry the execution path they actually
  used alongside their timing buckets. Metrics prefer that per-query path over
  the backend's nominal path, so runtime platform-kernel failures that fall
  back to packed CPU traversal are visible as `packed_cpu` instead of being
  mislabeled as `metal`.
- The Metal wrapper now caches the default device, command queue, and compiled
  smoke/basic-hit compute pipelines across dispatches. Prepared Metal backends
  now also upload scene-side BVH, primitive, payload, and static count buffers
  once when the backend is created. Prepared closest-hit and any-hit dispatches
  reuse growable ray, result, and dynamic-count buffers across queries through a
  prepared-scene buffer pool, so concurrent worker threads can check out
  separate reusable buffer sets instead of serializing every dispatch on one
  shared query buffer.
- Prepared Vulkan backends now also have a scene-owned render-path object. It
  keeps the Vulkan instance, device, queue, descriptor layout, pipelines,
  command pool, and static scene buffers alive for the supported scene, so
  closest-hit and any-hit dispatches upload only the per-query rays/counts and
  read back the query result records. Those serialized prepared-scene dispatches
  now reuse growable ray, result, and count buffers across queries, leaving
  per-thread/per-command-pool dispatch parallelism as the next Vulkan-specific
  overhead reduction.

## Phase 5 - common exact primitives and static instances

Tasks:

- Add GPU kernels/payloads for sphere, plane, rectangle, disk, and static
  instance transforms.
- Add exact host/packed CPU payloads for other common primitives before
  enabling platform kernels for them. ✅ **Done.** OpenCylinder now compiles to
  an exact packed CPU payload and the Metal/Vulkan basic kernels consume the
  same payload for closest-hit and any-hit traversal.
- Preserve material/object ids through instance transforms.
- Add scene fixtures that mix triangles and exact primitives.
- Add parity tests for:
  - hit distance;
  - normal direction;
  - barycentric/UV reconstruction where relevant;
  - rendered image RMS.

Gate:

- The common docs/example scenes that use only supported primitives can opt into
  GPU intersection and match CPU wavefront output.

Progress:

- The CPU `CompiledIntersectionSceneIntersector` now covers sphere, plane,
  rectangle, and disk payloads in addition to triangles. It transforms rays into
  payload-local space for static instances, then transforms the resulting hit
  point and normal back through the compiled transform payload so non-uniform
  instance transforms match the runtime `Instance` semantics.
- Packed closest-hit traversal now covers sphere, plane, rectangle, disk, exact
  OpenCylinder, and static transform payloads in addition to triangles. These
  common exact primitives and static instances preserve material/object ids, hit
  distance, hit point, normal, UV where applicable, and empty barycentric
  channels through the same GPU-style hit record shape used by the triangle
  traversal. Metal and Vulkan basic closest-hit/any-hit kernels now consume the
  OpenCylinder payload natively.
- Box leaves now compile into the same triangle payload path using the canonical
  12-triangle box tessellation, while preserving the exact raytraced Box's
  default hit-UV semantics. This lets Box-only supported scenes use prepared
  packed traversal without adding a separate box kernel ABI.
- Compiled box triangles now carry per-triangle world-space BVH bounds instead
  of reusing the full box bounds on all 12 records, preserving the supported
  triangle ABI while making box-heavy prepared traversal less wasteful.
- Metal-enabled prepared sphere, plane, rectangle, disk, OpenCylinder, and
  static-transform scenes now share the render-path basic hit kernels with
  triangle scenes for closest-hit and any-hit queries. The optional platform
  smoke coverage compares OpenCylinder closest-hit records and any-hit
  occlusion records against the packed CPU intersector.
- Vulkan-enabled prepared sphere, plane, rectangle, disk, OpenCylinder, and
  static-transform scenes now share the render-path basic hit kernels with
  triangle scenes for closest-hit and any-hit queries. The optional platform
  smoke coverage compares OpenCylinder closest-hit records and any-hit
  occlusion records against the packed CPU intersector.
- Wavefront renderer-level parity tests now compare `cpu` and `gpu`
  intersection-backend requests on a deterministic supported Whitted scene that
  mixes sphere, triangle, rectangle, disk, OpenCylinder, and static instance
  payloads. The test asserts the prepared packed backend path is actually used,
  so future platform kernels have an image-level gate instead of only
  hit-record parity.
- Wavefront metrics JSON, rendercli summaries, render graph trace metadata, and
  Modeler graph metadata now count the full supported payload breakdown:
  triangles, spheres, planes, rectangles, disks, OpenCylinder payloads, static
  transforms, and unsupported leaves. The rendercli graph functional test pins
  those fields for a supported prepared scene so diagnostics stay aligned with
  the kernel set.

## Phase 6 - any-hit / occlusion queries

Tasks:

- Add `intersectAny(...)` GPU query for shadow/visibility rays.
- Add a batched any-hit backend entry point so multi-sample direct-light
  visibility can submit one bounded shadow-ray group per shading point instead
  of one backend call per sample.
- Define correctness rules for alpha/transparent materials. Initial any-hit can
  be geometry-only and used only where CPU semantics are equivalent.
- Add metrics for closest-hit vs any-hit batches.
- Add direct-light/path-tracing tests where shadow rays use the GPU backend.

Gate:

- Direct-light scenes match CPU wavefront/path-tracing output.
- Unsupported transparency/alpha semantics fall back to CPU instead of producing
  incorrect shadows.

Progress:

- `WavefrontIntersectionBackend` now has an `intersectAny(...)` query for
  shadow/visibility rays. The CPU backend delegates to `Scene::occludes(...)`,
  unsupported-scene fallbacks delegate to that CPU backend, and supported
  scene-created GPU stubs answer eligible packed scenes through
  `GpuIntersectionIntersector::intersectAny(...)`. Batched path-tracing
  direct-light visibility records any-hit query metrics through the selected
  backend while preserving finite light-distance bounds.
- `WavefrontIntersectionBackend` now also has an `intersectAnyBatch(...)`
  query for grouped visibility rays plus a `prefersAnyHitBatch(...)`
  capability hook. Path-tracing direct-light sampling collects all valid
  light-sample shadow rays for a shading point and submits them as one bounded
  any-hit batch only when the selected backend advertises that it wants grouped
  visibility; the CPU default preserves the existing scalar `intersectAny(...)`
  behavior for backends that do not specialize batching.
- `WavefrontIntersectionBackend` now has an arbitrary closest-hit batch query
  in addition to scalar and Ray4/Ray8 packet queries. Prepared packed GPU
  backends opt into that path, so path-tracing and Whitted frontiers can submit
  one closest-hit group per depth instead of one platform dispatch per small
  packet while CPU wavefront rendering keeps the existing packet path.
- Wavefront diagnostics now split submitted intersection rays by closest-hit and
  any-hit query family, so batched visibility can be inspected as both one
  backend query and multiple submitted shadow rays.
- Wavefront diagnostics now also retain closest-hit and any-hit execution paths
  separately alongside the combined backend execution path, so mixed query
  families stay inspectable instead of collapsing all detail to `mixed`.
- Direct-light visibility batching now also records per-depth any-hit batch
  chunks and rays for backend-preferred grouped visibility. rendercli summaries,
  metrics JSON, and the Modeler graph tooltip expose those counters beside
  closest-hit frontier batches, making the two GPU query families independently
  visible without labeling scalar CPU fallback loops as batches.
- Wavefront metrics now also expose whether the selected backend preferred
  closest-hit and any-hit batches for the observed query sizes. rendercli
  summaries, metrics JSON, and the Modeler graph tooltip can now explain
  whether missing batch counters mean no eligible query work happened or the
  backend intentionally stayed on scalar/packet traversal.
- `CompiledIntersectionSceneIntersector` now has a CPU any-hit parity query for
  supported compiled payloads and static instances. It uses the same bounded
  light-distance rule as `Scene::occludes(...)`, so GPU any-hit kernels have a
  tested visibility contract before they are wired into rendering.
- `GpuIntersectionIntersector` now mirrors that any-hit visibility contract over
  packed upload buffers for triangle, sphere, plane, rectangle, disk, exact
  OpenCylinder, and static transform payloads. Host-side Metal/Vulkan fallback
  stubs use it when the packed scene is eligible, so closest-hit and shadow
  query metrics both report `packed_cpu` for those scenes. Metal-enabled
  prepared triangle,
  sphere, plane, rectangle, disk, and static-transform scenes can now route
  any-hit queries through the Metal basic visibility kernel and report `metal`
  when a device is available.
- Packed any-hit traversal now exposes a batch record API that returns one
  `GpuIntersectionOcclusionRecord` per submitted ray. Prepared CPU fallback,
  Metal fallback, and Vulkan fallback paths use that same batch-shaped contract
  before platform kernels take over, keeping the host parity path aligned with
  the GPU upload/readback ABI.
- Current runtime `Scene::occludes(...)` shadow semantics are geometry-only,
  including transparent materials, so the packed any-hit path is allowed to be
  material-agnostic and still match the CPU renderer. If future alpha,
  volumetric, or partial-shadow materials make visibility material-dependent,
  the compiler must make those leaves ineligible for packed/GPU any-hit until a
  matching visibility kernel exists.
- Prepared packed visibility rays now carry `Ray<float>::epsilon` as their
  minimum hit distance. This preserves the intent of epsilon-shifted shadow rays
  after the double-precision CPU ray is packed into the float GPU ABI, avoiding
  near-surface self-shadowing in path-traced direct lighting. A renderer-level
  parity test now compares `cpu` and `gpu` intersection-backend requests for a
  direct-light path-tracing scene and asserts both closest-hit and any-hit
  queries were exercised.

## Phase 7 - automatic selection and performance gates

Tasks:

- Add heuristic selection for `auto`:
  - GPU only when backend is available;
  - scene compiles fully;
  - frontier size or expected ray count justifies upload/readback cost.
- Add rendercli metrics summary fields for backend choice and transfer costs.
- Add benchmark fixtures for mesh-heavy scenes and small scenes where CPU should
  remain faster.

Gate:

- GPU intersection is measurably faster on large supported scenes.
- `auto` does not regress small scenes.
- CPU fallback remains deterministic and visible in traces.

Progress:

- Wavefront intersection metrics now estimate the query transfer footprint for
  the packed GPU ABI: ray upload bytes, closest-hit readback bytes, any-hit
  readback bytes, and their per-render query-transfer total. CPU and unsupported
  runtime-scene fallback paths report zero query-transfer bytes; prepared
  GPU-request stubs report the bytes their retained packed buffers would submit
  to a real Metal/Vulkan kernel. This gives `auto` selection and performance
  gates a visible upload/readback cost signal before real kernels are enabled.
- `auto` backend selection now has an explicit policy object and receives a
  conservative expected-ray-count estimate from `WavefrontRaytracer`. It
  requires platform GPU device availability, platform render-path availability,
  a fully supported packed intersection scene, and enough expected ray work to
  clear both the fixed minimum ray-count gate and a scene-upload amortization
  gate before choosing the GPU path. When no platform render-path kernel is
  available in the build or runtime, `auto` stays on the runtime CPU backend
  and reports that selection reason in render metrics and graph trace.
- The expected-ray-count estimate now comes from the selected integrator.
  Whitted renders scale by recursion depth, while path tracing scales by bounce
  count and direct-light visibility samples; metrics JSON records the final
  estimate so render graph traces explain the `auto` backend decision.
- rendercli metrics summaries and the Modeler render graph tooltip now surface
  that expected-ray estimate alongside backend choice, fallback, execution path,
  scene upload, and query-transfer diagnostics.
- Those same diagnostics now also surface the computed
  `auto` minimum GPU ray threshold, so small renders explain both the expected
  ray count and the cutoff that kept automatic backend selection on CPU.
- The benchmark suite now includes wavefront intersection backend fixtures for
  a small supported scene, a mesh-heavy supported scene, and an unsupported
  mixed scene. The fixtures measure compile/pack cost, runtime CPU closest-hit
  queries, packed closest-hit queries, and packed any-hit queries, and they
  report primitive/BVH counts plus scene upload, ray upload, and readback byte
  counters. This gives the `auto` policy a repeatable baseline for deciding
  when GPU upload/readback overhead is justified.
- Platform-enabled benchmark builds now also register requested-GPU closest-hit
  and any-hit batch fixtures for the small and mesh-heavy supported scenes.
  They run through the public `WavefrontIntersectionBackend` batch interface,
  skip with the backend fallback reason when no platform path is available, and
  label the selected backend/execution path so Metal/Vulkan timings can be
  compared directly with the runtime CPU and packed CPU baselines.
- Benchmark builds now also register automatic-backend closest-hit and any-hit
  batch fixtures for those same supported scenes. Those entries do not require a
  platform GPU build; they label the requested/resolved/execution path selected
  by `auto`, making the small-scene CPU gate and large-scene GPU gate visible in
  benchmark output.
- A regular unit-test performance gate now pins the packed upload-buffer CPU
  traversal against runtime `Scene` traversal on a mesh-heavy supported scene.
  The threshold is deliberately conservative, but it catches regressions that
  accidentally bypass the flat packed BVH before platform kernels make that path
  the default on eligible renders.
- Renderer-level image parity now also covers the user-facing `auto`
  intersection backend when the expected ray workload clears the GPU-selection
  threshold. Platform-enabled runs therefore exercise the same automatic path
  users will rely on, while platform-unavailable runs keep proving the visible
  CPU fallback branch.
- `auto` selection now gates on packed closest-hit and packed any-hit
  eligibility separately, rather than only the coarse basic-hit scene flag. That
  keeps automatic GPU routing tied to the two query families the renderer
  actually needs and prevents future one-sided kernel support from being
  selected automatically before both paths are ready.

## Phase 8 - future work

Possible follow-ups after the hybrid intersection backend is stable:

- GPU-resident frontiers across wavefront depths to reduce readback.
- GPU-side compaction of active rays.
- GPU-side direct-light occlusion batches.
- Hardware ray tracing backends:
  - Vulkan RT on capable Linux systems;
  - Metal ray tracing on supported macOS systems.
- Full GPU shading/path transport for a restricted material subset.
- Shared shader source strategy if native Metal/Vulkan kernels become too much
  duplicate maintenance.

---

## Testing strategy

Tests should be layered so GPU availability is optional:

- Unit tests for compiled intersection-scene records and fallback reasons run on
  every machine.
- CPU backend tests run everywhere and pin behavior after the backend refactor.
- GPU availability tests skip cleanly when Metal/Vulkan is unavailable.
- GPU parity tests run only when the backend is available:
  - closest-hit record parity;
  - rendered image RMS parity;
  - trace metadata presence;
  - fallback behavior for unsupported scenes.
- rendercli functional tests cover:
  - `--wavefront_intersection_backend cpu`;
  - `--wavefront_intersection_backend gpu` with fallback when unavailable;
  - graph export includes backend state;
  - trace reports backend and fallback reason.

No GPU backend should become the default until it has both correctness parity and
performance evidence on at least one large supported scene.

## Documentation and UI

- Update the wavefront/path-tracing textbook chapter once the CPU backend
  refactor lands, explaining that scheduling and intersection backend are
  separate choices.
- Add a small diagram/widget only after there is a working backend boundary:
  CPU shading loop, CPU/GPU intersection box, and hit records flowing back.
- Modeler should show backend choice and fallback reason in the render graph
  selected-pass metadata.
- rendercli should print compact backend diagnostics in metrics summaries.
