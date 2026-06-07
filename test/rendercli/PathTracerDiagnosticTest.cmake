if(NOT DEFINED RENDERCLI)
  message(FATAL_ERROR "RENDERCLI is required")
endif()

if(NOT DEFINED TEST_OUTPUT_DIR)
  message(FATAL_ERROR "TEST_OUTPUT_DIR is required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/RendercliTestHelpers.cmake")

file(REMOVE_RECURSE "${TEST_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${TEST_OUTPUT_DIR}")

set(direct_scene "${TEST_OUTPUT_DIR}/pathtracer-diagnostic-direct.json")
set(glass_scene "${PROJECT_SOURCE_DIR}/scenes/glass_torus.json")
set(reflections_scene "${PROJECT_SOURCE_DIR}/scenes/reflections.json")
set(area_light_scene "${PROJECT_SOURCE_DIR}/scenes/pathtracer_area_light_demo.json")

file(WRITE "${direct_scene}" [=[
{
  "id": "pathtracer-diagnostic-direct",
  "name": "Path Tracer Diagnostic Direct",
  "ambient": [0.0, 0.0, 0.0],
  "background": [0.02, 0.03, 0.04],
  "type": "Scene",
  "children": [
    {
      "id": "camera",
      "name": "Camera",
      "position": [0.0, 1.0, -5.0],
      "target": [0.0, 0.0, 0.0],
      "distance": 5.0,
      "zoom": 1.2,
      "type": "PinholeCamera",
      "children": []
    },
    {
      "id": "key",
      "name": "Key Light",
      "position": [-3.0, -4.0, -4.0],
      "rotation": [0.0, 0.0, 0.0],
      "scale": [1.0, 1.0, 1.0],
      "visible": true,
      "color": [1.0, 1.0, 1.0],
      "intensity": 0.8,
      "type": "PointLight",
      "children": []
    },
    {
      "id": "fill",
      "name": "Fill Light",
      "position": [4.0, -3.0, -2.0],
      "rotation": [0.0, 0.0, 0.0],
      "scale": [1.0, 1.0, 1.0],
      "visible": true,
      "color": [0.7, 0.85, 1.0],
      "intensity": 0.35,
      "type": "PointLight",
      "children": []
    },
    {
      "id": "red_texture",
      "name": "Red Texture",
      "color": [0.85, 0.12, 0.08],
      "type": "ConstantColorTexture",
      "children": []
    },
    {
      "id": "floor_texture",
      "name": "Floor Texture",
      "color": [0.72, 0.72, 0.68],
      "type": "ConstantColorTexture",
      "children": []
    },
    {
      "id": "matte",
      "name": "Matte",
      "diffuseTexture": "red_texture",
      "ambientCoefficient": 0.0,
      "diffuseCoefficient": 1.0,
      "type": "MatteMaterial",
      "children": []
    },
    {
      "id": "mirror",
      "name": "Mirror",
      "diffuseTexture": "floor_texture",
      "ambientCoefficient": 0.0,
      "diffuseCoefficient": 0.2,
      "reflectionCoefficient": 0.8,
      "reflectionColor": [1.0, 1.0, 1.0],
      "specularCoefficient": 0.2,
      "specularColor": [1.0, 1.0, 1.0],
      "exponent": 64,
      "type": "ReflectiveMaterial",
      "children": []
    },
    {
      "id": "sphere",
      "name": "Sphere",
      "position": [-0.7, 0.0, 0.0],
      "rotation": [0.0, 0.0, 0.0],
      "scale": [1.0, 1.0, 1.0],
      "visible": true,
      "material": "matte",
      "radius": 0.8,
      "type": "Sphere",
      "children": []
    },
    {
      "id": "mirror_sphere",
      "name": "Mirror Sphere",
      "position": [1.0, -0.1, 0.6],
      "rotation": [0.0, 0.0, 0.0],
      "scale": [1.0, 1.0, 1.0],
      "visible": true,
      "material": "mirror",
      "radius": 0.7,
      "type": "Sphere",
      "children": []
    }
  ]
}
]=])

