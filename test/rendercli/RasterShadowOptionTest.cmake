if(NOT DEFINED RENDERCLI)
  message(FATAL_ERROR "RENDERCLI is required")
endif()

if(NOT DEFINED TEST_OUTPUT_DIR)
  message(FATAL_ERROR "TEST_OUTPUT_DIR is required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/RendercliTestHelpers.cmake")

file(REMOVE_RECURSE "${TEST_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${TEST_OUTPUT_DIR}")

set(shadow_scene "${PROJECT_SOURCE_DIR}/test/fixtures/rendercli/raster_shadow_caster.json")
set(invalid_render "${TEST_OUTPUT_DIR}/invalid.png")

set(unshadowed_render "${TEST_OUTPUT_DIR}/raster-shadows-off.png")
set(shadowed_render "${TEST_OUTPUT_DIR}/raster-shadows-on.png")
rendercli_run(
  NAME "rendercli raster renders deterministic shadow scene without shadow maps"
  COMMAND
    "${RENDERCLI}" --engine raster --width 64 --height 64
    "${shadow_scene}" "${unshadowed_render}"
)
rendercli_run(
  NAME "rendercli --shadow_maps renders deterministic shadow scene"
  COMMAND
    "${RENDERCLI}" --engine raster --width 64 --height 64 --shadow_maps
    --shadow_map_size 128 --shadow_bias 0.1
    "${shadow_scene}" "${shadowed_render}"
)
rendercli_assert_image_dimensions("${shadowed_render}" 64 64
                                  NAME "rendercli --shadow_maps dimensions")
rendercli_assert_image_nonempty("${shadowed_render}" NAME "rendercli --shadow_maps pixels")
rendercli_assert_image_hash_differs("${unshadowed_render}" "${shadowed_render}"
                                    NAME "rendercli --shadow_maps changes raster output")

set(ray_traced_shadow_render "${TEST_OUTPUT_DIR}/raster-shadows-ray-traced.png")
set(ray_traced_shadow_trace "${TEST_OUTPUT_DIR}/raster-shadows-ray-traced-trace.json")
rendercli_run(
  NAME "rendercli --shadow_mode ray_traced renders deterministic shadow scene"
  COMMAND
    "${RENDERCLI}" --engine raster --width 64 --height 64
    --shadow_mode ray_traced --wavefront_intersection_backend cpu
    --render_graph_trace_out "${ray_traced_shadow_trace}"
    "${shadow_scene}" "${ray_traced_shadow_render}"
)
rendercli_assert_image_dimensions("${ray_traced_shadow_render}" 64 64
                                  NAME "rendercli --shadow_mode ray_traced dimensions")
rendercli_assert_image_nonempty("${ray_traced_shadow_render}"
                                NAME "rendercli --shadow_mode ray_traced pixels")
rendercli_assert_image_hash_differs(
  "${unshadowed_render}" "${ray_traced_shadow_render}"
  NAME "rendercli --shadow_mode ray_traced changes raster output"
)
rendercli_assert_nonempty("${ray_traced_shadow_trace}"
                          NAME "rendercli --shadow_mode ray_traced trace output")
file(READ "${ray_traced_shadow_trace}" ray_traced_shadow_trace_json)
foreach(expectation IN ITEMS
    "\"id\"[ \r\n]*:[ \r\n]*\"hybrid_ray_traced_shadows\""
    "\"intersectionService\""
    "\"queryFamily\"[ \r\n]*:[ \r\n]*\"closest_hit\\+any_hit\""
    "\"queryTag\"[ \r\n]*:[ \r\n]*\"hybrid_shadows\""
    "\"requestedBackend\"[ \r\n]*:[ \r\n]*\"cpu\""
    "\"shadowQueryCount\"[ \r\n]*:[ \r\n]*[1-9][0-9]*(\\.[0-9]+)?"
    "\"occludedCount\"[ \r\n]*:[ \r\n]*[1-9][0-9]*(\\.[0-9]+)?"
    "\"anyHitExecutionPath\"[ \r\n]*:[ \r\n]*\"runtime_scene\""
)
  if(NOT ray_traced_shadow_trace_json MATCHES "${expectation}")
    message(FATAL_ERROR
      "rendercli --shadow_mode ray_traced trace did not contain ${expectation}: ${ray_traced_shadow_trace}"
    )
  endif()
endforeach()

foreach(cascade_count IN ITEMS 1 2 4)
  set(cascade_render "${TEST_OUTPUT_DIR}/raster-shadow-cascades-${cascade_count}.png")
  rendercli_run(
    NAME "rendercli --shadow_cascades ${cascade_count} is accepted"
    COMMAND
      "${RENDERCLI}" --engine raster --width 40 --height 40 --shadow_maps
      --shadow_map_size 64 --shadow_cascades "${cascade_count}" --shadow_bias 0.1
      "${shadow_scene}" "${cascade_render}"
  )
  rendercli_assert_image_dimensions("${cascade_render}" 40 40
                                    NAME "rendercli --shadow_cascades ${cascade_count} dimensions")
