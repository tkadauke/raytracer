# Wavefront and path tracing

Whitted ray tracing answers a narrow question: what direct light,
mirror reflection, and refraction reach the camera along this ray?
Path tracing asks a broader question: what light reaches the camera
after all possible surface bounces, sampled statistically? Wavefront
rendering is a scheduling strategy for that broader problem, not a
different lighting model by itself.

This chapter connects those terms to the codebase. It explains the
scalar path tracer, the wavefront renderer, and the render graph
surface that lets both show up as inspectable passes.

By the end you should know:

- why path tracing produces indirect color bleeding that Whitted
  recursion does not,
- what `PathTracingIntegrator` owns versus what materials own,
- why `WavefrontRaytracer` is a sibling render engine rather than a
  subclass of the recursive raytracer,
- how sample streams, next-event estimation, Russian roulette, and
  per-depth queues fit together,
- why the intersection backend is separate from the transport
  algorithm and frame scheduler,
- which metrics tell you whether the current path tracer is using
  the path-tracing contract or terminating unsupported materials.

## <a id="three-separate-choices"></a>Three separate choices
The common confusion is to treat *raytracer*, *path tracer*, and
*wavefront* as mutually-exclusive renderer families. In this codebase
they are three different choices:

- **Transport algorithm:** Whitted or path tracing. This decides how
  one ray/path gathers radiance.
- **Frame scheduler:** recursive raytracer or wavefront. This decides
  how many rays/path states are grouped before intersection and
  shading.
- **Render graph executor:** the graph-visible pass that owns the
  selected engine and integrator settings for a render.

The recursive raytracer can run the Whitted integrator. It can also
run the scalar path-tracing integrator. The wavefront engine can run
Whitted-style batch work and path-tracing batch work. The graph
compiler chooses the pass from render settings, scene content, and
overrides; the low-level implementation should not require a user to
hand-author graph nodes to get a path-traced image.

In rendercli, `--engine pathtracer` asks for the path-tracing
transport algorithm. By default that compiles to the wavefront
schedule because it exposes per-depth diagnostics, denoising, and
adaptive sampling. Add `--path_tracing_schedule scalar` when you want
the same path-tracing integrator to run through the recursive
raytracer pass instead. In the Modeler preview and final-render
settings, the same choice is exposed as the Path Tracer schedule.

## <a id="visual-difference"></a>The visual difference
The following three images render the same small scene. The red wall
is directly lit. The floor and sphere are neutral.

| Whitted raytracer | Scalar path tracer | Wavefront path tracer |
|---|---|---|
| ![Whitted direct lighting without indirect red bounce](../../images/wavefront_path_tracing_whitted.png) | ![Scalar path tracer with red indirect bounce and Monte Carlo grain](../../images/wavefront_path_tracing_scalar_pathtracer.png) | ![Wavefront path tracer matching the scalar path tracer transport](../../images/wavefront_path_tracing_wavefront_pathtracer.png) |

The Whitted image is clean because it evaluates a deterministic direct
lighting expression and a small number of explicit secondary rays.
It does not integrate the red wall as a diffuse light source after
the first hit, so the neutral objects stay neutral.

The path-traced images are noisier because they estimate an integral
by sampling continuation directions. They also show the red wall
bleeding into the floor and sphere. That is the important capability:
the red wall is not a light object, but after the point light hits it,
the wall becomes part of the indirect illumination field.

The scalar and wavefront path tracer images should agree
statistically. Individual samples do not need to match pixel for
pixel unless the same sampler stream, seed, engine path, and
termination decisions are pinned. The rendered lesson is that
wavefront changes the order of work, not the transport equation.

## <a id="the-path-tracing-loop"></a>The path-tracing loop
`PathTracingIntegrator` is the teaching implementation. It keeps the
algorithm close to the textbook form:

1. Intersect the current ray.
2. If it misses, add background or explicit environment radiance and
   terminate.
3. If the material is not path-traceable, record a diagnostic and
   terminate.
4. Add emitted radiance when the hit surface is an emitter.
5. Add local ambient compatibility radiance where legacy materials
   expose it.
6. Estimate direct lighting with next-event estimation.
7. Sample or enumerate BSDF continuations.
8. Apply Russian roulette after the configured depth.
9. Continue with the new throughput.

