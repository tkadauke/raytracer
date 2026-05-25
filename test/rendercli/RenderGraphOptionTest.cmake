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
set(default_graph_scene "${TEST_OUTPUT_DIR}/default-graph-scene.json")
set(malformed_graph_json "${TEST_OUTPUT_DIR}/malformed-graph.json")
set(json_root_graph "${TEST_OUTPUT_DIR}/json-root-graph.json")
set(semantic_invalid_graph "${TEST_OUTPUT_DIR}/semantic-invalid-graph.json")
set(out_of_order_graph "${TEST_OUTPUT_DIR}/out-of-order-graph.json")
set(text_plan "${TEST_OUTPUT_DIR}/graph.txt")
set(dot_plan "${TEST_OUTPUT_DIR}/graph.dot")
set(intent_plan "${TEST_OUTPUT_DIR}/graph-intent.txt")
set(intent_view_plan "${TEST_OUTPUT_DIR}/graph-intent-view.txt")
set(depth_view_render "${TEST_OUTPUT_DIR}/graph-depth-view.png")
set(normal_view_render "${TEST_OUTPUT_DIR}/graph-normal-view.png")
set(object_id_view_render "${TEST_OUTPUT_DIR}/graph-object-id-view.png")
set(material_id_view_render "${TEST_OUTPUT_DIR}/graph-material-id-view.png")
set(world_position_view_render "${TEST_OUTPUT_DIR}/graph-world-position-view.png")
set(scene_intent_plan "${TEST_OUTPUT_DIR}/graph-scene-intent.txt")
set(overlay_plan "${TEST_OUTPUT_DIR}/graph-overlay.txt")
set(json_plan "${TEST_OUTPUT_DIR}/graph.json")
set(replayed_dot_plan "${TEST_OUTPUT_DIR}/graph-replayed.dot")
set(replayed_matching_render "${TEST_OUTPUT_DIR}/graph-replayed-matching-render.png")
set(invalid_plan "${TEST_OUTPUT_DIR}/invalid.txt")
set(default_graph_render "${TEST_OUTPUT_DIR}/default-graph-render.png")
set(direct_engine_render "${TEST_OUTPUT_DIR}/direct-engine-render.png")
set(graph_render "${TEST_OUTPUT_DIR}/graph-render.png")
set(graph_trace "${TEST_OUTPUT_DIR}/graph-trace.json")
set(graph_trace_render "${TEST_OUTPUT_DIR}/graph-trace-render.png")
set(graph_aov_render "${TEST_OUTPUT_DIR}/graph-aov-render.png")
set(graph_aov_depth "${TEST_OUTPUT_DIR}/graph-aov-depth.png")
set(graph_aov_normal "${TEST_OUTPUT_DIR}/graph-aov-normal.png")
set(raster_shadow_trace "${TEST_OUTPUT_DIR}/raster-shadow-trace.json")
set(raster_shadow_trace_render "${TEST_OUTPUT_DIR}/raster-shadow-trace-render.png")
set(raster_state_plan "${TEST_OUTPUT_DIR}/raster-state-graph.json")
set(raster_state_render "${TEST_OUTPUT_DIR}/raster-state-render.png")
set(wireframe_state_plan "${TEST_OUTPUT_DIR}/wireframe-state-graph.json")
set(raytracer_post_aa_plan "${TEST_OUTPUT_DIR}/raytracer-post-aa-graph.txt")
set(wireframe_post_aa_plan "${TEST_OUTPUT_DIR}/wireframe-post-aa-graph.txt")
set(replayed_render "${TEST_OUTPUT_DIR}/graph-replayed-render.png")
set(out_of_order_text_plan "${TEST_OUTPUT_DIR}/graph-out-of-order.txt")
set(out_of_order_render "${TEST_OUTPUT_DIR}/graph-out-of-order-render.png")
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
    "enableWireframeOverlay": true,
    "postProcessAA": "smaa"
  },
  "children": []
}
]=])

