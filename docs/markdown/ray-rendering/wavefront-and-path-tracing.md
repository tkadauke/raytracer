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

## <a id="execution-backends"></a>Execution backends
Algorithm, schedule, and backend are deliberately separate. The
algorithm decides the image being estimated: Whitted direct lighting,
path-traced global illumination, or a future transport method. The
schedule decides the order of work: one recursive ray at a time, a
scalar iterative path loop, a depth-major wavefront frontier, or a
future GPU queue. The backend decides which resource domain executes a
piece of that scheduled work: runtime CPU traversal, packed CPU
traversal, Metal compute, Vulkan compute, or another device path.

That split gives three useful execution modes:

- **CPU tracing.** The schedule, intersections, material evaluation,
  light sampling, path state, accumulation, and final image resolve all
  stay on the CPU. This is the reference path because it supports the
  full runtime scene and material model.
- **Hybrid GPU-intersection tracing.** The CPU still owns the
  algorithm and schedule. It builds wavefront closest-hit and any-hit
  frontiers, sends supported ray batches to a backend service, reads
  back hit or occlusion records, then continues CPU shading and path
  scheduling. The current Metal and Vulkan work is in this category:
  it accelerates the intersection service for supported packed scenes;
  it is not a full GPU path tracer.
- **Full GPU tracing.** The GPU owns the render loop for a restricted
  algorithm: resident path state, frontier compaction, intersection,
  material/BSDF evaluation, light sampling, accumulation, and resolve.
  CPU code may still choose settings, upload scene records, and read
  back the final image, but it no longer shades every hit or decides
  every continuation between device queries.

The current full-GPU path-loop support is intentionally narrow. Metal-enabled
builds can execute explicit GPU requests for empty scenes and optionally
transformed triangle/sphere/plane/rectangle/disk scenes using Matte or Emissive
materials, ConstantColor textures, and point, directional, or rectangular area
lights. Vulkan currently covers the same path-loop contract only for empty-scene
all-miss execution. Unsupported scenes still report the CPU-reference or hybrid
diagnostic backend instead of pretending that the GPU owns the whole path tracer.

Matching rendered images are therefore meaningful backend evidence
only when the comparison holds the algorithm and schedule contract
steady. A CPU path tracer and a hybrid GPU-intersection path tracer may
use different traversal code, memory layouts, and query batching, but
they are expected to converge to the same image because the CPU
integrator still owns the estimator and consumes the same hit/occlusion
contract. Image parity does not prove that the GPU owns shading,
sampling, or accumulation; it proves that the backend service returned
records equivalent enough for the shared algorithm to make the same
lighting decisions. That is why the source anchors for this chapter
include both the CPU integrator/scheduler code and the intersection
service, packed scene, metrics, and rendercli parity tests.

The comparison below holds the wavefront path-tracing algorithm, sample
stream, seed, dimensions, and supported matte scene fixed while changing only
the requested intersection backend. The `cpu` render is the runtime-scene
reference. The `auto` render shows the policy-selected path for the same work:
CPU when the scene or expected ray count is not worth a GPU handoff, or a
prepared platform path when the build and device make that useful. The `gpu`
request is the hybrid GPU-intersection candidate: the CPU still owns path
state and shading, while supported closest-hit and any-hit frontiers run
through packed CPU, Metal, or Vulkan execution depending on platform support.

| CPU backend request | Automatic backend policy | GPU-intersection request |
|---|---|---|
| ![CPU wavefront path-tracing backend comparison render](../../images/tracing_backend_comparison_cpu.png) | ![Automatic backend wavefront path-tracing comparison render](../../images/tracing_backend_comparison_auto.png) | ![GPU-requested wavefront path-tracing comparison render](../../images/tracing_backend_comparison_gpu_request.png) |

Regenerate the images and their metrics sidecars with:

```bash
rake docs:render[wavefront_path_tracing]
```

The driver writes
`docs/images/tracing_backend_comparison_{cpu,auto,gpu_request}.png` and matching
`*_metrics.json` files. Inspect each `runs[].metrics.batching` object's
`intersectionBackendRequest`,
`intersectionBackend`, `intersectionBackendExecutionPath`,
`intersectionBackendFallbackReason`, and
`intersectionSceneUnsupportedPrimitives` in those metrics files to verify
which execution path actually answered the ray queries for each image.

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

