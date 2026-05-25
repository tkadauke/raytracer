# Topics backlog — `tkadauke/raytracer`

> **Companion doc** to [`roadmap.md`](roadmap.md). The roadmap captures the planned scope; this document captures the wider field — algorithms and disciplines worth implementing and documenting that don't (yet) have their own pillar in the roadmap. Items here graduate to the roadmap when they get picked up.
>
> The §4.0 documentation invariant from the roadmap applies in full: anything from this list that lands ships with a topical doc page and (where applicable) an interactive WebGL/SVG diagram.
>
> **Status:** Living document. Pull requests that touch any of these items either move them up into the roadmap proper or check them off here.

---

## A. Sampling & Monte Carlo theory

Currently scattered across §4.1 path-tracer mentions of MIS / stratified / Sobol. The discipline merits its own pillar once enough of the foundation lands.

- **Low-discrepancy sequences.** Halton, Sobol, Niederreiter, Faure. Owen scrambling and randomized QMC — ship multiple variants for the side-by-side comparison.
- **Blue-noise sampling.** Spatial blue-noise distributions (void-and-cluster, dart throwing), Heitz-Belcour optimization, blue-noise mask textures for animation-stable seeds.
- **Stratification.** Latin hypercube, jittered, multi-jittered, correlated multi-jittered (CMJ).
- **Russian roulette and splitting.** Path termination heuristics; efficiency-optimized RR; weight windows.
- **Path guiding.** Müller's "Practical Path Guiding" (PPG) with on-line-learned spatial mixture models. Neural path guiding (Müller 2019). Vorba et al. spatial directional mixtures.
- **ReSTIR family.** Reservoir spatiotemporal importance resampling for direct (ReSTIR DI), global (ReSTIR GI), and full PT (ReSTIR PT). Biggest sampling breakthrough of the last several years; CPU-implementable.
- **Manifold sampling.** Manifold next-event estimation (MNEE) for caustics; manifold exploration MLT.
- **Vertex connection and merging (VCM / UPS).** Hybrid PT/photon mapping with proper MIS weighting.
- **Progressive photon mapping variants.** SPPM, PPM, and vertex merging as historically important caustic baselines.
- **Many-light sampling.** Lightcuts, light trees, alias tables, RIS, ReGIR, RTXDI, and reservoir-based direct-light sampling.

## B. Color science

Tone-mapping is in roadmap §4.9.a; the surrounding theory is missing.

- **Color spaces and gamut transforms.** sRGB, Linear, Rec.709, Rec.2020, ACEScg, ACES2065-1, DCI-P3, Display P3, ProPhoto. Round-trip matrices.
- **Chromatic adaptation.** Bradford, CAT02, von Kries; D50/D65 illuminant handling.
- **CIE color matching.** 1931 / 1976 / 2006 standard observers. XYZ, LAB, LUV, JzAzBz.
- **Color appearance models.** CIECAM02, CAM16. Practical use in tone mapping and gamut compression.
- **Spectral upsampling.** Smits 1999, Mallett & Yuksel, Jakob & Hanika 2019. RGB → reflectance/emission spectra preserving hue.
- **Spectral rendering plumbing.** Wavelength sampling strategies (uniform, hero wavelength, importance-sampled). Polarization-aware spectral (Stokes/Mueller).
- **Gamut mapping.** Soft-clipping vs hard-clipping; perceptual gamut compression (Olano et al.); ACES gamut compression.
- **HDR display.** HDR10, HDR10+, Dolby Vision; PQ vs HLG transfer; metadata handling.
- **Display calibration.** ICC profiles, monitor profiling.

## C. Global illumination beyond pure path tracing

Path tracing is in roadmap §4.1; the broader GI catalog is mostly absent.

