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
