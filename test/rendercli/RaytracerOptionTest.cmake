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
set(area_light_scene "${PROJECT_SOURCE_DIR}/scenes/pathtracer_area_light_demo.json")
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

foreach(sampler IN ITEMS Regular Random Jittered Halton)
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

set(pt_output "${TEST_OUTPUT_DIR}/pathtracer.png")
rendercli_run(
  NAME "rendercli raytracer accepts --integrator pathtracer"
  COMMAND
    "${RENDERCLI}" --direct_engine --engine raytracer --integrator pathtracer
    --width 24 --height 24 --sampler Regular --samples_per_pixel 4
    "${raytracer_scene}" "${pt_output}"
)
rendercli_assert_image_dimensions("${pt_output}" 24 24
                                  NAME "raytracer --integrator pathtracer dimensions")
rendercli_assert_image_nonempty("${pt_output}"
                                NAME "raytracer --integrator pathtracer pixels")

set(wavefront_output "${TEST_OUTPUT_DIR}/wavefront-direct.png")
rendercli_run(
  NAME "rendercli wavefront direct engine renders"
  COMMAND
    "${RENDERCLI}" --direct_engine --engine wavefront
    --width 24 --height 24 --sampler Regular --samples_per_pixel 4
    "${raytracer_scene}" "${wavefront_output}"
)
rendercli_assert_image_dimensions("${wavefront_output}" 24 24
                                  NAME "wavefront direct dimensions")
rendercli_assert_image_nonempty("${wavefront_output}" NAME "wavefront direct pixels")

set(wavefront_pathtracer_output "${TEST_OUTPUT_DIR}/wavefront-pathtracer-direct.png")
rendercli_run(
  NAME "rendercli wavefront direct engine accepts --integrator pathtracer"
  COMMAND
    "${RENDERCLI}" --direct_engine --engine wavefront --integrator pathtracer
    --width 24 --height 24 --sampler Regular --samples_per_pixel 4
    "${raytracer_scene}" "${wavefront_pathtracer_output}"
)
rendercli_assert_image_dimensions("${wavefront_pathtracer_output}" 24 24
                                  NAME "wavefront pathtracer direct dimensions")
rendercli_assert_image_nonempty("${wavefront_pathtracer_output}"
                                NAME "wavefront pathtracer direct pixels")

set(pathtracer_direct_output "${TEST_OUTPUT_DIR}/pathtracer-direct.png")
rendercli_run(
  NAME "rendercli pathtracer direct engine renders through wavefront"
  COMMAND
    "${RENDERCLI}" --direct_engine --engine pathtracer
    --width 24 --height 24 --sampler Regular --samples_per_pixel 4
    "${raytracer_scene}" "${pathtracer_direct_output}"
)
rendercli_assert_image_dimensions("${pathtracer_direct_output}" 24 24
                                  NAME "pathtracer direct dimensions")
rendercli_assert_image_nonempty("${pathtracer_direct_output}" NAME "pathtracer direct pixels")

set(pathtracer_scalar_direct_output "${TEST_OUTPUT_DIR}/pathtracer-scalar-direct.png")
rendercli_run(
  NAME "rendercli pathtracer direct engine accepts scalar schedule"
  COMMAND
    "${RENDERCLI}" --direct_engine --engine pathtracer --path_tracing_schedule scalar
    --width 24 --height 24 --sampler Regular --samples_per_pixel 4
    "${raytracer_scene}" "${pathtracer_scalar_direct_output}"
)
rendercli_assert_image_dimensions("${pathtracer_scalar_direct_output}" 24 24
                                  NAME "pathtracer scalar direct dimensions")
rendercli_assert_image_nonempty("${pathtracer_scalar_direct_output}"
                                NAME "pathtracer scalar direct pixels")