Future GPU sample streams use the same ownership rule. A stochastic
request is identified by `(primarySampleIndex, dimension)`: the
primary sample index is the per-pixel sample number, while `dimension`
comes from `sampleDimensionIndex(name, slot)`. Pixel, time, and lens
own dimensions 0, 1, and 2. Path-tracing slots then repeat in groups
of four: BSDF at `3 + 4*i`, light-surface at `4 + 4*i`,
direct-light selection at `5 + 4*i`, and Russian-roulette
continuation at `6 + 4*i`. Bounce number, direct-light sample number,
and light index are folded into `i`, so changing the scheduler does
not change which sample a path event reads.

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

Some exact materials, such as transparent delta transport, can spawn
more than one continuation from a single hit. Whitted materials can
also enqueue recursive continuation rays. The wavefront metrics
therefore report spawned continuations separately from retained
frontier samples. Retained samples describe the next depth's active
frontier after spawned continuations are appended; spawned counts show
how much continuation branching grew that frontier.

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

<!-- widget: wavefront_intersection_backend -->

The widget's **hybrid now** boundary is the current implementation:
the CPU scheduler submits ray batches to the selected backend, the
backend writes hit or occlusion records, and the CPU integrator shades
and decides what work exists at the next depth. The **resident target**
view sketches why the new mixed-depth, round-trip, throughput, and
compaction metrics exist. They measure where a future implementation
could keep reusable frontier state and active-ray compaction on the
device between query phases, while still treating CPU shading as the
semantic owner until a restricted GPU material path is designed.

