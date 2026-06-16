if(NOT DEFINED RENDERCLI)
  message(FATAL_ERROR "RENDERCLI is required")
endif()

if(NOT DEFINED TEST_OUTPUT_DIR)
  message(FATAL_ERROR "TEST_OUTPUT_DIR is required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/RendercliTestHelpers.cmake")

file(REMOVE_RECURSE "${TEST_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${TEST_OUTPUT_DIR}")

set(tracing_parity_scene
    "${PROJECT_SOURCE_DIR}/test/fixtures/tracing_parity/matte_direct_light.json")
set(cpu_render "${TEST_OUTPUT_DIR}/matte-direct-light-cpu.png")
set(cpu_metrics "${TEST_OUTPUT_DIR}/matte-direct-light-cpu-metrics.json")
set(gpu_request_render "${TEST_OUTPUT_DIR}/matte-direct-light-gpu-request.png")
set(gpu_request_metrics "${TEST_OUTPUT_DIR}/matte-direct-light-gpu-request-metrics.json")

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

set(common_render_args
    --engine wavefront
    --integrator pathtracer
    --width 32
    --height 24
    --sampler Regular
    --samples_per_pixel 1
    --sampling_seed 1337
    --pathtracer_direct_light_samples 1
    --wavefront_denoiser none
    --depth 1)

rendercli_run(
  NAME "rendercli tracing parity CPU baseline"
  OUTPUT_VARIABLE cpu_stdout
  STDOUT_MATCHES
    "wavefront_metrics.*integrator=pathtracer.*execution=depth_major_paths.*intersection_backend_request=cpu.*intersection_backend=cpu.*intersection_backend_availability=available.*intersection_backend_fallback=none.*intersection_backend_execution=runtime_scene.*closest_hit_execution=runtime_scene.*intersection_expected_closest_hit_rays=[1-9][0-9]*.*intersection_scene_compiled=false.*closest_hit_rays=[1-9][0-9]*.*direct_light_contribution_execution=cpu.*direct_light_contribution_fallback=none"
  COMMAND
    "${RENDERCLI}" ${common_render_args}
    --wavefront_intersection_backend cpu
    --wavefront_metrics_out "${cpu_metrics}"
    --wavefront_metrics_summary
    "${tracing_parity_scene}" "${cpu_render}"
)
rendercli_assert_image_dimensions("${cpu_render}" 32 24
                                  NAME "tracing parity CPU baseline dimensions")
rendercli_assert_image_nonempty("${cpu_render}" NAME "tracing parity CPU baseline pixels")

foreach(expectation
        "\"intersectionBackendRequest\"[ \r\n]*:[ \r\n]*\"cpu\""
        "\"intersectionBackend\"[ \r\n]*:[ \r\n]*\"cpu\""
        "\"intersectionBackendAvailability\"[ \r\n]*:[ \r\n]*\"available\""
        "\"intersectionBackendFallbackReason\"[ \r\n]*:[ \r\n]*\"\""
        "\"intersectionBackendExecutionPath\"[ \r\n]*:[ \r\n]*\"runtime_scene\""
        "\"intersectionBackendClosestHitExecutionPath\"[ \r\n]*:[ \r\n]*\"runtime_scene\""
        "\"directLightContributionExecutionPath\"[ \r\n]*:[ \r\n]*\"cpu\""
        "\"directLightContributionFallbackReason\"[ \r\n]*:[ \r\n]*\"\""
        "\"intersectionSceneCompiled\"[ \r\n]*:[ \r\n]*false")
  tracing_parity_assert_json_matches("tracing parity CPU metrics ${expectation}"
                                     "${cpu_metrics}" "${expectation}")
endforeach()

