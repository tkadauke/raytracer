if (NOT DEFINED RENDERCLI)
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
set(render_texture_screen_demo_scene
    "${PROJECT_SOURCE_DIR}/scenes/render_texture_screen_demo.json")
set(portal_graph_demo_scene "${PROJECT_SOURCE_DIR}/scenes/render_graph_portal_demo.json")
set(planar_mirror_graph_demo_scene
    "${PROJECT_SOURCE_DIR}/scenes/render_graph_planar_mirror_demo.json")
set(wavefront_indirect_scene
    "${PROJECT_SOURCE_DIR}/scenes/wavefront_indirect_environment_demo.json")
set(wavefront_indirect_bounce_scene
    "${PROJECT_SOURCE_DIR}/scenes/wavefront_indirect_bounce_demo.json")
set(wavefront_denoise_scene "${PROJECT_SOURCE_DIR}/scenes/wavefront_denoise_demo.json")
set(pathtracer_area_light_scene "${PROJECT_SOURCE_DIR}/scenes/pathtracer_area_light_demo.json")
set(scene_intent_scene "${TEST_OUTPUT_DIR}/scene-intent.json")
set(scene_viewplane_intent_scene "${TEST_OUTPUT_DIR}/scene-viewplane-intent.json")
set(scene_queue_intent_scene "${TEST_OUTPUT_DIR}/scene-queue-intent.json")
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
set(cyclic_graph "${TEST_OUTPUT_DIR}/cyclic-graph.json")
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
set(render_texture_screen_plan "${TEST_OUTPUT_DIR}/graph-render-texture-screen.json")
set(render_texture_screen_render "${TEST_OUTPUT_DIR}/graph-render-texture-screen-render.png")
set(render_texture_screen_trace "${TEST_OUTPUT_DIR}/graph-render-texture-screen-trace.json")
set(render_texture_screen_trace_render
    "${TEST_OUTPUT_DIR}/graph-render-texture-screen-trace-render.png")
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
set(selector_routed_text_plan "${TEST_OUTPUT_DIR}/graph-selector-routed.txt")
set(selector_routed_dot_plan "${TEST_OUTPUT_DIR}/graph-selector-routed.dot")
set(selector_routed_json_plan "${TEST_OUTPUT_DIR}/graph-selector-routed.json")
set(selector_routed_replayed_dot_plan "${TEST_OUTPUT_DIR}/graph-selector-routed-replayed.dot")
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
set(wavefront_plan "${TEST_OUTPUT_DIR}/wavefront-graph.json")
set(wavefront_gpu_backend_plan "${TEST_OUTPUT_DIR}/wavefront-gpu-backend-graph.json")
set(wavefront_auto_backend_plan "${TEST_OUTPUT_DIR}/wavefront-auto-backend-graph.json")
set(wavefront_render "${TEST_OUTPUT_DIR}/wavefront-render.png")
set(wavefront_supported_backend_scene
    "${TEST_OUTPUT_DIR}/wavefront-supported-backend-scene.json")
set(wavefront_supported_backend_render
    "${TEST_OUTPUT_DIR}/wavefront-supported-backend-render.png")
set(wavefront_supported_backend_cpu_render
    "${TEST_OUTPUT_DIR}/wavefront-supported-backend-cpu-render.png")
set(wavefront_supported_backend_gpu_render
    "${TEST_OUTPUT_DIR}/wavefront-supported-backend-gpu-render.png")
set(wavefront_supported_backend_auto_render
    "${TEST_OUTPUT_DIR}/wavefront-supported-backend-auto-render.png")
set(wavefront_supported_backend_auto_report
    "${TEST_OUTPUT_DIR}/wavefront-supported-backend-auto-metrics.json")
set(wavefront_supported_backend_report
    "${TEST_OUTPUT_DIR}/wavefront-supported-backend-metrics.json")
set(wavefront_supported_backend_trace
    "${TEST_OUTPUT_DIR}/wavefront-supported-backend-trace.json")
set(wavefront_supported_backend_trace_render
    "${TEST_OUTPUT_DIR}/wavefront-supported-backend-trace-render.png")
set(wavefront_metrics_render "${TEST_OUTPUT_DIR}/wavefront-metrics-render.png")
set(wavefront_metrics_report "${TEST_OUTPUT_DIR}/wavefront-metrics.json")
set(wavefront_unsupported_backend_trace
    "${TEST_OUTPUT_DIR}/wavefront-unsupported-backend-trace.json")
set(wavefront_unsupported_backend_trace_render
    "${TEST_OUTPUT_DIR}/wavefront-unsupported-backend-trace-render.png")
set(wavefront_converged_metrics_render
    "${TEST_OUTPUT_DIR}/wavefront-converged-metrics-render.png")
set(wavefront_converged_metrics_report "${TEST_OUTPUT_DIR}/wavefront-converged-metrics.json")
set(wavefront_feedback_metrics_render
    "${TEST_OUTPUT_DIR}/wavefront-feedback-metrics-render.png")
set(wavefront_feedback_metrics_report "${TEST_OUTPUT_DIR}/wavefront-feedback-metrics.json")
set(wavefront_direct_metrics_render
    "${TEST_OUTPUT_DIR}/wavefront-direct-metrics-render.png")
set(wavefront_direct_metrics_report "${TEST_OUTPUT_DIR}/wavefront-direct-metrics.json")
set(wavefront_batched_visibility_render
    "${TEST_OUTPUT_DIR}/wavefront-batched-visibility-render.png")
set(wavefront_parity_raytracer_render
    "${TEST_OUTPUT_DIR}/wavefront-parity-raytracer-render.png")
set(wavefront_parity_render "${TEST_OUTPUT_DIR}/wavefront-parity-render.png")
set(wavefront_glass_parity_raytracer_render
    "${TEST_OUTPUT_DIR}/wavefront-glass-parity-raytracer-render.png")
set(wavefront_glass_parity_render "${TEST_OUTPUT_DIR}/wavefront-glass-parity-render.png")
set(wavefront_reflection_parity_raytracer_render
    "${TEST_OUTPUT_DIR}/wavefront-reflection-parity-raytracer-render.png")
set(wavefront_reflection_parity_render
    "${TEST_OUTPUT_DIR}/wavefront-reflection-parity-render.png")
set(wavefront_bvh_macro_scene "${TEST_OUTPUT_DIR}/wavefront-bvh-macro-scene.json")
set(wavefront_bvh_macro_raytracer_render
    "${TEST_OUTPUT_DIR}/wavefront-bvh-macro-raytracer-render.png")
set(wavefront_bvh_macro_render "${TEST_OUTPUT_DIR}/wavefront-bvh-macro-render.png")
set(wavefront_indirect_render "${TEST_OUTPUT_DIR}/wavefront-indirect-render.png")
set(wavefront_indirect_whitted_render
    "${TEST_OUTPUT_DIR}/wavefront-indirect-whitted-render.png")
set(wavefront_indirect_bounce_render
    "${TEST_OUTPUT_DIR}/wavefront-indirect-bounce-render.png")
set(wavefront_indirect_bounce_whitted_render
    "${TEST_OUTPUT_DIR}/wavefront-indirect-bounce-whitted-render.png")
set(wavefront_pathtracer_plan "${TEST_OUTPUT_DIR}/wavefront-pathtracer-graph.json")
set(scalar_pathtracer_plan "${TEST_OUTPUT_DIR}/scalar-pathtracer-graph.json")
set(wavefront_convergence_plan "${TEST_OUTPUT_DIR}/wavefront-convergence-graph.json")
set(wavefront_default_convergence_plan
    "${TEST_OUTPUT_DIR}/wavefront-default-convergence-graph.json")
set(wavefront_adaptive_plan "${TEST_OUTPUT_DIR}/wavefront-adaptive-graph.json")
set(wavefront_scene_viewplane_plan
    "${TEST_OUTPUT_DIR}/wavefront-scene-viewplane-graph.json")
set(wavefront_scene_queue_plan "${TEST_OUTPUT_DIR}/wavefront-scene-queue-graph.json")
set(wavefront_cli_queue_plan "${TEST_OUTPUT_DIR}/wavefront-cli-queue-graph.json")
set(wavefront_denoise_plan "${TEST_OUTPUT_DIR}/wavefront-denoise-graph.json")
set(wavefront_scene_denoise_plan "${TEST_OUTPUT_DIR}/wavefront-scene-denoise-graph.json")
set(wavefront_scene_denoise_render "${TEST_OUTPUT_DIR}/wavefront-scene-denoise-render.png")
set(wavefront_scene_denoise_trace "${TEST_OUTPUT_DIR}/wavefront-scene-denoise-trace.json")
set(wavefront_denoise_quality_reference
    "${TEST_OUTPUT_DIR}/wavefront-denoise-quality-reference.png")
set(wavefront_denoise_quality_raw "${TEST_OUTPUT_DIR}/wavefront-denoise-quality-raw.png")
set(wavefront_denoise_quality_filtered
    "${TEST_OUTPUT_DIR}/wavefront-denoise-quality-filtered.png")
set(wavefront_pathtracer_render "${TEST_OUTPUT_DIR}/wavefront-pathtracer-render.png")
set(wavefront_compatibility_trace "${TEST_OUTPUT_DIR}/wavefront-compatibility-trace.json")
set(wavefront_compatibility_trace_render
    "${TEST_OUTPUT_DIR}/wavefront-compatibility-trace-render.png")
set(wavefront_glass_trace "${TEST_OUTPUT_DIR}/wavefront-glass-trace.json")
set(wavefront_glass_trace_render "${TEST_OUTPUT_DIR}/wavefront-glass-trace-render.png")
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
set(portal_graph_demo_plan "${TEST_OUTPUT_DIR}/graph-portal-demo.json")
set(portal_graph_demo_render "${TEST_OUTPUT_DIR}/graph-portal-demo-render.png")
set(planar_mirror_graph_demo_plan "${TEST_OUTPUT_DIR}/graph-planar-mirror-demo.json")
set(planar_mirror_graph_demo_render
    "${TEST_OUTPUT_DIR}/graph-planar-mirror-demo-render.png")

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

file(WRITE "${scene_viewplane_intent_scene}" [=[
{
  "id": "{90100000-0000-0000-0000-000000000000}",
  "name": "Scene View Plane Intent Fixture",
  "ambient": [0.4, 0.4, 0.4],
  "background": [0.4, 0.8, 1.0],
  "type": "Scene",
  "renderIntent": {
    "engineOptions": {
      "raytracer": {
        "viewPlane": {"type": "ViewPlane"}
      }
    }
  },
  "children": []
}
]=])

