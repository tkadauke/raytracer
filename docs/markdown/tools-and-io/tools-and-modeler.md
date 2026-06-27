# Tools and the Modeler

The library is the renderer. The checked-in front ends are the
small command-line renderer and the Qt modeling UI that turns scene
files into something visible and editable.

By the end of this chapter you should know:

- how `rendercli` loads and renders scene JSON,
- how the `Modeler` executable wires the shared render engines into a GUI,
- where reusable scene JSON files live.

## <a id="rendercli-the-headless-renderer"></a>`rendercli` — the headless renderer
[`tools/rendercli/`](../../../tools/rendercli/) is a command-line front end.
It reads a JSON scene file, builds the scene graph through the
[`world::`](../../../include/world/) wrapper layer, compiles a render graph from
the scene and command-line intent, runs the graph, and writes a PNG to disk.

The typical invocation:

```sh
$ rendercli --engine raytracer --width 1920 --height 1080 \
            scenes/dice.json \
            dice.png
```

The default path is graph-backed. The `--engine` flag sets the graph's preferred
executor (`raytracer` / `pathtracer` / `wavefront` / `raster` / `wireframe`), while a scene-level
`renderIntent` can also choose the executor, structural view mode, and overlay
intent. `pathtracer` names the transport algorithm; by default rendercli runs
it through the wavefront schedule with GPU tracing execution and the GPU sample
stream when the scene or command line has not explicitly selected CPU/hybrid
tracing or a sampler-backed path. `--path_tracing_schedule scalar` asks the
graph compiler to synthesize a raytracer beauty pass with the
`PathTracingIntegrator` instead. `--direct_engine` bypasses the graph and
renders with the selected engine directly; that mode is useful for focused
engine debugging and for low-level knobs that are not yet represented as graph
pass state. Rasterizer
controls such as MSAA, post-process AA, viewport/scissor state, and
color-output state are compiled into typed raster beauty pass state, while
preview shadow-map settings live on the graph shadow pass. Both are serialized
when graph JSON is exported.

