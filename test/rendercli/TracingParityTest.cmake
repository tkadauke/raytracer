if(NOT DEFINED RENDERCLI)
  message(FATAL_ERROR "RENDERCLI is required")
endif()

function(render_whitted_gpu_parity category scene depth)
  if(ARGC GREATER 3)
    set(rms_tolerance "${ARGV3}")
  else()
    set(rms_tolerance 0.001)
  endif()

  set(cpu_render "${TEST_OUTPUT_DIR}/whitted-${category}-cpu.png")
  set(cpu_metrics "${TEST_OUTPUT_DIR}/whitted-${category}-cpu-metrics.json")
  set(gpu_request_render "${TEST_OUTPUT_DIR}/whitted-${category}-gpu-request.png")
  set(gpu_request_metrics "${TEST_OUTPUT_DIR}/whitted-${category}-gpu-request-metrics.json")
  set(common_whitted_args
      --engine wavefront
      --integrator whitted
      --width 32
      --height 24
      --sampler Regular
      --samples_per_pixel 1
      --sampling_seed 1337
      --wavefront_denoiser none
      --depth "${depth}")

  rendercli_run(
    NAME "rendercli Whitted tracing parity ${category} CPU baseline"
    OUTPUT_VARIABLE whitted_cpu_stdout
    STDOUT_MATCHES
      "wavefront_metrics.*integrator=whitted.*execution=depth_major_whitted.*intersection_backend_request=cpu.*intersection_backend=cpu.*intersection_backend_availability=available.*intersection_backend_fallback=none.*intersection_backend_execution=runtime_scene.*closest_hit_execution=runtime_scene.*intersection_expected_closest_hit_rays=[1-9][0-9]*.*intersection_expected_any_hit_rays=[1-9][0-9]*.*intersection_scene_compiled=false.*closest_hit_rays=[1-9][0-9]*.*any_hit_rays=[1-9][0-9]*"
    COMMAND
      "${RENDERCLI}" ${common_whitted_args}
      --wavefront_intersection_backend cpu
      --wavefront_metrics_out "${cpu_metrics}"
      --wavefront_metrics_summary
      "${scene}" "${cpu_render}"
  )
  rendercli_assert_image_dimensions("${cpu_render}" 32 24
                                    NAME "Whitted tracing parity ${category} CPU dimensions")
  rendercli_assert_image_nonempty("${cpu_render}"
                                  NAME "Whitted tracing parity ${category} CPU pixels")

  foreach(expectation
          "\"intersectionBackendRequest\"[ \r\n]*:[ \r\n]*\"cpu\""
          "\"intersectionBackend\"[ \r\n]*:[ \r\n]*\"cpu\""
          "\"intersectionBackendAvailability\"[ \r\n]*:[ \r\n]*\"available\""
          "\"intersectionBackendFallbackReason\"[ \r\n]*:[ \r\n]*\"\""
          "\"intersectionBackendExecutionPath\"[ \r\n]*:[ \r\n]*\"runtime_scene\""
          "\"intersectionBackendClosestHitExecutionPath\"[ \r\n]*:[ \r\n]*\"runtime_scene\""
          "\"intersectionSceneCompiled\"[ \r\n]*:[ \r\n]*false")
    tracing_parity_assert_json_matches(
      "Whitted tracing parity ${category} CPU metrics ${expectation}" "${cpu_metrics}"
      "${expectation}")
  endforeach()

  rendercli_run(
    NAME "rendercli Whitted tracing parity ${category} GPU-requested candidate"
    OUTPUT_VARIABLE whitted_gpu_request_stdout
    STDOUT_MATCHES
      "wavefront_metrics.*integrator=whitted.*execution=depth_major_whitted.*intersection_backend_request=gpu.*intersection_backend=(cpu|metal|vulkan).*intersection_backend_availability=(available|fallback).*intersection_backend_execution=(packed_cpu|metal|vulkan).*closest_hit_execution=(packed_cpu|metal|vulkan).*intersection_expected_closest_hit_rays=[1-9][0-9]*.*intersection_expected_any_hit_rays=[1-9][0-9]*.*intersection_scene_compiled=true.*intersection_scene_unsupported=0.*closest_hit_rays=[1-9][0-9]*.*any_hit_rays=[1-9][0-9]*"
    COMMAND
      "${RENDERCLI}" ${common_whitted_args}
      --wavefront_intersection_backend gpu
      --wavefront_metrics_out "${gpu_request_metrics}"
      --wavefront_metrics_summary
      "${scene}" "${gpu_request_render}"
  )
  rendercli_assert_image_dimensions(
    "${gpu_request_render}" 32 24
    NAME "Whitted tracing parity ${category} GPU-requested dimensions")
  rendercli_assert_image_nonempty(
    "${gpu_request_render}" NAME "Whitted tracing parity ${category} GPU-requested pixels")
  rendercli_assert_image_rms_at_most(
    "${cpu_render}" "${gpu_request_render}" "${rms_tolerance}"
    NAME "Whitted tracing parity ${category} GPU-requested image RMS matches CPU")

  foreach(expectation
          "\"intersectionBackendRequest\"[ \r\n]*:[ \r\n]*\"gpu\""
          "\"intersectionSceneCompiled\"[ \r\n]*:[ \r\n]*true"
          "\"intersectionSceneUnsupportedPrimitives\"[ \r\n]*:[ \r\n]*0"
          "\"intersectionBackendExecutionPath\"[ \r\n]*:[ \r\n]*\"(packed_cpu|metal|vulkan)\""
          "\"intersectionBackendClosestHitExecutionPath\"[ \r\n]*:[ \r\n]*\"(packed_cpu|metal|vulkan)\"")
    tracing_parity_assert_json_matches(
      "Whitted tracing parity ${category} GPU-requested metrics ${expectation}"
      "${gpu_request_metrics}" "${expectation}")
  endforeach()

  if(whitted_gpu_request_stdout MATCHES "intersection_backend=(metal|vulkan)")
    set(actual_backend "${CMAKE_MATCH_1}")
    tracing_parity_assert_matches("Whitted tracing parity ${category} platform availability"
                                  "${whitted_gpu_request_stdout}"
                                  "intersection_backend_availability=available")
    tracing_parity_assert_matches("Whitted tracing parity ${category} platform fallback"
                                  "${whitted_gpu_request_stdout}"
                                  "intersection_backend_fallback=none")
    tracing_parity_assert_matches("Whitted tracing parity ${category} platform execution"
                                  "${whitted_gpu_request_stdout}"
                                  "intersection_backend_execution=${actual_backend}")
  else()
    tracing_parity_assert_matches("Whitted tracing parity ${category} packed CPU selected"
                                  "${whitted_gpu_request_stdout}" "intersection_backend=cpu")
    tracing_parity_assert_matches("Whitted tracing parity ${category} packed CPU availability"
                                  "${whitted_gpu_request_stdout}"
                                  "intersection_backend_availability=fallback")
    tracing_parity_assert_matches(
      "Whitted tracing parity ${category} packed CPU execution" "${whitted_gpu_request_stdout}"
      "intersection_backend_execution=packed_cpu.*closest_hit_execution=packed_cpu")
  endif()
