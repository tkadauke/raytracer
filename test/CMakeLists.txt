# Test helpers — Signal/Slot have Q_OBJECT, AUTOMOC handles them.
file(GLOB TEST_HELPER_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/helpers/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/helpers/*.h"
)

add_library(test_helpers STATIC ${TEST_HELPER_SOURCES})
set_target_properties(test_helpers PROPERTIES AUTOMOC ON)
target_include_directories(test_helpers
  PUBLIC
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/test
)
target_link_libraries(test_helpers PUBLIC $<LINK_LIBRARY:WHOLE_ARCHIVE,raytracer> GTest::gmock)

# GoogleTest's TYPED_TEST_CASE / TEST_F / EXPECT_* macros are variadic with
# zero args at the call sites, which Clang flags under -Wpedantic. The macros
# are gtest's design, not ours; suppress only on the test targets.
set(_RAYTRACER_TEST_COMPILE_OPTIONS -Wno-gnu-zero-variadic-macro-arguments)
target_compile_options(test_helpers PRIVATE ${_RAYTRACER_TEST_COMPILE_OPTIONS})

# Two test executables matching the existing Rakefile split.
file(GLOB_RECURSE UNIT_TEST_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/unit/*.cpp"
)

file(GLOB_RECURSE FUNCTIONAL_TEST_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/functional/*.cpp"
)

add_executable(unit_tests ${UNIT_TEST_SOURCES})
set_target_properties(unit_tests PROPERTIES
  AUTOMOC ON
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/unit"
)

# ScriptedSurface.cpp uses #include "ScriptedSurface.moc" to compile
# ScriptElementRegistry's moc inline. AUTOMOC can't find a separate
# moc_ScriptedSurface.cpp in the raytracer build tree and regenerates
# one for unit_tests, which causes duplicate-symbol link failures when
# raytracer is linked with --whole-archive. Skipping AUTOMOC on the
# one test file that includes ScriptedSurface.h prevents this.
set_source_files_properties(
  "${CMAKE_CURRENT_SOURCE_DIR}/unit/world/objects/PrimitiveShapeTest.cpp"
  PROPERTIES SKIP_AUTOMOC ON
)
target_compile_options(unit_tests PRIVATE ${_RAYTRACER_TEST_COMPILE_OPTIONS})
target_link_libraries(unit_tests PRIVATE
  test_helpers
  GTest::gmock_main
)

add_executable(functional_tests ${FUNCTIONAL_TEST_SOURCES})
set_target_properties(functional_tests PROPERTIES
  AUTOMOC ON
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/functional"
)
target_compile_options(functional_tests PRIVATE ${_RAYTRACER_TEST_COMPILE_OPTIONS})
target_link_libraries(functional_tests PRIVATE
  test_helpers
  GTest::gmock_main
)

add_test(NAME unit_tests       COMMAND unit_tests)
add_test(NAME functional_tests COMMAND functional_tests)

# Functional tests load fixtures from the source tree at runtime.
set_tests_properties(unit_tests functional_tests PROPERTIES
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)