- **Radiosity.** Form factors (analytic, hemicube, ray-traced). Hierarchical radiosity (Hanrahan et al. 1991). Progressive shooting. Pure-Lambertian baseline; classical and pedagogically essential.
- **Voxel cone tracing (VXGI).** Voxelize the scene, cone-trace from each shaded pixel.
- **Lumen-style hybrid GI.** Screen traces + signed-distance-field / mesh-card traces + radiance caches; valuable as a modern real-time GI case study even if not a direct implementation target.
- **Neural / probe radiance caches.** Neural radiance cache variants, surfel/probe caches, and temporal cache update policies.
- **Light propagation volumes (LPV).** Spherical-harmonic per-cell radiance grids; iterative propagation.
- **Screen-space GI (SSGI).** SSDO, SSIL, GTAO-as-GI variants.
- **Pre-computed radiance transfer (PRT).** Spherical-harmonic transfer matrices for static geometry; live for dynamic relighting.
- **Reflective shadow maps (RSM).** Sparse VPL sampling from a shadow map.
- **Irradiance probes / volumetric lightmaps.** SH probes on a grid; trilinear-interpolated lighting. Dynamic Diffuse Global Illumination (DDGI) as the modern probe-based variant.

## D. Shadow algorithms

Implicit in the raytracer/PT today; the rasterized-shadow catalog is absent.

- **Shadow mapping family.** Basic projection, PCF (percentage-closer filtering), PCSS (percentage-closer soft shadows), Variance Shadow Maps (VSM), Exponential Shadow Maps (ESM), Moment Shadow Maps (MSM).
- **Cascaded Shadow Maps (CSM).** Frustum-fitted cascade selection; stable cascade math; light-space cascade biasing.
- **Shadow volumes.** Stencil-based hard shadows; "Carmack's reverse" depth-fail.
- **Ray-traced shadow denoising.** Spatiotemporal blur, A-SVGF for soft shadow reconstruction.
- **Virtual shadow maps.** Page/tile-based high-resolution shadow maps for large scenes; useful alongside Nanite-style virtualized geometry.
- **Hybrid.** Irregular Z-buffer, deep shadow maps for hair/volumetric, dual-paraboloid/cubemap shadows for omnis.

## E. Differentiable & inverse rendering

Currently absent; arguably the most active area in graphics research.

- **Differentiable rendering frameworks.** Mitsuba 3 / Dr.Jit-style automatic differentiation through the rendering integral. Path-replay backpropagation. Reparameterized integration for visibility discontinuities.
- **Inverse rendering.** Solve for material parameters, lighting, geometry from images by gradient descent on a forward render.
- **Material acquisition.** Recover BRDFs from photographic measurements via differentiable rendering.
- **Geometry from images.** Differentiable mesh reconstruction; SDF optimization; the bridge to NeRF / 3DGS.
- **Neural BRDF representations.** MLP-encoded BRDFs with end-to-end fitting.

## F. Photogrammetry pipeline

Sits next to NeRF / 3DGS but is its own discipline.

- **Camera calibration.** Intrinsic / extrinsic; lens distortion (Brown-Conrady, OpenCV model).
- **Structure from motion.** Feature detection (SIFT, SURF, ORB, R2D2), matching, RANSAC pose recovery, incremental vs global SfM.
- **Bundle adjustment.** Sparse Levenberg-Marquardt; Ceres- / g2o-style solvers.
- **Multi-view stereo.** PatchMatch stereo, plane-sweep, neural MVS.
- **Dense reconstruction.** Poisson surface reconstruction, screened Poisson, ball-pivoting; mesh from point cloud.
- **Texture projection.** Multi-view texture baking with seam-aware blending; texture optimization.

## G. Procedural generation

Beyond §4.6.j scripted parametric and §4.3.b procedural textures.

- **L-systems.** Lindenmayer rewriting rules for plants, branching structures, fractals. Stochastic and parametric variants.
- **Wave Function Collapse.** Constraint-propagation tile/voxel generation (Gumin).
- **Terrain generation.** Erosion simulation (hydraulic, thermal), plate tectonics, fluvial network synthesis, noise-based heightfields with multi-fractal layering.
- **City and dungeon generation.** Recursive subdivision, growth simulation, parcel-driven layout.
- **Houdini-style procedural workflows.** Attribute-driven generation; the §4.6.c node graph is the right home.
- **Procedural texture synthesis from examples.** Image quilting (Efros-Freeman), PatchMatch, Wang tiles, neural texture synthesis (Gatys).