endfunction()

if(NOT DEFINED TEST_OUTPUT_DIR)
  message(FATAL_ERROR "TEST_OUTPUT_DIR is required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/RendercliTestHelpers.cmake")

file(REMOVE_RECURSE "${TEST_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${TEST_OUTPUT_DIR}")

set(tracing_parity_fixture_dir
    "${PROJECT_SOURCE_DIR}/test/fixtures/tracing_parity")

function(tracing_parity_assert_matches name text regex)
  if(NOT text MATCHES "${regex}")
    _rendercli_fail("${name}" "text did not match expected regex: ${regex}" "" "" "${text}" "")
  endif()
endfunction()

function(tracing_parity_assert_json_matches name path regex)
  rendercli_assert_exists("${path}" NAME "${name} exists")
  file(READ "${path}" content)
  if(NOT content MATCHES "${regex}")
    _rendercli_fail("${name}" "JSON did not match expected regex: ${regex}" "" "" "${content}" "")
  endif()
endfunction()

function(tracing_parity_slug output_variable category)
  string(REPLACE "_" "-" slug "${category}")
  set(${output_variable} "${slug}" PARENT_SCOPE)
endfunction()

function(tracing_parity_common_json_assertions name metrics_path request_regex backend_regex
         availability_regex execution_regex)
  foreach(expectation
          "\"intersectionBackendRequest\"[ \r\n]*:[ \r\n]*\"${request_regex}\""
          "\"intersectionBackend\"[ \r\n]*:[ \r\n]*\"${backend_regex}\""
          "\"intersectionBackendAvailability\"[ \r\n]*:[ \r\n]*\"${availability_regex}\""
          "\"intersectionBackendExecutionPath\"[ \r\n]*:[ \r\n]*\"${execution_regex}\""
          "\"tracingBackend\"[ \r\n]*:[ \r\n]*\"${backend_regex}\""
          "\"tracingBackendMode\"[ \r\n]*:[ \r\n]*\"wavefront_intersection\""
          "\"tracingBackendCapabilities\"[ \r\n]*:[ \r\n]*\\[")
    tracing_parity_assert_json_matches("${name} metrics ${expectation}"
                                       "${metrics_path}" "${expectation}")
  endforeach()
