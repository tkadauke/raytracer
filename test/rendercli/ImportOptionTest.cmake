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
set(missing_render "${TEST_OUTPUT_DIR}/missing.png")
set(fake_openscad "${TEST_OUTPUT_DIR}/openscad-fake.sh")
set(openscad_scene "${TEST_OUTPUT_DIR}/openscad-source-asset.rtjson")
set(openscad_source "${TEST_OUTPUT_DIR}/simple.scad")
set(openscad_cache "${TEST_OUTPUT_DIR}/openscad-cache")

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
file(WRITE "${openscad_source}" "cube([1, 1, 1], center = true);\n")
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
      "sourcePath": "simple.scad",
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