The CPU backend is the canonical implementation and supports the full
scene/primitive set. Render intent and rendercli can still request
`auto`, `cpu`, or `gpu` so the graph has a stable place for future GPU
intersection work. `cpu` resolves directly to the CPU backend. `auto`
runs a conservative selection policy. It rejects work below the fixed expected
ray floor before probing platform backends or compiling the scene. Work that
clears that preflight must then have a platform GPU device, a render-path-capable
platform backend, and a compiled intersection scene with no unsupported leaves.
The compiled scene must also be eligible for the platform basic-hit kernel and
for the packed closest-hit and any-hit kernels. Only then can `auto` compare the
effective expected ray count with the transfer threshold. That threshold is the
larger of the fixed floor and the prepared scene upload size in KiB multiplied
by `minimumGpuRaysPerSceneUploadKiB`. Today scene support is intentionally
limited to triangle, mesh-triangle, sphere, plane, rectangle, disk,
OpenCylinder, and Torus leaves with either no transform or static instance
transforms that can use the first Metal/Vulkan packed closest-hit and any-hit
kernels. Other scenes report the CPU-selection reason in metrics and graph trace
metadata. The experimental
CMake flags
`RAYTRACER_ENABLE_METAL_WAVEFRONT` and
`RAYTRACER_ENABLE_VULKAN_WAVEFRONT` enable platform plumbing checks. The
matching presets are `release-metal-wavefront` and
`benchmark-metal-wavefront` on macOS, and `release-vulkan-wavefront` and
`benchmark-vulkan-wavefront` on Linux. The Metal flag already builds a tiny
smoke wrapper that uploads a buffer, dispatches a
deterministic compute kernel, and reads the result back outside the renderer.
Metal and Vulkan can also route eligible exact-primitive and static-instance
closest-hit and any-hit work through kernels that consume the packed BVH,
primitive, payload, transform, and ray buffers and write the same hit-record and
occlusion-record layouts as the CPU packed intersector. A `gpu` request
records durable intent and reports either a platform execution path or the CPU
fallback in wavefront metrics and graph trace metadata.
That makes the backend boundary inspectable before any Metal or Vulkan
kernel is allowed to cover the broader primitive set. The fallback is scene-aware:
before tile work starts, the renderer tries to compile the scene into the
GPU-ready intersection record format. If that diagnostic compiler rejects a
leaf, the reported fallback reason names the first unsupported primitive and,
when multiple leaves fail, includes counts grouped by unsupported reason
instead of only saying that the platform GPU backend is absent. The same reason
counts are structured metrics under `intersectionSceneUnsupportedReasons`, so
rendercli, graph trace metadata, and Modeler pass details can display them
without parsing the fallback sentence.
For a supported scene, the GPU path names the host platform backend that would
run next: Metal on macOS, Vulkan elsewhere. Metal and Vulkan can now execute the
exact-primitive basic subset, including static instance transforms, when their
build flags and platform devices are present. Scenes outside that packed
basic-kernel contract still report CPU fallback. The metrics deliberately split
platform GPU device availability from render-path availability: a platform may
detect a compute device and run smoke kernels, but `auto` still selects CPU
unless the render-path closest-hit and any-hit kernels can also be constructed.
The scene-created backend object retains the compiled records it was prepared
from and answers fallback closest-hit, packet closest-hit, and any-hit queries
through the packed upload-buffer CPU traversal, with the compiled-scene CPU
parity intersector still available for supported payloads that are not yet
packed.
That keeps the ownership boundary the platform-backed intersection path needs:
supported scenes can use the same compiled and packed records for CPU parity
traversal, Metal kernels, or Vulkan kernels, while unsupported scenes fall back
before render work begins. Metrics report whether that diagnostic scene was
compiled plus its BVH node, primitive, payload, and unsupported-leaf counts, so
rendercli and the graph inspector can show the upload workload or fallback
reason. They also report the actual query execution path: runtime `Scene`
traversal, compiled CPU parity traversal, packed-buffer CPU traversal, a Metal
kernel, or a Vulkan kernel. Backend-owned closest-hit and any-hit frontiers
also report their actual residency and packed-ray byte count, so the trace
distinguishes today's host-backed handles from a future device-resident
frontier path. Prepared GPU-style backends already build `packed_host`
frontier handles: the rays are converted to the packed GPU query ABI when the
frontier is created, and the original query vector is discarded after the
packed rays and per-ray state handles are retained. State updates and fallback
still remain CPU-readable. `packed_host` frontiers report their CPU ray-packing
time through the same preparation/upload timing counters that Metal and Vulkan
prepared frontiers use for ray-buffer upload, so frontier creation work is not
lost just because it happens before the intersection dispatch. Whitted
closest-hit batches use the same frontier handle path, so their diagnostics
line up with path-tracing closest-hit batches. Whitted and path-tracing
direct-light visibility also go through backend-owned any-hit frontiers, even
when the selected backend resolves those frontiers with the CPU host fallback.
That keeps closest-hit and any-hit ray counts separated for CPU, packed-host,
and GPU-resident query families. The path tracer can still group all valid
direct-light shadow rays for an active depth frontier into one any-hit backend
frontier, while smaller scalar shading paths use host-resident frontiers with
the same submission shape. Metrics keep per-depth direct-light any-hit chunk
counters, so one visibility frontier with many shadow rays is distinguished
from many visibility frontiers with one or a few shadow rays each.
They also mark depths where closest-hit frontier batches and direct-light
any-hit batches both happened. Those mixed query depths are the baseline for
later GPU-resident frontier scheduling: they show where the current hybrid path
still crosses the host/device boundary for both path continuation and
visibility work in the same depth. The trace also reports how many rays and
query round trips belonged to those mixed depths, which makes the possible
host/device boundary savings visible before the scheduler keeps those frontiers
on the GPU. It also derives an observed frontier round-trip count, a
resident-frontier round-trip estimate, and an estimated savings count by
treating each mixed depth as one future resident scheduling boundary.
Backend-owned frontier diagnostics also separate packed-ray bytes from
host-query bytes: packed bytes describe the intersection payload being handed
to the backend, while host-query bytes describe any original CPU query vector
that still has to be retained for runtime host execution. A third
state-handle byte counter tracks the per-ray `State*` association that
prepared `packed_host` and platform frontiers still keep on the CPU so
intersection results can update the correct path state after readback.
The backend capability flags keep that estimate honest: current hybrid
backends can report lower-level prepared ray-batch compaction when an uploaded
platform ray buffer can be compacted, but they still report that they do not
yet support scheduler-level GPU frontier compaction or resident direct-light
batches. The trace also carries unavailable reasons, so prepared buffer
compaction is not confused with full scheduler-owned frontier compaction, and
platform any-hit frontiers are not confused with direct-light visibility work
that stays resident across shading. Future backends can flip those higher-level
flags only when they actually keep that state on device. Direct-light any-hit
frontier byte totals size the packed-ray, host-query, and state-handle payloads
for next-event-estimation occlusion work specifically, rather than mixing that
pressure into all any-hit frontiers. The matching direct-light round-trip
diagnostics report current any-hit chunk boundaries and the host round trips a
resident direct-light implementation would remove. The compaction
candidate diagnostics apply the same split to inactive paths: packed-ray bytes
estimate the intersection payload that could be compacted, while state-handle
bytes estimate the remaining CPU path-state association tied to those candidate
rays. Host path-state byte counters separately estimate the full scheduler
`BatchPath` footprint that is still owned by the CPU path tracer. That makes the
larger Phase 8 handoff visible: compacting ray buffers is not enough until the
active path state itself has a resident representation or an explicit mirror.
`GpuDiffusePathStateRecord` is the first stable resident representation for the
supported diffuse path-step subset: it carries the active ray, throughput,
accumulated radiance, pixel/sample ids, depth, deterministic sample cursor,
flags, and previous-event MIS metadata in one 16-byte-aligned record. The
current renderer still uses the CPU-owned `BatchPath`; the GPU record exists so
the next path-step kernels have a byte-stable contract to target.
The path tracer also reports frontier compaction metrics for the operation it
already performs between depths: input path slots, retained slots, removed
inactive slots, moved live slots, removed fraction, retained-index bytes, and
the compaction execution path. Retained-index bytes estimate the 32-bit device
index payload a GPU compaction kernel would consume for the retained frontier.
The compaction request/result also carries optional path-state bytes per path;
today the path tracer fills that with `BatchPath` size, which lets the backend
contract see the scheduler-owned payload that remains outside the ray-buffer
compaction primitive.
The same result contract now carries upload, kernel, and readback timing
buckets. Current host compaction normally reports zero for those backend timing
fields, but a future Metal/Vulkan scheduler-level compaction kernel can fill
the same slots without changing rendercli, graph traces, Modeler details, or
capture scripts.
Today that execution path is `host` whenever a compaction pass runs. That makes
the current CPU compaction contract explicit before any future kernel keeps the
frontier resident on the GPU.
The tracing capability records expose the same distinction: `state.frontier_compaction`
names the operation path, while `state.path_state_residency` names where the
active path records live. The compiled diffuse path-loop CPU reference uses the
same capability names: it reports the GPU-facing path-loop operation separately
from the host-resident path-state storage it still interprets on the CPU. Its
capability fallbacks now also name the direct-light, BSDF, continuation, and
accumulation steps that still run through host-owned CPU-reference records, so
the trace distinguishes "no full platform path-loop kernel yet" from the
individual subsystems a future kernel has to take over.
The resident path-state layer mirrors the same contract with `uint32_t`
retained indices, retained/removed/moved path counts, resident path-state byte
totals, and an explicit execution-path label for future Metal/Vulkan kernels.
Executed compaction also reports input, retained, and removed host path-state
bytes. Those counters describe the CPU scheduler payload moved or discarded by
the compaction pass itself, while the retained-index byte counter continues to
describe the compact GPU-style index list.
The same metrics compare active samples entering a depth with retained samples
after that depth. The difference is reported as compaction candidate work: the
samples a future GPU-side compaction pass would remove from the next frontier
instead of carrying as inactive lanes. The candidate fraction reports that
count relative to total active sample-depth work so small and large renders can
be compared directly. The candidate packed-ray byte estimate converts that
sample count into the transfer-sized payload a GPU backend would need to
compact. The host path-state byte estimate converts the same candidate work
into the CPU scheduler memory still tied to those samples. The
largest-candidate depth, sample count, packed-ray bytes, and host path-state
bytes identify where compaction pressure is concentrated, which is more useful
for planning a per-depth GPU compaction pass than only knowing the whole-render
total. The largest-candidate fraction then reports how much of that depth is
inactive, so the diagnostic separates a large frontier from a mostly wasted
frontier.
Executed compaction also reports the fraction of retained paths that had to move
to a new slot. That moved-retained fraction approximates copy pressure for a
future GPU compaction kernel: removing many inactive paths is useful, but moving
nearly every retained path still has a cost.
Prepared GPU-style backends can also opt into arbitrary closest-hit frontier
batches, letting a path-tracing bounce submit one group of camera/path rays
instead of slicing that frontier into Ray4/Ray8 packets before it reaches the
platform backend. The same trace records submitted intersection rays per
intersection-worker second and, when a platform kernel reports dispatch timing,
backend-kernel rays per second. Those throughput numbers are the measured
counterpart to the `auto` policy's expected-ray and transfer estimates.
The transfer estimate is also split into closest-hit and any-hit ray upload
bytes so resident-frontier experiments can see whether camera/path frontiers or
next-event visibility batches dominate the upload pressure. The matching
per-family transfer totals include readback bytes, which makes it easier to
compare full closest-hit versus any-hit host/device traffic without rebuilding
that arithmetic from separate upload and readback counters.

