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

function(render_whitted_fallback_parity category scene depth)
  set(cpu_render "${TEST_OUTPUT_DIR}/whitted-${category}-cpu.png")
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
    NAME "rendercli Whitted tracing parity ${category} CPU fallback baseline"
    COMMAND
      "${RENDERCLI}" ${common_whitted_args}
      --wavefront_intersection_backend cpu
      "${scene}" "${cpu_render}"
  )
  rendercli_assert_image_dimensions(
    "${cpu_render}" 32 24 NAME "Whitted tracing parity ${category} CPU fallback dimensions")
  rendercli_assert_image_nonempty("${cpu_render}"
                                  NAME "Whitted tracing parity ${category} CPU fallback pixels")

  rendercli_run(
    NAME "rendercli Whitted tracing parity ${category} GPU-requested fallback"
    OUTPUT_VARIABLE whitted_fallback_stdout
    COMMAND
      "${RENDERCLI}" ${common_whitted_args}
      --wavefront_intersection_backend gpu
      --wavefront_metrics_out "${gpu_request_metrics}"
      --wavefront_metrics_summary
      "${scene}" "${gpu_request_render}"
  )
  rendercli_assert_image_dimensions(
    "${gpu_request_render}" 32 24
    NAME "Whitted tracing parity ${category} GPU-requested fallback dimensions")
  rendercli_assert_image_nonempty(
    "${gpu_request_render}" NAME "Whitted tracing parity ${category} GPU-requested fallback pixels")
  rendercli_assert_image_rms_at_most(
    "${cpu_render}" "${gpu_request_render}" 0.001
    NAME "Whitted tracing parity ${category} GPU-requested fallback RMS matches CPU")

  foreach(expectation
          "integrator=whitted"
          "execution=depth_major_whitted"
          "intersection_backend_request=gpu"
          "intersection_backend=cpu"
          "intersection_backend_availability=fallback"
          "intersection_backend_fallback=GPU_intersection_scene_unsupported"
          "intersection_backend_execution=runtime_scene"
          "closest_hit_execution=runtime_scene"
          "intersection_scene_compiled=true"
          "intersection_scene_unsupported=[1-9][0-9]*"
          "intersection_scene_unsupported_by_reason=transparent_material_requires_runtime_intersection_for_Whitted_continuation_precision:[1-9][0-9]*")
    tracing_parity_assert_matches("Whitted tracing parity ${category} fallback ${expectation}"
                                  "${whitted_fallback_stdout}" "${expectation}")
  endforeach()

  foreach(expectation
          "\"intersectionBackendRequest\"[ \r\n]*:[ \r\n]*\"gpu\""
          "\"intersectionBackend\"[ \r\n]*:[ \r\n]*\"cpu\""
          "\"intersectionBackendAvailability\"[ \r\n]*:[ \r\n]*\"fallback\""
          "\"intersectionBackendFallbackReason\"[ \r\n]*:[ \r\n]*\"GPU intersection scene unsupported"
          "\"intersectionBackendExecutionPath\"[ \r\n]*:[ \r\n]*\"runtime_scene\""
          "\"intersectionBackendClosestHitExecutionPath\"[ \r\n]*:[ \r\n]*\"runtime_scene\""
          "\"intersectionSceneCompiled\"[ \r\n]*:[ \r\n]*true"
          "\"intersectionSceneUnsupportedPrimitives\"[ \r\n]*:[ \r\n]*[1-9][0-9]*")
    tracing_parity_assert_json_matches(
      "Whitted tracing parity ${category} fallback metrics ${expectation}"
      "${gpu_request_metrics}" "${expectation}")
  endforeach()
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

