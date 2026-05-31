function(_rendercli_command_text output_variable)
  set(command_text "")
  foreach(argument IN LISTS ARGN)
    if(command_text STREQUAL "")
      set(command_text "${argument}")
    else()
      string(APPEND command_text " ${argument}")
    endif()
  endforeach()
  set(${output_variable} "${command_text}" PARENT_SCOPE)
endfunction()

function(_rendercli_fail name reason command result stdout stderr)
  set(message_text "${name}: ${reason}")
  if(NOT command STREQUAL "")
    string(APPEND message_text "\nCommand: ${command}")
  endif()
  if(NOT "${result}" STREQUAL "")
    string(APPEND message_text "\nExit code: ${result}")
  endif()
  if(NOT stdout STREQUAL "")
    string(APPEND message_text "\nstdout:\n${stdout}")
  endif()
  if(NOT stderr STREQUAL "")
    string(APPEND message_text "\nstderr:\n${stderr}")
  endif()
  message(FATAL_ERROR "${message_text}")
endfunction()

function(_rendercli_check_output name stream_name text regexes command result stdout stderr)
  foreach(regex IN LISTS ${regexes})
    if(NOT text MATCHES "${regex}")
      _rendercli_fail(
        "${name}"
        "${stream_name} did not match expected regex: ${regex}"
        "${command}"
        "${result}"
        "${stdout}"
        "${stderr}"
      )
    endif()
  endforeach()
endfunction()

function(_rendercli_set_parent variable value)
  if(NOT variable STREQUAL "")
    set(${variable} "${value}" PARENT_SCOPE)
  endif()
endfunction()

function(rendercli_run)
  set(one_value_args NAME RESULT_VARIABLE OUTPUT_VARIABLE ERROR_VARIABLE)
  set(multi_value_args COMMAND STDOUT_MATCHES STDERR_MATCHES)
  cmake_parse_arguments(ARG "" "${one_value_args}" "${multi_value_args}" ${ARGN})

  if(ARG_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "rendercli_run received unexpected arguments: ${ARG_UNPARSED_ARGUMENTS}")
  endif()
  if(NOT ARG_NAME)
    message(FATAL_ERROR "rendercli_run requires NAME")
  endif()
  if(NOT ARG_COMMAND)
    message(FATAL_ERROR "rendercli_run(${ARG_NAME}) requires COMMAND")
  endif()

  _rendercli_command_text(command_text ${ARG_COMMAND})
  execute_process(
    COMMAND ${ARG_COMMAND}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
  )

  if(NOT result STREQUAL "0")
    _rendercli_fail("${ARG_NAME}" "command failed" "${command_text}" "${result}" "${stdout}" "${stderr}")
  endif()

  _rendercli_check_output("${ARG_NAME}" stdout "${stdout}" ARG_STDOUT_MATCHES
                          "${command_text}" "${result}" "${stdout}" "${stderr}")
  _rendercli_check_output("${ARG_NAME}" stderr "${stderr}" ARG_STDERR_MATCHES
                          "${command_text}" "${result}" "${stdout}" "${stderr}")

  _rendercli_set_parent("${ARG_RESULT_VARIABLE}" "${result}")
  _rendercli_set_parent("${ARG_OUTPUT_VARIABLE}" "${stdout}")
  _rendercli_set_parent("${ARG_ERROR_VARIABLE}" "${stderr}")
endfunction()

