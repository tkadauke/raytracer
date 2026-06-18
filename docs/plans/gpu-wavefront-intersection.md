# GPU-assisted wavefront intersection plan - June 2026

> **Plan ownership update:** this is no longer the top-level GPU tracing goal.
> It is the reusable closest-hit/any-hit intersection-service slice under
> `docs/plans/tracing-execution-backends.md`. Keep this plan active for
> platform intersection kernels, fallback behavior, parity tests, and hybrid
> visibility use cases. Do not expand it into material evaluation, BSDF
> sampling, path-state residency, accumulation, or full GPU path tracing; those
> belong to the parent tracing execution backend plan.

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
> rectangle, disk, exact OpenCylinder, exact Torus, and static-transform
> payloads can run through the packed CPU kernel contract and the platform
> basic-kernel contract.
> Transparent-material leaves now explicitly opt out of the packed
> intersection scene so glass/refraction renders stay on the runtime CPU
> intersection path until the packed/GPU hit metadata contract is precise
> enough for Whitted continuation rays.
> Metal-only smoke kernels now prove optional compute dispatch
> outside the render path, and the first render-path Metal basic closest-hit
> and any-hit kernels can execute for prepared triangle, sphere, plane,
> rectangle, disk, OpenCylinder, and Torus scenes, including static transform
> payloads, when a Metal device is available. Vulkan-enabled builds can now run
> basic closest-hit and any-hit kernels for prepared triangle, sphere, plane,
> rectangle, disk, OpenCylinder, and Torus scenes, including static transform
> payloads. This is a
> follow-up to
> `docs/plans/wavefront-and-path-tracing.md` Phase 7+ and is now a child slice
> of `docs/plans/tracing-execution-backends.md`. It should not replace the CPU
> wavefront renderer, and it should not attempt a full GPU path tracer in this
> slice.

---

## Goals

- Preserve GPU intersection as a reusable service for wavefront tracing, hybrid
  raster/tracing shadows, visibility queries, graph AOV/debug passes, and
  future full GPU tracing backends.
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
  in this plan. Those belong to `tracing-execution-backends.md` milestones for
  compiled tracing scenes, GPU direct lighting, GPU BSDF/path transport, and
  full GPU tracing.

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

GPU v1 platform kernels should reject:

- CSG/boolean composites;
- curve, convex operation, and other exact primitives not yet ported to
  Metal/Vulkan shaders;
- moving instances;
- scenes with primitive/material references that cannot be represented by
  stable ids.

The host-side compiled and packed CPU contracts may support more primitives
than the platform kernels. `auto` still requires stricter platform
basic-kernel eligibility before selecting a GPU backend, so newly compiled
payloads should remain packed-CPU-only until Metal and Vulkan have matching
shader support.

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
  triangle, sphere, plane, rectangle, disk, OpenCylinder, and Torus scenes,
  including static transform payloads, against the packed
  BVH/primitive/payload/ray ABI. The smoke path remains a
  platform-plumbing proof; the basic hit kernels are now selected only for
  eligible prepared scenes.
- Platform diagnostics now distinguish disabled plumbing, enabled-without-device,
  prepared-scene ineligibility, and active platform execution.
- Vulkan-enabled builds now also probe the loader for a physical device with a
  compute queue and can run a tiny deterministic compute dispatch/readback
  smoke kernel. That early probe now feeds the same device/render-path
  availability diagnostics used by the prepared Vulkan closest-hit and any-hit
  kernels, so `auto` can distinguish missing Vulkan compute support from a
  missing render-path kernel.
- Vulkan-enabled builds now compile the smoke compute shader from GLSL into
  generated SPIR-V at build time, replacing the hand-written C++ word array and
  creating the native shader pipeline needed for Vulkan render-path kernels.
- Vulkan-enabled builds now compile and expose direct basic closest-hit and
  any-hit compute dispatches against the packed BVH/primitive/exact-payload/ray
  ABI. Prepared triangle, sphere, plane, rectangle, disk, OpenCylinder, and
  Torus scenes can now execute wavefront closest-hit and any-hit batches through
  Vulkan, including static transform payloads.
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
  triangle/mesh-triangle, sphere, plane, rectangle, disk, OpenCylinder, and
  Torus leaves.
- Static instances are captured as transform payloads through the existing
  transformed-leaf traversal hook.
- Moving instances are rejected before child leaves are flattened, preserving
  the all-or-nothing fallback contract.
- Unsupported leaves are represented explicitly with primitive names, object
  ids, and fallback reasons.
- Compiled unsupported scenes now summarize unsupported leaves by first-seen
  reason, and GPU-request fallback text includes those reason counts when more
  than one unsupported leaf prevents using the packed/platform backend.
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
  counts, including unsupported leaves grouped by fallback reason. rendercli
  compact summaries and the Modeler render graph pass details surface the same
  counts beside the backend fallback reason, making the future upload workload
  and unsupported-scene shape visible before kernels exist.
- Unsupported scene fallbacks now keep those compiled primitive and unsupported
  counts without packing throwaway GPU upload buffers. Their diagnostics report
  zero scene-upload bytes unless a supported prepared backend retained packed
  buffers.
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
- Triangle payloads now carry the source leaf's minimum hit distance. Flat mesh
  triangles therefore keep their runtime near-hit cutoff through the compiled,
  packed CPU, Metal, and Vulkan basic-kernel ABI instead of being treated like
  generic zero-minimum triangles.
- `GpuIntersectionScenePacker` now converts compiled BVH nodes, primitive
  records, triangle/sphere/plane/rectangle/disk payloads, exact OpenCylinder
  payloads, static transform payloads, ray work items, and miss records into
  16-byte-aligned POD buffers. It marks whether a compiled scene is eligible for
  the first basic hit kernel: all primitive records must be triangle, sphere,
  plane, rectangle, disk, OpenCylinder, or Torus records with either no
  transform or a valid static transform payload. This keeps the next kernel work
  focused on traversal and hit-record parity instead of ad hoc per-backend
  layout decisions.
