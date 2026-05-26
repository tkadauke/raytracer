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

if(TARGET rendercli)
  add_executable(rendercli_image_probe rendercli/rendercli_image_probe.cpp)
  set_target_properties(rendercli_image_probe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/rendercli"
  )
  target_link_libraries(rendercli_image_probe PRIVATE Qt6::Gui)

  add_test(
    NAME rendercli_import
    COMMAND
      ${CMAKE_COMMAND}
      -DRENDERCLI=$<TARGET_FILE:rendercli>
      -DRENDERCLI_IMAGE_PROBE=$<TARGET_FILE:rendercli_image_probe>
      -DPROJECT_SOURCE_DIR=${PROJECT_SOURCE_DIR}
      -DTEST_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/rendercli-import
      -P ${CMAKE_CURRENT_SOURCE_DIR}/rendercli/ImportOptionTest.cmake
  )
  add_test(
    NAME rendercli_basic
    COMMAND
      ${CMAKE_COMMAND}
      -DRENDERCLI=$<TARGET_FILE:rendercli>
      -DRENDERCLI_IMAGE_PROBE=$<TARGET_FILE:rendercli_image_probe>
      -DPROJECT_SOURCE_DIR=${PROJECT_SOURCE_DIR}
      -DTEST_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/rendercli-basic
      -P ${CMAKE_CURRENT_SOURCE_DIR}/rendercli/BasicOptionTest.cmake
  )
  add_test(
    NAME rendercli_validation
    COMMAND
      ${CMAKE_COMMAND}
      -DRENDERCLI=$<TARGET_FILE:rendercli>
      -DRENDERCLI_IMAGE_PROBE=$<TARGET_FILE:rendercli_image_probe>
      -DPROJECT_SOURCE_DIR=${PROJECT_SOURCE_DIR}
      -DTEST_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/rendercli-validation
      -P ${CMAKE_CURRENT_SOURCE_DIR}/rendercli/ValidationOptionTest.cmake
  )
  add_test(
    NAME rendercli_animation
    COMMAND
      ${CMAKE_COMMAND}
      -DRENDERCLI=$<TARGET_FILE:rendercli>
      -DRENDERCLI_IMAGE_PROBE=$<TARGET_FILE:rendercli_image_probe>
      -DPROJECT_SOURCE_DIR=${PROJECT_SOURCE_DIR}
      -DTEST_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/rendercli-frame
      -P ${CMAKE_CURRENT_SOURCE_DIR}/rendercli/FrameOptionTest.cmake
  )
  add_test(
    NAME rendercli_render_graph
    COMMAND
      ${CMAKE_COMMAND}
      -DRENDERCLI=$<TARGET_FILE:rendercli>
      -DRENDERCLI_IMAGE_PROBE=$<TARGET_FILE:rendercli_image_probe>
      -DPROJECT_SOURCE_DIR=${PROJECT_SOURCE_DIR}
      -DTEST_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/rendercli-render-graph
      -P ${CMAKE_CURRENT_SOURCE_DIR}/rendercli/RenderGraphOptionTest.cmake
  )
  add_test(
    NAME rendercli_raster
    COMMAND
      ${CMAKE_COMMAND}
      -DRENDERCLI=$<TARGET_FILE:rendercli>
      -DRENDERCLI_IMAGE_PROBE=$<TARGET_FILE:rendercli_image_probe>
      -DPROJECT_SOURCE_DIR=${PROJECT_SOURCE_DIR}
      -DTEST_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/rendercli-raster
      -P ${CMAKE_CURRENT_SOURCE_DIR}/rendercli/RasterOptionTest.cmake
  )
  add_test(
    NAME rendercli_raytracer
    COMMAND
      ${CMAKE_COMMAND}
      -DRENDERCLI=$<TARGET_FILE:rendercli>
      -DRENDERCLI_IMAGE_PROBE=$<TARGET_FILE:rendercli_image_probe>
      -DPROJECT_SOURCE_DIR=${PROJECT_SOURCE_DIR}
      -DTEST_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/rendercli-raytracer
      -P ${CMAKE_CURRENT_SOURCE_DIR}/rendercli/RaytracerOptionTest.cmake
  )
  add_test(
    NAME rendercli_wireframe
    COMMAND
      ${CMAKE_COMMAND}
      -DRENDERCLI=$<TARGET_FILE:rendercli>
      -DRENDERCLI_IMAGE_PROBE=$<TARGET_FILE:rendercli_image_probe>
      -DPROJECT_SOURCE_DIR=${PROJECT_SOURCE_DIR}
      -DTEST_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/rendercli-wireframe
      -P ${CMAKE_CURRENT_SOURCE_DIR}/rendercli/WireframeOptionTest.cmake
  )
  add_test(
    NAME rendercli_raster_shadows
    COMMAND
      ${CMAKE_COMMAND}
      -DRENDERCLI=$<TARGET_FILE:rendercli>
      -DRENDERCLI_IMAGE_PROBE=$<TARGET_FILE:rendercli_image_probe>
      -DPROJECT_SOURCE_DIR=${PROJECT_SOURCE_DIR}
      -DTEST_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/rendercli-raster-shadows
      -P ${CMAKE_CURRENT_SOURCE_DIR}/rendercli/RasterShadowOptionTest.cmake
  )
  add_test(
    NAME rendercli_raster_output_state
    COMMAND
      ${CMAKE_COMMAND}
      -DRENDERCLI=$<TARGET_FILE:rendercli>
      -DRENDERCLI_IMAGE_PROBE=$<TARGET_FILE:rendercli_image_probe>
      -DPROJECT_SOURCE_DIR=${PROJECT_SOURCE_DIR}
      -DTEST_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/rendercli-raster-output-state
      -P ${CMAKE_CURRENT_SOURCE_DIR}/rendercli/RasterOutputStateOptionTest.cmake
  )
  add_test(
    NAME rendercli_cross_options
    COMMAND
      ${CMAKE_COMMAND}
      -DRENDERCLI=$<TARGET_FILE:rendercli>
      -DRENDERCLI_IMAGE_PROBE=$<TARGET_FILE:rendercli_image_probe>
      -DPROJECT_SOURCE_DIR=${PROJECT_SOURCE_DIR}
      -DTEST_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/rendercli-cross-options
      -P ${CMAKE_CURRENT_SOURCE_DIR}/rendercli/CrossOptionValidationTest.cmake
  )
  add_test(
    NAME rendercli_option_coverage_audit
    COMMAND
      ${CMAKE_COMMAND}
      -DRENDERCLI_SOURCE=${PROJECT_SOURCE_DIR}/tools/rendercli/rendercli.cpp
      "-DRENDERCLI_TEST_FILES=${CMAKE_CURRENT_SOURCE_DIR}/rendercli/BasicOptionTest.cmake;${CMAKE_CURRENT_SOURCE_DIR}/rendercli/ValidationOptionTest.cmake;${CMAKE_CURRENT_SOURCE_DIR}/rendercli/FrameOptionTest.cmake;${CMAKE_CURRENT_SOURCE_DIR}/rendercli/RenderGraphOptionTest.cmake;${CMAKE_CURRENT_SOURCE_DIR}/rendercli/RasterOptionTest.cmake;${CMAKE_CURRENT_SOURCE_DIR}/rendercli/RaytracerOptionTest.cmake;${CMAKE_CURRENT_SOURCE_DIR}/rendercli/WireframeOptionTest.cmake;${CMAKE_CURRENT_SOURCE_DIR}/rendercli/RasterShadowOptionTest.cmake;${CMAKE_CURRENT_SOURCE_DIR}/rendercli/RasterOutputStateOptionTest.cmake;${CMAKE_CURRENT_SOURCE_DIR}/rendercli/CrossOptionValidationTest.cmake;${CMAKE_CURRENT_SOURCE_DIR}/rendercli/ImportOptionTest.cmake"
      -P ${CMAKE_CURRENT_SOURCE_DIR}/rendercli/OptionCoverageAudit.cmake
  )
endif()

# Functional tests load fixtures from the source tree at runtime.
set_tests_properties(unit_tests functional_tests PROPERTIES
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)