## H. Neural / AI graphics

NeRF / 3DGS in §4.10 only scratches it.

- **Neural denoising as a topic.** Train an OIDN-like denoiser; understand the loss functions, the AOV inputs, the training data pipeline. Compare against Intel OIDN, OptiX denoiser, and A-SVGF-style temporal reconstruction.
- **Neural radiosity.** MLP per-surface representation of indirect light.
- **Neural texture compression (NTC).** Per-asset neural decoders for ultra-compressed textures.
- **Neural shading.** Learned BRDF approximations; learned shading models for stylized rendering.
- **Generative 3D.** Text-to-3D (DreamFusion, Magic3D, Zero-1-to-3, GET3D, Shap-E). Score distillation sampling.
- **Image-conditional 3D.** Single-image novel-view synthesis; 3D-aware generative models.
- **Neural rigging.** Auto-rigging via learned skeletons; learned skinning weights.

## I. Discrete differential geometry & advanced mesh processing

UV unwrap covers part of §4.6.e; the deeper toolbox is missing.

- **Operators.** Discrete Laplace-Beltrami, mean-curvature normal, discrete exterior calculus.
- **Curvature.** Gaussian curvature, mean curvature, principal curvature directions.
- **Geodesics.** Heat method (Crane et al.), fast marching on triangulated surfaces, exact polyhedral geodesics.
- **Deformation.** As-Rigid-As-Possible (ARAP), Laplacian editing, biharmonic deformation, cage-based deformation.
- **Parameterization.** Conformal flattening, hyperbolic parameterization, periodic global parameterization, seamless parameterization.
- **Mesh segmentation.** Geodesic-distance clustering, normal-cycle, learning-based segmentation.
- **Vector field design on surfaces.** N-RoSy fields, cross fields for quad-meshing.

## J. Physics — depth beyond §4.10

The §4.10 listing names rigid/cloth/SPH/smoke; the algorithm catalog deeper down is missing.

- **Solver families.** Position-based dynamics (PBD), extended PBD (XPBD), projective dynamics, finite-element methods (FEM, corotational), material point method (MPM).
- **Fluid solvers.** SPH (already noted), FLIP (Fluid-Implicit-Particle), PIC, APIC, Eulerian grid (Stam stable fluids), lattice-Boltzmann, level-set surface tracking.
- **Cloth.** Mass-spring (Provot), PBD/XPBD constraints, FEM cloth, anisotropic woven models.
- **Hair.** Mass-spring chains, Discrete Elastic Rods (DER), super-helices.
- **Collision detection.** Broad-phase: SAH-BVH, sweep-and-prune, spatial hashing. Narrow-phase: GJK + EPA, Minkowski portal, separating-axis test (SAT). Continuous collision detection (CCD).
- **Constraint solvers.** Sequential impulses (Catto), projected Gauss-Seidel, projected conjugate gradient, ADMM.
- **Smoke and fire.** Stam stable fluids, vorticity confinement, FFT-based solvers, combustion models.

## K. Atmosphere, sky, and aerial perspective

Hosek-Wilkie is in §4.4.b; the surrounding work isn't.

- **Bruneton-Neyret precomputed atmospheric scattering.** 4D LUT-based real-time atmospheric scattering.
- **Wilkie et al.** "A fitted analytic spectral model" of the sky.
- **Multiple scattering.** Rayleigh + Mie, ground bounce, multi-scattering integration.
- **Aerial perspective.** Distance-driven atmospheric tinting in any engine, not just realistic-sky scenes.
- **Volumetric fog with sun shafts.** Per-pixel ray-marched fog with shadow-mapped scattering.

## L. Anti-aliasing catalog

The implementation sequence has been promoted to roadmap §4.1.c; keep this catalog here for variants that have not been pulled into active work yet.