- Scene-created GPU fallback stubs now retain those packed upload buffers next
  to the compiled scene. Wavefront metrics, rendercli compact summaries, and
  Modeler graph tooltips expose the retained upload byte count plus
  triangle-kernel, basic-kernel, and packed closest-hit eligibility, so each
  backend slice can switch from `compiled_cpu` to a platform execution path with
  visible parity gates.
- `GpuIntersectionIntersector` now executes iterative closest-hit BVH
  traversal directly against the packed upload buffers and writes GPU-style
  hit/miss records. Triangle, sphere, plane, rectangle, disk, exact
  OpenCylinder, exact Torus, and static instance prepared GPU fallbacks route
  closest-hit, packet closest-hit, and bounded any-hit queries through this
  packed CPU kernel contract.
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
  keeps the Vulkan instance, device, queue, descriptor layout, pipelines, and
  static scene buffers alive for the supported scene, so
  closest-hit and any-hit dispatches upload only the per-query rays/counts and
  read back the query result records. Prepared-scene dispatches now lease
  per-query command pools plus growable ray, result, and count buffers across
  queries, and they wait on per-dispatch fences after a short synchronized queue
  submit instead of holding one render-path mutex through upload, dispatch, and
  readback.

## Phase 5 - common exact primitives and static instances

Tasks:

- Add GPU kernels/payloads for sphere, plane, rectangle, disk, and static
  instance transforms.
- Add exact host/packed CPU payloads for other common primitives before
  enabling platform kernels for them. ✅ **Done.** OpenCylinder and Torus now
  compile to exact packed CPU payloads, and the Metal/Vulkan basic kernels
  consume the same payloads for closest-hit and any-hit traversal.
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
  OpenCylinder, Torus, and static transform payloads in addition to triangles.
  These common exact primitives and static instances preserve material/object
  ids, hit distance, hit point, normal, UV where applicable, and empty
  barycentric channels through the same GPU-style hit record shape used by the
  triangle traversal. Metal and Vulkan basic closest-hit/any-hit kernels now
  consume the OpenCylinder and Torus payloads natively.
- Disk payloads now carry their runtime near-hit cutoff through the compiled
  scene and packed GPU ABI, keeping the host parity intersector plus the Metal
  and Vulkan basic kernels aligned with the runtime disk intersection rule
  instead of duplicating a hard-coded shader constant.
- Packed CPU, Metal, and Vulkan closest-hit paths now preserve plane,
  rectangle, and disk payload normals in local space, normalizing only after a
  static instance transform is applied. That keeps GPU-style hit records
  aligned with the compiled/runtime primitive contract for non-unit payload
  normals.
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
- Vulkan-enabled prepared sphere, plane, rectangle, disk, OpenCylinder, Torus,
  and static-transform scenes now share the render-path basic hit kernels with
  triangle scenes for closest-hit and any-hit queries. The optional platform
  smoke coverage compares OpenCylinder and Torus closest-hit records and any-hit
  occlusion records against the packed CPU intersector.
- Wavefront renderer-level parity tests now compare `cpu` and `gpu`
  intersection-backend requests on a deterministic supported Whitted scene that
  mixes sphere, triangle, rectangle, disk, OpenCylinder, Torus, and static
  instance payloads. The test asserts the prepared packed backend path is
  actually used, so future platform kernels have an image-level gate instead of
  only hit-record parity.
- Wavefront metrics JSON, rendercli summaries, render graph trace metadata, and
  Modeler graph metadata now count the full supported payload breakdown:
  triangles, spheres, planes, rectangles, disks, OpenCylinder payloads, Torus
  payloads, static transforms, and unsupported leaves. The rendercli graph
  functional test pins
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
- Wavefront path tracing now groups direct-light visibility rays across the
  active depth frontier when the selected backend prefers any-hit batching.
  That turns many per-hit visibility batches into one backend query for the
  bounce while preserving the same light-selection, shadow-distance, MIS, and
  per-sample direct-light diagnostics.
- Wavefront metrics now also expose whether the selected backend preferred
  closest-hit and any-hit batches for the observed query sizes. rendercli
  summaries, metrics JSON, and the Modeler graph tooltip can now explain
  whether missing batch counters mean no eligible query work happened or the
  backend intentionally stayed on scalar/packet traversal.
- rendercli compact summaries plus the Modeler graph tooltip and selected-pass
  property rows now also report average rays per closest-hit and direct-light
  any-hit backend batch. This keeps frontier grouping effectiveness visible
  while the full per-depth arrays remain in the metrics JSON.
- `CompiledIntersectionSceneIntersector` now has a CPU any-hit parity query for
  supported compiled payloads and static instances. It uses the same bounded
  light-distance rule as `Scene::occludes(...)`, so GPU any-hit kernels have a
  tested visibility contract before they are wired into rendering.
- `GpuIntersectionIntersector` now mirrors that any-hit visibility contract over
  packed upload buffers for triangle, sphere, plane, rectangle, disk, exact
  OpenCylinder, exact Torus, and static transform payloads. Host-side
  Metal/Vulkan fallback stubs use it when the packed scene is eligible, so
  closest-hit and shadow query metrics both report `packed_cpu` for those
  scenes. Metal-enabled
  prepared triangle,
  sphere, plane, rectangle, disk, OpenCylinder, Torus, and static-transform
  scenes can now route any-hit queries through the Metal basic visibility
  kernel and report `metal` when a device is available.
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
- rendercli functional coverage now also renders that direct-light
  path-tracing scene with explicit CPU and GPU intersection backend requests,
  using the same sampling seed and sample count, and asserts image RMS parity
  while the area-light path-tracing fixture continues to report requested-GPU
  batched any-hit visibility metrics.

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
  readback bytes, their per-render query-transfer total, and the estimated
  number of query round trips at the current host/device boundary, split by
  closest-hit and any-hit query family. CPU and unsupported runtime-scene
  fallback paths report zero query-transfer bytes and zero query round trips,
  and unsupported compiled-scene fallbacks report zero scene-upload bytes;
  prepared GPU-request stubs report the bytes and query boundaries their
  retained packed buffers would submit to a real Metal/Vulkan kernel only for
  query families that are actually packed/platform eligible. This gives `auto`
  selection and performance gates a visible upload/readback cost signal before
  real kernels are enabled, and gives Phase 8 GPU-resident-frontier work a
  baseline to reduce.
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
- Auto-selection diagnostics now also carry a conservative pre-render estimate
  of the packed GPU query transfer bytes implied by the expected ray count. The
  estimate is present in metrics JSON, rendercli summaries, and Modeler pass
  details/tooltips, so an auto-selected CPU render can show the avoided
  upload/readback footprint rather than only reporting the observed CPU work.