function(rendercli_expect_failure)
  set(one_value_args NAME EXIT_CODE RESULT_VARIABLE OUTPUT_VARIABLE ERROR_VARIABLE)
  set(multi_value_args COMMAND STDOUT_MATCHES STDERR_MATCHES)
  cmake_parse_arguments(ARG "" "${one_value_args}" "${multi_value_args}" ${ARGN})

  if(ARG_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "rendercli_expect_failure received unexpected arguments: ${ARG_UNPARSED_ARGUMENTS}")
  endif()
  if(NOT ARG_NAME)
    message(FATAL_ERROR "rendercli_expect_failure requires NAME")
  endif()
  if(NOT ARG_COMMAND)
    message(FATAL_ERROR "rendercli_expect_failure(${ARG_NAME}) requires COMMAND")
  endif()

  _rendercli_command_text(command_text ${ARG_COMMAND})
  execute_process(
    COMMAND ${ARG_COMMAND}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
  )

  if(result STREQUAL "0")
    _rendercli_fail("${ARG_NAME}" "command unexpectedly succeeded" "${command_text}" "${result}"
                    "${stdout}" "${stderr}")
  endif()
  if(DEFINED ARG_EXIT_CODE AND NOT result STREQUAL "${ARG_EXIT_CODE}")
    _rendercli_fail("${ARG_NAME}" "command failed with an unexpected exit code"
                    "${command_text}" "${result}" "${stdout}" "${stderr}")
  endif()

  _rendercli_check_output("${ARG_NAME}" stdout "${stdout}" ARG_STDOUT_MATCHES
                          "${command_text}" "${result}" "${stdout}" "${stderr}")
  _rendercli_check_output("${ARG_NAME}" stderr "${stderr}" ARG_STDERR_MATCHES
                          "${command_text}" "${result}" "${stdout}" "${stderr}")

  _rendercli_set_parent("${ARG_RESULT_VARIABLE}" "${result}")
  _rendercli_set_parent("${ARG_OUTPUT_VARIABLE}" "${stdout}")
  _rendercli_set_parent("${ARG_ERROR_VARIABLE}" "${stderr}")
endfunction()

function(rendercli_assert_exists path)
  set(one_value_args NAME)
  cmake_parse_arguments(ARG "" "${one_value_args}" "" ${ARGN})
  if(ARG_NAME)
    set(name "${ARG_NAME}")
  else()
    set(name "artifact exists")
  endif()
  if(NOT EXISTS "${path}")
    _rendercli_fail("${name}" "expected file to exist: ${path}" "" "" "" "")
  endif()
endfunction()

function(rendercli_assert_not_exists path)
  set(one_value_args NAME)
  cmake_parse_arguments(ARG "" "${one_value_args}" "" ${ARGN})
  if(ARG_NAME)
    set(name "${ARG_NAME}")
  else()
    set(name "artifact does not exist")
  endif()
  if(EXISTS "${path}")
    _rendercli_fail("${name}" "expected file not to exist: ${path}" "" "" "" "")
  endif()
endfunction()

function(rendercli_assert_nonempty path)
  set(one_value_args NAME)
  cmake_parse_arguments(ARG "" "${one_value_args}" "" ${ARGN})
  if(ARG_NAME)
    set(name "${ARG_NAME}")
  else()
    set(name "artifact is non-empty")
  endif()
  rendercli_assert_exists("${path}" NAME "${name}")
  file(SIZE "${path}" file_size)
  if(file_size EQUAL 0)
    _rendercli_fail("${name}" "expected file to be non-empty: ${path}" "" "" "" "")
  endif()
endfunction()

function(_rendercli_probe_image path output_variable)
  set(one_value_args NAME)
  cmake_parse_arguments(ARG "" "${one_value_args}" "" ${ARGN})
  if(ARG_NAME)
    set(name "${ARG_NAME}")
  else()
    set(name "image probe")
  endif()
  if(NOT DEFINED RENDERCLI_IMAGE_PROBE)
    _rendercli_fail("${name}" "RENDERCLI_IMAGE_PROBE is required for image assertions" "" "" "" "")
  endif()

  rendercli_assert_exists("${path}" NAME "${name}")
  execute_process(
    COMMAND "${RENDERCLI_IMAGE_PROBE}" "${path}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
  )
  _rendercli_command_text(command_text "${RENDERCLI_IMAGE_PROBE}" "${path}")
  if(NOT result STREQUAL "0")
    _rendercli_fail("${name}" "image probe failed" "${command_text}" "${result}" "${stdout}" "${stderr}")
  endif()
  if(NOT stdout MATCHES "^width=([0-9]+) height=([0-9]+) nonzero_pixels=([0-9]+) unique_colors=([0-9]+)( [a-z_]+=[0-9]+)* hash=([0-9a-f]+)$")
    _rendercli_fail("${name}" "image probe printed an unexpected format" "${command_text}"
                    "${result}" "${stdout}" "${stderr}")
  endif()

  set(${output_variable} "${stdout}" PARENT_SCOPE)
endfunction()