The core state variable is **throughput**. It is the product of the
BRDF/BTDF weights accumulated so far. When a continuation sample
survives, the path multiplies throughput by the material sample value
and divides by the sample PDF when the sample was stochastic. When
Russian roulette keeps a path alive, throughput is also divided by
the continuation probability. That compensation is what keeps the
estimator unbiased even though many low-energy paths terminate early.

The integrator owns recursion. Materials do not call back into
`rayColor()` during path tracing. Instead they expose
`PathMaterialTransport`: emitted radiance, direct BSDF evaluation,
BSDF sampling, exact delta branches, PDFs, and denoising albedo.
That interface is why the path tracer does not need concrete-material
type switches.

## <a id="next-event-estimation"></a>Next-event estimation
A pure path tracer could sample only the BSDF continuation direction
at each hit. That eventually finds lights, but it is noisy when lights
cover a small solid angle. This codebase also performs
**next-event estimation**: at each non-emissive surface hit, the
integrator asks `LightSampler` for one or more candidate lights, draws
a light sample, casts a shadow ray, and evaluates the material's BSDF
toward that light.

When a BSDF-sampled path later hits an emitter, the integrator uses
multiple-importance sampling to combine the BSDF PDF and light PDF.
That keeps direct-light samples and BSDF-emitter hits from double
counting the same contribution. The emitter-hit counters in
`IntegratorBatchMetrics` are there so rendercli and the graph trace
can tell whether those cases actually occurred.

## <a id="sample-streams"></a>Sample streams
Path tracing consumes many independent random dimensions:

- pixel position,
- shutter time,
- lens position,
- light selection,
- light surface samples,
- BSDF direction samples,
- Russian-roulette continuation tests.

The `SampleStream` named-dimension API is what keeps those dimensions
from accidentally reading the same 2D sample pattern. The chapter on
[Sampling and anti-aliasing](sampling-and-anti-aliasing.md) explains
the slots in detail; here the important point is ownership. The
integrator asks for the dimension it needs by semantic name instead
of "the next random number." That makes scalar and wavefront paths
comparable even when they schedule work differently.

## <a id="wavefront-scheduling"></a>Wavefront scheduling
The scalar path tracer can be read as one path at a time: intersect,
shade, sample continuation, repeat. That is simple, but it gives the
intersection code one ray at a time and keeps scheduling hidden
inside a call stack or local loop.

For interactive previews the scalar path tracer publishes the running
sample average for each tile as samples accumulate, so the image
refines across the frame instead of appearing only after the last
sample of each pixel. That display behavior does not change the
estimator; the final pixel is still the arithmetic mean of the same
per-sample radiance values.

The wavefront engine makes the frontier explicit. For each tile and
sample, it stores a path state. At depth 0 it has a frontier of
camera rays. It intersects that frontier, shades all hits, emits
shadow work and continuation work, then compacts the surviving
continuations into the depth-1 frontier. The same process repeats
until every path terminates, convergence stops the batch, or the
depth cap is reached.

<!-- widget: wavefront_path_tracing -->

This explicit frontier gives the engine places to optimize and
inspect:

- packet intersection can trace a small group of active rays through
  the BVH together,
- per-depth metrics can report active samples, hits, misses, stopped
  depth, and packet utilization,
- graph trace metadata can show which pass is executing and what it
  produced,
- adaptive sampling can stop pixels whose sample variance has dropped
  below a threshold.

Those are scheduling benefits. They do not authorize changing the
path-tracing estimator. When the wavefront engine runs the path
tracer, its output should be a scheduling-equivalent version of the
scalar integrator.

## <a id="intersection-backends"></a>Intersection backends
Wavefront scheduling also exposes a second, narrower choice:
which backend answers the scene-intersection query for the current
frontier. The scheduler still owns path state, direct lighting,
material transport, denoising, convergence, and final accumulation.
The intersection backend only receives rays or ray packets and answers
two query shapes: closest-hit records for camera/path-continuation rays,
and any-hit occlusion for direct-light shadow rays.

