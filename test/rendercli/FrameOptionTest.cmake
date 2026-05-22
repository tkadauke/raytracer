if(NOT DEFINED RENDERCLI)
  message(FATAL_ERROR "RENDERCLI is required")
endif()

if(NOT DEFINED TEST_OUTPUT_DIR)
  message(FATAL_ERROR "TEST_OUTPUT_DIR is required")
endif()

file(MAKE_DIRECTORY "${TEST_OUTPUT_DIR}")

set(animated_scene "${PROJECT_SOURCE_DIR}/examples/GeneratedRayTracer/scenes/animation_frame_demo.json")
set(static_scene "${PROJECT_SOURCE_DIR}/examples/GeneratedRayTracer/scenes/dice.json")
set(frame_1 "${TEST_OUTPUT_DIR}/frame_0001.png")
set(frame_48 "${TEST_OUTPUT_DIR}/frame_0048.png")
set(static_frame "${TEST_OUTPUT_DIR}/static_frame.png")
set(invalid_frame "${TEST_OUTPUT_DIR}/invalid_frame.png")

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
