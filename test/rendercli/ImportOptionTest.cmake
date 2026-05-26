if(NOT DEFINED RENDERCLI)
  message(FATAL_ERROR "RENDERCLI is required")
endif()

if(NOT DEFINED TEST_OUTPUT_DIR)
  message(FATAL_ERROR "TEST_OUTPUT_DIR is required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/RendercliTestHelpers.cmake")

file(REMOVE_RECURSE "${TEST_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${TEST_OUTPUT_DIR}")

set(import_scene "${TEST_OUTPUT_DIR}/imported.rtjson")
set(explicit_import_scene "${TEST_OUTPUT_DIR}/imported.scene")
set(extension_render "${TEST_OUTPUT_DIR}/extension.png")
set(explicit_render "${TEST_OUTPUT_DIR}/explicit.png")
set(missing_render "${TEST_OUTPUT_DIR}/missing.png")

set(scene_json [=[
{
  "id": "import-scene",
  "name": "Rendercli Import Fixture",
  "ambient": [0.4, 0.4, 0.4],
  "background": [0.4, 0.8, 1.0],
  "type": "Scene",
  "children": [
    {
      "id": "camera",
      "name": "Camera",
      "position": [0.0, 0.0, -3.0],
      "target": [0.0, 0.0, 0.0],
      "distance": 5.0,
      "zoom": 1.0,
      "type": "PinholeCamera",
      "children": []
    }
  ]
}
]=])

file(WRITE "${import_scene}" "${scene_json}")
file(WRITE "${explicit_import_scene}" "${scene_json}")

rendercli_run(
  NAME "rendercli imports by registered extension"
  COMMAND
    "${RENDERCLI}" --width 8 --height 8
    "${import_scene}" "${extension_render}"
)
rendercli_assert_image_dimensions("${extension_render}" 8 8
                                  NAME "rendercli import extension dimensions")
rendercli_assert_image_nonempty("${extension_render}"
                                NAME "rendercli import extension pixels")

rendercli_run(
  NAME "rendercli imports by explicit format"
  COMMAND
    "${RENDERCLI}" --width 8 --height 8 --import_format json --import_option fixture=minimal
    "${explicit_import_scene}" "${explicit_render}"
)
rendercli_assert_image_dimensions("${explicit_render}" 8 8
                                  NAME "rendercli explicit import dimensions")
rendercli_assert_image_nonempty("${explicit_render}"
                                NAME "rendercli explicit import pixels")

rendercli_expect_failure(
  NAME "rendercli reports unknown importer"
  STDERR_MATCHES "No scene importer registered for format: missing"
  COMMAND
    "${RENDERCLI}" --width 8 --height 8 --import_format missing
    "${explicit_import_scene}" "${missing_render}"
)

rendercli_expect_failure(
  NAME "rendercli reports fatal import diagnostics"
  STDERR_MATCHES "import error .*Unable to read import source" "Unable to import input scene"
  COMMAND
    "${RENDERCLI}" --width 8 --height 8 --import_format json
    "${TEST_OUTPUT_DIR}/missing.rtjson" "${missing_render}"
)