function(tracing_parity_render_unsupported category scene_file depth samples)
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

  rendercli_run(
    NAME "rendercli tracing parity ${category} explicit GPU fallback"
    OUTPUT_VARIABLE gpu_request_stdout
    STDOUT_MATCHES
      "wavefront_metrics.*integrator=pathtracer.*execution=depth_major_paths.*tracing_backend=cpu.*tracing_backend_mode=wavefront_intersection.*tracing_backend_fallback=GPU_intersection_scene_unsupported.*intersection_backend_request=gpu.*intersection_backend=cpu.*intersection_backend_availability=fallback.*intersection_backend_fallback=GPU_intersection_scene_unsupported.*intersection_backend_execution=runtime_scene.*closest_hit_execution=runtime_scene.*intersection_scene_compiled=true.*intersection_scene_unsupported=[1-9][0-9]*.*intersection_scene_unsupported_by_reason=.*transparent_material_requires_runtime_intersection_for_Whitted_continuation_precision"
    COMMAND
      "${RENDERCLI}" ${common_render_args}
      --wavefront_intersection_backend gpu
      --wavefront_metrics_out "${gpu_request_metrics}"
      --wavefront_metrics_summary
      "${scene_path}" "${gpu_request_render}"
  )
  rendercli_assert_image_dimensions("${gpu_request_render}" 32 24
                                    NAME "tracing parity ${category} fallback dimensions")
  rendercli_assert_image_nonempty("${gpu_request_render}"
                                  NAME "tracing parity ${category} fallback pixels")
  rendercli_assert_image_rms_at_most(
    "${cpu_render}" "${gpu_request_render}" 0.001
    NAME "tracing parity ${category} fallback image RMS matches CPU")

  foreach(expectation
          "\"intersectionBackendRequest\"[ \r\n]*:[ \r\n]*\"gpu\""
          "\"intersectionBackend\"[ \r\n]*:[ \r\n]*\"cpu\""
          "\"intersectionBackendAvailability\"[ \r\n]*:[ \r\n]*\"fallback\""
          "\"intersectionBackendFallbackReason\"[ \r\n]*:[ \r\n]*\"GPU intersection scene unsupported: .*transparent material requires runtime intersection for Whitted continuation precision\""
          "\"intersectionBackendExecutionPath\"[ \r\n]*:[ \r\n]*\"runtime_scene\""
          "\"intersectionSceneUnsupportedPrimitives\"[ \r\n]*:[ \r\n]*[1-9][0-9]*"
          "\"tracingBackend\"[ \r\n]*:[ \r\n]*\"cpu\""
          "\"tracingBackendMode\"[ \r\n]*:[ \r\n]*\"wavefront_intersection\""
          "\"tracingBackendFallback\"[ \r\n]*:[ \r\n]*\\{"
          "\"tracingSceneUnsupportedMaterials\"[ \r\n]*:[ \r\n]*[1-9][0-9]*"
          "\"tracingSceneUnsupportedTextures\"[ \r\n]*:[ \r\n]*[1-9][0-9]*")
    tracing_parity_assert_json_matches(
      "tracing parity ${category} fallback metrics ${expectation}"
      "${gpu_request_metrics}" "${expectation}")
  endforeach()
endfunction()

tracing_parity_render_supported(
  "matte_direct_light" "matte_direct_light.json" 1 1 0.001
  "direct_light_samples=[1-9][0-9]*")
tracing_parity_render_supported(
  "indirect_bounce" "indirect_bounce.json" 3 4 0.02
  "secondary_direct_light_luminance=[1-9]")
tracing_parity_render_supported(
  "environment_miss" "environment_miss.json" 2 1 0.001
  "tracing_scene_environment=1.*miss_luminance=[1-9]")
tracing_parity_render_unsupported(
  "transparent_fallback" "transparent_fallback.json" 4 1)
render_whitted_gpu_parity(
  "matte-direct-light" "${PROJECT_SOURCE_DIR}/test/fixtures/tracing_parity/matte_direct_light.json"
  2)
render_whitted_gpu_parity(
  "reflection" "${PROJECT_SOURCE_DIR}/test/fixtures/tracing_parity/whitted_reflection.json" 3
  0.01)
render_whitted_fallback_parity(
  "transparent-fallback"
  "${PROJECT_SOURCE_DIR}/test/fixtures/tracing_parity/transparent_fallback.json" 4)