- Integrators now split expected intersection work into closest-hit and any-hit
  query families. Whitted renders report closest-hit work only, while path
  tracing reports closest-hit bounce work separately from direct-light
  visibility work, letting the auto transfer estimate use hit-record and
  occlusion-record readback sizes independently.
- The base integrator contract now derives the total expected intersection-ray
  count from the closest-hit and any-hit family estimates by default. Custom
  integrators can therefore override the two query-family estimates without
  accidentally leaving automatic backend selection on a stale total estimate.
- `WavefrontRaytracer` now also derives the expected-ray total passed to
  automatic backend selection from the closest-hit and any-hit family estimates,
  keeping render metrics and selection policy on the same split workload even
  for custom integrators with an inconsistent legacy total estimate.
- The backend selection context now owns that closest-hit plus any-hit
  derivation, so renderer code and benchmark fixtures use the same saturated
  expected-ray total when preparing automatic backend selection.
- The automatic backend policy now reads the context's effective expected-ray
  total, preferring closest-hit/any-hit family estimates when present so direct
  policy callers cannot select from a stale legacy total.
- `auto` backend selection now rejects workloads below the fixed GPU ray-count
  threshold before probing the platform backend or compiling and packing the
  scene, avoiding platform and scene-preparation overhead for small renders that
  cannot select GPU anyway.
- The benchmark suite now includes wavefront intersection backend fixtures for
  a small supported scene, a mesh-heavy supported scene, and an unsupported
  mixed scene. The fixtures measure compile/pack cost, runtime CPU closest-hit
  and any-hit queries, packed closest-hit queries, and packed any-hit queries,
  and they report primitive/BVH counts plus scene upload, ray upload, and
  readback byte counters. This gives the `auto` policy a repeatable baseline
  for deciding when GPU upload/readback overhead is justified.
- Platform-enabled benchmark builds now also register requested-GPU closest-hit
  and any-hit batch fixtures for the small and mesh-heavy supported scenes.
  They run through the public `WavefrontIntersectionBackend` batch interface,
  skip with the backend fallback reason when no platform path is available, and
  label the selected backend/execution path so Metal/Vulkan timings can be
  compared directly with the runtime CPU and packed CPU baselines.
- Platform-enabled benchmark builds now also include a requested-GPU mixed
  closest-hit/any-hit fixture, giving explicit Metal/Vulkan requests the same
  combined query-family workload shape as the automatic-backend benchmark.
- Benchmark builds now also register automatic-backend closest-hit and any-hit
  batch fixtures for those same supported scenes. Those entries do not require a
  platform GPU build; they label the requested/resolved/execution path selected
  by `auto`, making the small-scene CPU gate and large-scene GPU gate visible in
  benchmark output.
- The automatic-backend benchmark fixtures now feed closest-hit and any-hit
  expected ray counts into the same selection context used by the renderer and
  report those family counts, the computed automatic GPU threshold, and the
  estimated query-transfer bytes as benchmark counters. They now also report
  compiled-scene, BVH/primitive/unsupported, upload-byte, and packed
  closest-hit/any-hit eligibility counters, so small-workload preflight rows
  visibly show that no scene was prepared. Backend benchmark rows also report
  closest-hit, any-hit, and combined estimated query round trips beside the
  upload/readback byte counters. This keeps benchmark evidence aligned with
  render metrics instead of treating all intersection work as one
  undifferentiated ray count.
- Benchmark builds now also include a mixed automatic-backend fixture that
  chooses a backend from combined closest-hit and any-hit expected work, then
  submits both query families in the same iteration. That better represents the
  path-tracing render path, where a bounce can need both frontier intersection
  and direct-light visibility work after one backend selection.
- Benchmark builds now also include an explicit GPU-requested unsupported-scene
  mixed closest-hit/any-hit fixture. It proves the all-or-nothing fallback path
  can be timed separately from supported prepared scenes, and benchmark
  transfer counters now come from the selected backend's query-family estimates
  so unsupported fallback rows show zero upload/readback cost.
- Wavefront intersection backend benchmarks now also publish the
  resident-frontier round-trip estimate and savings counters for mixed
  closest-hit plus any-hit query-family rows, keeping benchmark evidence
  aligned with render metrics and the Phase 8 frontier-residency target.
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
- rendercli functional coverage now also pins explicit CPU wavefront
  intersection backend metrics and JSON trace fields, proving the non-fallback
  path reports `cpu`/`available`/`runtime_scene` with zero GPU transfer.
- rendercli functional coverage now also exercises a large enough supported
  `auto` workload to clear the fixed GPU ray-count threshold. Platform builds
  can therefore prove the compiled/eligible branch, while CPU-only builds prove
  the decision moves past small-workload preflight and reports platform GPU
  unavailability explicitly.
- rendercli functional coverage now also pins explicit GPU requests for an
  unsupported exact-primitive scene, proving the fallback reports compiled
  unsupported counts but zero scene-upload and query-transfer bytes.
- `auto` selection now gates on packed closest-hit and packed any-hit
  eligibility separately, rather than only the coarse basic-hit scene flag. That
  keeps automatic GPU routing tied to the two query families the renderer
  actually needs and prevents future one-sided kernel support from being
  selected automatically before both paths are ready.

## Phase 8 - future work

Possible follow-ups after the hybrid intersection backend is stable:

- GPU-resident frontiers across wavefront depths to reduce readback.
- GPU-side compaction of active rays.
- GPU-resident direct-light occlusion batches that stay on device across
  shading/frontier phases.