endfunction()

function(tracing_parity_render_supported category scene_file depth samples rms_threshold
         extra_gpu_stdout_regex)
  tracing_parity_slug(slug "${category}")
  set(scene_path "${tracing_parity_fixture_dir}/${scene_file}")
  set(cpu_render "${TEST_OUTPUT_DIR}/${slug}-cpu.png")
  set(cpu_metrics "${TEST_OUTPUT_DIR}/${slug}-cpu-metrics.json")
  set(gpu_request_render "${TEST_OUTPUT_DIR}/${slug}-gpu-request.png")
  set(gpu_request_metrics "${TEST_OUTPUT_DIR}/${slug}-gpu-request-metrics.json")

  set(common_render_args
      --engine wavefront
      --integrator pathtracer
      --width 32
      --height 24
      --sampler Regular
      --samples_per_pixel "${samples}"
      --sampling_seed 1337
      --pathtracer_direct_light_samples 1
      --wavefront_denoiser none
      --depth "${depth}")

  rendercli_run(
    NAME "rendercli tracing parity ${category} CPU baseline"
    OUTPUT_VARIABLE cpu_stdout
    STDOUT_MATCHES
      "wavefront_metrics.*integrator=pathtracer.*execution=depth_major_paths.*tracing_backend=cpu.*tracing_backend_mode=wavefront_intersection.*tracing_backend_fallback=none.*intersection_backend_request=cpu.*intersection_backend=cpu.*intersection_backend_availability=available.*intersection_backend_fallback=none.*intersection_backend_execution=runtime_scene.*closest_hit_execution=runtime_scene.*intersection_expected_closest_hit_rays=[1-9][0-9]*.*intersection_scene_compiled=false.*closest_hit_rays=[1-9][0-9]*.*direct_light_contribution_execution=cpu.*direct_light_contribution_fallback=none"
    COMMAND
      "${RENDERCLI}" ${common_render_args}
      --wavefront_intersection_backend cpu
      --wavefront_metrics_out "${cpu_metrics}"
      --wavefront_metrics_summary
      "${scene_path}" "${cpu_render}"
  )
  rendercli_assert_image_dimensions("${cpu_render}" 32 24
                                    NAME "tracing parity ${category} CPU dimensions")
  rendercli_assert_image_nonempty("${cpu_render}"
                                  NAME "tracing parity ${category} CPU pixels")
  tracing_parity_common_json_assertions(
    "tracing parity ${category} CPU" "${cpu_metrics}" "cpu" "cpu" "available"
    "runtime_scene")
  tracing_parity_assert_json_matches(
    "tracing parity ${category} CPU metrics uncompiled scene" "${cpu_metrics}"
    "\"intersectionSceneCompiled\"[ \r\n]*:[ \r\n]*false")
  tracing_parity_assert_json_matches(
    "tracing parity ${category} CPU metrics direct light contribution execution" "${cpu_metrics}"
    "\"directLightContributionExecutionPath\"[ \r\n]*:[ \r\n]*\"cpu\"")
  tracing_parity_assert_json_matches(
    "tracing parity ${category} CPU metrics direct light contribution fallback" "${cpu_metrics}"
    "\"directLightContributionFallbackReason\"[ \r\n]*:[ \r\n]*\"\"")

  rendercli_run(
    NAME "rendercli tracing parity ${category} GPU-requested candidate"
    OUTPUT_VARIABLE gpu_request_stdout
    STDOUT_MATCHES
      "wavefront_metrics.*integrator=pathtracer.*execution=depth_major_paths.*tracing_backend=(cpu|metal|vulkan).*tracing_backend_mode=wavefront_intersection.*intersection_backend_request=gpu.*intersection_backend=(cpu|metal|vulkan).*intersection_backend_availability=(available|fallback).*intersection_backend_execution=(packed_cpu|metal|vulkan).*closest_hit_execution=(packed_cpu|metal|vulkan).*intersection_expected_closest_hit_rays=[1-9][0-9]*.*intersection_scene_compiled=true.*intersection_scene_unsupported=0.*closest_hit_rays=[1-9][0-9]*.*direct_light_contribution_execution=cpu.*direct_light_contribution_fallback=GPU_diffuse_direct-light_contribution_kernel_unavailable"
      "${extra_gpu_stdout_regex}"
    COMMAND
      "${RENDERCLI}" ${common_render_args}
      --wavefront_intersection_backend gpu
      --wavefront_metrics_out "${gpu_request_metrics}"
      --wavefront_metrics_summary
      "${scene_path}" "${gpu_request_render}"
  )
  rendercli_assert_image_dimensions("${gpu_request_render}" 32 24
                                    NAME "tracing parity ${category} GPU-requested dimensions")
  rendercli_assert_image_nonempty("${gpu_request_render}"
                                  NAME "tracing parity ${category} GPU-requested pixels")
  rendercli_assert_image_rms_at_most(
    "${cpu_render}" "${gpu_request_render}" "${rms_threshold}"
    NAME "tracing parity ${category} GPU-requested image RMS matches CPU")

  foreach(expectation
          "\"intersectionBackendRequest\"[ \r\n]*:[ \r\n]*\"gpu\""
          "\"intersectionSceneCompiled\"[ \r\n]*:[ \r\n]*true"
          "\"intersectionSceneUnsupportedPrimitives\"[ \r\n]*:[ \r\n]*0"
          "\"intersectionBackendExecutionPath\"[ \r\n]*:[ \r\n]*\"(packed_cpu|metal|vulkan)\""
          "\"intersectionBackendClosestHitExecutionPath\"[ \r\n]*:[ \r\n]*\"(packed_cpu|metal|vulkan)\""
          "\"tracingBackendMode\"[ \r\n]*:[ \r\n]*\"wavefront_intersection\""
          "\"tracingBackendCapabilities\"[ \r\n]*:[ \r\n]*\\["
          "\"directLightContributionExecutionPath\"[ \r\n]*:[ \r\n]*\"cpu\""
          "\"directLightContributionFallbackReason\"[ \r\n]*:[ \r\n]*\"GPU diffuse direct-light contribution kernel unavailable\""
          "\"name\"[ \r\n]*:[ \r\n]*\"lighting\\.direct_light_contribution\"[^}]*\"support\"[ \r\n]*:[ \r\n]*\"fallback\"")
    tracing_parity_assert_json_matches(
      "tracing parity ${category} GPU-requested metrics ${expectation}"
      "${gpu_request_metrics}" "${expectation}")
  endforeach()

  if(gpu_request_stdout MATCHES "intersection_backend=(metal|vulkan)")
    set(actual_backend "${CMAKE_MATCH_1}")
    tracing_parity_assert_matches("tracing parity ${category} platform backend availability"
                                  "${gpu_request_stdout}"
                                  "intersection_backend_availability=available")
    tracing_parity_assert_matches("tracing parity ${category} platform backend fallback"
                                  "${gpu_request_stdout}"
                                  "intersection_backend_fallback=none")
    tracing_parity_assert_matches("tracing parity ${category} platform backend execution"
                                  "${gpu_request_stdout}"
                                  "intersection_backend_execution=${actual_backend}")
    tracing_parity_assert_matches("tracing parity ${category} platform GPU flags"
                                  "${gpu_request_stdout}"
                                  "intersection_backend_gpu_device=true.*intersection_backend_gpu_render_path=true")
  else()
    tracing_parity_assert_matches("tracing parity ${category} packed CPU fallback selected"
                                  "${gpu_request_stdout}"
                                  "intersection_backend=cpu")
    tracing_parity_assert_matches("tracing parity ${category} packed CPU fallback availability"
                                  "${gpu_request_stdout}"
                                  "intersection_backend_availability=fallback")
    tracing_parity_assert_matches("tracing parity ${category} packed CPU fallback execution"
                                  "${gpu_request_stdout}"
                                  "intersection_backend_execution=packed_cpu.*closest_hit_execution=packed_cpu")
  endif()
