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
set(static_scene "${PROJECT_SOURCE_DIR}/scenes/dice.json")
set(reflective_scene "${PROJECT_SOURCE_DIR}/scenes/reflections.json")
set(transmissive_scene "${PROJECT_SOURCE_DIR}/scenes/glass_torus.json")
set(basic_render "${TEST_OUTPUT_DIR}/raster-basic.png")
set(opengl_graph "${TEST_OUTPUT_DIR}/raster-opengl-graph.json")
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
  NAME "rendercli --raster_backend opengl compiles graph pass state"
  COMMAND
    "${RENDERCLI}" --engine raster --render_graph_only --render_graph_format json
    --raster_backend opengl
    "${matte_scene}" "${opengl_graph}"
)
file(READ "${opengl_graph}" opengl_graph_json)
if(NOT opengl_graph_json MATCHES "\"backend\"[ \r\n]*:[ \r\n]*\"opengl\"")
  _rendercli_fail("rendercli --raster_backend opengl graph state"
                  "compiled graph did not serialize the OpenGL raster backend"
                  "" "" "" "${opengl_graph_json}")
endif()

set(opengl_render "${TEST_OUTPUT_DIR}/raster-opengl.png")
execute_process(
  COMMAND
    "${RENDERCLI}" --engine raster --width 16 --height 12 --raster_backend gpu
    "${matte_scene}" "${opengl_render}"
  RESULT_VARIABLE opengl_result
  OUTPUT_VARIABLE opengl_stdout
  ERROR_VARIABLE opengl_stderr
)
if(opengl_result STREQUAL "0")
  rendercli_assert_image_dimensions("${opengl_render}" 16 12
                                    NAME "rendercli --raster_backend gpu dimensions")
  rendercli_assert_image_nonempty("${opengl_render}"
                                  NAME "rendercli --raster_backend gpu pixels")
elseif(opengl_stderr MATCHES "OpenGL raster backend is selected")
  if(opengl_stderr MATCHES "QCoreApplication")
    _rendercli_fail("rendercli --raster_backend gpu application bootstrap"
                    "OpenGL backend still failed before rendercli started a GUI-capable application"
                    "" "${opengl_result}" "${opengl_stdout}" "${opengl_stderr}")
  endif()
else()
  _rendercli_fail("rendercli --raster_backend gpu application bootstrap"
                  "OpenGL backend neither rendered nor reported a clear OpenGL capability error"
                  "" "${opengl_result}" "${opengl_stdout}" "${opengl_stderr}")
endif()

set(opengl_msaa_render "${TEST_OUTPUT_DIR}/raster-opengl-msaa.png")
execute_process(
  COMMAND
    "${RENDERCLI}" --engine raster --width 16 --height 12 --raster_backend gpu --msaa 4
    "${matte_scene}" "${opengl_msaa_render}"
  RESULT_VARIABLE opengl_msaa_result
  OUTPUT_VARIABLE opengl_msaa_stdout
  ERROR_VARIABLE opengl_msaa_stderr
)
if(opengl_msaa_result STREQUAL "0")
  rendercli_assert_image_dimensions("${opengl_msaa_render}" 16 12
                                    NAME "rendercli --raster_backend gpu --msaa dimensions")
  rendercli_assert_image_nonempty("${opengl_msaa_render}"
                                  NAME "rendercli --raster_backend gpu --msaa pixels")
elseif(opengl_msaa_stderr MATCHES "OpenGL raster backend is selected")
  if(opengl_msaa_stderr MATCHES "QCoreApplication")
    _rendercli_fail("rendercli --raster_backend gpu --msaa application bootstrap"
                    "OpenGL MSAA still failed before rendercli started a GUI-capable application"
                    "" "${opengl_msaa_result}" "${opengl_msaa_stdout}" "${opengl_msaa_stderr}")
  endif()
else()
  _rendercli_fail("rendercli --raster_backend gpu --msaa"
                  "OpenGL MSAA neither rendered nor reported a clear OpenGL capability error"
                  "" "${opengl_msaa_result}" "${opengl_msaa_stdout}" "${opengl_msaa_stderr}")
endif()

