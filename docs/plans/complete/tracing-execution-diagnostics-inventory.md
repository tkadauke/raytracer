# Tracing execution backend diagnostics inventory - June 2026

> **Scope:** inventory the current wavefront and intersection diagnostics that
> describe tracing execution. This is the compatibility map for moving from
> intersection-centered field names toward broader CPU, hybrid, and GPU
> tracing-execution capability records.
>
> **Status:** complete and archived. This inventory covered rendercli compact
> summaries, wavefront metrics JSON, render graph trace metadata, and Modeler
> render graph metadata. The current renderer behavior is unchanged by this
> document.

## Current data flow

The lowest-level counters live in `render::IntegratorBatchMetrics`
(`include/render/Integrator.h`). The wavefront engine aggregates those into
`engine::wavefront::WavefrontRenderMetrics::BatchSummary`
(`include/engine/wavefront/WavefrontRaytracer.h`) and serializes them from
`WavefrontRenderMetrics::toJson()` (`src/engine/wavefront/WavefrontRaytracer.cpp`).

That serialized payload is the stable diagnostics shape:

- `rendercli --wavefront_metrics_out` writes the aggregate JSON payload.
- `rendercli --wavefront_metrics_summary` reads the same JSON and prints
  compact snake_case fields on the `wavefront_metrics` line.
- Graph-backed wavefront passes attach the same payload to
  `RenderPassTrace::metadata()["batching"]`.
- The Modeler Render Graph inspector reads the pass trace `batching` object for
  summary text, selected-pass detail rows, and tooltips.

Because graph traces and rendercli metrics use the same serialized object, new
execution diagnostics should first appear in `WavefrontRenderMetrics::toJson()`.
rendercli and Modeler should then select a human-scale subset instead of
inventing alternate field names for the same concept.

## Concept inventory