- **Stochastic supersampling.** N samples per pixel, jittered/blue-noise distributions, reconstruction filter (see M).
- **MSAA.** Multi-sample anti-aliasing in a rasterizer; per-fragment vs per-sample shading.
- **Temporal AA family.** TAA (jittered camera + history reprojection), TAAU (temporal upsampling), DLAA, FXAA, SMAA (T1x/T2x/4x).
- **Temporal super-resolution and frame generation.** DLSS / FSR / XeSS-style upscalers, optical-flow frame interpolation, and the motion-vector/history-buffer contracts they require.
- **Morphological AA.** MLAA, SMAA-as-morphological.
- **Conservative rasterization.** For voxelization and procedural geometry.
- **Alpha-to-coverage.** Decoupling shading from coverage for foliage and hair.
- **Stochastic transparency.** Hashed/stochastic alpha for unsorted transparency.
- **Order-independent transparency (OIT).** Depth peeling, weighted blended OIT, per-pixel linked lists / A-buffer, and moment-based transparency.

## M. Reconstruction filters

For framebuffer accumulation and image resampling.

- **Filter shapes.** Box, tent (linear), Gaussian (with controllable σ), Mitchell-Netravali (B/C tunable), Lanczos (n=2/3), Blackman-Harris, B-spline cubic.
- **Negative-lobe filters and ringing artifacts.** The visual difference at 1024 SPP is instructive.
- **Per-pixel filter weight accumulation.** Separable vs non-separable; importance-sample-aware filters.
- **Belongs near roadmap §R1** once the float framebuffer lands.

## N. Character animation

Currently a one-line entry at the bottom of §4.10. A whole field that deserves its own pillar.

- **Skinning.** Linear blend skinning (LBS), dual-quaternion skinning (DQS), optimized centers of rotation (Le-Hodgins), implicit skinning.
- **Blend shapes / morph targets.** Linear interpolation, sparse blend shapes, corrective shape keys.
- **Pose-space deformation (PSD).** Shape correctives keyed on joint poses.
- **Inverse kinematics.** CCD, FABRIK, Jacobian transpose / pseudo-inverse / damped least squares, Newton-Raphson; foot-IK, two-bone IK, full-body IK.
- **Motion capture.** BVH/FBX import, retargeting (skeleton remapping, motion warping), motion graphs.
- **Auto-rigging.** Pinocchio (Baran-Popović), learned approaches (RigNet).
- **Facial animation.** FACS-based blend rigs, audio-driven lip sync (phoneme-to-blendshape), 3D Morphable Models (3DMM, FLAME).
- **Crowd simulation.** Reynolds boids, ORCA / RVO, vector-field steering, flow tiles.

## O. File formats — additions

§4.5 is solid; these belong:

- **Alembic (`.abc`).** Industry-standard offline animation cache.
- **USDZ.** USD-for-AR; zip-bundled USD for delivery.
- **LDraw.** LEGO part-library and model ingestion. Command-record parsing exists;
  referenced-file resolution, part-library search paths, and scene or mesh
  conversion remain open.
- **glTF extensions.** KHR_materials_* (clearcoat, sheen, transmission, volume, ior, anisotropy, dispersion); KHR_lights_punctual; KHR_animation_pointer.
- **Draco mesh compression.** Codec for compressed mesh delivery.
- **KTX2 + Basis Universal.** Transmissible compressed-texture container.
- **MaterialX.** Portable material/shader graph standard. The §4.6.c material node graph should target/import MaterialX rather than reinventing it.
- **OSL (Open Shading Language).** Cross-renderer shader DSL; plausible target for §4.3.b layer 4 (scripted textures) or for the §4.6.c material graph compiler.

## P. Texture compression and synthesis

§4.3 covers types and procedural generation; missing:

- **Compression.** BC1-BC7 (DXT/BPTC), ASTC, ETC2, PVRTC. Rate-distortion tradeoffs; sRGB-aware compression.
- **Synthesis from examples.** Image quilting (Efros-Freeman), PatchMatch (Barnes et al.), texture transfer, Wang tiles, neural texture synthesis.
- **Texture optimization.** Super-resolution, denoising, gradient-domain editing.