set(opengl_color_mask_render "${TEST_OUTPUT_DIR}/raster-opengl-color-mask.png")
execute_process(
  COMMAND
    "${RENDERCLI}" --engine raster --width 16 --height 12 --raster_backend gpu
    --color_write_mask none
    "${matte_scene}" "${opengl_color_mask_render}"
  RESULT_VARIABLE opengl_color_mask_result
  OUTPUT_VARIABLE opengl_color_mask_stdout
  ERROR_VARIABLE opengl_color_mask_stderr
)
if(opengl_color_mask_result STREQUAL "0")
  rendercli_assert_image_dimensions("${opengl_color_mask_render}" 16 12
                                    NAME "rendercli --raster_backend gpu color mask dimensions")
elseif(opengl_color_mask_stderr MATCHES "OpenGL raster backend is selected")
  if(opengl_color_mask_stderr MATCHES "QCoreApplication")
    _rendercli_fail("rendercli --raster_backend gpu color mask application bootstrap"
                    "OpenGL color mask still failed before rendercli started a GUI-capable application"
                    "" "${opengl_color_mask_result}" "${opengl_color_mask_stdout}"
                    "${opengl_color_mask_stderr}")
  endif()
else()
  _rendercli_fail("rendercli --raster_backend gpu color mask"
                  "OpenGL color mask neither rendered nor reported a clear OpenGL capability error"
                  "" "${opengl_color_mask_result}" "${opengl_color_mask_stdout}"
                  "${opengl_color_mask_stderr}")
endif()

set(opengl_blend_render "${TEST_OUTPUT_DIR}/raster-opengl-blend.png")
execute_process(
  COMMAND
    "${RENDERCLI}" --engine raster --width 16 --height 12 --raster_backend gpu --blend
    --blend_src constant_alpha --blend_dst one_minus_constant_alpha
    --blend_constant_alpha 0.35
    "${matte_scene}" "${opengl_blend_render}"
  RESULT_VARIABLE opengl_blend_result
  OUTPUT_VARIABLE opengl_blend_stdout
  ERROR_VARIABLE opengl_blend_stderr
)
if(opengl_blend_result STREQUAL "0")
  rendercli_assert_image_dimensions("${opengl_blend_render}" 16 12
                                    NAME "rendercli --raster_backend gpu blend dimensions")
  rendercli_assert_image_nonempty("${opengl_blend_render}"
                                  NAME "rendercli --raster_backend gpu blend pixels")
elseif(opengl_blend_stderr MATCHES "OpenGL raster backend is selected")
  if(opengl_blend_stderr MATCHES "QCoreApplication")
    _rendercli_fail("rendercli --raster_backend gpu blend application bootstrap"
                    "OpenGL blending still failed before rendercli started a GUI-capable application"
                    "" "${opengl_blend_result}" "${opengl_blend_stdout}"
                    "${opengl_blend_stderr}")
  endif()
else()
  _rendercli_fail("rendercli --raster_backend gpu blend"
                  "OpenGL blending neither rendered nor reported a clear OpenGL capability error"
                  "" "${opengl_blend_result}" "${opengl_blend_stdout}"
                  "${opengl_blend_stderr}")
endif()

set(opengl_alpha_test_render "${TEST_OUTPUT_DIR}/raster-opengl-alpha-test.png")
execute_process(
  COMMAND
    "${RENDERCLI}" --engine raster --width 16 --height 12 --raster_backend gpu
    --alpha_test --alpha_func greater --alpha_ref 0.25
    "${matte_scene}" "${opengl_alpha_test_render}"
  RESULT_VARIABLE opengl_alpha_test_result
  OUTPUT_VARIABLE opengl_alpha_test_stdout
  ERROR_VARIABLE opengl_alpha_test_stderr
)
if(opengl_alpha_test_result STREQUAL "0")
  rendercli_assert_image_dimensions("${opengl_alpha_test_render}" 16 12
                                    NAME "rendercli --raster_backend gpu alpha test dimensions")
  rendercli_assert_image_nonempty("${opengl_alpha_test_render}"
                                  NAME "rendercli --raster_backend gpu alpha test pixels")
elseif(opengl_alpha_test_stderr MATCHES "OpenGL raster backend is selected")
  if(opengl_alpha_test_stderr MATCHES "QCoreApplication")
    _rendercli_fail("rendercli --raster_backend gpu alpha test application bootstrap"
                    "OpenGL alpha test still failed before rendercli started a GUI-capable application"
                    "" "${opengl_alpha_test_result}" "${opengl_alpha_test_stdout}"
                    "${opengl_alpha_test_stderr}")
  endif()