The CPU backend is the canonical implementation and supports the full
scene/primitive set. Render intent and rendercli can still request
`auto`, `cpu`, or `gpu` so the graph has a stable place for future GPU
intersection work. `cpu` resolves directly to the CPU backend. `auto`
runs a selection policy over platform availability, scene support, expected ray
count, and scene-upload amortization; today scene support is intentionally
limited to triangle, sphere, plane, rectangle, and disk leaves with either no
transform or static instance transforms that can use the first Metal closest-hit
and any-hit kernels, and other scenes report the CPU-selection reason in
metrics and graph trace metadata. The experimental
CMake flags
`RAYTRACER_ENABLE_METAL_WAVEFRONT` and
`RAYTRACER_ENABLE_VULKAN_WAVEFRONT` enable platform plumbing checks. The Metal
flag already builds a tiny smoke wrapper that uploads a buffer, dispatches a
deterministic compute kernel, and reads the result back outside the renderer.
It also routes eligible exact-primitive and static-instance closest-hit and
any-hit work through Metal kernels that consume the packed BVH, primitive,
payload, transform, and ray buffers and write the same hit-record and
occlusion-record layouts as the CPU packed intersector. A `gpu` request
records durable intent and reports either that Metal execution path or the CPU
fallback in wavefront metrics and graph trace metadata.
That makes the backend boundary inspectable before any Metal or Vulkan
kernel is allowed to cover the broader primitive set. The fallback is scene-aware:
before tile work starts, the renderer tries to compile the scene into the
GPU-ready intersection record format. If that diagnostic compiler rejects a
leaf, the reported fallback reason names the first unsupported primitive
instead of only saying that the platform GPU backend is absent.
For a supported scene, the GPU path names the host platform backend that would
run next: Metal on macOS, Vulkan elsewhere. Metal can now execute the
exact-primitive basic subset, including static instance transforms, when the
flag and device are present. Vulkan and scenes outside the basic Metal contract
still report CPU fallback.
The scene-created backend object retains the compiled records it was prepared
from and answers fallback closest-hit, packet closest-hit, and any-hit queries
through the packed upload-buffer CPU traversal, with the compiled-scene CPU
parity intersector still available for supported payloads that are not yet
packed.
That keeps the ownership boundary the future upload-backed backend will need
without changing rendered output yet. Metrics report whether that diagnostic
scene was compiled plus its BVH node, primitive, payload, and unsupported-leaf
counts, so rendercli and the graph inspector can show the would-be upload
workload beside the fallback reason. They also report the actual query
execution path: runtime `Scene` traversal, compiled CPU parity traversal,
packed-buffer CPU traversal, or a Metal kernel. Batched path-tracing
direct-light visibility goes through the same backend seam via
`intersectAny(...)`, so the graph and metrics can separate closest-hit and
any-hit ray counts for CPU and GPU-resident query families. Metrics also keep
per-depth direct-light any-hit batch counters, so one visibility batch with many
shadow rays is distinguishable from many scalar shadow queries.
Prepared GPU-style backends can also opt into arbitrary closest-hit frontier
batches, letting a path-tracing bounce submit one group of camera/path rays
instead of slicing that frontier into Ray4/Ray8 packets before it reaches the
platform backend.

The expected ray count used by `auto` is an estimate of intersection work, not
just the number of primary camera samples. `WavefrontRaytracer` starts with
image size and camera samples per pixel, then asks the active integrator how
many scene-intersection queries one primary sample can generate. Whitted work
scales with recursion depth. Path-tracing work scales with bounce depth and
next-event visibility samples. That estimate is deliberately conservative:
runtime metrics still record the exact closest-hit, any-hit, and submitted-ray
counts after the render finishes. rendercli summaries and Modeler graph
tooltips show the expected-ray estimate next to backend choice, fallback,
execution path, scene-upload bytes, and query-transfer bytes so a user can see
why `auto` stayed on CPU or selected a GPU path.

The next boundary is the compiled intersection scene. GPU kernels
cannot consume arbitrary C++ primitive objects, so
`IntersectionSceneCompiler` walks the runtime scene through each
primitive's leaf hook and emits stable records: primitive kind,
material id, object id, transform id, bounds, payload offsets, and
explicit unsupported reasons. Supported leaves currently include
triangles and mesh triangles, sphere, plane, rectangle, and disk.
Static instances become transform payloads on those leaves. Unsupported
exact or CSG primitives remain visible as fallback diagnostics instead
of being silently skipped.