function(pathtracer_diagnostic_compare name scene low_samples reference_samples max_rms)
  set(low_output "${TEST_OUTPUT_DIR}/${name}-low.png")
  set(reference_output "${TEST_OUTPUT_DIR}/${name}-reference.png")

  rendercli_run(
    NAME "pathtracer diagnostic ${name} low samples"
    COMMAND
      "${RENDERCLI}" --direct_engine --engine pathtracer
      --width 40 --height 30 --sampler Halton --sampling_seed 12345
      --samples_per_pixel "${low_samples}" --depth 4
      "${scene}" "${low_output}"
  )
  rendercli_run(
    NAME "pathtracer diagnostic ${name} reference samples"
    COMMAND
      "${RENDERCLI}" --direct_engine --engine pathtracer
      --width 40 --height 30 --sampler Halton --sampling_seed 12345
      --samples_per_pixel "${reference_samples}" --depth 4
      "${scene}" "${reference_output}"
  )
  rendercli_assert_image_dimensions("${low_output}" 40 30
                                    NAME "pathtracer diagnostic ${name} low dimensions")
  rendercli_assert_image_dimensions("${reference_output}" 40 30
                                    NAME "pathtracer diagnostic ${name} reference dimensions")
  rendercli_assert_image_nonempty("${low_output}" NAME "pathtracer diagnostic ${name} low pixels")
  rendercli_assert_image_nonempty("${reference_output}"
                                  NAME "pathtracer diagnostic ${name} reference pixels")
  rendercli_assert_image_rms_at_most(
    "${reference_output}" "${low_output}" "${max_rms}"
    NAME "pathtracer diagnostic ${name} low/reference RMS"
  )
endfunction()

function(pathtracer_diagnostic_compare_raytracer_parity name scene max_rms)
  set(raytracer_output "${TEST_OUTPUT_DIR}/${name}-raytracer.png")
  set(pathtracer_output "${TEST_OUTPUT_DIR}/${name}-pathtracer.png")

  rendercli_run(
    NAME "pathtracer diagnostic ${name} raytracer parity reference"
    COMMAND
      "${RENDERCLI}" --direct_engine --engine raytracer
      --width 40 --height 30 --samples_per_pixel 1 --depth 4
      "${scene}" "${raytracer_output}"
  )
  rendercli_run(
    NAME "pathtracer diagnostic ${name} pathtracer parity candidate"
    COMMAND
      "${RENDERCLI}" --direct_engine --engine pathtracer
      --width 40 --height 30 --sampler Regular --sampling_seed 12345
      --samples_per_pixel 1 --depth 4
      "${scene}" "${pathtracer_output}"
  )
  rendercli_assert_image_dimensions("${raytracer_output}" 40 30
                                    NAME "pathtracer diagnostic ${name} raytracer dimensions")
  rendercli_assert_image_dimensions("${pathtracer_output}" 40 30
                                    NAME "pathtracer diagnostic ${name} pathtracer dimensions")
  rendercli_assert_image_nonempty("${raytracer_output}"
                                  NAME "pathtracer diagnostic ${name} raytracer pixels")
  rendercli_assert_image_nonempty("${pathtracer_output}"
                                  NAME "pathtracer diagnostic ${name} pathtracer pixels")
  rendercli_assert_image_rms_at_most(
    "${raytracer_output}" "${pathtracer_output}" "${max_rms}"
    NAME "pathtracer diagnostic ${name} raytracer/pathtracer RMS"
  )
endfunction()

function(pathtracer_diagnostic_assert_metric_at_most name stdout metric threshold)
  string(REGEX MATCH "${metric}=([0-9]+\\.[0-9]+)" metric_match "${stdout}")
  if(NOT metric_match)
    _rendercli_fail("${name}" "stdout did not contain ${metric}" "" "" "${stdout}" "")
  endif()
  set(value "${CMAKE_MATCH_1}")
  if(value GREATER threshold)
    _rendercli_fail("${name}" "expected ${metric} at most ${threshold}, got ${value}"
                    "" "" "${stdout}" "")
  endif()
endfunction()

function(pathtracer_diagnostic_assert_low_variance name scene)
  set(output "${TEST_OUTPUT_DIR}/${name}.png")

  rendercli_run(
    NAME "pathtracer diagnostic ${name} low variance"
    OUTPUT_VARIABLE metrics_stdout
    COMMAND
      "${RENDERCLI}" --direct_engine --engine pathtracer
      --width 80 --height 60 --sampler Jittered --sampling_seed 12345
      --samples_per_pixel 16 --depth 10 --wavefront_denoiser none
      --wavefront_metrics_summary "${scene}" "${output}"
  )
  rendercli_assert_image_dimensions("${output}" 80 60
                                    NAME "pathtracer diagnostic ${name} dimensions")
  rendercli_assert_image_nonempty("${output}" NAME "pathtracer diagnostic ${name} pixels")
  pathtracer_diagnostic_assert_metric_at_most(
    "pathtracer diagnostic ${name} variance threshold"
    "${metrics_stdout}" "sample_stddev_rms" 1.0
  )
endfunction()

pathtracer_diagnostic_compare_raytracer_parity("direct-parity" "${direct_scene}" 0.03)
pathtracer_diagnostic_compare("direct" "${direct_scene}" 4 32 0.08)
pathtracer_diagnostic_compare("glass" "${glass_scene}" 4 32 0.10)
pathtracer_diagnostic_compare("area-light" "${area_light_scene}" 8 64 0.12)
pathtracer_diagnostic_assert_low_variance("exact-delta-reflections" "${reflections_scene}")