else()
  _rendercli_fail("rendercli --raster_backend gpu alpha test"
                  "OpenGL alpha test neither rendered nor reported a clear OpenGL capability error"
                  "" "${opengl_alpha_test_result}" "${opengl_alpha_test_stdout}"
                  "${opengl_alpha_test_stderr}")
endif()

set(opengl_depth_render "${TEST_OUTPUT_DIR}/raster-opengl-depth.png")
execute_process(
  COMMAND
    "${RENDERCLI}" --engine raster --width 16 --height 12 --raster_backend gpu
    --render_graph_view depth
    "${matte_scene}" "${opengl_depth_render}"
  RESULT_VARIABLE opengl_depth_result
  OUTPUT_VARIABLE opengl_depth_stdout
  ERROR_VARIABLE opengl_depth_stderr
)
if(opengl_depth_result STREQUAL "0")
  rendercli_assert_image_dimensions("${opengl_depth_render}" 16 12
                                    NAME "rendercli --raster_backend gpu depth dimensions")
  rendercli_assert_image_nonempty("${opengl_depth_render}"
                                  NAME "rendercli --raster_backend gpu depth pixels")
elseif(opengl_depth_stderr MATCHES "OpenGL raster backend is selected")
  if(opengl_depth_stderr MATCHES "QCoreApplication")
    _rendercli_fail("rendercli --raster_backend gpu depth application bootstrap"
                    "OpenGL depth AOV still failed before rendercli started a GUI-capable application"
                    "" "${opengl_depth_result}" "${opengl_depth_stdout}" "${opengl_depth_stderr}")
  endif()
else()
  _rendercli_fail("rendercli --raster_backend gpu depth AOV"
                  "OpenGL depth AOV neither rendered nor reported a clear OpenGL capability error"
                  "" "${opengl_depth_result}" "${opengl_depth_stdout}" "${opengl_depth_stderr}")
endif()

set(opengl_stencil_render "${TEST_OUTPUT_DIR}/raster-opengl-stencil.png")
execute_process(
  COMMAND
    "${RENDERCLI}" --engine raster --width 16 --height 12 --raster_backend gpu
    --render_graph_view stencil
    "${matte_scene}" "${opengl_stencil_render}"
  RESULT_VARIABLE opengl_stencil_result
  OUTPUT_VARIABLE opengl_stencil_stdout
  ERROR_VARIABLE opengl_stencil_stderr
)
if(opengl_stencil_result STREQUAL "0")
  rendercli_assert_image_dimensions("${opengl_stencil_render}" 16 12
                                    NAME "rendercli --raster_backend gpu stencil dimensions")
  rendercli_assert_image_nonempty("${opengl_stencil_render}"
                                  NAME "rendercli --raster_backend gpu stencil pixels")
elseif(opengl_stencil_stderr MATCHES "OpenGL raster backend is selected")
  if(opengl_stencil_stderr MATCHES "QCoreApplication")
    _rendercli_fail("rendercli --raster_backend gpu stencil application bootstrap"
                    "OpenGL stencil AOV still failed before rendercli started a GUI-capable application"
                    "" "${opengl_stencil_result}" "${opengl_stencil_stdout}"
                    "${opengl_stencil_stderr}")
  endif()
else()
  _rendercli_fail("rendercli --raster_backend gpu stencil AOV"
                  "OpenGL stencil AOV neither rendered nor reported a clear OpenGL capability error"
                  "" "${opengl_stencil_result}" "${opengl_stencil_stdout}"
                  "${opengl_stencil_stderr}")
endif()

set(opengl_shadow_render "${TEST_OUTPUT_DIR}/raster-opengl-shadow-maps.png")
execute_process(
  COMMAND
    "${RENDERCLI}" --engine raster --width 16 --height 12 --raster_backend gpu
    --shadow_maps
    "${matte_scene}" "${opengl_shadow_render}"
  RESULT_VARIABLE opengl_shadow_result
  OUTPUT_VARIABLE opengl_shadow_stdout
  ERROR_VARIABLE opengl_shadow_stderr
)
if(opengl_shadow_result STREQUAL "0")
  rendercli_assert_image_dimensions("${opengl_shadow_render}" 16 12
                                    NAME "rendercli --raster_backend gpu shadow maps dimensions")
  rendercli_assert_image_nonempty("${opengl_shadow_render}"
                                  NAME "rendercli --raster_backend gpu shadow maps pixels")
