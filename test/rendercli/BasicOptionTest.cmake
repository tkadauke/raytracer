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
set(default_render "${TEST_OUTPUT_DIR}/default.png")
set(sized_render "${TEST_OUTPUT_DIR}/sized.png")
set(timing_render "${TEST_OUTPUT_DIR}/timing.png")
set(repeat_render "${TEST_OUTPUT_DIR}/repeat.png")

file(WRITE "${basic_scene}" [=[
{
  "id": "{92000000-0000-0000-0000-000000000000}",
  "name": "Rendercli Basic Fixture",
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

rendercli_run(
  NAME "rendercli renders default input and output"
  COMMAND
    "${RENDERCLI}" "${basic_scene}" "${default_render}"
)
rendercli_assert_image_nonempty("${default_render}"
                                NAME "default render contains pixels")

foreach(engine IN ITEMS raytracer pathtracer wavefront raster wireframe)
  set(engine_render "${TEST_OUTPUT_DIR}/engine-${engine}.png")
  rendercli_run(
    NAME "rendercli --engine ${engine} renders"
    COMMAND
      "${RENDERCLI}" --engine "${engine}" --width 24 --height 16
      "${basic_scene}" "${engine_render}"
  )
  rendercli_assert_image_dimensions("${engine_render}" 24 16
                                    NAME "rendercli --engine ${engine} dimensions")
  rendercli_assert_image_nonempty("${engine_render}"
                                  NAME "rendercli --engine ${engine} pixels")
endforeach()

rendercli_run(
  NAME "rendercli --width and --height render requested dimensions"
  COMMAND
    "${RENDERCLI}" --width 37 --height 23
    "${basic_scene}" "${sized_render}"
)
rendercli_assert_image_dimensions("${sized_render}" 37 23
                                  NAME "rendercli --width/--height dimensions")
rendercli_assert_image_nonempty("${sized_render}"
                                NAME "rendercli --width/--height pixels")

foreach(tonemap IN ITEMS Linear Reinhard ACES)
  set(tonemap_render "${TEST_OUTPUT_DIR}/tonemap-${tonemap}.png")
  rendercli_run(
    NAME "rendercli --tonemap ${tonemap} renders"
    COMMAND
      "${RENDERCLI}" --width 16 --height 16 --tonemap "${tonemap}"
      "${basic_scene}" "${tonemap_render}"
  )
  rendercli_assert_image_dimensions("${tonemap_render}" 16 16
                                    NAME "rendercli --tonemap ${tonemap} dimensions")
  rendercli_assert_image_nonempty("${tonemap_render}"
                                  NAME "rendercli --tonemap ${tonemap} pixels")
endforeach()

rendercli_run(
  NAME "rendercli --timing prints render timing"
  OUTPUT_VARIABLE timing_stdout
  STDOUT_MATCHES "render_ms runs=1"
  COMMAND
    "${RENDERCLI}" --width 16 --height 16 --timing
    "${basic_scene}" "${timing_render}"
)
string(REGEX MATCHALL "render_ms runs=1" timing_lines "${timing_stdout}")
list(LENGTH timing_lines timing_line_count)
if(NOT timing_line_count EQUAL 1)
  _rendercli_fail("rendercli --timing prints one timing line"
                  "expected exactly one render_ms summary, got ${timing_line_count}"
                  "" "" "${timing_stdout}" "")
endif()
rendercli_assert_image_nonempty("${timing_render}" NAME "rendercli --timing pixels")

rendercli_run(
  NAME "rendercli --repeat 2 prints timing summary"
  OUTPUT_VARIABLE repeat_stdout
  STDOUT_MATCHES "runs=2"
  COMMAND
    "${RENDERCLI}" --width 16 --height 16 --repeat 2
    "${basic_scene}" "${repeat_render}"
)
string(REGEX MATCHALL "render_ms runs=2" repeat_lines "${repeat_stdout}")
list(LENGTH repeat_lines repeat_line_count)
if(NOT repeat_line_count EQUAL 1)
  _rendercli_fail("rendercli --repeat prints one timing line"
                  "expected exactly one render_ms summary, got ${repeat_line_count}"
                  "" "" "${repeat_stdout}" "")
endif()
rendercli_assert_image_nonempty("${repeat_render}" NAME "rendercli --repeat pixels")

rendercli_run(
  NAME "rendercli --help succeeds"
  STDOUT_MATCHES "Usage:" "--width" "--engine"
  COMMAND
    "${RENDERCLI}" --help
)

rendercli_run(
  NAME "rendercli --version succeeds"
  STDOUT_MATCHES "Command line renderer"
  COMMAND
    "${RENDERCLI}" --version
)
