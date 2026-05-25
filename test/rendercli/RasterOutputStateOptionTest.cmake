if(NOT DEFINED RENDERCLI)
  message(FATAL_ERROR "RENDERCLI is required")
endif()

if(NOT DEFINED TEST_OUTPUT_DIR)
  message(FATAL_ERROR "TEST_OUTPUT_DIR is required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/RendercliTestHelpers.cmake")

file(REMOVE_RECURSE "${TEST_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${TEST_OUTPUT_DIR}")

set(scene "${PROJECT_SOURCE_DIR}/scenes/raster_material_preview.json")
set(invalid_render "${TEST_OUTPUT_DIR}/invalid.png")
set(alpha_func_error
    "Alpha function must be never, less, equal, less_equal, greater, greater_equal, not_equal, or always")

function(render_raster_state name output_path)
  rendercli_run(
    NAME "${name}"
    COMMAND
      "${RENDERCLI}" --engine raster --width 48 --height 32 ${ARGN}
      "${scene}" "${output_path}"
  )
  rendercli_assert_image_dimensions("${output_path}" 48 32 NAME "${name} dimensions")
endfunction()

set(baseline_render "${TEST_OUTPUT_DIR}/baseline.png")
render_raster_state("rendercli raster output-state baseline" "${baseline_render}")
rendercli_assert_image_nonempty("${baseline_render}" NAME "rendercli output-state baseline pixels")

foreach(mask IN ITEMS rgb r g b rg rb gb none)
  set(mask_render "${TEST_OUTPUT_DIR}/color-mask-${mask}.png")
  render_raster_state("rendercli --color_write_mask ${mask} is accepted"
                      "${mask_render}"
                      --color_write_mask "${mask}")
endforeach()
rendercli_assert_image_hash_differs(
  "${baseline_render}" "${TEST_OUTPUT_DIR}/color-mask-none.png"
  NAME "rendercli --color_write_mask none changes raster output")
rendercli_assert_image_hash_differs(
  "${baseline_render}" "${TEST_OUTPUT_DIR}/color-mask-r.png"
  NAME "rendercli single-channel color write mask changes raster output")

set(blend_render "${TEST_OUTPUT_DIR}/blend-source-alpha.png")
render_raster_state("rendercli --blend with source alpha factors is accepted"
                    "${blend_render}"
                    --blend --blend_src source_alpha --blend_dst one_minus_source_alpha)

set(constant_color_blend_render "${TEST_OUTPUT_DIR}/blend-constant-color.png")
render_raster_state("rendercli --blend with constant color factors is accepted"
                    "${constant_color_blend_render}"
                    --blend --blend_src constant_color --blend_dst one_minus_constant_color
                    --blend_constant_color 0.25,0.5,0.75)

set(constant_alpha_blend_render "${TEST_OUTPUT_DIR}/blend-constant-alpha.png")
render_raster_state("rendercli --blend with constant alpha factors is accepted"
                    "${constant_alpha_blend_render}"
                    --blend --blend_src constant_alpha --blend_dst one_minus_constant_alpha
                    --blend_constant_alpha 0.35)

foreach(blend_op IN ITEMS add subtract reverse_subtract min max)
  set(blend_op_render "${TEST_OUTPUT_DIR}/blend-op-${blend_op}.png")
  render_raster_state("rendercli --blend_op ${blend_op} is accepted"
                      "${blend_op_render}"
                      --blend --blend_src source_alpha --blend_dst one_minus_source_alpha
                      --blend_op "${blend_op}")
endforeach()

foreach(alpha_func IN ITEMS never less equal less_equal greater greater_equal not_equal always)
  set(alpha_render "${TEST_OUTPUT_DIR}/alpha-${alpha_func}.png")
  render_raster_state("rendercli --alpha_func ${alpha_func} is accepted"
                      "${alpha_render}"
                      --alpha_test --alpha_func "${alpha_func}" --alpha_ref 0.25)
endforeach()

set(viewport_render "${TEST_OUTPUT_DIR}/viewport.png")
render_raster_state("rendercli --viewport is accepted"
                    "${viewport_render}"
                    --viewport 8,4,24,20)
rendercli_assert_image_hash_differs(
  "${baseline_render}" "${viewport_render}"
  NAME "rendercli --viewport changes raster output")

set(scissor_render "${TEST_OUTPUT_DIR}/scissor.png")
render_raster_state("rendercli --scissor is accepted"
                    "${scissor_render}"
                    --scissor 10,6,22,18)
rendercli_assert_image_hash_differs(
  "${baseline_render}" "${scissor_render}"
  NAME "rendercli --scissor changes raster output")

