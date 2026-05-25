if(NOT DEFINED RENDERCLI)
  message(FATAL_ERROR "RENDERCLI is required")
endif()

if(NOT DEFINED TEST_OUTPUT_DIR)
  message(FATAL_ERROR "TEST_OUTPUT_DIR is required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/RendercliTestHelpers.cmake")

file(REMOVE_RECURSE "${TEST_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${TEST_OUTPUT_DIR}")

set(matte_scene "${PROJECT_SOURCE_DIR}/scenes/raster_material_preview.json")
set(reflective_scene "${PROJECT_SOURCE_DIR}/scenes/reflections.json")
set(transmissive_scene "${PROJECT_SOURCE_DIR}/scenes/glass_torus.json")
set(basic_render "${TEST_OUTPUT_DIR}/raster-basic.png")
set(lod_0_render "${TEST_OUTPUT_DIR}/raster-lod-0.png")
set(lod_3_render "${TEST_OUTPUT_DIR}/raster-lod-3.png")
set(invalid_render "${TEST_OUTPUT_DIR}/invalid.png")

rendercli_run(
  NAME "rendercli --engine raster renders a tiny image"
  ERROR_VARIABLE matte_stderr
  COMMAND
    "${RENDERCLI}" --engine raster --width 40 --height 24
    "${matte_scene}" "${basic_render}"
)
rendercli_assert_image_dimensions("${basic_render}" 40 24
                                  NAME "rendercli --engine raster dimensions")
rendercli_assert_image_nonempty("${basic_render}"
                                NAME "rendercli --engine raster pixels")
if(matte_stderr MATCHES "Rasterizer fallback:")
  _rendercli_fail("rendercli raster matte fallback warnings"
                  "simple matte/phong scene emitted recursive-material fallback warnings"
                  "" "" "" "${matte_stderr}")
endif()

rendercli_run(
  NAME "rendercli --lod 0 renders curved raster scene"
  COMMAND
    "${RENDERCLI}" --engine raster --width 64 --height 40 --lod 0
    "${matte_scene}" "${lod_0_render}"
)
rendercli_run(
  NAME "rendercli --lod 3 renders curved raster scene"
  COMMAND
    "${RENDERCLI}" --engine raster --width 64 --height 40 --lod 3
    "${matte_scene}" "${lod_3_render}"
)
rendercli_assert_image_dimensions("${lod_0_render}" 64 40
                                  NAME "rendercli --lod 0 dimensions")
rendercli_assert_image_dimensions("${lod_3_render}" 64 40
                                  NAME "rendercli --lod 3 dimensions")
rendercli_assert_image_hash_differs("${lod_0_render}" "${lod_3_render}"
                                    NAME "rendercli --lod changes raster output")

foreach(cull_mode IN ITEMS both back front)
  set(cull_render "${TEST_OUTPUT_DIR}/raster-cull-${cull_mode}.png")
  rendercli_run(
    NAME "rendercli --cull ${cull_mode} is accepted"
    COMMAND
      "${RENDERCLI}" --engine raster --width 32 --height 20 --cull "${cull_mode}"
      "${matte_scene}" "${cull_render}"
  )
  rendercli_assert_image_dimensions("${cull_render}" 32 20
                                    NAME "rendercli --cull ${cull_mode} dimensions")
endforeach()

foreach(msaa_samples IN ITEMS 1 2 4 8)
  set(msaa_render "${TEST_OUTPUT_DIR}/raster-msaa-${msaa_samples}.png")
  rendercli_run(
    NAME "rendercli --msaa ${msaa_samples} is accepted"
    COMMAND
      "${RENDERCLI}" --engine raster --width 36 --height 22 --msaa "${msaa_samples}"
      "${matte_scene}" "${msaa_render}"
  )
  rendercli_assert_image_dimensions("${msaa_render}" 36 22
                                    NAME "rendercli --msaa ${msaa_samples} dimensions")
endforeach()

foreach(msaa_shading IN ITEMS per_sample per_fragment)
  set(msaa_shading_render "${TEST_OUTPUT_DIR}/raster-msaa-shading-${msaa_shading}.png")
  rendercli_run(
    NAME "rendercli --msaa_shading ${msaa_shading} is accepted"
    COMMAND
      "${RENDERCLI}" --engine raster --width 36 --height 22
      --msaa 4 --msaa_shading "${msaa_shading}"
      "${matte_scene}" "${msaa_shading_render}"
  )
  rendercli_assert_image_dimensions("${msaa_shading_render}" 36 22
                                    NAME "rendercli --msaa_shading ${msaa_shading} dimensions")
endforeach()

foreach(post_aa IN ITEMS none fxaa smaa taa)
  set(post_aa_render "${TEST_OUTPUT_DIR}/raster-post-aa-${post_aa}.png")
  rendercli_run(
    NAME "rendercli --post_aa ${post_aa} is accepted"
    COMMAND
      "${RENDERCLI}" --engine raster --width 36 --height 22 --post_aa "${post_aa}"
      "${matte_scene}" "${post_aa_render}"
  )
  rendercli_assert_image_dimensions("${post_aa_render}" 36 22
                                    NAME "rendercli --post_aa ${post_aa} dimensions")
endforeach()

rendercli_run(
  NAME "rendercli raster warns for reflective material fallback"
  STDERR_MATCHES "Rasterizer fallback: ReflectiveMaterial"
  COMMAND
    "${RENDERCLI}" --engine raster --width 24 --height 16
    "${reflective_scene}" "${TEST_OUTPUT_DIR}/raster-reflective.png"
)

rendercli_run(
  NAME "rendercli raster warns for transmissive material fallback"
  STDERR_MATCHES "Rasterizer fallback: TransparentMaterial"
  COMMAND
    "${RENDERCLI}" --engine raster --width 24 --height 16
    "${transmissive_scene}" "${TEST_OUTPUT_DIR}/raster-transmissive.png"
)

rendercli_expect_failure(
  NAME "rendercli rejects invalid --cull"
  STDERR_MATCHES "Cull mode must be 'both', 'back', or 'front'"
  COMMAND
    "${RENDERCLI}" --engine raster --cull sideways
    "${matte_scene}" "${invalid_render}"
)

rendercli_expect_failure(
  NAME "rendercli rejects invalid --msaa"
  STDERR_MATCHES "MSAA samples must be 1, 2, 4, or 8"
  COMMAND
    "${RENDERCLI}" --engine raster --msaa 3
    "${matte_scene}" "${invalid_render}"
)

rendercli_expect_failure(
  NAME "rendercli rejects invalid --msaa_shading"
  STDERR_MATCHES "MSAA shading mode must be 'per_sample' or 'per_fragment'"
  COMMAND
    "${RENDERCLI}" --engine raster --msaa_shading centroid
    "${matte_scene}" "${invalid_render}"
)

rendercli_expect_failure(
  NAME "rendercli rejects invalid --post_aa"
  STDERR_MATCHES "Post-process AA must be 'none', 'fxaa', 'smaa', or 'taa'"
  COMMAND
    "${RENDERCLI}" --engine raster --post_aa sharpen
    "${matte_scene}" "${invalid_render}"
)