## Q. Production-rendering plumbing

Cryptomatte is in §4.3.d AOVs; the rest of the production pipeline isn't.

- **Light path expressions (LPE).** Filter contributions by path topology (e.g., `CD<L.>` = direct diffuse from light L).
- **Light groups.** Per-light AOV channels for relighting in compositing.
- **Render layers / passes.** Beauty / shadow / ambient occlusion / matte passes as parallel render targets.
- **Holdout mattes.** Objects that block light but aren't shaded — for compositing CG over plates.
- **Render submission and farm management.** Already partly in §4.8 distributed rendering; the production-side concerns (priorities, dependencies, retries, frame-range chunking) are separate.

## R. 2D vector & typography

For grease pencil (§4.6.g) and NLE titles (§4.7.b).

- **GPU Bezier rendering.** Loop-Blinn quadratic-cubic curve rendering. Resolution-independent vector graphics on the GPU.
- **Multi-channel signed distance fields (MSDF).** Sharp text rendering at arbitrary scale (Chlumský et al.).
- **Variable fonts.** OpenType variable fonts, axis interpolation, animation-friendly type.
- **Path operations.** Vector boolean operations (Vatti — already in §4.2.c), stroke offsetting, path simplification.

## S. VR / AR pipeline

Stereo cameras exist in §4.4.a; VR-specific concerns don't.

- **Foveated rendering.** Eye-tracked variable rate shading; circular foveation patterns.
- **Async timewarp / spacewarp.** Reproject the last frame to current head pose; spacewarp for motion compensation.
- **Lens distortion correction.** Mesh-based barrel correction for VR HMD lenses; chromatic aberration correction.
- **IPD and convergence calibration.** Per-user adjustments; dynamic convergence for in-headset comfort.
- **Stereo rendering optimizations.** Single-pass stereo, multi-view rendering, instanced stereo.

## T. Volumetric tooling

OpenVDB read in §4.5; the rest:

- **Direct volume rendering (DVR).** Ray-marched volume integration with a transfer function.
- **Transfer functions.** Density-to-color/opacity mappings; multi-dimensional transfer functions on (density, gradient magnitude); hand-edited transfer-function widgets.
- **Iso-surface extraction.** Marching cubes (already mentioned), surface nets, dual contouring, dual marching cubes.
- **Volume sculpt mode.** UI for editing voxel-grid representations directly (paint density, paint vector field, paint emission).
- **Sparse volumes.** OpenVDB-style spatially sparse grid math; topology-aware operations.
- **Volume-rendering estimators.** Delta / Woodcock tracking, ratio tracking, residual-ratio tracking, null-collision estimators, decomposition/spectral tracking, majorant-grid construction, equiangular sampling near lights, photon beams, and volumetric photon mapping.

## U. Image-processing fundamentals

Foundation for many §4.9 effects, not currently called out as its own topic.

- **Linear filters.** Gaussian, box, separable convolutions; FIR/IIR.
- **Non-linear filters.** Bilateral, joint bilateral, guided filter, domain transform, anisotropic diffusion, NL-means.
- **Image pyramids.** Gaussian pyramid, Laplacian pyramid, steerable pyramid.
- **Wavelets.** Haar, Daubechies, biorthogonal; 2D wavelet transforms for compression and denoising.
- **Frequency domain.** FFT, DCT; spectrum analysis; convolution via FFT.
- **Edge detection.** Sobel, Prewitt, Laplacian-of-Gaussian, Canny.
- **Morphological operations.** Erosion, dilation, opening, closing, morphological gradient.

## V. Observability and performance instrumentation

