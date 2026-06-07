if(NOT DEFINED RENDERCLI)
  message(FATAL_ERROR "RENDERCLI is required")
endif()

if(NOT DEFINED TEST_OUTPUT_DIR)
  message(FATAL_ERROR "TEST_OUTPUT_DIR is required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/RendercliTestHelpers.cmake")

file(REMOVE_RECURSE "${TEST_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${TEST_OUTPUT_DIR}")

set(basic_scene "${TEST_OUTPUT_DIR}/basic-scene.json")
set(missing_scene "${TEST_OUTPUT_DIR}/missing-scene.json")
set(output_dir "${TEST_OUTPUT_DIR}/output-directory")
set(valid_output "${TEST_OUTPUT_DIR}/valid.png")

file(MAKE_DIRECTORY "${output_dir}")

file(WRITE "${basic_scene}" [=[
{
  "id": "{93000000-0000-0000-0000-000000000000}",
  "name": "Rendercli Validation Fixture",
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

function(rendercli_expect_parser_failure name expected_stderr)
  rendercli_expect_failure(
    NAME "${name}"
    STDERR_MATCHES "${expected_stderr}"
    COMMAND
      "${RENDERCLI}" ${ARGN}
  )
endfunction()

rendercli_expect_parser_failure(
  "rendercli rejects missing positional args"
  "Need input and output filename"
)

rendercli_expect_parser_failure(
  "rendercli rejects missing output"
  "Need input and output filename"
  "${basic_scene}"
)

rendercli_expect_parser_failure(
  "rendercli rejects invalid engine"
  "Engine must be 'raytracer', 'pathtracer', 'wavefront', 'wireframe', or 'raster'"
  --engine invalid
  "${basic_scene}" "${valid_output}"
)

rendercli_expect_parser_failure(
  "rendercli rejects invalid width"
  "Width must be > 0"
  --width 0
  "${basic_scene}" "${valid_output}"
)

rendercli_expect_parser_failure(
  "rendercli rejects invalid height"
  "Height must be > 0"
  --height 0
  "${basic_scene}" "${valid_output}"
)

rendercli_expect_parser_failure(
  "rendercli rejects invalid depth"
  "Depth must be > 0"
  --depth 0
  "${basic_scene}" "${valid_output}"
)

rendercli_expect_parser_failure(
  "rendercli rejects invalid samples"
  "Samples per pixel must be > 0"
  --samples_per_pixel 0
  "${basic_scene}" "${valid_output}"
)

rendercli_expect_parser_failure(
  "rendercli rejects invalid threads"
  "Threads must be > 0"
  --threads 0
  "${basic_scene}" "${valid_output}"
)

rendercli_expect_parser_failure(
  "rendercli rejects invalid queue size"
  "Queue size must be > 0"
  --queue_size 0
  "${basic_scene}" "${valid_output}"
)

rendercli_expect_parser_failure(
  "rendercli rejects invalid raster tessellation quality"
  "Raster tessellation quality must be 'preview', 'balanced', or 'final'"
  --raster_tessellation_quality draft
  "${basic_scene}" "${valid_output}"
)

rendercli_expect_parser_failure(
  "rendercli rejects invalid raster max screen-space error"
  "Raster max screen-space error must be a non-negative number"
  --raster_max_screen_space_error -1
  "${basic_scene}" "${valid_output}"
)

rendercli_expect_parser_failure(
  "rendercli rejects invalid repeat count"
  "Repeat must be a positive integer"
  --repeat 0
  "${basic_scene}" "${valid_output}"
)

rendercli_expect_failure(
  NAME "rendercli rejects unreadable input"
  STDERR_MATCHES "Unable to load input scene"
  COMMAND
    "${RENDERCLI}" --width 8 --height 8
    "${missing_scene}" "${valid_output}"
)

rendercli_expect_failure(
  NAME "rendercli rejects unwritable output"
  STDERR_MATCHES "Unable to write output image"
  COMMAND
    "${RENDERCLI}" --width 8 --height 8
    "${basic_scene}" "${output_dir}"
)