set(area_light_output "${TEST_OUTPUT_DIR}/pathtracer-area-light.png")
rendercli_run(
  NAME "rendercli pathtracer renders rectangular area light scene"
  COMMAND
    "${RENDERCLI}" --direct_engine --engine pathtracer
    --width 32 --height 24 --sampler Halton --samples_per_pixel 8 --depth 3
    "${area_light_scene}" "${area_light_output}"
)
rendercli_assert_image_dimensions("${area_light_output}" 32 24
                                  NAME "pathtracer area light dimensions")
rendercli_assert_image_nonempty("${area_light_output}" NAME "pathtracer area light pixels")

set(sample_stddev_output "${TEST_OUTPUT_DIR}/pathtracer-sample-stddev.png")
set(sample_stddev_render "${TEST_OUTPUT_DIR}/pathtracer-sample-stddev-render.png")
rendercli_run(
  NAME "rendercli pathtracer writes sample standard-deviation image"
  COMMAND
    "${RENDERCLI}" --direct_engine --engine pathtracer --wavefront_sample_stddev_out
    "${sample_stddev_output}" --width 32 --height 24 --sampler Halton --samples_per_pixel 8
    --sampling_seed 17 --depth 3 "${area_light_scene}" "${sample_stddev_render}"
)
rendercli_assert_image_dimensions("${sample_stddev_output}" 32 24
                                  NAME "pathtracer sample stddev dimensions")
rendercli_assert_image_nonempty("${sample_stddev_output}"
                                NAME "pathtracer sample stddev pixels")

set(sample_stddev_color_output "${TEST_OUTPUT_DIR}/pathtracer-sample-stddev-color.png")
set(sample_stddev_color_render "${TEST_OUTPUT_DIR}/pathtracer-sample-stddev-color-render.png")
rendercli_run(
  NAME "rendercli pathtracer writes color sample standard-deviation image"
  COMMAND
    "${RENDERCLI}" --direct_engine --engine pathtracer --wavefront_sample_stddev_color_out
    "${sample_stddev_color_output}" --width 32 --height 24 --sampler Halton --samples_per_pixel 8
    --sampling_seed 17 --depth 3 "${area_light_scene}" "${sample_stddev_color_render}"
)
rendercli_assert_image_dimensions("${sample_stddev_color_output}" 32 24
                                  NAME "pathtracer sample stddev color dimensions")
rendercli_assert_image_nonempty("${sample_stddev_color_output}"
                                NAME "pathtracer sample stddev color pixels")

set(sample_stddev_graph_output "${TEST_OUTPUT_DIR}/pathtracer-sample-stddev-graph.png")
set(sample_stddev_graph_render "${TEST_OUTPUT_DIR}/pathtracer-sample-stddev-graph-render.png")
rendercli_run(
  NAME "rendercli pathtracer graph writes sample standard-deviation image"
  COMMAND
    "${RENDERCLI}" --engine pathtracer --wavefront_sample_stddev_out
    "${sample_stddev_graph_output}"
    --width 16 --height 12 --sampler Halton --samples_per_pixel 4
    "${area_light_scene}" "${sample_stddev_graph_render}"
)
rendercli_assert_image_dimensions("${sample_stddev_graph_output}" 16 12
                                  NAME "pathtracer graph sample stddev dimensions")
rendercli_assert_image_nonempty("${sample_stddev_graph_output}"
                                NAME "pathtracer graph sample stddev pixels")

set(sample_stddev_aov_output "${TEST_OUTPUT_DIR}/pathtracer-sample-stddev-aov.png")
set(sample_stddev_aov_render "${TEST_OUTPUT_DIR}/pathtracer-sample-stddev-aov-render.png")
rendercli_run(
  NAME "rendercli graph AOV output accepts sample standard-deviation view"
  COMMAND
    "${RENDERCLI}" --engine pathtracer --render_graph_aov_out
    "sample_stddev=${sample_stddev_aov_output}" --width 16 --height 12 --sampler Halton
    --samples_per_pixel 4 "${area_light_scene}" "${sample_stddev_aov_render}"
)
rendercli_assert_image_dimensions("${sample_stddev_aov_output}" 16 12
                                  NAME "pathtracer graph AOV sample stddev dimensions")