function(_rendercli_probe_value probe_output key output_variable)
  if(NOT probe_output MATCHES "(^| )${key}=([^ ]+)")
    _rendercli_fail("image probe" "probe output did not contain ${key}: ${probe_output}" "" "" "" "")
  endif()
  set(${output_variable} "${CMAKE_MATCH_2}" PARENT_SCOPE)
endfunction()

function(rendercli_probe_image path)
  set(one_value_args NAME OUTPUT_VARIABLE WIDTH_VARIABLE HEIGHT_VARIABLE NONZERO_PIXELS_VARIABLE UNIQUE_COLORS_VARIABLE WARM_PIXELS_VARIABLE COOL_PIXELS_VARIABLE NEUTRAL_PIXELS_VARIABLE HASH_VARIABLE)
  cmake_parse_arguments(ARG "" "${one_value_args}" "" ${ARGN})
  if(ARG_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "rendercli_probe_image received unexpected arguments: ${ARG_UNPARSED_ARGUMENTS}")
  endif()

  _rendercli_probe_image("${path}" probe_output NAME "${ARG_NAME}")
  _rendercli_probe_value("${probe_output}" "width" width)
  _rendercli_probe_value("${probe_output}" "height" height)
  _rendercli_probe_value("${probe_output}" "nonzero_pixels" nonzero_pixels)
  _rendercli_probe_value("${probe_output}" "unique_colors" unique_colors)
  _rendercli_probe_value("${probe_output}" "warm_pixels" warm_pixels)
  _rendercli_probe_value("${probe_output}" "cool_pixels" cool_pixels)
  _rendercli_probe_value("${probe_output}" "neutral_pixels" neutral_pixels)
  _rendercli_probe_value("${probe_output}" "hash" hash)

  if(NOT ARG_OUTPUT_VARIABLE STREQUAL "")
    set(${ARG_OUTPUT_VARIABLE} "${probe_output}" PARENT_SCOPE)
  endif()
  if(NOT ARG_WIDTH_VARIABLE STREQUAL "")
    set(${ARG_WIDTH_VARIABLE} "${width}" PARENT_SCOPE)
  endif()
  if(NOT ARG_HEIGHT_VARIABLE STREQUAL "")
    set(${ARG_HEIGHT_VARIABLE} "${height}" PARENT_SCOPE)
  endif()
  if(NOT ARG_NONZERO_PIXELS_VARIABLE STREQUAL "")
    set(${ARG_NONZERO_PIXELS_VARIABLE} "${nonzero_pixels}" PARENT_SCOPE)
  endif()
  if(NOT ARG_UNIQUE_COLORS_VARIABLE STREQUAL "")
    set(${ARG_UNIQUE_COLORS_VARIABLE} "${unique_colors}" PARENT_SCOPE)
  endif()
  if(NOT ARG_WARM_PIXELS_VARIABLE STREQUAL "")
    set(${ARG_WARM_PIXELS_VARIABLE} "${warm_pixels}" PARENT_SCOPE)
  endif()
  if(NOT ARG_COOL_PIXELS_VARIABLE STREQUAL "")
    set(${ARG_COOL_PIXELS_VARIABLE} "${cool_pixels}" PARENT_SCOPE)
  endif()
  if(NOT ARG_NEUTRAL_PIXELS_VARIABLE STREQUAL "")
    set(${ARG_NEUTRAL_PIXELS_VARIABLE} "${neutral_pixels}" PARENT_SCOPE)
  endif()
  if(NOT ARG_HASH_VARIABLE STREQUAL "")
    set(${ARG_HASH_VARIABLE} "${hash}" PARENT_SCOPE)
  endif()
endfunction()

function(rendercli_assert_image_dimensions path expected_width expected_height)
  set(one_value_args NAME)
  cmake_parse_arguments(ARG "" "${one_value_args}" "" ${ARGN})
  if(ARG_NAME)
    set(name "${ARG_NAME}")
  else()
    set(name "image dimensions")
  endif()

  rendercli_probe_image("${path}" NAME "${name}" WIDTH_VARIABLE width HEIGHT_VARIABLE height
                        OUTPUT_VARIABLE probe_output)
  if(NOT width STREQUAL "${expected_width}" OR NOT height STREQUAL "${expected_height}")
    _rendercli_fail("${name}"
                    "expected ${expected_width}x${expected_height}, got ${width}x${height}: ${path}"
                    "" "" "${probe_output}" "")
  endif()