| Execution concept | Metrics JSON and graph trace keys under `batching` | rendercli summary fields | Modeler metadata surface |
| --- | --- | --- | --- |
| Integrator and scheduler mode | `integrator`, `executionMode` | `integrator`, `execution` | Pass summary reports samples and humanized execution mode. |
| Backend request, resolution, and fallback | `intersectionBackendRequest`, `intersectionBackend`, `intersectionBackendPlatform`, `intersectionBackendAvailability`, `intersectionBackendFallbackReason` | `intersection_backend_request`, `intersection_backend`, `intersection_backend_platform`, `intersection_backend_availability`, `intersection_backend_fallback` | Pass summary and selected-pass details show request/resolution, platform, availability, and fallback reason. |
| Platform GPU capability probes | `intersectionBackendPlatformGpuDeviceAvailable`, `intersectionBackendPlatformGpuRenderPathAvailable` | `intersection_backend_gpu_device`, `intersection_backend_gpu_render_path` | Pass summary reports GPU device and render-path availability when present. |
| Automatic backend selection inputs | `intersectionBackendExpectedRays`, `intersectionBackendExpectedClosestHitRays`, `intersectionBackendExpectedAnyHitRays`, `intersectionBackendAutoMinimumGpuRays`, `intersectionBackendAutoEstimatedQueryTransferBytes` | `intersection_expected_rays`, `intersection_expected_closest_hit_rays`, `intersection_expected_any_hit_rays`, `intersection_auto_minimum_gpu_rays`, `intersection_auto_estimated_query_transfer_bytes` | Pass summary reports expected rays, query-family split, auto GPU threshold, and auto transfer estimate. |
| Query execution path | `intersectionBackendExecutionPath`, `intersectionBackendClosestHitExecutionPath`, `intersectionBackendAnyHitExecutionPath` | `intersection_backend_execution`, `closest_hit_execution`, `any_hit_execution` | Pass summary shows the resolved path (`runtime_scene`, `compiled_cpu`, `packed_cpu`, `metal`, `vulkan`, or `mixed`). |
| Submitted query work | `intersectionRaysSubmitted`, `closestHitRaysSubmitted`, `anyHitRaysSubmitted`, `closestHitQueries`, `anyHitQueries`, `intersectionRaysPerWorkerSecond`, `intersectionBackendKernelRaysPerSecond` | `intersection_rays`, `closest_hit_rays`, `any_hit_rays`, `closest_hit_queries`, `any_hit_queries`, `intersection_rays_per_worker_second`, `intersection_backend_kernel_rays_per_second` | Pass summary reports closest-hit and any-hit rays; details include throughput rows when available. |
| Backend timing buckets | `intersectionBackendUploadWorkerSeconds`, `intersectionBackendKernelWorkerSeconds`, `intersectionBackendReadbackWorkerSeconds` | `intersection_backend_upload_worker_ms`, `intersection_backend_kernel_worker_ms`, `intersection_backend_readback_worker_ms` | Pass summary reports upload/kernel/readback time when any bucket is non-zero. |
| Compiled intersection scene | `intersectionSceneCompiled`, `intersectionSceneBvhNodes`, `intersectionScenePrimitives`, primitive count keys, `intersectionSceneUnsupportedPrimitives`, `intersectionSceneUnsupportedReasons`, `intersectionSceneUploadBytes` | `intersection_scene_compiled`, `intersection_scene_bvh_nodes`, `intersection_scene_primitives`, primitive count fields, `intersection_scene_unsupported`, `intersection_scene_unsupported_by_reason`, `intersection_scene_upload_bytes` | Pass summary reports primitive/BVH counts, payload summary, unsupported reasons, and upload bytes. |
| Packed/platform eligibility | `intersectionSceneTriangleClosestHitEligible`, `intersectionSceneBasicHitEligible`, `intersectionScenePackedClosestHitEligible`, `intersectionScenePackedAnyHitEligible` | `intersection_scene_triangle_kernel_eligible`, `intersection_scene_basic_hit_kernel_eligible`, `intersection_scene_packed_closest_hit_eligible`, `intersection_scene_packed_any_hit_eligible` | Pass summary reports each eligibility flag. |
| Estimated query transfer | `intersectionEstimatedRayUploadBytes`, closest-hit/any-hit upload and readback keys, `intersectionEstimatedQueryTransferBytes`, closest-hit/any-hit transfer keys, query round-trip keys | `intersection_estimated_ray_upload_bytes`, closest-hit/any-hit upload/readback/transfer fields, `intersection_estimated_query_round_trips`, closest-hit/any-hit round-trip fields | Pass summary reports total query transfer, query-family upload split, and round trips. |
| Backend frontier residency and payload size | `intersectionBackendClosestHitFrontierResidency`, `intersectionBackendAnyHitFrontierResidency`, closest-hit/any-hit `PackedRayBytes`, `HostQueryBytes`, and `StateHandleBytes` | `closest_hit_frontier_residency`, `any_hit_frontier_residency`, closest-hit/any-hit `*_frontier_packed_ray_bytes`, `*_frontier_host_query_bytes`, and `*_frontier_state_handle_bytes` | Pass summary reports residency and frontier payload byte totals. |
| Resident-frontier and scheduler capability flags | `intersectionBackendPrefersClosestHitBatch`, `intersectionBackendPrefersAnyHitBatch`, `intersectionBackendSupportsResidentFrontiers`, `intersectionBackendSupportsGpuFrontierCompaction`, `intersectionBackendGpuFrontierCompactionUnavailableReason`, `intersectionBackendSupportsPreparedRayBatchCompaction`, `intersectionBackendSupportsResidentDirectLightBatches`, `intersectionBackendResidentDirectLightBatchesUnavailableReason` | `closest_hit_batch_preferred`, `any_hit_batch_preferred`, `resident_frontiers_supported`, `gpu_frontier_compaction_supported`, `gpu_frontier_compaction_unavailable_reason`, `prepared_ray_batch_compaction_supported`, `resident_direct_light_batches_supported`, `resident_direct_light_batches_unavailable_reason` | Pass summary groups resident-frontier, prepared-ray compaction, frontier compaction, and resident direct-light support. |
| Depth-major frontier work | `activeSamplesPerDepth`, `retainedActiveSamplesPerDepth`, `frontierRayHitsPerDepth`, `frontierRayMissesPerDepth`, packet/scalar/refinement arrays, closest-hit batch arrays | `active_depths`, `last_active`, `last_retained_active`, `frontier_hit_rays`, `frontier_miss_rays`, packet/scalar/refinement fields, `frontier_closest_hit_batch_*` | Pass summary reports frontier hit/miss, packet fill, scalar fallback, and batch sizes. |
| Direct-light any-hit work | `directLightAnyHitBatchChunksPerDepth`, `directLightAnyHitBatchRaysPerDepth`, `directLightAnyHitQueryRoundTrips`, resident direct-light round-trip estimates, direct-light host byte keys, direct-light any-hit frontier byte keys | `direct_light_any_hit_batch_chunks`, `direct_light_any_hit_batch_rays`, `direct_light_any_hit_batch_avg`, round-trip fields, direct-light byte fields | Pass summary reports direct-light sample counts, any-hit batches, round trips, host bytes, and frontier bytes. |
| Mixed query depths | `frontierMixedQueryDepths`, `frontierMixedQueryRoundTrips`, `frontierMixedQueryRays`, `frontierMixedQueryClosestHitRays`, `frontierMixedQueryAnyHitRays` | `frontier_mixed_query_depths`, `frontier_mixed_query_round_trips`, `frontier_mixed_query_rays`, `frontier_mixed_query_closest_hit_rays`, `frontier_mixed_query_any_hit_rays` | Pass summary reports mixed query depths and mixed-depth round trips. |
| Host path-state and active-hit memory | `activeHostPathStateBytesPerDepth`, `activeHitHostBytesPerDepth`, `retainedHostPathStateBytesPerDepth`, `activeHostPathStateBytesProcessed`, `activeHitHostBytesProcessed`, spawned-continuation keys | `active_host_path_state_bytes`, `active_hit_host_bytes`, `last_active_host_path_state_bytes`, `last_active_hit_host_bytes`, retained/spawned continuation fields | Pass summary reports active-hit bytes, active/retained host path-state bytes, and spawned continuations. |
| Frontier compaction | `frontierCompaction*`, `frontierCompactionExecutionPath`, compaction candidate keys, largest compaction candidate keys | `frontier_compaction_*`, `frontier_compaction_candidate_*`, `frontier_largest_compaction_candidate_*` | Pass summary reports executed compaction, retained-index bytes, host path-state bytes, candidates, and largest candidate depth. |
| Material and radiance compatibility | `compatibilityShadeSamples`, `unsupportedPathMaterialSamples`, emitter/direct-light sample counters, radiance luminance sum keys | `compatibility_shade_samples`, `unsupported_path_material_samples`, emitter/direct-light/luminance fields | Pass summary reports compatibility shading, unsupported path materials, emitter hits, direct-light samples, and luminance split. |
| Convergence, adaptive sampling, denoising, tiling, and timings | Top-level `convergence`, `adaptiveSampling`, `denoise`, `tiling`, `timings`; selected values also come from `batching` | `convergence`, `adaptive`, `denoiser`, tile/load-balance fields, total and worker timing fields | Modeler pass details and summaries combine `batching`, `scheduling`, `convergence`, `adaptiveSampling`, `denoise`, `tiling`, and `timings`. |