The compiled scene also has a CPU parity harness. For closest-hit queries it
traverses the same flat-array BVH and emits the fields the future GPU kernels
must return: object id, material id, distance, point, normal, UV where the
runtime primitive supplies one, and barycentric coordinates for triangles. For
any-hit visibility, it short-circuits on the first supported payload hit inside
the same finite light-distance bound used by `Scene::occludes(...)`. When path
tracing requests more than one direct-light sample at a surface, those shadow
rays are submitted through one batched any-hit backend call so prepared
Metal/Vulkan backends can treat direct-light visibility as a group instead of
as unrelated scalar queries. The
harness currently covers triangles, mesh triangles, sphere, plane, rectangle,
disk, and static instance transforms by tracing in payload-local space and
transforming hit data back to world space. It is not a render backend; it is
the executable contract the Metal and Vulkan kernels need to match before they
can be trusted in the wavefront renderer.

The first GPU-facing upload seam is intentionally one step narrower than the
compiled scene. `GpuIntersectionScenePacker` takes the compiled records and
packs only the data the platform kernels can consume directly: flat BVH nodes,
primitive records, triangle payloads, sphere/plane/rectangle/disk payloads,
static transform payloads, ray work items, and miss/hit records. Those structs
are 16-byte-aligned, row-major, and plain-layout so Metal and Vulkan can share
the same host-side contract even if their shader source is platform-native. The
current execution eligibility check is strict: the packed closest-hit kernel may
only accept triangle, sphere, plane, rectangle, and disk records with either no
transform or a static transform payload. Prepared GPU fallback backends retain
the packed buffers next to the compiled scene, and the wavefront metrics report
`intersectionSceneUploadBytes` plus
`intersectionSceneTriangleClosestHitEligible` and
`intersectionSceneBasicHitEligible`, `intersectionScenePackedClosestHitEligible`,
and `intersectionScenePackedAnyHitEligible`
so rendercli and the graph inspector can show the would-be upload workload
before a platform kernel runs.
For eligible exact-primitive and static-instance scenes, closest-hit and packet
closest-hit queries already run through a packed CPU traversal that consumes
those upload buffers and emits the same hit-record layout the GPU kernel will
write. The Metal basic closest-hit and any-hit kernels use that same upload
layout for triangle, sphere, plane, rectangle, disk, and static-transform
scenes in the render path. Bounded any-hit visibility for the remaining
eligible packed scenes uses the same packed traversal contract, including the finite
light-distance epsilon used by the compiled parity intersector. This matches
the current CPU shadow rule:
`Scene::occludes(...)` is geometry-only, so transparent materials still block
shadow rays unless a higher-level material model changes that policy. If alpha,
volumetric, or partial-shadow materials later make visibility
material-dependent, the compiler must make those leaves ineligible for packed
any-hit until a matching visibility kernel exists. That is separate from the
broader compiled scene intersector: the packed path proves the kernel ABI and
traversal contract, while the compiled-scene path remains the fallback for
unsupported packed payloads until platform kernels cover them. Metrics label
those query paths separately: basic-kernel closest-hit and any-hit frontiers
report `metal` when the Metal kernels run, `packed_cpu` for the packed CPU
contract, the compiled parity fallback reports `compiled_cpu`, and a render
that combines different query families still reports `mixed`. The same metrics
also estimate the query transfer footprint that a real GPU backend would pay:
ray upload bytes plus closest-hit and any-hit readback bytes. CPU and
unsupported runtime-scene fallbacks report zero for those query-transfer fields;
prepared GPU-request stubs report the packed ABI byte counts so `auto`
selection can compare expected ray work against scene upload and query
upload/readback cost. The policy scales the effective GPU threshold upward for
larger prepared scene uploads. When the Metal
render-path kernels actually execute, metrics also split
backend wall time into host upload/setup, kernel dispatch/wait, and CPU
readback buckets. CPU fallback paths leave those buckets at zero, while the
broader intersection-worker timer still records the full CPU query cost. Today
the policy selects CPU unless a platform kernel is available and worth using;
once Metal/Vulkan coverage is broader, the same policy can require a fully
supported packed scene and enough expected ray work before choosing the GPU
path automatically.

## <a id="diagnostics"></a>Diagnostics and current limits
The path tracer is still intentionally conservative about material
coverage. If a material has not implemented `PathMaterialTransport`,
the path tracer terminates the path and records
`unsupportedPathMaterialSamples`. It does not call the material's
Whitted `shade()` fallback, because that would hide a recursive
Whitted estimator inside a path-tracing render and make the image
hard to reason about.

