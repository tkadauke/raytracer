if(NOT DEFINED RENDERCLI)
  message(FATAL_ERROR "RENDERCLI is required")
endif()

if(NOT DEFINED TEST_OUTPUT_DIR)
  message(FATAL_ERROR "TEST_OUTPUT_DIR is required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/RendercliTestHelpers.cmake")

file(REMOVE_RECURSE "${TEST_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${TEST_OUTPUT_DIR}")

set(grouped_scene "${PROJECT_SOURCE_DIR}/test/fixtures/rendercli/grouped_steps.json")
set(single_render "${TEST_OUTPUT_DIR}/single-step.png")
set(cumulative_render "${TEST_OUTPUT_DIR}/cumulative-step.png")
set(sequence_render_pattern "${TEST_OUTPUT_DIR}/sequence-step-%02d.png")
set(sequence_step_0 "${TEST_OUTPUT_DIR}/sequence-step-00.png")
set(sequence_step_1 "${TEST_OUTPUT_DIR}/sequence-step-01.png")
set(sequence_step_2 "${TEST_OUTPUT_DIR}/sequence-step-02.png")

rendercli_run(
  NAME "rendercli --step single renders one grouped step"
  COMMAND
    "${RENDERCLI}" --width 48 --height 32 --step single:1
    "${grouped_scene}" "${single_render}"
)
rendercli_assert_image_dimensions("${single_render}" 48 32
                                  NAME "rendercli single step dimensions")
rendercli_assert_image_nonempty("${single_render}" NAME "rendercli single step pixels")

rendercli_run(
  NAME "rendercli --step cumulative renders grouped steps through index"
  COMMAND
    "${RENDERCLI}" --width 48 --height 32 --step cumulative:1
    "${grouped_scene}" "${cumulative_render}"
)
rendercli_assert_image_dimensions("${cumulative_render}" 48 32
                                  NAME "rendercli cumulative step dimensions")
rendercli_assert_image_nonempty("${cumulative_render}" NAME "rendercli cumulative step pixels")
rendercli_assert_image_hash_differs(
  "${single_render}" "${cumulative_render}"
  NAME "rendercli cumulative step changes grouped output")

rendercli_run(
  NAME "rendercli --step sequence renders grouped step sequence"
  COMMAND
    "${RENDERCLI}" --width 48 --height 32 --step sequence
    "${grouped_scene}" "${sequence_render_pattern}"
  STDOUT_MATCHES "step 1/3 number=0" "step 2/3 number=1" "step 3/3 number=2"
)
rendercli_assert_image_nonempty("${sequence_step_0}" NAME "rendercli sequence step 0 pixels")
rendercli_assert_image_nonempty("${sequence_step_1}" NAME "rendercli sequence step 1 pixels")
rendercli_assert_image_nonempty("${sequence_step_2}" NAME "rendercli sequence step 2 pixels")
rendercli_assert_image_hash_differs(
  "${sequence_step_0}" "${sequence_step_2}"
  NAME "rendercli sequence step outputs differ")

rendercli_expect_failure(
  NAME "rendercli rejects malformed --step selection"
  COMMAND
    "${RENDERCLI}" --step banana
    "${grouped_scene}" "${TEST_OUTPUT_DIR}/invalid-step.png"
  STDERR_MATCHES "Step selection must be"
)

rendercli_expect_failure(
  NAME "rendercli rejects missing --step sequence placeholder"
  COMMAND
    "${RENDERCLI}" --step sequence
    "${grouped_scene}" "${TEST_OUTPUT_DIR}/sequence.png"
  STDERR_MATCHES "Step sequence output must contain exactly one printf-style signed integer placeholder"
)

rendercli_expect_failure(
  NAME "rendercli rejects repeated --step sequence renders"
  COMMAND
    "${RENDERCLI}" --step sequence --repeat 2
    "${grouped_scene}" "${TEST_OUTPUT_DIR}/repeated-sequence-%02d.png"
  STDERR_MATCHES "Cannot combine --step sequence with --repeat"
)

rendercli_expect_failure(
  NAME "rendercli rejects unmatched --step selection"
  COMMAND
    "${RENDERCLI}" --step single:99
    "${grouped_scene}" "${TEST_OUTPUT_DIR}/missing-step.png"
  STDERR_MATCHES "Step selection matches no group with stepIndex metadata"
)