elseif(opengl_shadow_stderr MATCHES "OpenGL raster backend is selected")
  if(opengl_shadow_stderr MATCHES "QCoreApplication")
    _rendercli_fail("rendercli --raster_backend gpu shadow maps application bootstrap"
                    "OpenGL shadow-map plan still failed before rendercli started a GUI-capable application"
                    "" "${opengl_shadow_result}" "${opengl_shadow_stdout}"
                    "${opengl_shadow_stderr}")
  endif()
else()
  _rendercli_fail("rendercli --raster_backend gpu shadow maps"
                  "OpenGL shadow-map plan neither rendered nor reported a clear OpenGL capability error"
                  "" "${opengl_shadow_result}" "${opengl_shadow_stdout}"
                  "${opengl_shadow_stderr}")
endif()

set(opengl_object_id_render "${TEST_OUTPUT_DIR}/raster-opengl-object-id.png")
execute_process(
  COMMAND
    "${RENDERCLI}" --engine raster --width 32 --height 24 --raster_backend gpu
    --render_graph_view object_id
    "${static_scene}" "${opengl_object_id_render}"
  RESULT_VARIABLE opengl_object_id_result
  OUTPUT_VARIABLE opengl_object_id_stdout
  ERROR_VARIABLE opengl_object_id_stderr
)
if(opengl_object_id_result STREQUAL "0")
  rendercli_assert_image_dimensions("${opengl_object_id_render}" 32 24
                                    NAME "rendercli --raster_backend gpu object ID dimensions")
elseif(opengl_object_id_stderr MATCHES "OpenGL raster backend is selected")
  if(opengl_object_id_stderr MATCHES "QCoreApplication")
    _rendercli_fail("rendercli --raster_backend gpu object ID application bootstrap"
                    "OpenGL object ID AOV still failed before rendercli started a GUI-capable application"
                    "" "${opengl_object_id_result}" "${opengl_object_id_stdout}"
                    "${opengl_object_id_stderr}")
  endif()
else()
  _rendercli_fail("rendercli --raster_backend gpu object ID AOV"
                  "OpenGL object ID AOV neither rendered nor reported a clear OpenGL capability error"
                  "" "${opengl_object_id_result}" "${opengl_object_id_stdout}"
                  "${opengl_object_id_stderr}")
endif()

set(opengl_material_id_render "${TEST_OUTPUT_DIR}/raster-opengl-material-id.png")
execute_process(
  COMMAND
    "${RENDERCLI}" --engine raster --width 32 --height 24 --raster_backend gpu
    --render_graph_view material_id
    "${static_scene}" "${opengl_material_id_render}"
  RESULT_VARIABLE opengl_material_id_result
  OUTPUT_VARIABLE opengl_material_id_stdout
  ERROR_VARIABLE opengl_material_id_stderr
)
if(opengl_material_id_result STREQUAL "0")
  rendercli_assert_image_dimensions("${opengl_material_id_render}" 32 24
                                    NAME "rendercli --raster_backend gpu material ID dimensions")
  rendercli_assert_image_nonempty("${opengl_material_id_render}"
                                  NAME "rendercli --raster_backend gpu material ID pixels")
elseif(opengl_material_id_stderr MATCHES "OpenGL raster backend is selected")
  if(opengl_material_id_stderr MATCHES "QCoreApplication")
    _rendercli_fail("rendercli --raster_backend gpu material ID application bootstrap"
                    "OpenGL material ID AOV still failed before rendercli started a GUI-capable application"
                    "" "${opengl_material_id_result}" "${opengl_material_id_stdout}"
                    "${opengl_material_id_stderr}")
  endif()
else()
  _rendercli_fail("rendercli --raster_backend gpu material ID AOV"
                  "OpenGL material ID AOV neither rendered nor reported a clear OpenGL capability error"
                  "" "${opengl_material_id_result}" "${opengl_material_id_stdout}"
                  "${opengl_material_id_stderr}")
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

rendercli_expect_failure(
  NAME "rendercli rejects invalid --raster_backend"
  STDERR_MATCHES "Raster backend must be 'cpu', 'opengl', or 'gpu'"
  COMMAND
    "${RENDERCLI}" --engine raster --raster_backend metal
    "${matte_scene}" "${invalid_render}"
)

rendercli_expect_failure(
  NAME "rendercli rejects OpenGL raster backend with --direct_engine"
  STDERR_MATCHES "OpenGL raster backend is graph-backed"
  COMMAND
    "${RENDERCLI}" --engine raster --direct_engine --raster_backend opengl
    "${matte_scene}" "${invalid_render}"
)