The expected ray count used by `auto` is an estimate of intersection work, not
just the number of primary camera samples. `WavefrontRaytracer` starts with
image size and camera samples per pixel, then asks the active integrator how
many scene-intersection queries one primary sample can generate. Whitted work
scales with recursion depth. Path-tracing work scales with bounce depth and
next-event visibility samples. That estimate is deliberately conservative:
runtime metrics still record the exact closest-hit, any-hit, and submitted-ray
counts after the render finishes. rendercli summaries and Modeler graph
tooltips show the expected-ray estimate next to backend choice, fallback,
execution path, scene-upload bytes, query-transfer bytes, GPU device
availability, and render-path availability so a user can see why `auto` stayed
on CPU or selected a GPU path.

The next boundary is the compiled intersection scene. GPU kernels
cannot consume arbitrary C++ primitive objects, so
`IntersectionSceneCompiler` walks the runtime scene through each
primitive's leaf hook and emits stable records: primitive kind,
material id, object id, transform id, bounds, payload offsets, and
explicit unsupported reasons. The compiled scene currently represents
triangles and mesh triangles, box tessellations, sphere, plane, rectangle,
disk, OpenCylinder, and Torus leaves.
Static instances become transform payloads on those leaves. Unsupported
exact or CSG primitives remain visible as fallback diagnostics instead
of being silently skipped.

