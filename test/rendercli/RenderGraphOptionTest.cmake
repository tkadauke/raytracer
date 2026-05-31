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
set(graph_demo_scene "${PROJECT_SOURCE_DIR}/scenes/render_graph_aov_demo.json")
set(stencil_composite_demo_scene
    "${PROJECT_SOURCE_DIR}/scenes/render_graph_stencil_composite_demo.json")
set(scene_intent_scene "${TEST_OUTPUT_DIR}/scene-intent.json")
set(camera_override_runtime_scene "${TEST_OUTPUT_DIR}/camera-override-runtime-scene.json")
set(default_graph_scene "${TEST_OUTPUT_DIR}/default-graph-scene.json")
set(invalid_exported_aov_scene "${TEST_OUTPUT_DIR}/invalid-exported-aov-scene.json")
set(selector_specific_intent_scene "${TEST_OUTPUT_DIR}/selector-specific-intent-scene.json")
set(subview_intent_scene "${TEST_OUTPUT_DIR}/subview-intent-scene.json")
set(subview_recursion_limit_scene "${TEST_OUTPUT_DIR}/subview-recursion-limit-scene.json")
set(offscreen_culling_scene "${TEST_OUTPUT_DIR}/offscreen-culling-scene.json")
set(material_sidedness_culling_scene "${TEST_OUTPUT_DIR}/material-sidedness-culling-scene.json")
set(malformed_graph_json "${TEST_OUTPUT_DIR}/malformed-graph.json")
set(json_root_graph "${TEST_OUTPUT_DIR}/json-root-graph.json")
set(semantic_invalid_graph "${TEST_OUTPUT_DIR}/semantic-invalid-graph.json")
set(external_input_graph "${TEST_OUTPUT_DIR}/external-input-graph.json")
set(depth_input_graph "${TEST_OUTPUT_DIR}/depth-input-graph.json")
set(stencil_input_graph "${TEST_OUTPUT_DIR}/stencil-input-graph.json")
set(object_id_input_graph "${TEST_OUTPUT_DIR}/object-id-input-graph.json")
set(material_id_input_graph "${TEST_OUTPUT_DIR}/material-id-input-graph.json")
set(depth_composite_graph "${TEST_OUTPUT_DIR}/depth-composite-graph.json")
set(out_of_order_graph "${TEST_OUTPUT_DIR}/out-of-order-graph.json")
set(text_plan "${TEST_OUTPUT_DIR}/graph.txt")
set(dot_plan "${TEST_OUTPUT_DIR}/graph.dot")
set(intent_plan "${TEST_OUTPUT_DIR}/graph-intent.txt")
set(intent_view_plan "${TEST_OUTPUT_DIR}/graph-intent-view.txt")
set(intent_view_override_plan "${TEST_OUTPUT_DIR}/graph-intent-view-override.txt")
set(subview_plan "${TEST_OUTPUT_DIR}/graph-subview.json")
set(depth_view_render "${TEST_OUTPUT_DIR}/graph-depth-view.png")
set(raster_depth_view_render "${TEST_OUTPUT_DIR}/graph-raster-depth-view.png")
set(raster_depth_view_plan "${TEST_OUTPUT_DIR}/graph-raster-depth-view.json")
set(stencil_view_render "${TEST_OUTPUT_DIR}/graph-stencil-view.png")
set(raster_stencil_view_render "${TEST_OUTPUT_DIR}/graph-raster-stencil-view.png")
set(raster_stencil_view_plan "${TEST_OUTPUT_DIR}/graph-raster-stencil-view.json")
set(stencil_composite_view_render "${TEST_OUTPUT_DIR}/graph-stencil-composite-view.png")
set(stencil_composite_view_plan "${TEST_OUTPUT_DIR}/graph-stencil-composite-view.json")
set(normal_view_render "${TEST_OUTPUT_DIR}/graph-normal-view.png")
set(object_id_view_render "${TEST_OUTPUT_DIR}/graph-object-id-view.png")
set(material_id_view_render "${TEST_OUTPUT_DIR}/graph-material-id-view.png")
set(world_position_view_render "${TEST_OUTPUT_DIR}/graph-world-position-view.png")
set(scene_intent_plan "${TEST_OUTPUT_DIR}/graph-scene-intent.json")
set(scene_intent_text_plan "${TEST_OUTPUT_DIR}/graph-scene-intent.txt")
set(active_camera_plan "${TEST_OUTPUT_DIR}/graph-active-camera.json")
set(camera_override_plan "${TEST_OUTPUT_DIR}/graph-camera-override.json")
set(camera_override_runtime_plan "${TEST_OUTPUT_DIR}/graph-camera-runtime-plan.json")
set(camera_override_default_render "${TEST_OUTPUT_DIR}/graph-camera-default-render.png")
set(camera_override_selected_render "${TEST_OUTPUT_DIR}/graph-camera-selected-render.png")
set(camera_override_replayed_render "${TEST_OUTPUT_DIR}/graph-camera-replayed-render.png")
set(shading_profile_override_plan "${TEST_OUTPUT_DIR}/graph-shading-profile-override.json")
set(shading_profile_override_text_plan "${TEST_OUTPUT_DIR}/graph-shading-profile-override.txt")
set(overlay_plan "${TEST_OUTPUT_DIR}/graph-overlay.txt")
set(curve_overlay_plan "${TEST_OUTPUT_DIR}/graph-curve-overlay.txt")
set(json_plan "${TEST_OUTPUT_DIR}/graph.json")
set(replayed_dot_plan "${TEST_OUTPUT_DIR}/graph-replayed.dot")
set(external_input_text_plan "${TEST_OUTPUT_DIR}/graph-external-input.txt")
set(replayed_matching_render "${TEST_OUTPUT_DIR}/graph-replayed-matching-render.png")
set(invalid_plan "${TEST_OUTPUT_DIR}/invalid.txt")
set(default_graph_render "${TEST_OUTPUT_DIR}/default-graph-render.png")
set(direct_engine_render "${TEST_OUTPUT_DIR}/direct-engine-render.png")
set(graph_render "${TEST_OUTPUT_DIR}/graph-render.png")
set(graph_trace "${TEST_OUTPUT_DIR}/graph-trace.json")
set(graph_trace_render "${TEST_OUTPUT_DIR}/graph-trace-render.png")
set(graph_aov_render "${TEST_OUTPUT_DIR}/graph-aov-render.png")
set(graph_aov_depth "${TEST_OUTPUT_DIR}/graph-aov-depth.png")
set(graph_aov_stencil "${TEST_OUTPUT_DIR}/graph-aov-stencil.png")
set(graph_aov_normal "${TEST_OUTPUT_DIR}/graph-aov-normal.png")
set(raster_shadow_trace "${TEST_OUTPUT_DIR}/raster-shadow-trace.json")
set(raster_shadow_trace_render "${TEST_OUTPUT_DIR}/raster-shadow-trace-render.png")
set(raster_state_plan "${TEST_OUTPUT_DIR}/raster-state-graph.json")
set(raster_state_render "${TEST_OUTPUT_DIR}/raster-state-render.png")
set(raytracer_integrator_plan "${TEST_OUTPUT_DIR}/raytracer-integrator-graph.json")
set(raytracer_integrator_render "${TEST_OUTPUT_DIR}/raytracer-integrator-render.png")
set(raster_culling_plan "${TEST_OUTPUT_DIR}/raster-culling-graph.txt")
set(raster_culling_trace "${TEST_OUTPUT_DIR}/raster-culling-trace.json")
set(raster_culling_trace_render "${TEST_OUTPUT_DIR}/raster-culling-trace-render.png")
set(raster_sidedness_culling_trace "${TEST_OUTPUT_DIR}/raster-sidedness-culling-trace.json")
set(raster_sidedness_culling_trace_render
    "${TEST_OUTPUT_DIR}/raster-sidedness-culling-trace-render.png")