endfunction()

function(tracing_parity_render_compiled_gpu_execution category scene_file depth samples)
  tracing_parity_slug(slug "${category}")
  set(scene_path "${tracing_parity_fixture_dir}/${scene_file}")
  set(gpu_execution_render "${TEST_OUTPUT_DIR}/${slug}-compiled-gpu-execution.png")
  set(gpu_execution_metrics "${TEST_OUTPUT_DIR}/${slug}-compiled-gpu-execution-metrics.json")

  rendercli_run(
    NAME "rendercli tracing parity ${category} compiled GPU execution metrics"
    OUTPUT_VARIABLE gpu_execution_stdout
    STDOUT_MATCHES
      "wavefront_metrics.*integrator=pathtracer.*execution=compiled_diffuse_path_loop.*tracing_backend_request=gpu.*tracing_backend=(cpu|gpu).*tracing_backend_mode=(compiled_cpu_reference|full_gpu_subset).*tracing_backend_platform=(none|metal|vulkan).*tracing_backend_fallback=(platform_full-GPU_path-loop_kernel_is_not_available_yet|none).*tracing_backend_capabilities=[1-9][0-9]*.*tracing_scene_compiled=true.*tracing_scene_materials=[1-9][0-9]*.*resident_path_loop_execution=(compiled_cpu_reference|full_gpu_subset).*resident_path_loop_residency=(cpu_host|metal_shared_diffuse_path_state|vulkan_host_visible_diffuse_path_state).*resident_path_loop_platform=(none|metal|vulkan).*resident_path_loop_depths=[1-9][0-9]*.*resident_path_loop_active_paths_per_depth=[1-9][0-9]*(,[0-9]+)*.*resident_path_loop_submitted_intersection_rays=[1-9][0-9]*.*resident_path_loop_full_platform_gpu_kernel=(false|true).*samples=[1-9][0-9]*.*accumulation_backend=(gpu_diffuse_path_loop|metal_diffuse_path_loop|vulkan_diffuse_path_loop)"
    COMMAND
      "${RENDERCLI}" --engine pathtracer --tracing_execution gpu
      --width 32 --height 24
      --sampler Regular
      --samples_per_pixel "${samples}"
      --sampling_seed 1337
      --pathtracer_direct_light_samples 1
      --wavefront_denoiser none
      --depth "${depth}"
      --wavefront_metrics_out "${gpu_execution_metrics}"
      --wavefront_metrics_summary
      "${scene_path}" "${gpu_execution_render}"
  )
  rendercli_assert_image_dimensions(
    "${gpu_execution_render}" 32 24
    NAME "tracing parity ${category} compiled GPU execution dimensions")
  rendercli_assert_image_nonempty(
    "${gpu_execution_render}"
    NAME "tracing parity ${category} compiled GPU execution pixels")

  foreach(expectation
          "\"compiledDiffusePathLoop\""
          "\"executionMode\"[ \r\n]*:[ \r\n]*\"compiled_diffuse_path_loop\""
          "\"tracingBackendCapabilities\""
          "\"capability\"[ \r\n]*:[ \r\n]*\"geometry.closest_hit\""
          "\"tracingBackendRequest\"[ \r\n]*:[ \r\n]*\"gpu\""
          "\"tracingSceneCompiled\"[ \r\n]*:[ \r\n]*true"
          "\"residentPathLoopSubmittedIntersectionRays\"[ \r\n]*:[ \r\n]*[1-9][0-9]*"
          "\"submittedIntersectionRays\"[ \r\n]*:[ \r\n]*[1-9][0-9]*"
          "\"requestedMode\"[ \r\n]*:[ \r\n]*\"gpu\"")
    tracing_parity_assert_json_matches(
      "tracing parity ${category} compiled GPU execution metrics ${expectation}"
      "${gpu_execution_metrics}" "${expectation}")
  endforeach()

  if(gpu_execution_stdout MATCHES "tracing_backend_mode=full_gpu_subset")
    tracing_parity_assert_matches(
      "tracing parity ${category} compiled GPU execution platform path"
      "${gpu_execution_stdout}"
      "tracing_backend=gpu.*tracing_backend_platform=(metal|vulkan).*tracing_backend_fallback=none")
    tracing_parity_assert_matches(
      "tracing parity ${category} compiled GPU execution resident path state"
      "${gpu_execution_stdout}"
      "resident_path_loop_execution=full_gpu_subset.*resident_path_loop_residency=(metal_shared_diffuse_path_state|vulkan_host_visible_diffuse_path_state).*resident_path_loop_platform=(metal|vulkan).*resident_path_loop_full_platform_gpu_kernel=true")
    tracing_parity_assert_matches(
      "tracing parity ${category} compiled GPU execution compaction summary"
      "${gpu_execution_stdout}"
      "frontier_compaction_execution=(metal_diffuse_path_loop|metal_diffuse_path_loop_wavefront|vulkan_diffuse_path_loop).*frontier_compaction_path_state_residency=(metal_shared_diffuse_path_state|vulkan_host_visible_diffuse_path_state)")
    tracing_parity_assert_matches(
      "tracing parity ${category} compiled GPU execution direct-light platform path"
      "${gpu_execution_stdout}"
      "resident_direct_light_batches_unavailable_reason=none.*direct_light_contribution_execution=full_gpu_subset.*direct_light_contribution_fallback=none")
    tracing_parity_assert_matches(
      "tracing parity ${category} compiled GPU execution supported capabilities summary"
      "${gpu_execution_stdout}"
      "tracing_backend_fallback_capabilities=0:none.*tracing_backend_restricted_capabilities=0:none")

    foreach(expectation
            "\"backend\"[ \r\n]*:[ \r\n]*\"full_gpu_subset\""
            "\"tracingBackend\"[ \r\n]*:[ \r\n]*\"gpu\""
            "\"tracingBackendMode\"[ \r\n]*:[ \r\n]*\"full_gpu_subset\""
            "\"tracingBackendPlatform\"[ \r\n]*:[ \r\n]*\"(metal|vulkan)\""
            "\"directLightContributionExecutionPath\"[ \r\n]*:[ \r\n]*\"full_gpu_subset\""
            "\"directLightContributionFallbackReason\"[ \r\n]*:[ \r\n]*\"\""
            "\"frontierCompactionExecutionPath\"[ \r\n]*:[ \r\n]*\"(metal_diffuse_path_loop|metal_diffuse_path_loop_wavefront|vulkan_diffuse_path_loop)\""
            "\"frontierCompactionPathStateResidency\"[ \r\n]*:[ \r\n]*\"(metal_shared_diffuse_path_state|vulkan_host_visible_diffuse_path_state)\""
            "\"intersectionBackendResidentDirectLightBatchesUnavailableReason\"[ \r\n]*:[ \r\n]*\"\""
            "\"residentPathLoopExecutionPath\"[ \r\n]*:[ \r\n]*\"full_gpu_subset\""
            "\"residentPathLoopResidency\"[ \r\n]*:[ \r\n]*\"(metal_shared_diffuse_path_state|vulkan_host_visible_diffuse_path_state)\""
            "\"residentPathLoopPlatformName\"[ \r\n]*:[ \r\n]*\"(metal|vulkan)\""
            "\"residentPathLoopFullPlatformGpuKernel\"[ \r\n]*:[ \r\n]*true"
            "\"platformName\"[ \r\n]*:[ \r\n]*\"(metal|vulkan)\""
            "\"fullPlatformGpuKernel\"[ \r\n]*:[ \r\n]*true"
            "\"predictedMode\"[ \r\n]*:[ \r\n]*\"gpu\""
            "\"actualMode\"[ \r\n]*:[ \r\n]*\"gpu\""
            "\"actualFallbackReason\"[ \r\n]*:[ \r\n]*\"\"")
      tracing_parity_assert_json_matches(
        "tracing parity ${category} compiled GPU execution metrics ${expectation}"
        "${gpu_execution_metrics}" "${expectation}")
    endforeach()
  else()
    tracing_parity_assert_matches(
      "tracing parity ${category} compiled GPU execution fallback path"
      "${gpu_execution_stdout}"
      "tracing_backend=cpu.*tracing_backend_platform=none.*tracing_backend_fallback=platform_full-GPU_path-loop_kernel_is_not_available_yet")
    tracing_parity_assert_matches(
      "tracing parity ${category} compiled GPU execution compaction summary"
      "${gpu_execution_stdout}"
      "frontier_compaction_execution=cpu_diffuse_frontier_compaction.*frontier_compaction_path_state_residency=cpu_host")
    tracing_parity_assert_matches(
      "tracing parity ${category} compiled GPU execution direct-light fallback"
      "${gpu_execution_stdout}"
      "resident_direct_light_batches_unavailable_reason=compiled_CPU-reference_path_loop_resolves_direct-light_visibility_on_the_host.*direct_light_contribution_execution=cpu_record.*direct_light_contribution_fallback=compiled_CPU-reference_path_loop_evaluates_direct-light_contribution_on_the_host")
    tracing_parity_assert_matches(
      "tracing parity ${category} compiled GPU execution fallback capabilities summary"
      "${gpu_execution_stdout}"
      "tracing_backend_fallback_capabilities=[1-9][0-9]*:.*lighting\\.direct_light_contribution=gpu->cpu:compiled_CPU-reference_path_loop_evaluates_direct-light_contribution_on_the_host.*state\\.frontier_compaction=gpu->cpu:compiled_CPU-reference_path_loop_compacts_path_state_on_the_host.*state\\.path_state_residency=gpu->cpu:compiled_CPU-reference_path_loop_keeps_path_state_on_the_host")
    tracing_parity_assert_matches(
      "tracing parity ${category} compiled GPU execution restricted capabilities summary"
      "${gpu_execution_stdout}"
      "tracing_backend_restricted_capabilities=[1-9][0-9]*:.*sampling\\.gpu_rng=cpu:gpu_sample_stream_cpu_reference")

    foreach(expectation
            "\"backend\"[ \r\n]*:[ \r\n]*\"compiled_cpu_reference\""
            "\"tracingBackend\"[ \r\n]*:[ \r\n]*\"cpu\""
            "\"tracingBackendMode\"[ \r\n]*:[ \r\n]*\"compiled_cpu_reference\""
            "\"tracingBackendPlatform\"[ \r\n]*:[ \r\n]*\"none\""
            "\"directLightContributionFallbackReason\"[ \r\n]*:[ \r\n]*\"compiled CPU-reference path loop evaluates direct-light contribution on the host\""
            "\"frontierCompactionExecutionPath\"[ \r\n]*:[ \r\n]*\"cpu_diffuse_frontier_compaction\""
            "\"frontierCompactionPathStateResidency\"[ \r\n]*:[ \r\n]*\"cpu_host\""
            "\"intersectionBackendResidentDirectLightBatchesUnavailableReason\"[ \r\n]*:[ \r\n]*\"compiled CPU-reference path loop resolves direct-light visibility on the host\""
            "\"residentPathLoopExecutionPath\"[ \r\n]*:[ \r\n]*\"compiled_cpu_reference\""
            "\"residentPathLoopResidency\"[ \r\n]*:[ \r\n]*\"cpu_host\""
            "\"residentPathLoopPlatformName\"[ \r\n]*:[ \r\n]*\"none\""
            "\"residentPathLoopFullPlatformGpuKernel\"[ \r\n]*:[ \r\n]*false"
            "\"platformName\"[ \r\n]*:[ \r\n]*\"none\""
            "\"fullPlatformGpuKernel\"[ \r\n]*:[ \r\n]*false"
            "\"predictedMode\"[ \r\n]*:[ \r\n]*\"hybrid\""
            "\"actualMode\"[ \r\n]*:[ \r\n]*\"cpu\""
            "\"actualFallbackReason\"[ \r\n]*:[ \r\n]*\"GPU tracing request executed by compiled CPU-reference diffuse path loop")
      tracing_parity_assert_json_matches(
        "tracing parity ${category} compiled GPU execution metrics ${expectation}"
        "${gpu_execution_metrics}" "${expectation}")
    endforeach()
  endif()
