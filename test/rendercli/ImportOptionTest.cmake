if(NOT DEFINED RENDERCLI)
  message(FATAL_ERROR "RENDERCLI is required")
endif()

if(NOT DEFINED TEST_OUTPUT_DIR)
  message(FATAL_ERROR "TEST_OUTPUT_DIR is required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/RendercliTestHelpers.cmake")

file(REMOVE_RECURSE "${TEST_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${TEST_OUTPUT_DIR}")

set(run_real_openscad_fixture "$ENV{RAYTRACER_RUN_REAL_OPENSCAD_TESTS}")
if(run_real_openscad_fixture MATCHES "^(1|ON|TRUE|YES|true|yes)$")
  set(run_real_openscad_fixture TRUE)
else()
  set(run_real_openscad_fixture FALSE)
endif()

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
set(gltf_fixture "${PROJECT_SOURCE_DIR}/test/fixtures/gltf/comprehensive_scene.gltf")
set(stl_render "${TEST_OUTPUT_DIR}/stl-model.png")
set(threemf_render "${TEST_OUTPUT_DIR}/3mf-model.png")
set(gltf_render "${TEST_OUTPUT_DIR}/gltf-model.png")
set(gcode_speed_render "${TEST_OUTPUT_DIR}/gcode-speed.png")
set(gcode_tool_layer_render "${TEST_OUTPUT_DIR}/gcode-tool-layer.png")
set(gcode_cumulative_render "${TEST_OUTPUT_DIR}/gcode-cumulative.png")
set(molecule_fixture_dir "${PROJECT_SOURCE_DIR}/test/fixtures/molecules")
set(molecule_pdb_fixture "${molecule_fixture_dir}/small.pdb")
set(molecule_cif_fixture "${molecule_fixture_dir}/small.cif")
set(molecule_source_asset_fixture "${PROJECT_SOURCE_DIR}/test/fixtures/rendercli/molecule_source_asset.json")
set(molecule_ball_and_stick_render "${TEST_OUTPUT_DIR}/molecule-ball-and-stick.png")
set(molecule_space_filling_render "${TEST_OUTPUT_DIR}/molecule-space-filling.png")
set(molecule_source_asset_render "${TEST_OUTPUT_DIR}/molecule-source-asset.png")
set(molecule_source_asset_atoms_render "${TEST_OUTPUT_DIR}/molecule-source-asset-atoms.png")
set(molecule_source_asset_atoms_scene "${TEST_OUTPUT_DIR}/molecule-source-asset-atoms.json")

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

set(molecule_source_asset_atoms_scene_json [=[
{
  "id": "molecule-source-asset-atoms-scene",
  "name": "Molecule SourceAsset Atoms Render Fixture",
  "ambient": [0.7, 0.7, 0.7],
  "background": [0.0, 0.0, 0.0],
  "type": "Scene",
  "children": [
    {
      "id": "camera",
      "name": "Camera",
      "position": [0.0, 0.0, -2.5],
      "target": [0.0, 0.0, 0.0],
      "distance": 2.5,
      "zoom": 2.5,
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
      "id": "molecule-source",
      "name": "Molecule Source",
      "sourcePath": "__MOLECULE_SOURCE__",
      "format": "molecule",
      "importOptions": {
        "parameters": {
          "renderMode": "atoms"
        }
      },
      "type": "SourceAsset",
      "children": []
    }
  ]
}
]=])
string(REPLACE "__MOLECULE_SOURCE__" "${molecule_fixture_dir}/source_asset_demo.pdb"
       molecule_source_asset_atoms_scene_json "${molecule_source_asset_atoms_scene_json}")
file(WRITE "${molecule_source_asset_atoms_scene}" "${molecule_source_asset_atoms_scene_json}")

if(run_real_openscad_fixture)
  find_program(REAL_OPENSCAD_EXECUTABLE openscad)
endif()
if(run_real_openscad_fixture AND REAL_OPENSCAD_EXECUTABLE)
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

if(run_real_openscad_fixture AND REAL_OPENSCAD_EXECUTABLE)
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
elseif(run_real_openscad_fixture)
  message(STATUS "Skipping real OpenSCAD render smoke: openscad executable was not found")
