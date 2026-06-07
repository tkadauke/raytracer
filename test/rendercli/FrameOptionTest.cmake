if(NOT DEFINED RENDERCLI)
  message(FATAL_ERROR "RENDERCLI is required")
endif()

if(NOT DEFINED TEST_OUTPUT_DIR)
  message(FATAL_ERROR "TEST_OUTPUT_DIR is required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/RendercliTestHelpers.cmake")

file(REMOVE_RECURSE "${TEST_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${TEST_OUTPUT_DIR}")

set(animated_scene "${PROJECT_SOURCE_DIR}/scenes/animation_frame_demo.json")
set(runtime_track_scene "${PROJECT_SOURCE_DIR}/scenes/animated_runtime_translation_parity.json")
set(runtime_track_frame1_static_scene
    "${PROJECT_SOURCE_DIR}/scenes/runtime_translation_frame1_static.json")
set(step_scene "${PROJECT_SOURCE_DIR}/scenes/step_playback_demo.json")
set(static_scene "${PROJECT_SOURCE_DIR}/scenes/dice.json")
set(frame_1 "${TEST_OUTPUT_DIR}/frame_0001.png")
set(frame_48 "${TEST_OUTPUT_DIR}/frame_0048.png")
set(runtime_track_frame1 "${TEST_OUTPUT_DIR}/runtime_track_frame1.png")
set(runtime_track_frame1_static "${TEST_OUTPUT_DIR}/runtime_track_frame1_static.png")
set(runtime_track_frame2_single_sample "${TEST_OUTPUT_DIR}/runtime_track_frame2_single_sample.png")
set(runtime_track_frame2_shutter_samples "${TEST_OUTPUT_DIR}/runtime_track_frame2_shutter_samples.png")
set(static_frame "${TEST_OUTPUT_DIR}/static_frame.png")
set(invalid_frame "${TEST_OUTPUT_DIR}/invalid_frame.png")
set(sequence_dir "${TEST_OUTPUT_DIR}/sequence")
set(sequence_pattern "${sequence_dir}/frame_%04d.png")
set(missing_placeholder_pattern "${sequence_dir}/frame.png")
set(unsigned_placeholder_pattern "${sequence_dir}/frame_%04u.png")
set(integer_placeholder_pattern "${sequence_dir}/integer_%d.png")
set(i_placeholder_pattern "${sequence_dir}/i_%04i.png")
set(multiple_placeholder_pattern "${sequence_dir}/multiple_%04d_%04i.png")
set(incomplete_placeholder_pattern "${sequence_dir}/incomplete_%")
set(invalid_range_pattern "${sequence_dir}/invalid_%04d.png")
set(static_animation_pattern "${sequence_dir}/static_%04d.png")
set(graph_output_plan "${sequence_dir}/animation-graph.json")
set(step_default "${TEST_OUTPUT_DIR}/step_default.png")
set(step_two "${TEST_OUTPUT_DIR}/step_two.png")
set(step_two_highlight "${TEST_OUTPUT_DIR}/step_two_highlight.png")
set(step_two_ghost "${TEST_OUTPUT_DIR}/step_two_ghost.png")
set(invalid_step "${TEST_OUTPUT_DIR}/invalid_step.png")

file(MAKE_DIRECTORY "${sequence_dir}")

rendercli_run(
  NAME "rendercli --frame renders a static scene"
  COMMAND
    "${RENDERCLI}" --engine wireframe --width 32 --height 32 --frame 5
    "${static_scene}" "${static_frame}"
)
rendercli_assert_image_dimensions("${static_frame}" 32 32
                                  NAME "rendercli --frame static scene dimensions")
rendercli_assert_image_nonempty("${static_frame}"
                                NAME "rendercli --frame static scene pixels")

rendercli_run(
  NAME "rendercli --frame 1 renders animated scene"
  COMMAND
    "${RENDERCLI}" --engine wireframe --width 64 --height 64 --frame 1
    "${animated_scene}" "${frame_1}"
)