set(wireframe_state_plan "${TEST_OUTPUT_DIR}/wireframe-state-graph.json")
set(raytracer_post_aa_plan "${TEST_OUTPUT_DIR}/raytracer-post-aa-graph.txt")
set(wireframe_post_aa_plan "${TEST_OUTPUT_DIR}/wireframe-post-aa-graph.txt")
set(scene_post_aa_none_plan "${TEST_OUTPUT_DIR}/scene-post-aa-none-graph.txt")
set(replayed_render "${TEST_OUTPUT_DIR}/graph-replayed-render.png")
set(out_of_order_text_plan "${TEST_OUTPUT_DIR}/graph-out-of-order.txt")
set(out_of_order_render "${TEST_OUTPUT_DIR}/graph-out-of-order-render.png")
set(mismatched_render "${TEST_OUTPUT_DIR}/graph-mismatched-render.png")
set(external_color_input "${TEST_OUTPUT_DIR}/graph-external-color-input.png")
set(external_input_render "${TEST_OUTPUT_DIR}/graph-external-input-render.png")
set(external_input_bound_render "${TEST_OUTPUT_DIR}/graph-external-input-bound-render.png")
set(depth_input_bound_render "${TEST_OUTPUT_DIR}/graph-depth-input-bound-render.png")
set(stencil_input_bound_render "${TEST_OUTPUT_DIR}/graph-stencil-input-bound-render.png")
set(object_id_input_bound_render "${TEST_OUTPUT_DIR}/graph-object-id-input-bound-render.png")
set(material_id_input_bound_render "${TEST_OUTPUT_DIR}/graph-material-id-input-bound-render.png")
set(depth_composite_render "${TEST_OUTPUT_DIR}/graph-depth-composite-render.png")
set(graph_demo_render "${TEST_OUTPUT_DIR}/graph-demo-render.png")
set(stencil_composite_scene_render "${TEST_OUTPUT_DIR}/graph-stencil-composite-scene-render.png")

file(WRITE "${scene_intent_scene}" [=[
{
  "id": "{90000000-0000-0000-0000-000000000000}",
  "name": "Scene Render Intent Fixture",
  "ambient": [0.4, 0.4, 0.4],
  "background": [0.4, 0.8, 1.0],
  "type": "Scene",
  "renderIntent": {
    "defaultExecutor": "raytracer",
    "defaultViewMode": "beauty",
    "defaultShadingProfile": "toon",
    "enableWireframeOverlay": true,
    "postProcessAA": "smaa",
    "viewOverrides": [
      {
        "selector": {"kind": "all"},
        "executor": "rasterizer",
        "camera": {"sceneCameraId": "inspection-camera"}
      }
    ]
  },
  "children": []
}
]=])

