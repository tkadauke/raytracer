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
set(scene_intent_scene "${TEST_OUTPUT_DIR}/scene-intent.json")
set(text_plan "${TEST_OUTPUT_DIR}/graph.txt")
set(dot_plan "${TEST_OUTPUT_DIR}/graph.dot")
set(intent_plan "${TEST_OUTPUT_DIR}/graph-intent.txt")
set(intent_view_plan "${TEST_OUTPUT_DIR}/graph-intent-view.txt")
set(scene_intent_plan "${TEST_OUTPUT_DIR}/graph-scene-intent.txt")
set(overlay_plan "${TEST_OUTPUT_DIR}/graph-overlay.txt")
set(json_plan "${TEST_OUTPUT_DIR}/graph.json")
set(replayed_dot_plan "${TEST_OUTPUT_DIR}/graph-replayed.dot")
set(invalid_plan "${TEST_OUTPUT_DIR}/invalid.txt")
set(graph_render "${TEST_OUTPUT_DIR}/graph-render.png")
set(replayed_render "${TEST_OUTPUT_DIR}/graph-replayed-render.png")
set(mismatched_render "${TEST_OUTPUT_DIR}/graph-mismatched-render.png")

file(WRITE "${scene_intent_scene}" [=[
{
  "id": "{90000000-0000-0000-0000-000000000000}",
  "name": "Scene Render Intent Fixture",
  "ambient": [0.4, 0.4, 0.4],
  "background": [0.4, 0.8, 1.0],
  "type": "Scene",
  "renderIntent": {
    "defaultExecutor": "rasterizer",
    "defaultViewMode": "beauty",
    "enableWireframeOverlay": true
  },
  "children": []
}
]=])

rendercli_run(
  NAME "rendercli exports text render graph"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format text
    --engine raytracer --width 32 --height 16
    "${static_scene}" "${text_plan}"
)
rendercli_assert_nonempty("${text_plan}" NAME "text render graph output")
file(READ "${text_plan}" text_graph)
if(NOT text_graph MATCHES "raytrace_beauty")
  message(FATAL_ERROR "text graph export did not contain raytrace_beauty: ${text_graph}")
endif()
if(NOT text_graph MATCHES "main_color")
  message(FATAL_ERROR "text graph export did not contain main_color: ${text_graph}")
endif()
if(NOT text_graph MATCHES "beauty_color")
  message(FATAL_ERROR "text graph export did not contain beauty_color: ${text_graph}")
endif()
if(NOT text_graph MATCHES "tonemap")
  message(FATAL_ERROR "text graph export did not contain tonemap: ${text_graph}")
endif()

rendercli_run(
  NAME "rendercli exports DOT render graph"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format dot
    --engine wireframe --width 32 --height 16
    "${static_scene}" "${dot_plan}"
)
rendercli_assert_nonempty("${dot_plan}" NAME "DOT render graph output")
file(READ "${dot_plan}" dot_graph)
if(NOT dot_graph MATCHES "digraph RenderPlan")
  message(FATAL_ERROR "DOT graph export did not contain a graph declaration: ${dot_graph}")
endif()
if(NOT dot_graph MATCHES "wireframe_beauty")
  message(FATAL_ERROR "DOT graph export did not contain wireframe_beauty: ${dot_graph}")
endif()

rendercli_run(
  NAME "rendercli graph executor override selects rasterizer"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format text
    --engine raytracer --render_graph_executor rasterizer --width 32 --height 16
    "${static_scene}" "${intent_plan}"
)
rendercli_assert_nonempty("${intent_plan}" NAME "graph executor override output")
file(READ "${intent_plan}" intent_graph)
if(NOT intent_graph MATCHES "raster_beauty")
  message(FATAL_ERROR "graph executor override did not select raster_beauty: ${intent_graph}")
endif()

rendercli_run(
  NAME "rendercli graph view override selects wireframe"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format text
    --engine raytracer --render_graph_executor raytracer --render_graph_view wireframe
    --width 32 --height 16
    "${static_scene}" "${intent_view_plan}"
)
rendercli_assert_nonempty("${intent_view_plan}" NAME "graph view override output")
file(READ "${intent_view_plan}" intent_view_graph)
if(NOT intent_view_graph MATCHES "wireframe_beauty")
  message(FATAL_ERROR "graph view override did not select wireframe_beauty: ${intent_view_graph}")