- Hardware ray tracing backends:
  - Vulkan RT on capable Linux systems;
  - Metal ray tracing on supported macOS systems.
- Full GPU shading/path transport for a restricted material subset.
- Shared shader source strategy if native Metal/Vulkan kernels become too much
  duplicate maintenance.

Progress:

- The path-tracing scheduler now records its CPU-side frontier compaction
  operation explicitly: pass count, input path slots, retained path slots,
  removed inactive slots, moved live slots, removed fraction, and execution
  path. This is still CPU-side compaction, but it creates a concrete execution
  contract for a future GPU-side compaction kernel.
- Integrator and wavefront summary compaction counters now use generic
  frontier-compaction names internally, while the metrics JSON keeps the older
  `frontierHostCompaction*` aliases. That keeps the API ready for Metal/Vulkan
  compaction execution paths without breaking existing trace consumers.
- Frontier compaction requests now reject duplicate or decreasing retained path
  indices as they are built, not only when a result is materialized. Future
  GPU-side compaction code can therefore assume a strictly increasing retained
  frontier when consuming the backend contract.
- Frontier compaction now goes through the resolved
  `WavefrontIntersectionBackend`: the path tracer builds a retained-frontier
  request, the backend returns a compaction result with an execution path, and
  current backends answer with the existing host compaction behavior. That makes
  GPU-side compaction an overridable backend operation instead of a
  path-tracer-private list edit.
- Closest-hit batch frontiers now also go through a backend-owned frontier
  handle. The current handle is host-resident and wraps the existing query
  vector, but the path tracer asks the backend to create and consume the
  frontier so a later Metal/Vulkan backend can keep those rays resident without
  changing path-tracer control flow.
- Whitted closest-hit batch frontiers now use the same backend-owned frontier
  handle, so Whitted and path tracing report comparable closest-hit frontier
  residency and packed-ray payload diagnostics.
- Direct-light any-hit batches now use the same backend-owned frontier shape.
  Current frontiers are host-resident and still route through the existing
  any-hit batch implementation, but next-event-estimation visibility work now
  has a backend boundary for future resident occlusion batches.
- Backend-owned closest-hit and any-hit frontiers now report their actual
  residency through wavefront metrics, rendercli summaries, graph traces, and
  Modeler pass details. CPU/runtime frontiers report `host`, packed prepared
  fallback frontiers report `packed_host`, and platform frontiers can report
  their Metal/Vulkan residency without changing the UI or metrics contract
  again.
- The wavefront convergence capture helper now carries those frontier
  compaction counters into candidate/reference comparisons and queue-sweep
  summaries, and queue sweeps also preserve the compaction execution label.
  Phase 8 scheduler experiments can see whether a queue policy or future GPU
  compaction path actually reduces retained inactive path state.
- The same capture helper now also preserves closest-hit batch, direct-light
  any-hit batch, mixed-query-depth, resident-frontier round-trip counters, and
  closest-hit/any-hit frontier residency labels in comparison reports and
  queue-sweep summaries. That makes benchmark captures line up with the render
  metrics used to judge GPU-resident frontier work.
- The capture helper now also preserves closest-hit and any-hit frontier
  packed-ray byte counters in comparison reports and queue-sweep summaries, so
  benchmark captures include the same packed-host payload baseline as rendercli
  and the Modeler graph.
- Wavefront intersection backend benchmarks now publish the same packed frontier
  ray byte counters as render metrics and convergence captures, including
  closest-hit, any-hit, and total packed frontier payload sizes.
- The backend contract now exposes explicit Phase 8 capability flags for
  resident frontiers, GPU frontier compaction, and resident direct-light
  batches. CPU and packed fallback backends report these as unsupported;
  prepared Metal/Vulkan backends opt into resident-frontier support when their
  platform scene is available, while GPU compaction and resident direct-light
  batches remain gated off. The values flow through render metrics, rendercli
  summaries, and Modeler graph details so future implementation can be gated
  visibly.
- The resident-direct-light flag is now explicitly documented and tested as a
  stricter capability than platform any-hit frontiers: current Metal/Vulkan
  frontiers may own packed occlusion rays, but shading still creates those rays
  on the host and reads results back immediately.
- The convergence capture helper now also keeps those capability flags in
  reference/candidate comparisons and queue-sweep summaries, so future Phase 8
  captures can distinguish an estimated opportunity from a backend that
  actually supports resident scheduling.
- Wavefront intersection backend benchmarks now publish the same resident
  capability flags next to their resident-frontier round-trip estimates, so
  benchmark rows can be filtered by real support rather than only by estimated
  opportunity.
- Wavefront intersection backend benchmarks now include frontier-compaction
  rows that run through `WavefrontIntersectionBackend::compactFrontier` and
  report input/retained/removed/moved samples plus GPU-compaction support. The
  current rows time the CPU behavior, and future GPU compaction kernels can
  override the same hook without changing the benchmark shape.
- Frontier compaction results now expose their removed-path fraction directly,
  so renderer metrics, benchmarks, and future GPU kernels can report the same
  normalized compaction value without reimplementing the arithmetic at each
  call site.
- Wavefront intersection backend batch benchmarks now submit closest-hit and
  any-hit workloads through backend-owned frontier handles instead of bypassing
  the frontier contract with raw batch calls. That keeps benchmark evidence
  aligned with the renderer path that future resident frontiers will override.
- Prepared GPU-intersection backends now create closest-hit and any-hit
  frontiers that own the packed GPU ray payload at frontier-construction time.
  The packed fallback handles still live on the CPU and report `packed_host`
  residency, but the path tracer no longer needs to hand raw query vectors to
  the prepared backend before the rays take the packed-kernel shape.
- Prepared closest-hit and any-hit frontiers now have a polymorphic packed
  execution hook. The current `packed_host` frontiers still replay packed rays
  through the backend from CPU memory, while platform Metal/Vulkan frontiers can
  execute from platform-owned query buffers without adding type switches to the
  backend intersection path.