The compiled scene also has a CPU parity harness. For closest-hit queries it
traverses the same flat-array BVH and emits the fields the future GPU kernels
must return: object id, material id, distance, point, normal, UV where the
runtime primitive supplies one, and barycentric coordinates for triangles. For
any-hit visibility, it short-circuits on the first supported payload hit inside
the same finite light-distance bound used by `Scene::occludes(...)`. When path
tracing shades a depth frontier, all valid direct-light shadow rays in that
frontier are submitted through a backend-owned any-hit frontier. CPU fallback
frontiers keep the canonical host behavior, while prepared Metal/Vulkan
frontiers can treat direct-light visibility as the same query family without
changing the integrator's visibility boundary. The
harness currently covers triangles, mesh triangles, box tessellations, sphere,
plane, rectangle, disk, OpenCylinder, Torus, and static instance transforms by
tracing in payload-local space and transforming hit data back to world space.
It is not a render backend; it is the executable contract the Metal and Vulkan
kernels need to match before they can be trusted in the wavefront renderer.

The first GPU-facing upload seam is intentionally one step narrower than the
compiled scene. `GpuIntersectionScenePacker` takes the compiled records and
packs flat BVH nodes, primitive records, triangle payloads,
sphere/plane/rectangle/disk/OpenCylinder/Torus payloads, static transform
payloads, ray work items, and miss/hit records. Those structs are
16-byte-aligned, row-major, and plain-layout so Metal and Vulkan can share the
same host-side contract even if their shader source is platform-native. The
current execution eligibility check is strict: platform basic kernels may only
accept triangle, sphere, plane, rectangle, disk, OpenCylinder, and Torus records
with either no transform or a static transform payload. The Torus payload uses
the same quartic intersection contract in packed CPU traversal and in the Metal
and Vulkan basic kernels, so exact Torus scenes no longer have to fall back only
because their primitive is curved.
Prepared GPU fallback backends retain the packed buffers next to the compiled
scene, and the wavefront metrics report
`intersectionSceneUploadBytes`, payload counts such as
`intersectionSceneOpenCylinders` and `intersectionSceneTori`, plus
`intersectionSceneTriangleClosestHitEligible` and
`intersectionSceneBasicHitEligible`, `intersectionScenePackedClosestHitEligible`,
and `intersectionScenePackedAnyHitEligible`
so rendercli and the graph inspector can show the would-be upload workload
before a platform kernel runs.
For eligible exact-primitive and static-instance scenes, closest-hit and packet
closest-hit queries already run through a packed CPU traversal that consumes
those upload buffers and emits the same hit-record layout the GPU kernel will
write. The Metal and Vulkan basic closest-hit and any-hit kernels use that same
upload layout for triangle, sphere, plane, rectangle, disk, OpenCylinder,
Torus, and static-transform scenes in the render path. Prepared Vulkan
backends now keep
their device, pipelines, descriptor layout, command pool, and scene-side buffers
alive for the scene, and reuse growable ray/result/count buffers across
serialized query dispatches. A dispatch still uploads the current rays/counts
and reads back the hit or occlusion records, but it no longer reallocates those
dynamic buffers for every prepared-scene query. Metrics report both the
estimated transfer bytes and the query round-trip count, split by closest-hit
and any-hit query family, which makes the remaining host/device boundary
visible before future GPU-resident frontier work removes some of those
dispatch/readback cycles. The per-frontier metrics also estimate how many
round trips a resident frontier scheduler would keep for mixed closest-hit and
any-hit depths, plus the obvious savings relative to the observed frontier
query batches. Bounded any-hit visibility uses the same packed
traversal contract, including the finite light-distance epsilon used by the
compiled parity intersector. This matches
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
report `metal` or `vulkan` when platform kernels run, `packed_cpu` for the
packed CPU contract, the compiled parity fallback reports `compiled_cpu`, and a
render that combines different query families still reports `mixed`. The
execution label comes from the completed query, so a platform dispatch that
falls back at runtime is labeled with the fallback path instead of the backend's
nominal availability. The same metrics also estimate the query transfer
footprint that a platform GPU backend pays:
ray upload bytes plus closest-hit and any-hit readback bytes. CPU and
unsupported runtime-scene fallbacks report zero for those query-transfer fields;
unsupported compiled-scene fallbacks can still report primitive and unsupported
leaf counts, but report zero scene-upload bytes because no packed upload buffers
are retained. Prepared GPU-request stubs report the packed ABI byte counts so `auto`
selection can compare expected ray work against scene upload and query
upload/readback cost. The policy scales the effective GPU threshold upward for
larger prepared scene uploads, and metrics report the computed
`intersectionBackendAutoMinimumGpuRays` threshold next to
`intersectionBackendExpectedRays`. Workloads below the fixed GPU ray-count floor
are rejected before platform probing or scene compilation, while workloads that
clear that floor can evaluate platform availability and then compile the scene
so upload-size amortization can be evaluated. For `auto`, metrics also include
`intersectionBackendAutoEstimatedQueryTransferBytes`, a conservative pre-render
query-transfer estimate that remains visible even when the selected backend is
CPU. The expected workload is split into
`intersectionBackendExpectedClosestHitRays` and
`intersectionBackendExpectedAnyHitRays`, so the estimate can account for
hit-record and occlusion-record readback separately. Custom integrators can
override those two family estimates and inherit a total estimate derived from
their sum, keeping automatic backend selection aligned with the query split.
The backend selection context owns that same saturated sum, and
`WavefrontRaytracer` uses that context before preparing an automatic backend, so
render metrics and selection use the same closest-hit/any-hit workload even if
an integrator provides an inconsistent legacy total estimate. The auto-selection
policy also reads that effective context estimate, so direct policy callers that
provide query-family counts cannot accidentally select from a stale total.
The decision is inspectable through metrics: `intersectionBackendExpectedRays`,
`intersectionBackendExpectedClosestHitRays`,
`intersectionBackendExpectedAnyHitRays`,
`intersectionBackendAutoMinimumGpuRays`,
`intersectionBackendAutoEstimatedQueryTransferBytes`,
`intersectionSceneUploadBytes`,
`intersectionSceneUnsupportedReasons`,
`intersection_backend_gpu_device`,
`intersection_backend_gpu_render_path`, the selected closest-hit and any-hit
execution paths, fallback reason, frontier residency and byte counters, and
backend upload/setup, kernel, and readback timing. Frontier residency remains
diagnostic for this policy: host-backed frontiers do not disqualify a GPU
intersection query after the platform, scene-support, work, and transfer gates
pass, but the residency and byte metrics name the transfer cost that future
resident-frontier backends must beat. The
wavefront intersection backend benchmarks include runtime CPU and packed
closest-hit rows plus runtime CPU and packed any-hit rows, so both query
families have baseline evidence. The automatic-backend fixtures use the same
context as the renderer and report expected closest-hit rays, expected any-hit
rays, the computed automatic GPU threshold, and the pre-render query-transfer
estimate as benchmark counters. Those rows also report whether a scene was
compiled, how many BVH nodes/primitives/unsupported leaves were prepared, the
unsupported-leaf reason buckets, the prepared-scene upload bytes, and packed
closest-hit/any-hit eligibility. That makes the small-workload preflight visible
as a benchmark row with no prepared scene instead of only as a policy unit test.
They also include a mixed automatic-backend row that submits closest-hit and
any-hit batches after one combined backend selection, matching the shape of a
path-tracing bounce more closely than either query family alone. Platform-enabled
benchmark builds include the same mixed row for explicit GPU requests, so
Metal/Vulkan kernels can be measured against the combined query-family workload
directly. Frontier-compaction benchmark rows run through the backend's
`compactFrontier()` hook and report input, retained, removed, moved, and
GPU-compaction-support counters, so the current CPU compaction path has a
baseline before a device-side compaction kernel exists.
Compiled diffuse path-loop benchmark rows also report peak and final active
path counts from the depth frontier, matching the render metrics and capture
summaries used to judge future resident path-loop kernels.