file(WRITE "${camera_override_runtime_scene}" [=[
{
  "id": "{94000000-0000-0000-0000-000000000000}",
  "name": "Camera Override Runtime Fixture",
  "ambient": [1.0, 1.0, 1.0],
  "background": [0.0, 0.0, 0.0],
  "type": "Scene",
  "children": [
    {
      "id": "object-camera",
      "name": "Object Camera",
      "position": [0.0, 0.0, -4.0],
      "target": [0.0, 0.0, 0.0],
      "distance": 5.0,
      "zoom": 1.4,
      "type": "PinholeCamera",
      "children": []
    },
    {
      "id": "empty-camera",
      "name": "Empty Camera",
      "position": [0.0, 0.0, -4.0],
      "target": [8.0, 0.0, 0.0],
      "distance": 5.0,
      "zoom": 1.4,
      "type": "PinholeCamera",
      "children": []
    },
    {
      "id": "red-texture",
      "name": "Red Texture",
      "color": [1.0, 0.0, 0.0],
      "type": "ConstantColorTexture",
      "children": []
    },
    {
      "id": "red-material",
      "name": "Red Material",
      "diffuseTexture": "red-texture",
      "ambientCoefficient": 1.0,
      "diffuseCoefficient": 0.0,
      "type": "MatteMaterial",
      "children": []
    },
    {
      "id": "sphere",
      "name": "Sphere",
      "position": [0.0, 0.0, 0.0],
      "rotation": [0.0, 0.0, 0.0],
      "scale": [1.0, 1.0, 1.0],
      "visible": true,
      "material": "red-material",
      "radius": 1.0,
      "type": "Sphere",
      "children": []
    }
  ]
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

file(WRITE "${invalid_exported_aov_scene}" [=[
{
  "id": "{92000000-0000-0000-0000-000000000000}",
  "name": "Invalid Exported AOV Intent Fixture",
  "ambient": [0.4, 0.4, 0.4],
  "background": [0.4, 0.8, 1.0],
  "type": "Scene",
  "renderIntent": {
    "exportedAOVs": ["beauty"]
  },
  "children": []
}
]=])

file(WRITE "${selector_specific_intent_scene}" [=[
{
  "id": "{93000000-0000-0000-0000-000000000000}",
  "name": "Selector Specific Intent Fixture",
  "ambient": [0.4, 0.4, 0.4],
  "background": [0.4, 0.8, 1.0],
  "type": "Scene",
  "renderIntent": {
    "viewOverrides": [
      {
        "selector": {"kind": "object_name", "value": "Monitor"},
        "executor": "wireframe"
      }
    ]
  },
  "children": []
}
]=])

file(WRITE "${subview_intent_scene}" [=[
{
  "id": "{94000000-0000-0000-0000-000000000000}",
  "name": "Subview Intent Fixture",
  "ambient": [0.4, 0.4, 0.4],
  "background": [0.4, 0.8, 1.0],
  "type": "Scene",
  "renderIntent": {
    "engineOptions": {
      "rasterizer": {
        "execution": {"backend": "opengl"}
      }
    },
    "subviews": [
      {
        "name": "mirror_probe",
        "view": {
          "selector": {"kind": "all"},
          "executor": "rasterizer"
        }
      }
    ]
  },
  "children": []
}
]=])

file(WRITE "${subview_recursion_limit_scene}" [=[
{
  "id": "{94000000-0000-0000-0000-000000000001}",
  "name": "Subview Recursion Limit Fixture",
  "ambient": [0.4, 0.4, 0.4],
  "background": [0.4, 0.8, 1.0],
  "type": "Scene",
  "renderIntent": {
    "maxRenderToTextureRecursionDepth": 0,
    "subviews": [
      {
        "name": "mirror_probe",
        "view": {
          "selector": {"kind": "all"},
          "executor": "rasterizer"
        }
      }
    ]
  },
  "children": []
}
]=])

file(WRITE "${offscreen_culling_scene}" [=[
{
  "id": "{95000000-0000-0000-0000-000000000000}",
  "name": "Offscreen Culling Fixture",
  "ambient": [0.6, 0.6, 0.6],
  "background": [0.0, 0.0, 0.0],
  "type": "Scene",
  "children": [
    {
      "id": "camera",
      "name": "Camera",
      "position": [0.0, 0.0, -4.0],
      "target": [0.0, 0.0, 0.0],
      "distance": 5.0,
      "zoom": 1.2,
      "type": "PinholeCamera",
      "children": []
    },
    {
      "id": "red-texture",
      "name": "Red Texture",
      "color": [1.0, 0.0, 0.0],
      "type": "ConstantColorTexture",
      "children": []
    },
    {
      "id": "red-matte",
      "name": "Red Matte",
      "diffuseTexture": "red-texture",
      "ambientCoefficient": 1.0,
      "diffuseCoefficient": 1.0,
      "type": "MatteMaterial",
      "children": []
    },
    {
      "id": "visible-box",
      "name": "Visible Box",
      "position": [0.0, 0.0, 0.0],
      "rotation": [0.0, 0.0, 0.0],
      "scale": [1.0, 1.0, 1.0],
      "visible": true,
      "material": "red-matte",
      "size": [1.0, 1.0, 1.0],
      "bevelRadius": 0.0,
      "type": "Box",
      "children": []
    },
    {
      "id": "offscreen-box",
      "name": "Offscreen Box",
      "position": [1000.0, 0.0, 0.0],
      "rotation": [0.0, 0.0, 0.0],
      "scale": [1.0, 1.0, 1.0],
      "visible": true,
      "material": "red-matte",
      "size": [1.0, 1.0, 1.0],
      "bevelRadius": 0.0,
      "type": "Box",
      "children": []
    }
  ]
}
]=])

file(WRITE "${material_sidedness_culling_scene}" [=[
{
  "id": "{96000000-0000-0000-0000-000000000000}",
  "name": "Material Sidedness Culling Fixture",
  "ambient": [1.0, 1.0, 1.0],
  "background": [0.0, 0.0, 0.0],
  "type": "Scene",
  "children": [
    {
      "id": "camera",
      "name": "Camera",
      "position": [0.0, 0.0, -4.0],
      "target": [0.0, 0.0, 0.0],
      "distance": 5.0,
      "zoom": 1.2,
      "type": "PinholeCamera",
      "children": []
    },
    {
      "id": "white-texture",
      "name": "White Texture",
      "color": [1.0, 1.0, 1.0],
      "type": "ConstantColorTexture",
      "children": []
    },
    {
      "id": "front-matte",
      "name": "Front Sided Matte",
      "diffuseTexture": "white-texture",
      "ambientCoefficient": 1.0,
      "diffuseCoefficient": 1.0,
      "sidedness": "Front",
      "type": "MatteMaterial",
      "children": []
    },
    {
      "id": "front-facing",
      "name": "Front Facing Triangle",
      "position": [0.0, 0.0, 0.0],
      "rotation": [0.0, 0.0, 0.0],
      "scale": [1.0, 1.0, 1.0],
      "visible": true,
      "material": "front-matte",
      "vertexA": [-1.5, -1.0, 0.0],
      "vertexB": [-0.5, 1.0, 0.0],
      "vertexC": [0.5, -1.0, 0.0],
      "type": "Triangle",
      "children": []
    },
    {
      "id": "back-facing",
      "name": "Back Facing Triangle",
      "position": [0.0, 0.0, 0.0],
      "rotation": [0.0, 0.0, 0.0],
      "scale": [1.0, 1.0, 1.0],
      "visible": true,
      "material": "front-matte",
      "vertexA": [-0.5, -1.0, 0.0],
      "vertexB": [1.5, -1.0, 0.0],
      "vertexC": [0.5, 1.0, 0.0],
      "type": "Triangle",
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

file(WRITE "${external_input_graph}" [=[
{
  "resources": [
    {
      "id": "history_color",
      "name": "History color",
      "type": "color",
      "format": "rgb_double",
      "width": 32,
      "height": 16,
      "sampleCount": 1,
      "domain": "cpu",
      "lifetime": "history"
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
      "reads": ["history_color"],
      "writes": ["main_color"],
      "disabledBehavior": "passthrough",
      "enabled": true,
      "hasExternalSideEffects": false,
      "canRunConcurrently": false
    }
  ]
}
]=])

file(WRITE "${depth_input_graph}" [=[
{
  "resources": [
    {
      "id": "history_depth",
      "name": "History depth",
      "type": "depth",
      "format": "depth_double",
      "width": 32,
      "height": 16,
      "sampleCount": 1,
      "domain": "cpu",
      "lifetime": "history"
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
      "id": "visualize_depth",
      "name": "Visualize depth",
      "kind": "aov",
      "executor": "postprocess",
      "features": ["depth", "visualization"],
      "reads": ["history_depth"],
      "writes": ["main_color"],
      "disabledBehavior": "error",
      "enabled": true,
      "hasExternalSideEffects": false,
      "canRunConcurrently": false
    }
  ]
}
]=])

file(WRITE "${stencil_input_graph}" [=[
{
  "resources": [
    {
      "id": "base_color",
      "name": "Base color",
      "type": "color",
      "format": "rgb_double",
      "width": 32,
      "height": 16,
      "sampleCount": 1,
      "domain": "cpu",
      "lifetime": "transient"
    },
    {
      "id": "foreground_color",
      "name": "Foreground color",
      "type": "color",
      "format": "rgb_double",
      "width": 32,
      "height": 16,
      "sampleCount": 1,
      "domain": "cpu",
      "lifetime": "transient"
    },
    {
      "id": "stencil_mask",
      "name": "Stencil mask",
      "type": "stencil",
      "format": "uint8",
      "width": 32,
      "height": 16,
      "sampleCount": 1,
      "domain": "cpu",
      "lifetime": "history"
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
      "id": "raytrace_beauty",
      "name": "Raytraced beauty",
      "kind": "beauty",
      "executor": "raytracer",
      "features": ["main", "beauty", "raytracer"],
      "reads": [],
      "writes": ["base_color"],
      "disabledBehavior": "error",
      "enabled": true,
      "hasExternalSideEffects": false,
      "canRunConcurrently": false
    },
    {
      "id": "wireframe_beauty",
      "name": "Wireframe beauty",
      "kind": "beauty",
      "executor": "wireframe",
      "features": ["foreground", "beauty", "wireframe"],
      "reads": [],
      "writes": ["foreground_color"],
      "disabledBehavior": "error",
      "enabled": true,
      "hasExternalSideEffects": false,
      "canRunConcurrently": false
    },
    {
      "id": "stencil_composite",
      "name": "Stencil composite",
      "kind": "composite",
      "executor": "composite",
      "features": ["stencil_composite"],
      "reads": ["base_color", "foreground_color", "stencil_mask"],
      "writes": ["main_color"],
      "disabledBehavior": "error",
      "enabled": true,
      "hasExternalSideEffects": false,
      "canRunConcurrently": false
    }
  ]
}
]=])

file(WRITE "${object_id_input_graph}" [=[
{
  "resources": [
    {
      "id": "history_object_id",
      "name": "History object ID",
      "type": "object_id",
      "format": "uint32",
      "width": 32,
      "height": 16,
      "sampleCount": 1,
      "domain": "cpu",
      "lifetime": "history"
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
      "id": "visualize_object_id",
      "name": "Visualize object ID",
      "kind": "aov",
      "executor": "postprocess",
      "features": ["object_id", "visualization"],
      "reads": ["history_object_id"],
      "writes": ["main_color"],
      "disabledBehavior": "error",
      "enabled": true,
      "hasExternalSideEffects": false,
      "canRunConcurrently": false
    }
  ]
}
]=])

file(WRITE "${material_id_input_graph}" [=[
{
  "resources": [
    {
      "id": "history_material_id",
      "name": "History material ID",
      "type": "material_id",
      "format": "uint32",
      "width": 32,
      "height": 16,
      "sampleCount": 1,
      "domain": "cpu",
      "lifetime": "history"
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
      "id": "visualize_material_id",
      "name": "Visualize material ID",
      "kind": "aov",
      "executor": "postprocess",
      "features": ["material_id", "visualization"],
      "reads": ["history_material_id"],
      "writes": ["main_color"],
      "disabledBehavior": "error",
      "enabled": true,
      "hasExternalSideEffects": false,
      "canRunConcurrently": false
    }
  ]
}
]=])

file(WRITE "${depth_composite_graph}" [=[
{
  "resources": [
    {
      "id": "base_color",
      "name": "Base color",
      "type": "color",
      "format": "rgb_double",
      "width": 32,
      "height": 16,
      "sampleCount": 1,
      "domain": "cpu",
      "lifetime": "transient"
    },
    {
      "id": "foreground_color",
      "name": "Foreground color",
      "type": "color",
      "format": "rgb_double",
      "width": 32,
      "height": 16,
      "sampleCount": 1,
      "domain": "cpu",
      "lifetime": "transient"
    },
    {
      "id": "base_depth",
      "name": "Base depth",
      "type": "depth",
      "format": "depth_double",
      "width": 32,
      "height": 16,
      "sampleCount": 1,
      "domain": "cpu",
      "lifetime": "transient"
    },
    {
      "id": "foreground_depth",
      "name": "Foreground depth",
      "type": "depth",
      "format": "depth_double",
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
      "id": "raytrace_beauty",
      "name": "Raytraced beauty",
      "kind": "beauty",
      "executor": "raytracer",
      "features": ["main", "beauty", "raytracer"],
      "reads": [],
      "writes": ["base_color"],
      "disabledBehavior": "error",
      "enabled": true,
      "hasExternalSideEffects": false,
      "canRunConcurrently": false
    },
    {
      "id": "wireframe_beauty",
      "name": "Wireframe beauty",
      "kind": "beauty",
      "executor": "wireframe",
      "features": ["foreground", "beauty", "wireframe"],
      "reads": [],
      "writes": ["foreground_color"],
      "disabledBehavior": "error",
      "enabled": true,
      "hasExternalSideEffects": false,
      "canRunConcurrently": false
    },
    {
      "id": "base_depth_aov",
      "name": "Base depth AOV",
      "kind": "aov",
      "executor": "raytracer",
      "features": ["depth"],
      "reads": [],
      "writes": ["base_depth"],
      "disabledBehavior": "substitute_default",
      "enabled": true,
      "hasExternalSideEffects": false,
      "canRunConcurrently": false
    },
    {
      "id": "foreground_depth_aov",
      "name": "Foreground depth AOV",
      "kind": "aov",
      "executor": "wireframe",
      "features": ["depth"],
      "reads": [],
      "writes": ["foreground_depth"],
      "disabledBehavior": "substitute_default",
      "enabled": true,
      "hasExternalSideEffects": false,
      "canRunConcurrently": false
    },
    {
      "id": "depth_composite",
      "name": "Depth composite",
      "kind": "composite",
      "executor": "composite",
      "features": ["depth_composite"],
      "reads": ["base_color", "foreground_color", "base_depth", "foreground_depth"],
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
if(NOT text_graph MATCHES "Execution stages")
  message(FATAL_ERROR "text graph export did not contain execution stages: ${text_graph}")
endif()
if(NOT text_graph MATCHES "schedule: stage=1, order=1")
  message(FATAL_ERROR "text graph export did not contain the first pass schedule: ${text_graph}")
endif()
if(NOT text_graph MATCHES "schedule: stage=2, order=2")
  message(FATAL_ERROR "text graph export did not contain the second pass schedule: ${text_graph}")
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
if(NOT dot_graph MATCHES "execution_stage_1")
  message(FATAL_ERROR "DOT graph export did not contain execution stage rank hints: ${dot_graph}")
endif()
if(NOT dot_graph MATCHES "camera 2")
  message(FATAL_ERROR "DOT graph export did not contain scene camera metadata: ${dot_graph}")
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
  NAME "rendercli graph view override intent selects whole frame"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format text
    --render_graph_view_override "all,executor=wireframe,view=wireframe,camera=camera 2,shading_profile=toon,parameter:levels=3"
    --width 32 --height 16
    "${static_scene}" "${intent_view_override_plan}"
)
rendercli_assert_nonempty("${intent_view_override_plan}"
                          NAME "graph view override intent output")
file(READ "${intent_view_override_plan}" intent_view_override_graph)
if(NOT intent_view_override_graph MATCHES "wireframe_beauty")
  message(FATAL_ERROR
    "graph view override intent did not select wireframe_beauty: ${intent_view_override_graph}")
endif()
if(NOT intent_view_override_graph MATCHES "camera=camera 2")
  message(FATAL_ERROR
    "graph view override intent did not carry camera metadata: ${intent_view_override_graph}")
endif()
if(NOT intent_view_override_graph MATCHES "shading=toon\\(levels=3")
  message(FATAL_ERROR
    "graph view override intent did not carry shading metadata: ${intent_view_override_graph}")
endif()

rendercli_run(
  NAME "rendercli uses scene render intent"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format json
    --width 32 --height 16 --msaa 4 --msaa_shading per_fragment
    "${scene_intent_scene}" "${scene_intent_plan}"
)
rendercli_assert_nonempty("${scene_intent_plan}" NAME "scene render intent graph output")
file(READ "${scene_intent_plan}" scene_intent_graph)
if(NOT scene_intent_graph MATCHES "raster_beauty")
  message(FATAL_ERROR "scene render intent did not select raster_beauty: ${scene_intent_graph}")
endif()
if(scene_intent_graph MATCHES "raytrace_beauty")
  message(FATAL_ERROR "whole-frame scene override should replace raytrace_beauty: ${scene_intent_graph}")
endif()
if(NOT scene_intent_graph MATCHES "wireframe_overlay")
  message(FATAL_ERROR "scene render intent did not add wireframe_overlay: ${scene_intent_graph}")
endif()
if(NOT scene_intent_graph MATCHES "post_smaa")
  message(FATAL_ERROR "scene render intent did not add post_smaa: ${scene_intent_graph}")
endif()
if(NOT scene_intent_graph MATCHES "\"sceneCameraId\": \"inspection-camera\"")
  message(FATAL_ERROR "scene render intent did not carry the override camera: ${scene_intent_graph}")
endif()
if(NOT scene_intent_graph MATCHES "\"sceneShadingProfile\"")
  message(FATAL_ERROR "scene render intent did not carry the shading profile: ${scene_intent_graph}")
endif()
if(NOT scene_intent_graph MATCHES "\"name\": \"toon\"")
  message(FATAL_ERROR "scene render intent did not carry the toon shading profile: ${scene_intent_graph}")
endif()
if(NOT scene_intent_graph MATCHES "\"sampleCount\": 4")
  message(FATAL_ERROR "whole-frame scene override did not select raster MSAA resources: ${scene_intent_graph}")
endif()
if(NOT scene_intent_graph MATCHES "msaaSamples")
  message(FATAL_ERROR "whole-frame scene override did not write raster pass state: ${scene_intent_graph}")
endif()
if(NOT scene_intent_graph MATCHES "per_fragment")
  message(FATAL_ERROR "whole-frame scene override did not write raster MSAA shading state: ${scene_intent_graph}")
endif()

rendercli_run(
  NAME "rendercli text graph shows scene view intent"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format text
    --width 32 --height 16
    "${scene_intent_scene}" "${scene_intent_text_plan}"
)
rendercli_assert_nonempty("${scene_intent_text_plan}" NAME "scene render intent text output")
file(READ "${scene_intent_text_plan}" scene_intent_text_graph)
if(NOT scene_intent_text_graph MATCHES "scene: selector=all, camera=inspection-camera")
  message(FATAL_ERROR "text graph did not include scene view intent: ${scene_intent_text_graph}")
endif()

rendercli_run(
  NAME "rendercli graph intent uses active scene camera"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format json
    --width 32 --height 16
    "${static_scene}" "${active_camera_plan}"
)
rendercli_assert_nonempty("${active_camera_plan}" NAME "active camera graph output")
file(READ "${active_camera_plan}" active_camera_graph)
if(NOT active_camera_graph MATCHES "\"sceneCameraId\": \"2\"")
  message(FATAL_ERROR "graph output did not carry the active scene camera: ${active_camera_graph}")
endif()

rendercli_run(
  NAME "rendercli graph camera override selects scene camera intent"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format json
    --render_graph_camera command-camera
    --width 32 --height 16
    "${static_scene}" "${camera_override_plan}"
)
rendercli_assert_nonempty("${camera_override_plan}" NAME "camera override graph output")
file(READ "${camera_override_plan}" camera_override_graph)
if(NOT camera_override_graph MATCHES "\"sceneCameraId\": \"command-camera\"")
  message(FATAL_ERROR "graph camera override did not carry command-camera: ${camera_override_graph}")
endif()

rendercli_run(
  NAME "rendercli graph camera override selects execution camera"
  COMMAND
    "${RENDERCLI}" --width 32 --height 16
    "${camera_override_runtime_scene}" "${camera_override_default_render}"
)
rendercli_assert_image_dimensions("${camera_override_default_render}" 32 16
                                  NAME "default graph camera render dimensions")

rendercli_run(
  NAME "rendercli graph camera override changes rendered camera"
  COMMAND
    "${RENDERCLI}" --render_graph_camera object-camera --render_graph_out "${camera_override_runtime_plan}"
    --render_graph_format json
    --width 32 --height 16
    "${camera_override_runtime_scene}" "${camera_override_selected_render}"
)
rendercli_assert_image_dimensions("${camera_override_selected_render}" 32 16
                                  NAME "selected graph camera render dimensions")
rendercli_assert_nonempty("${camera_override_runtime_plan}" NAME "selected graph camera plan")
rendercli_assert_image_hash_differs("${camera_override_default_render}"
                                    "${camera_override_selected_render}"
                                    NAME "graph camera override render")

rendercli_run(
  NAME "rendercli graph replay uses plan camera"
  COMMAND
    "${RENDERCLI}" --render_graph_in "${camera_override_runtime_plan}"
    "${camera_override_runtime_scene}" "${camera_override_replayed_render}"
)
rendercli_assert_image_dimensions("${camera_override_replayed_render}" 32 16
                                  NAME "replayed graph camera render dimensions")
rendercli_assert_image_hash_equals("${camera_override_selected_render}"
                                   "${camera_override_replayed_render}"
                                   NAME "replayed graph camera render")

rendercli_run(
  NAME "rendercli graph shading profile override selects scene profile intent"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format json
    --render_graph_shading_profile clay
    --render_graph_shading_parameter levels=4
    --render_graph_shading_parameter enabled=true
    --render_graph_shading_parameter ramp=warm
    --width 32 --height 16
    "${static_scene}" "${shading_profile_override_plan}"
)
rendercli_assert_nonempty("${shading_profile_override_plan}" NAME "shading profile override graph output")
file(READ "${shading_profile_override_plan}" shading_profile_override_graph)
if(NOT shading_profile_override_graph MATCHES "\"sceneShadingProfile\"")
  message(FATAL_ERROR "graph shading profile override did not write sceneShadingProfile: ${shading_profile_override_graph}")
endif()
if(NOT shading_profile_override_graph MATCHES "\"name\": \"clay\"")
  message(FATAL_ERROR "graph shading profile override did not carry clay: ${shading_profile_override_graph}")
endif()
if(NOT shading_profile_override_graph MATCHES "\"parameters\"")
  message(FATAL_ERROR "graph shading profile override did not write parameters: ${shading_profile_override_graph}")
endif()
if(NOT shading_profile_override_graph MATCHES "\"levels\": 4")
  message(FATAL_ERROR "graph shading profile override did not carry numeric parameter: ${shading_profile_override_graph}")
endif()
if(NOT shading_profile_override_graph MATCHES "\"enabled\": true")
  message(FATAL_ERROR "graph shading profile override did not carry bool parameter: ${shading_profile_override_graph}")
endif()
if(NOT shading_profile_override_graph MATCHES "\"ramp\": \"warm\"")
  message(FATAL_ERROR "graph shading profile override did not carry string parameter: ${shading_profile_override_graph}")
endif()

rendercli_run(
  NAME "rendercli text graph shows shading profile parameters"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format text
    --render_graph_shading_profile clay
    --render_graph_shading_parameter levels=4
    --width 32 --height 16
    "${static_scene}" "${shading_profile_override_text_plan}"
)
rendercli_assert_nonempty("${shading_profile_override_text_plan}" NAME "shading profile text graph output")
file(READ "${shading_profile_override_text_plan}" shading_profile_override_text_graph)
if(NOT shading_profile_override_text_graph MATCHES "shading=clay\\(levels=4\\)")
  message(FATAL_ERROR "text graph did not show shading profile parameters: ${shading_profile_override_text_graph}")
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
  NAME "rendercli graph curve overlay intent adds overlay pass"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format text
    --render_graph_curve_overlay --width 32 --height 16
    "${static_scene}" "${curve_overlay_plan}"
)
rendercli_assert_nonempty("${curve_overlay_plan}" NAME "graph curve overlay output")
file(READ "${curve_overlay_plan}" curve_overlay_graph)
if(NOT curve_overlay_graph MATCHES "curve_overlay")
  message(FATAL_ERROR "graph curve overlay intent did not add curve_overlay: ${curve_overlay_graph}")
endif()
if(NOT curve_overlay_graph MATCHES "curve_overlay_color")
  message(FATAL_ERROR "graph curve overlay intent did not add curve_overlay_color: ${curve_overlay_graph}")
endif()
if(NOT curve_overlay_graph MATCHES "tonemap")
  message(FATAL_ERROR "graph curve overlay intent did not retain tonemap: ${curve_overlay_graph}")
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
file(READ "${json_plan}" json_graph)
if(NOT json_graph MATCHES "executionStages")
  message(FATAL_ERROR "JSON graph export did not contain executionStages: ${json_graph}")
endif()

rendercli_run(
  NAME "rendercli exports raytracer integrator state in render graph"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format json
    --engine raytracer --integrator pathtracer --width 32 --height 16
    "${static_scene}" "${raytracer_integrator_plan}"
)
rendercli_assert_nonempty("${raytracer_integrator_plan}" NAME "raytracer integrator graph output")
file(READ "${raytracer_integrator_plan}" raytracer_integrator_graph)
if(NOT raytracer_integrator_graph MATCHES "\"integrator\": \"pathtracer\"")
  message(FATAL_ERROR
          "raytracer integrator graph did not contain pathtracer state: ${raytracer_integrator_graph}")
endif()

rendercli_run(
  NAME "rendercli renders graph raytracer with pathtracer integrator"
  COMMAND
    "${RENDERCLI}" --engine raytracer --integrator pathtracer --width 16 --height 16
    "${static_scene}" "${raytracer_integrator_render}"
)
rendercli_assert_image_nonempty("${raytracer_integrator_render}"
                                NAME "raytracer pathtracer graph render pixels")

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

rendercli_expect_failure(
  NAME "rendercli rejects invalid render graph shading parameter"
  STDERR_MATCHES "Render graph shading parameter must use key=value syntax"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_shading_parameter levels
    "${static_scene}" "${invalid_plan}"
)

rendercli_expect_failure(
  NAME "rendercli rejects malformed render graph view override"
  STDERR_MATCHES "Render graph view override must use selector,key=value syntax"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_view_override "tag:debug"
    "${static_scene}" "${invalid_plan}"
)

rendercli_expect_failure(
  NAME "rendercli rejects invalid render graph view override selector"
  STDERR_MATCHES "Render graph view override selector must use"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_view_override "unknown:debug,view=wireframe"
    "${static_scene}" "${invalid_plan}"
)

rendercli_expect_failure(
  NAME "rendercli rejects scene intent non-AOV export"
  STDERR_MATCHES "renderIntent.exportedAOVs"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format json
    "${invalid_exported_aov_scene}" "${invalid_plan}"
)

rendercli_expect_failure(
  NAME "rendercli rejects unsupported selector-specific scene intent"
  STDERR_MATCHES "selector-specific render intent"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format json
    "${selector_specific_intent_scene}" "${invalid_plan}"
)

rendercli_run(
  NAME "rendercli exports scene subview render graph"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format json
    --width 32 --height 16
    "${subview_intent_scene}" "${subview_plan}"
)
rendercli_assert_nonempty("${subview_plan}" NAME "scene subview graph output")
file(READ "${subview_plan}" subview_graph)
if(NOT subview_graph MATCHES "subview_mirror_probe_raster_beauty")
  message(FATAL_ERROR "scene subview intent did not compile a raster branch: ${subview_graph}")
endif()
if(NOT subview_graph MATCHES "subview_mirror_probe_beauty_readback")
  message(FATAL_ERROR "OpenGL scene subview intent did not route beauty through readback: ${subview_graph}")
endif()
if(NOT subview_graph MATCHES "subview_mirror_probe_main_color")
  message(FATAL_ERROR "scene subview intent did not export a subview color: ${subview_graph}")
endif()
if(NOT subview_graph MATCHES "subview_mirror_probe_depth_aov")
  message(FATAL_ERROR "scene subview intent did not export a subview depth resource: ${subview_graph}")
endif()
if(NOT subview_graph MATCHES "subview_mirror_probe_readback_depth_aov")
  message(FATAL_ERROR "OpenGL scene subview intent did not route depth through readback: ${subview_graph}")
endif()
if(NOT subview_graph MATCHES "render_to_texture")
  message(FATAL_ERROR "scene subview intent did not mark render-to-texture features: ${subview_graph}")
endif()

rendercli_expect_failure(
  NAME "rendercli rejects subview recursion limit"
  STDERR_MATCHES "render-to-texture recursion limit 0 reached.*mirror_probe"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format json
    "${subview_recursion_limit_scene}" "${invalid_plan}"
)

rendercli_expect_failure(
  NAME "rendercli rejects unsupported selector-specific CLI intent"
  STDERR_MATCHES "selector-specific render intent.*tag: debug"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_view_override "tag:debug,view=wireframe"
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
  NAME "rendercli renders raster depth AOV view through graph"
  COMMAND
    "${RENDERCLI}" --engine raster --render_graph_view depth --render_graph_format json
    --render_graph_out "${raster_depth_view_plan}"
    --width 32 --height 24 --msaa 4 --msaa_shading per_fragment
    "${static_scene}" "${raster_depth_view_render}"
)
rendercli_assert_nonempty("${raster_depth_view_render}" NAME "raster depth AOV graph render output")
rendercli_assert_nonempty("${raster_depth_view_plan}" NAME "raster depth AOV graph plan")
file(READ "${raster_depth_view_plan}" raster_depth_view_graph)
if(NOT raster_depth_view_graph MATCHES "depth_aov")
  message(FATAL_ERROR "raster depth AOV graph did not contain depth_aov: ${raster_depth_view_graph}")
endif()
if(NOT raster_depth_view_graph MATCHES "msaaSamples")
  message(FATAL_ERROR "raster depth AOV graph did not contain raster sampling state: ${raster_depth_view_graph}")
endif()

set(raster_counter_view_render "${TEST_OUTPUT_DIR}/raster-depth-test-count-view.png")
set(raster_counter_view_plan "${TEST_OUTPUT_DIR}/raster-depth-test-count-view.json")
rendercli_run(
  NAME "rendercli renders raster counter AOV view through graph"
  COMMAND
    "${RENDERCLI}" --engine raster --render_graph_view raster_depth_test_count
    --render_graph_format json --render_graph_out "${raster_counter_view_plan}"
    --width 32 --height 24
    "${static_scene}" "${raster_counter_view_render}"
)
rendercli_assert_nonempty("${raster_counter_view_render}" NAME "raster counter AOV graph render output")
rendercli_assert_nonempty("${raster_counter_view_plan}" NAME "raster counter AOV graph plan")
file(READ "${raster_counter_view_plan}" raster_counter_view_graph)
if(NOT raster_counter_view_graph MATCHES "raster_depth_test_count_aov")
  message(FATAL_ERROR "raster counter AOV graph did not contain raster_depth_test_count_aov: ${raster_counter_view_graph}")
endif()

rendercli_run(
  NAME "rendercli exports stencil AOV render graph"
  STDOUT_MATCHES
    "stencil_aov"
    "visualize_stencil_aov"
    "main_color"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_view stencil
    --width 32 --height 24
    "${static_scene}"
)

rendercli_run(
  NAME "rendercli renders stencil AOV view through graph"
  COMMAND
    "${RENDERCLI}" --render_graph_view stencil --width 32 --height 24
    "${static_scene}" "${stencil_view_render}"
)
rendercli_assert_nonempty("${stencil_view_render}" NAME "stencil AOV graph render output")

rendercli_run(
  NAME "rendercli renders raster stencil AOV view through graph"
  COMMAND
    "${RENDERCLI}" --engine raster --render_graph_view stencil --render_graph_format json
    --render_graph_out "${raster_stencil_view_plan}"
    --width 32 --height 24 --msaa 4 --msaa_shading per_fragment
    "${static_scene}" "${raster_stencil_view_render}"
)
rendercli_assert_nonempty("${raster_stencil_view_render}"
                          NAME "raster stencil AOV graph render output")
rendercli_assert_nonempty("${raster_stencil_view_plan}" NAME "raster stencil AOV graph plan")
file(READ "${raster_stencil_view_plan}" raster_stencil_view_graph)
if(NOT raster_stencil_view_graph MATCHES "stencil_aov")
  message(FATAL_ERROR "raster stencil AOV graph did not contain stencil_aov: ${raster_stencil_view_graph}")
endif()

rendercli_run(
  NAME "rendercli exports stencil composite view render graph"
  STDOUT_MATCHES
    "raster_beauty"
    "wireframe_beauty"
    "stencil_aov"
    "stencil_composite"
    "tonemap"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_view stencil_composite
    --width 32 --height 24
    "${static_scene}"
)

rendercli_run(
  NAME "rendercli renders stencil composite view through graph"
  COMMAND
    "${RENDERCLI}" --render_graph_view stencil_composite --render_graph_aov_out
    "stencil=${graph_aov_stencil}" --render_graph_format json
    --render_graph_out "${stencil_composite_view_plan}"
    --width 32 --height 24
    "${static_scene}" "${stencil_composite_view_render}"
)
rendercli_assert_nonempty("${stencil_composite_view_render}"
                          NAME "stencil composite graph render output")
rendercli_assert_nonempty("${stencil_composite_view_plan}"
                          NAME "stencil composite graph plan")
file(READ "${stencil_composite_view_plan}" stencil_composite_view_graph)
if(NOT stencil_composite_view_graph MATCHES "stencil_aov_color")
  message(FATAL_ERROR
          "stencil composite graph did not contain exported stencil preview: ${stencil_composite_view_graph}")
endif()

rendercli_run(
  NAME "rendercli renders stencil composite scene intent"
  COMMAND
    "${RENDERCLI}" --width 64 --height 36
    "${stencil_composite_demo_scene}" "${stencil_composite_scene_render}"
)
rendercli_assert_nonempty("${stencil_composite_scene_render}"
                          NAME "stencil composite scene render output")

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
    --render_graph_aov_out "stencil=${graph_aov_stencil}"
    --render_graph_aov_out "normal=${graph_aov_normal}" --width 32 --height 24
    "${static_scene}" "${graph_aov_render}"
)
rendercli_assert_nonempty("${graph_aov_render}" NAME "multi AOV graph main render output")
rendercli_assert_nonempty("${graph_aov_depth}" NAME "multi AOV graph depth output")
rendercli_assert_nonempty("${graph_aov_stencil}" NAME "multi AOV graph stencil output")
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
  NAME "rendercli rejects malformed graph color input"
  STDERR_MATCHES "--render_graph_color_in must use resource=file syntax"
  COMMAND
    "${RENDERCLI}" --render_graph_color_in history_color
    "${static_scene}" "${invalid_plan}"
)

rendercli_expect_failure(
  NAME "rendercli rejects malformed graph depth input"
  STDERR_MATCHES "--render_graph_depth_in must use resource=file syntax"
  COMMAND
    "${RENDERCLI}" --render_graph_depth_in history_depth
    "${static_scene}" "${invalid_plan}"
)

rendercli_expect_failure(
  NAME "rendercli rejects malformed graph stencil input"
  STDERR_MATCHES "--render_graph_stencil_in must use resource=file syntax"
  COMMAND
    "${RENDERCLI}" --render_graph_stencil_in stencil_mask
    "${static_scene}" "${invalid_plan}"
)

rendercli_expect_failure(
  NAME "rendercli rejects malformed graph object-id input"
  STDERR_MATCHES "--render_graph_object_id_in must use resource=file syntax"
  COMMAND
    "${RENDERCLI}" --render_graph_object_id_in history_object_id
    "${static_scene}" "${invalid_plan}"
)

rendercli_expect_failure(
  NAME "rendercli rejects malformed graph material-id input"
  STDERR_MATCHES "--render_graph_material_id_in must use resource=file syntax"
  COMMAND
    "${RENDERCLI}" --render_graph_material_id_in history_material_id
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
  NAME "rendercli rejects invalid raster visibility culling mode"
  STDERR_MATCHES "Raster visibility culling must be"
  COMMAND
    "${RENDERCLI}" --render_graph_only --raster_culling maybe
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
  NAME "rendercli graph-only accepts imported external inputs"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_in "${external_input_graph}"
    --render_graph_format text
    "${static_scene}" "${external_input_text_plan}"
)
rendercli_assert_nonempty("${external_input_text_plan}"
                          NAME "external-input graph-only output")
file(READ "${external_input_text_plan}" external_input_graph_text)
if(NOT external_input_graph_text MATCHES "history_color")
  message(FATAL_ERROR "external-input graph text did not contain history_color: ${external_input_graph_text}")
endif()
if(NOT external_input_graph_text MATCHES "history")
  message(FATAL_ERROR "external-input graph text did not mark the history lifetime: ${external_input_graph_text}")
endif()

rendercli_expect_failure(
  NAME "rendercli rejects unbound imported render graph inputs"
  STDERR_MATCHES "external resource 'history_color'.*was not bound"
  COMMAND
    "${RENDERCLI}" --render_graph --render_graph_in "${external_input_graph}"
    "${static_scene}" "${external_input_render}"
)
rendercli_assert_not_exists("${external_input_render}" NAME "unbound external input render output")

rendercli_run(
  NAME "rendercli prepares external color input image"
  COMMAND
    "${RENDERCLI}" --width 32 --height 16
    "${static_scene}" "${external_color_input}"
)
rendercli_assert_image_dimensions("${external_color_input}" 32 16
                                  NAME "external color input dimensions")

rendercli_run(
  NAME "rendercli binds imported color graph inputs"
  COMMAND
    "${RENDERCLI}" --render_graph --render_graph_in "${external_input_graph}"
    --render_graph_color_in "history_color=${external_color_input}"
    "${static_scene}" "${external_input_bound_render}"
)
rendercli_assert_image_dimensions("${external_input_bound_render}" 32 16
                                  NAME "bound external input render dimensions")
rendercli_assert_image_nonempty("${external_input_bound_render}"
                                NAME "bound external input render pixels")

rendercli_run(
  NAME "rendercli binds imported depth graph inputs"
  COMMAND
    "${RENDERCLI}" --render_graph --render_graph_in "${depth_input_graph}"
    --render_graph_depth_in "history_depth=${external_color_input}"
    "${static_scene}" "${depth_input_bound_render}"
)
rendercli_assert_image_dimensions("${depth_input_bound_render}" 32 16
                                  NAME "bound depth input render dimensions")
rendercli_assert_image_nonempty("${depth_input_bound_render}"
                                NAME "bound depth input render pixels")

rendercli_run(
  NAME "rendercli binds imported stencil graph inputs"
  COMMAND
    "${RENDERCLI}" --render_graph --render_graph_in "${stencil_input_graph}"
    --render_graph_stencil_in "stencil_mask=${external_color_input}"
    "${static_scene}" "${stencil_input_bound_render}"
)
rendercli_assert_image_dimensions("${stencil_input_bound_render}" 32 16
                                  NAME "bound stencil input render dimensions")
rendercli_assert_image_nonempty("${stencil_input_bound_render}"
                                NAME "bound stencil input render pixels")

rendercli_run(
  NAME "rendercli binds imported object-id graph inputs"
  COMMAND
    "${RENDERCLI}" --render_graph --render_graph_in "${object_id_input_graph}"
    --render_graph_object_id_in "history_object_id=${external_color_input}"
    "${static_scene}" "${object_id_input_bound_render}"
)
rendercli_assert_image_dimensions("${object_id_input_bound_render}" 32 16
                                  NAME "bound object-id input render dimensions")
rendercli_assert_image_nonempty("${object_id_input_bound_render}"
                                NAME "bound object-id input render pixels")

rendercli_run(
  NAME "rendercli binds imported material-id graph inputs"
  COMMAND
    "${RENDERCLI}" --render_graph --render_graph_in "${material_id_input_graph}"
    --render_graph_material_id_in "history_material_id=${external_color_input}"
    "${static_scene}" "${material_id_input_bound_render}"
)
rendercli_assert_image_dimensions("${material_id_input_bound_render}" 32 16
                                  NAME "bound material-id input render dimensions")
rendercli_assert_image_nonempty("${material_id_input_bound_render}"
                                NAME "bound material-id input render pixels")

rendercli_run(
  NAME "rendercli executes depth-aware composite graph"
  COMMAND
    "${RENDERCLI}" --render_graph_in "${depth_composite_graph}"
    "${static_scene}" "${depth_composite_render}"
)
rendercli_assert_image_dimensions("${depth_composite_render}" 32 16
                                  NAME "depth composite graph render dimensions")
rendercli_assert_image_nonempty("${depth_composite_render}"
                                NAME "depth composite graph render pixels")

rendercli_run(
  NAME "rendercli renders through default graph"
  COMMAND
    "${RENDERCLI}" --engine wireframe --width 32 --height 16
    "${static_scene}" "${graph_render}"
)
rendercli_assert_image_dimensions("${graph_render}" 32 16
                                  NAME "rendercli default graph image dimensions")
rendercli_assert_image_nonempty("${graph_render}" NAME "rendercli default graph image pixels")

rendercli_run(
  NAME "rendercli compiles reusable render graph demo scene"
  STDOUT_MATCHES
    "raster_beauty \\[beauty/rasterizer\\] enabled"
    "post_smaa \\[postprocess/postprocess\\] enabled"
    "stencil_aov \\[aov/rasterizer\\] enabled"
    "visualize_stencil_aov \\[aov/postprocess\\] enabled"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format text
    --width 32 --height 18
    "${graph_demo_scene}"
)

rendercli_run(
  NAME "rendercli renders reusable render graph demo scene"
  COMMAND
    "${RENDERCLI}" --width 32 --height 18
    "${graph_demo_scene}" "${graph_demo_render}"
)
rendercli_assert_image_dimensions("${graph_demo_render}" 32 18
                                  NAME "render graph demo scene dimensions")
rendercli_assert_image_nonempty("${graph_demo_render}" NAME "render graph demo scene pixels")

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
if(NOT graph_trace_json MATCHES "\"cacheable\": false")
  message(FATAL_ERROR "graph trace did not contain non-cacheable flag: ${graph_trace_json}")
endif()

rendercli_run(
  NAME "rendercli writes raster shadow artifact trace"
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
if(NOT raster_shadow_trace_json MATCHES "\"cacheable\": true")
  message(FATAL_ERROR "raster shadow trace did not contain cacheable flag: ${raster_shadow_trace_json}")
endif()
if(NOT raster_shadow_trace_json MATCHES "\"storedCachedArtifact\": true")
  message(FATAL_ERROR "raster shadow trace did not contain stored cache flag: ${raster_shadow_trace_json}")
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
rendercli_assert_image_dimensions("${raster_state_render}" 32 16
                                  NAME "graph raster state render dimensions")
rendercli_assert_image_nonempty("${raster_state_render}" NAME "graph raster state render pixels")
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
  NAME "rendercli exports graph-visible raster visibility culling"
  COMMAND
    "${RENDERCLI}" --engine raster --render_graph_only --render_graph_format text
    --raster_culling on --width 32 --height 16
    "${static_scene}" "${raster_culling_plan}"
)
rendercli_assert_nonempty("${raster_culling_plan}" NAME "graph raster culling plan")
file(READ "${raster_culling_plan}" raster_culling_graph)
if(NOT raster_culling_graph MATCHES "raster_visibility")
  message(FATAL_ERROR "raster culling graph did not contain visibility pass: ${raster_culling_graph}")
endif()
if(NOT raster_culling_graph MATCHES "raster_visibility_set")
  message(FATAL_ERROR "raster culling graph did not contain visibility resource: ${raster_culling_graph}")
endif()
if(NOT raster_culling_graph MATCHES "features: visibility culling rasterizer")
  message(FATAL_ERROR "raster culling graph did not describe visibility resource features: ${raster_culling_graph}")
endif()
if(NOT raster_culling_graph MATCHES "visibility_set, unknown, cpu, persistent_cache, 32x16, samples=1")
  message(FATAL_ERROR "raster culling graph did not describe visibility resource shape: ${raster_culling_graph}")
endif()
if(NOT raster_culling_graph MATCHES "raster_beauty")
  message(FATAL_ERROR "raster culling graph did not retain raster beauty pass: ${raster_culling_graph}")
endif()

rendercli_run(
  NAME "rendercli traces raster frustum culling for offscreen geometry"
  COMMAND
    "${RENDERCLI}" --engine raster --raster_culling on --width 32 --height 16
    --render_graph_trace_out "${raster_culling_trace}"
    "${offscreen_culling_scene}" "${raster_culling_trace_render}"
)
rendercli_assert_image_dimensions("${raster_culling_trace_render}" 32 16
                                  NAME "graph raster culling trace image dimensions")
rendercli_assert_image_nonempty("${raster_culling_trace_render}"
                                NAME "graph raster culling trace image pixels")
rendercli_assert_nonempty("${raster_culling_trace}" NAME "graph raster culling trace JSON")
file(READ "${raster_culling_trace}" raster_culling_trace_json)
if(NOT raster_culling_trace_json MATCHES "\"id\": \"raster_visibility\"")
  message(FATAL_ERROR
    "raster culling trace did not contain visibility pass: ${raster_culling_trace_json}")
endif()
if(NOT raster_culling_trace_json MATCHES "frustumRejectedLeaves=1")
  message(FATAL_ERROR
    "raster culling trace did not record one frustum-rejected leaf: ${raster_culling_trace_json}")
endif()
if(NOT raster_culling_trace_json MATCHES "tileGrid=1x1")
  message(FATAL_ERROR
    "raster culling trace did not record the coarse tile grid: ${raster_culling_trace_json}")
endif()
if(NOT raster_culling_trace_json MATCHES "visibleTileReferences=1")
  message(FATAL_ERROR
    "raster culling trace did not record visible tile references: ${raster_culling_trace_json}")
endif()
if(NOT raster_culling_trace_json MATCHES "depthSummarizedTiles=1")
  message(FATAL_ERROR
    "raster culling trace did not record tile depth summaries: ${raster_culling_trace_json}")
endif()
if(NOT raster_culling_trace_json MATCHES "CPU raster passes can skip rejected leaves")
  message(FATAL_ERROR
    "raster culling trace did not record CPU visibility set consumption: ${raster_culling_trace_json}")
endif()

rendercli_run(
  NAME "rendercli traces material-sided raster backface culling"
  COMMAND
    "${RENDERCLI}" --engine raster --raster_culling on --width 32 --height 16
    --render_graph_trace_out "${raster_sidedness_culling_trace}"
    "${material_sidedness_culling_scene}" "${raster_sidedness_culling_trace_render}"
)
rendercli_assert_image_dimensions("${raster_sidedness_culling_trace_render}" 32 16
                                  NAME "graph raster sidedness culling trace image dimensions")
rendercli_assert_image_nonempty("${raster_sidedness_culling_trace_render}"
                                NAME "graph raster sidedness culling trace image pixels")
rendercli_assert_nonempty("${raster_sidedness_culling_trace}"
                          NAME "graph raster sidedness culling trace JSON")
file(READ "${raster_sidedness_culling_trace}" raster_sidedness_culling_trace_json)
if(NOT raster_sidedness_culling_trace_json MATCHES "visibleLeaves=1")
  message(FATAL_ERROR
    "raster sidedness culling trace did not keep one visible leaf: ${raster_sidedness_culling_trace_json}")
endif()
if(NOT raster_sidedness_culling_trace_json MATCHES "backfaceRejectedLeaves=1")
  message(FATAL_ERROR
    "raster sidedness culling trace did not reject one material-sided backface: ${raster_sidedness_culling_trace_json}")
endif()
if(NOT raster_sidedness_culling_trace_json MATCHES "backfaceRejectedTriangles=1")
  message(FATAL_ERROR
    "raster sidedness culling trace did not count the material-sided backface triangle: ${raster_sidedness_culling_trace_json}")
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

set(raytracer_state_plan "${TEST_OUTPUT_DIR}/raytracer_state_plan.json")
rendercli_run(
  NAME "rendercli writes raytracer pass state through graph intent"
  COMMAND
    "${RENDERCLI}" --engine raytracer --render_graph_only --render_graph_format json
    --width 32 --height 16 --sampler Jittered --samples_per_pixel 9 --depth 6
    "${static_scene}" "${raytracer_state_plan}"
)
rendercli_assert_nonempty("${raytracer_state_plan}" NAME "graph raytracer state plan")
file(READ "${raytracer_state_plan}" raytracer_state_graph)
if(NOT raytracer_state_graph MATCHES "raytrace_beauty")
  message(FATAL_ERROR "raytracer state graph did not contain raytrace_beauty: ${raytracer_state_graph}")
endif()
if(NOT raytracer_state_graph MATCHES "samplesPerPixel")
  message(FATAL_ERROR "raytracer state graph did not contain samplesPerPixel: ${raytracer_state_graph}")
endif()
if(NOT raytracer_state_graph MATCHES "Jittered")
  message(FATAL_ERROR "raytracer state graph did not contain sampler: ${raytracer_state_graph}")
endif()
if(NOT raytracer_state_graph MATCHES "maxRecursionDepth")
  message(FATAL_ERROR "raytracer state graph did not contain recursion depth: ${raytracer_state_graph}")
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
  NAME "rendercli post AA none overrides scene graph intent"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format text
    --width 32 --height 16 --post_aa none
    "${scene_intent_scene}" "${scene_post_aa_none_plan}"
)
file(READ "${scene_post_aa_none_plan}" scene_post_aa_none_graph)
if(scene_post_aa_none_graph MATCHES "post_smaa|post_fxaa|post_aa_color")
  message(FATAL_ERROR
          "scene post-AA none graph still contained image post-AA pass: ${scene_post_aa_none_graph}")
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
rendercli_assert_image_dimensions("${default_graph_render}" 48 32
                                  NAME "rendercli default graph intent dimensions")
rendercli_assert_image_nonempty("${default_graph_render}"
                                NAME "rendercli default graph intent pixels")

rendercli_run(
  NAME "rendercli direct engine bypasses scene render intent"
  COMMAND
    "${RENDERCLI}" --direct_engine --engine raytracer --width 48 --height 32
    "${default_graph_scene}" "${direct_engine_render}"
)
rendercli_assert_image_dimensions("${direct_engine_render}" 48 32
                                  NAME "rendercli direct engine dimensions")
rendercli_assert_image_nonempty("${direct_engine_render}" NAME "rendercli direct engine pixels")
rendercli_assert_image_hash_differs("${default_graph_render}" "${direct_engine_render}"
                                    NAME "default graph output differs from direct engine output")

rendercli_run(
  NAME "rendercli renders through replayed JSON graph"
  COMMAND
    "${RENDERCLI}" --render_graph --render_graph_in "${json_plan}"
    "${static_scene}" "${replayed_render}"
)
rendercli_assert_image_dimensions("${replayed_render}" 32 16
                                  NAME "rendercli --render_graph_in dimensions")
rendercli_assert_image_nonempty("${replayed_render}" NAME "rendercli --render_graph_in pixels")

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
       "Execution order:\n- raytrace_beauty\n- tonemap\nExecution stages:\n- 1: raytrace_beauty\n- 2: tonemap\nDependencies:\n- raytrace_beauty -> tonemap via beauty_color\nPasses:\n- tonemap"
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
rendercli_assert_image_dimensions("${out_of_order_render}" 32 16
                                  NAME "out-of-order graph replay dimensions")
rendercli_assert_image_nonempty("${out_of_order_render}" NAME "out-of-order graph replay pixels")

rendercli_run(
  NAME "rendercli renders through replayed JSON graph with matching explicit size"
  COMMAND
    "${RENDERCLI}" --render_graph --render_graph_in "${json_plan}" --width 32 --height 16
    "${static_scene}" "${replayed_matching_render}"
)
rendercli_assert_image_dimensions("${replayed_matching_render}" 32 16
                                  NAME "matching explicit graph replay dimensions")
rendercli_assert_image_nonempty("${replayed_matching_render}"
                                NAME "matching explicit graph replay pixels")
rendercli_assert_image_hash_equals("${replayed_render}" "${replayed_matching_render}"
                                   NAME "implicit and explicit graph replay output match")

rendercli_run(
  NAME "rendercli disables optional tonemap pass"
  STDOUT_MATCHES "tonemap \\[tonemap/postprocess\\] disabled"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format text --disable_pass tonemap
    --width 32 --height 16
    "${static_scene}"
)

rendercli_run(
  NAME "rendercli disables graph passes by kind"
  STDOUT_MATCHES "tonemap \\[tonemap/postprocess\\] disabled"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format text --disable_pass_kind tonemap
    --width 32 --height 16
    "${static_scene}"
)

rendercli_run(
  NAME "rendercli disables graph passes by executor"
  STDOUT_MATCHES "tonemap \\[tonemap/postprocess\\] disabled"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format text --disable_executor postprocess
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