The wavefront metrics summary is the fastest way to inspect this from
rendercli:

```bash
build/release/tools/rendercli/rendercli \
  --direct_engine --engine wavefront --integrator pathtracer \
  --wavefront_metrics_summary \
  scenes/wavefront_indirect_bounce_demo.json /tmp/path.png
```

For the Modeler, the same counters travel through graph trace
metadata. Selecting the graph pass lets the property/trace views show
whether the render used compatibility paths, emitter hits, denoising,
adaptive sampling, or unsupported material termination.

## <a id="where-to-read-next"></a>Where to read the source
Start with the scalar implementation before the wavefront engine.
`PathTracingIntegrator` is where the estimator is easiest to see.
Then read `WavefrontRaytracer` and `WavefrontTileRenderer` to see how
that same work is batched over tiles and depth frontiers. Finally,
read the render graph pass state and trace code to see how those
choices become inspectable user-facing metadata.

## <a id="exercises"></a>Exercises
1. Render the comparison scene at 16, 64, and 256 samples per pixel.
   Which regions converge fastest? Which stay noisy?
2. Disable direct-light sampling in a local experiment and rely only
   on BSDF continuation. How much noisier is the first visible light
   contribution?
3. Add a material that returns `supportsPathTracing() == false`.
   Confirm that the image loses that path contribution and the
   unsupported-material counter increases.
4. Render the same scene with `--engine pathtracer` and with
   `--engine pathtracer --path_tracing_schedule scalar`. Which metrics
   differ even when the image looks statistically equivalent?

## See also

- Volume index: [Ray rendering](README.md)
- Previous: [Sampling and anti-aliasing](sampling-and-anti-aliasing.md)
- Next: [Textures](textures.md)
- Material transport contracts:
  [Materials and BRDFs](materials-and-brdfs.md#the-bsdf-interface)
- Light PDFs and MIS:
  [Lights and shading](lights-and-shading.md)
- Render graph inspection:
  [Render plans and resources](../render-graph/render-plans-and-resources.md)
- Implementation plan:
  [`docs/plans/wavefront-and-path-tracing.md`](../../plans/wavefront-and-path-tracing.md)

## Source anchors

<!-- source-anchors -->
- `include/engine/wavefront/WavefrontRaytracer.h`
- `src/engine/wavefront/WavefrontRaytracer.cpp`
- `src/engine/wavefront/WavefrontTileRenderer.cpp`
- `include/render/Integrator.h`
- `src/render/Integrator.cpp`
- `include/render/IntersectionSceneCompiler.h`
- `src/render/IntersectionSceneCompiler.cpp`
- `include/render/GpuIntersectionScene.h`
- `src/render/GpuIntersectionScene.cpp`
- `include/render/MetalWavefrontSmokeKernel.h`
- `src/render/MetalWavefrontSmokeKernel.mm`
- `include/render/WavefrontIntersectionQueryTiming.h`
- `include/render/WavefrontIntersectionBackend.h`
- `src/render/WavefrontIntersectionBackend.cpp`
- `include/render/PathTracingIntegrator.h`
- `src/render/PathTracingIntegrator.cpp`
- `include/render/PathTermination.h`
- `include/render/State.h`
- `include/render/materials/Material.h`
- `include/render/lights/LightSampler.h`
- `include/render/samplers/SampleStream.h`
- `include/engine/graph/RenderPassState.h`
- `include/engine/graph/RenderGraphExecutionTrace.h`
- `src/widgets/world/RenderGraphInspectorWidget.cpp`
- `tools/rendercli/rendercli.cpp`
- `scenes/wavefront_indirect_bounce_demo.json`
- `scenes/wavefront_denoise_demo.json`
- `test/unit/render/PathTracingIntegratorTest.cpp`
- `test/unit/render/IntersectionSceneCompilerTest.cpp`
- `test/unit/render/GpuIntersectionSceneTest.cpp`
- `test/unit/render/WavefrontIntersectionBackendTest.cpp`
- `test/unit/render/PathTerminationTest.cpp`
- `test/unit/render/StateTest.cpp`
- `test/unit/engine/wavefront/WavefrontRaytracerTest.cpp`
- `test/rendercli/RaytracerOptionTest.cmake`
<!-- /source-anchors -->
