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
> compiled scenes and answer closest/any-hit queries through the compiled CPU
> parity intersector; real Metal/Vulkan kernels remain future phases. This is a
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
2. **Common exact primitives.** Sphere, plane, triangle, rectangle, and disk are
   useful early because many educational scenes use them and they avoid
   raster-style tessellation drift.
3. **Static instances.** Static object transforms are important for imported
   assets. Moving instances and shutter-time transforms can wait until the
   ray/time contract is explicit in GPU payloads.

GPU v1 should reject:

- CSG/boolean composites;
- torus, open cylinder, curve, convex operation, and other exact primitives not
  yet ported;
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
  records.
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
  triangle/mesh-triangle, sphere, plane, rectangle, and disk leaves.
- Static instances are captured as transform payloads through the existing
  transformed-leaf traversal hook.
- Moving instances are rejected before child leaves are flattened, preserving
  the all-or-nothing fallback contract.
- Unsupported leaves are represented explicitly with primitive names, object
  ids, and fallback reasons.
- The first BVH representation now emits a deterministic median-split tree
  with bounded leaf ranges over the compiled primitive records. A real SAH or
  GPU-tuned builder is still outstanding before performance work.

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
  intersector. They still report a CPU fallback because no Metal/Vulkan kernel
  has executed, but they no longer re-enter the runtime `Scene` for supported
  query shapes.
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

- A CPU `CompiledIntersectionSceneIntersector` now traverses the compiled
  flat-array BVH and produces GPU-style closest-hit records for triangle
  payloads, including object/material ids, distance, point, normal, UVs, and
  barycentric coordinates. This is a parity harness for the upcoming
  Metal/Vulkan triangle kernel; it is not selected as a render backend yet.

## Phase 5 - common exact primitives and static instances

Tasks:

- Add GPU kernels/payloads for sphere, plane, rectangle, disk, and static
  instance transforms.
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

## Phase 6 - any-hit / occlusion queries

Tasks:

- Add `intersectAny(...)` GPU query for shadow/visibility rays.
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
  scene-created GPU stubs answer through the compiled-scene parity intersector.
  Batched path-tracing direct-light visibility records any-hit query metrics
  through the selected backend while preserving finite light-distance bounds.
- `CompiledIntersectionSceneIntersector` now has a CPU any-hit parity query for
  supported compiled payloads and static instances. It uses the same bounded
  light-distance rule as `Scene::occludes(...)`, so GPU any-hit kernels have a
  tested visibility contract before they are wired into rendering.

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