The §3.7 perf counters (`RAYTRACER_ENABLE_STATS`) are the minimal-viable seed: four hand-wired atomic counters in `Sphere`/`BoundingBox`/`Grid`, dumped to stderr as one-line JSON at the end of `Raytracer::render`. The whole subsystem is gated on a single compile-time macro and the increments expand to `(void)0` when off. Production builds carry zero overhead, but the trade-off is that the counters aren't there when you need them most — when you're trying to debug a regression on a user's machine or characterise a scene that someone else built.

The endgame is a counter infrastructure that's **always on, always available, always cheap** — and where individual counters can still be opted out at compile time when they sit on truly hot inner loops where a `relaxed` atomic would measurably perturb the result.

Pillars of that future system:

- **Always-on registry.** A self-registering counter type (`raytracer::stats::Counter counter("grid.traversal.steps")`) that lives in a global registry. Adding a new counter is a one-line declaration, not a fork of `Stats.h` plus a wire-up commit. Hierarchical names (`grid.traversal.steps`, `sphere.intersect.calls`, `sphere.intersect.hits`) so dump output is groupable and comparable across renders.
- **Tiered cost model.** Three tiers of counters, picked per call site:
  1. **Always-on** — the cheap stuff (`std::atomic<uint64_t>` increments at function-call granularity, e.g. one per primary ray, per BVH traversal, per material shade). These have measurable but acceptable cost on a benchmark and stay in production builds.
  2. **Default-on, compile-time gated** — the per-iteration counters that sit inside the tightest inner loops (per-DDA-step, per-ray-vs-tri, per-quartic-iteration). Default ON in debug/release/profile builds, off in a `--profile=ship` preset.
  3. **Manual opt-in** — diagnostic counters that only the developer of that algorithm cares about; off unless they pass `-DRAYTRACER_STATS_FOO=ON`.
- **Per-thread accumulation, snapshot on demand.** Workers increment thread-local counters; `Counters::snapshot()` reduces across threads on read. Avoids the cache-line bouncing on a shared `std::atomic` when you have 24 worker threads hammering the same field. Already worth it for `gridTraversalSteps`.
- **Histograms and timers, not just counters.** Sample distributions: ray-traversal-depth histogram, shading-recursion-depth histogram, per-pixel time histogram. Plus scoped timers (`stats::Timer t("Raytracer::render")`) backing into either a flat total or a flame-chart-friendly nested format. HDR-histogram-style log-bucket layout so a single histogram gives you `p50/p95/p99/p99.9` without keeping every sample.
- **Per-region, per-material, per-light counters.** Tag the current shading context (which Material, which Light, which BVH branch) so counters can be sliced after the fact: "ray-vs-tri tests broken down by Mesh", "shadow rays per Light", "recursion depth distribution by Material type". This is what makes counters useful for *why is this scene slow* rather than just *how slow is this scene*.
- **Structured output formats.**
  - One-line JSON (current state) for grep / `jq`.
  - Newline-delimited JSON to a file, one record per render frame, for offline analysis of an animation.
  - Chrome trace JSON (`chrome://tracing` / Perfetto) for the scoped-timer flame chart.
  - StatsD / OpenTelemetry / Prometheus-compatible export for long-running renderers (think: render farms).
- **In-process API for tests.** A `Counters::scope()` RAII that snapshots-on-enter and snapshots-on-exit, so tests can write `EXPECT_LT(scope.delta("grid.traversal.steps"), 100'000)` without the global-state coordination headache. This is the bit that makes counters a *first-class invariant* rather than a debugging aid — regressions in traversal cost get caught by CI, not by the next person who happens to render a slow scene.
- **GUI surface in `Modeler`.** A live counters panel that updates as the render progresses; click-to-graph for histograms; per-frame deltas for animation. Shipping the analysis surface alongside the render makes it tractable to actually use this stuff, instead of having to wire up a separate tool every time.
- **Hardware perf integration (stretch).** PMU counters via `perf_event_open` on Linux and `kperf` on macOS — cache misses, branch mispredicts, IPC. These need root or special entitlements and are platform-specific (the §3.7 doc explicitly punted on them); a clean separation between "library counters" and "OS-level counters" keeps this as a pluggable backend that doesn't bleed into the core API.

