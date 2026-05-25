if(NOT DEFINED RENDERCLI_SOURCE)
  message(FATAL_ERROR "RENDERCLI_SOURCE is required")
endif()

cmake_policy(SET CMP0057 NEW)

if(NOT DEFINED RENDERCLI_TEST_FILES)
  message(FATAL_ERROR "RENDERCLI_TEST_FILES is required")
endif()

set(allowlisted_options
  disable_feature
  no_render_graph
  sampler
)

file(STRINGS "${RENDERCLI_SOURCE}" rendercli_lines)

set(in_add_options FALSE)
set(current_entry "")
set(current_depth 0)
set(options "")
set(removed_add_options_list_open FALSE)

foreach(line IN LISTS rendercli_lines)
  if(NOT in_add_options)
    if(line MATCHES "parser\\.addOptions\\(")
      set(in_add_options TRUE)
    endif()
    continue()
  endif()

  if(line MATCHES "^ *\\}\\);")
    break()
  endif()

  set(entry_line "${line}")
  if(NOT removed_add_options_list_open)
    string(REGEX REPLACE "^( *)\\{" "\\1" entry_line "${entry_line}")
    set(removed_add_options_list_open TRUE)
  endif()

  string(STRIP "${entry_line}" stripped_line)
  if(current_entry STREQUAL "" AND NOT stripped_line MATCHES "^\\{")
    continue()
  endif()

  string(REGEX REPLACE "[^{]" "" opens "${entry_line}")
  string(LENGTH "${opens}" open_count)
  string(REGEX REPLACE "[^}]" "" closes "${entry_line}")
  string(LENGTH "${closes}" close_count)

  string(APPEND current_entry "${entry_line}\n")
  math(EXPR current_depth "${current_depth} + ${open_count} - ${close_count}")

  if(current_depth EQUAL 0)
    if(current_entry MATCHES "^ *\\{ *\\{([^}]*)\\}")
      set(alias_text "${CMAKE_MATCH_1}")
      string(REGEX MATCHALL "\"[^\"]+\"" aliases "${alias_text}")
      foreach(alias IN LISTS aliases)
        string(REGEX REPLACE "^\"|\"$" "" option "${alias}")
        string(LENGTH "${option}" option_length)
        if(option_length GREATER 1)
          list(APPEND options "${option}")
        endif()
      endforeach()
    elseif(current_entry MATCHES "^ *\\{ *\"([^\"]+)\"")
      set(option "${CMAKE_MATCH_1}")
      string(LENGTH "${option}" option_length)
      if(option_length GREATER 1)
        list(APPEND options "${option}")
      endif()
    endif()

    set(current_entry "")
  endif()
endforeach()

list(REMOVE_DUPLICATES options)
list(SORT options)

set(test_contents "")
foreach(test_file IN LISTS RENDERCLI_TEST_FILES)
  file(READ "${test_file}" content)
  string(APPEND test_contents "${content}\n")
endforeach()

set(unaccounted_options "")
foreach(option IN LISTS options)
  if(test_contents MATCHES "--${option}([^A-Za-z0-9_]|$)")
    message(STATUS "rendercli option --${option}: covered")
  elseif(option IN_LIST allowlisted_options)
    message(STATUS "rendercli option --${option}: allowlisted")
  else()
    message(STATUS "rendercli option --${option}: missing")
    list(APPEND unaccounted_options "${option}")
  endif()
endforeach()

if(unaccounted_options)
  list(JOIN unaccounted_options ", --" missing_text)
  message(FATAL_ERROR
    "rendercli options lack CMake test coverage or an allowlist entry: --${missing_text}")
endif()