file(WRITE "${default_graph_scene}" [=[
{
  "id": "{91000000-0000-0000-0000-000000000000}",
  "name": "Default Graph Fixture",
  "ambient": [0.4, 0.4, 0.4],
  "background": [0.4, 0.8, 1.0],
  "type": "Scene",
  "renderIntent": {
    "defaultExecutor": "raytracer",
    "defaultViewMode": "wireframe"
  },
  "children": [
    {
      "id": "camera",
      "name": "Camera",
      "position": [0.0, 0.0, -3.0],
      "target": [0.0, 0.0, 0.0],
      "distance": 5.0,
      "zoom": 1.4,
      "type": "PinholeCamera",
      "children": []
    },
    {
      "id": "red",
      "name": "Red",
      "color": [1.0, 0.0, 0.0],
      "type": "ConstantColorTexture",
      "children": []
    },
    {
      "id": "matte",
      "name": "Matte",
      "diffuseTexture": "red",
      "ambientCoefficient": 1.0,
      "diffuseCoefficient": 1.0,
      "type": "MatteMaterial",
      "children": []
    },
    {
      "id": "light",
      "name": "Light",
      "position": [0.0, 0.0, 0.0],
      "rotation": [0.0, 0.0, 0.0],
      "scale": [1.0, 1.0, 1.0],
      "visible": true,
      "color": [1.0, 1.0, 1.0],
      "intensity": 1.0,
      "direction": [-0.5, -1.0, -0.5],
      "type": "DirectionalLight",
      "children": []
    },
    {
      "id": "sphere",
      "name": "Sphere",
      "position": [0.0, 0.0, 0.0],
      "rotation": [0.0, 0.0, 0.0],
      "scale": [1.0, 1.0, 1.0],
      "visible": true,
      "material": "matte",
      "radius": 1.0,
      "type": "Sphere",
      "children": []
    }
  ]
}
]=])

file(WRITE "${malformed_graph_json}" "{ not valid json")
file(WRITE "${json_root_graph}" "[]")
file(WRITE "${semantic_invalid_graph}" [=[
{
  "resources": [
    {
      "id": "main_color",
      "name": "Main color",
      "type": "color",
      "format": "rgb_double",
      "width": 32,
      "height": 16,
      "sampleCount": 1,
      "domain": "cpu",
      "lifetime": "exported"
    }
  ],
  "passes": [
    {
      "id": "first_writer",
      "name": "First writer",
      "kind": "beauty",
      "executor": "raytracer",
      "features": ["main"],
      "reads": [],
      "writes": ["main_color"],
      "disabledBehavior": "error",
      "enabled": true,
      "hasExternalSideEffects": false,
      "canRunConcurrently": false
    },
    {
      "id": "second_writer",
      "name": "Second writer",
      "kind": "beauty",
      "executor": "wireframe",
      "features": ["debug"],
      "reads": [],
      "writes": ["main_color"],
      "disabledBehavior": "error",
      "enabled": true,
      "hasExternalSideEffects": false,
      "canRunConcurrently": false
    }
  ]
}
]=])