rendercli_assert_image_nonempty("${sample_stddev_aov_output}"
                                NAME "pathtracer graph AOV sample stddev pixels")

set(sample_stddev_color_aov_output
    "${TEST_OUTPUT_DIR}/pathtracer-sample-stddev-color-aov.png")
set(sample_stddev_color_aov_render
    "${TEST_OUTPUT_DIR}/pathtracer-sample-stddev-color-aov-render.png")
rendercli_run(
  NAME "rendercli graph AOV output accepts color sample standard-deviation view"
  COMMAND
    "${RENDERCLI}" --engine pathtracer --render_graph_aov_out
    "sample_stddev_color=${sample_stddev_color_aov_output}" --width 16 --height 12
    --sampler Halton --samples_per_pixel 4 "${area_light_scene}"
    "${sample_stddev_color_aov_render}"
)
rendercli_assert_image_dimensions("${sample_stddev_color_aov_output}" 16 12
                                  NAME "pathtracer graph AOV sample stddev color dimensions")
rendercli_assert_image_nonempty("${sample_stddev_color_aov_output}"
                                NAME "pathtracer graph AOV sample stddev color pixels")

set(wavefront_denoise_output "${TEST_OUTPUT_DIR}/wavefront-denoise-direct.png")
rendercli_run(
  NAME "rendercli wavefront direct engine accepts denoiser"
  COMMAND
    "${RENDERCLI}" --direct_engine --engine wavefront --wavefront_denoiser bilateral
    --wavefront_denoise_radius 2 --wavefront_denoise_color_sigma 0.2 --width 24 --height 24
    --sampler Regular --samples_per_pixel 4 "${raytracer_scene}" "${wavefront_denoise_output}"
)
rendercli_assert_image_dimensions("${wavefront_denoise_output}" 24 24
                                  NAME "wavefront denoise direct dimensions")
rendercli_assert_image_nonempty("${wavefront_denoise_output}"
                                NAME "wavefront denoise direct pixels")

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

set(ldraw_extension_output "${TEST_OUTPUT_DIR}/ldraw-extension-import.png")
rendercli_run(
  NAME "rendercli imports LDraw input by extension"
  COMMAND
    "${RENDERCLI}" --direct_engine --engine raytracer --width 24 --height 24
    --ldraw_library_root "${PROJECT_SOURCE_DIR}/test/fixtures/ldraw/rendercli/library"
    "${PROJECT_SOURCE_DIR}/test/fixtures/ldraw/rendercli/model.ldr"
    "${ldraw_extension_output}"
)
rendercli_assert_image_dimensions("${ldraw_extension_output}" 24 24
                                  NAME "LDraw extension import dimensions")
rendercli_assert_image_nonempty("${ldraw_extension_output}"
                                NAME "LDraw extension import pixels")

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
    --ldraw_coordinate_conversion none
    "${PROJECT_SOURCE_DIR}/test/fixtures/ldraw/smoke/model.mpd"
    "${ldraw_direct_mpd_output}"
)
rendercli_assert_image_dimensions("${ldraw_direct_mpd_output}" 24 24
                                  NAME "direct LDraw MPD dimensions")
rendercli_assert_image_nonempty("${ldraw_direct_mpd_output}"
                                NAME "direct LDraw MPD pixels")
rendercli_assert_image_varied("${ldraw_direct_mpd_output}"
                              NAME "direct LDraw MPD varied pixels")

set(ldraw_background_output "${TEST_OUTPUT_DIR}/ldraw-background-color.png")
rendercli_run(
  NAME "rendercli applies direct LDraw background color"
  COMMAND
    "${RENDERCLI}" --direct_engine --engine raytracer --width 24 --height 24
    --ldraw_input
    --ldraw_library_root "${PROJECT_SOURCE_DIR}/test/fixtures/ldraw/smoke/library"
    --ldraw_coordinate_conversion none
    --ldraw-background-color "#ff00ff"
    "${PROJECT_SOURCE_DIR}/test/fixtures/ldraw/smoke/model.mpd"
    "${ldraw_background_output}"
)
rendercli_assert_image_dimensions("${ldraw_background_output}" 24 24
                                  NAME "LDraw background color dimensions")