endfunction()

tracing_parity_render_supported(
  "matte_direct_light" "matte_direct_light.json" 1 1 0.001
  "direct_light_samples=[1-9][0-9]*")
tracing_parity_render_supported(
  "indirect_bounce" "indirect_bounce.json" 3 4 0.02
  "secondary_direct_light_luminance=[1-9]")
tracing_parity_render_compiled_gpu_execution(
  "indirect_bounce" "indirect_bounce.json" 3 4)
tracing_parity_render_supported(
  "environment_miss" "environment_miss.json" 2 1 0.001
  "tracing_scene_environment=[1-9][0-9]*.*miss_luminance=[1-9]")
tracing_parity_render_compiled_gpu_execution(
  "imported_mesh" "imported_mesh.json" 1 1)
tracing_parity_render_supported(
  "transparent_glass" "transparent_fallback.json" 4 1 0.05
  "tracing_scene_materials=[1-9][0-9]*")
tracing_parity_render_compiled_gpu_execution(
  "transparent_glass" "transparent_fallback.json" 4 1)
tracing_parity_render_compiled_gpu_execution(
  "visibility_heavy" "visibility_heavy.json" 2 1)
render_whitted_gpu_parity(
  "matte-direct-light" "${PROJECT_SOURCE_DIR}/test/fixtures/tracing_parity/matte_direct_light.json"
  2)
render_whitted_gpu_parity(
  "reflection" "${PROJECT_SOURCE_DIR}/test/fixtures/tracing_parity/whitted_reflection.json" 3
  0.01)
render_whitted_gpu_parity(
  "transparent-fallback"
  "${PROJECT_SOURCE_DIR}/test/fixtures/tracing_parity/transparent_fallback.json" 4
  0.02)
