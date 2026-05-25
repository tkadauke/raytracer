if(NOT DEFINED RENDERCLI)
  message(FATAL_ERROR "RENDERCLI is required")
endif()

if(NOT DEFINED TEST_OUTPUT_DIR)
  message(FATAL_ERROR "TEST_OUTPUT_DIR is required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/RendercliTestHelpers.cmake")

file(REMOVE_RECURSE "${TEST_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${TEST_OUTPUT_DIR}")

set(static_scene "${PROJECT_SOURCE_DIR}/scenes/dice.json")
set(animated_scene "${PROJECT_SOURCE_DIR}/scenes/animation_frame_demo.json")
set(static_frame "${TEST_OUTPUT_DIR}/static-frame.png")
set(static_animation_pattern "${TEST_OUTPUT_DIR}/static-frame-%04d.png")
set(animation_frame_pattern "${TEST_OUTPUT_DIR}/animation-frame-%04d.png")
set(animation_repeat_pattern "${TEST_OUTPUT_DIR}/animation-repeat-%04d.png")
set(animation_graph_only "${TEST_OUTPUT_DIR}/animation-graph-only.txt")
set(animation_graph_out_pattern "${TEST_OUTPUT_DIR}/animation-graph-out-%04d.png")
set(animation_graph_out "${TEST_OUTPUT_DIR}/animation-graph-out.json")
set(raytracer_baseline "${TEST_OUTPUT_DIR}/raytracer-baseline.png")
set(raytracer_raster_flags "${TEST_OUTPUT_DIR}/raytracer-raster-flags.png")
set(wireframe_baseline "${TEST_OUTPUT_DIR}/wireframe-baseline.png")
set(wireframe_raster_flags "${TEST_OUTPUT_DIR}/wireframe-raster-flags.png")

set(raster_only_flags
  --cull front
  --msaa 8
  --msaa_shading per_fragment
  --post_aa taa
  --color_write_mask none
  --blend
  --blend_src src_alpha
  --blend_dst one_minus_source_alpha
  --blend_op reverse_subtract
  --blend_constant_color 0.1,0.2,0.3
  --blend_constant_alpha 0.4
  --alpha_test
  --alpha_func greater_equal
  --alpha_ref 0.25
  --viewport 1,1,14,14
  --scissor 2,2,12,12
  --depth_bias 0.5
  --shadow_maps
  --shadow_map_size 128
  --shadow_cascades 2
  --shadow_cascade_split 0.25
  --shadow_bias 0.002
  --shadow_slope_bias 0.003
  --shadow_filter_radius 2
  --shadow_filter pcss
)

rendercli_run(
  NAME "rendercli --frame renders a static scene"
  COMMAND
    "${RENDERCLI}" --engine wireframe --width 32 --height 32 --frame 5
    "${static_scene}" "${static_frame}"
)
rendercli_assert_image_dimensions("${static_frame}" 32 32
                                  NAME "rendercli static --frame dimensions")
rendercli_assert_image_nonempty("${static_frame}" NAME "rendercli static --frame pixels")

rendercli_expect_failure(
  NAME "rendercli rejects --animation for a static scene"
  STDERR_MATCHES "requires a scene animation block"
  COMMAND
    "${RENDERCLI}" --engine wireframe --width 16 --height 16 --animation
    "${static_scene}" "${static_animation_pattern}"
)

rendercli_expect_failure(
  NAME "rendercli rejects --animation with --frame"
  STDERR_MATCHES "Cannot combine --animation with --frame"
  COMMAND
    "${RENDERCLI}" --engine wireframe --width 16 --height 16 --animation --frame 3
    "${animated_scene}" "${animation_frame_pattern}"
)

rendercli_expect_failure(
  NAME "rendercli rejects --animation with --repeat"
  STDERR_MATCHES "Cannot combine --animation with --repeat"
  COMMAND
    "${RENDERCLI}" --engine wireframe --width 16 --height 16 --animation --repeat 2
    "${animated_scene}" "${animation_repeat_pattern}"
)

rendercli_expect_failure(
  NAME "rendercli rejects --animation with --render_graph_only"
  STDERR_MATCHES "Cannot combine --animation with --render_graph_only"
  COMMAND
    "${RENDERCLI}" --animation --render_graph_only
    "${animated_scene}" "${animation_graph_only}"
)

rendercli_expect_failure(
  NAME "rendercli rejects --animation with --render_graph_out"
  STDERR_MATCHES "Cannot combine --animation with --render_graph_out"
  COMMAND
    "${RENDERCLI}" --engine wireframe --width 16 --height 16 --animation
    --render_graph_out "${animation_graph_out}"
    "${animated_scene}" "${animation_graph_out_pattern}"
)

rendercli_run(
  NAME "rendercli direct raytracer baseline"
  COMMAND
    "${RENDERCLI}" --direct_engine --engine raytracer --width 32 --height 32
    "${static_scene}" "${raytracer_baseline}"
)

rendercli_run(
  NAME "rendercli accepts raster-only flags as inert with direct raytracer"
  COMMAND
    "${RENDERCLI}" --direct_engine --engine raytracer --width 32 --height 32
    ${raster_only_flags}
    "${static_scene}" "${raytracer_raster_flags}"
)
rendercli_assert_image_hash_equals("${raytracer_baseline}" "${raytracer_raster_flags}"
                                   NAME "direct raytracer ignores raster-only flags")

rendercli_run(
  NAME "rendercli direct wireframe baseline"
  COMMAND
    "${RENDERCLI}" --direct_engine --engine wireframe --width 32 --height 32
    "${static_scene}" "${wireframe_baseline}"
)

rendercli_run(
  NAME "rendercli accepts raster-only flags as inert with direct wireframe"
  COMMAND
    "${RENDERCLI}" --direct_engine --engine wireframe --width 32 --height 32
    ${raster_only_flags}
    "${static_scene}" "${wireframe_raster_flags}"
)
rendercli_assert_image_hash_equals("${wireframe_baseline}" "${wireframe_raster_flags}"
                                   NAME "direct wireframe ignores raster-only flags")