endfunction()

function(rendercli_assert_image_nonempty path)
  set(one_value_args NAME)
  cmake_parse_arguments(ARG "" "${one_value_args}" "" ${ARGN})
  if(ARG_NAME)
    set(name "${ARG_NAME}")
  else()
    set(name "image has nonzero pixels")
  endif()

  rendercli_probe_image("${path}" NAME "${name}" NONZERO_PIXELS_VARIABLE nonzero_pixels
                        OUTPUT_VARIABLE probe_output)
  if(nonzero_pixels EQUAL 0)
    _rendercli_fail("${name}" "expected image to contain at least one nonzero RGB pixel: ${path}"
                    "" "" "${probe_output}" "")
  endif()
endfunction()

function(rendercli_assert_image_varied path)
  set(one_value_args NAME)
  cmake_parse_arguments(ARG "" "${one_value_args}" "" ${ARGN})
  if(ARG_NAME)
    set(name "${ARG_NAME}")
  else()
    set(name "image has varied colors")
  endif()

  rendercli_probe_image("${path}" NAME "${name}" UNIQUE_COLORS_VARIABLE unique_colors
                        OUTPUT_VARIABLE probe_output)
  if(unique_colors LESS 2)
    _rendercli_fail("${name}" "expected image to contain more than one color: ${path}"
                    "" "" "${probe_output}" "")
  endif()
endfunction()

function(_rendercli_image_hash path output_variable name)
  rendercli_probe_image("${path}" NAME "${name}" HASH_VARIABLE hash)
  set(${output_variable} "${hash}" PARENT_SCOPE)
endfunction()

function(rendercli_assert_image_hash_equals expected_path actual_path)
  set(one_value_args NAME)
  cmake_parse_arguments(ARG "" "${one_value_args}" "" ${ARGN})
  if(ARG_NAME)
    set(name "${ARG_NAME}")
  else()
    set(name "image hashes match")
  endif()

  _rendercli_image_hash("${expected_path}" expected_hash "${name}")
  _rendercli_image_hash("${actual_path}" actual_hash "${name}")
  if(NOT expected_hash STREQUAL actual_hash)
    _rendercli_fail("${name}"
                    "expected image hashes to match: ${expected_path}=${expected_hash}, ${actual_path}=${actual_hash}"
                    "" "" "" "")
  endif()
endfunction()

function(rendercli_assert_image_hash_differs expected_path actual_path)
  set(one_value_args NAME)
  cmake_parse_arguments(ARG "" "${one_value_args}" "" ${ARGN})
  if(ARG_NAME)
    set(name "${ARG_NAME}")
  else()
    set(name "image hashes differ")
  endif()

  _rendercli_image_hash("${expected_path}" expected_hash "${name}")
  _rendercli_image_hash("${actual_path}" actual_hash "${name}")
  if(expected_hash STREQUAL actual_hash)
    _rendercli_fail("${name}"
                    "expected image hashes to differ, but both were ${expected_hash}: ${expected_path}, ${actual_path}"
                    "" "" "" "")
  endif()
endfunction()

function(rendercli_assert_files_differ expected_path actual_path)
  set(one_value_args NAME)
  cmake_parse_arguments(ARG "" "${one_value_args}" "" ${ARGN})
  if(ARG_NAME)
    set(name "${ARG_NAME}")
  else()
    set(name "files differ")
  endif()

  rendercli_assert_exists("${expected_path}" NAME "${name}")
  rendercli_assert_exists("${actual_path}" NAME "${name}")

  execute_process(
    COMMAND ${CMAKE_COMMAND} -E compare_files "${expected_path}" "${actual_path}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
  )
  _rendercli_command_text(command_text ${CMAKE_COMMAND} -E compare_files
                          "${expected_path}" "${actual_path}")
  if(result STREQUAL "0")
    _rendercli_fail("${name}" "files were unexpectedly identical" "${command_text}"
                    "${result}" "${stdout}" "${stderr}")
  endif()
endfunction()