**Fast-track triggers.** Reasons to graduate this from backlog to the roadmap before the natural cadence:
- A bottleneck investigation that the current four-counter dump can't answer (the moment you find yourself adding a fifth ad-hoc counter just to debug one thing, the registry should already exist).
- A render-farm or distributed-rendering effort (§4.8) — at that point you need exportable, time-series-friendly metrics, not stderr lines.
- A perf-regression in CI that the existing benchmark suite missed but that better counters would have caught.

**Adjacent items already in the roadmap.** Worth coordinating with these so this doesn't fork:
- `roadmap.md` §3.4 — Google Benchmark suite for SSE3 hot paths. Counters and benchmarks are complements, not competitors.
- `roadmap.md` §3.7 — current perf-counter section; this backlog entry is the long-tail vision, that section is the seed.


## W. Renderer architecture and production engine references

Roadmap §4.1 names the project engines; this backlog keeps the wider implementation patterns and industry comparators from disappearing.

- **Megakernel vs wavefront path tracing.** Compare simple monolithic transport against queue-based ray generation / intersection / shading / shadow / medium stages; document GPU divergence, compaction, occupancy, and CPU cache tradeoffs.
- **Sorted and batched shading.** Material-sorted queues, texture-set batching, light-sorted next-event work, and packet/SIMD traversal beyond the first `Ray4` / `Ray8` kernels.
- **Out-of-core rendering.** Geometry, texture, volume, and acceleration-structure paging; cache eviction; progressive refinement while data streams in.
- **Production renderer case studies.** PBRT, Mitsuba, Cycles, RenderMan, Arnold, V-Ray, OSPRay, Embree, Hydra/Storm, and USD render delegates. The goal is not compatibility with all of them — it is to understand which architectural choices each one made and why.
- **REYES / micropolygon pipeline.** Dicing, shading grids, displacement-heavy rendering, motion blur, and why RenderMan's historical architecture still matters pedagogically.
- **Modern real-time engine case studies.** Nanite-style virtualized geometry, Lumen-style GI, virtual shadow maps, mesh/task shaders, bindless resources, shader permutations, and GPU-driven culling.
- **Shader-language targets.** GLSL, HLSL, WGSL, SPIR-V, Metal Shading Language, OSL, and MaterialX as authoring/interop surfaces.

## X. Implicit, point, and curve rendering

Some pieces live in roadmap §4.2, but the broader field is worth tracking explicitly.

- **Signed-distance-field rendering.** Sphere tracing, distance-field ray marching, smooth-min CSG, SDF fractals, procedural SDF authoring, and neural SDFs.
- **Blobby implicit surfaces.** Metaballs, soft objects, convolution surfaces, and field composition operators.
- **Point-based rendering.** Point clouds, surfels, EWA splatting, visibility splatting, point-set surfaces, and conversion to/from meshes or Gaussian splats.
- **Curve and hair geometry.** Bezier/B-spline curve primitives, ribbons/tubes, true ray-curve intersections, hair acceleration structures, and raster/GPU curve rendering.
- **Adaptive meshing.** Marching tetrahedra, dual contouring variants, adaptive octree extraction, and out-of-core meshing for large fields.

## Y. Texture filtering, ray differentials, and procedural detail

The roadmap has texture sources and MIP-mapping; this is the deeper sampling story.

- **Ray differentials.** Footprint propagation through reflection/refraction, texture LOD for ray/path tracing, glossy footprints, and camera/lens differential initialization.
- **Anisotropic texture filtering.** Elliptical weighted average (EWA), ripmaps, summed-area tables, and practical approximation tradeoffs.
- **Procedural detail families.** Worley/Voronoi, Perlin/simplex, fBm, wavelet noise, Gabor noise, sparse convolution noise, texture bombing, and domain warping.
- **Relief-style mapping.** Parallax mapping, steep parallax, parallax occlusion, relief mapping, and when they should yield to true displacement.

---

*End of backlog. Items graduate to the roadmap when picked up.*
