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
   `--engine wavefront --integrator pathtracer`. Which metrics differ
   even when the image looks statistically equivalent?

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
- `test/unit/render/PathTerminationTest.cpp`
- `test/unit/render/StateTest.cpp`
- `test/unit/engine/wavefront/WavefrontRaytracerTest.cpp`
- `test/rendercli/RaytracerOptionTest.cmake`
<!-- /source-anchors -->