- Packed closest-hit and any-hit frontier execution no longer requires the
  frontier to expose its original host query vector. The backend reconstructs
  results from ray count plus per-ray state hooks, which is a smaller host
  dependency for future resident frontier handles.
- Metal prepared scenes now expose a prepared packed-ray batch object. It
  uploads a packed frontier once, reports ray count and packed bytes, and can be
  reused by closest-hit and any-hit dispatches without copying the ray vector
  inside each dispatch.
- Vulkan prepared scenes now expose the same prepared packed-ray batch API. In
  Vulkan-enabled builds the batch uploads the packed frontier and matching
  count buffer once, retains prepared-scene device lifetime, and can be reused
  by closest-hit and any-hit prepared-scene dispatches.
- Metal and Vulkan prepared scenes now also expose a prepared ray-batch
  compaction primitive. It copies retained ray records into a new prepared
  batch with platform compute, updates the dynamic ray-count buffer, and is
  parity-tested by reusing the compacted batch for closest-hit and any-hit
  dispatches. The renderer still performs `BatchPath` compaction on the host,
  so the public GPU frontier-compaction capability remains disabled until this
  primitive is wired into scheduler-owned active path state.
- Platform-enabled wavefront intersection benchmarks now include prepared
  ray-batch compaction rows for Metal and Vulkan. These rows time the new
  lower-level platform compaction primitive directly and publish the same
  input/retained/removed sample counters as the public frontier-compaction
  rows, while keeping public GPU frontier compaction marked unsupported.
- Render metrics, rendercli compact summaries, convergence captures, and the
  Modeler graph details now expose prepared ray-batch compaction as its own
  capability flag. That keeps the new platform ray-buffer primitive visible
  without conflating it with scheduler-owned GPU frontier compaction.
- Metal GPU-intersection backends now create `metal_shared` closest-hit and
  any-hit frontiers when a prepared Metal scene is available. Those frontiers
  dispatch through their prepared ray batch and fall back to `packed_host`
  behavior if Metal frontier preparation fails.
- Vulkan GPU-intersection backends now create `vulkan_host_coherent`
  closest-hit and any-hit frontiers in Vulkan-enabled builds when a prepared
  Vulkan scene is available. Those frontiers dispatch through their prepared
  ray batch and fall back to `packed_host` behavior if Vulkan frontier
  preparation fails.
- Metal and Vulkan prepared frontiers now carry ray-preparation/upload time
  into the later intersection timing record, so render metrics and benchmark
  rows include the transfer work moved from dispatch time to frontier-creation
  time.
- `packed_host` prepared frontiers now carry CPU ray-packing time into the
  same preparation/upload timing record, so the fallback path reports frontier
  creation work consistently with platform prepared frontiers.
- Metal and Vulkan prepared frontiers now retain only per-ray `State*` handles
  plus the packed ray payload after construction. The host query vectors are
  discarded once rays are packed, reducing the host-side data that future
  resident frontier handles need to mirror.
- Backend-owned closest-hit and any-hit frontiers now report packed-ray byte
  counts through wavefront metrics, rendercli summaries, graph traces, and the
  Modeler pass details. `packed_host`, `metal_shared`, and
  `vulkan_host_coherent` frontiers contribute the retained packed-ray payload
  size, giving GPU-resident frontier work a concrete payload baseline.
- Backend-owned frontiers now also report retained host-query byte counts.
  Runtime host frontiers expose the original query-vector footprint, while
  `packed_host` and platform resident frontiers report zero once they have
  discarded that vector. This makes the remaining host-side dependency visible
  separately from packed ray payload size.
- `packed_host` prepared frontiers now retain only per-ray `State*` handles
  plus the packed ray payload after construction. That matches the platform
  frontier shape more closely and removes the last original-query-vector
  dependency from the prepared host fallback path.
- Backend-owned frontiers now also report retained state-handle byte counts.
  Host frontiers report zero because their state pointers are part of the
  original query vector, while `packed_host` and platform frontiers report the
  separate per-ray `State*` array they still need for CPU-side shading updates.
- Wavefront metrics now report mixed query depths: depth frontiers where both a
  closest-hit frontier batch and a direct-light any-hit chunk ran, plus the
  participating closest-hit and any-hit ray counts. This does not keep frontiers
  on the GPU yet, but it gives rendercli, graph traces, and Modeler diagnostics
  a baseline for deciding where GPU-resident frontier/direct-light scheduling
  would remove host/device boundary crossings.
- Wavefront metrics now also report compaction candidate depths and sample
  counts by comparing active samples entering a depth with retained samples
  after that depth. This gives future GPU-side compaction work a visible
  baseline for how much inactive path state can be removed before the next
  frontier dispatch.
- The compaction baseline now also reports the largest candidate depth and
  sample count, so future GPU-side compaction work can identify where inactive
  path state is concentrated rather than only seeing whole-render totals.
- The largest compaction candidate now also reports its inactive-sample
  fraction at that depth, distinguishing high absolute sample counts from
  depths that are mostly carrying inactive path state.
- The mixed-query baseline now also reports mixed-depth ray counts and the
  query round trips attached to those depths, and the compaction baseline
  reports candidate samples as a fraction of total active sample-depth work.
  These derived diagnostics make GPU-resident frontier and GPU-side compaction
  opportunities comparable across render sizes before the scheduler changes.
- The compaction baseline now also reports candidate packed-ray byte estimates
  for the whole render and the largest candidate depth. This keeps the Phase 8
  planning signal tied to the actual GPU-intersection ray payload, not only to
  path-state sample counts.
- The compaction baseline now also reports candidate state-handle byte
  estimates for the whole render and the largest candidate depth. This shows
  the remaining per-path CPU state association that scheduler-owned GPU
  compaction would still need to address.
- Frontier compaction results now also report retained-index byte estimates for
  the executed compaction pass. The estimate follows the 32-bit index ABI used
  by the current Metal/Vulkan prepared ray-batch compaction kernels, making the
  future retained-index transfer or residency cost visible beside input,
  retained, removed, moved, and execution-path counters.