## Compatibility aliases that must remain

These names are already consumed by tests or external diagnostics and should
stay until a deliberate schema migration removes them with release notes and
consumer guidance.

- Metrics JSON and graph trace aliases:
  - `frontierHostCompactionPasses` aliases `frontierCompactionPasses`.
  - `frontierHostCompactionInputSamples` aliases
    `frontierCompactionInputSamples`.
  - `frontierHostCompactionRetainedSamples` aliases
    `frontierCompactionRetainedSamples`.
  - `frontierHostCompactionRemovedSamples` aliases
    `frontierCompactionRemovedSamples`.
  - `frontierHostCompactionMovedSamples` aliases
    `frontierCompactionMovedSamples`.
  - `frontierHostCompactionRemovedSampleFraction` aliases
    `frontierCompactionRemovedSampleFraction`.
- rendercli compact-summary aliases:
  - `direct_light_any_hit_chunks` aliases
    `direct_light_any_hit_batch_chunks`.
  - `direct_light_any_hit_chunk_rays` aliases
    `direct_light_any_hit_batch_rays`.
  - `direct_light_any_hit_chunk_avg` aliases
    `direct_light_any_hit_batch_avg`.

## Naming guidance for follow-up issues

Future capability records should group these fields by execution concept:
selection and fallback, platform availability, query-family execution,
compiled-scene capability, frontier residency, scheduler-resident operations,
memory movement, and timing. New broad tracing-execution names should be added
beside the existing `intersectionBackend*` names first. The existing names are
the compatibility surface for rendercli, graph traces, and Modeler until all
consumers have migrated.