Tracing backend performance captures use the same workload families in
rendercli form: small supported primitives, a generated large triangle mesh,
visibility-heavy area-light blockers, indirect diffuse bounce, and an
unsupported fallback scene. The capture wrapper runs CPU, automatic, and
explicit GPU-request modes where the scene is supported, then keeps the
transparent-material fallback as an explicit GPU request so the metrics show
why runtime CPU execution was selected. The scene list and command surface live
in
[`docs/perf/tracing-backend-benchmark-scenes-2026-06-15.md`](../../perf/tracing-backend-benchmark-scenes-2026-06-15.md).

The GPU tracing accumulation layout is defined separately from the
intersection-query layout. `TracingAccumulationLayout` describes four
image-shaped planes that future accumulation backends must allocate with the
same width and height: an `rgba32_float` HDR color-sum plane, a `uint32`
sample-count plane, an optional `rgba32_float_second_raw_moment` plane for
variance/adaptive sampling, and a resolved `rgba8_unorm_srgb` display plane.
The resolve plane is deliberately not the accumulator. Backends add linear HDR
sample radiance into the color sum, increment the count, optionally add squared
linear RGB into the moment plane, then resolve by dividing and tonemapping into
the LDR plane. This keeps HDR accumulation and LDR presentation separate while
letting CPU reference operations and platform kernels expose the same resource
diagnostics: residency, per-plane byte counts, clear/add/resolve operation
counts, and readback counts/bytes. Wavefront metrics currently report the
CPU tile-local accumulation mode explicitly, while the standalone Vulkan
accumulation result reports GPU-resident storage and the CPU readbacks used by
its parity tests.
Path-state residency has its own layout contract. `TracingPathStateLayout`
allocates two same-capacity buffers, one for the active frontier and one for
the next frontier being produced by shading. Each entry is a 16-byte aligned
`GpuPathStateRecord` carrying the ray, throughput, accumulated radiance,
sample identity, depth, flags, and continuation timing/PDF fields that a
resident path loop needs to ping-pong between bounces. The CPU reference
`TracingPathStateBuffers` uses that same record layout, so tests can exercise
active-to-next swaps and record round-trips before a platform GPU kernel owns
the buffers.
Direct-light diagnostics also report the host bytes used for sampled light
selection records that pair each occlusion query with its eventual lighting
contribution. Those bytes are separate from the any-hit ray frontier and show
the remaining CPU-side state that resident next-event-estimation batches would
need to eliminate or mirror. The any-hit frontier payload bytes are reported
both as whole-render totals and per-depth arrays, split into packed rays, host
queries, and state handles, so resident-direct-light captures can identify the
specific bounce that moved the occlusion payload.
The resident-direct-light candidate counters collapse those rows into a total
candidate count and the largest candidate depth, so captures can point at the
bounce that would benefit most from keeping light selections, visibility, and
contribution storage resident.
The structured capability list exposes the same distinction as
`lighting.resident_direct_light_batches`: it remains unsupported when the
backend can answer platform any-hit visibility but the scheduler still creates
light selections, visibility resolution, or direct-light contributions on the
host.
They also report the host bytes used by the resolved occlusion flags and by the
direct-light contribution array that carries resolved lighting back to the
per-hit shading pass. Selection and occlusion byte arrays include explicit zero
entries for depths that had no visibility work, so depth-indexed diagnostics
stay comparable with contribution-byte rows. The direct-light any-hit chunk and
ray arrays use the same zero-row convention when no visibility batch was
materialized. If cancellation stops a path-tracing depth after it has been
announced but before intersection starts, the skipped frontier and direct-light
diagnostics likewise publish zero rows for that depth.
Wavefront diagnostics also report active-hit host bytes: the per-depth
closest-hit records that bridge intersection results to Whitted shading or path
tracing's direct-light and BSDF shading passes. Those bytes are separate from
path state and make the closest-hit-to-shading host boundary visible before
resident frontiers own it.
When the platform render-path kernels actually execute, metrics also split
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
- `include/render/IntersectionService.h`
- `src/render/IntersectionService.cpp`
- `include/render/GpuIntersectionScene.h`
- `src/render/GpuIntersectionScene.cpp`
- `include/render/GpuTracingScene.h`
- `src/render/GpuTracingScene.cpp`
- `include/render/MetalWavefrontSmokeKernel.h`
- `src/render/MetalWavefrontSmokeKernel.mm`
- `include/render/VulkanWavefrontSmokeKernel.h`
- `src/render/VulkanWavefrontSmokeKernel.cpp`
- `include/render/WavefrontIntersectionQueryTiming.h`
- `include/render/WavefrontIntersectionBackend.h`
- `src/render/WavefrontIntersectionBackend.cpp`
- `include/render/WavefrontFrontierCompaction.h`
- `src/render/WavefrontFrontierCompaction.cpp`
- `include/render/TracingAccumulationLayout.h`
- `src/render/TracingAccumulationLayout.cpp`
- `include/render/TracingAccumulationReference.h`
- `src/render/TracingAccumulationReference.cpp`
- `include/render/TracingPathStateBuffer.h`
- `src/render/TracingPathStateBuffer.cpp`
- `include/render/VulkanTracingAccumulationKernel.h`
- `src/render/VulkanTracingAccumulationKernel.cpp`
- `include/render/GpuDiffusePathLoopBackend.h`
- `src/render/GpuDiffusePathLoopBackend.cpp`
- `include/render/GpuDiffusePathLoopLaunch.h`
- `src/render/GpuDiffusePathLoopLaunch.cpp`
- `include/render/MetalGpuDiffusePathLoopBackend.h`
- `src/render/MetalGpuDiffusePathLoopBackend.cpp`
- `include/render/MetalGpuDiffusePathLoopKernel.h`
- `src/render/MetalGpuDiffusePathLoopKernel.mm`
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
- `benchmarks/WavefrontIntersectionBackendBenchmark.cpp`
- `benchmarks/tracing_backend_capture.sh`
- `benchmarks/scenes/tracing_backend_benchmark_scenes.json`
- `docs/perf/tracing-backend-benchmark-scenes-2026-06-15.md`
- `scripts/docs/wavefront_path_tracing.rb`
- `scripts/docs/wavefront_intersection_backend.js`
- `scenes/wavefront_indirect_bounce_demo.json`
- `scenes/wavefront_denoise_demo.json`
- `test/unit/render/PathTracingIntegratorTest.cpp`
- `test/unit/render/IntersectionSceneCompilerTest.cpp`
- `test/unit/render/GpuIntersectionSceneTest.cpp`
- `test/unit/render/GpuTracingSceneTest.cpp`
- `test/unit/render/WavefrontIntersectionBackendTest.cpp`
- `test/unit/render/TracingAccumulationLayoutTest.cpp`
- `test/unit/render/TracingAccumulationReferenceTest.cpp`
- `test/unit/render/TracingPathStateBufferTest.cpp`
- `test/unit/render/VulkanTracingAccumulationKernelTest.cpp`
- `test/unit/render/PathTerminationTest.cpp`
- `test/unit/render/StateTest.cpp`
- `test/unit/engine/wavefront/WavefrontRaytracerTest.cpp`
- `test/rendercli/RaytracerOptionTest.cmake`
- `test/rendercli/TracingParityTest.cmake`
<!-- /source-anchors -->
