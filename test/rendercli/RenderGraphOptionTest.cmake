if(NOT DEFINED RENDERCLI)
  message(FATAL_ERROR "RENDERCLI is required")
endif()

if(NOT DEFINED TEST_OUTPUT_DIR)
  message(FATAL_ERROR "TEST_OUTPUT_DIR is required")
endif()

file(REMOVE_RECURSE "${TEST_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${TEST_OUTPUT_DIR}")

set(static_scene "${PROJECT_SOURCE_DIR}/scenes/dice.json")
set(text_plan "${TEST_OUTPUT_DIR}/graph.txt")
set(dot_plan "${TEST_OUTPUT_DIR}/graph.dot")
set(json_plan "${TEST_OUTPUT_DIR}/graph.json")
set(replayed_dot_plan "${TEST_OUTPUT_DIR}/graph-replayed.dot")
set(invalid_plan "${TEST_OUTPUT_DIR}/invalid.txt")
set(graph_render "${TEST_OUTPUT_DIR}/graph-render.png")
set(replayed_render "${TEST_OUTPUT_DIR}/graph-replayed-render.png")
set(mismatched_render "${TEST_OUTPUT_DIR}/graph-mismatched-render.png")

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
  text_stdout
  text_stderr
  text_result
  "${RENDERCLI}" --render_graph_only --render_graph_format text
  --engine raytracer --width 32 --height 16
  "${static_scene}" "${text_plan}"
)
if(NOT text_result EQUAL 0)
  message(FATAL_ERROR "rendercli text graph export failed: ${text_stderr}")
endif()
file(READ "${text_plan}" text_graph)
if(NOT text_graph MATCHES "raytrace_beauty")
  message(FATAL_ERROR "text graph export did not contain raytrace_beauty: ${text_graph}")
endif()
if(NOT text_graph MATCHES "main_color")
  message(FATAL_ERROR "text graph export did not contain main_color: ${text_graph}")
endif()

run_rendercli(
  dot_stdout
  dot_stderr
  dot_result
  "${RENDERCLI}" --render_graph_only --render_graph_format dot
  --engine wireframe --width 32 --height 16
  "${static_scene}" "${dot_plan}"
)
if(NOT dot_result EQUAL 0)
  message(FATAL_ERROR "rendercli DOT graph export failed: ${dot_stderr}")
endif()
file(READ "${dot_plan}" dot_graph)
if(NOT dot_graph MATCHES "digraph RenderPlan")
  message(FATAL_ERROR "DOT graph export did not contain a graph declaration: ${dot_graph}")
endif()
if(NOT dot_graph MATCHES "wireframe_beauty")
  message(FATAL_ERROR "DOT graph export did not contain wireframe_beauty: ${dot_graph}")
endif()

run_rendercli(
  json_stdout
  json_stderr
  json_result
  "${RENDERCLI}" --render_graph_only --render_graph_format json
  --engine raster --width 20 --height 10
  "${static_scene}"
)
if(NOT json_result EQUAL 0)
  message(FATAL_ERROR "rendercli JSON graph export to stdout failed: ${json_stderr}")
endif()
if(NOT json_stdout MATCHES "raster_beauty")
  message(FATAL_ERROR "JSON graph stdout did not contain raster_beauty: ${json_stdout}")
endif()
if(NOT json_stdout MATCHES "\"width\"")
  message(FATAL_ERROR "JSON graph stdout did not contain resource dimensions: ${json_stdout}")
endif()

run_rendercli(
  json_file_stdout
  json_file_stderr
  json_file_result
  "${RENDERCLI}" --render_graph_only --render_graph_format json
  --engine raytracer --width 32 --height 16
  "${static_scene}" "${json_plan}"
)
if(NOT json_file_result EQUAL 0)
  message(FATAL_ERROR "rendercli JSON graph export to file failed: ${json_file_stderr}")
endif()

run_rendercli(
  replay_dot_stdout
  replay_dot_stderr
  replay_dot_result
  "${RENDERCLI}" --render_graph_only --render_graph_in "${json_plan}" --render_graph_format dot
  "${static_scene}" "${replayed_dot_plan}"
)
if(NOT replay_dot_result EQUAL 0)
  message(FATAL_ERROR "rendercli JSON graph replay as DOT failed: ${replay_dot_stderr}")
endif()
file(READ "${replayed_dot_plan}" replayed_dot_graph)
if(NOT replayed_dot_graph MATCHES "raytrace_beauty")
  message(FATAL_ERROR "replayed DOT graph did not contain raytrace_beauty: ${replayed_dot_graph}")
endif()

run_rendercli(
  render_stdout
  render_stderr
  render_result
  "${RENDERCLI}" --render_graph --engine wireframe --width 32 --height 16
  "${static_scene}" "${graph_render}"
)
if(NOT render_result EQUAL 0)
  message(FATAL_ERROR "rendercli graph render failed: ${render_stderr}")
endif()
if(NOT EXISTS "${graph_render}")
  message(FATAL_ERROR "rendercli --render_graph did not create an image")
endif()

run_rendercli(
  replay_render_stdout
  replay_render_stderr
  replay_render_result
  "${RENDERCLI}" --render_graph --render_graph_in "${json_plan}"
  "${static_scene}" "${replayed_render}"
)
if(NOT replay_render_result EQUAL 0)
  message(FATAL_ERROR "rendercli JSON graph replay render failed: ${replay_render_stderr}")
endif()
if(NOT EXISTS "${replayed_render}")
  message(FATAL_ERROR "rendercli --render_graph_in did not create an image")
endif()

run_rendercli(
  mismatch_stdout
  mismatch_stderr
  mismatch_result
  "${RENDERCLI}" --render_graph --render_graph_in "${json_plan}" --width 31 --height 16
  "${static_scene}" "${mismatched_render}"
)
if(mismatch_result EQUAL 0)
  message(FATAL_ERROR "rendercli accepted a render graph output size mismatch")
endif()
if(NOT mismatch_stderr MATCHES "Render graph output width is 32")
  message(FATAL_ERROR "rendercli reported an unexpected graph size mismatch: ${mismatch_stderr}")
endif()

run_rendercli(
  invalid_stdout
  invalid_stderr
  invalid_result
  "${RENDERCLI}" --render_graph_only --disable_pass raytrace_beauty
  "${static_scene}" "${invalid_plan}"
)
if(invalid_result EQUAL 0)
  message(FATAL_ERROR "rendercli accepted a disabled required graph pass")
endif()
if(NOT invalid_stderr MATCHES "disabled_required_pass")
  message(FATAL_ERROR "rendercli reported an unexpected disabled-pass error: ${invalid_stderr}")
endif()
