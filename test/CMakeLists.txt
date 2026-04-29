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
target_link_libraries(test_helpers PUBLIC raytracer GTest::gtest GTest::gmock)

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
target_link_libraries(unit_tests PRIVATE
  raytracer
  test_helpers
  GTest::gtest
  GTest::gmock
  GTest::gmock_main
)

add_executable(functional_tests ${FUNCTIONAL_TEST_SOURCES})
set_target_properties(functional_tests PROPERTIES
  AUTOMOC ON
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/functional"
)
target_link_libraries(functional_tests PRIVATE
  raytracer
  test_helpers
  GTest::gtest
  GTest::gmock
  GTest::gmock_main
)

add_test(NAME unit_tests       COMMAND unit_tests)
add_test(NAME functional_tests COMMAND functional_tests)

# Functional tests load fixtures from the source tree at runtime.
set_tests_properties(unit_tests functional_tests PROPERTIES
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)