endforeach()

foreach(filter_mode IN ITEMS pcf pcss)
  set(filter_render "${TEST_OUTPUT_DIR}/raster-shadow-filter-${filter_mode}.png")
  rendercli_run(
    NAME "rendercli --shadow_filter ${filter_mode} is accepted"
    COMMAND
      "${RENDERCLI}" --engine raster --width 40 --height 40 --shadow_maps
      --shadow_map_size 64 --shadow_filter "${filter_mode}" --shadow_bias 0.1
      "${shadow_scene}" "${filter_render}"
  )
  rendercli_assert_image_dimensions("${filter_render}" 40 40
                                    NAME "rendercli --shadow_filter ${filter_mode} dimensions")
endforeach()

set(tuned_render "${TEST_OUTPUT_DIR}/raster-shadow-tuned.png")
rendercli_run(
  NAME "rendercli representative raster shadow options are accepted"
  COMMAND
    "${RENDERCLI}" --engine raster --width 48 --height 48 --shadow_maps
    --shadow_map_size 96
    --shadow_cascades 2
    --shadow_cascade_split 0.75
    --shadow_bias 0.1
    --shadow_slope_bias 0.02
    --shadow_filter_radius 2
    --shadow_filter pcf
    "${shadow_scene}" "${tuned_render}"
)
rendercli_assert_image_dimensions("${tuned_render}" 48 48
                                  NAME "rendercli representative shadow option dimensions")
rendercli_assert_image_nonempty("${tuned_render}" NAME "rendercli representative shadow pixels")

rendercli_expect_failure(
  NAME "rendercli rejects non-positive --shadow_map_size"
  STDERR_MATCHES "Shadow map size must be a positive integer"
  COMMAND
    "${RENDERCLI}" --engine raster --shadow_map_size 0
    "${shadow_scene}" "${invalid_render}"
)

rendercli_expect_failure(
  NAME "rendercli rejects non-positive --shadow_cascades"
  STDERR_MATCHES "Shadow cascade count must be a positive integer"
  COMMAND
    "${RENDERCLI}" --engine raster --shadow_cascades 0
    "${shadow_scene}" "${invalid_render}"
)

rendercli_expect_failure(
  NAME "rendercli rejects --shadow_cascade_split below range"
  STDERR_MATCHES "Shadow cascade split blend must be a number from 0 to 1"
  COMMAND
    "${RENDERCLI}" --engine raster --shadow_cascade_split -0.01
    "${shadow_scene}" "${invalid_render}"
)

rendercli_expect_failure(
  NAME "rendercli rejects --shadow_cascade_split above range"
  STDERR_MATCHES "Shadow cascade split blend must be a number from 0 to 1"
  COMMAND
    "${RENDERCLI}" --engine raster --shadow_cascade_split 1.01
    "${shadow_scene}" "${invalid_render}"
)

rendercli_expect_failure(
  NAME "rendercli rejects negative --shadow_bias"
  STDERR_MATCHES "Shadow bias must be a non-negative number"
  COMMAND
    "${RENDERCLI}" --engine raster --shadow_bias -0.001
    "${shadow_scene}" "${invalid_render}"
)

rendercli_expect_failure(
  NAME "rendercli rejects negative --shadow_slope_bias"
  STDERR_MATCHES "Shadow slope bias must be a non-negative number"
  COMMAND
    "${RENDERCLI}" --engine raster --shadow_slope_bias -0.001
    "${shadow_scene}" "${invalid_render}"
)

rendercli_expect_failure(
  NAME "rendercli rejects negative --shadow_filter_radius"
  STDERR_MATCHES "Shadow filter radius must be a non-negative integer"
  COMMAND
    "${RENDERCLI}" --engine raster --shadow_filter_radius -1
    "${shadow_scene}" "${invalid_render}"
)

rendercli_expect_failure(
  NAME "rendercli rejects invalid --shadow_filter"
  STDERR_MATCHES "Shadow filter must be 'pcf' or 'pcss'"
  COMMAND
    "${RENDERCLI}" --engine raster --shadow_filter variance
    "${shadow_scene}" "${invalid_render}"
)

rendercli_expect_failure(
  NAME "rendercli rejects invalid --shadow_mode"
  STDERR_MATCHES "Shadow mode must be 'shadow_maps' or 'ray_traced'"
  COMMAND
    "${RENDERCLI}" --engine raster --shadow_mode variance
    "${shadow_scene}" "${invalid_render}"
)