- Frontier compaction request/result retained-index storage now uses that same
  32-bit GPU ABI shape directly instead of host-sized indices. Host path counts
  remain `std::size_t`, but retained indices are validated before entering the
  backend compaction contract so future Metal/Vulkan compaction code can consume
  the public contract without another narrowing pass.
- Wavefront metrics now split estimated ray upload bytes by closest-hit and
  any-hit query family, so resident-frontier planning can tell whether path
  frontier intersections or direct-light visibility batches are driving the
  current upload workload.
- Wavefront metrics now also split estimated query transfer bytes by closest-hit
  and any-hit family, giving capture scripts, benchmarks, rendercli summaries,
  and Modeler pass details full per-family host/device traffic totals without
  re-deriving them from separate upload and readback counters.
- Executed frontier compaction now reports moved retained sample fraction, so
  GPU-side compaction planning can distinguish "many inactive paths removed"
  from "most retained paths still had to be copied to new slots."
- Wavefront metrics now report a GPU frontier-compaction unavailable reason
  beside the capability flag. That makes the current boundary explicit:
  prepared platform ray batches can be compacted, but scheduler-owned active
  path state is still host-resident.
- The convergence capture helper now preserves GPU frontier-compaction
  unavailable reasons in reference/candidate comparisons and queue-sweep
  summaries, keeping offline Phase 8 captures aligned with render metrics and
  graph diagnostics.
- Wavefront metrics also report why resident direct-light batches are
  unavailable beside the capability flag. Current platform any-hit frontiers may
  own occlusion rays, but shading still creates and consumes direct-light
  batches on the host.
- The convergence capture helper preserves resident direct-light unavailable
  reasons in reference/candidate comparisons and queue-sweep summaries as well.
- Wavefront intersection backend benchmarks now expose numeric unavailable-gap
  counters for GPU frontier compaction and resident direct-light batches. The
  counters separate scheduler/host-state gaps from missing backend capability,
  so benchmark dashboards can filter Phase 8 blockers without parsing text
  labels.
- Wavefront metrics now report active and retained host path-state bytes per
  depth plus compaction-candidate and largest-candidate host path-state byte
  estimates. That sizes the CPU-owned `BatchPath` frontier state separately
  from packed ray payloads and `State*` handles, making the remaining scheduler
  residency work measurable before the representation changes.
- The convergence capture helper now also preserves the last active and
  retained host path-state byte rows in reference/candidate comparisons and
  queue-sweep summaries, matching the rendercli compact summary fields used to
  inspect the final CPU-owned frontier size.
- Modeler graph-node tooltips and selected-pass graph details now show the same
  active and final retained host path-state byte rows, keeping the GUI aligned
  with rendercli's compact summary for the CPU-owned frontier-size check.
- Path-tracing wavefront metrics now also report spawned-continuation counts
  and host path-state bytes per depth. Exact-delta branches append those states
  after the old frontier is compacted, so this separates frontier growth from
  inactive-path removal when planning resident path state.
- Frontier compaction requests and results now carry optional path-state bytes
  per path. The path tracer fills this with its `BatchPath` size, so the backend
  compaction contract can see the scheduler-owned state payload attached to the
  retained-index list even before that state becomes device-resident.
- Executed frontier compaction metrics now report input, retained, and removed
  host path-state byte counts beside retained-index bytes. That makes the
  scheduler-owned `BatchPath` payload removed by current host compaction visible
  before future GPU-resident path state changes the representation.
- Direct-light any-hit batching metrics now report packed-ray, host-query, and
  state-handle byte totals for next-event-estimation occlusion frontiers. That
  sizes the resident-direct-light Phase 8 gap separately from aggregate any-hit
  frontier payloads.
- Direct-light metrics now also report host-side light-selection byte counts.
  Those selections are the CPU-owned records that connect each any-hit
  occlusion query back to its lighting contribution, so resident direct-light
  planning can see both the ray frontier payload and the remaining host
  selection state.
- Direct-light any-hit batching metrics now also report current direct-light
  round trips and a resident direct-light round-trip savings estimate. This
  makes the host readback boundary for next-event-estimation occlusion visible
  independently of the mixed-depth resident-frontier estimate.
- Direct-light any-hit chunk metrics now also count scalar-loop visibility as
  one chunk per shadow query. Resident direct-light estimates therefore cover
  both grouped any-hit frontiers and non-grouped fallback visibility work.
- Scalar and batched path-tracing direct-light visibility now stage light
  selections and shadow any-hit queries through a dedicated visibility batch
  object. It still executes on the current backend immediately, but
  next-event-estimation occlusion now has an owner that can become resident
  without spreading query vector assembly through the shading loop.
- The direct-light visibility batch now also owns its resolved occlusion bits,
  so scalar and batched shading consume one object instead of parallel
  selection and `vector<bool>` arrays. That tightens the boundary a future
  resident direct-light batch will override.
- Wavefront metrics now also report submitted intersection rays per measured
  intersection-worker second and backend-kernel rays per second when platform
  kernels provide dispatch timing. This gives Phase 7/8 threshold tuning a
  measured throughput signal next to expected-ray, transfer-byte, and
  round-trip estimates.
- Wavefront metrics now derive an observed frontier query round-trip count, a
  resident-frontier round-trip estimate, and an estimated savings count for
  mixed closest-hit plus any-hit depths. This gives the resident-frontier work a
  concrete target before scheduler state is moved onto the device.
- The wavefront/path-tracing textbook backend widget now includes a
  hybrid-boundary view and a resident-frontier target view. The chapter uses
  that control to connect the new mixed-depth, round-trip, throughput, and
  compaction diagnostics to the GPU-resident frontier work they are meant to
  guide.
- Path-tracing batched execution now routes active `BatchPath` ownership,
  host path-state byte sizing, and backend-returned compaction through a
  dedicated host frontier object. Spawned continuation paths are staged
  through the same frontier shape. Existing shading still consumes host path
  state, but the scheduler no longer performs compaction as an ad hoc vector
  edit inside `radianceBatch`.
- The host path frontier now records its own active, retained, and spawned
  host path-state byte metrics. The path-tracing scheduler asks the frontier
  owner to report its footprint instead of recomputing those byte totals in the
  main loop.
