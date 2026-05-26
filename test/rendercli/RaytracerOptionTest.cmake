if(NOT DEFINED RENDERCLI)
  message(FATAL_ERROR "RENDERCLI is required")
endif()

if(NOT DEFINED TEST_OUTPUT_DIR)
  message(FATAL_ERROR "TEST_OUTPUT_DIR is required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/RendercliTestHelpers.cmake")

file(REMOVE_RECURSE "${TEST_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${TEST_OUTPUT_DIR}")

set(raytracer_scene "${TEST_OUTPUT_DIR}/raytracer-options-scene.json")
set(ldraw_scene "${TEST_OUTPUT_DIR}/ldraw-scene.json")
set(reflective_scene "${PROJECT_SOURCE_DIR}/scenes/reflections.json")
set(invalid_sampler_output "${TEST_OUTPUT_DIR}/invalid-sampler.png")

file(WRITE "${raytracer_scene}" [=[
{
  "id": "rendercli-raytracer-options",
  "name": "Rendercli Raytracer Options",
  "ambient": [0.25, 0.25, 0.25],
  "background": [0.02, 0.04, 0.08],
  "type": "Scene",
  "children": [
    {
      "id": "camera",
      "name": "Camera",
      "position": [0.0, 0.0, -4.0],
      "target": [0.0, 0.0, 0.0],
      "distance": 4.0,
      "zoom": 1.3,
      "type": "PinholeCamera",
      "children": []
    },
    {
      "id": "red",
      "name": "Red",
      "color": [0.95, 0.15, 0.08],
      "type": "ConstantColorTexture",
      "children": []
    },
    {
      "id": "matte",
      "name": "Matte",
      "diffuseTexture": "red",
      "ambientCoefficient": 0.5,
      "diffuseCoefficient": 0.8,
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

file(WRITE "${ldraw_scene}" [=[
{
  "id": "rendercli-ldraw-scene",
  "name": "Rendercli LDraw Scene",
  "ambient": [0.25, 0.25, 0.25],
  "background": [0.02, 0.04, 0.08],
  "type": "Scene",
  "children": [
    {
      "id": "camera",
      "name": "Camera",
      "position": [0.0, 0.0, -4.0],
      "target": [0.0, 0.0, 0.0],
      "distance": 4.0,
      "zoom": 1.0,
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
      "direction": [-0.5, -1.0, -0.5],
      "type": "DirectionalLight",
      "children": []
    },
    {
      "id": "ldraw",
      "name": "LDraw Import",
      "position": [0.0, 0.0, 0.0],
      "rotation": [0.0, 0.0, 0.0],
      "scale": [1.0, 1.0, 1.0],
      "visible": true,
      "metadata": {
        "sourceFormat": "LDraw",
        "sourcePath": "%%PROJECT_SOURCE_DIR%%/test/fixtures/ldraw/rendercli/model.ldr",
        "normalMode": "flat"
      },
      "type": "Collection",
      "children": []
    }
  ]
}
]=])
file(READ "${ldraw_scene}" ldraw_scene_json)
string(REPLACE "%%PROJECT_SOURCE_DIR%%" "${PROJECT_SOURCE_DIR}" ldraw_scene_json "${ldraw_scene_json}")
file(WRITE "${ldraw_scene}" "${ldraw_scene_json}")

foreach(depth IN ITEMS 1 4)
  set(output "${TEST_OUTPUT_DIR}/depth-${depth}.png")
  rendercli_run(
    NAME "rendercli raytracer accepts --depth ${depth}"
    COMMAND
      "${RENDERCLI}" --direct_engine --engine raytracer --width 24 --height 24
      --depth "${depth}" "${reflective_scene}" "${output}"
  )
  rendercli_assert_image_dimensions("${output}" 24 24
                                    NAME "raytracer --depth ${depth} dimensions")
  rendercli_assert_image_nonempty("${output}"
                                  NAME "raytracer --depth ${depth} pixels")
endforeach()

foreach(sampler IN ITEMS Regular Random Jittered)
  set(output "${TEST_OUTPUT_DIR}/sampler-${sampler}.png")
  rendercli_run(
    NAME "rendercli raytracer accepts --sampler ${sampler}"
    COMMAND
      "${RENDERCLI}" --direct_engine --engine raytracer --width 24 --height 24
      --sampler "${sampler}" --samples_per_pixel 4
      "${raytracer_scene}" "${output}"
  )
  rendercli_assert_image_dimensions("${output}" 24 24
                                    NAME "raytracer --sampler ${sampler} dimensions")
  rendercli_assert_image_nonempty("${output}"
                                  NAME "raytracer --sampler ${sampler} pixels")
endforeach()

set(samples_output "${TEST_OUTPUT_DIR}/samples-per-pixel.png")
rendercli_run(
  NAME "rendercli raytracer accepts --samples_per_pixel"
  COMMAND
    "${RENDERCLI}" --direct_engine --engine raytracer --width 24 --height 24
    --sampler Regular --samples_per_pixel 9
    "${raytracer_scene}" "${samples_output}"
)
rendercli_assert_image_dimensions("${samples_output}" 24 24
                                  NAME "raytracer --samples_per_pixel dimensions")
rendercli_assert_image_nonempty("${samples_output}"
                                NAME "raytracer --samples_per_pixel pixels")

set(ldraw_scene_output "${TEST_OUTPUT_DIR}/ldraw-scene.png")
rendercli_run(
  NAME "rendercli raytracer renders LDraw authoring import with --ldraw_library_root"
  COMMAND
    "${RENDERCLI}" --direct_engine --engine raytracer --width 24 --height 24
    --ldraw_library_root "${PROJECT_SOURCE_DIR}/test/fixtures/ldraw/rendercli/library"
    "${ldraw_scene}" "${ldraw_scene_output}"
)
rendercli_assert_image_dimensions("${ldraw_scene_output}" 24 24
                                  NAME "LDraw authoring import dimensions")
rendercli_assert_image_nonempty("${ldraw_scene_output}"
                                NAME "LDraw authoring import pixels")

set(ldraw_direct_output "${TEST_OUTPUT_DIR}/ldraw-direct-hierarchy.png")
rendercli_run(
  NAME "rendercli raytracer renders direct LDraw input with preserved hierarchy"
  COMMAND
    "${RENDERCLI}" --direct_engine --engine raytracer --width 24 --height 24
    --ldraw_input
    --ldraw_preserve_hierarchy
    --ldraw_library_root "${PROJECT_SOURCE_DIR}/test/fixtures/ldraw/rendercli/library"
    --ldraw_scale 0.5
    --ldraw_coordinate_conversion none
    --ldraw_flatten_hierarchy
    --ldraw_normals smooth
    --ldraw_no_edge_overlays
    --ldraw_max_recursion 8
    --ldraw_missing_part_policy skip
    "${PROJECT_SOURCE_DIR}/test/fixtures/ldraw/rendercli/model.ldr"
    "${ldraw_direct_output}"
)
rendercli_assert_image_dimensions("${ldraw_direct_output}" 24 24
                                  NAME "direct LDraw hierarchy dimensions")
rendercli_assert_image_nonempty("${ldraw_direct_output}"
                                NAME "direct LDraw hierarchy pixels")

set(ldraw_direct_mpd_output "${TEST_OUTPUT_DIR}/ldraw-direct-mpd.png")
rendercli_run(
  NAME "rendercli raytracer renders direct LDraw MPD input"
  COMMAND
    "${RENDERCLI}" --direct_engine --engine raytracer --width 24 --height 24
    --ldraw_input
    --ldraw_library_root "${PROJECT_SOURCE_DIR}/test/fixtures/ldraw/smoke/library"
    "${PROJECT_SOURCE_DIR}/test/fixtures/ldraw/smoke/model.mpd"
    "${ldraw_direct_mpd_output}"
)
rendercli_assert_image_dimensions("${ldraw_direct_mpd_output}" 24 24
                                  NAME "direct LDraw MPD dimensions")
rendercli_assert_image_nonempty("${ldraw_direct_mpd_output}"
                                NAME "direct LDraw MPD pixels")

rendercli_expect_failure(
  NAME "rendercli validates direct LDraw import options"
  STDERR_MATCHES "LDraw missing part policy must be 'error' or 'skip'"
  COMMAND
    "${RENDERCLI}" --direct_engine --engine raytracer --width 24 --height 24
    --ldraw_input --ldraw_missing_part_policy maybe
    "${PROJECT_SOURCE_DIR}/test/fixtures/ldraw/rendercli/model.ldr"
    "${TEST_OUTPUT_DIR}/ldraw-invalid.png"
)

set(threaded_output "${TEST_OUTPUT_DIR}/threads-and-queue.png")
rendercli_run(
  NAME "rendercli raytracer accepts --threads and --queue_size"
  COMMAND
    "${RENDERCLI}" --direct_engine --engine raytracer --width 24 --height 24
    --threads 2 --queue_size 3
    "${raytracer_scene}" "${threaded_output}"
)
rendercli_assert_image_dimensions("${threaded_output}" 24 24
                                  NAME "raytracer --threads --queue_size dimensions")
rendercli_assert_image_nonempty("${threaded_output}"
                                NAME "raytracer --threads --queue_size pixels")

rendercli_expect_failure(
  NAME "rendercli rejects unknown raytracer sampler"
  STDERR_MATCHES "Sampler must be 'Regular', 'Random', or 'Jittered'"
  COMMAND
    "${RENDERCLI}" --direct_engine --engine raytracer --width 16 --height 16
    --sampler Stratified "${raytracer_scene}" "${invalid_sampler_output}"
)
rendercli_assert_not_exists("${invalid_sampler_output}" NAME "invalid sampler output")