set(depth_bias_render "${TEST_OUTPUT_DIR}/depth-bias.png")
render_raster_state("rendercli --depth_bias is accepted"
                    "${depth_bias_render}"
                    --depth_bias 0.0005)

rendercli_expect_failure(
  NAME "rendercli rejects invalid --color_write_mask"
  STDERR_MATCHES "Color write mask must contain only r, g, b, or be 'none'"
  COMMAND
    "${RENDERCLI}" --engine raster --color_write_mask rgba
    "${scene}" "${invalid_render}"
)

rendercli_expect_failure(
  NAME "rendercli rejects invalid --blend_src"
  STDERR_MATCHES "Source blend factor is not recognized"
  COMMAND
    "${RENDERCLI}" --engine raster --blend_src sideways
    "${scene}" "${invalid_render}"
)

rendercli_expect_failure(
  NAME "rendercli rejects invalid --blend_dst"
  STDERR_MATCHES "Destination blend factor is not recognized"
  COMMAND
    "${RENDERCLI}" --engine raster --blend_dst sideways
    "${scene}" "${invalid_render}"
)

rendercli_expect_failure(
  NAME "rendercli rejects invalid --blend_op"
  STDERR_MATCHES "Blend operation must be add, subtract, reverse_subtract, min, or max"
  COMMAND
    "${RENDERCLI}" --engine raster --blend_op multiply
    "${scene}" "${invalid_render}"
)

rendercli_expect_failure(
  NAME "rendercli rejects malformed --blend_constant_color"
  STDERR_MATCHES "Blend constant color must be three comma-separated values in 0..1"
  COMMAND
    "${RENDERCLI}" --engine raster --blend_constant_color 0.1,0.2
    "${scene}" "${invalid_render}"
)

rendercli_expect_failure(
  NAME "rendercli rejects out-of-range --blend_constant_color"
  STDERR_MATCHES "Blend constant color must be three comma-separated values in 0..1"
  COMMAND
    "${RENDERCLI}" --engine raster --blend_constant_color 0.1,1.2,0.3
    "${scene}" "${invalid_render}"
)

rendercli_expect_failure(
  NAME "rendercli rejects invalid --blend_constant_alpha"
  STDERR_MATCHES "Blend constant alpha must be a number from 0 to 1"
  COMMAND
    "${RENDERCLI}" --engine raster --blend_constant_alpha 1.2
    "${scene}" "${invalid_render}"
)

rendercli_expect_failure(
  NAME "rendercli rejects invalid --alpha_func"
  STDERR_MATCHES "${alpha_func_error}"
  COMMAND
    "${RENDERCLI}" --engine raster --alpha_func maybe
    "${scene}" "${invalid_render}"
)

rendercli_expect_failure(
  NAME "rendercli rejects invalid --alpha_ref"
  STDERR_MATCHES "Alpha reference must be a number from 0 to 1"
  COMMAND
    "${RENDERCLI}" --engine raster --alpha_ref -0.1
    "${scene}" "${invalid_render}"
)

rendercli_expect_failure(
  NAME "rendercli rejects malformed --viewport"
  STDERR_MATCHES "Viewport must be four comma-separated integers x,y,width,height with non-negative size"
  COMMAND
    "${RENDERCLI}" --engine raster --viewport 0,0,10
    "${scene}" "${invalid_render}"
)

rendercli_expect_failure(
  NAME "rendercli rejects negative-size --viewport"
  STDERR_MATCHES "Viewport must be four comma-separated integers x,y,width,height with non-negative size"
  COMMAND
    "${RENDERCLI}" --engine raster --viewport 0,0,-10,10
    "${scene}" "${invalid_render}"
)

rendercli_expect_failure(
  NAME "rendercli rejects malformed --scissor"
  STDERR_MATCHES "Scissor must be four comma-separated integers x,y,width,height with non-negative size"
  COMMAND
    "${RENDERCLI}" --engine raster --scissor 0,0,10
    "${scene}" "${invalid_render}"
)

rendercli_expect_failure(
  NAME "rendercli rejects negative-size --scissor"
  STDERR_MATCHES "Scissor must be four comma-separated integers x,y,width,height with non-negative size"
  COMMAND
    "${RENDERCLI}" --engine raster --scissor 0,0,10,-10
    "${scene}" "${invalid_render}"
)

rendercli_expect_failure(
  NAME "rendercli rejects invalid --depth_bias"
  STDERR_MATCHES "Depth bias must be a finite number"
  COMMAND
    "${RENDERCLI}" --engine raster --depth_bias nan
    "${scene}" "${invalid_render}"
)