- The host path frontier now publishes the active-depth metric row together
  with active host path-state bytes. The depth scheduler keeps only the
  original active count it needs for convergence RMS math, while metric
  publication stays with the frontier owner that sizes host path state.
- Path-tracing closest-hit batch execution now stages query construction,
  pre-depth radiance snapshots, backend frontier ownership, and returned hit
  records through a dedicated closest-hit frontier batch object. This keeps the
  remaining host query-vector assembly out of the main scheduler loop.
- Path-tracing active closest-hit records now flow through a dedicated active-hit
  owner between frontier intersection, direct-light sampling, and BSDF shading.
  The records are still host-resident, but the scheduler no longer passes a raw
  hit vector through every frontier path.
- Active path-hit records now also own compaction-retention lookup for the
  paths they represent. The main path-tracing loop no longer reaches into hit
  internals to retrieve retained path indices.
- Wavefront metrics now report active-hit host bytes per depth and in compact
  rendercli summaries. This sizes the closest-hit records consumed by
  direct-light and BSDF shading separately from larger `BatchPath` state.
- Path-tracing direct-light contributions now flow through a dedicated
  contribution batch between next-event-estimation visibility and BSDF shading,
  instead of returning a raw color vector to the scheduler loop.
- Wavefront metrics now report direct-light contribution host bytes per depth
  and in compact rendercli summaries. This sizes the resolved lighting payload
  that bridges resident direct-light visibility and per-hit shading.
- Modeler graph-node tooltips now summarize active-hit and direct-light
  contribution host bytes alongside the existing frontier payload diagnostics,
  making the remaining Phase 8 host-side shading boundary visible without
  opening the detail table.
- Batched path-tracing shading for an active hit now lives behind a dedicated
  `PathTracingIntegrator` instance method. The depth scheduler still owns
  frontier compaction and spawned continuations, but its loop now expresses the
  Phase 8 boundary as intersect, resolve direct-light contributions, shade
  active hits, then compact retained path state.
- Direct-light visibility batches now store resolved occlusion as explicit byte
  flags instead of `std::vector<bool>` proxies. The backend-facing any-hit
  batch and frontier APIs now return the same byte flag shape, so resident
  direct-light result storage no longer has to adapt proxy containers at the
  scheduler boundary.
- Wavefront metrics now report direct-light occlusion-result host bytes per
  depth and in compact rendercli summaries. Resident direct-light planning can
  now size the visibility result payload separately from light selections,
  contribution colors, any-hit ray frontiers, and per-ray state handles.
- Path-tracing sampled and exact-delta continuations now store throughput,
  background visibility, and emitter-MIS metadata in a dedicated
  continuation-state value on each scalar/batched path record. Scalar and
  batched execution still keep that state on the host, but the scheduler
  boundary no longer has to pass those fields as loose parameters when spawning
  or updating paths.
- Direct-light visibility batches now retain their backend-owned any-hit
  frontier handle while resolving occlusion instead of creating it as a local
  temporary. Current results are still read back immediately, but resident
  direct-light scheduling now has one batch object that owns selections,
  frontier, and resolved visibility.
- Direct-light visibility batches now also own resolved-sample lookup, including
  selection PDF and occlusion lookup. Scalar and batched shading no longer pull
  raw selection records out of the batch, keeping the future resident
  direct-light boundary narrower.
- Direct-light visibility batch metric recording now lives with visibility
  resolution for materialized batches: callers only distinguish empty versus
  non-empty batches, while the batch records its own selection bytes, occlusion
  bytes, any-hit chunks, and frontier residency.
- Direct-light visibility byte-count helpers are now private to the visibility
  batch, so callers cannot bypass the empty/materialized visibility recording
  paths.
- Direct-light visibility batches now also own active-hit light selection and
  shadow-query construction for batched path tracing. The scheduler supplies
  active hits and path state, then the batch builds its own selection/query
  payload before resolving visibility, narrowing the future resident
  direct-light boundary another step.
- Direct-light visibility batches now also own the mapping from resolved
  light-selection records to contribution slots. The contribution batch still
  owns resolved color storage, but selection indices, PDFs, occlusion lookup,
  and per-sample metric recording no longer leak back into the scheduler or
  contribution container.
- Scalar and batched direct-light contribution resolution now route through the
  visibility and contribution batch owners instead of open-coded loops in the
  path-tracing scheduler. Scalar direct lighting uses the same visibility-batch
  selection/resolution path as depth-major batches, and batched contribution
  accumulation lives with the contribution batch that owns the resolved color
  payload.
- Direct-light contribution batches now record their own host-byte footprint
  when created, keeping resolved lighting payload accounting with the object
  that owns that payload.
- Direct-light visibility batches now materialize their own contribution batch
  after visibility resolution. The scheduler no longer creates contribution
  storage, records its host bytes, maps selections into it, or averages it;
  those steps are owned by the visibility boundary that a future resident
  direct-light backend will replace.
- Batched path tracing now asks the direct-light visibility batch to run the
  whole active-hit contribution lifecycle: collect light selections, resolve
  visibility, validate results, and materialize averaged contributions. The
  depth scheduler no longer special-cases empty direct-light batches or owns the
  visibility-to-contribution transition.
- Scalar path tracing now routes its direct-light lifecycle through the same
  visibility batch owner. That keeps scalar and batched next-event estimation on
  the same collect, resolve, validate, and contribution-materialization
  boundary before resident direct-light execution replaces the host object.
- Direct-light visibility resolution now validates that backend any-hit
  frontiers return exactly one occlusion flag per light-selection record. That
  makes malformed resident-backend results fail loudly instead of silently
  misaligning lighting contributions.
- Direct-light resolved-sample lookup now also validates the resolved occlusion
  shape, so future callers cannot accidentally consume an unresolved visibility
  batch as fully unoccluded.
- Closest-hit path frontier resolution now validates that backend frontiers
  return exactly one hit record per submitted path ray. That gives future
  resident-frontier backends the same strict shape contract as direct-light
  visibility batches.