file(WRITE "${out_of_order_graph}" [=[
{
  "resources": [
    {
      "id": "beauty_color",
      "name": "Beauty color",
      "type": "color",
      "format": "rgb_double",
      "width": 32,
      "height": 16,
      "sampleCount": 1,
      "domain": "cpu",
      "lifetime": "transient"
    },
    {
      "id": "main_color",
      "name": "Main color",
      "type": "color",
      "format": "rgb_double",
      "width": 32,
      "height": 16,
      "sampleCount": 1,
      "domain": "cpu",
      "lifetime": "exported"
    }
  ],
  "passes": [
    {
      "id": "tonemap",
      "name": "Tone map",
      "kind": "tonemap",
      "executor": "postprocess",
      "features": ["main", "tonemap", "postprocess"],
      "reads": ["beauty_color"],
      "writes": ["main_color"],
      "disabledBehavior": "passthrough",
      "enabled": true,
      "hasExternalSideEffects": false,
      "canRunConcurrently": false
    },
    {
      "id": "raytrace_beauty",
      "name": "Raytraced beauty",
      "kind": "beauty",
      "executor": "raytracer",
      "features": ["main", "beauty", "raytracer"],
      "reads": [],
      "writes": ["beauty_color"],
      "disabledBehavior": "error",
      "enabled": true,
      "hasExternalSideEffects": false,
      "canRunConcurrently": false
    }
  ]
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
if(NOT scene_intent_graph MATCHES "post_smaa")
  message(FATAL_ERROR "scene render intent did not add post_smaa: ${scene_intent_graph}")
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

rendercli_expect_failure(
  NAME "rendercli rejects invalid render graph format"
  STDERR_MATCHES "Render graph format must be"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format yaml
    "${static_scene}" "${invalid_plan}"
)

rendercli_expect_failure(
  NAME "rendercli rejects invalid render graph executor"
  STDERR_MATCHES "Render graph executor must be"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_executor pathtracer
    "${static_scene}" "${invalid_plan}"
)

rendercli_expect_failure(
  NAME "rendercli rejects invalid render graph view"
  STDERR_MATCHES "Render graph view mode must be"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_view motion_vector
    "${static_scene}" "${invalid_plan}"
)

rendercli_run(
  NAME "rendercli exports depth AOV render graph"
  STDOUT_MATCHES
    "depth_aov"
    "visualize_depth_aov"
    "main_color"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_view depth
    --width 32 --height 24
    "${static_scene}"
)

rendercli_run(
  NAME "rendercli renders depth AOV view through graph"
  COMMAND
    "${RENDERCLI}" --render_graph_view depth --width 32 --height 24
    "${static_scene}" "${depth_view_render}"
)
rendercli_assert_nonempty("${depth_view_render}" NAME "depth AOV graph render output")

rendercli_run(
  NAME "rendercli exports normal AOV render graph"
  STDOUT_MATCHES
    "normal_aov"
    "visualize_normal_aov"
    "main_color"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_view normal
    --width 32 --height 24
    "${static_scene}"
)

rendercli_run(
  NAME "rendercli renders normal AOV view through graph"
  COMMAND
    "${RENDERCLI}" --render_graph_view normal --width 32 --height 24
    "${static_scene}" "${normal_view_render}"
)
rendercli_assert_nonempty("${normal_view_render}" NAME "normal AOV graph render output")

rendercli_run(
  NAME "rendercli exports object ID AOV render graph"
  STDOUT_MATCHES
    "object_id_aov"
    "visualize_object_id_aov"
    "main_color"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_view object_id
    --width 32 --height 24
    "${static_scene}"
)

rendercli_run(
  NAME "rendercli renders object ID AOV view through graph"
  COMMAND
    "${RENDERCLI}" --render_graph_view object_id --width 32 --height 24
    "${static_scene}" "${object_id_view_render}"
)
rendercli_assert_nonempty("${object_id_view_render}" NAME "object ID AOV graph render output")

rendercli_run(
  NAME "rendercli exports material ID AOV render graph"
  STDOUT_MATCHES
    "material_id_aov"
    "visualize_material_id_aov"
    "main_color"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_view material_id
    --width 32 --height 24
    "${static_scene}"
)

rendercli_run(
  NAME "rendercli renders material ID AOV view through graph"
  COMMAND
    "${RENDERCLI}" --render_graph_view material_id --width 32 --height 24
    "${static_scene}" "${material_id_view_render}"
)
rendercli_assert_nonempty("${material_id_view_render}" NAME "material ID AOV graph render output")

rendercli_run(
  NAME "rendercli exports world position AOV render graph"
  STDOUT_MATCHES
    "world_position_aov"
    "visualize_world_position_aov"
    "main_color"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_view world_position
    --width 32 --height 24
    "${static_scene}"
)

rendercli_run(
  NAME "rendercli renders world position AOV view through graph"
  COMMAND
    "${RENDERCLI}" --render_graph_view world_position --width 32 --height 24
    "${static_scene}" "${world_position_view_render}"
)
rendercli_assert_nonempty("${world_position_view_render}"
                          NAME "world position AOV graph render output")

rendercli_run(
  NAME "rendercli writes multiple graph AOV output images"
  COMMAND
    "${RENDERCLI}" --render_graph_aov_out "depth=${graph_aov_depth}"
    --render_graph_aov_out "normal=${graph_aov_normal}" --width 32 --height 24
    "${static_scene}" "${graph_aov_render}"
)
rendercli_assert_nonempty("${graph_aov_render}" NAME "multi AOV graph main render output")
rendercli_assert_nonempty("${graph_aov_depth}" NAME "multi AOV graph depth output")
rendercli_assert_nonempty("${graph_aov_normal}" NAME "multi AOV graph normal output")

rendercli_expect_failure(
  NAME "rendercli rejects invalid graph AOV output view"
  STDERR_MATCHES "Render graph AOV output view must be"
  COMMAND
    "${RENDERCLI}" --render_graph_aov_out "beauty=${invalid_plan}"
    "${static_scene}" "${invalid_plan}"
)

rendercli_expect_failure(
  NAME "rendercli rejects graph-only AOV output"
  STDERR_MATCHES "Cannot combine --render_graph_only with --render_graph_aov_out"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_aov_out "depth=${invalid_plan}"
    "${static_scene}" "${invalid_plan}"
)

rendercli_expect_failure(
  NAME "rendercli rejects invalid disabled pass kind"
  STDERR_MATCHES "Render graph pass kind is not recognized"
  COMMAND
    "${RENDERCLI}" --render_graph_only --disable_pass_kind not_a_kind
    "${static_scene}" "${invalid_plan}"
)

rendercli_expect_failure(
  NAME "rendercli rejects invalid disabled executor"
  STDERR_MATCHES "Render graph executor is not recognized"
  COMMAND
    "${RENDERCLI}" --render_graph_only --disable_executor gpu
    "${static_scene}" "${invalid_plan}"
)

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

rendercli_expect_failure(
  NAME "rendercli rejects malformed JSON render graph"
  STDERR_MATCHES "Unable to parse render graph JSON"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_in "${malformed_graph_json}"
    "${static_scene}" "${invalid_plan}"
)

rendercli_expect_failure(
  NAME "rendercli rejects non-object JSON render graph"
  STDERR_MATCHES "Render graph JSON must contain an object"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_in "${json_root_graph}"
    "${static_scene}" "${invalid_plan}"
)

rendercli_expect_failure(
  NAME "rendercli rejects semantically invalid JSON render graph"
  STDERR_MATCHES "duplicate_writer"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_in "${semantic_invalid_graph}"
    "${static_scene}" "${invalid_plan}"
)

rendercli_run(
  NAME "rendercli renders through default graph"
  COMMAND
    "${RENDERCLI}" --engine wireframe --width 32 --height 16
    "${static_scene}" "${graph_render}"
)
rendercli_assert_nonempty("${graph_render}" NAME "rendercli default graph image")

rendercli_run(
  NAME "rendercli writes execution trace while rendering through graph"
  COMMAND
    "${RENDERCLI}" --engine raytracer --width 32 --height 16 --post_aa fxaa
    --render_graph_trace_out "${graph_trace}"
    "${static_scene}" "${graph_trace_render}"
)
rendercli_assert_nonempty("${graph_trace_render}" NAME "rendercli graph trace image")
rendercli_assert_nonempty("${graph_trace}" NAME "rendercli graph trace JSON")
file(READ "${graph_trace}" graph_trace_json)
if(NOT graph_trace_json MATCHES "\"id\": \"post_fxaa\"")
  message(FATAL_ERROR "graph trace did not contain post_fxaa pass: ${graph_trace_json}")
endif()
if(NOT graph_trace_json MATCHES "\"status\": \"completed\"")
  message(FATAL_ERROR "graph trace did not contain completed pass status: ${graph_trace_json}")
endif()
if(NOT graph_trace_json MATCHES "\"inputs\"")
  message(FATAL_ERROR "graph trace did not contain inputs: ${graph_trace_json}")
endif()
if(NOT graph_trace_json MATCHES "\"outputs\"")
  message(FATAL_ERROR "graph trace did not contain outputs: ${graph_trace_json}")
endif()
if(NOT graph_trace_json MATCHES "\"diffs\"")
  message(FATAL_ERROR "graph trace did not contain diffs: ${graph_trace_json}")
endif()
if(NOT graph_trace_json MATCHES "\"previewWidth\": 32")
  message(FATAL_ERROR "graph trace did not contain full-width previews: ${graph_trace_json}")
endif()
if(NOT graph_trace_json MATCHES "\"previewHeight\": 16")
  message(FATAL_ERROR "graph trace did not contain full-height previews: ${graph_trace_json}")
endif()
if(NOT graph_trace_json MATCHES "\"cache\"")
  message(FATAL_ERROR "graph trace did not contain cache metadata: ${graph_trace_json}")
endif()
if(NOT graph_trace_json MATCHES "\"status\": \"not_cacheable\"")
  message(FATAL_ERROR "graph trace did not contain not-cacheable resource status: ${graph_trace_json}")
endif()

rendercli_run(
  NAME "rendercli writes raster shadow depth trace"
  COMMAND
    "${RENDERCLI}" --engine raster --shadow_maps --width 32 --height 16
    --render_graph_trace_out "${raster_shadow_trace}"
    "${static_scene}" "${raster_shadow_trace_render}"
)
rendercli_assert_nonempty("${raster_shadow_trace_render}" NAME "rendercli raster shadow trace image")
rendercli_assert_nonempty("${raster_shadow_trace}" NAME "rendercli raster shadow trace JSON")
file(READ "${raster_shadow_trace}" raster_shadow_trace_json)
if(NOT raster_shadow_trace_json MATCHES "\"id\": \"raster_preview_shadows\"")
  message(FATAL_ERROR "raster shadow trace did not contain shadow pass: ${raster_shadow_trace_json}")
endif()
if(NOT raster_shadow_trace_json MATCHES "\"resource\": \"preview_shadow_map\"")
  message(FATAL_ERROR "raster shadow trace did not contain preview shadow map: ${raster_shadow_trace_json}")
endif()
if(NOT raster_shadow_trace_json MATCHES "\"previewKind\": \"depth\"")
  message(FATAL_ERROR "raster shadow trace did not contain depth preview kind: ${raster_shadow_trace_json}")
endif()
if(NOT raster_shadow_trace_json MATCHES "\"status\": \"stored\"")
  message(FATAL_ERROR "raster shadow trace did not contain stored cache status: ${raster_shadow_trace_json}")
endif()

rendercli_run(
  NAME "rendercli writes raster pass state while rendering through graph"
  COMMAND
    "${RENDERCLI}" --engine raster --render_graph_format json
    --render_graph_out "${raster_state_plan}"
    --width 32 --height 16 --msaa 4 --msaa_shading per_fragment --post_aa fxaa
    --shadow_maps --shadow_map_size 64 --shadow_bias 0.2
    "${static_scene}" "${raster_state_render}"
)
rendercli_assert_nonempty("${raster_state_render}" NAME "graph raster state render")
rendercli_assert_nonempty("${raster_state_plan}" NAME "graph raster state plan")
file(READ "${raster_state_plan}" raster_state_graph)
if(NOT raster_state_graph MATCHES "raster_beauty")
  message(FATAL_ERROR "raster state graph did not contain raster_beauty: ${raster_state_graph}")
endif()
if(NOT raster_state_graph MATCHES "msaaSamples")
  message(FATAL_ERROR "raster state graph did not contain msaaSamples: ${raster_state_graph}")
endif()
if(NOT raster_state_graph MATCHES "per_fragment")
  message(FATAL_ERROR "raster state graph did not contain MSAA shading mode: ${raster_state_graph}")
endif()
if(NOT raster_state_graph MATCHES "post_fxaa")
  message(FATAL_ERROR "raster state graph did not contain post_fxaa pass: ${raster_state_graph}")
endif()
if(NOT raster_state_graph MATCHES "raster_preview_shadows")
  message(FATAL_ERROR "raster state graph did not contain preview shadow pass: ${raster_state_graph}")
endif()
if(NOT raster_state_graph MATCHES "post_aa_color")
  message(FATAL_ERROR "raster state graph did not contain post-AA resource: ${raster_state_graph}")
endif()
if(NOT raster_state_graph MATCHES "post_process_aa")
  message(FATAL_ERROR "raster state graph did not contain typed post-AA state: ${raster_state_graph}")
endif()
if(raster_state_graph MATCHES "postProcessAA")
  message(FATAL_ERROR "FXAA should be graph-visible, not hidden in raster state: ${raster_state_graph}")
endif()
if(NOT raster_state_graph MATCHES "mapSize")
  message(FATAL_ERROR "raster state graph did not contain shadow state: ${raster_state_graph}")
endif()

rendercli_run(
  NAME "rendercli writes wireframe pass state while rendering through graph"
  COMMAND
    "${RENDERCLI}" --engine wireframe --render_graph_only --render_graph_format json
    --width 32 --height 16 --lod 2
    "${static_scene}" "${wireframe_state_plan}"
)
rendercli_assert_nonempty("${wireframe_state_plan}" NAME "graph wireframe state plan")
file(READ "${wireframe_state_plan}" wireframe_state_graph)
if(NOT wireframe_state_graph MATCHES "wireframe_beauty")
  message(FATAL_ERROR "wireframe state graph did not contain wireframe_beauty: ${wireframe_state_graph}")
endif()
if(NOT wireframe_state_graph MATCHES "\"lod\"")
  message(FATAL_ERROR "wireframe state graph did not contain lod state: ${wireframe_state_graph}")
endif()

rendercli_run(
  NAME "rendercli compiles raytracer FXAA as graph postprocess"
  COMMAND
    "${RENDERCLI}" --engine raytracer --render_graph_only --render_graph_format text
    --width 32 --height 16 --post_aa fxaa
    "${static_scene}" "${raytracer_post_aa_plan}"
)
file(READ "${raytracer_post_aa_plan}" raytracer_post_aa_graph)
if(NOT raytracer_post_aa_graph MATCHES "raytrace_beauty")
  message(FATAL_ERROR "raytracer post-AA graph did not contain raytrace_beauty: ${raytracer_post_aa_graph}")
endif()
if(NOT raytracer_post_aa_graph MATCHES "post_fxaa")
  message(FATAL_ERROR "raytracer post-AA graph did not contain post_fxaa: ${raytracer_post_aa_graph}")
endif()

rendercli_run(
  NAME "rendercli compiles wireframe SMAA as graph postprocess"
  COMMAND
    "${RENDERCLI}" --engine wireframe --render_graph_only --render_graph_format text
    --width 32 --height 16 --post_aa smaa
    "${static_scene}" "${wireframe_post_aa_plan}"
)
file(READ "${wireframe_post_aa_plan}" wireframe_post_aa_graph)
if(NOT wireframe_post_aa_graph MATCHES "wireframe_beauty")
  message(FATAL_ERROR "wireframe post-AA graph did not contain wireframe_beauty: ${wireframe_post_aa_graph}")
endif()
if(NOT wireframe_post_aa_graph MATCHES "post_smaa")
  message(FATAL_ERROR "wireframe post-AA graph did not contain post_smaa: ${wireframe_post_aa_graph}")
endif()

set(raster_taa_plan "${TEST_OUTPUT_DIR}/raster_taa_plan.json")
rendercli_run(
  NAME "rendercli keeps raster TAA in beauty pass state"
  COMMAND
    "${RENDERCLI}" --engine raster --render_graph_only --render_graph_format json
    --width 32 --height 16 --post_aa taa
    "${static_scene}" "${raster_taa_plan}"
)
file(READ "${raster_taa_plan}" raster_taa_graph)
if(NOT raster_taa_graph MATCHES "postProcessAA")
  message(FATAL_ERROR "raster TAA graph did not contain postProcessAA state: ${raster_taa_graph}")
endif()
if(NOT raster_taa_graph MATCHES "taa")
  message(FATAL_ERROR "raster TAA graph did not contain TAA setting: ${raster_taa_graph}")
endif()

rendercli_run(
  NAME "rendercli disables graph-visible FXAA pass"
  STDOUT_MATCHES "post_fxaa \\[postprocess/postprocess\\] disabled"
  COMMAND
    "${RENDERCLI}" --engine raster --render_graph_only --render_graph_format text
    --width 32 --height 16 --post_aa fxaa --disable_pass post_fxaa
    "${static_scene}"
)

rendercli_run(
  NAME "rendercli disables graph-visible raster preview shadow pass"
  STDOUT_MATCHES
    "raster_preview_shadows \\[shadow/rasterizer\\] disabled"
    "raster_beauty \\[beauty/rasterizer\\] enabled"
  COMMAND
    "${RENDERCLI}" --engine raster --render_graph_only --render_graph_format text
    --width 32 --height 16 --shadow_maps --disable_pass raster_preview_shadows
    "${static_scene}"
)

rendercli_run(
  NAME "rendercli default render honors scene render intent"
  COMMAND
    "${RENDERCLI}" --width 48 --height 32
    "${default_graph_scene}" "${default_graph_render}"
)
rendercli_assert_nonempty("${default_graph_render}" NAME "rendercli default graph intent output")

rendercli_run(
  NAME "rendercli direct engine bypasses scene render intent"
  COMMAND
    "${RENDERCLI}" --direct_engine --engine raytracer --width 48 --height 32
    "${default_graph_scene}" "${direct_engine_render}"
)
rendercli_assert_nonempty("${direct_engine_render}" NAME "rendercli direct engine output")
rendercli_assert_files_differ("${default_graph_render}" "${direct_engine_render}"
                              NAME "default graph output differs from direct engine output")

rendercli_run(
  NAME "rendercli renders through replayed JSON graph"
  COMMAND
    "${RENDERCLI}" --render_graph --render_graph_in "${json_plan}"
    "${static_scene}" "${replayed_render}"
)
rendercli_assert_nonempty("${replayed_render}" NAME "rendercli --render_graph_in image")

rendercli_run(
  NAME "rendercli text export shows dependency execution order"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_in "${out_of_order_graph}"
    --render_graph_format text
    "${static_scene}" "${out_of_order_text_plan}"
)
rendercli_assert_nonempty("${out_of_order_text_plan}" NAME "out-of-order text graph replay")
file(READ "${out_of_order_text_plan}" out_of_order_text_graph)
string(FIND "${out_of_order_text_graph}"
       "Execution order:\n- raytrace_beauty\n- tonemap\nDependencies:\n- raytrace_beauty -> tonemap via beauty_color\nPasses:\n- tonemap"
       out_of_order_execution_position)
if(out_of_order_execution_position EQUAL -1)
  message(FATAL_ERROR "text graph export did not show dependency execution order: ${out_of_order_text_graph}")
endif()

rendercli_run(
  NAME "rendercli renders out-of-order replayed JSON graph"
  COMMAND
    "${RENDERCLI}" --render_graph --render_graph_in "${out_of_order_graph}"
    "${static_scene}" "${out_of_order_render}"
)
rendercli_assert_nonempty("${out_of_order_render}" NAME "out-of-order graph replay image")

rendercli_run(
  NAME "rendercli renders through replayed JSON graph with matching explicit size"
  COMMAND
    "${RENDERCLI}" --render_graph --render_graph_in "${json_plan}" --width 32 --height 16
    "${static_scene}" "${replayed_matching_render}"
)
rendercli_assert_nonempty("${replayed_matching_render}" NAME "matching explicit graph replay image")

rendercli_run(
  NAME "rendercli disables optional tonemap pass"
  STDOUT_MATCHES "tonemap \\[tonemap/postprocess\\] disabled"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format text --disable_pass tonemap
    --width 32 --height 16
    "${static_scene}"
)

rendercli_run(
  NAME "rendercli accepts comma-separated and repeated graph disable filters"
  STDOUT_MATCHES
    "wireframe_overlay \\[overlay/wireframe\\] disabled"
    "tonemap \\[tonemap/postprocess\\] disabled"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format text
    --render_graph_wireframe_overlay
    --disable_pass wireframe_overlay,tonemap --disable_pass tonemap
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
  NAME "rendercli rejects direct engine with graph controls"
  STDERR_MATCHES "Cannot combine --direct_engine with render graph options"
  COMMAND
    "${RENDERCLI}" --direct_engine --render_graph_only --render_graph_format text
    "${static_scene}" "${invalid_plan}"
)

rendercli_expect_failure(
  NAME "rendercli rejects graph-only with repeat"
  STDERR_MATCHES "Cannot combine --render_graph_only with --repeat"
  COMMAND
    "${RENDERCLI}" --render_graph_only --repeat 2
    "${static_scene}" "${invalid_plan}"
)

rendercli_expect_failure(
  NAME "rendercli rejects graph-only with trace output"
  STDERR_MATCHES "Cannot combine --render_graph_only with --render_graph_trace_out"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_trace_out "${graph_trace}"
    "${static_scene}" "${invalid_plan}"
)

rendercli_expect_failure(
  NAME "rendercli rejects animation with graph trace output"
  STDERR_MATCHES "Cannot combine --animation with --render_graph_trace_out"
  COMMAND
    "${RENDERCLI}" --animation --render_graph_trace_out "${graph_trace}"
    "${static_scene}" "${invalid_plan}"
)

rendercli_expect_failure(
  NAME "rendercli rejects graph-only with animation"
  STDERR_MATCHES "Cannot combine --animation with --render_graph_only"
  COMMAND
    "${RENDERCLI}" --render_graph_only --animation
    "${static_scene}" "${invalid_plan}"
)

rendercli_expect_failure(
  NAME "rendercli rejects disabled required graph pass"
  STDERR_MATCHES "disabled_required_pass"
  COMMAND
    "${RENDERCLI}" --render_graph_only --disable_pass raytrace_beauty
    "${static_scene}" "${invalid_plan}"
)