The flags cover output size, sampler choice, sample-stream mode,
samples-per-pixel, recursion depth, tonemap operator, and per-engine knobs such
as [LOD](../appendix/a-glossary.md#l), [MSAA](../appendix/a-glossary.md#m),
queue size, and thread count.

For repeatable tracing-backend inspection, build the default release preset
first. On macOS this includes the Metal wavefront/full-GPU path-loop backend by
default; on Linux the default release preset remains CPU-only unless Vulkan is
explicitly enabled:

```sh
$ cmake --preset release
$ cmake --build --preset release --target rendercli
```

Platform backend presets are still available as explicit host-platform
spellings:

```sh
$ cmake --preset release-metal-wavefront
$ cmake --build --preset release-metal-wavefront --target rendercli

$ cmake --preset release-vulkan-wavefront
$ cmake --build --preset release-vulkan-wavefront --target rendercli
```

Metal presets are available on macOS, Vulkan presets are available on Linux,
and unsupported host platforms hide those presets through CMake preset
conditions. The matching benchmark presets are `benchmark-metal-wavefront` and
`benchmark-vulkan-wavefront` when you need backend timing fixtures rather than
a rendercli image.

If the scene has a top-level `animation` block, `--frame N` evaluates the
world scene at frame `N` before the runtime render scene and active camera are
built:

```sh
$ rendercli --engine raster --frame 24 \
            scenes/animation_frame_demo.json \
            frame_0024.png
```

That integer frame evaluation is the **frame-baked** path: tracks for discrete
or render-scene-changing properties are sampled on the editable scene before
runtime objects exist. Eligible continuous tracks are also compiled onto
runtime cameras, instances, lights, and materials. Raytraced renders then
combine the evaluated frame with each ray's shutter sample, so a transform
track can render at `24.0`, `24.5`, or any other subframe time without rebuilding
the world scene. The checked-in
[`scenes/animated_runtime_translation_parity.json`](../../../scenes/animated_runtime_translation_parity.json)
fixture exercises both behaviors: an integer-frame plateau matches the
equivalent static scene, while frame 2 visibly changes when multiple shutter
samples are rendered.

`--animation` renders a scene timeline as an image sequence. The output path
must contain one printf-style integer placeholder:

```sh
$ rendercli --engine raster --animation --frame_start 1 --frame_end 48 \
            scenes/animation_frame_demo.json \
            frames/frame_%04d.png
```

Grouped importer scenes can carry ordered step metadata on `Group` nodes.
`--step N` or `--step single:N` renders only that step's groups plus static
groups, `--step cumulative:N` renders every step through `N`, and
`--step sequence[:FIRST-LAST]` writes one cumulative build-view image per
available step. Sequence output paths use the same printf-style integer
placeholder rule as animation output:

```sh
$ rendercli --step sequence test/fixtures/rendercli/grouped_steps.json \
            steps/step_%02d.png
```

`rendercli` is the right front end for headless rendering, batch rendering,
documentation image generation, and timing runs.

It can also expose the compiled render graph without drawing pixels:

```sh
$ rendercli --render_graph_only --render_graph_format dot \
            scenes/dice.json \
            dice-graph.dot
```

The graph export formats are `text`, `dot`, and `json`. `--render_graph` is
accepted as an explicit spelling of the default graph-backed render path, and
`--render_graph_in plan.json` loads a saved JSON plan instead of compiling one.
If the scene JSON contains a top-level `renderIntent` block, rendercli uses
that as the graph compiler's base intent. Command-line graph options are
layered through the same `RenderGraphRequest` resolver that the Modeler preview
uses, so engine, view, AA, shadow, overlay, camera, shading, and AOV choices
share one interpretation before plan compilation.
General render controls that affect an underlying engine are also translated
into typed intent engine options before compilation. For example,
`--sampler`, `--sample_stream_mode`, `--samples_per_pixel`,
`--sampling_seed`, `--depth`, `--threads`, and explicit `--queue_size` values
become raytracer pass state when the graph contains
`raytrace_beauty` or `wavefront_beauty`. The seed is optional, but when present
it makes stochastic ray-family renders repeatable and is serialized into
exported graph JSON. `--sample_stream_mode gpu_sample_stream` requests the
deterministic GPU-owned sample dimensions used by compiled full-GPU path-loop
renders; sampler-backed modes continue to use the selected `--sampler` and can
force full-GPU requests back to CPU or hybrid execution. When the scene intent
does not specify a ray-family view plane or queue size, rendercli also writes
`TiledViewPlane` and its automatic ray-family queue size into compiled
raytracer or wavefront pass state so graph-backed final renders use the same
tiled pixel walk as the direct
command-line path; scene-authored view-plane and queue intent are left
unchanged. `--pathtracer_russian_roulette_depth N` and
`--pathtracer_direct_light_samples N` become graph-visible path-tracer
execution state, so exported plans preserve both where Russian-roulette
termination begins and how many next-event-estimation light samples are averaged
at each non-delta hit. `--path_tracing_schedule wavefront|scalar` chooses
whether an explicit path-tracer request compiles to the wavefront executor or
to the recursive raytracer executor with the path-tracing integrator installed.
Wavefront controls such as
`--wavefront_convergence`, `--wavefront_no_convergence`,
`--wavefront_convergence_active_fraction`, and
`--wavefront_convergence_rms_delta` become graph-visible convergence state for
depth-major path batches. `--wavefront_adaptive_sampling`,
`--wavefront_no_adaptive_sampling`, `--wavefront_adaptive_min_samples`, and
`--wavefront_adaptive_stddev_threshold` become graph-visible per-pixel
adaptive sampling state for wavefront path tracing.
`--wavefront_intersection_backend auto|cpu|gpu` records the requested
ray-scene intersection backend for wavefront batches. `cpu` uses the canonical
CPU backend. `auto` now runs through the same selection policy used for GPU
backend requests: a fixed expected-ray preflight first, then platform device
availability, platform render-path availability, scene support, and an
expected-ray-count threshold that scales with prepared scene-upload size. At
this stage, scene support means triangle, mesh-triangle, sphere, plane,
rectangle, disk, OpenCylinder, and Torus leaves with either no transform or
static instance transforms that can use the first Metal/Vulkan closest-hit and
any-hit kernels; other packed scenes still select CPU and report that selection
reason in graph trace and wavefront metrics instead of silently behaving like
`cpu`.
Metrics include both `intersectionBackendExpectedRays` and
`intersectionBackendAutoMinimumGpuRays`, so a small auto-selected CPU render can
show the exact ray-count threshold it failed to clear. Auto renders also report
`intersectionBackendAutoEstimatedQueryTransferBytes`, a conservative pre-render
estimate of the packed GPU ray upload plus readback footprint that the
selection policy considered. The expected workload is also split into
`intersectionBackendExpectedClosestHitRays` and
`intersectionBackendExpectedAnyHitRays`, so path-tracing direct-light visibility
work is priced using the smaller occlusion-record readback rather than being
hidden inside one closest-hit-style total.
The same metrics name the scene-upload bytes, platform availability flags,
unsupported-scene reason buckets, selected execution path, fallback reason,
frontier residency/byte counters, and backend upload/kernel/readback timing.
For compiled diffuse path-loop renders, the selected pass details also show the
accumulation backend, residency, resident storage, operation counts, and
readback bytes so the resolve boundary is inspectable alongside path-state and
frontier diagnostics.

Use the same scene and metrics flags when comparing CPU, hybrid automatic, and
GPU-requested wavefront execution. The current hybrid path keeps scheduling,
shading, path state, and accumulation on the CPU while requesting the compiled
intersection backend service for closest-hit and any-hit batches:

```sh
$ rendercli --engine pathtracer --width 640 --height 360 \
            --samples_per_pixel 4 --sampling_seed 17 \
            --wavefront_intersection_backend cpu \
            --wavefront_metrics_summary \
            --wavefront_metrics_out tracing-cpu-metrics.json \
            scenes/tracing_execution_inspection_demo.json tracing-cpu.png

$ rendercli --engine pathtracer --width 640 --height 360 \
            --samples_per_pixel 4 --sampling_seed 17 \
            --wavefront_intersection_backend auto \
            --wavefront_metrics_summary \
            --wavefront_metrics_out tracing-auto-metrics.json \
            scenes/tracing_execution_inspection_demo.json tracing-auto.png

$ rendercli --engine pathtracer --width 640 --height 360 \
            --samples_per_pixel 4 --sampling_seed 17 \
            --wavefront_intersection_backend gpu \
            --wavefront_metrics_summary \
            --wavefront_metrics_out tracing-gpu-request-metrics.json \
            scenes/tracing_execution_inspection_demo.json tracing-gpu-request.png
```

Inspect the compact `wavefront_metrics` stdout line first. The fields
`tracing_backend_platform`,
`intersection_backend_request`, `intersection_backend`,
`intersection_backend_execution`, `closest_hit_execution`,
`any_hit_execution`, `intersection_backend_platform`,
`intersection_backend_gpu_device`,
`intersection_backend_gpu_render_path`, and
`intersection_backend_fallback` show what was requested, what actually ran,
which platform backend was considered, and why the request changed. The JSON
sidecar keeps the same information as
`intersectionBackendRequest`, `intersectionBackend`,
`intersectionBackendExecutionPath`,
`intersectionBackendClosestHitExecutionPath`,
`intersectionBackendAnyHitExecutionPath`, `intersectionBackendPlatform`,
`intersectionBackendPlatformGpuDeviceAvailable`,
`intersectionBackendPlatformGpuRenderPathAvailable`, and
`intersectionBackendFallbackReason`, plus unsupported-scene reason buckets
under `intersectionSceneUnsupportedReasons`. The compact summary also separates
`tracing_backend_fallback_capabilities` from
`tracing_backend_restricted_capabilities`: fallback rows mean a requested GPU
capability resolved somewhere else, while restricted rows mean the engine is
using an explicit limited contract such as a CPU reference implementation for a
GPU-style sampling stream. The Modeler render graph selected-pass details use
the same split, so a pass can show fallback GPU-to-CPU boundaries separately
from restricted CPU-reference contracts such as
`gpu_sample_stream_cpu_reference`.

`gpu` is accepted as durable intent and reports either the active platform path
or a CPU fallback reason in graph trace and wavefront metrics. For a
`gpu` request, the renderer also runs the compiled-intersection-scene diagnostic
before the render starts, so scenes that contain unsupported exact leaves report
the first unsupported primitive as the fallback reason. Supported scenes retain
that compiled record set on the scene-created Metal/Vulkan backend object,
ready for upload-backed work. Metal- and Vulkan-enabled builds prove a tiny
upload/dispatch/readback smoke kernel and can execute triangle, sphere, plane,
rectangle, disk, OpenCylinder, Torus, and static-transform closest-hit and
any-hit queries through packed-ABI platform kernels in the render path. Prepared
scenes outside that basic subset still run through the packed upload buffers via
a CPU traversal with the same hit-record and visibility contract that future
wider kernels must write.
Wavefront metrics and
`--wavefront_metrics_summary` expose the compiled-scene primitive, BVH,
payload, unsupported-leaf counts, basic-kernel, packed closest-hit, and packed any-hit
eligibility, and actual query execution path for those requests. The path is
`metal` or `vulkan` for platform basic kernels, `packed_cpu` for the packed CPU
contract, `compiled_cpu` for compiled parity traversal, and `mixed` when a
render uses different query paths. The
summary also reports estimated ray-upload and readback byte counts for the
packed GPU ABI; CPU and unsupported runtime-scene fallbacks report zero query
transfer bytes because no upload/readback would be attempted for those paths.
It also reports the estimated query round-trip count at the current host/device
boundary, split by closest-hit and any-hit query family, then splits submitted
intersection rays into closest-hit and any-hit ray counts, so a batched shadow
query can report one any-hit query while still showing how many visibility rays
were submitted.
Hybrid visibility and ray-traced raster preview shadows use that same
intersection backend surface, but only for visibility queries. They are not
full GPU path tracing modes. `--render_graph_aov_out
hybrid_visibility=visibility.png` requests a graph AOV that submits primary
closest-hit debug rays through `render::IntersectionService`; `--shadow_mode
ray_traced` requests a raster shadow-mask pass that submits bounded any-hit
shadow rays through the service and composites the mask over raster beauty. In
both cases, the CPU still owns graph scheduling, camera and sample setup,
material and texture evaluation, light sampling, BSDF evaluation, path state,
continuation generation, accumulation, tonemapping, and final image output.
A platform GPU execution path in the trace therefore means the closest-hit or
any-hit query batch ran on Metal/Vulkan, not that the full frame was shaded or
path traced on the GPU. The Modeler render graph details expose the same
service boundary with frontier residency plus packed-ray, retained host-query,
and retained state-handle byte counts for those hybrid passes.

The smallest headless checks are:

```sh
$ rendercli --engine raster --shadow_mode ray_traced \
            --wavefront_intersection_backend gpu \
            --render_graph_trace_out trace.json \
            --wavefront_metrics_summary \
            test/fixtures/rendercli/raster_shadow_caster.json \
            shadow.png

$ rendercli --engine raster \
            --render_graph_aov_out hybrid_visibility=visibility.png \
            --wavefront_intersection_backend gpu \
            --render_graph_trace_out trace.json \
            --wavefront_metrics_summary \
            test/fixtures/rendercli/raster_shadow_caster.json \
            raster.png
```

After either command, inspect `trace.json` or the compact metrics summary. The
summary includes a `wavefront_metrics` row for wavefront beauty passes and an
`intersection_service` row for hybrid visibility and ray-traced shadow graph
passes. The service row reports the pass id, query family, requested and
selected backend, backend availability, platform, closest-hit and any-hit
execution paths, compiled-scene shape and upload bytes, query counts,
unsupported-scene reason buckets, hit/occlusion counts, upload/readback and
transfer-byte estimates, frontier residency, frontier payload byte counters,
and fallback reason. Normal
fallback reasons include CPU policy selection, missing platform device or
render-path kernels, unsupported scene leaves, unsupported closest-hit or
any-hit kernels, platform preparation or dispatch failure, malformed result
counts, and runtime-only material or continuation semantics. Unsupported scenes
fall back to the full runtime CPU path rather than producing a partial GPU
visibility result.
The Modeler graph tooltip for those hybrid passes includes the same compact
query family, backend path, query counts, fallback reason, compiled-scene shape,
and upload-byte summary, while the selected-pass details remain the fuller
inspection surface.
For path-tracing renders, the same diagnostics identify mixed query depths,
where closest-hit frontier batches and direct-light any-hit chunks both ran.
Those counts make the future GPU-resident frontier opportunity visible before
the scheduler actually keeps path state on device. rendercli and the Modeler
also report the mixed-depth ray count and mixed-depth query round trips, which
are the current host/device boundaries a resident frontier path would try to
remove. They also report the observed frontier query round trips, the
resident-frontier round-trip estimate, and the estimated savings if mixed
closest-hit/any-hit depths could be treated as one resident scheduling
boundary. The closest-hit and any-hit frontier residency labels record whether
those backend-owned frontiers were host-backed or came from a future resident
backend path. The paired packed-ray byte counters show the retained
intersection payload, while host-query byte counters show how much original
CPU query-vector storage is still kept after a frontier is prepared. State
handle byte counters show the remaining per-ray `State*` association retained
by packed or platform frontiers after the original query vector is gone.
Compaction candidate byte counters apply the same split to inactive paths so
the graph can show both ray payload and remaining state-handle footprint. Host
path-state byte counters add the larger scheduler-owned queue footprint: the
path tracer's `BatchPath` records or Whitted queued-ray records that the CPU
scheduler still owns directly. The Modeler graph-node tooltip and
selected-pass details show both the total active host path-state bytes and the
last active/retained per-depth rows so the final CPU-owned frontier can be
inspected without opening the JSON trace.
Path-tracing spawned-continuation counters sit beside those host path-state
metrics. They count exact-delta branches that append new path states after the
current frontier is compacted, plus the host `BatchPath` bytes attached to
those spawned states. That separates frontier growth from inactive-path
removal when sizing future resident path-state work.
Backend capability flags separately state whether the selected backend already
supports resident frontiers, prepared ray-batch compaction, scheduler-level GPU
frontier compaction, or resident direct-light batches. Prepared ray-batch
compaction means an already uploaded platform ray buffer can be compacted, but
the current hybrid scheduler still compacts `BatchPath` state on the host. The
trace includes the reason scheduler-level GPU compaction or resident
direct-light batches are unavailable, so the Modeler can show those boundaries
beside the capability flags. The scheduler-level GPU compaction and resident
direct-light flags stay unsupported until a real Phase 8 scheduling path lands,
so the estimates stay visibly separate from implemented behavior.

Common backend fallback reasons are intentionally reported as data, not just as
warnings:

- `intersection_backend_gpu_device=false` means the platform probe did not find
  a usable GPU device for the selected backend. Use a platform preset for the
  host OS, and check the machine's Metal or Vulkan runtime before expecting a
  platform execution path.
- `intersection_backend_gpu_render_path=false` means a GPU device was found,
  but the render-path kernels or required queue/runtime features are not
  available in this build or runtime. The request remains visible, but
  intersection work falls back to a CPU path.
- An `intersection_backend_fallback` value beginning with `auto selected CPU`
  means `auto` rejected GPU setup because the estimated closest-hit and any-hit
  work was too small for the current scene-upload cost. Compare
  `intersection_expected_rays` with `intersection_auto_minimum_gpu_rays`.
- Unsupported scene buckets in `intersection_scene_unsupported_by_reason` mean
  the scene could not be represented by the compiled intersection-service
  subset. Remove or replace the listed runtime-only primitive, material,
  texture, light, transform, or CSG feature when you need platform execution.
- `closest_hit_execution=packed_cpu`, `any_hit_execution=packed_cpu`, or
  `intersection_backend_execution=compiled_cpu` means the scene compiled into
  the GPU-style record layout, but the render ran through CPU parity traversal
  rather than a Metal or Vulkan kernel. This is still useful for ABI and metrics
  validation, but it is not GPU dispatch.
- `intersection_backend_execution=mixed` means closest-hit and any-hit query
  families used different execution paths in the same render. Inspect
  `closest_hit_execution` and `any_hit_execution` before comparing timings.

The path tracer also reports frontier compaction passes: the current CPU
scheduler compacts the surviving path frontier after each depth, and the
metrics expose the input, retained, removed, moved, removed-fraction sample
counts, retained-index bytes, and execution path for that operation. The
retained-index byte count estimates the 32-bit device index buffer a future GPU
compaction kernel would need for the retained frontier. Today that execution
path is `host`; those counts are the execution contract a future GPU compaction
kernel must preserve.
They also report compaction candidate depths and sample counts by comparing
active samples entering a depth with retained samples after that depth. That is
the CPU-side baseline for future GPU-side active-ray compaction; the candidate
fraction keeps that pressure comparable across different image sizes and sample
counts. The same diagnostics convert candidate samples into packed-ray byte
estimates so a future GPU pass can size upload, compaction, and dispatch
buffers from the current render. Host path-state bytes size the CPU scheduler
state tied to those same inactive samples. The largest compaction candidate
depth, sample count, packed-ray bytes, and host path-state bytes point to the
single depth where a future compaction pass would remove the most inactive path
state, and its local fraction distinguishes a large depth from a mostly
inactive one.
Executed compaction also reports input, retained, and removed host path-state
bytes beside retained-index bytes, so the summary separates the index list a GPU
kernel would need from the CPU scheduler state that still has to become
resident. The same executed-compaction row carries upload, kernel, and readback
timing buckets; current host compaction normally reports zero there, while a
future scheduler-level Metal/Vulkan compaction kernel can populate those fields
without changing the inspection surface.
Direct-light any-hit summaries now also include packed-ray, host-query,
state-handle, and resolved occlusion-result byte totals for next-event-estimation
occlusion frontiers. Those fields size the direct-light work that still has to
become resident separately from all other any-hit frontiers.
The same summary reports direct-light any-hit round trips and a resident
direct-light savings estimate, making the host readback boundary visible before
that work can stay on device.
It also reports the number of resident-direct-light candidate depths, total
candidate rays, candidate host payload bytes, and the largest candidate depth
with its ray count, packed-ray bytes, and remaining host bytes. Those values
identify which bounce currently offers the largest resident direct-light
payoff.
When a platform kernel actually runs, the same summary separates backend
upload/setup, kernel dispatch/wait, and readback time; CPU fallback paths keep
those backend buckets at zero while total intersection worker time still
captures their CPU query cost. The summary also reports submitted
intersection rays per measured intersection-worker second, plus backend-kernel
rays per second when the platform backend reports kernel timing, so automatic
backend thresholds can be compared against measured throughput instead of only
byte counts.
Raster
controls such as `--lod`, `--msaa`,
`--msaa_shading`, `--raster_backend`, viewport/scissor, blending, alpha test,
depth bias, shadow-map quality, and `--raster_culling on|auto|off` become
raster pass, shadow-node, or visibility-node state; wireframe `--lod` becomes
wireframe pass state. The compiler emits those parameters while synthesizing
nodes, so exported graph JSON is self-describing and replay does not rely on
hidden rendercli setup. CPU raster remains the default backend; when
`--raster_backend opengl|gpu` is selected explicitly, rendercli starts a
GUI-capable Qt application and defaults to Qt's offscreen platform so the
OpenGL executor can probe or create an offscreen context and render the initial
lit mesh pass when the host supports it.
On macOS, Qt's offscreen platform may not be able to create an OpenGL context
for a command-line process. In an interactive terminal session, developers can
opt into Qt's Cocoa platform for that explicit GPU raster path:

```sh
$ RAYTRACER_ALLOW_RENDERCLI_COCOA_OPENGL=1 \
  rendercli --engine raster --raster_backend opengl \
            scenes/dice.json dice-gpu.png
```

That opt-in is intentionally not the default headless behavior; render farms
and CI jobs should continue to use CPU raster unless they provide a known-good
GUI-capable OpenGL context.
If that intent does not name a default camera, rendercli annotates compiled
scene-rendering passes with the active scene camera id.
Selector-specific scene intent is preserved by scene JSON, but graph
compilation currently rejects it until the compiler can synthesize real
scene-partitioning and composition passes.
When compiling a plan, `--render_graph_executor raytracer|pathtracer|wavefront|rasterizer|wireframe`
overrides the graph intent's default executor, and
`--render_graph_view
default|beauty|wireframe|depth|stencil|stencil_composite|normal|object_id|material_id|world_position|sample_stddev|sample_stddev_color|raster_coverage_count|raster_depth_test_count|raster_depth_pass_count|raster_shade_count|raster_color_write_count`
overrides the graph intent's structural view mode. `--render_graph_camera
camera_id` overrides the intent's default scene-camera reference; current
executors still render with the active runtime camera, but the compiled graph
records the requested camera intent for inspection and future alternate-camera
execution. `--render_graph_shading_profile name` overrides the default named
shading profile, and repeated `--render_graph_shading_parameter key=value`
options attach scalar profile parameters. The compiler records that profile
intent on synthesized scene-rendering passes.
`--render_graph_view_override selector,key=value` adds one high-level view
override to the request. `all,executor=rasterizer,view=depth` is equivalent to
changing the default frame intent; selector-specific values such as
`tag:debug,executor=wireframe,camera=inspection-camera` ask the graph compiler
to split the selected scene subset into a stencil mask, foreground pass, and
composite branch. The
depth, stencil, normal, object-id, material-id, world-position, and raster
counter views
compile graph-visible AOV passes and visualization passes that write the final
color image. When the selected graph executor is the rasterizer, those AOV
passes use rasterizer diagnostic buffers, so they reflect tessellated raster
geometry and raster pass state rather than analytic primary-ray intersections.
The raster counter views are rasterizer-only heatmaps for coverage, depth tests,
depth passes, shaded fragments, and color writes, useful for spotting overdraw
or wasted shading in complex imported models. They use an absolute color scale:
black is zero work, cool colors are low counts, and red marks high repeated
work.
`--render_graph_aov_out depth=depth.png` writes an additional graph AOV preview
image while preserving the main render output; repeat the option for multiple
AOV files such as `stencil=mask.png`, `normal=normal.png`, or
`world_position=positions.png`.
When replaying explicit graph JSON, `--render_graph_color_in`,
`--render_graph_depth_in`, `--render_graph_stencil_in`,
`--render_graph_object_id_in`, and `--render_graph_material_id_in` bind imported
or history resources from image files with `resource=file` syntax before
execution.
`--render_graph_wireframe_overlay` asks the compiler to insert a graph-visible
wireframe overlay pass between the beauty pass and the tonemap pass.
For graph renders, `--post_aa fxaa` and `--post_aa smaa` are also
graph-visible: the shared compiler inserts a postprocess pass after the beauty
pass rather than hiding the filter inside one engine, and the pass's typed
`post_process_aa` parameters select the replayed filter. `--post_aa taa` stays
on the raster beauty pass until temporal history resources are graph resources.
Raster preview shadows compile as a `raster_preview_shadows` node feeding the
beauty pass. Its typed `shadows` parameters carry map size, cascades, bias, and
filtering. The shadow node builds the full directional/cascade collection
through the raster shadow-map builder, publishes a first-cascade depth preview
for inspection, and passes the full artifact to raster beauty. Disabling that
node leaves the raster beauty pass running without graph-controlled shadows.
Replayed graph JSON can also contain `composite/composite` passes tagged
`depth_composite` or `stencil_composite`; rendercli executes those with
graph-visible color, depth, and stencil resources.
Wireframe graph renders carry `--lod` in typed wireframe pass state, so
graph-only JSON exports and replayed graph renders preserve the requested
tessellation density. The Modeler raster render dialog uses the same intent
engine-option path for its raster quality controls before compiling the render
window graph.
`--disable_pass`, `--disable_pass_kind`, `--disable_executor`, and
`--disable_feature` apply graph overrides before validation or rendering.
Those controls are intentionally graph-level: disabling the required
`raytrace_beauty` pass, for example, makes validation fail before any image is
written. Replaying a saved graph uses the exported color resource dimensions,
so `--width` and `--height` only need to be supplied when they intentionally
match the saved plan.

`--render_graph_trace_out trace.json` writes the last executed graph trace as
JSON while rendering an image. The trace includes the executed plan, each pass's
status and elapsed time, the render-input fingerprint, supported input/output
resource preview metadata, cache status metadata, and available
difference-preview metadata. Trace capture is opt-in; ordinary graph renders
skip those diagnostic artifacts, while this flag enables them for the render
that is being exported. OpenGL raster passes also add trace messages for mesh
preparation, prepared vertex/index upload byte counts, OpenGL setup and draw
submission, which attachments were read back to CPU buffers, and how long those
phases took; shadow-enabled OpenGL passes also report whether the graph
shadow-map artifact was used through shader-side texture sampling or a
CPU-prepared visibility fallback. Graph-only mode cannot write a trace because
no graph execution happened.

`--wavefront_metrics_out metrics.json` writes aggregate wavefront metrics for
each render run, and `--wavefront_metrics_summary` prints a compact
`wavefront_metrics` line to stdout. Direct wavefront renders report the active
engine's `lastMetrics()` payload. Graph-backed wavefront renders collect the
same payload from matching graph pass trace metadata, so the report includes
the pass id plus integrator, batch execution mode, active sample counts,
radiance-delta RMS values, retained active sample counts after each depth,
per-depth frontier hit/miss counts, compatibility fallback counts, convergence
thresholds, unsupported path-material counts, stop decisions, denoiser
diagnostics when enabled, convergence
feedback depth counts, tile load-balance counts, requested and resolved
intersection backend names, combined plus closest-hit/any-hit query execution
paths, fallback reason, compiled unsupported-scene reason counts,
backend upload/setup, kernel, and readback timing buckets when a platform
kernel runs,
per-pixel sample radiance
standard-deviation diagnostics, path-tracing emitter-hit counts, sampled
direct-light counts, contribution luminance sums for emitted, direct-light,
ambient, miss/background, and compatibility-shaded radiance, and total render
time. The
simple trace-disabled graph LDR path can now report a platform display resolve
without a companion HDR accumulation readback when the full-GPU path tracer
feeds only the final GPU-resolvable tonemap (currently Linear, Reinhard, or
ACES); denoising, graph trace capture, postprocess consumers, and unsupported
tonemaps still force HDR materialization. The compiled path-loop metadata also
records the capture policy that was requested from the backend: diagnostic
capture, platform accumulation readback, platform display resolve readback, and
the display-resolve tonemap.
The
compact summary prints total `tiles`, `tile_grid`,
`max_tile_width`, `max_tile_height`, `max_tile_pixels`, `avg_tile_pixels`,
`nonempty_tiles`, `min_tile_samples`, `avg_tile_samples`, `max_tile_samples`,
`last_retained_active`,
`frontier_hit_rays`, `frontier_miss_rays`, `frontier_packet_chunks`,
`frontier_packet_rays`, `frontier_closest_hit_batch_chunks`,
`frontier_closest_hit_batch_rays`, `frontier_closest_hit_batch_avg`,
`direct_light_any_hit_batch_chunks`, `direct_light_any_hit_batch_rays`,
`direct_light_any_hit_batch_avg`, the clearer compatibility aliases
`direct_light_any_hit_chunks`, `direct_light_any_hit_chunk_rays`,
`direct_light_any_hit_chunk_avg`, `frontier_ray4_packet_chunks`,
`frontier_ray8_packet_chunks`, `frontier_scalar_rays`, and
`frontier_packet_scalar_fallback_rays`,
`frontier_packet_scalar_fallback_by_reason`, `frontier_packet_refined_rays`,
`frontier_packet_refined_by_material`, `sample_variance_pixels`,
`sample_stddev_rms`, `max_sample_stddev`, `frontier_query_round_trips`,
`frontier_resident_query_round_trips_estimate`,
`frontier_resident_query_round_trip_savings_estimate`,
`active_hit_host_bytes`,
`last_active_hit_host_bytes`,
`closest_hit_ray_upload_bytes`,
`any_hit_ray_upload_bytes`,
`closest_hit_query_transfer_bytes`,
`any_hit_query_transfer_bytes`,
`closest_hit_frontier_residency`, `any_hit_frontier_residency`,
`closest_hit_frontier_packed_ray_bytes`,
`any_hit_frontier_packed_ray_bytes`,
`closest_hit_frontier_host_packed_ray_bytes`,
`any_hit_frontier_host_packed_ray_bytes`,
`closest_hit_frontier_host_query_bytes`,
`any_hit_frontier_host_query_bytes`,
`closest_hit_frontier_state_handle_bytes`,
`any_hit_frontier_state_handle_bytes`,
`resident_frontiers_supported`,
`gpu_frontier_compaction_supported`,
`prepared_ray_batch_compaction_supported`,
`resident_direct_light_batches_supported`,
`resident_direct_light_candidate_depths`,
`resident_direct_light_candidate_rays`,
`resident_direct_light_candidate_host_bytes`,
`resident_largest_direct_light_depth`,
`resident_largest_direct_light_rays`,
`resident_largest_direct_light_packed_ray_bytes`,
`resident_largest_direct_light_host_bytes`,
`direct_light_selection_host_bytes`,
`last_direct_light_selection_host_bytes`,
`direct_light_occlusion_host_bytes`,
`last_direct_light_occlusion_host_bytes`,
`direct_light_contribution_host_bytes`,
`last_direct_light_contribution_host_bytes`,
`direct_light_any_hit_frontier_packed_ray_bytes`,
`direct_light_any_hit_frontier_host_packed_ray_bytes`,
`direct_light_any_hit_frontier_host_query_bytes`,
`direct_light_any_hit_frontier_state_handle_bytes`,
`last_direct_light_any_hit_frontier_packed_ray_bytes`,
`last_direct_light_any_hit_frontier_host_packed_ray_bytes`,
`last_direct_light_any_hit_frontier_host_query_bytes`,
`last_direct_light_any_hit_frontier_state_handle_bytes`,
`frontier_compaction_passes`,
`frontier_compaction_input_samples`,
`frontier_compaction_retained_samples`,
`frontier_compaction_removed_samples`,
`frontier_compaction_removed_fraction`,
`frontier_compaction_moved_samples`,
`frontier_compaction_moved_retained_fraction`,
`frontier_compaction_path_state_residency`,
`frontier_compaction_upload_ms`,
`frontier_compaction_kernel_ms`,
`frontier_compaction_readback_ms`,
`active_host_path_state_bytes`,
`last_active_host_path_state_bytes`,
`last_retained_host_path_state_bytes`,
`spawned_continuations`,
`spawned_continuation_host_path_state_bytes`,
`resident_path_loop_execution`,
`resident_path_loop_residency`,
`resident_path_loop_platform`,
`resident_path_loop_depths`,
`resident_path_loop_active_paths_per_depth`,
`resident_path_loop_input_paths`,
`resident_path_loop_retained_paths`,
`resident_path_loop_removed_paths`,
`resident_path_loop_moved_paths`,
`resident_path_loop_retained_index_bytes`,
`resident_path_loop_resident_path_state_bytes`,
`resident_path_loop_input_resident_path_state_bytes`,
`resident_path_loop_retained_resident_path_state_bytes`,
`resident_path_loop_removed_resident_path_state_bytes`,
`resident_path_loop_compaction_passes`,
`resident_path_loop_round_trips`,
`resident_path_loop_submitted_intersection_rays`,
`resident_path_loop_full_platform_gpu_kernel`,
`resident_path_loop_saved_host_readbacks`,
`resident_path_loop_saved_host_readback_bytes`,
`frontier_compaction_candidate_packed_ray_bytes`,
`frontier_compaction_candidate_state_handle_bytes`,
`frontier_compaction_candidate_host_path_state_bytes`,
`frontier_largest_compaction_candidate_depth`,
`frontier_largest_compaction_candidate_samples`,
`frontier_largest_compaction_candidate_packed_ray_bytes`,
`frontier_largest_compaction_candidate_state_handle_bytes`,
`frontier_largest_compaction_candidate_host_path_state_bytes`,
`frontier_largest_compaction_candidate_fraction`,
`intersection_backend_gpu_device`,
`intersection_backend_gpu_render_path`,
`intersection_backend_platform`, `closest_hit_execution`, `any_hit_execution`,
`closest_hit_batch_preferred`,
`any_hit_batch_preferred`, `intersection_scene_triangles`,
`intersection_scene_spheres`, `intersection_scene_planes`,
`intersection_scene_rectangles`, `intersection_scene_disks`,
`intersection_scene_open_cylinders`, `intersection_scene_tori`,
`intersection_scene_transforms`,
`intersection_scene_unsupported_by_reason`,
`emitter_hit_samples`,
`primary_emitter_hit_samples`, `delta_emitter_hit_samples`,
`bsdf_emitter_hit_samples`, `mis_weighted_emitter_hit_samples`,
`direct_light_samples`, `direct_light_contributing_samples`,
`direct_light_occluded_samples`, `emitted_luminance`,
`direct_light_luminance`, `primary_direct_light_luminance`,
`secondary_direct_light_luminance`, `ambient_luminance`, `miss_luminance`,
`compatibility_shade_luminance`, `unsupported_path_material_samples`, `adaptive`,
`adaptive_min_samples`, `adaptive_stddev_threshold`,
`adaptive_max_samples`, `adaptive_skipped_samples`, and
`adaptive_skipped_fraction`.
Packet chunks can be mixed Ray8/Ray4 chunks, so `frontier_packet_rays` is the
exact packet-lane work count, while the Ray4/Ray8 chunk counters expose packet
fill directly. The compact closest-hit batch and direct-light any-hit chunk
averages divide submitted rays by backend chunks, which makes scalar visibility
chunks and useful frontier-sized batches easy to compare in one line. The JSON
report keeps the
per-depth `frontierRayHitsPerDepth`, `frontierRayMissesPerDepth`,
`frontierPacketChunksPerDepth`, `frontierPacketRaysPerDepth`,
`frontierClosestHitBatchChunksPerDepth`,
`frontierClosestHitBatchRaysPerDepth`,
`directLightAnyHitBatchChunksPerDepth`,
`directLightAnyHitBatchRaysPerDepth`,
`directLightAnyHitFrontierPackedRayBytesPerDepth`,
`directLightAnyHitFrontierHostPackedRayBytesPerDepth`,
`directLightAnyHitFrontierHostQueryBytesPerDepth`,
`directLightAnyHitFrontierStateHandleBytesPerDepth`,
`frontierRay4PacketChunksPerDepth`, `frontierRay8PacketChunksPerDepth`,
`frontierScalarRaysPerDepth`, `frontierPacketScalarFallbackRaysPerDepth`, and
`frontierPacketRefinedRaysPerDepth` arrays for deeper captures, plus
`intersectionBackendPlatform` for the selected platform backend,
`intersectionBackendPlatformGpuDeviceAvailable` for platform GPU device
probing, `intersectionBackendPlatformGpuRenderPathAvailable` for whether that
platform backend can run the render-path intersection kernels,
`intersectionBackendPrefersClosestHitBatch` and
`intersectionBackendPrefersAnyHitBatch` for the backend's observed query-family
batch preferences,
`frontierPacketScalarFallbackRaysByReason` for the base packet-hit fallback
breakdown and
`frontierPacketRefinedRaysByMaterial` for the material-family breakdown.
The JSON also keeps `spawnedContinuationSamplesPerDepth` and
`spawnedContinuationHostPathStateBytesPerDepth`, which show where path-tracing
exact-delta branching or Whitted recursive continuations appended new path
states before the next frontier depth was intersected.
Direct-light any-hit chunk arrays count visibility chunks: grouped backends
record one chunk per submitted any-hit frontier, while scalar-loop backends
record one chunk per shadow query. That keeps the resident-direct-light
round-trip estimate visible even when the selected backend does not prefer a
grouped visibility frontier. Direct-light selection and occlusion host-byte
arrays also keep explicit zero rows for depths where no visibility batch was
materialized, matching the contribution-byte array shape. The direct-light
any-hit chunk and ray arrays follow the same zero-row convention. Cancelled
path-tracing renders keep zero rows for the depth that was started but skipped
before intersection, so per-depth arrays remain comparable in graph details and
compact summaries. The any-hit frontier payload byte arrays expose the same
depth-local view for packed rays, retained host packed-ray staging, retained
host queries, and per-ray state handles while the compact summary keeps the
whole-render totals.
The convergence capture helper also keeps the last recorded depth row from
those payload arrays as
`direct_light_any_hit_frontier_last_packed_ray_bytes`,
`direct_light_any_hit_frontier_last_host_packed_ray_bytes`,
`direct_light_any_hit_frontier_last_host_query_bytes`, and
`direct_light_any_hit_frontier_last_state_handle_bytes`, so queue sweeps can
compare the final visibility frontier without expanding the full JSON arrays.
The same capture summaries preserve `last_active_host_path_state_bytes` and
`last_retained_host_path_state_bytes`, matching the rendercli compact summary
fields that show the final CPU-owned path frontier size before and after
compaction/spawned continuations.
They also preserve resident path-loop execution/residency labels, depth/path
counts, peak and last active path counts by depth, retained-index bytes,
resident path-state byte movement, compaction passes, round trips, and saved
readback estimates. The compact summaries also preserve submitted intersection
ray counts and whether the path-loop ran through a full platform GPU kernel.
That keeps queue sweeps aligned with the compiled CPU-reference path-loop
diagnostics that future platform path-state kernels must replace.
The JSON also includes `sampleVariancePixelArea`, `sampleRadianceStddevRms`, and
`maxSampleRadianceStddev`, which measure disagreement between samples of the
same pixel and complement the between-depth radiance-delta convergence fields.
The emitter-hit counters split visible emitter contributions into primary
camera hits, delta/specular continuation hits, finite BSDF continuation hits,
and the subset that were MIS-weighted against light sampling.
`--wavefront_sample_stddev_out FILE` writes a grayscale image of the same
per-pixel sample radiance standard-deviation data. In graph-backed renders this
requests the `sample_stddev` AOV and exports its preview image; in
`--direct_engine` wavefront or pathtracer renders it writes the engine-side
diagnostic buffer directly. Bright pixels are the noisiest pixels in that render,
normalized against the maximum standard deviation in the captured frame.
`--wavefront_sample_stddev_color_out FILE` writes the companion per-channel
standard-deviation diagnostic. In graph-backed renders this requests
`sample_stddev_color`; in direct wavefront/pathtracer renders it writes the
engine-side color diagnostic buffer. Hue identifies which color channel is
producing the variance, which is useful for diagnosing colored caustic or
emitter outliers.
The
refined counter is Whitted-specific diagnostic work: it counts packet-hit lanes
that still need scalar hit refinement for strict secondary-ray parity. Local
Matte/Phong and built-in reflective, transparent, and portal packet hits can
therefore report zero refined rays because packet lanes preserve the original
double-precision ray for hit materialization. Custom materials keep the
conservative scalar-refinement default and are bucketed separately. The
timing payload also reports summed worker time for sample
generation and integrator batch work, so performance captures can separate
camera/sample setup from scene-intersection and material/shading transport even
when parallel worker time exceeds wall-clock render time. The sample-generation
timing split reports sampler stream creation, camera primary-ray sampling,
sample enqueueing, and residual setup overhead. The integrator timing split also
reports the remaining batch overhead after intersection and shading worker time
are subtracted, making scheduler and frontier bookkeeping cost visible.
Wavefront batches further break that residual down into path setup, frontier
partitioning, frontier bookkeeping, progress snapshot publication, and
convergence-test worker time so tuning captures can see which scheduler phase
is consuming the unexplained cost; Whitted uses the setup bucket for initial
queued-ray and result-buffer construction. `integrator_residual_worker_ms` is
the still-unclassified remainder after those named buckets are subtracted.
Metrics capture is
opt-in; requesting wavefront metrics enables graph trace collection for that
render but does not require writing a separate trace file.
The Modeler Balanced convergence preset uses `active_fraction=0.05` and
`rms_delta=0.002`, matching the benchmark capture default; Preview is looser
and Final waits for inactive paths. In rendercli, `--wavefront_convergence`
without explicit numeric thresholds resolves to the same Balanced defaults in
exported graph JSON. Adaptive sampling renders an initial sample batch for
each pixel, then spends remaining samples only where the per-pixel sample
radiance standard deviation remains above the configured threshold; use
`--wavefront_adaptive_min_samples N` and
`--wavefront_adaptive_stddev_threshold T` to tune that policy.
`--pathtracer_russian_roulette_depth N` controls the bounce depth where
Russian-roulette termination begins for path-traced renders.
`--pathtracer_direct_light_samples N` controls how many direct-light samples are
drawn and averaged at each non-delta path-tracing hit.
`--path_tracing_schedule scalar` runs path tracing through the recursive
raytracer pass; omit it, or pass `wavefront`, to use the graph-visible
wavefront scheduler. Wavefront-only diagnostics such as
`--wavefront_sample_stddev_out` require the wavefront schedule.
`--wavefront_denoiser box|bilateral` requests an opt-in wavefront denoiser.
Box is a small HDR blur intended as the first graph-visible hook. Bilateral is
a color-edge-preserving filter controlled by `--wavefront_denoise_radius N` and
`--wavefront_denoise_color_sigma S`; giving a radius without a denoiser selects
box, while giving a color sigma without a denoiser selects bilateral. `none`
disables an inherited scene denoiser in the compiled intent. Wavefront metrics
record the chosen denoiser, its published parameters, and how much time
filtering took. Feature-aware filters such as the bilateral denoiser can also
request albedo/normal/depth buffers; simpler filters skip that tile-parallel
feature prepass. The compact `--wavefront_metrics_summary` line prints those
same denoiser fields as `denoiser`, `denoise_ms`,
`denoise_feature_prepass_ms`, and `denoise_<parameter>`, plus
`feedback_depths` for convergence decisions that used observer feedback. In
Modeler previews, wavefront depth-progress snapshots also run through the
selected denoiser before the tile is published, so the live image no longer
flips from raw noisy progress directly to a filtered final frame. Before
feature-aware beauty tiles start, the feature prepass is shown as active render
tiles in the normal progress overlay rather than an idle preview, and graph
trace metadata separates that work under `denoise.featurePrepass`. When
convergence is enabled, denoised snapshots can also provide the per-depth RMS
delta used by the scheduler; the filtered color is not written back into path
transport state.

That gives a two-step debugging loop:

```sh
$ rendercli --render_graph_only --render_graph_format json \
            scenes/dice.json \
            dice-graph.json
$ rendercli --render_graph_in dice-graph.json \
            scenes/dice.json \
            dice-from-graph.png
```

## <a id="src-modeler-the-interactive-editor"></a>`src/modeler` — the interactive editor
[`src/modeler/`](../../../src/modeler/) builds the `Modeler` executable. It is
the general scene modeling UI: a scene tree on the left, a property editor on
the right, a material/camera preview dock, a timeline dock when the scene has
animation, and a central live render preview.

Build and launch it from the release preset:

```sh
$ cmake --build --preset release --target Modeler
$ build/release/src/modeler/Modeler
```

The central `Display` widget is a subclass of
[`QtDisplay`](../../../src/widgets/QtDisplay.cpp), which is itself a subclass
of [`RenderWidget`](../../../src/widgets/RenderWidget.cpp). The inheritance
chain reflects the responsibilities:

- `RenderWidget` owns the framebuffer pair and render-job lifecycle.
- `QtDisplay` adds mouse-drag camera control.
- `src/modeler/Display.cpp` adds scene-camera timeline poses, preview intent
  selection, rasterizer-preview shadow toggling, and the Ctrl-click ray-state
  probe.

The editor can swap the live preview intent between Raytracer, Path Tracer,
Wavefront, Rasterizer, and [Wireframe](../appendix/a-glossary.md#w). The preview itself is
graph-backed:
the selected kind becomes the default executor in the compiled render graph,
while the scene and camera stay shared so the preview keeps looking at the same
thing across the swap. Property edits refresh preview geometry and materials
without resetting the live preview camera; opening a different scene still
starts from that scene's saved camera. The Elements dock exposes a generated
`Render Settings` item under the scene; selecting it opens the saved scene
intent in the property editor. Those properties write the scene's top-level
`renderIntent` block, not
normal child geometry. The editor groups the settings by role, uses dropdowns
for enumerated choices such as engine, raster backend, view mode, sampler, and
postprocess AA. Raster settings include an opt-in Visibility Culling selector;
`On` and `Auto` currently synthesize the graph-visible baseline culling node
without changing submitted raster work. Selecting the node after a traced
render shows leaf/triangle counts plus any frustum-rejected baseline counts in
the graph inspector metadata. Wavefront settings include Preview/Balanced/Final
convergence quality presets plus the raw active-sample-fraction and RMS
radiance-delta thresholds for advanced tuning; the wavefront executor uses the
resolved values as the current depth-major path-batch stop policy, reports the
decision in trace metadata, and publishes depth-pass preview updates while a
graph-backed Wavefront pass is still running. The same wavefront section can
enable adaptive sampling, choose its initial sample count and standard-deviation
threshold, request the box or bilateral denoiser, choose its pixel radius, and
set the bilateral color sigma. Path Tracer settings include the
schedule selector, Sample Stream selector, Russian-roulette start depth, and
direct-light sample count. Choosing GPU Sample Stream there removes any saved
sampler override because compiled full-GPU path-loop renders own their sampling
dimensions on the backend side.
Scalar schedule previews publish running sample averages during multi-sample
renders, while wavefront schedule previews publish depth-frontier snapshots and
can use wavefront denoising/adaptive sampling and the selected intersection
backend. The final render
dialog starts from the scene's
saved Render Settings, then acts as a one-off override surface for that render.
When the scene omits sampler or sample-count intent, it keeps Regular as the
Raytracer default, switches Auto/GPU Path Tracer renders to `GPU sample stream`
so eligible scenes can take the compiled full-GPU path-loop route, and switches
explicit CPU Path Tracer renders to Halton and 64 samples per pixel. Ordinary
sampler choices such as Regular, Jittered, and Halton stay sampler-backed and
therefore keep unsupported full-GPU path loops on CPU or hybrid fallback paths.
For Path Tracer final renders using the
wavefront schedule, the same
dialog can leave the scene's saved denoiser intent alone or explicitly override
it to None, Box, or Bilateral with radius and bilateral color-sigma controls.
The dialog's `Capture graph trace` checkbox is off by default; leaving it off
lets eligible GPU Path Tracer renders use the trace-disabled display-only
full-GPU route, while enabling it records per-pass images and metadata for
post-render graph inspection.
It can also request Auto, CPU, or GPU wavefront intersection. GPU records the
request and either uses the active Metal/Vulkan platform path or falls back to
CPU visibly in the compiled graph and trace metadata; if the scene cannot be
represented by the GPU intersection record format yet, the fallback reason names
the unsupported leaf. Supported scenes are compiled into the GPU-ready record
format and retained by the scene-created platform backend so the Metal/Vulkan
upload path can reuse the same preparation step. Metal- and Vulkan-enabled
builds also expose separate smoke kernel test paths outside rendering and can
run the exact-primitive and static-transform basic closest-hit/any-hit subset in
the render path when a platform compute device and render-path kernels are
available. Auto selection distinguishes a detected GPU device from a
render-path-capable backend, so device discovery alone is not treated as enough
to select GPU rendering.
Scenes outside that subset use the supported packed
CPU traversal or the compiled CPU parity intersector; unsupported-scene fallback
keeps the full runtime `Scene` path. The render graph pass tooltip shows the actual query
execution path plus
compiled-scene primitive, BVH, and unsupported-leaf counts, and it marks when
the packed closest-hit or packed any-hit path is eligible for the compiled
scene. Unsupported compiled fallbacks report zero scene-upload bytes because the
backend did not retain packed GPU upload buffers.
The same distinction applies to Modeler's hybrid visibility surfaces. For a
preview, choose `Render -> Preview Engine -> Rasterizer`, enable preview
shadows, and select the ray-traced shadow mode when that control is available.
For an explicit debug image, choose `Render -> Preview View -> Hybrid
visibility`. Selecting the resulting graph pass or resource in the Render Graph
dock shows whether the hybrid pass used a platform backend, packed CPU parity
traversal, or runtime CPU fallback. The pass details report the visibility
query execution path and fallback reason; they should not be read as full GPU
path-tracing capability unless shading, sampling, path-state, and accumulation
capability records also move to GPU-owned paths. In those capability records,
`state.path_state_residency` names where active path records live, while
`state.frontier_compaction` names the compaction operation path; current
wavefront renders still report host-resident scheduler path state.
The final render dialog intentionally keeps its engine list user-facing:
Raytracer, Path Tracer, Rasterizer, and Wireframe. Wavefront path tracing is
selected as the Path Tracer schedule rather than as a second top-level engine,
and scalar path tracing is the other schedule choice. That avoids duplicate
ways to request the same path-traced output while still preserving the
graph-visible executor in the compiled plan.
Engine-specific fields only show for the selected default engine. The same
property editor has a search field for filtering long property sets and
collapsible groups so advanced scene/import settings can stay out of the way.
Internal execution controls such as
view-plane type, worker thread count, and queue size stay hidden in Modeler;
Modeler's own preview/final controls keep using the point-interlaced view
plane and the same automatic ray-family queue policy as rendercli, while
lower-level values can still be authored through scene JSON or rendercli.
`Render -> Preview Engine -> Use Scene Render Settings`
compiles the live preview from that saved intent. Choosing a preview engine,
preview view, overlay, shadows, or preview FXAA/SMAA switches the preview into
an explicit override mode, layering temporary request overrides without
rewriting the scene file. FXAA/SMAA apply to the selected preview executor; the
Raster Backend submenu selects CPU or OpenGL for overridden Rasterizer previews;
rasterizer preview shadows switch the live preview to Rasterizer before
recompiling because the shadow pass is raster-specific. When the scene intent
does not name a default camera, Modeler annotates scene-rendering passes with
the active scene camera id from the editable scene.
`Capture Graph Trace` in the same preview menu is off by default; enable it
when the central Graph Trace tab needs fresh pass/resource snapshots. Leaving it
off lets ordinary Path Tracer previews use trace-disabled GPU display paths
instead of materializing host-side diagnostics.
`Render -> Preview
View` can also switch the live graph preview from beauty to depth, stencil,
normal, object-id, material-id, world-position, or raster counter AOVs; the
graph recompiles to show the corresponding AOV producer and visualization
nodes. Raster preview AOVs are backed by rasterizer diagnostics or a raster
stencil-marking pass, so their images match raster tessellation, sampling, and
clipping. Selecting a raster counter preview switches the preview executor to
Rasterizer because those diagnostics measure raster work. `Render -> Preview
Tonemap` selects the operator used by the graph's tonemap node.

`Render -> Render` opens the final render window. Its Graph tab compiles the
final render plan before the Render button starts execution. The plan starts
from the scene render intent, then applies the render-window controls as
temporary final-render overrides such as engine, resolution, samples, raster
backend, MSAA, and shadow-map quality. The image render executes the graph shown
in that tab, including pass toggles made in the graph inspector. Completed
trace images and trace metadata are captured only when `Capture graph trace` is
enabled for that render; otherwise the graph tab still shows the compiled plan,
but GPU path-tracer renders avoid trace readback and host diagnostic buffers.

The Render Graph dock compiles the current preview intent into a
[`RenderPlan`](../render-graph/render-plans-and-resources.md) before preview
renders begin. The Graph tab is the primary view: it shows pass nodes,
resource nodes, and dependency edges, supports double-click pass toggles, and
drives the property editor when a pass or resource is selected. Pass and
resource nodes use human-readable display names in the graph while stable ids
remain available in tooltips and exports. Pass nodes show non-default scene
selector, camera, and shading-profile intent directly in the graph and elide
long labels inside the node bounds. Its Passes tab shows each compiled pass
display name, execution order, execution stage, kind, executor, scene selector,
camera reference, shading profile, and resource edges with human-readable enum
labels. Its Resources tab shows each declared resource's display name, type,
format, domain, lifetime, and dimensions with the same UI-facing labels.
Selecting a pass also shows its execution stage, order, incoming dependencies,
and outgoing dependencies in the property editor alongside pass state, scene
view, shading profile, resource edges, and trace metadata. Wavefront pass
metadata includes the same intersection backend diagnostics as the graph
tooltip, including average rays per closest-hit batch and direct-light any-hit
chunk.
Hovering a pass or resource node summarizes its scene-view intent and declared
graph edges without leaving the graph view.
Unchecking a pass adds a graph override and the dock validates the
manipulated plan immediately. When the manipulated plan is still valid, the
central preview renders through that effective plan. After a render, selecting
a pass or inspectable color/depth resource in the graph opens the central Graph
Trace preview tab with input, output, and difference images from the last
execution when preview graph trace capture was enabled and that trace still
matches the current plan and preview inputs.
The graph nodes themselves summarize pass status/timing and resource
preview/cache status from that same trace.
When a selected resource has no captured image, the trace preview distinguishes
resources missing from the executed plan from resources that were declared but
not read or written by the last execution path.
The Groups tab applies the same override system to every pass matching a
present pass kind, executor, or feature tag. It presents those labels in
human-readable UI text while keeping the raw graph ids in item metadata for
the saved override values.
Resource selections also show trace cache status in the property editor.
For graph-visible raster preview shadows, that status distinguishes a rebuilt
full shadow-map artifact from one restored from the graph artifact cache.
The dock can also export the effective plan as text, DOT, or JSON for the same
inspection/replay workflows as rendercli.

Scenes with a top-level `animation` block enable the Timeline dock. Its slider
and spinbox choose the current frame. The central preview and render dialog
evaluate a copied scene at that frame before building runtime render objects,
so animated camera poses, transforms, colors, and lights are visible in the
editor. Runtime-continuous tracks are preserved on those built render objects,
so the raytraced preview can still sample transform motion across shutter time
instead of seeing only a single baked integer pose. When the scene has an active
camera, changing frames resets the
central preview to that evaluated camera pose; mouse-drag preview controls can
then move from the keyed pose. The scene tree and property editor remain
attached to the unevaluated authoring scene.

## <a id="scene-files"></a>Scene files
Reusable scene JSON files live under [`scenes/`](../../../scenes/). They are
ordinary world-scene files, so both `rendercli` and `Modeler` load the same
data. The current checked-in scenes cover camera demos, depth of field,
animation frame evaluation, camera panning, light sweeps, material fades,
motion-blur velocity sweeps, runtime-continuous translation compilation,
visibility-step timelines, transparent materials, reflections, raster material
previews, render-graph AOV and stencil-composite demos, wavefront path-tracing
indirect-lighting demos, and small geometry fixtures used by tests.

[`scenes/render_graph_aov_demo.json`](../../../scenes/render_graph_aov_demo.json)
is a focused Modeler graph-inspection scene. Its saved render intent asks for a
rasterizer beauty preview, an SMAA postprocess pass, and a stencil AOV side
branch, so opening the Render Graph dock immediately shows both the main color
chain and an auxiliary resource branch.
[`scenes/render_graph_stencil_composite_demo.json`](../../../scenes/render_graph_stencil_composite_demo.json)
opens with the rasterizer preview and Stencil Composite view selected from its
saved render intent. The compiler synthesizes raster beauty, wireframe beauty,
stencil AOV, composite, tonemap, and exported stencil-preview nodes; the scene
does not name those nodes directly.
[`scenes/wavefront_indirect_environment_demo.json`](../../../scenes/wavefront_indirect_environment_demo.json)
opens with path tracing selected in render intent. It contains no
direct lights and opts into scene environment radiance, so it is a compact
sanity scene for environment/indirect lighting through the graph path.
[`scenes/wavefront_indirect_bounce_demo.json`](../../../scenes/wavefront_indirect_bounce_demo.json)
uses the same graph-backed path-tracing intent for a small
diffuse-bounce setup: a side-lit red wall bounces light onto otherwise-dark
neutral receivers, while a Whitted override misses that bounce.
[`scenes/wavefront_denoise_demo.json`](../../../scenes/wavefront_denoise_demo.json)
keeps the path-tracer setup at low samples per pixel and enables the
bilateral denoiser in scene render intent, making it a compact Modeler sanity
scene for denoiser controls and graph trace metadata.
When a scene's render intent is ahead of the current compiler, Modeler reports
the graph compile error in the Render Graph dock and pauses the live preview
instead of drawing from a stale plan.

The Modeler does not bake scene catalogs into C++; it opens scene JSON files
directly and routes external model formats through registered
`world::SceneImporter` implementations. Its Open dialog builds the default
scene/import filter from registered importer extensions, so new importers become
selectable without hand-editing the dialog. Successful opens and scene saves are
remembered in `File -> Open Recent`, capped to the ten most recent scene/import
files. LDraw `.ldr`, `.dat`, and `.mpd` imports build a new scene shell on a
worker thread, use the importer's default library-root lookup, and frame a
front-facing camera around the compiled model on a white product-view
background. OpenSCAD `.scad` imports use the same standalone-scene defaults when
opened directly: the imported Z-up asset is oriented upright for the
product-view camera, lit with ambient fill, and framed with a pinhole camera
before the preview starts. `File -> Import` is the same importer path used as an
additive scene operation: it inserts the imported root into the current Elements
tree without replacing the scene or changing the current camera, background,
lights, render settings, or timeline. Direct
OpenSCAD opens and imports remain source-backed as `SourceAsset` objects;
Customizer-style sections, comments, numeric values, booleans, string choices,
and vector expressions appear in the property inspector and rebuild the
generated mesh when edited. Scene animation tracks can target the same editable
source parameters, so evaluated frames rebuild the source-backed output with the
sampled Customizer values. The same source asset exposes a normal material
reference, so assigning a scene material to the asset overrides the generated
OpenSCAD mesh without exposing transient importer children in the Elements tree.
New reusable demos should be added as scene files unless they need a new runtime
feature, a new world wrapper type, or a dedicated importer.

## <a id="the-wireup"></a>The wireup
```text
Scene JSON
       |
       |  loaded by world wrappers
       v
world::Scene
       |
       |  optional frame evaluation, then runtime conversion
       v
render::Scene + render::Camera + render::Tonemap
       |
       |  passed to a chosen RenderEngine subclass
       v
engine::raytracer::Raytracer / engine::raster::Rasterizer / engine::wireframe::Wireframe
       |
       |  render(buffer)
       v
Buffer<unsigned int> -> PNG file (rendercli) or QImage paint (Modeler)
```

Every front end is a different way of producing the inputs on the left and
consuming the output on the right. The chain in the middle is what the ray,
scene-structure, and rasterization chapters cover.

## <a id="exercises"></a>Exercises
1. Predict what changes in the middle of the diagram when the user toggles the
   Modeler preview from Raytracer to Wireframe. What persists across the swap?
2. Run `rendercli --engine raster --msaa 4` on a scene with sharp edges, then
   again with `--msaa 1`. Diff the two output PNGs and identify which pixels
   differ.
3. Open `scenes/animation_frame_demo.json` in the Modeler and scrub the
   Timeline dock. Which scene data is edited by the property editor, and which
   scene data is only evaluated for preview?

## See also

- Volume index: [Tools & I/O](README.md)
- Previous: [PLY parsing](ply-parsing.md)
- Engines used:
  [The Whitted pipeline](../ray-rendering/the-whitted-pipeline.md),
  [The rasterization pipeline](../rasterization/the-rasterization-pipeline.md),
  [Wireframe rendering](../rasterization/wireframe-rendering.md)
- [Top-level TOC](../README.md)

## Source anchors

<!-- source-anchors -->
- `tools/rendercli/`
- `test/rendercli/StepOptionTest.cmake`
- `test/rendercli/RenderGraphOptionTest.cmake`
- `src/modeler/MainWindow.cpp`
- `include/engine/graph/RenderGraphRequest.h`
- `src/modeler/`
- `include/widgets/world/RenderGraphInspectorWidget.h`
- `include/widgets/world/RenderGraphTracePreviewWidget.h`
- `src/widgets/world/RenderGraphInspectorWidget.cpp`
- `src/widgets/world/RenderGraphTracePreviewWidget.cpp`
- `scenes/`
<!-- /source-anchors -->