- Whitted closest-hit frontier resolution now enforces the same one-hit-record
  per queued ray contract, keeping Whitted and path-tracing backend-owned
  frontiers aligned for future resident-frontier implementations.
- Whitted closest-hit batch execution now stages query construction, backend
  frontier ownership, result validation, and hit lookup through a dedicated
  queued-ray frontier batch object. The render behavior is unchanged, but the
  Whitted scheduler no longer owns the backend frontier as a raw local vector
  pair.
- Whitted wavefront metrics now report active-hit host bytes for the queued-hit
  records that bridge closest-hit frontier resolution to material shading,
  matching the path-tracing active-hit host-byte diagnostic.
- Whitted active hits now live behind a dedicated `ActiveQueuedHits` owner that
  centralizes reservation, insertion, iteration, and host-byte sizing. The
  scheduler still shades on the host, but it no longer treats the active-hit
  list as an unstructured vector at the depth loop boundary.
- Whitted wavefront metrics now report spawned recursive continuation counts
  and host path-state bytes per depth. That makes Whitted queue growth visible
  in the same diagnostics already used for path-tracing exact-delta branches.
- Whitted queued rays now live behind a `QueuedRayFrontier` owner that
  centralizes path-state reservation, enqueueing, swapping, iteration, and
  host-byte sizing. That matches the path tracer's host-frontier direction and
  leaves fewer ad hoc vector operations in the Whitted depth loop.
- The Whitted queued-ray frontier now records its own active, retained, and
  spawned host path-state byte metrics, matching the path tracer's frontier
  metric ownership.
- Whitted next-depth lifecycle now also lives with the queued-ray frontier. The
  frontier prepares its next-depth storage, reports the retained active sample
  count, records spawned/retained frontier metrics, and performs the depth
  handoff. That keeps Whitted depth finalization aligned with the path-tracing
  frontier owner.
- Path-tracing direct-light selection and occlusion host-byte depth arrays now
  record explicit zero rows for depths with no visibility work. That keeps
  selection, occlusion, and contribution byte diagnostics aligned by depth when
  comparing resident-direct-light opportunities.
- Path-tracing direct-light any-hit chunk and ray depth arrays now record the
  same explicit zero rows for depths with no visibility work, keeping the
  direct-light resident-frontier diagnostics shape-compatible across empty and
  materialized visibility batches.
- Direct-light any-hit frontier packed-ray, host-query, and state-handle byte
  metrics now also have per-depth arrays in addition to whole-render totals.
  This lets resident-direct-light analysis identify which bounce moved the
  occlusion frontier payload.
- The convergence capture helper now preserves the last recorded depth row for
  those direct-light any-hit frontier byte arrays in both reference/candidate
  comparisons and queue-sweep summaries, so offline captures can compare the
  final visibility frontier without expanding the full metrics JSON.
- Path-tracing cancellation after a depth starts now records zero rows for the
  skipped frontier, active-hit, direct-light, spawned-continuation, retained,
  and radiance-delta diagnostics. That keeps cancelled renders comparable with
  completed depth rows when inspecting resident-frontier opportunities.
- Path-tracing depth-frontier metrics now publish through the
  `BatchDepthMetrics` owner instead of a field-by-field list in the depth
  scheduler. Active-hit bytes, frontier hit/miss counts, packet traversal
  counts, closest-hit batch counts, fallback reasons, retained counts, and
  radiance-delta rows therefore stay with the depth object that resident
  frontier implementations will update.
- Wavefront tile metric merging now carries active-hit host bytes and
  direct-light selection, occlusion-result, and contribution host-byte totals
  plus per-depth arrays across every sample batch in a tile. Adaptive or
  multi-pass tile renders therefore expose the same resident-direct-light
  baselines as a single integrator batch.
- `IntegratorBatchMetrics` now owns whole-batch merging through an instance
  method, so tile rendering no longer has a second ad hoc metric-field merge
  list that can drift when new resident-frontier diagnostics are added.
- Direct-light any-hit frontier payload byte totals now merge only through the
  whole-batch metric merge path, not the backend-label merge path. That removes
  a double-count in tiled or adaptive renders.
- Path-tracing sample-color storage now lives behind a dedicated
  `SampleColorBuffer` owner inside `PathTracingIntegrator::radianceBatch`. The
  public batch API still returns `std::vector<Colord>` and progress observers
  still receive a vector view, but active path records no longer point at a raw
  local vector owned directly by the scheduler loop. That narrows another
  host-resident state boundary a future resident path-loop implementation will
  replace.
- Path-tracing closest-hit frontier batches now also own hit/miss
  materialization into the active-hit list after backend result validation. The
  depth scheduler no longer loops over backend hit records directly for the
  batch-frontier path, so closest-hit query construction, result validation, and
  active-hit staging live on the same owner.
- Path-tracing active-hit batches now own the per-depth shade-and-retain
  traversal. The main depth scheduler no longer iterates active hits directly
  to decide compaction retention, and direct-light contribution execution
  metadata is recorded by `PathTracingIntegrator` instead of a file-local
  helper.
- Path-tracing active-hit batches now also build the retained-path compaction
  request while shading, and the host path frontier owns the compact-and-append
  depth finalization step. The depth scheduler still coordinates the phases,
  but retained-index construction and spawned-continuation append bookkeeping
  now live with the frontier owners that a resident path-state implementation
  will replace.

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
  CPU shading loop, CPU/GPU intersection box, and hit records flowing back. ✅
  **Done.** The wavefront/path-tracing textbook backend widget now contrasts
  the current hybrid boundary with the future resident-frontier target.
- Modeler should show backend choice and fallback reason in the render graph
  selected-pass metadata. ✅ **Done.** Selected wavefront passes expose backend
  request, chosen backend, availability, platform, execution path, and fallback
  reason in Modeler graph details and tooltips.
- rendercli should print compact backend diagnostics in metrics summaries. ✅
  **Done.** Wavefront metrics summaries print backend request, selected backend,
  availability, fallback reason, platform, execution path, expected work, and
  transfer/round-trip diagnostics.