rendercli_run(
  NAME "rendercli --frame 48 renders animated scene"
  COMMAND
    "${RENDERCLI}" --engine wireframe --width 64 --height 64 --frame 48
    "${animated_scene}" "${frame_48}"
)
rendercli_assert_image_dimensions("${frame_1}" 64 64
                                  NAME "rendercli --frame 1 dimensions")
rendercli_assert_image_dimensions("${frame_48}" 64 64
                                  NAME "rendercli --frame 48 dimensions")
rendercli_assert_image_hash_differs("${frame_1}" "${frame_48}"
                                    NAME "animated frame renders differ")

rendercli_run(
  NAME "rendercli --frame preserves runtime-track integer-frame parity"
  COMMAND
    "${RENDERCLI}" --engine raytracer --width 64 --height 48 --samples_per_pixel 1
    --frame 1 "${runtime_track_scene}" "${runtime_track_frame1}"
)
rendercli_run(
  NAME "rendercli renders static parity scene"
  COMMAND
    "${RENDERCLI}" --engine raytracer --width 64 --height 48 --samples_per_pixel 1
    "${runtime_track_frame1_static_scene}" "${runtime_track_frame1_static}"
)
rendercli_assert_image_dimensions("${runtime_track_frame1}" 64 48
                                  NAME "runtime track frame parity dimensions")
rendercli_assert_image_hash_equals("${runtime_track_frame1_static}" "${runtime_track_frame1}"
                                   NAME "runtime-continuous integer frame matches baked frame")

rendercli_run(
  NAME "rendercli --frame renders runtime-track shutter single sample"
  COMMAND
    "${RENDERCLI}" --engine raytracer --width 64 --height 48 --samples_per_pixel 1
    --frame 2 "${runtime_track_scene}" "${runtime_track_frame2_single_sample}"
)
rendercli_run(
  NAME "rendercli --frame renders runtime-track shutter samples"
  COMMAND
    "${RENDERCLI}" --engine raytracer --width 64 --height 48 --samples_per_pixel 16
    --frame 2 "${runtime_track_scene}" "${runtime_track_frame2_shutter_samples}"
)
rendercli_assert_image_hash_differs("${runtime_track_frame2_single_sample}"
                                    "${runtime_track_frame2_shutter_samples}"
                                    NAME "runtime-continuous shutter samples change render")

rendercli_run(
  NAME "rendercli default step playback scene renders normally"
  COMMAND
    "${RENDERCLI}" --engine raster --width 64 --height 48
    "${step_scene}" "${step_default}"
)
rendercli_run(
  NAME "rendercli --step filters active step"
  COMMAND
    "${RENDERCLI}" --engine raster --width 64 --height 48 --step 2
    "${step_scene}" "${step_two}"
)
rendercli_run(
  NAME "rendercli --step_highlight changes active step material"
  COMMAND
    "${RENDERCLI}" --engine raster --width 64 --height 48 --step 2 --step_highlight
    "${step_scene}" "${step_two_highlight}"
)
rendercli_run(
  NAME "rendercli --step_ghost_previous keeps earlier groups visible"
  COMMAND
    "${RENDERCLI}" --engine raster --width 64 --height 48 --step 2 --step_highlight
    --step_ghost_previous "${step_scene}" "${step_two_ghost}"
)
rendercli_assert_image_nonempty("${step_default}"
                                NAME "step playback normal output pixels")
rendercli_assert_image_nonempty("${step_two_highlight}"
                                NAME "step playback highlighted output pixels")
rendercli_assert_image_hash_differs("${step_default}" "${step_two}"
                                    NAME "step filter differs from normal output")
rendercli_assert_image_hash_differs("${step_two}" "${step_two_highlight}"
                                    NAME "step highlight differs from unstyled step output")
rendercli_assert_image_hash_differs("${step_two_highlight}" "${step_two_ghost}"
                                    NAME "step ghost differs from active-only highlight")

