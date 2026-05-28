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
set(openscad_render "${TEST_OUTPUT_DIR}/openscad-source-asset.png")
set(openscad_direct_render "${TEST_OUTPUT_DIR}/openscad-direct.png")
set(openscad_real_render "${TEST_OUTPUT_DIR}/openscad-real-source-asset.png")
set(missing_render "${TEST_OUTPUT_DIR}/missing.png")
set(fake_openscad "${TEST_OUTPUT_DIR}/openscad-fake.sh")
set(openscad_scene "${TEST_OUTPUT_DIR}/openscad-source-asset.rtjson")
set(openscad_real_scene "${TEST_OUTPUT_DIR}/openscad-real-source-asset.rtjson")
set(openscad_source "${TEST_OUTPUT_DIR}/compiler_smoke.scad")
set(openscad_cache "${TEST_OUTPUT_DIR}/openscad-cache")
set(openscad_real_cache "${TEST_OUTPUT_DIR}/openscad-real-cache")
set(openscad_fixture_dir "${PROJECT_SOURCE_DIR}/test/fixtures/openscad")
set(openscad_external_fixture "${openscad_fixture_dir}/external-compiler/compiler_smoke.scad")
set(stl_fixture "${PROJECT_SOURCE_DIR}/test/fixtures/additive/wedge.stl")
set(threemf_fixture "${PROJECT_SOURCE_DIR}/test/fixtures/additive/wedge.3mf")
set(gcode_fixture "${PROJECT_SOURCE_DIR}/test/fixtures/additive/two_layer_path.gcode")
set(gltf_scene "${TEST_OUTPUT_DIR}/triangle.gltf")
set(stl_render "${TEST_OUTPUT_DIR}/stl-model.png")
set(threemf_render "${TEST_OUTPUT_DIR}/3mf-model.png")
set(gltf_render "${TEST_OUTPUT_DIR}/gltf-model.png")
set(gcode_speed_render "${TEST_OUTPUT_DIR}/gcode-speed.png")
set(gcode_tool_layer_render "${TEST_OUTPUT_DIR}/gcode-tool-layer.png")
set(gcode_cumulative_render "${TEST_OUTPUT_DIR}/gcode-cumulative.png")

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
configure_file("${openscad_external_fixture}" "${openscad_source}" COPYONLY)
file(WRITE "${fake_openscad}" [=[
#!/bin/sh
out=""
while [ "$#" -gt 0 ]; do
  if [ "$1" = "-o" ]; then
    shift
    out="$1"
  fi
  shift
done
cat > "$out" <<'STL'
solid openscad
  facet normal 0 0 1
    outer loop
      vertex -0.8 -0.8 0
      vertex 0.8 -0.8 0
      vertex 0.0 0.8 0
    endloop
  endfacet
endsolid openscad
STL
exit 0
]=])
execute_process(COMMAND "${CMAKE_COMMAND}" -E chmod +x "${fake_openscad}")

set(openscad_scene_json [=[
{
  "id": "openscad-source-asset-scene",
  "name": "OpenSCAD SourceAsset Render Fixture",
  "ambient": [0.4, 0.4, 0.4],
  "background": [0.02, 0.04, 0.08],
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
    },
    {
      "id": "light",
      "name": "Light",
      "direction": [-0.2, -0.4, -1.0],
      "color": [1.0, 1.0, 1.0],
      "intensity": 1.0,
      "visible": true,
      "type": "DirectionalLight",
      "children": []
    },
    {
      "id": "openscad-source",
      "name": "OpenSCAD Source",
      "sourcePath": "compiler_smoke.scad",
      "format": "openscad",
      "importOptions": {
        "executable": "__OPENSCAD_EXECUTABLE__",
        "cacheDirectory": "__OPENSCAD_CACHE__",
        "outputFormat": "stl"
      },
      "type": "SourceAsset",
      "children": []
    }
  ]
}
]=])
string(REPLACE "__OPENSCAD_EXECUTABLE__" "${fake_openscad}" openscad_scene_json "${openscad_scene_json}")
string(REPLACE "__OPENSCAD_CACHE__" "${openscad_cache}" openscad_scene_json "${openscad_scene_json}")
file(WRITE "${openscad_scene}" "${openscad_scene_json}")

file(WRITE "${gltf_scene}" [=[
{
  "asset": {"version": "2.0", "generator": "raytracer rendercli smoke"},
  "buffers": [{
    "uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAAAAAAAAgD8AAAAAAACAPwAAAAAAAAAA",
    "byteLength": 36
  }],
  "bufferViews": [{"buffer": 0, "byteLength": 36}],
  "accessors": [{"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3"}],
  "materials": [{
    "pbrMetallicRoughness": {"baseColorFactor": [0.9, 0.1, 0.05, 1.0]}
  }],
  "meshes": [{
    "name": "Triangle",
    "primitives": [{"attributes": {"POSITION": 0}, "material": 0}]
  }],
  "scenes": [{"name": "Triangle Scene", "nodes": [0]}],
  "nodes": [{"name": "Triangle Node", "mesh": 0}]
}
]=])

find_program(REAL_OPENSCAD_EXECUTABLE openscad)
if(REAL_OPENSCAD_EXECUTABLE)
  set(openscad_real_scene_json "${openscad_scene_json}")
  string(REPLACE "${fake_openscad}" "${REAL_OPENSCAD_EXECUTABLE}" openscad_real_scene_json
         "${openscad_real_scene_json}")
  string(REPLACE "${openscad_cache}" "${openscad_real_cache}" openscad_real_scene_json
         "${openscad_real_scene_json}")
  file(WRITE "${openscad_real_scene}" "${openscad_real_scene_json}")
