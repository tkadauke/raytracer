if(NOT DEFINED RENDERCLI)
  message(FATAL_ERROR "RENDERCLI is required")
endif()

if(NOT DEFINED TEST_OUTPUT_DIR)
  message(FATAL_ERROR "TEST_OUTPUT_DIR is required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/RendercliTestHelpers.cmake")

file(REMOVE_RECURSE "${TEST_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${TEST_OUTPUT_DIR}")

set(wireframe_scene "${TEST_OUTPUT_DIR}/wireframe-options-scene.json")
set(invalid_lod_output "${TEST_OUTPUT_DIR}/invalid-lod.png")

file(WRITE "${wireframe_scene}" [=[
{
  "id": "rendercli-wireframe-options",
  "name": "Rendercli Wireframe Options",
  "ambient": [0.2, 0.2, 0.2],
  "background": [0.0, 0.0, 0.0],
  "type": "Scene",
  "children": [
    {
      "id": "camera",
      "name": "Camera",
      "position": [0.0, 0.0, -4.0],
      "target": [0.0, 0.0, 0.0],
      "distance": 4.0,
      "zoom": 1.4,
      "type": "PinholeCamera",
      "children": []
    },
    {
      "id": "white",
      "name": "White",
      "color": [1.0, 1.0, 1.0],
      "type": "ConstantColorTexture",
      "children": []
    },
    {
      "id": "matte",
      "name": "Matte",
      "diffuseTexture": "white",
      "ambientCoefficient": 1.0,
      "diffuseCoefficient": 1.0,
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
      "material": "matte",
      "radius": 1.0,
      "type": "Sphere",
      "children": []
    }
  ]
}
]=])

set(lod0_output "${TEST_OUTPUT_DIR}/lod-0.png")
set(lod1_output "${TEST_OUTPUT_DIR}/lod-1.png")
set(lod3_output "${TEST_OUTPUT_DIR}/lod-3.png")

foreach(lod IN ITEMS 0 1 3)
  set(output "${TEST_OUTPUT_DIR}/lod-${lod}.png")
  rendercli_run(
    NAME "rendercli wireframe accepts --lod ${lod}"
    COMMAND
      "${RENDERCLI}" --engine wireframe --width 64 --height 64
      --lod "${lod}" "${wireframe_scene}" "${output}"
  )
  rendercli_assert_image_dimensions("${output}" 64 64
                                    NAME "wireframe --lod ${lod} dimensions")
  rendercli_assert_image_nonempty("${output}"
                                  NAME "wireframe --lod ${lod} pixels")
endforeach()

rendercli_assert_image_hash_differs("${lod0_output}" "${lod1_output}"
                                    NAME "wireframe lod 0 and 1 differ")
rendercli_assert_image_hash_differs("${lod1_output}" "${lod3_output}"
                                    NAME "wireframe lod 1 and 3 differ")

rendercli_expect_failure(
  NAME "rendercli rejects non-integer --lod"
  STDERR_MATCHES "LOD must be a non-negative integer"
  COMMAND
    "${RENDERCLI}" --engine wireframe --width 16 --height 16
    --lod not-an-integer "${wireframe_scene}" "${invalid_lod_output}"
)
rendercli_assert_not_exists("${invalid_lod_output}" NAME "non-integer lod output")

rendercli_expect_failure(
  NAME "rendercli rejects negative --lod"
  STDERR_MATCHES "LOD must be a non-negative integer"
  COMMAND
    "${RENDERCLI}" --engine wireframe --width 16 --height 16
    --lod -1 "${wireframe_scene}" "${invalid_lod_output}"
)
rendercli_assert_not_exists("${invalid_lod_output}" NAME "negative lod output")