set(animated_catalog
  animated_camera_pan.json
  animated_light_sweep.json
  animated_material_fade.json
  animated_motion_blur_sweep.json
  animated_visibility_steps.json
)
foreach(scene_name IN LISTS animated_catalog)
  string(REPLACE ".json" "" scene_stem "${scene_name}")
  foreach(frame IN ITEMS 1 72)
    set(catalog_frame "${TEST_OUTPUT_DIR}/${scene_stem}_${frame}.png")
    rendercli_run(
      NAME "rendercli --frame ${frame} renders ${scene_name}"
      COMMAND
        "${RENDERCLI}" --engine raster --width 48 --height 32 --frame "${frame}"
        "${PROJECT_SOURCE_DIR}/scenes/${scene_name}" "${catalog_frame}"
    )
    rendercli_assert_image_dimensions("${catalog_frame}" 48 32
                                      NAME "rendercli --frame ${frame} dimensions for ${scene_name}")
    rendercli_assert_image_nonempty("${catalog_frame}"
                                    NAME "rendercli --frame ${frame} pixels for ${scene_name}")
  endforeach()
endforeach()

rendercli_expect_failure(
  NAME "rendercli rejects non-integer --frame"
  STDERR_MATCHES "Frame must be an integer"
  COMMAND
    "${RENDERCLI}" --frame not-an-integer
    "${static_scene}" "${invalid_frame}"
)
rendercli_assert_not_exists("${invalid_frame}" NAME "invalid --frame output")

rendercli_expect_failure(
  NAME "rendercli rejects non-integer --step"
  STDERR_MATCHES "Step must be an integer"
  COMMAND
    "${RENDERCLI}" --step not-an-integer
    "${step_scene}" "${invalid_step}"
)
rendercli_assert_not_exists("${invalid_step}" NAME "invalid --step output")

rendercli_expect_failure(
  NAME "rendercli rejects step visuals without --step"
  STDERR_MATCHES "Step playback visual modes require --step"
  COMMAND
    "${RENDERCLI}" --step_highlight
    "${step_scene}" "${invalid_step}"
)

rendercli_expect_failure(
  NAME "rendercli rejects non-integer --frame_start"
  STDERR_MATCHES "Frame start must be an integer"
  COMMAND
    "${RENDERCLI}" --animation --frame_start early
    "${animated_scene}" "${sequence_pattern}"
)

rendercli_expect_failure(
  NAME "rendercli rejects non-integer --frame_end"
  STDERR_MATCHES "Frame end must be an integer"
  COMMAND
    "${RENDERCLI}" --animation --frame_end late
    "${animated_scene}" "${sequence_pattern}"
)

rendercli_expect_failure(
  NAME "rendercli rejects non-positive --fps"
  STDERR_MATCHES "FPS must be a positive number"
  COMMAND
    "${RENDERCLI}" --animation --fps 0
    "${animated_scene}" "${sequence_pattern}"
)

rendercli_expect_failure(
  NAME "rendercli rejects non-numeric --fps"
  STDERR_MATCHES "FPS must be a positive number"
  COMMAND
    "${RENDERCLI}" --animation --fps fast
    "${animated_scene}" "${sequence_pattern}"
)

rendercli_expect_failure(
  NAME "rendercli rejects --animation with --frame"
  STDERR_MATCHES "Cannot combine --animation with --frame"
  COMMAND
    "${RENDERCLI}" --animation --frame 1
    "${animated_scene}" "${sequence_pattern}"
)

rendercli_expect_failure(
  NAME "rendercli rejects --animation with --repeat"
  STDERR_MATCHES "Cannot combine --animation with --repeat"
  COMMAND
    "${RENDERCLI}" --animation --repeat 2
    "${animated_scene}" "${sequence_pattern}"
)

rendercli_expect_failure(
  NAME "rendercli rejects --animation with graph-only export"
  STDERR_MATCHES "Cannot combine --animation with --render_graph_only"
  COMMAND
    "${RENDERCLI}" --animation --render_graph_only
    "${animated_scene}" "${sequence_pattern}"
)

rendercli_expect_failure(
  NAME "rendercli rejects --animation with graph output"
  STDERR_MATCHES "Cannot combine --animation with --render_graph_out"
  COMMAND
    "${RENDERCLI}" --animation --render_graph_out "${graph_output_plan}"
    "${animated_scene}" "${sequence_pattern}"
)