endif()

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

rendercli_run(
  NAME "rendercli renders OpenSCAD SourceAsset mesh"
  COMMAND
    "${RENDERCLI}" --width 8 --height 8 --import_format json
    "${openscad_scene}" "${openscad_render}"
)
rendercli_assert_image_dimensions("${openscad_render}" 8 8
                                  NAME "rendercli OpenSCAD source asset dimensions")
rendercli_assert_image_nonempty("${openscad_render}"
                                NAME "rendercli OpenSCAD source asset pixels")

rendercli_run(
  NAME "rendercli renders direct OpenSCAD import as product-view scene"
  COMMAND
    "${RENDERCLI}" --width 8 --height 8
    --import_option "executable=${fake_openscad}"
    --import_option "cacheDirectory=${openscad_cache}"
    --import_option "outputFormat=stl"
    "${openscad_source}" "${openscad_direct_render}"
)
rendercli_assert_image_dimensions("${openscad_direct_render}" 8 8
                                  NAME "rendercli direct OpenSCAD dimensions")
rendercli_assert_image_nonempty("${openscad_direct_render}"
                                NAME "rendercli direct OpenSCAD pixels")

rendercli_run(
  NAME "rendercli renders direct glTF import as product-view scene"
  COMMAND
    "${RENDERCLI}" --width 32 --height 32 --import_option "background_color=black"
    "${gltf_scene}" "${gltf_render}"
)
rendercli_assert_image_dimensions("${gltf_render}" 32 32
                                  NAME "rendercli direct glTF dimensions")
rendercli_assert_image_varied("${gltf_render}"
                              NAME "rendercli direct glTF varied pixels")

if(REAL_OPENSCAD_EXECUTABLE)
  rendercli_run(
    NAME "rendercli renders OpenSCAD fixture with external compiler"
    COMMAND
      "${RENDERCLI}" --width 8 --height 8 --import_format json
      "${openscad_real_scene}" "${openscad_real_render}"
  )
  rendercli_assert_image_dimensions("${openscad_real_render}" 8 8
                                    NAME "rendercli real OpenSCAD fixture dimensions")
  rendercli_assert_image_nonempty("${openscad_real_render}"
                                  NAME "rendercli real OpenSCAD fixture pixels")
else()
  message(STATUS "Skipping real OpenSCAD render smoke: openscad executable was not found")
endif()

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

rendercli_run(
  NAME "rendercli imports additive STL model"
  COMMAND
    "${RENDERCLI}" --engine raster --width 64 --height 64
    "${stl_fixture}" "${stl_render}"
)
rendercli_assert_image_dimensions("${stl_render}" 64 64
                                  NAME "rendercli STL model dimensions")
rendercli_assert_image_nonempty("${stl_render}"
                                NAME "rendercli STL model pixels")

rendercli_run(
  NAME "rendercli imports additive 3MF model"
  COMMAND
    "${RENDERCLI}" --engine raster --width 64 --height 64
    "${threemf_fixture}" "${threemf_render}"
)
rendercli_assert_image_dimensions("${threemf_render}" 64 64
                                  NAME "rendercli 3MF model dimensions")
rendercli_assert_image_nonempty("${threemf_render}"
                                NAME "rendercli 3MF model pixels")

rendercli_run(
  NAME "rendercli colors G-code by speed"
  COMMAND
    "${RENDERCLI}" --engine raster --width 64 --height 64
    --gcode_visualization speed
    "${gcode_fixture}" "${gcode_speed_render}"
)
rendercli_assert_image_dimensions("${gcode_speed_render}" 64 64
                                  NAME "rendercli G-code speed dimensions")
rendercli_assert_image_nonempty("${gcode_speed_render}"
                                NAME "rendercli G-code speed pixels")

rendercli_run(
  NAME "rendercli filters one G-code layer and hides travel"
  COMMAND
    "${RENDERCLI}" --engine raster --width 64 --height 64
    --gcode_visualization tool --gcode_layer 0 --gcode_hide_travel
    "${gcode_fixture}" "${gcode_tool_layer_render}"
)
rendercli_assert_image_dimensions("${gcode_tool_layer_render}" 64 64
                                  NAME "rendercli G-code layer dimensions")
rendercli_assert_image_nonempty("${gcode_tool_layer_render}"
                                NAME "rendercli G-code layer pixels")
rendercli_assert_image_hash_differs(
  "${gcode_speed_render}" "${gcode_tool_layer_render}"
  NAME "rendercli G-code visualization modes differ")

rendercli_run(
  NAME "rendercli filters cumulative G-code layers"
  COMMAND
    "${RENDERCLI}" --engine raster --width 64 --height 64
    --gcode_visualization tool --gcode_layer 1 --gcode_cumulative_layers --gcode_hide_travel
    "${gcode_fixture}" "${gcode_cumulative_render}"
)
rendercli_assert_image_nonempty("${gcode_cumulative_render}"
                                NAME "rendercli G-code cumulative pixels")
rendercli_assert_image_hash_differs(
  "${gcode_tool_layer_render}" "${gcode_cumulative_render}"
  NAME "rendercli G-code current and cumulative layers differ")
