if(NOT DEFINED RENDERCLI)
  message(FATAL_ERROR "RENDERCLI is required")
endif()

if(NOT DEFINED TEST_OUTPUT_DIR)
  message(FATAL_ERROR "TEST_OUTPUT_DIR is required")
endif()

file(REMOVE_RECURSE "${TEST_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${TEST_OUTPUT_DIR}")

set(animated_scene "${PROJECT_SOURCE_DIR}/scenes/animation_frame_demo.json")
set(static_scene "${PROJECT_SOURCE_DIR}/scenes/dice.json")
set(frame_1 "${TEST_OUTPUT_DIR}/frame_0001.png")
set(frame_48 "${TEST_OUTPUT_DIR}/frame_0048.png")
set(static_frame "${TEST_OUTPUT_DIR}/static_frame.png")
set(invalid_frame "${TEST_OUTPUT_DIR}/invalid_frame.png")
set(sequence_dir "${TEST_OUTPUT_DIR}/sequence")
set(sequence_pattern "${sequence_dir}/frame_%04d.png")
set(missing_placeholder_pattern "${sequence_dir}/frame.png")
set(unsigned_placeholder_pattern "${sequence_dir}/frame_%04u.png")
set(invalid_range_pattern "${sequence_dir}/invalid_%04d.png")
set(static_animation_pattern "${sequence_dir}/static_%04d.png")

file(MAKE_DIRECTORY "${sequence_dir}")

function(run_rendercli output_variable error_variable result_variable)
  execute_process(
    COMMAND ${ARGN}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
  )
  set(${output_variable} "${stdout}" PARENT_SCOPE)
  set(${error_variable} "${stderr}" PARENT_SCOPE)
  set(${result_variable} "${result}" PARENT_SCOPE)
endfunction()

run_rendercli(
  static_stdout
  static_stderr
  static_result
  "${RENDERCLI}" --engine wireframe --width 32 --height 32 --frame 5
  "${static_scene}" "${static_frame}"
)
if(NOT static_result EQUAL 0)
  message(FATAL_ERROR "rendercli --frame failed for static scene: ${static_stderr}")
endif()
if(NOT EXISTS "${static_frame}")
  message(FATAL_ERROR "rendercli --frame did not create static output")
endif()

run_rendercli(
  frame_1_stdout
  frame_1_stderr
  frame_1_result
  "${RENDERCLI}" --engine wireframe --width 64 --height 64 --frame 1
  "${animated_scene}" "${frame_1}"
)
if(NOT frame_1_result EQUAL 0)
  message(FATAL_ERROR "rendercli --frame 1 failed: ${frame_1_stderr}")
endif()

run_rendercli(
  frame_48_stdout
  frame_48_stderr
  frame_48_result
  "${RENDERCLI}" --engine wireframe --width 64 --height 64 --frame 48
  "${animated_scene}" "${frame_48}"
)
if(NOT frame_48_result EQUAL 0)
  message(FATAL_ERROR "rendercli --frame 48 failed: ${frame_48_stderr}")
endif()

execute_process(
  COMMAND ${CMAKE_COMMAND} -E compare_files "${frame_1}" "${frame_48}"
  RESULT_VARIABLE frame_compare
)
if(frame_compare EQUAL 0)
  message(FATAL_ERROR "animated frame renders were unexpectedly identical")
endif()

run_rendercli(
  invalid_stdout
  invalid_stderr
  invalid_result
  "${RENDERCLI}" --frame not-an-integer
  "${static_scene}" "${invalid_frame}"
)
if(invalid_result EQUAL 0)
  message(FATAL_ERROR "rendercli accepted a non-integer --frame value")
endif()
if(NOT invalid_stderr MATCHES "Frame must be an integer")
  message(FATAL_ERROR "rendercli reported an unexpected invalid-frame error: ${invalid_stderr}")
endif()

run_rendercli(
  animation_stdout
  animation_stderr
  animation_result
  "${RENDERCLI}" --engine wireframe --width 64 --height 64 --animation
  --frame_start 2 --frame_end 4 --fps 12
  "${animated_scene}" "${sequence_pattern}"
)
if(NOT animation_result EQUAL 0)
  message(FATAL_ERROR "rendercli --animation failed: ${animation_stderr}")
endif()
foreach(frame 0002 0003 0004)
  if(NOT EXISTS "${sequence_dir}/frame_${frame}.png")
    message(FATAL_ERROR "rendercli --animation did not create frame_${frame}.png")
  endif()
endforeach()
if(EXISTS "${sequence_dir}/frame_0001.png")
  message(FATAL_ERROR "rendercli --animation ignored --frame_start")
endif()
if(EXISTS "${sequence_dir}/frame_0005.png")
  message(FATAL_ERROR "rendercli --animation ignored --frame_end")
endif()
if(NOT animation_stdout MATCHES "frame 1/3 number=2")
  message(FATAL_ERROR "rendercli --animation did not print expected progress: ${animation_stdout}")
endif()

run_rendercli(
  missing_placeholder_stdout
  missing_placeholder_stderr
  missing_placeholder_result
  "${RENDERCLI}" --engine wireframe --width 16 --height 16 --animation
  "${animated_scene}" "${missing_placeholder_pattern}"
)
if(missing_placeholder_result EQUAL 0)
  message(FATAL_ERROR "rendercli --animation accepted an output path without a frame placeholder")
endif()
if(NOT missing_placeholder_stderr MATCHES "printf-style signed integer placeholder")
  message(FATAL_ERROR "rendercli reported an unexpected missing-placeholder error: ${missing_placeholder_stderr}")
endif()

run_rendercli(
  unsigned_placeholder_stdout
  unsigned_placeholder_stderr
  unsigned_placeholder_result
  "${RENDERCLI}" --engine wireframe --width 16 --height 16 --animation
  "${animated_scene}" "${unsigned_placeholder_pattern}"
)
if(unsigned_placeholder_result EQUAL 0)
  message(FATAL_ERROR "rendercli --animation accepted an unsupported unsigned placeholder")
endif()
if(NOT unsigned_placeholder_stderr MATCHES "printf-style signed integer placeholder")
  message(FATAL_ERROR "rendercli reported an unexpected unsigned-placeholder error: ${unsigned_placeholder_stderr}")
endif()

run_rendercli(
  invalid_range_stdout
  invalid_range_stderr
  invalid_range_result
  "${RENDERCLI}" --engine wireframe --width 16 --height 16 --animation
  --frame_start 4 --frame_end 2
  "${animated_scene}" "${invalid_range_pattern}"
)
if(invalid_range_result EQUAL 0)
  message(FATAL_ERROR "rendercli --animation accepted an invalid frame range")
endif()
if(NOT invalid_range_stderr MATCHES "Frame end must be greater than or equal to frame start")
  message(FATAL_ERROR "rendercli reported an unexpected invalid-range error: ${invalid_range_stderr}")
endif()

run_rendercli(
  static_animation_stdout
  static_animation_stderr
  static_animation_result
  "${RENDERCLI}" --engine wireframe --width 16 --height 16 --animation
  "${static_scene}" "${static_animation_pattern}"
)
if(static_animation_result EQUAL 0)
  message(FATAL_ERROR "rendercli --animation accepted a static scene")
endif()
if(NOT static_animation_stderr MATCHES "requires a scene animation block")
  message(FATAL_ERROR "rendercli reported an unexpected static-scene animation error: ${static_animation_stderr}")
endif()