rendercli_run(
  NAME "rendercli tracing parity GPU-requested candidate"
  OUTPUT_VARIABLE gpu_request_stdout
  STDOUT_MATCHES
    "wavefront_metrics.*integrator=pathtracer.*execution=depth_major_paths.*intersection_backend_request=gpu.*intersection_backend=(cpu|metal|vulkan).*intersection_backend_availability=(available|fallback).*intersection_backend_execution=(packed_cpu|metal|vulkan).*closest_hit_execution=(packed_cpu|metal|vulkan).*intersection_expected_closest_hit_rays=[1-9][0-9]*.*intersection_scene_compiled=true.*intersection_scene_unsupported=0.*closest_hit_rays=[1-9][0-9]*.*direct_light_contribution_execution=cpu.*direct_light_contribution_fallback=GPU_diffuse_direct-light_contribution_kernel_unavailable"
  COMMAND
    "${RENDERCLI}" ${common_render_args}
    --wavefront_intersection_backend gpu
    --wavefront_metrics_out "${gpu_request_metrics}"
    --wavefront_metrics_summary
    "${tracing_parity_scene}" "${gpu_request_render}"
)
rendercli_assert_image_dimensions("${gpu_request_render}" 32 24
                                  NAME "tracing parity GPU-requested dimensions")
rendercli_assert_image_nonempty("${gpu_request_render}"
                                NAME "tracing parity GPU-requested pixels")
rendercli_assert_image_rms_at_most("${cpu_render}" "${gpu_request_render}" 0.001
                                   NAME "tracing parity GPU-requested image RMS matches CPU")

foreach(expectation
        "\"intersectionBackendRequest\"[ \r\n]*:[ \r\n]*\"gpu\""
        "\"intersectionSceneCompiled\"[ \r\n]*:[ \r\n]*true"
        "\"intersectionSceneUnsupportedPrimitives\"[ \r\n]*:[ \r\n]*0"
        "\"intersectionBackendExecutionPath\"[ \r\n]*:[ \r\n]*\"(packed_cpu|metal|vulkan)\""
        "\"intersectionBackendClosestHitExecutionPath\"[ \r\n]*:[ \r\n]*\"(packed_cpu|metal|vulkan)\""
        "\"directLightContributionExecutionPath\"[ \r\n]*:[ \r\n]*\"cpu\""
        "\"directLightContributionFallbackReason\"[ \r\n]*:[ \r\n]*\"GPU diffuse direct-light contribution kernel unavailable\""
        "\"name\"[ \r\n]*:[ \r\n]*\"lighting\\.direct_light_contribution\"[^}]*\"support\"[ \r\n]*:[ \r\n]*\"fallback\"")
  tracing_parity_assert_json_matches("tracing parity GPU-requested metrics ${expectation}"
                                     "${gpu_request_metrics}" "${expectation}")
endforeach()

if(gpu_request_stdout MATCHES "intersection_backend=(metal|vulkan)")
  set(actual_backend "${CMAKE_MATCH_1}")
  tracing_parity_assert_matches("tracing parity platform backend availability"
                                "${gpu_request_stdout}"
                                "intersection_backend_availability=available")
  tracing_parity_assert_matches("tracing parity platform backend fallback"
                                "${gpu_request_stdout}"
                                "intersection_backend_fallback=none")
  tracing_parity_assert_matches("tracing parity platform backend execution"
                                "${gpu_request_stdout}"
                                "intersection_backend_execution=${actual_backend}")
  tracing_parity_assert_matches("tracing parity platform GPU flags"
                                "${gpu_request_stdout}"
                                "intersection_backend_gpu_device=true.*intersection_backend_gpu_render_path=true")
else()
  tracing_parity_assert_matches("tracing parity packed CPU fallback selected"
                                "${gpu_request_stdout}"
                                "intersection_backend=cpu")
  tracing_parity_assert_matches("tracing parity packed CPU fallback availability"
                                "${gpu_request_stdout}"
                                "intersection_backend_availability=fallback")
  tracing_parity_assert_matches("tracing parity packed CPU fallback execution"
                                "${gpu_request_stdout}"
                                "intersection_backend_execution=packed_cpu.*closest_hit_execution=packed_cpu")
endif()