rendercli_run(
  NAME "rendercli --animation renders selected frame range"
  STDOUT_MATCHES "frame 1/3 number=2"
  COMMAND
    "${RENDERCLI}" --engine wireframe --width 64 --height 64 --animation
    --frame_start 2 --frame_end 4 --fps 12
    "${animated_scene}" "${sequence_pattern}"
)
foreach(frame 0002 0003 0004)
  rendercli_assert_image_dimensions("${sequence_dir}/frame_${frame}.png" 64 64
                                    NAME "rendercli --animation frame_${frame}.png dimensions")
  rendercli_assert_image_nonempty("${sequence_dir}/frame_${frame}.png"
                                  NAME "rendercli --animation frame_${frame}.png pixels")
endforeach()
rendercli_assert_not_exists("${sequence_dir}/frame_0001.png"
                            NAME "rendercli --animation honors --frame_start")
rendercli_assert_not_exists("${sequence_dir}/frame_0005.png"
                            NAME "rendercli --animation honors --frame_end")

rendercli_run(
  NAME "rendercli --animation accepts percent d placeholder"
  STDOUT_MATCHES "frame 1/1 number=2"
  COMMAND
    "${RENDERCLI}" --engine wireframe --width 16 --height 16 --animation
    --frame_start 2 --frame_end 2
    "${animated_scene}" "${integer_placeholder_pattern}"
)
rendercli_assert_image_dimensions("${sequence_dir}/integer_2.png" 16 16
                                  NAME "rendercli --animation %d dimensions")

rendercli_run(
  NAME "rendercli --animation accepts percent i placeholder"
  STDOUT_MATCHES "frame 1/1 number=3"
  COMMAND
    "${RENDERCLI}" --engine wireframe --width 16 --height 16 --animation
    --frame_start 3 --frame_end 3
    "${animated_scene}" "${i_placeholder_pattern}"
)
rendercli_assert_image_dimensions("${sequence_dir}/i_0003.png" 16 16
                                  NAME "rendercli --animation %04i dimensions")

rendercli_expect_failure(
  NAME "rendercli --animation rejects missing frame placeholder"
  STDERR_MATCHES "printf-style signed integer placeholder"
  COMMAND
    "${RENDERCLI}" --engine wireframe --width 16 --height 16 --animation
    "${animated_scene}" "${missing_placeholder_pattern}"
)

rendercli_expect_failure(
  NAME "rendercli --animation rejects unsigned frame placeholder"
  STDERR_MATCHES "printf-style signed integer placeholder"
  COMMAND
    "${RENDERCLI}" --engine wireframe --width 16 --height 16 --animation
    "${animated_scene}" "${unsigned_placeholder_pattern}"
)

rendercli_expect_failure(
  NAME "rendercli --animation rejects multiple frame placeholders"
  STDERR_MATCHES "exactly one printf-style signed integer placeholder"
  COMMAND
    "${RENDERCLI}" --engine wireframe --width 16 --height 16 --animation
    "${animated_scene}" "${multiple_placeholder_pattern}"
)

rendercli_expect_failure(
  NAME "rendercli --animation rejects incomplete frame placeholder"
  STDERR_MATCHES "incomplete printf placeholder"
  COMMAND
    "${RENDERCLI}" --engine wireframe --width 16 --height 16 --animation
    "${animated_scene}" "${incomplete_placeholder_pattern}"
)

rendercli_expect_failure(
  NAME "rendercli --animation rejects invalid frame range"
  STDERR_MATCHES "Frame end must be greater than or equal to frame start"
  COMMAND
    "${RENDERCLI}" --engine wireframe --width 16 --height 16 --animation
    --frame_start 4 --frame_end 2
    "${animated_scene}" "${invalid_range_pattern}"
)

rendercli_expect_failure(
  NAME "rendercli --animation rejects static scene"
  STDERR_MATCHES "requires a scene animation block"
  COMMAND
    "${RENDERCLI}" --engine wireframe --width 16 --height 16 --animation
    "${static_scene}" "${static_animation_pattern}"
)