file(WRITE "${scene_queue_intent_scene}" [=[
{
  "id": "{90200000-0000-0000-0000-000000000000}",
  "name": "Scene Queue Intent Fixture",
  "ambient": [0.4, 0.4, 0.4],
  "background": [0.4, 0.8, 1.0],
  "type": "Scene",
  "renderIntent": {
    "engineOptions": {
      "raytracer": {
        "execution": {"queueSize": 7}
      }
    }
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

file(WRITE "${wavefront_supported_backend_scene}" [=[
{
  "id": "{94100000-0000-0000-0000-000000000000}",
  "name": "Wavefront Supported Backend Fixture",
  "ambient": [1.0, 1.0, 1.0],
  "background": [0.4, 0.8, 1.0],
  "type": "Scene",
  "children": [
    {
      "id": "camera",
      "name": "Camera",
      "position": [0.0, 0.0, -4.0],
      "target": [0.0, 0.0, 0.0],
      "distance": 5.0,
      "zoom": 1.4,
      "type": "PinholeCamera",
      "children": []
    },
    {
      "id": "blue-texture",
      "name": "Blue Texture",
      "color": [0.1, 0.4, 1.0],
      "type": "ConstantColorTexture",
      "children": []
    },
    {
      "id": "blue-material",
      "name": "Blue Material",
      "diffuseTexture": "blue-texture",
      "ambientCoefficient": 1.0,
      "diffuseCoefficient": 0.0,
      "type": "MatteMaterial",
      "children": []
    },
    {
      "id": "sphere",
      "name": "Supported Sphere",
      "position": [-0.55, 0.0, 0.0],
      "rotation": [0.0, 0.0, 0.0],
      "scale": [1.0, 1.0, 1.0],
      "visible": true,
      "material": "blue-material",
      "radius": 1.0,
      "type": "Sphere",
      "children": []
    },
    {
      "id": "box",
      "name": "Supported Box",
      "position": [1.0, 0.0, 0.0],
      "rotation": [0.0, 0.0, 0.0],
      "scale": [1.0, 1.0, 1.0],
      "visible": true,
      "material": "blue-material",
      "size": [0.8, 0.8, 0.8],
      "bevelRadius": 0.0,
      "type": "Box",
      "children": []
    },
    {
      "id": "cylinder",
      "name": "Supported Cylinder",
      "position": [0.2, -1.2, 0.0],
      "rotation": [0.0, 0.0, 0.0],
      "scale": [1.0, 1.0, 1.0],
      "visible": true,
      "material": "blue-material",
      "radius": 0.35,
      "height": 0.8,
      "bevelRadius": 0.0,
      "type": "Cylinder",
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

file(WRITE "${wavefront_bvh_macro_scene}" [=[
{
  "id": "{9a000000-0000-0000-0000-000000000000}",
  "name": "Wavefront BVH Macro Fixture",
  "ambient": [0.45, 0.45, 0.45],
  "background": [0.02, 0.02, 0.03],
  "accelerationMode": 3,
  "type": "Scene",
  "children": [
    {
      "id": "camera",
      "name": "Camera",
      "position": [0.0, 1.0, -5.0],
      "target": [0.0, 0.0, 0.0],
      "distance": 5.0,
      "zoom": 1.3,
      "type": "PinholeCamera",
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
      "direction": [-0.35, -1.0, -0.45],
      "type": "DirectionalLight",
      "children": []
    },
    {
      "id": "warm",
      "name": "Warm",
      "color": [0.9, 0.45, 0.18],
      "type": "ConstantColorTexture",
      "children": []
    },
    {
      "id": "matte",
      "name": "Matte",
      "diffuseTexture": "warm",
      "ambientCoefficient": 1.0,
      "diffuseCoefficient": 1.0,
      "type": "MatteMaterial",
      "children": []
    },
]=])
set(wavefront_bvh_positions -1.4 -0.7 0.0 0.7 1.4)
set(wavefront_bvh_sphere_index 0)
foreach(z IN LISTS wavefront_bvh_positions)
  foreach(x IN LISTS wavefront_bvh_positions)
    if(NOT wavefront_bvh_sphere_index EQUAL 0)
      file(APPEND "${wavefront_bvh_macro_scene}" ",\n")
    endif()
    file(APPEND "${wavefront_bvh_macro_scene}"
"    {
      \"id\": \"bvh-sphere-${wavefront_bvh_sphere_index}\",
      \"name\": \"BVH Sphere ${wavefront_bvh_sphere_index}\",
      \"position\": [${x}, 0.0, ${z}],
      \"rotation\": [0.0, 0.0, 0.0],
      \"scale\": [1.0, 1.0, 1.0],
      \"visible\": true,
      \"material\": \"matte\",
      \"radius\": 0.28,
      \"type\": \"Sphere\",
      \"children\": []
    }")
    math(EXPR wavefront_bvh_sphere_index "${wavefront_bvh_sphere_index} + 1")
  endforeach()
endforeach()
file(APPEND "${wavefront_bvh_macro_scene}" [=[

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
  "children": [
    {
      "id": "{94000000-0000-0000-0000-000000000010}",
      "name": "Subview Receiver Material",
      "type": "MatteMaterial",
      "renderTextureSubview": "mirror_probe",
      "children": []
    }
  ]
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

file(WRITE "${cyclic_graph}" [=[
{
  "resources": [
    {
      "id": "screen_color",
      "name": "Screen color",
      "features": ["render_to_texture", "subview_output", "subview_color_output"],
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
      "id": "screen_receiver",
      "name": "Screen receiver",
      "kind": "beauty",
      "executor": "raytracer",
      "features": ["main", "render_to_texture"],
      "reads": ["screen_color"],
      "writes": ["main_color"],
      "disabledBehavior": "error",
      "enabled": true,
      "hasExternalSideEffects": false,
      "canRunConcurrently": false
    },
    {
      "id": "screen_feed",
      "name": "Screen feed",
      "kind": "beauty",
      "executor": "rasterizer",
      "features": ["subview", "render_to_texture"],
      "reads": ["main_color"],
      "writes": ["screen_color"],
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
  NAME "rendercli graph view override routes selector branch in text export"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format text
    --render_graph_view_override "object_id:12,executor=wireframe,view=wireframe"
    --width 32 --height 16
    "${static_scene}" "${selector_routed_text_plan}"
)
rendercli_assert_nonempty("${selector_routed_text_plan}"
                          NAME "selector-routed text graph output")
file(READ "${selector_routed_text_plan}" selector_routed_text_graph)
if(NOT selector_routed_text_graph MATCHES "selector_1_wireframe_beauty")
  message(FATAL_ERROR
    "selector-routed text graph did not contain routed wireframe pass: ${selector_routed_text_graph}")
endif()
if(NOT selector_routed_text_graph MATCHES "selector_1_composite")
  message(FATAL_ERROR
    "selector-routed text graph did not contain routed composite pass: ${selector_routed_text_graph}")
endif()
if(NOT selector_routed_text_graph MATCHES "selector_1_composited_color")
  message(FATAL_ERROR
    "selector-routed text graph did not route tonemap through composited color: ${selector_routed_text_graph}")
endif()
if(NOT selector_routed_text_graph MATCHES "scene: selector=object_id: 12")
  message(FATAL_ERROR
    "selector-routed text graph did not show selector metadata: ${selector_routed_text_graph}")
endif()
if(NOT selector_routed_text_graph MATCHES "features: .*selector_override")
  message(FATAL_ERROR
    "selector-routed text graph did not mark routed resources or passes: ${selector_routed_text_graph}")
endif()

rendercli_run(
  NAME "rendercli graph view override routes selector branch in DOT export"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format dot
    --render_graph_view_override "object_id:12,view=normal"
    --width 32 --height 16
    "${static_scene}" "${selector_routed_dot_plan}"
)
rendercli_assert_nonempty("${selector_routed_dot_plan}"
                          NAME "selector-routed DOT graph output")
file(READ "${selector_routed_dot_plan}" selector_routed_dot_graph)
if(NOT selector_routed_dot_graph MATCHES "selector_1_normal_aov")
  message(FATAL_ERROR
    "selector-routed DOT graph did not contain routed normal AOV: ${selector_routed_dot_graph}")
endif()
if(NOT selector_routed_dot_graph MATCHES "selector object_id: 12")
  message(FATAL_ERROR
    "selector-routed DOT graph did not show selector metadata: ${selector_routed_dot_graph}")
endif()
if(NOT selector_routed_dot_graph MATCHES "selector_1_composite")
  message(FATAL_ERROR
    "selector-routed DOT graph did not contain routed composite pass: ${selector_routed_dot_graph}")
endif()

rendercli_run(
  NAME "rendercli graph view override routes selector branch in JSON export"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format json
    --render_graph_view_override "object_id:12,executor=wavefront"
    --width 32 --height 16
    "${static_scene}" "${selector_routed_json_plan}"
)
rendercli_assert_nonempty("${selector_routed_json_plan}"
                          NAME "selector-routed JSON graph output")
file(READ "${selector_routed_json_plan}" selector_routed_json_graph)
if(NOT selector_routed_json_graph MATCHES "\"id\": \"selector_1_wavefront_beauty\"")
  message(FATAL_ERROR
    "selector-routed JSON graph did not contain routed wavefront pass: ${selector_routed_json_graph}")
endif()
if(NOT selector_routed_json_graph MATCHES "\"id\": \"selector_1_composite\"")
  message(FATAL_ERROR
    "selector-routed JSON graph did not contain routed composite pass: ${selector_routed_json_graph}")
endif()
if(NOT selector_routed_json_graph MATCHES "\"sceneSelector\"")
  message(FATAL_ERROR
    "selector-routed JSON graph did not carry sceneSelector metadata: ${selector_routed_json_graph}")
endif()
if(NOT selector_routed_json_graph MATCHES "\"kind\": \"object_id\"")
  message(FATAL_ERROR
    "selector-routed JSON graph did not carry selector kind: ${selector_routed_json_graph}")
endif()
if(NOT selector_routed_json_graph MATCHES "\"value\": \"12\"")
  message(FATAL_ERROR
    "selector-routed JSON graph did not carry selector value: ${selector_routed_json_graph}")
endif()
if(NOT selector_routed_json_graph MATCHES "\"selector_override\"")
  message(FATAL_ERROR
    "selector-routed JSON graph did not mark selector_override metadata: ${selector_routed_json_graph}")
endif()

rendercli_run(
  NAME "rendercli replays selector-routed graph JSON"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format dot
    --render_graph_in "${selector_routed_json_plan}"
    "${static_scene}" "${selector_routed_replayed_dot_plan}"
)
rendercli_assert_nonempty("${selector_routed_replayed_dot_plan}"
                          NAME "selector-routed replay DOT graph output")
file(READ "${selector_routed_replayed_dot_plan}" selector_routed_replayed_dot_graph)
if(NOT selector_routed_replayed_dot_graph MATCHES "selector_1_wavefront_beauty")
  message(FATAL_ERROR
    "replayed selector-routed DOT graph did not preserve routed wavefront pass: ${selector_routed_replayed_dot_graph}")
endif()
if(NOT selector_routed_replayed_dot_graph MATCHES "selector object_id: 12")
  message(FATAL_ERROR
    "replayed selector-routed DOT graph did not preserve selector metadata: ${selector_routed_replayed_dot_graph}")
endif()
if(NOT selector_routed_replayed_dot_graph MATCHES "selector_1_composite")
  message(FATAL_ERROR
    "replayed selector-routed DOT graph did not preserve routed composite pass: ${selector_routed_replayed_dot_graph}")
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

rendercli_run(
  NAME "rendercli exports wavefront executor in render graph"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format json
    --engine wavefront --wavefront_intersection_backend cpu --width 32 --height 16
    "${static_scene}" "${wavefront_plan}"
)
rendercli_assert_nonempty("${wavefront_plan}" NAME "wavefront graph output")
file(READ "${wavefront_plan}" wavefront_graph)
if(NOT wavefront_graph MATCHES "\"executor\": \"wavefront\"")
  message(FATAL_ERROR "wavefront graph did not contain wavefront executor: ${wavefront_graph}")
endif()
if(NOT wavefront_graph MATCHES "\"id\": \"wavefront_beauty\"")
  message(FATAL_ERROR "wavefront graph did not contain wavefront beauty pass: ${wavefront_graph}")
endif()
if(NOT wavefront_graph MATCHES "\"queueSize\"")
  message(FATAL_ERROR
          "wavefront graph did not contain rendercli ray-family queue state: ${wavefront_graph}")
endif()
if(NOT wavefront_graph MATCHES "\"viewPlane\"")
  message(FATAL_ERROR
          "wavefront graph did not contain rendercli ray-family view-plane state: ${wavefront_graph}")
endif()
if(NOT wavefront_graph MATCHES "\"type\"[ \r\n]*:[ \r\n]*\"TiledViewPlane\"")
  message(FATAL_ERROR
          "wavefront graph did not default rendercli ray-family view plane to TiledViewPlane: ${wavefront_graph}")
endif()
if(NOT wavefront_graph MATCHES "\"intersectionBackend\"[ \r\n]*:[ \r\n]*\"cpu\"")
  message(
    FATAL_ERROR
      "wavefront graph did not contain requested intersection backend state: ${wavefront_graph}"
  )
endif()

rendercli_run(
  NAME "rendercli exports requested GPU wavefront backend in render graph"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format json --engine wavefront
    --wavefront_intersection_backend gpu --width 32 --height 16 "${static_scene}"
    "${wavefront_gpu_backend_plan}"
)
rendercli_assert_nonempty("${wavefront_gpu_backend_plan}"
                          NAME "GPU wavefront backend graph output")
file(READ "${wavefront_gpu_backend_plan}" wavefront_gpu_backend_graph)
if(NOT wavefront_gpu_backend_graph MATCHES "\"intersectionBackend\"[ \r\n]*:[ \r\n]*\"gpu\"")
  message(
    FATAL_ERROR
      "wavefront graph did not contain requested GPU intersection backend state: ${wavefront_gpu_backend_graph}"
  )
endif()

rendercli_run(
  NAME "rendercli exports requested auto wavefront backend in render graph"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format json --engine wavefront
    --wavefront_intersection_backend auto --width 32 --height 16 "${static_scene}"
    "${wavefront_auto_backend_plan}"
)
rendercli_assert_nonempty("${wavefront_auto_backend_plan}"
                          NAME "auto wavefront backend graph output")
file(READ "${wavefront_auto_backend_plan}" wavefront_auto_backend_graph)
if(NOT wavefront_auto_backend_graph MATCHES "\"intersectionBackend\"[ \r\n]*:[ \r\n]*\"auto\"")
  message(
    FATAL_ERROR
      "wavefront graph did not contain requested auto intersection backend state: ${wavefront_auto_backend_graph}"
  )
endif()

rendercli_run(
  NAME "rendercli preserves scene-authored wavefront view plane"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format json
    --engine wavefront --width 32 --height 16
    "${scene_viewplane_intent_scene}" "${wavefront_scene_viewplane_plan}"
)
rendercli_assert_nonempty("${wavefront_scene_viewplane_plan}"
                          NAME "scene-authored wavefront view plane graph output")
file(READ "${wavefront_scene_viewplane_plan}" wavefront_scene_viewplane_graph)
if(NOT wavefront_scene_viewplane_graph MATCHES "\"viewPlane\"")
  message(
    FATAL_ERROR
      "scene-authored wavefront graph did not contain ray-family view-plane state: ${wavefront_scene_viewplane_graph}"
  )
endif()
if(NOT wavefront_scene_viewplane_graph MATCHES "\"type\"[ \r\n]*:[ \r\n]*\"ViewPlane\"")
  message(
    FATAL_ERROR
      "scene-authored wavefront graph did not preserve ViewPlane state: ${wavefront_scene_viewplane_graph}"
  )
endif()
if(wavefront_scene_viewplane_graph MATCHES "\"type\"[ \r\n]*:[ \r\n]*\"TiledViewPlane\"")
  message(
    FATAL_ERROR
      "rendercli TiledViewPlane default overrode scene-authored ViewPlane state: ${wavefront_scene_viewplane_graph}"
  )
endif()

rendercli_run(
  NAME "rendercli preserves scene-authored wavefront queue size"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format json
    --engine wavefront --width 32 --height 16 "${scene_queue_intent_scene}"
    "${wavefront_scene_queue_plan}"
)
rendercli_assert_nonempty("${wavefront_scene_queue_plan}"
                          NAME "scene-authored wavefront queue graph output")
file(READ "${wavefront_scene_queue_plan}" wavefront_scene_queue_graph)
if(NOT wavefront_scene_queue_graph MATCHES "\"queueSize\"[ \r\n]*:[ \r\n]*7")
  message(
    FATAL_ERROR
      "scene-authored wavefront graph did not preserve queueSize 7: ${wavefront_scene_queue_graph}"
  )
endif()

rendercli_run(
  NAME "rendercli explicit queue size overrides scene-authored wavefront queue size"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format json
    --engine wavefront --queue_size 11 --width 32 --height 16 "${scene_queue_intent_scene}"
    "${wavefront_cli_queue_plan}"
)
rendercli_assert_nonempty("${wavefront_cli_queue_plan}" NAME "CLI wavefront queue graph output")
file(READ "${wavefront_cli_queue_plan}" wavefront_cli_queue_graph)
if(NOT wavefront_cli_queue_graph MATCHES "\"queueSize\"[ \r\n]*:[ \r\n]*11")
  message(FATAL_ERROR
          "explicit rendercli queueSize 11 did not override scene intent: ${wavefront_cli_queue_graph}")
endif()

rendercli_run(
  NAME "rendercli renders graph wavefront executor"
  COMMAND
    "${RENDERCLI}" --engine wavefront --width 16 --height 16
    "${static_scene}" "${wavefront_render}"
)
rendercli_assert_image_nonempty("${wavefront_render}" NAME "wavefront graph render pixels")

rendercli_run(
  NAME "rendercli writes graph wavefront metrics JSON and summary"
  OUTPUT_VARIABLE wavefront_metrics_stdout
  STDOUT_MATCHES
    "wavefront_metrics.*pass=wavefront_beauty.*integrator=whitted.*execution=depth_major_whitted.*intersection_backend_request=gpu.*intersection_backend=cpu.*intersection_backend_availability=fallback.*intersection_backend_execution=packed_cpu.*intersection_expected_rays=[1-9][0-9]*.*intersection_scene_compiled=true.*intersection_scene_primitives=[1-9][0-9]*.*intersection_scene_unsupported=0.*intersection_scene_upload_bytes=[1-9][0-9]*.*intersection_scene_triangle_kernel_eligible=false.*intersection_estimated_ray_upload_bytes=[1-9][0-9]*.*intersection_estimated_query_transfer_bytes=[1-9][0-9]*.*intersection_backend_upload_worker_ms=0.*intersection_backend_kernel_worker_ms=0.*intersection_backend_readback_worker_ms=0.*intersection_rays=[0-9][0-9]*.*closest_hit_rays=[0-9][0-9]*.*any_hit_rays=[0-9][0-9]*.*closest_hit_queries=[0-9][0-9]*.*any_hit_queries=[0-9][0-9]*.*samples=.*active_sample_depths=.*compatibility_shade_samples=.*convergence=disabled.*denoiser=box.*denoise_radius=2"
  COMMAND
    "${RENDERCLI}" --engine wavefront --width 16 --height 16
    --wavefront_intersection_backend gpu
    --wavefront_denoiser box --wavefront_denoise_radius 2
    --wavefront_metrics_out "${wavefront_metrics_report}" --wavefront_metrics_summary
    "${static_scene}" "${wavefront_metrics_render}"
)
rendercli_assert_image_nonempty("${wavefront_metrics_render}"
                                NAME "wavefront metrics graph render pixels")
foreach(feature_name albedo normal depth)
  if(NOT wavefront_metrics_stdout MATCHES "denoise_feature_${feature_name}=0")
    _rendercli_fail("rendercli wavefront metrics denoiser feature summary"
                    "wavefront metrics summary did not report skipped ${feature_name} feature metadata"
                    "${wavefront_metrics_stdout}" "" "" "")
  endif()
endforeach()
foreach(tiling_name
        tiles
        tile_grid
        max_tile_width
        max_tile_height
        max_tile_pixels
        avg_tile_pixels
        nonempty_tiles
        min_tile_samples
        avg_tile_samples
        max_tile_samples)
  if(NOT wavefront_metrics_stdout MATCHES "${tiling_name}=")
    _rendercli_fail("rendercli wavefront metrics ${tiling_name} summary"
                    "wavefront metrics summary did not contain ${tiling_name}"
                    "${wavefront_metrics_stdout}" "" "" "")
  endif()
endforeach()
if(NOT wavefront_metrics_stdout MATCHES "denoise_feature_prepass_ms=")
  _rendercli_fail("rendercli wavefront metrics denoiser feature timing summary"
                  "wavefront metrics summary did not contain denoiser feature timing"
                  "${wavefront_metrics_stdout}" "" "" "")
endif()
if(NOT wavefront_metrics_stdout MATCHES "sample_gen_worker_ms=")
  _rendercli_fail("rendercli wavefront metrics sample generation worker timing summary"
                  "wavefront metrics summary did not contain sample generation worker timing"
                  "${wavefront_metrics_stdout}" "" "" "")
endif()
foreach(timing_name
        stream
        primary_ray
        enqueue
        gen_overhead)
  if(NOT wavefront_metrics_stdout MATCHES "sample_${timing_name}_worker_ms=")
    _rendercli_fail("rendercli wavefront metrics sample ${timing_name} timing summary"
                    "wavefront metrics summary did not contain sample ${timing_name} worker timing"
                    "${wavefront_metrics_stdout}" "" "" "")
  endif()
endforeach()
if(NOT wavefront_metrics_stdout MATCHES "integrator_worker_ms=")
  _rendercli_fail("rendercli wavefront metrics integrator worker timing summary"
                  "wavefront metrics summary did not contain integrator worker timing"
                  "${wavefront_metrics_stdout}" "" "" "")
endif()
if(NOT wavefront_metrics_stdout MATCHES "integrator_intersection_worker_ms=")
  _rendercli_fail("rendercli wavefront metrics integrator intersection timing summary"
                  "wavefront metrics summary did not contain integrator intersection worker timing"
                  "${wavefront_metrics_stdout}" "" "" "")
endif()
if(NOT wavefront_metrics_stdout MATCHES "integrator_shading_worker_ms=")
  _rendercli_fail("rendercli wavefront metrics integrator shading timing summary"
                  "wavefront metrics summary did not contain integrator shading worker timing"
                  "${wavefront_metrics_stdout}" "" "" "")
endif()
if(NOT wavefront_metrics_stdout MATCHES "integrator_overhead_worker_ms=")
  _rendercli_fail("rendercli wavefront metrics integrator overhead timing summary"
                  "wavefront metrics summary did not contain integrator overhead worker timing"
                  "${wavefront_metrics_stdout}" "" "" "")
endif()
foreach(timing_name
        path_setup
        frontier_bookkeeping
        progress_snapshot
        convergence_test
        residual)
  if(NOT wavefront_metrics_stdout MATCHES "integrator_${timing_name}_worker_ms=")
    _rendercli_fail("rendercli wavefront metrics integrator ${timing_name} timing summary"
                    "wavefront metrics summary did not contain integrator ${timing_name} worker timing"
                    "${wavefront_metrics_stdout}" "" "" "")
  endif()
endforeach()
if(NOT wavefront_metrics_stdout MATCHES "frontier_hit_rays=")
  _rendercli_fail("rendercli wavefront metrics frontier hit summary"
                  "wavefront metrics summary did not contain frontier hit counters"
                  "${wavefront_metrics_stdout}" "" "" "")
endif()
if(NOT wavefront_metrics_stdout MATCHES "frontier_miss_rays=")
  _rendercli_fail("rendercli wavefront metrics frontier miss summary"
                  "wavefront metrics summary did not contain frontier miss counters"
                  "${wavefront_metrics_stdout}" "" "" "")
endif()
if(NOT wavefront_metrics_stdout MATCHES "frontier_packet_chunks=")
  _rendercli_fail("rendercli wavefront metrics frontier packet chunk summary"
                  "wavefront metrics summary did not contain frontier packet chunk counters"
                  "${wavefront_metrics_stdout}" "" "" "")
endif()
if(NOT wavefront_metrics_stdout MATCHES "frontier_packet_rays=")
  _rendercli_fail("rendercli wavefront metrics frontier packet ray summary"
                  "wavefront metrics summary did not contain frontier packet ray counters"
                  "${wavefront_metrics_stdout}" "" "" "")
endif()
if(NOT wavefront_metrics_stdout MATCHES "frontier_ray4_packet_chunks=")
  _rendercli_fail("rendercli wavefront metrics frontier Ray4 packet chunk summary"
                  "wavefront metrics summary did not contain frontier Ray4 packet chunk counters"
                  "${wavefront_metrics_stdout}" "" "" "")
endif()
if(NOT wavefront_metrics_stdout MATCHES "frontier_ray8_packet_chunks=")
  _rendercli_fail("rendercli wavefront metrics frontier Ray8 packet chunk summary"
                  "wavefront metrics summary did not contain frontier Ray8 packet chunk counters"
                  "${wavefront_metrics_stdout}" "" "" "")
endif()
if(NOT wavefront_metrics_stdout MATCHES "frontier_packet_fill=")
  _rendercli_fail("rendercli wavefront metrics frontier packet fill summary"
                  "wavefront metrics summary did not contain frontier packet fill ratio"
                  "${wavefront_metrics_stdout}" "" "" "")
endif()
if(NOT wavefront_metrics_stdout MATCHES "frontier_scalar_tail_fraction=")
  _rendercli_fail("rendercli wavefront metrics frontier scalar tail fraction summary"
                  "wavefront metrics summary did not contain frontier scalar tail fraction"
                  "${wavefront_metrics_stdout}" "" "" "")
endif()
if(NOT wavefront_metrics_stdout MATCHES "frontier_scalar_rays=")
  _rendercli_fail("rendercli wavefront metrics frontier scalar ray summary"
                  "wavefront metrics summary did not contain frontier scalar ray counters"
                  "${wavefront_metrics_stdout}" "" "" "")
endif()
if(NOT wavefront_metrics_stdout MATCHES "frontier_packet_scalar_fallback_rays=")
  _rendercli_fail("rendercli wavefront metrics frontier packet scalar fallback summary"
                  "wavefront metrics summary did not contain frontier packet scalar fallback counters"
                  "${wavefront_metrics_stdout}" "" "" "")
endif()
if(NOT wavefront_metrics_stdout MATCHES "frontier_packet_scalar_fallback_fraction=")
  _rendercli_fail("rendercli wavefront metrics frontier packet scalar fallback fraction summary"
                  "wavefront metrics summary did not contain frontier packet scalar fallback fraction"
                  "${wavefront_metrics_stdout}" "" "" "")
endif()
if(NOT wavefront_metrics_stdout MATCHES "frontier_packet_scalar_fallback_by_reason=")
  _rendercli_fail("rendercli wavefront metrics frontier packet scalar fallback reason summary"
                  "wavefront metrics summary did not contain frontier packet scalar fallback reason counters"
                  "${wavefront_metrics_stdout}" "" "" "")
endif()
if(NOT wavefront_metrics_stdout MATCHES "frontier_packet_scalar_fallback_rays=0")
  _rendercli_fail("rendercli wavefront metrics zero packet scalar fallback summary"
                  "wavefront metrics summary reported scalar packet fallback work for the stable wavefront fixture"
                  "${wavefront_metrics_stdout}" "" "" "")
endif()
if(NOT wavefront_metrics_stdout MATCHES "frontier_packet_scalar_fallback_by_reason=none")
  _rendercli_fail("rendercli wavefront metrics empty packet scalar fallback reason summary"
                  "wavefront metrics summary reported packet scalar fallback reasons for the stable wavefront fixture"
                  "${wavefront_metrics_stdout}" "" "" "")
endif()
if(NOT wavefront_metrics_stdout MATCHES "frontier_packet_refined_rays=")
  _rendercli_fail("rendercli wavefront metrics frontier packet refined summary"
                  "wavefront metrics summary did not contain frontier packet refined counters"
                  "${wavefront_metrics_stdout}" "" "" "")
endif()
if(NOT wavefront_metrics_stdout MATCHES "frontier_packet_refined_by_material=")
  _rendercli_fail("rendercli wavefront metrics frontier packet refined material summary"
                  "wavefront metrics summary did not contain frontier packet refined material counters"
                  "${wavefront_metrics_stdout}" "" "" "")
endif()
if(NOT wavefront_metrics_stdout MATCHES "frontier_packet_refined_rays=0")
  _rendercli_fail("rendercli wavefront metrics zero packet refined summary"
                  "wavefront metrics summary reported packet hit refinement for the stable wavefront fixture"
                  "${wavefront_metrics_stdout}" "" "" "")
endif()
if(NOT wavefront_metrics_stdout MATCHES "frontier_packet_refined_by_material=none")
  _rendercli_fail("rendercli wavefront metrics empty packet refined material summary"
                  "wavefront metrics summary reported packet hit refinement material buckets for the stable wavefront fixture"
                  "${wavefront_metrics_stdout}" "" "" "")
endif()
foreach(variance_name
        sample_variance_pixels
        sample_stddev_rms
        max_sample_stddev)
  if(NOT wavefront_metrics_stdout MATCHES "${variance_name}=")
    _rendercli_fail("rendercli wavefront metrics ${variance_name} summary"
                    "wavefront metrics summary did not contain ${variance_name}"
                    "${wavefront_metrics_stdout}" "" "" "")
  endif()
endforeach()
foreach(emitter_name
        emitter_hit_samples
        primary_emitter_hit_samples
        delta_emitter_hit_samples
        bsdf_emitter_hit_samples
        mis_weighted_emitter_hit_samples)
  if(NOT wavefront_metrics_stdout MATCHES "${emitter_name}=")
    _rendercli_fail("rendercli wavefront metrics ${emitter_name} summary"
                    "wavefront metrics summary did not contain ${emitter_name}"
                    "${wavefront_metrics_stdout}" "" "" "")
  endif()
	endforeach()
	if(NOT wavefront_metrics_stdout MATCHES "unsupported_path_material_samples=")
	  _rendercli_fail("rendercli wavefront metrics unsupported path material summary"
	                  "wavefront metrics summary did not contain unsupported_path_material_samples"
	                  "${wavefront_metrics_stdout}" "" "" "")
	endif()
	foreach(direct_light_name
	        direct_light_samples
        direct_light_contributing_samples
        direct_light_occluded_samples)
  if(NOT wavefront_metrics_stdout MATCHES "${direct_light_name}=")
    _rendercli_fail("rendercli wavefront metrics ${direct_light_name} summary"
                    "wavefront metrics summary did not contain ${direct_light_name}"
                    "${wavefront_metrics_stdout}" "" "" "")
  endif()
endforeach()
foreach(contribution_name
        emitted_luminance
        direct_light_luminance
        primary_direct_light_luminance
        secondary_direct_light_luminance
        ambient_luminance
        miss_luminance
        compatibility_shade_luminance)
  if(NOT wavefront_metrics_stdout MATCHES "${contribution_name}=")
    _rendercli_fail("rendercli wavefront metrics ${contribution_name} summary"
                    "wavefront metrics summary did not contain ${contribution_name}"
                    "${wavefront_metrics_stdout}" "" "" "")
  endif()
endforeach()
foreach(adaptive_name
        adaptive
        adaptive_min_samples
        adaptive_stddev_threshold
        adaptive_max_samples
        adaptive_skipped_samples
        adaptive_skipped_fraction)
  if(NOT wavefront_metrics_stdout MATCHES "${adaptive_name}=")
    _rendercli_fail("rendercli wavefront metrics ${adaptive_name} summary"
                    "wavefront metrics summary did not contain ${adaptive_name}"
                    "${wavefront_metrics_stdout}" "" "" "")
  endif()
endforeach()
if(NOT wavefront_metrics_stdout MATCHES "feedback_depths=")
  _rendercli_fail("rendercli wavefront metrics convergence feedback summary"
                  "wavefront metrics summary did not contain convergence feedback counters"
                  "${wavefront_metrics_stdout}" "" "" "")
endif()
if(NOT wavefront_metrics_stdout MATCHES "integrator_frontier_partition_worker_ms=")
  _rendercli_fail("rendercli wavefront metrics frontier partition summary"
                  "wavefront metrics summary did not contain frontier partition timing"
                  "${wavefront_metrics_stdout}" "" "" "")
endif()
rendercli_assert_exists("${wavefront_metrics_report}" NAME "wavefront metrics report exists")
file(READ "${wavefront_metrics_report}" wavefront_metrics_json)
if(NOT wavefront_metrics_json MATCHES "\"schema\"[^\n]*raytracer\\.wavefront_metrics\\.v1")
  _rendercli_fail("rendercli wavefront metrics schema"
                  "wavefront metrics report did not contain schema marker"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"activeSamplesPerDepth\"")
  _rendercli_fail("rendercli wavefront metrics batching"
                  "wavefront metrics report did not contain batch counters"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"intersectionBackendRequest\"[ \r\n]*:[ \r\n]*\"gpu\"")
  _rendercli_fail("rendercli wavefront metrics backend request"
                  "wavefront metrics report did not contain GPU backend request"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"intersectionBackend\"[ \r\n]*:[ \r\n]*\"cpu\"")
  _rendercli_fail("rendercli wavefront metrics resolved backend"
                  "wavefront metrics report did not contain CPU resolved backend"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"intersectionBackendAvailability\"[ \r\n]*:[ \r\n]*\"fallback\"")
  _rendercli_fail("rendercli wavefront metrics backend fallback"
                  "wavefront metrics report did not contain fallback backend availability"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES
       "\"intersectionBackendFallbackReason\"[ \r\n]*:[^\n]*(Metal|Vulkan) wavefront intersection backend is not enabled in this build")
  _rendercli_fail("rendercli wavefront metrics backend platform fallback reason"
                  "wavefront metrics report did not contain platform backend fallback reason"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"intersectionBackendExecutionPath\"[ \r\n]*:[ \r\n]*\"packed_cpu\"")
  _rendercli_fail("rendercli wavefront metrics backend execution path"
                  "wavefront metrics report did not contain packed CPU execution path"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"intersectionBackendUploadWorkerSeconds\"[ \r\n]*:[ \r\n]*0")
  _rendercli_fail("rendercli wavefront metrics backend upload time"
                  "wavefront metrics report did not contain backend upload timing"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"intersectionBackendKernelWorkerSeconds\"[ \r\n]*:[ \r\n]*0")
  _rendercli_fail("rendercli wavefront metrics backend kernel time"
                  "wavefront metrics report did not contain backend kernel timing"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"intersectionBackendReadbackWorkerSeconds\"[ \r\n]*:[ \r\n]*0")
  _rendercli_fail("rendercli wavefront metrics backend readback time"
                  "wavefront metrics report did not contain backend readback timing"
                  "" "" "${wavefront_metrics_json}" "")
endif()
rendercli_run(
  NAME "rendercli writes platform wavefront backend fallback to graph trace"
  COMMAND
    "${RENDERCLI}" --engine wavefront --wavefront_intersection_backend gpu --width 16
    --height 16 --render_graph_trace_out "${wavefront_unsupported_backend_trace}"
    "${static_scene}" "${wavefront_unsupported_backend_trace_render}"
)
rendercli_assert_image_nonempty("${wavefront_unsupported_backend_trace_render}"
                                NAME "unsupported wavefront backend trace render pixels")
rendercli_assert_nonempty("${wavefront_unsupported_backend_trace}"
                          NAME "unsupported wavefront backend trace JSON")
file(READ "${wavefront_unsupported_backend_trace}" wavefront_unsupported_backend_trace_json)
foreach(expectation
        "\"id\": \"wavefront_beauty\""
        "\"intersectionBackendRequest\"[ \r\n]*:[ \r\n]*\"gpu\""
        "\"intersectionBackend\"[ \r\n]*:[ \r\n]*\"cpu\""
        "\"intersectionBackendAvailability\"[ \r\n]*:[ \r\n]*\"fallback\""
        "\"intersectionBackendExecutionPath\"[ \r\n]*:[ \r\n]*\"packed_cpu\""
        "\"intersectionBackendFallbackReason\"[ \r\n]*:[^\n]*(Metal|Vulkan) wavefront intersection backend is not enabled in this build")
  if(NOT wavefront_unsupported_backend_trace_json MATCHES "${expectation}")
    _rendercli_fail("rendercli platform wavefront backend trace ${expectation}"
                    "platform wavefront backend trace did not match ${expectation}"
                    "" "" "${wavefront_unsupported_backend_trace_json}" "")
  endif()
endforeach()
	if(NOT wavefront_metrics_json MATCHES "\"activeSampleDepthsProcessed\"")
	  _rendercli_fail("rendercli wavefront metrics sample-depth work"
	                  "wavefront metrics report did not contain active sample-depth work"
	                  "" "" "${wavefront_metrics_json}" "")
	endif()
	if(NOT wavefront_metrics_json MATCHES "\"unsupportedPathMaterialSamples\"")
	  _rendercli_fail("rendercli wavefront metrics unsupported path materials"
	                  "wavefront metrics report did not contain unsupported path material samples"
	                  "" "" "${wavefront_metrics_json}" "")
	endif()
	if(NOT wavefront_metrics_json MATCHES "\"retainedActiveSamplesPerDepth\"")
  _rendercli_fail("rendercli wavefront metrics retained active samples"
                  "wavefront metrics report did not contain retained active sample counts"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"integratorFrontierPartitionWorkerSeconds\"")
  _rendercli_fail("rendercli wavefront metrics frontier partition timing"
                  "wavefront metrics report did not contain frontier partition timing"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_stdout MATCHES "last_retained_active=")
  _rendercli_fail("rendercli wavefront metrics retained active summary"
                  "wavefront metrics summary did not contain last_retained_active"
                  "${wavefront_metrics_stdout}" "" "" "")
endif()
foreach(tiling_field
        tileRows
        tileColumns
        maxTileWidth
        maxTileHeight
        maxTilePixels
        averageTilePixels
        minNonEmptyTileSamples
        maxTileSamples
        averageNonEmptyTileSamples)
  if(NOT wavefront_metrics_json MATCHES "\"${tiling_field}\"")
    _rendercli_fail("rendercli wavefront metrics ${tiling_field}"
                    "wavefront metrics report did not contain ${tiling_field}"
                    "" "" "${wavefront_metrics_json}" "")
  endif()
endforeach()
if(NOT wavefront_metrics_json MATCHES "\"feedbackDepthCount\"")
  _rendercli_fail("rendercli wavefront metrics convergence feedback"
                  "wavefront metrics report did not contain convergence feedback counters"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"frontierRayHitsPerDepth\"")
  _rendercli_fail("rendercli wavefront metrics frontier hits"
                  "wavefront metrics report did not contain frontier hit counters"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"frontierRayMissesPerDepth\"")
  _rendercli_fail("rendercli wavefront metrics frontier misses"
                  "wavefront metrics report did not contain frontier miss counters"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"frontierPacketChunksPerDepth\"")
  _rendercli_fail("rendercli wavefront metrics frontier packet chunks"
                  "wavefront metrics report did not contain frontier packet chunk counters"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"frontierPacketRaysPerDepth\"")
  _rendercli_fail("rendercli wavefront metrics frontier packet rays"
                  "wavefront metrics report did not contain frontier packet ray counters"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"frontierClosestHitBatchChunksPerDepth\"")
  _rendercli_fail("rendercli wavefront metrics frontier closest-hit batch chunks"
                  "wavefront metrics report did not contain frontier closest-hit batch chunk counters"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"frontierClosestHitBatchRaysPerDepth\"")
  _rendercli_fail("rendercli wavefront metrics frontier closest-hit batch rays"
                  "wavefront metrics report did not contain frontier closest-hit batch ray counters"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"directLightAnyHitBatchChunksPerDepth\"")
  _rendercli_fail("rendercli wavefront metrics direct-light any-hit batch chunks"
                  "wavefront metrics report did not contain direct-light any-hit batch chunk counters"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"directLightAnyHitBatchRaysPerDepth\"")
  _rendercli_fail("rendercli wavefront metrics direct-light any-hit batch rays"
                  "wavefront metrics report did not contain direct-light any-hit batch ray counters"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"intersectionBackendPrefersClosestHitBatch\"")
  _rendercli_fail("rendercli wavefront metrics closest-hit batch preference"
                  "wavefront metrics report did not contain closest-hit batch preference"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"intersectionBackendPrefersAnyHitBatch\"")
  _rendercli_fail("rendercli wavefront metrics any-hit batch preference"
                  "wavefront metrics report did not contain any-hit batch preference"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"intersectionBackendPlatform\"")
  _rendercli_fail("rendercli wavefront metrics backend platform"
                  "wavefront metrics report did not contain backend platform"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"intersectionBackendPlatformGpuDeviceAvailable\"")
  _rendercli_fail("rendercli wavefront metrics platform GPU device availability"
                  "wavefront metrics report did not contain platform GPU device availability"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"intersectionBackendPlatformGpuRenderPathAvailable\"")
  _rendercli_fail("rendercli wavefront metrics platform GPU render path availability"
                  "wavefront metrics report did not contain platform GPU render path availability"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"frontierRay4PacketChunksPerDepth\"")
  _rendercli_fail("rendercli wavefront metrics frontier Ray4 packet chunks"
                  "wavefront metrics report did not contain frontier Ray4 packet chunk counters"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"frontierRay8PacketChunksPerDepth\"")
  _rendercli_fail("rendercli wavefront metrics frontier Ray8 packet chunks"
                  "wavefront metrics report did not contain frontier Ray8 packet chunk counters"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"frontierScalarRaysPerDepth\"")
  _rendercli_fail("rendercli wavefront metrics frontier scalar rays"
                  "wavefront metrics report did not contain frontier scalar ray counters"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"frontierPacketScalarFallbackRaysPerDepth\"")
  _rendercli_fail("rendercli wavefront metrics frontier packet scalar fallback rays"
                  "wavefront metrics report did not contain frontier packet scalar fallback counters"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"frontierPacketScalarFallbackRaysByReason\"")
  _rendercli_fail("rendercli wavefront metrics frontier packet scalar fallback reasons"
                  "wavefront metrics report did not contain frontier packet scalar fallback reason counters"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"frontierPacketRefinedRaysPerDepth\"")
  _rendercli_fail("rendercli wavefront metrics frontier packet refined rays"
                  "wavefront metrics report did not contain frontier packet refined counters"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"frontierPacketRefinedRaysByMaterial\"")
  _rendercli_fail("rendercli wavefront metrics frontier packet refined material rays"
                  "wavefront metrics report did not contain frontier packet refined material counters"
                  "" "" "${wavefront_metrics_json}" "")
endif()
foreach(variance_field
        sampleVariancePixelArea
        sampleRadianceStddevRms
        maxSampleRadianceStddev)
  if(NOT wavefront_metrics_json MATCHES "\"${variance_field}\"")
    _rendercli_fail("rendercli wavefront metrics ${variance_field}"
                    "wavefront metrics report did not contain ${variance_field}"
                    "" "" "${wavefront_metrics_json}" "")
  endif()
endforeach()
if(NOT wavefront_metrics_json MATCHES "\"adaptiveSampling\"")
  _rendercli_fail("rendercli wavefront metrics adaptive sampling"
                  "wavefront metrics report did not contain adaptive sampling metadata"
                  "" "" "${wavefront_metrics_json}" "")
endif()
foreach(adaptive_field
        minimumSamples
        stddevThreshold
        maximumPrimarySamples
        skippedPrimarySamples
        skippedPrimarySampleFraction)
  if(NOT wavefront_metrics_json MATCHES "\"${adaptive_field}\"")
    _rendercli_fail("rendercli wavefront metrics adaptive ${adaptive_field}"
                    "wavefront metrics report did not contain adaptive ${adaptive_field}"
                    "" "" "${wavefront_metrics_json}" "")
  endif()
endforeach()
if(NOT wavefront_metrics_json MATCHES "\"sampleGenerationWorkerSeconds\"")
  _rendercli_fail("rendercli wavefront metrics sample generation worker timing"
                  "wavefront metrics report did not contain sample-generation worker timing"
                  "" "" "${wavefront_metrics_json}" "")
endif()
foreach(timing_field
        sampleStreamWorkerSeconds
        primaryRayWorkerSeconds
        sampleEnqueueWorkerSeconds
        sampleGenerationOverheadWorkerSeconds)
  if(NOT wavefront_metrics_json MATCHES "\"${timing_field}\"")
    _rendercli_fail("rendercli wavefront metrics ${timing_field}"
                    "wavefront metrics report did not contain ${timing_field}"
                    "" "" "${wavefront_metrics_json}" "")
  endif()
endforeach()
if(NOT wavefront_metrics_json MATCHES "\"integratorBatchWorkerSeconds\"")
  _rendercli_fail("rendercli wavefront metrics integrator worker timing"
                  "wavefront metrics report did not contain integrator-batch worker timing"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"integratorIntersectionWorkerSeconds\"")
  _rendercli_fail("rendercli wavefront metrics integrator intersection timing"
                  "wavefront metrics report did not contain integrator intersection worker timing"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"integratorShadingWorkerSeconds\"")
  _rendercli_fail("rendercli wavefront metrics integrator shading timing"
                  "wavefront metrics report did not contain integrator shading worker timing"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"integratorOverheadWorkerSeconds\"")
  _rendercli_fail("rendercli wavefront metrics integrator overhead timing"
                  "wavefront metrics report did not contain integrator overhead worker timing"
                  "" "" "${wavefront_metrics_json}" "")
endif()
foreach(timing_field
        integratorPathSetupWorkerSeconds
        integratorFrontierBookkeepingWorkerSeconds
        integratorProgressSnapshotWorkerSeconds
        integratorConvergenceTestWorkerSeconds
        integratorResidualWorkerSeconds)
  if(NOT wavefront_metrics_json MATCHES "\"${timing_field}\"")
    _rendercli_fail("rendercli wavefront metrics ${timing_field}"
                    "wavefront metrics report did not contain ${timing_field}"
                    "" "" "${wavefront_metrics_json}" "")
  endif()
endforeach()
if(NOT wavefront_metrics_json MATCHES "\"radianceDeltaRmsPerDepth\"")
  _rendercli_fail("rendercli wavefront metrics radiance delta"
                  "wavefront metrics report did not contain radiance-delta counters"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"convergence\"")
  _rendercli_fail("rendercli wavefront metrics convergence"
                  "wavefront metrics report did not contain convergence metadata"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"denoise\"")
  _rendercli_fail("rendercli wavefront metrics denoise"
                  "wavefront metrics report did not contain denoise metadata"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"denoiser\"[ \r\n]*:[ \r\n]*\"box\"")
  _rendercli_fail("rendercli wavefront metrics denoiser"
                  "wavefront metrics report did not contain denoiser name"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"radius\"[ \r\n]*:[ \r\n]*2")
  _rendercli_fail("rendercli wavefront metrics denoiser parameters"
                  "wavefront metrics report did not contain denoiser radius"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"features\"")
  _rendercli_fail("rendercli wavefront metrics denoiser features"
                  "wavefront metrics report did not contain denoiser feature metadata"
                  "" "" "${wavefront_metrics_json}" "")
endif()
if(NOT wavefront_metrics_json MATCHES "\"featureSeconds\"")
  _rendercli_fail("rendercli wavefront metrics denoiser feature timing"
                  "wavefront metrics report did not contain denoiser feature timing"
                  "" "" "${wavefront_metrics_json}" "")
endif()

rendercli_run(
  NAME "rendercli reports prepared wavefront GPU backend metrics for supported scene"
  OUTPUT_VARIABLE wavefront_supported_backend_stdout
  COMMAND
    "${RENDERCLI}" --engine wavefront --width 16 --height 16
    --wavefront_intersection_backend gpu
    --wavefront_metrics_out "${wavefront_supported_backend_report}"
    --wavefront_metrics_summary "${wavefront_supported_backend_scene}"
    "${wavefront_supported_backend_render}"
)
rendercli_assert_image_nonempty("${wavefront_supported_backend_render}"
                                NAME "supported wavefront backend render pixels")
rendercli_assert_exists("${wavefront_supported_backend_report}"
                        NAME "supported wavefront backend metrics report exists")
foreach(expectation
        "intersection_backend_request=gpu"
        "intersection_backend=(cpu|metal|vulkan)"
        "intersection_backend_availability=(fallback|available)"
        "intersection_backend_fallback=(none|Metal_wavefront_intersection_backend.*|Vulkan_wavefront_intersection_backend.*|Metal_(closest-hit|any-hit)_kernel_failed.*|Vulkan_(closest-hit|any-hit)_kernel_failed.*)"
        "intersection_backend_execution=(packed_cpu|metal|vulkan)"
        "intersection_scene_compiled=true"
        "intersection_scene_unsupported=0"
        "intersection_scene_triangles=[1-9][0-9]*"
        "intersection_scene_spheres=[1-9][0-9]*"
        "intersection_scene_planes=[0-9][0-9]*"
        "intersection_scene_rectangles=[0-9][0-9]*"
        "intersection_scene_disks=2"
        "intersection_scene_open_cylinders=1"
        "intersection_scene_transforms=[0-9][0-9]*"
        "intersection_scene_upload_bytes=[1-9][0-9]*"
        "intersection_scene_basic_hit_kernel_eligible=true"
        "intersection_scene_packed_closest_hit_eligible=true"
        "intersection_scene_packed_any_hit_eligible=true"
        "intersection_estimated_ray_upload_bytes=[1-9][0-9]*"
        "intersection_estimated_closest_hit_readback_bytes=[1-9][0-9]*"
        "intersection_estimated_query_transfer_bytes=[1-9][0-9]*"
        "intersection_rays=[1-9][0-9]*"
        "closest_hit_rays=[1-9][0-9]*"
        "any_hit_rays=0"
        "closest_hit_queries=[1-9][0-9]*")
  if(NOT wavefront_supported_backend_stdout MATCHES "${expectation}")
    _rendercli_fail("rendercli supported wavefront backend summary ${expectation}"
                    "supported wavefront backend summary did not match ${expectation}"
                    "${wavefront_supported_backend_stdout}" "" "" "")
  endif()
endforeach()
file(READ "${wavefront_supported_backend_report}" wavefront_supported_backend_json)
foreach(expectation
        "\"intersectionBackendRequest\"[ \r\n]*:[ \r\n]*\"gpu\""
        "\"intersectionBackend\"[ \r\n]*:[ \r\n]*\"(cpu|metal|vulkan)\""
        "\"intersectionBackendAvailability\"[ \r\n]*:[ \r\n]*\"(fallback|available)\""
        "\"intersectionBackendFallbackReason\"[ \r\n]*:[ \r\n]*\"(|Metal wavefront intersection backend[^\"]*|Vulkan wavefront intersection backend[^\"]*|Metal (closest-hit|any-hit) kernel failed:[^\"]*|Vulkan (closest-hit|any-hit) kernel failed:[^\"]*)\""
        "\"intersectionBackendExecutionPath\"[ \r\n]*:[ \r\n]*\"(packed_cpu|metal|vulkan)\""
        "\"intersectionSceneCompiled\"[ \r\n]*:[ \r\n]*true"
        "\"intersectionScenePrimitives\"[ \r\n]*:[ \r\n]*[1-9][0-9]*"
        "\"intersectionSceneBvhNodes\"[ \r\n]*:[ \r\n]*[1-9][0-9]*"
        "\"intersectionSceneTriangles\"[ \r\n]*:[ \r\n]*[1-9][0-9]*"
        "\"intersectionSceneSpheres\"[ \r\n]*:[ \r\n]*1"
        "\"intersectionScenePlanes\"[ \r\n]*:[ \r\n]*0"
        "\"intersectionSceneRectangles\"[ \r\n]*:[ \r\n]*0"
        "\"intersectionSceneDisks\"[ \r\n]*:[ \r\n]*2"
        "\"intersectionSceneOpenCylinders\"[ \r\n]*:[ \r\n]*1"
        "\"intersectionSceneTransforms\"[ \r\n]*:[ \r\n]*[1-9][0-9]*"
        "\"intersectionSceneUnsupportedPrimitives\"[ \r\n]*:[ \r\n]*0"
        "\"intersectionSceneUploadBytes\"[ \r\n]*:[ \r\n]*[1-9][0-9]*"
        "\"intersectionSceneBasicHitEligible\"[ \r\n]*:[ \r\n]*true"
        "\"intersectionScenePackedClosestHitEligible\"[ \r\n]*:[ \r\n]*true"
        "\"intersectionScenePackedAnyHitEligible\"[ \r\n]*:[ \r\n]*true"
        "\"intersectionEstimatedRayUploadBytes\"[ \r\n]*:[ \r\n]*[1-9][0-9]*"
        "\"intersectionEstimatedClosestHitReadbackBytes\"[ \r\n]*:[ \r\n]*[1-9][0-9]*"
        "\"intersectionEstimatedQueryTransferBytes\"[ \r\n]*:[ \r\n]*[1-9][0-9]*"
        "\"closestHitRaysSubmitted\"[ \r\n]*:[ \r\n]*[1-9][0-9]*"
        "\"anyHitRaysSubmitted\"[ \r\n]*:[ \r\n]*0")
  if(NOT wavefront_supported_backend_json MATCHES "${expectation}")
    _rendercli_fail("rendercli supported wavefront backend report ${expectation}"
                    "supported wavefront backend report did not match ${expectation}"
                    "" "" "${wavefront_supported_backend_json}" "")
  endif()
endforeach()

rendercli_run(
  NAME "rendercli writes wavefront backend diagnostics to graph trace"
  COMMAND
    "${RENDERCLI}" --engine wavefront --width 16 --height 16
    --wavefront_intersection_backend gpu
    --render_graph_trace_out "${wavefront_supported_backend_trace}"
    "${wavefront_supported_backend_scene}" "${wavefront_supported_backend_trace_render}"
)
rendercli_assert_image_nonempty("${wavefront_supported_backend_trace_render}"
                                NAME "supported wavefront backend trace render pixels")
rendercli_assert_nonempty("${wavefront_supported_backend_trace}"
                          NAME "supported wavefront backend trace JSON")
file(READ "${wavefront_supported_backend_trace}" wavefront_supported_backend_trace_json)
foreach(expectation
        "\"id\": \"wavefront_beauty\""
        "\"metadata\""
        "\"batching\""
        "\"intersectionBackendRequest\"[ \r\n]*:[ \r\n]*\"gpu\""
        "\"intersectionBackend\"[ \r\n]*:[ \r\n]*\"(cpu|metal|vulkan)\""
        "\"intersectionBackendAvailability\"[ \r\n]*:[ \r\n]*\"(fallback|available)\""
        "\"intersectionBackendFallbackReason\"[ \r\n]*:[ \r\n]*\"(|Metal wavefront intersection backend[^\"]*|Vulkan wavefront intersection backend[^\"]*|Metal (closest-hit|any-hit) kernel failed:[^\"]*|Vulkan (closest-hit|any-hit) kernel failed:[^\"]*)\""
        "\"intersectionBackendExecutionPath\"[ \r\n]*:[ \r\n]*\"(packed_cpu|metal|vulkan)\""
        "\"intersectionSceneCompiled\"[ \r\n]*:[ \r\n]*true"
        "\"intersectionScenePrimitives\"[ \r\n]*:[ \r\n]*[1-9][0-9]*"
        "\"intersectionSceneBvhNodes\"[ \r\n]*:[ \r\n]*[1-9][0-9]*"
        "\"intersectionSceneTriangles\"[ \r\n]*:[ \r\n]*[1-9][0-9]*"
        "\"intersectionSceneSpheres\"[ \r\n]*:[ \r\n]*1"
        "\"intersectionScenePlanes\"[ \r\n]*:[ \r\n]*0"
        "\"intersectionSceneRectangles\"[ \r\n]*:[ \r\n]*0"
        "\"intersectionSceneDisks\"[ \r\n]*:[ \r\n]*2"
        "\"intersectionSceneOpenCylinders\"[ \r\n]*:[ \r\n]*1"
        "\"intersectionSceneTransforms\"[ \r\n]*:[ \r\n]*[1-9][0-9]*"
        "\"intersectionSceneUnsupportedPrimitives\"[ \r\n]*:[ \r\n]*0"
        "\"intersectionSceneUploadBytes\"[ \r\n]*:[ \r\n]*[1-9][0-9]*"
        "\"intersectionEstimatedQueryTransferBytes\"[ \r\n]*:[ \r\n]*[1-9][0-9]*")
  if(NOT wavefront_supported_backend_trace_json MATCHES "${expectation}")
    _rendercli_fail("rendercli supported wavefront backend trace ${expectation}"
                    "supported wavefront backend trace did not match ${expectation}"
                    "" "" "${wavefront_supported_backend_trace_json}" "")
  endif()
endforeach()

rendercli_run(
  NAME "rendercli keeps small supported wavefront auto backend on CPU"
  OUTPUT_VARIABLE wavefront_supported_backend_auto_stdout
  COMMAND
    "${RENDERCLI}" --engine wavefront --width 16 --height 16
    --wavefront_intersection_backend auto
    --wavefront_metrics_out "${wavefront_supported_backend_auto_report}"
    --wavefront_metrics_summary "${wavefront_supported_backend_scene}"
    "${wavefront_supported_backend_auto_render}"
)
rendercli_assert_image_nonempty("${wavefront_supported_backend_auto_render}"
                                NAME "small supported wavefront auto backend render pixels")
rendercli_assert_exists("${wavefront_supported_backend_auto_report}"
                        NAME "small supported wavefront auto backend metrics report exists")
foreach(expectation
        "intersection_backend_request=auto"
        "intersection_backend=cpu"
        "intersection_backend_availability=available"
        "intersection_backend_fallback=auto_selected_CPU"
        "intersection_backend_execution=runtime_scene"
        "intersection_expected_rays=[1-9][0-9]*"
        "intersection_auto_minimum_gpu_rays=[1-9][0-9]*")
  if(NOT wavefront_supported_backend_auto_stdout MATCHES "${expectation}")
    _rendercli_fail("rendercli small supported wavefront auto summary ${expectation}"
                    "small supported wavefront auto summary did not match ${expectation}"
                    "${wavefront_supported_backend_auto_stdout}" "" "" "")
  endif()
endforeach()
file(READ "${wavefront_supported_backend_auto_report}" wavefront_supported_backend_auto_json)
foreach(expectation
        "\"intersectionBackendRequest\"[ \r\n]*:[ \r\n]*\"auto\""
        "\"intersectionBackend\"[ \r\n]*:[ \r\n]*\"cpu\""
        "\"intersectionBackendAvailability\"[ \r\n]*:[ \r\n]*\"available\""
        "\"intersectionBackendFallbackReason\"[ \r\n]*:[ \r\n]*\"auto selected CPU"
        "\"intersectionBackendExecutionPath\"[ \r\n]*:[ \r\n]*\"runtime_scene\""
        "\"intersectionBackendExpectedRays\"[ \r\n]*:[ \r\n]*[1-9][0-9]*"
        "\"intersectionBackendAutoMinimumGpuRays\"[ \r\n]*:[ \r\n]*[1-9][0-9]*")
  if(NOT wavefront_supported_backend_auto_json MATCHES "${expectation}")
    _rendercli_fail("rendercli small supported wavefront auto report ${expectation}"
                    "small supported wavefront auto report did not match ${expectation}"
                    "" "" "${wavefront_supported_backend_auto_json}" "")
  endif()
endforeach()

rendercli_run(
  NAME "rendercli renders supported scene with forced CPU wavefront backend"
  COMMAND
    "${RENDERCLI}" --engine wavefront --width 16 --height 16
    --wavefront_intersection_backend cpu "${wavefront_supported_backend_scene}"
    "${wavefront_supported_backend_cpu_render}"
)
rendercli_run(
  NAME "rendercli renders supported scene with requested GPU wavefront backend"
  COMMAND
    "${RENDERCLI}" --engine wavefront --width 16 --height 16
    --wavefront_intersection_backend gpu "${wavefront_supported_backend_scene}"
    "${wavefront_supported_backend_gpu_render}"
)
rendercli_assert_image_rms_at_most("${wavefront_supported_backend_cpu_render}"
                                   "${wavefront_supported_backend_gpu_render}" 0.001
                                   NAME
                                     "wavefront GPU backend supported scene RMS matches CPU")

rendercli_run(
  NAME "rendercli reports wavefront convergence-stopped tiles"
  OUTPUT_VARIABLE wavefront_converged_metrics_stdout
  STDOUT_MATCHES
    "wavefront_metrics.*pass=wavefront_beauty.*integrator=pathtracer.*execution=depth_major_paths.*active_sample_depths=.*active_depths=1.*convergence=stopped_some_tiles.*stopped_tiles=[1-9].*earliest_stop_depth=[1-9].*latest_stop_depth=[1-9]"
  COMMAND
    "${RENDERCLI}" --engine wavefront --integrator pathtracer --width 16 --height 16
    --wavefront_convergence --wavefront_convergence_active_fraction 1
    --wavefront_convergence_rms_delta 10
    --wavefront_metrics_out "${wavefront_converged_metrics_report}"
    --wavefront_metrics_summary "${static_scene}" "${wavefront_converged_metrics_render}"
)
rendercli_assert_image_nonempty("${wavefront_converged_metrics_render}"
                                NAME "wavefront converged metrics render pixels")
rendercli_assert_exists("${wavefront_converged_metrics_report}"
                        NAME "wavefront converged metrics report exists")
file(READ "${wavefront_converged_metrics_report}" wavefront_converged_metrics_json)
if(NOT wavefront_converged_metrics_json MATCHES "\"decision\"[ \r\n]*:[ \r\n]*\"stopped_some_tiles\"")
  _rendercli_fail("rendercli wavefront convergence metrics decision"
                  "wavefront convergence metrics did not report stopped tiles"
                  "" "" "${wavefront_converged_metrics_json}" "")
endif()
if(NOT wavefront_converged_metrics_json MATCHES "\"stoppedTileCount\"[ \r\n]*:[ \r\n]*[1-9]")
  _rendercli_fail("rendercli wavefront convergence metrics stopped tile count"
                  "wavefront convergence metrics did not count stopped tiles"
                  "" "" "${wavefront_converged_metrics_json}" "")
endif()
if(NOT wavefront_converged_metrics_json MATCHES "\"stoppedTileDepthHistogram\"[ \r\n]*:[ \r\n]*\\[[ \r\n]*[1-9]")
  _rendercli_fail("rendercli wavefront convergence metrics stopped depth histogram"
                  "wavefront convergence metrics did not report the stopped-depth histogram"
                  "" "" "${wavefront_converged_metrics_json}" "")
endif()
if(NOT wavefront_converged_metrics_json MATCHES "\"activeSamplesPerDepth\"[ \r\n]*:[ \r\n]*\\[[ \r\n]*[1-9]")
  _rendercli_fail("rendercli wavefront convergence metrics active depth"
                  "wavefront convergence metrics did not stop after one active depth"
                  "" "" "${wavefront_converged_metrics_json}" "")
endif()

rendercli_run(
  NAME "rendercli reports wavefront denoised convergence feedback"
  OUTPUT_VARIABLE wavefront_feedback_metrics_stdout
  STDOUT_MATCHES
    "wavefront_metrics.*pass=wavefront_beauty.*convergence=stopped_some_tiles.*feedback_depths=[1-9].*denoiser=box"
  COMMAND
    "${RENDERCLI}" --engine wavefront --integrator pathtracer --width 16 --height 16
    --wavefront_denoiser box --wavefront_denoise_radius 1
    --wavefront_convergence --wavefront_convergence_active_fraction 1
    --wavefront_convergence_rms_delta 10
    --wavefront_metrics_out "${wavefront_feedback_metrics_report}"
    --wavefront_metrics_summary "${static_scene}" "${wavefront_feedback_metrics_render}"
)
rendercli_assert_image_nonempty("${wavefront_feedback_metrics_render}"
                                NAME "wavefront feedback metrics render pixels")
rendercli_assert_exists("${wavefront_feedback_metrics_report}"
                        NAME "wavefront feedback metrics report exists")
file(READ "${wavefront_feedback_metrics_report}" wavefront_feedback_metrics_json)
if(NOT wavefront_feedback_metrics_json MATCHES "\"feedbackDepthCount\"[ \r\n]*:[ \r\n]*[1-9]")
  _rendercli_fail("rendercli wavefront denoised convergence feedback metrics"
                  "wavefront convergence metrics did not count denoised feedback depths"
                  "${wavefront_feedback_metrics_stdout}" "" "${wavefront_feedback_metrics_json}" "")
endif()

rendercli_run(
  NAME "rendercli writes direct wavefront metrics JSON and summary"
  OUTPUT_VARIABLE wavefront_direct_metrics_stdout
  STDOUT_MATCHES
    "wavefront_metrics.*integrator=whitted.*execution=depth_major_whitted.*samples=.*convergence=disabled"
  COMMAND
    "${RENDERCLI}" --direct_engine --engine wavefront --width 16 --height 16
    --wavefront_metrics_out "${wavefront_direct_metrics_report}" --wavefront_metrics_summary
    "${static_scene}" "${wavefront_direct_metrics_render}"
)
rendercli_assert_image_nonempty("${wavefront_direct_metrics_render}"
                                NAME "wavefront direct metrics render pixels")
rendercli_assert_exists("${wavefront_direct_metrics_report}"
                        NAME "wavefront direct metrics report exists")
file(READ "${wavefront_direct_metrics_report}" wavefront_direct_metrics_json)
if(NOT wavefront_direct_metrics_json MATCHES "\"schema\"[^\n]*raytracer\\.wavefront_metrics\\.v1")
  _rendercli_fail("rendercli direct wavefront metrics schema"
                  "direct wavefront metrics report did not contain schema marker"
                  "" "" "${wavefront_direct_metrics_json}" "")
endif()
if(NOT wavefront_direct_metrics_json MATCHES "\"metrics\"")
  _rendercli_fail("rendercli direct wavefront metrics payload"
                  "direct wavefront metrics report did not contain direct metrics"
                  "" "" "${wavefront_direct_metrics_json}" "")
endif()

rendercli_run(
  NAME "rendercli reports batched wavefront any-hit visibility metrics"
  OUTPUT_VARIABLE wavefront_batched_visibility_stdout
  STDOUT_MATCHES
    "wavefront_metrics.*integrator=pathtracer.*execution=depth_major_paths.*intersection_backend_platform=(metal|vulkan).*intersection_backend_gpu_device=(true|false).*intersection_backend_gpu_render_path=(true|false).*any_hit_rays=[1-9][0-9]*.*any_hit_queries=[1-9][0-9]*.*closest_hit_batch_preferred=true.*any_hit_batch_preferred=true.*direct_light_any_hit_batch_chunks=[1-9][0-9]*.*direct_light_any_hit_batch_rays=[1-9][0-9]*.*direct_light_samples=[1-9][0-9]*"
  COMMAND
    "${RENDERCLI}" --engine wavefront --integrator pathtracer --width 16 --height 16
    --pathtracer_direct_light_samples 3 --wavefront_denoiser none
    --wavefront_intersection_backend gpu
    --wavefront_metrics_summary "${pathtracer_area_light_scene}"
    "${wavefront_batched_visibility_render}"
)
rendercli_assert_image_nonempty("${wavefront_batched_visibility_render}"
                                NAME "batched wavefront visibility render pixels")
string(REGEX MATCH "any_hit_rays=([0-9][0-9]*)" _any_hit_rays_match
             "${wavefront_batched_visibility_stdout}")
set(wavefront_batched_visibility_any_hit_rays "${CMAKE_MATCH_1}")
string(REGEX MATCH "any_hit_queries=([0-9][0-9]*)" _any_hit_queries_match
             "${wavefront_batched_visibility_stdout}")
set(wavefront_batched_visibility_any_hit_queries "${CMAKE_MATCH_1}")
string(REGEX MATCH "direct_light_any_hit_batch_rays=([0-9][0-9]*)"
             _direct_light_any_hit_batch_rays_match
             "${wavefront_batched_visibility_stdout}")
set(wavefront_batched_visibility_any_hit_batch_rays "${CMAKE_MATCH_1}")
string(REGEX MATCH "direct_light_any_hit_batch_chunks=([0-9][0-9]*)"
             _direct_light_any_hit_batch_chunks_match
             "${wavefront_batched_visibility_stdout}")
set(wavefront_batched_visibility_any_hit_batch_chunks "${CMAKE_MATCH_1}")
if(NOT wavefront_batched_visibility_any_hit_rays GREATER wavefront_batched_visibility_any_hit_queries)
  _rendercli_fail(
    "rendercli batched wavefront any-hit visibility metrics"
    "expected any_hit_rays to exceed any_hit_queries when direct-light visibility is batched"
    "${wavefront_batched_visibility_stdout}" "" "" "")
endif()
if(NOT wavefront_batched_visibility_any_hit_batch_rays GREATER wavefront_batched_visibility_any_hit_batch_chunks)
  _rendercli_fail(
    "rendercli direct-light any-hit batch metrics"
    "expected direct_light_any_hit_batch_rays to exceed direct_light_any_hit_batch_chunks"
    "${wavefront_batched_visibility_stdout}" "" "" "")
endif()

rendercli_run(
  NAME "rendercli renders recursive raytracer parity baseline"
  COMMAND
    "${RENDERCLI}" --engine raytracer --width 24 --height 24 --depth 4
    "${static_scene}" "${wavefront_parity_raytracer_render}"
)
rendercli_run(
  NAME "rendercli renders wavefront parity baseline"
  COMMAND
    "${RENDERCLI}" --engine wavefront --width 24 --height 24 --depth 4
    "${static_scene}" "${wavefront_parity_render}"
)
rendercli_assert_image_rms_at_most("${wavefront_parity_raytracer_render}"
                                   "${wavefront_parity_render}" 0.001
                                   NAME "wavefront static scene RMS matches recursive raytracer")
rendercli_assert_image_hash_equals("${wavefront_parity_raytracer_render}"
                                   "${wavefront_parity_render}"
                                   NAME "wavefront graph matches recursive raytracer graph")

rendercli_run(
  NAME "rendercli renders recursive raytracer glass parity baseline"
  COMMAND
    "${RENDERCLI}" --engine raytracer --width 24 --height 24 --depth 4
    "${PROJECT_SOURCE_DIR}/scenes/glass_torus.json" "${wavefront_glass_parity_raytracer_render}"
)
rendercli_run(
  NAME "rendercli renders wavefront glass parity baseline"
  COMMAND
    "${RENDERCLI}" --engine wavefront --width 24 --height 24 --depth 4
    "${PROJECT_SOURCE_DIR}/scenes/glass_torus.json" "${wavefront_glass_parity_render}"
)
rendercli_assert_image_rms_at_most("${wavefront_glass_parity_raytracer_render}"
                                   "${wavefront_glass_parity_render}" 0.001
                                   NAME "wavefront glass torus RMS matches recursive raytracer")
rendercli_assert_image_hash_equals("${wavefront_glass_parity_raytracer_render}"
                                   "${wavefront_glass_parity_render}"
                                   NAME "wavefront graph matches recursive glass raytracer graph")

rendercli_run(
  NAME "rendercli renders recursive raytracer reflection parity baseline"
  COMMAND
    "${RENDERCLI}" --engine raytracer --width 24 --height 24 --depth 4
    "${PROJECT_SOURCE_DIR}/scenes/reflections.json"
    "${wavefront_reflection_parity_raytracer_render}"
)
rendercli_run(
  NAME "rendercli renders wavefront reflection parity baseline"
  COMMAND
    "${RENDERCLI}" --engine wavefront --width 24 --height 24 --depth 4
    "${PROJECT_SOURCE_DIR}/scenes/reflections.json" "${wavefront_reflection_parity_render}"
)
rendercli_assert_image_rms_at_most("${wavefront_reflection_parity_raytracer_render}"
                                   "${wavefront_reflection_parity_render}" 0.001
                                   NAME "wavefront reflection RMS matches recursive raytracer")
rendercli_assert_image_hash_equals("${wavefront_reflection_parity_raytracer_render}"
                                   "${wavefront_reflection_parity_render}"
                                   NAME "wavefront graph matches recursive reflection raytracer graph")

rendercli_run(
  NAME "rendercli renders recursive raytracer BVH macro baseline"
  COMMAND
    "${RENDERCLI}" --engine raytracer --width 32 --height 32 --depth 3
    "${wavefront_bvh_macro_scene}" "${wavefront_bvh_macro_raytracer_render}"
)
rendercli_run(
  NAME "rendercli renders wavefront BVH macro baseline"
  COMMAND
    "${RENDERCLI}" --engine wavefront --width 32 --height 32 --depth 3
    "${wavefront_bvh_macro_scene}" "${wavefront_bvh_macro_render}"
)
rendercli_assert_image_rms_at_most("${wavefront_bvh_macro_raytracer_render}"
                                   "${wavefront_bvh_macro_render}" 0.001
                                   NAME "wavefront BVH macro RMS matches recursive raytracer")

rendercli_run(
  NAME "rendercli renders wavefront indirect environment scene from intent"
  COMMAND
    "${RENDERCLI}" --width 32 --height 32
    "${wavefront_indirect_scene}" "${wavefront_indirect_render}"
)
rendercli_assert_image_nonempty("${wavefront_indirect_render}"
                                NAME "wavefront indirect environment render pixels")
rendercli_run(
  NAME "rendercli renders Whitted comparison for indirect environment scene"
  COMMAND
    "${RENDERCLI}" --engine raytracer --integrator whitted --width 32 --height 32
    "${wavefront_indirect_scene}" "${wavefront_indirect_whitted_render}"
)
rendercli_assert_image_hash_differs("${wavefront_indirect_whitted_render}"
                                    "${wavefront_indirect_render}"
                                    NAME "wavefront path tracing shows indirect environment light")

rendercli_run(
  NAME "rendercli renders wavefront indirect bounce scene from intent"
  COMMAND
    "${RENDERCLI}" --width 32 --height 32
    "${wavefront_indirect_bounce_scene}" "${wavefront_indirect_bounce_render}"
)
rendercli_assert_image_nonempty("${wavefront_indirect_bounce_render}"
                                NAME "wavefront indirect bounce render pixels")
rendercli_run(
  NAME "rendercli renders Whitted comparison for indirect bounce scene"
  COMMAND
    "${RENDERCLI}" --engine raytracer --integrator whitted --width 32 --height 32
    "${wavefront_indirect_bounce_scene}" "${wavefront_indirect_bounce_whitted_render}"
)
rendercli_assert_image_hash_differs("${wavefront_indirect_bounce_whitted_render}"
                                    "${wavefront_indirect_bounce_render}"
                                    NAME "wavefront path tracing shows diffuse bounce light")

rendercli_run(
  NAME "rendercli exports wavefront pathtracer state in render graph"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format json
    --engine wavefront --integrator pathtracer --pathtracer_russian_roulette_depth 4
    --pathtracer_direct_light_samples 3
    --width 32 --height 16
    "${static_scene}" "${wavefront_pathtracer_plan}"
)
rendercli_assert_nonempty("${wavefront_pathtracer_plan}"
                          NAME "wavefront pathtracer graph output")
file(READ "${wavefront_pathtracer_plan}" wavefront_pathtracer_graph)
if(NOT wavefront_pathtracer_graph MATCHES "\"executor\": \"wavefront\"")
  message(FATAL_ERROR
          "wavefront pathtracer graph did not contain wavefront executor: ${wavefront_pathtracer_graph}")
endif()
if(NOT wavefront_pathtracer_graph MATCHES "\"integrator\": \"pathtracer\"")
  message(FATAL_ERROR
          "wavefront pathtracer graph did not contain pathtracer state: ${wavefront_pathtracer_graph}")
endif()
if(NOT wavefront_pathtracer_graph MATCHES "\"russianRouletteDepth\"[ \r\n]*:[ \r\n]*4")
  message(FATAL_ERROR
          "wavefront pathtracer graph did not contain Russian roulette depth: ${wavefront_pathtracer_graph}")
endif()
if(NOT wavefront_pathtracer_graph MATCHES "\"directLightSamples\"[ \r\n]*:[ \r\n]*3")
  message(FATAL_ERROR
          "wavefront pathtracer graph did not contain direct-light samples: ${wavefront_pathtracer_graph}")
endif()

rendercli_run(
  NAME "rendercli exports scalar pathtracer schedule in render graph"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format json
    --engine pathtracer --path_tracing_schedule scalar
    --pathtracer_direct_light_samples 2 --width 32 --height 16
    "${static_scene}" "${scalar_pathtracer_plan}"
)
rendercli_assert_nonempty("${scalar_pathtracer_plan}"
                          NAME "scalar pathtracer graph output")
file(READ "${scalar_pathtracer_plan}" scalar_pathtracer_graph)
if(NOT scalar_pathtracer_graph MATCHES "\"executor\": \"raytracer\"")
  message(FATAL_ERROR
          "scalar pathtracer graph did not contain raytracer executor: ${scalar_pathtracer_graph}")
endif()
if(NOT scalar_pathtracer_graph MATCHES "\"integrator\": \"pathtracer\"")
  message(FATAL_ERROR
          "scalar pathtracer graph did not contain pathtracer state: ${scalar_pathtracer_graph}")
endif()
if(NOT scalar_pathtracer_graph MATCHES "\"directLightSamples\"[ \r\n]*:[ \r\n]*2")
  message(FATAL_ERROR
          "scalar pathtracer graph did not contain direct-light samples: ${scalar_pathtracer_graph}")
endif()

rendercli_run(
  NAME "rendercli exports wavefront convergence state in render graph"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format json
    --engine wavefront --integrator pathtracer --wavefront_convergence
    --wavefront_convergence_active_fraction 0.25 --wavefront_convergence_rms_delta 0.125
    --width 32 --height 16 "${static_scene}" "${wavefront_convergence_plan}"
)
rendercli_assert_nonempty("${wavefront_convergence_plan}"
                          NAME "wavefront convergence graph output")
file(READ "${wavefront_convergence_plan}" wavefront_convergence_graph)
if(NOT wavefront_convergence_graph MATCHES "\"convergence\"")
  message(FATAL_ERROR
          "wavefront convergence graph did not contain convergence state: ${wavefront_convergence_graph}")
endif()
if(NOT wavefront_convergence_graph MATCHES "\"enabled\"[ \r\n]*:[ \r\n]*true")
  message(FATAL_ERROR
          "wavefront convergence graph did not enable convergence: ${wavefront_convergence_graph}")
endif()
if(NOT wavefront_convergence_graph
   MATCHES "\"activeSampleFractionThreshold\"[ \r\n]*:[ \r\n]*0\\.25")
  message(FATAL_ERROR
          "wavefront convergence graph did not contain active fraction threshold: ${wavefront_convergence_graph}")
endif()
if(NOT wavefront_convergence_graph
   MATCHES "\"radianceDeltaRmsThreshold\"[ \r\n]*:[ \r\n]*0\\.125")
  message(FATAL_ERROR
          "wavefront convergence graph did not contain RMS delta threshold: ${wavefront_convergence_graph}")
endif()

rendercli_run(
  NAME "rendercli exports wavefront adaptive sampling state in render graph"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format json
    --engine wavefront --integrator pathtracer --wavefront_adaptive_sampling
    --wavefront_adaptive_min_samples 3 --wavefront_adaptive_stddev_threshold 0.05
    --width 32 --height 16 "${static_scene}" "${wavefront_adaptive_plan}"
)
rendercli_assert_nonempty("${wavefront_adaptive_plan}"
                          NAME "wavefront adaptive sampling graph output")
file(READ "${wavefront_adaptive_plan}" wavefront_adaptive_graph)
if(NOT wavefront_adaptive_graph MATCHES "\"adaptiveSampling\"")
  message(FATAL_ERROR
          "wavefront adaptive graph did not contain adaptive state: ${wavefront_adaptive_graph}")
endif()
if(NOT wavefront_adaptive_graph MATCHES "\"enabled\"[ \r\n]*:[ \r\n]*true")
  message(FATAL_ERROR
          "wavefront adaptive graph did not enable adaptive sampling: ${wavefront_adaptive_graph}")
endif()
if(NOT wavefront_adaptive_graph MATCHES "\"minimumSamples\"[ \r\n]*:[ \r\n]*3")
  message(FATAL_ERROR
          "wavefront adaptive graph did not contain minimum samples: ${wavefront_adaptive_graph}")
endif()
if(NOT wavefront_adaptive_graph MATCHES "\"stddevThreshold\"[ \r\n]*:[ \r\n]*0\\.05")
  message(FATAL_ERROR
          "wavefront adaptive graph did not contain stddev threshold: ${wavefront_adaptive_graph}")
endif()

rendercli_run(
  NAME "rendercli exports wavefront default convergence thresholds"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format json
    --engine wavefront --integrator pathtracer --wavefront_convergence
    --width 32 --height 16 "${static_scene}" "${wavefront_default_convergence_plan}"
)
rendercli_assert_nonempty("${wavefront_default_convergence_plan}"
                          NAME "wavefront default convergence graph output")
file(READ "${wavefront_default_convergence_plan}" wavefront_default_convergence_graph)
if(NOT wavefront_default_convergence_graph
   MATCHES "\"activeSampleFractionThreshold\"[ \r\n]*:[ \r\n]*0\\.05")
  message(
    FATAL_ERROR
      "wavefront default convergence graph did not contain active fraction threshold: ${wavefront_default_convergence_graph}"
  )
endif()
if(NOT wavefront_default_convergence_graph
   MATCHES "\"radianceDeltaRmsThreshold\"[ \r\n]*:[ \r\n]*0\\.002")
  message(
    FATAL_ERROR
      "wavefront default convergence graph did not contain RMS delta threshold: ${wavefront_default_convergence_graph}"
  )
endif()

rendercli_run(
  NAME "rendercli renders graph wavefront with pathtracer integrator"
  COMMAND
    "${RENDERCLI}" --engine wavefront --integrator pathtracer --width 16 --height 16
    "${static_scene}" "${wavefront_pathtracer_render}"
)
rendercli_assert_image_nonempty("${wavefront_pathtracer_render}"
                                NAME "wavefront pathtracer graph render pixels")

rendercli_run(
  NAME "rendercli exports wavefront denoiser state in render graph"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format json
    --engine wavefront --wavefront_denoiser bilateral --wavefront_denoise_radius 2
    --wavefront_denoise_color_sigma 0.2 --width 32 --height 16
    "${static_scene}" "${wavefront_denoise_plan}"
)
rendercli_assert_nonempty("${wavefront_denoise_plan}" NAME "wavefront denoise graph output")
file(READ "${wavefront_denoise_plan}" wavefront_denoise_graph)
if(NOT wavefront_denoise_graph MATCHES "\"denoise\"")
  message(FATAL_ERROR
          "wavefront denoise graph did not contain denoise state: ${wavefront_denoise_graph}")
endif()
if(NOT wavefront_denoise_graph MATCHES "\"type\"[ \r\n]*:[ \r\n]*\"bilateral\"")
  message(FATAL_ERROR
          "wavefront denoise graph did not contain bilateral denoiser: ${wavefront_denoise_graph}")
endif()
if(NOT wavefront_denoise_graph MATCHES "\"radius\"[ \r\n]*:[ \r\n]*2")
  message(FATAL_ERROR
          "wavefront denoise graph did not contain denoise radius: ${wavefront_denoise_graph}")
endif()
if(NOT wavefront_denoise_graph MATCHES "\"colorSigma\"[ \r\n]*:[ \r\n]*0\\.2")
  message(FATAL_ERROR
          "wavefront denoise graph did not contain denoise color sigma: ${wavefront_denoise_graph}")
endif()

rendercli_run(
  NAME "rendercli exports scene-authored wavefront denoiser state in render graph"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format json
    --width 32 --height 16 "${wavefront_denoise_scene}" "${wavefront_scene_denoise_plan}"
)
rendercli_assert_nonempty("${wavefront_scene_denoise_plan}"
                          NAME "scene wavefront denoise graph output")
file(READ "${wavefront_scene_denoise_plan}" wavefront_scene_denoise_graph)
if(NOT wavefront_scene_denoise_graph MATCHES "\"denoise\"")
  message(FATAL_ERROR
          "scene wavefront denoise graph did not contain denoise state: ${wavefront_scene_denoise_graph}")
endif()
if(NOT wavefront_scene_denoise_graph MATCHES "\"type\"[ \r\n]*:[ \r\n]*\"bilateral\"")
  message(FATAL_ERROR
          "scene wavefront denoise graph did not contain bilateral denoiser: ${wavefront_scene_denoise_graph}")
endif()
if(NOT wavefront_scene_denoise_graph MATCHES "\"radius\"[ \r\n]*:[ \r\n]*2")
  message(FATAL_ERROR
          "scene wavefront denoise graph did not contain denoise radius: ${wavefront_scene_denoise_graph}")
endif()
if(NOT wavefront_scene_denoise_graph MATCHES "\"colorSigma\"[ \r\n]*:[ \r\n]*0\\.18")
  message(FATAL_ERROR
          "scene wavefront denoise graph did not contain denoise color sigma: ${wavefront_scene_denoise_graph}")
endif()

rendercli_run(
  NAME "rendercli traces scene-authored wavefront bilateral denoiser"
  COMMAND
    "${RENDERCLI}" --width 32 --height 32
    --render_graph_trace_out "${wavefront_scene_denoise_trace}"
    "${wavefront_denoise_scene}" "${wavefront_scene_denoise_render}"
)
rendercli_assert_image_nonempty("${wavefront_scene_denoise_render}"
                                NAME "scene wavefront denoise render pixels")
rendercli_assert_nonempty("${wavefront_scene_denoise_trace}"
                          NAME "scene wavefront denoise trace JSON")
file(READ "${wavefront_scene_denoise_trace}" wavefront_scene_denoise_trace_json)
if(NOT wavefront_scene_denoise_trace_json MATCHES "\"id\": \"wavefront_beauty\"")
  message(FATAL_ERROR
          "scene wavefront denoise trace did not contain wavefront pass: ${wavefront_scene_denoise_trace_json}")
endif()
if(NOT wavefront_scene_denoise_trace_json MATCHES "\"denoiser\"[ \r\n]*:[ \r\n]*\"bilateral\"")
  message(FATAL_ERROR
          "scene wavefront denoise trace did not contain bilateral denoiser metadata: ${wavefront_scene_denoise_trace_json}")
endif()
if(NOT wavefront_scene_denoise_trace_json MATCHES "\"color_sigma\"[ \r\n]*:[ \r\n]*0\\.18")
  message(FATAL_ERROR
          "scene wavefront denoise trace did not contain bilateral color sigma metadata: ${wavefront_scene_denoise_trace_json}")
endif()
if(NOT wavefront_scene_denoise_trace_json MATCHES "\"features\"")
  message(FATAL_ERROR
          "scene wavefront denoise trace did not contain feature metadata: ${wavefront_scene_denoise_trace_json}")
endif()
foreach(feature_name albedo normal depth)
  if(NOT wavefront_scene_denoise_trace_json MATCHES "\"${feature_name}\"[ \r\n]*:[ \r\n]*true")
    message(FATAL_ERROR
            "scene wavefront denoise trace did not report ${feature_name} feature metadata: ${wavefront_scene_denoise_trace_json}")
  endif()
endforeach()

rendercli_run(
  NAME "rendercli renders wavefront denoise quality reference"
  COMMAND
    "${RENDERCLI}" --engine wavefront --integrator pathtracer --wavefront_denoiser none
    --sampling_seed 1337 --samples_per_pixel 64 --width 32 --height 32
    "${wavefront_denoise_scene}" "${wavefront_denoise_quality_reference}"
)
rendercli_assert_image_nonempty("${wavefront_denoise_quality_reference}"
                                NAME "wavefront denoise quality reference pixels")
rendercli_run(
  NAME "rendercli renders raw low-spp wavefront denoise comparison"
  COMMAND
    "${RENDERCLI}" --engine wavefront --integrator pathtracer --wavefront_denoiser none
    --sampling_seed 1337 --samples_per_pixel 4 --width 32 --height 32
    "${wavefront_denoise_scene}" "${wavefront_denoise_quality_raw}"
)
rendercli_assert_image_nonempty("${wavefront_denoise_quality_raw}"
                                NAME "wavefront denoise quality raw pixels")
rendercli_run(
  NAME "rendercli renders filtered low-spp wavefront denoise comparison"
  COMMAND
    "${RENDERCLI}" --engine wavefront --integrator pathtracer --wavefront_denoiser bilateral
    --wavefront_denoise_radius 3 --wavefront_denoise_color_sigma 0.25
    --sampling_seed 1337 --samples_per_pixel 4 --width 32 --height 32
    "${wavefront_denoise_scene}" "${wavefront_denoise_quality_filtered}"
)
rendercli_assert_image_nonempty("${wavefront_denoise_quality_filtered}"
                                NAME "wavefront denoise quality filtered pixels")
rendercli_compare_images("${wavefront_denoise_quality_reference}"
                         "${wavefront_denoise_quality_raw}"
                         NAME "wavefront raw low-spp RMS against reference"
                         RMS_DELTA_VARIABLE wavefront_denoise_raw_rms
                         OUTPUT_VARIABLE wavefront_denoise_raw_compare)
rendercli_compare_images("${wavefront_denoise_quality_reference}"
                         "${wavefront_denoise_quality_filtered}"
                         NAME "wavefront filtered low-spp RMS against reference"
                         RMS_DELTA_VARIABLE wavefront_denoise_filtered_rms
                         OUTPUT_VARIABLE wavefront_denoise_filtered_compare)
if(wavefront_denoise_filtered_rms GREATER 0.03)
  _rendercli_fail("wavefront bilateral denoise quality threshold"
                  "expected filtered RMS at most 0.03, got ${wavefront_denoise_filtered_rms}"
                  "" "" "${wavefront_denoise_filtered_compare}" "")
endif()

rendercli_run(
  NAME "rendercli traces wavefront material compatibility counter"
  COMMAND
    "${RENDERCLI}" --engine wavefront --integrator pathtracer --samples_per_pixel 2
    --width 16 --height 16 --render_graph_trace_out "${wavefront_compatibility_trace}"
    "${static_scene}" "${wavefront_compatibility_trace_render}"
)
rendercli_assert_image_nonempty("${wavefront_compatibility_trace_render}"
                                NAME "wavefront compatibility trace render pixels")
rendercli_assert_nonempty("${wavefront_compatibility_trace}"
                          NAME "wavefront compatibility trace JSON")
file(READ "${wavefront_compatibility_trace}" wavefront_compatibility_trace_json)
if(NOT wavefront_compatibility_trace_json MATCHES "\"id\": \"wavefront_beauty\"")
  message(FATAL_ERROR
          "wavefront compatibility trace did not contain wavefront pass: ${wavefront_compatibility_trace_json}")
endif()
if(NOT wavefront_compatibility_trace_json MATCHES "\"compatibilityShadeSamples\"")
  message(FATAL_ERROR
          "wavefront compatibility trace did not publish material compatibility counter: ${wavefront_compatibility_trace_json}")
endif()
if(NOT wavefront_compatibility_trace_json MATCHES "\"unsupportedPathMaterialSamples\"")
  message(FATAL_ERROR
          "wavefront compatibility trace did not publish unsupported path material counter: ${wavefront_compatibility_trace_json}")
endif()

rendercli_run(
  NAME "rendercli traces transparent wavefront path without compatibility shading"
  COMMAND
    "${RENDERCLI}" --engine wavefront --integrator pathtracer --samples_per_pixel 2
    --width 16 --height 16 --render_graph_trace_out "${wavefront_glass_trace}"
    "${PROJECT_SOURCE_DIR}/scenes/glass_torus.json" "${wavefront_glass_trace_render}"
)
rendercli_assert_image_nonempty("${wavefront_glass_trace_render}"
                                NAME "wavefront transparent glass trace render pixels")
rendercli_assert_nonempty("${wavefront_glass_trace}" NAME "wavefront transparent glass trace JSON")
file(READ "${wavefront_glass_trace}" wavefront_glass_trace_json)
if(NOT wavefront_glass_trace_json MATCHES "\"id\": \"wavefront_beauty\"")
  message(FATAL_ERROR
          "wavefront transparent glass trace did not contain wavefront pass: ${wavefront_glass_trace_json}")
endif()
if(NOT wavefront_glass_trace_json MATCHES "\"compatibilityShadeSamples\"[ \r\n]*:[ \r\n]*0")
  message(FATAL_ERROR
          "wavefront transparent glass path used compatibility material shading: ${wavefront_glass_trace_json}")
endif()
if(NOT wavefront_glass_trace_json MATCHES "\"unsupportedPathMaterialSamples\"[ \r\n]*:[ \r\n]*0")
  message(FATAL_ERROR
          "wavefront transparent glass path hit unsupported path materials: ${wavefront_glass_trace_json}")
endif()

rendercli_expect_failure(
  NAME "rendercli rejects invalid render graph format"
  STDERR_MATCHES "Render graph format must be"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format yaml
    "${static_scene}" "${invalid_plan}"
)

set(pathtracer_executor_plan "${TEST_OUTPUT_DIR}/pathtracer-executor-graph.json")
rendercli_run(
  NAME "rendercli exports pathtracer executor as wavefront graph pass"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format json --render_graph_executor pathtracer
    "${static_scene}" "${pathtracer_executor_plan}"
)
rendercli_assert_nonempty("${pathtracer_executor_plan}" NAME "pathtracer executor graph output")
file(READ "${pathtracer_executor_plan}" pathtracer_executor_graph)
if(NOT pathtracer_executor_graph MATCHES "\"name\": \"Path traced beauty\"")
  _rendercli_fail(
    "pathtracer executor graph name"
    "expected pathtracer executor graph to contain a path traced beauty pass"
    "" "" "${pathtracer_executor_graph}" "")
endif()
if(NOT pathtracer_executor_graph MATCHES "\"executor\": \"wavefront\"")
  _rendercli_fail(
    "pathtracer executor graph backend"
    "expected pathtracer executor graph to compile to the wavefront executor"
    "" "" "${pathtracer_executor_graph}" "")
endif()
if(NOT pathtracer_executor_graph MATCHES "\"integrator\": \"pathtracer\"")
  _rendercli_fail(
    "pathtracer executor graph state"
    "expected pathtracer executor graph to force pathtracer integrator state"
    "" "" "${pathtracer_executor_graph}" "")
endif()

rendercli_expect_failure(
  NAME "rendercli rejects invalid render graph executor"
  STDERR_MATCHES "Render graph executor must be"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_executor buckets
    "${static_scene}" "${invalid_plan}"
)

rendercli_expect_failure(
  NAME "rendercli rejects conflicting wavefront convergence switches"
  STDERR_MATCHES "Cannot combine --wavefront_convergence"
  COMMAND
    "${RENDERCLI}" --engine wavefront --wavefront_convergence --wavefront_no_convergence
    "${static_scene}" "${invalid_plan}"
)

rendercli_expect_failure(
  NAME "rendercli rejects invalid wavefront convergence active fraction"
  STDERR_MATCHES "Wavefront convergence active fraction must be"
  COMMAND
    "${RENDERCLI}" --engine wavefront --wavefront_convergence_active_fraction 1.5
    "${static_scene}" "${invalid_plan}"
)

rendercli_expect_failure(
  NAME "rendercli rejects invalid pathtracer Russian roulette depth"
  STDERR_MATCHES "Path tracer Russian roulette depth must be"
  COMMAND
    "${RENDERCLI}" --engine pathtracer --pathtracer_russian_roulette_depth 0
    "${static_scene}" "${invalid_plan}"
)

rendercli_expect_failure(
  NAME "rendercli rejects invalid pathtracer direct light samples"
  STDERR_MATCHES "Path tracer direct light samples must be"
  COMMAND
    "${RENDERCLI}" --engine pathtracer --pathtracer_direct_light_samples 0
    "${static_scene}" "${invalid_plan}"
)

rendercli_expect_failure(
  NAME "rendercli rejects invalid wavefront convergence RMS delta"
  STDERR_MATCHES "Wavefront convergence RMS delta must be"
  COMMAND
    "${RENDERCLI}" --engine wavefront --wavefront_convergence_rms_delta -0.1
    "${static_scene}" "${invalid_plan}"
)

rendercli_expect_failure(
  NAME "rendercli rejects conflicting wavefront adaptive sampling switches"
  STDERR_MATCHES "Cannot combine --wavefront_adaptive_sampling"
  COMMAND
    "${RENDERCLI}" --engine wavefront --wavefront_adaptive_sampling
    --wavefront_no_adaptive_sampling "${static_scene}" "${invalid_plan}"
)

rendercli_expect_failure(
  NAME "rendercli rejects invalid wavefront adaptive minimum samples"
  STDERR_MATCHES "Wavefront adaptive minimum samples must be"
  COMMAND
    "${RENDERCLI}" --engine wavefront --wavefront_adaptive_min_samples 0
    "${static_scene}" "${invalid_plan}"
)

rendercli_expect_failure(
  NAME "rendercli rejects invalid wavefront adaptive stddev threshold"
  STDERR_MATCHES "Wavefront adaptive standard-deviation threshold must be"
  COMMAND
    "${RENDERCLI}" --engine wavefront --wavefront_adaptive_stddev_threshold -0.1
    "${static_scene}" "${invalid_plan}"
)

rendercli_expect_failure(
  NAME "rendercli rejects invalid wavefront denoiser"
  STDERR_MATCHES "Wavefront denoiser must be"
  COMMAND
    "${RENDERCLI}" --engine wavefront --wavefront_denoiser mystery
    "${static_scene}" "${invalid_plan}"
)

rendercli_expect_failure(
  NAME "rendercli rejects invalid wavefront denoise radius"
  STDERR_MATCHES "Wavefront denoise radius must be"
  COMMAND
    "${RENDERCLI}" --engine wavefront --wavefront_denoise_radius -1
    "${static_scene}" "${invalid_plan}"
)

rendercli_expect_failure(
  NAME "rendercli rejects invalid wavefront denoise color sigma"
  STDERR_MATCHES "Wavefront denoise color sigma must be"
  COMMAND
    "${RENDERCLI}" --engine wavefront --wavefront_denoise_color_sigma 0
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
  NAME "rendercli rejects duplicate selector-specific graph overrides"
  STDERR_MATCHES "conflicting selector-specific render intent"
  COMMAND
    "${RENDERCLI}" --render_graph_only
    --render_graph_view_override "object_id:12,view=wireframe"
    --render_graph_view_override "object_id:12,executor=wireframe"
    "${static_scene}" "${invalid_plan}"
)

rendercli_expect_failure(
  NAME "rendercli rejects unsupported selector-specific stencil composite view"
  STDERR_MATCHES "does not support selector-specific stencil_composite view"
  COMMAND
    "${RENDERCLI}" --render_graph_only
    --render_graph_view_override "object_id:12,view=stencil_composite"
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
  STDERR_MATCHES "cannot resolve scene selector object_name: Monitor"
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
if(NOT subview_graph MATCHES "\"reads\": \\[[^]]*\"subview_mirror_probe_main_color\"")
  message(FATAL_ERROR "scene subview receiver did not read the subview color output: ${subview_graph}")
endif()
if(NOT subview_graph MATCHES "\"reads\": \\[[^]]*\"subview_mirror_probe_depth_aov\"")
  message(FATAL_ERROR "scene subview receiver did not read the subview depth output: ${subview_graph}")
endif()
if(NOT subview_graph MATCHES "render_to_texture")
  message(FATAL_ERROR "scene subview intent did not mark render-to-texture features: ${subview_graph}")
endif()

rendercli_run(
  NAME "rendercli exports hybrid render-to-texture screen graph"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format json
    --width 48 --height 32
    "${render_texture_screen_demo_scene}" "${render_texture_screen_plan}"
)
rendercli_assert_nonempty("${render_texture_screen_plan}"
                          NAME "hybrid render-to-texture screen graph output")
file(READ "${render_texture_screen_plan}" render_texture_screen_graph)
if(NOT render_texture_screen_graph MATCHES "subview_monitor_feed_raster_beauty")
  message(FATAL_ERROR
          "hybrid screen demo did not compile the monitor-feed raster branch: ${render_texture_screen_graph}")
endif()
if(NOT render_texture_screen_graph MATCHES "subview_monitor_feed_main_color")
  message(FATAL_ERROR
          "hybrid screen demo did not export monitor-feed color: ${render_texture_screen_graph}")
endif()
if(NOT render_texture_screen_graph MATCHES "subview_monitor_feed_depth_aov")
  message(FATAL_ERROR
          "hybrid screen demo did not export monitor-feed depth: ${render_texture_screen_graph}")
endif()
if(NOT render_texture_screen_graph MATCHES "\"reads\": \\[[^]]*\"subview_monitor_feed_main_color\"")
  message(FATAL_ERROR
          "hybrid screen final beauty did not read the monitor-feed color: ${render_texture_screen_graph}")
endif()
if(NOT render_texture_screen_graph MATCHES "\"reads\": \\[[^]]*\"subview_monitor_feed_depth_aov\"")
  message(FATAL_ERROR
          "hybrid screen final beauty did not read the monitor-feed depth: ${render_texture_screen_graph}")
endif()
if(NOT render_texture_screen_graph MATCHES "\"id\": \"raytrace_beauty\"")
  message(FATAL_ERROR
          "hybrid screen demo did not keep the raytraced final beauty pass: ${render_texture_screen_graph}")
endif()
if(NOT render_texture_screen_graph MATCHES "render_to_texture")
  message(FATAL_ERROR
          "hybrid screen demo did not mark render-to-texture features: ${render_texture_screen_graph}")
endif()

rendercli_run(
  NAME "rendercli renders hybrid render-to-texture screen demo"
  COMMAND
    "${RENDERCLI}" --width 48 --height 32
    "${render_texture_screen_demo_scene}" "${render_texture_screen_render}"
)
rendercli_assert_image_dimensions("${render_texture_screen_render}" 48 32
                                  NAME "hybrid render-to-texture screen render dimensions")
rendercli_assert_image_nonempty("${render_texture_screen_render}"
                                NAME "hybrid render-to-texture screen render pixels")

rendercli_run(
  NAME "rendercli traces hybrid render-to-texture screen resources"
  COMMAND
    "${RENDERCLI}" --width 48 --height 32
    --render_graph_trace_out "${render_texture_screen_trace}"
    "${render_texture_screen_demo_scene}" "${render_texture_screen_trace_render}"
)
rendercli_assert_image_dimensions("${render_texture_screen_trace_render}" 48 32
                                  NAME "hybrid render-to-texture trace render dimensions")
rendercli_assert_nonempty("${render_texture_screen_trace}"
                          NAME "hybrid render-to-texture trace JSON")
file(READ "${render_texture_screen_trace}" render_texture_screen_trace_json)
if(NOT render_texture_screen_trace_json MATCHES "subview_monitor_feed_main_color")
  message(FATAL_ERROR
          "hybrid screen trace did not include monitor-feed color output: ${render_texture_screen_trace_json}")
endif()
if(NOT render_texture_screen_trace_json MATCHES "subview_monitor_feed_depth_aov")
  message(FATAL_ERROR
          "hybrid screen trace did not include monitor-feed depth output: ${render_texture_screen_trace_json}")
endif()
if(NOT render_texture_screen_trace_json MATCHES "bound [0-9]+ render-to-texture receiver material")
  message(FATAL_ERROR
          "hybrid screen trace did not record receiver material binding: ${render_texture_screen_trace_json}")
endif()

set(subview_recursion_limit_plan "${TEST_OUTPUT_DIR}/subview-recursion-limit-plan.json")
rendercli_run(
  NAME "rendercli exports subview recursion limit diagnostics"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format json
    "${subview_recursion_limit_scene}" "${subview_recursion_limit_plan}"
)
rendercli_assert_nonempty("${subview_recursion_limit_plan}" NAME "subview recursion limit graph output")
file(READ "${subview_recursion_limit_plan}" subview_recursion_limit_graph)
if(NOT subview_recursion_limit_graph MATCHES "subview_mirror_probe_recursion_limit")
  message(FATAL_ERROR "subview recursion limit did not export a diagnostic pass: ${subview_recursion_limit_graph}")
endif()
if(NOT subview_recursion_limit_graph MATCHES "render_to_texture_recursion_limit")
  message(FATAL_ERROR "subview recursion limit diagnostic did not mark the recursion-limit feature: ${subview_recursion_limit_graph}")
endif()
if(NOT subview_recursion_limit_graph MATCHES "truncated at render-to-texture recursion limit 0")
  message(FATAL_ERROR "subview recursion limit diagnostic did not describe the truncated branch: ${subview_recursion_limit_graph}")
endif()

rendercli_expect_failure(
  NAME "rendercli rejects unsupported selector-specific CLI intent"
  STDERR_MATCHES "cannot resolve scene selector tag: debug"
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

rendercli_expect_failure(
  NAME "rendercli rejects cyclic render-to-texture graph references"
  STDERR_MATCHES "dependency cycle"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_in "${cyclic_graph}"
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
  NAME "rendercli exports graph-synthesized portal demo scene"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format json
    --width 36 --height 24
    "${portal_graph_demo_scene}" "${portal_graph_demo_plan}"
)
rendercli_assert_nonempty("${portal_graph_demo_plan}" NAME "portal demo graph output")
file(READ "${portal_graph_demo_plan}" portal_graph_demo_json)
if(NOT portal_graph_demo_json MATCHES "subview_portal_portal_window_raytrace_beauty")
  message(FATAL_ERROR "portal demo graph did not compile an alternate-camera subview pass: ${portal_graph_demo_json}")
endif()
if(NOT portal_graph_demo_json MATCHES "\"kind\": \"portal\"")
  message(FATAL_ERROR "portal demo graph did not export a derived portal camera: ${portal_graph_demo_json}")
endif()
if(NOT portal_graph_demo_json MATCHES "subview_portal_portal_window_receiver_mask")
  message(FATAL_ERROR "portal demo graph did not compile a receiver mask: ${portal_graph_demo_json}")
endif()
if(NOT portal_graph_demo_json MATCHES "subview_portal_portal_window_composite")
  message(FATAL_ERROR "portal demo graph did not compile a portal composite: ${portal_graph_demo_json}")
endif()
if(NOT portal_graph_demo_json MATCHES "portal_receiver")
  message(FATAL_ERROR "portal demo graph did not tag portal receiver nodes: ${portal_graph_demo_json}")
endif()

rendercli_run(
  NAME "rendercli renders graph-synthesized portal demo scene"
  COMMAND
    "${RENDERCLI}" --width 36 --height 24
    "${portal_graph_demo_scene}" "${portal_graph_demo_render}"
)
rendercli_assert_image_dimensions("${portal_graph_demo_render}" 36 24
                                  NAME "portal graph demo scene dimensions")
rendercli_assert_image_nonempty("${portal_graph_demo_render}"
                                NAME "portal graph demo scene pixels")

rendercli_run(
  NAME "rendercli exports graph-synthesized planar mirror demo scene"
  COMMAND
    "${RENDERCLI}" --render_graph_only --render_graph_format json
    --width 36 --height 24
    "${planar_mirror_graph_demo_scene}" "${planar_mirror_graph_demo_plan}"
)
rendercli_assert_nonempty("${planar_mirror_graph_demo_plan}"
                          NAME "planar mirror demo graph output")
file(READ "${planar_mirror_graph_demo_plan}" planar_mirror_graph_demo_json)
if(NOT planar_mirror_graph_demo_json MATCHES "subview_mirror_mirror_panel_raytrace_beauty")
  message(FATAL_ERROR "planar mirror demo graph did not compile an alternate-camera subview pass: ${planar_mirror_graph_demo_json}")
endif()
if(NOT planar_mirror_graph_demo_json MATCHES "\"kind\": \"planar_mirror\"")
  message(FATAL_ERROR "planar mirror demo graph did not export a derived mirror camera: ${planar_mirror_graph_demo_json}")
endif()
if(NOT planar_mirror_graph_demo_json MATCHES "subview_mirror_mirror_panel_receiver_mask")
  message(FATAL_ERROR "planar mirror demo graph did not compile a receiver mask: ${planar_mirror_graph_demo_json}")
endif()
if(NOT planar_mirror_graph_demo_json MATCHES "subview_mirror_mirror_panel_composite")
  message(FATAL_ERROR "planar mirror demo graph did not compile a mirror composite: ${planar_mirror_graph_demo_json}")
endif()
if(NOT planar_mirror_graph_demo_json MATCHES "mirror_receiver")
  message(FATAL_ERROR "planar mirror demo graph did not tag mirror receiver nodes: ${planar_mirror_graph_demo_json}")
endif()

rendercli_run(
  NAME "rendercli renders graph-synthesized planar mirror demo scene"
  COMMAND
    "${RENDERCLI}" --width 36 --height 24
    "${planar_mirror_graph_demo_scene}" "${planar_mirror_graph_demo_render}"
)
rendercli_assert_image_dimensions("${planar_mirror_graph_demo_render}" 36 24
                                  NAME "planar mirror graph demo scene dimensions")
rendercli_assert_image_nonempty("${planar_mirror_graph_demo_render}"
                                NAME "planar mirror graph demo scene pixels")

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
if(NOT graph_trace_json MATCHES "\"startedAtMs\"")
  message(FATAL_ERROR "graph trace did not contain pass start times: ${graph_trace_json}")
endif()
if(NOT graph_trace_json MATCHES "\"finishedAtMs\"")
  message(FATAL_ERROR "graph trace did not contain pass finish times: ${graph_trace_json}")
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
    --width 32 --height 16 --sampler Jittered --samples_per_pixel 9 --sampling_seed 12345
    --depth 6
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
if(NOT raytracer_state_graph MATCHES "\"seed\"[ \r\n]*:[ \r\n]*12345")
  message(FATAL_ERROR "raytracer state graph did not contain sampling seed: ${raytracer_state_graph}")
endif()
if(NOT raytracer_state_graph MATCHES "maxRecursionDepth")
  message(FATAL_ERROR "raytracer state graph did not contain recursion depth: ${raytracer_state_graph}")
endif()

rendercli_expect_failure(
  NAME "rendercli rejects sampling seed outside exact JSON integer range"
  STDERR_MATCHES "Sampling seed must be a non-negative integer <= 9007199254740991"
  COMMAND
    "${RENDERCLI}" --engine raytracer --render_graph_only --render_graph_format json
    --sampling_seed 9007199254740992
    "${static_scene}" "${invalid_plan}"
)

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