else()
  message(STATUS "Skipping real OpenSCAD render smoke: set RAYTRACER_RUN_REAL_OPENSCAD_TESTS=1 to opt in")
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
  NAME "rendercli imports glTF mesh fixture"
  COMMAND
    "${RENDERCLI}" --engine raster --width 64 --height 64
    "${gltf_fixture}" "${gltf_render}"
)
rendercli_assert_image_dimensions("${gltf_render}" 64 64
                                  NAME "rendercli glTF model dimensions")
rendercli_assert_image_varied("${gltf_render}"
                              NAME "rendercli glTF model pixels")

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

rendercli_run(
  NAME "rendercli imports PDB molecule as ball-and-stick"
  COMMAND
    "${RENDERCLI}" --engine raster --width 64 --height 64
    --import_option representation=ball-and-stick
    --import_option colorScheme=element
    "${molecule_pdb_fixture}" "${molecule_ball_and_stick_render}"
)
rendercli_assert_image_dimensions("${molecule_ball_and_stick_render}" 64 64
                                  NAME "rendercli PDB molecule dimensions")
rendercli_assert_image_nonempty("${molecule_ball_and_stick_render}"
                                NAME "rendercli PDB molecule pixels")

rendercli_run(
  NAME "rendercli imports mmCIF molecule as space-filling"
  COMMAND
    "${RENDERCLI}" --engine raster --width 64 --height 64
    --import_option representation=space-filling
    --import_option colorScheme=chain
    --import_option spaceFillingScale=0.55
    "${molecule_cif_fixture}" "${molecule_space_filling_render}"
)
rendercli_assert_image_dimensions("${molecule_space_filling_render}" 64 64
                                  NAME "rendercli mmCIF molecule dimensions")
rendercli_assert_image_nonempty("${molecule_space_filling_render}"
                                NAME "rendercli mmCIF molecule pixels")
rendercli_assert_image_hash_differs(
  "${molecule_ball_and_stick_render}" "${molecule_space_filling_render}"
  NAME "rendercli molecule representations differ")

rendercli_run(
  NAME "rendercli renders molecule SourceAsset default mode"
  COMMAND
    "${RENDERCLI}" --engine raster --width 96 --height 96 --import_format json
    "${molecule_source_asset_fixture}" "${molecule_source_asset_render}"
)
rendercli_assert_image_dimensions("${molecule_source_asset_render}" 96 96
                                  NAME "rendercli molecule SourceAsset dimensions")
rendercli_assert_image_varied("${molecule_source_asset_render}"
                              NAME "rendercli molecule SourceAsset styled pixels")
rendercli_probe_image("${molecule_source_asset_render}"
                      NAME "rendercli molecule SourceAsset color families"
                      WARM_PIXELS_VARIABLE molecule_source_warm_pixels
                      COOL_PIXELS_VARIABLE molecule_source_cool_pixels
                      NEUTRAL_PIXELS_VARIABLE molecule_source_neutral_pixels
                      OUTPUT_VARIABLE molecule_source_probe)
if(molecule_source_warm_pixels LESS 1 OR molecule_source_cool_pixels LESS 1)
  _rendercli_fail(
    "rendercli molecule SourceAsset styled atoms"
    "expected default molecule SourceAsset render to contain warm and cool element-colored atom pixels"
    "" "" "${molecule_source_probe}" "")
endif()
if(molecule_source_neutral_pixels LESS 1)
  _rendercli_fail(
    "rendercli molecule SourceAsset bond pixels"
    "expected default molecule SourceAsset render to contain neutral bond pixels"
    "" "" "${molecule_source_probe}" "")
endif()

rendercli_run(
  NAME "rendercli renders molecule SourceAsset atoms mode"
  COMMAND
    "${RENDERCLI}" --engine raster --width 96 --height 96 --import_format json
    "${molecule_source_asset_atoms_scene}" "${molecule_source_asset_atoms_render}"
)
rendercli_assert_image_hash_differs(
  "${molecule_source_asset_render}" "${molecule_source_asset_atoms_render}"
  NAME "rendercli molecule SourceAsset default mode includes bonds")