endif()

rendercli_run(
  NAME "rendercli uses scene render intent"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format text
    --width 32 --height 16
    "${scene_intent_scene}" "${scene_intent_plan}"
)
rendercli_assert_nonempty("${scene_intent_plan}" NAME "scene render intent graph output")
file(READ "${scene_intent_plan}" scene_intent_graph)
if(NOT scene_intent_graph MATCHES "raster_beauty")
  message(FATAL_ERROR "scene render intent did not select raster_beauty: ${scene_intent_graph}")
endif()
if(NOT scene_intent_graph MATCHES "wireframe_overlay")
  message(FATAL_ERROR "scene render intent did not add wireframe_overlay: ${scene_intent_graph}")
endif()

rendercli_run(
  NAME "rendercli graph wireframe overlay intent adds overlay pass"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format text
    --render_graph_wireframe_overlay --width 32 --height 16
    "${static_scene}" "${overlay_plan}"
)
rendercli_assert_nonempty("${overlay_plan}" NAME "graph wireframe overlay output")
file(READ "${overlay_plan}" overlay_graph)
if(NOT overlay_graph MATCHES "wireframe_overlay")
  message(FATAL_ERROR "graph overlay intent did not add wireframe_overlay: ${overlay_graph}")
endif()
if(NOT overlay_graph MATCHES "overlay_color")
  message(FATAL_ERROR "graph overlay intent did not add overlay_color: ${overlay_graph}")
endif()
if(NOT overlay_graph MATCHES "tonemap")
  message(FATAL_ERROR "graph overlay intent did not retain tonemap: ${overlay_graph}")
endif()

rendercli_run(
  NAME "rendercli exports JSON render graph to stdout"
  STDOUT_MATCHES
    "raster_beauty"
    "tonemap"
    "\"width\""
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format json
    --engine raster --width 20 --height 10
    "${static_scene}"
)

rendercli_run(
  NAME "rendercli exports JSON render graph to file"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format json
    --engine raytracer --width 32 --height 16
    "${static_scene}" "${json_plan}"
)
rendercli_assert_nonempty("${json_plan}" NAME "JSON render graph output file")

rendercli_run(
  NAME "rendercli replays JSON render graph as DOT"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_in "${json_plan}" --render_graph_format dot
    "${static_scene}" "${replayed_dot_plan}"
)
rendercli_assert_nonempty("${replayed_dot_plan}" NAME "replayed DOT render graph output")
file(READ "${replayed_dot_plan}" replayed_dot_graph)
if(NOT replayed_dot_graph MATCHES "raytrace_beauty")
  message(FATAL_ERROR "replayed DOT graph did not contain raytrace_beauty: ${replayed_dot_graph}")
endif()

rendercli_run(
  NAME "rendercli renders through compiled graph"
  COMMAND
    "${RENDERCLI}" --render_graph --engine wireframe --width 32 --height 16
    "${static_scene}" "${graph_render}"
)
rendercli_assert_nonempty("${graph_render}" NAME "rendercli --render_graph image")

rendercli_run(
  NAME "rendercli renders through replayed JSON graph"
  COMMAND
    "${RENDERCLI}" --render_graph --render_graph_in "${json_plan}"
    "${static_scene}" "${replayed_render}"
)
rendercli_assert_nonempty("${replayed_render}" NAME "rendercli --render_graph_in image")

rendercli_run(
  NAME "rendercli disables optional tonemap pass"
  STDOUT_MATCHES "tonemap \\[tonemap/postprocess\\] disabled"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format text --disable_pass tonemap
    --width 32 --height 16
    "${static_scene}"
)

rendercli_expect_failure(
  NAME "rendercli rejects render graph output size mismatch"
  STDERR_MATCHES "Render graph output width is 32"
  COMMAND
    "${RENDERCLI}" --render_graph --render_graph_in "${json_plan}" --width 31 --height 16
    "${static_scene}" "${mismatched_render}"
)

rendercli_expect_failure(
  NAME "rendercli rejects disabled required graph pass"
  STDERR_MATCHES "disabled_required_pass"
  COMMAND
    "${RENDERCLI}" --render_graph_only --disable_pass raytrace_beauty
    "${static_scene}" "${invalid_plan}"
)