rendercli_assert_image_hash_differs("${ldraw_direct_mpd_output}" "${ldraw_background_output}"
                                    NAME "LDraw background color changes output")

set(ldraw_offset_model "${TEST_OUTPUT_DIR}/ldraw-offset.ldr")
set(ldraw_offset_output "${TEST_OUTPUT_DIR}/ldraw-offset.png")
file(WRITE "${ldraw_offset_model}" [=[
3 0x02C91A09 -320 -320 100 -240 -320 100 -320 -240 100
3 0x020055BF -320 -320 140 -320 -240 100 -240 -320 100
3 0x02237841 -320 -240 100 -320 -320 140 -240 -320 100
]=])
rendercli_run(
  NAME "rendercli raytracer frames direct LDraw input away from the origin"
  COMMAND
    "${RENDERCLI}" --direct_engine --engine raytracer --width 24 --height 24
    --ldraw_input
    "${ldraw_offset_model}"
    "${ldraw_offset_output}"
)
rendercli_assert_image_dimensions("${ldraw_offset_output}" 24 24
                                  NAME "offset LDraw dimensions")
rendercli_assert_image_varied("${ldraw_offset_output}"
                              NAME "offset LDraw varied pixels")

rendercli_expect_failure(
  NAME "rendercli validates direct LDraw import options"
  STDERR_MATCHES "LDraw missing part policy must be 'error' or 'skip'"
  COMMAND
    "${RENDERCLI}" --direct_engine --engine raytracer --width 24 --height 24
    --ldraw_input --ldraw_missing_part_policy maybe
    "${PROJECT_SOURCE_DIR}/test/fixtures/ldraw/rendercli/model.ldr"
    "${TEST_OUTPUT_DIR}/ldraw-invalid.png"
)

rendercli_expect_failure(
  NAME "rendercli validates direct LDraw background color"
  STDERR_MATCHES "LDraw background_color must be a color name or hex color"
  COMMAND
    "${RENDERCLI}" --direct_engine --engine raytracer --width 24 --height 24
    --ldraw_input --ldraw-background-color definitely-not-a-color
    "${PROJECT_SOURCE_DIR}/test/fixtures/ldraw/rendercli/model.ldr"
    "${TEST_OUTPUT_DIR}/ldraw-invalid-background.png"
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

rendercli_expect_failure(
  NAME "rendercli rejects unknown path tracing schedule"
  STDERR_MATCHES "Path tracing schedule must be 'wavefront' or 'scalar'"
  COMMAND
    "${RENDERCLI}" --direct_engine --engine pathtracer --path_tracing_schedule maybe
    --width 16 --height 16 "${raytracer_scene}" "${TEST_OUTPUT_DIR}/invalid-path-schedule.png"
)

rendercli_expect_failure(
  NAME "rendercli rejects path tracing schedule engine conflicts"
  STDERR_MATCHES "Path tracing schedule conflicts with the selected engine"
  COMMAND
    "${RENDERCLI}" --direct_engine --engine wavefront --integrator pathtracer
    --path_tracing_schedule scalar --width 16 --height 16 "${raytracer_scene}"
    "${TEST_OUTPUT_DIR}/invalid-path-schedule-conflict.png"
)

rendercli_expect_failure(
  NAME "rendercli rejects scalar schedule for wavefront sample standard deviation"
  STDERR_MATCHES
    "Wavefront sample standard-deviation output requires a wavefront path-tracing schedule"
  COMMAND
    "${RENDERCLI}" --direct_engine --engine pathtracer --path_tracing_schedule scalar
    --wavefront_sample_stddev_out "${TEST_OUTPUT_DIR}/invalid-path-stddev.png"
    --width 16 --height 16 "${raytracer_scene}" "${TEST_OUTPUT_DIR}/invalid-path-stddev-render.png"
)
