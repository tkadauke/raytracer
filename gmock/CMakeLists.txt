add_library(gmock_vendored STATIC
  src/gmock-all.cpp
)
target_include_directories(gmock_vendored
  PUBLIC
    ${PROJECT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(gmock_vendored PUBLIC gtest_vendored)
target_compile_options(gmock_vendored PRIVATE
  -Wno-pedantic
  -Wno-error
)

add_library(gmock ALIAS gmock_vendored)

# gmock_main provides main() that calls testing::InitGoogleMock + RUN_ALL_TESTS.
# The Rakefile pulls it in by globbing **/*.cpp under gmock/.
add_library(gmock_main_vendored STATIC
  src/gmock_main.cpp
)
target_link_libraries(gmock_main_vendored PUBLIC gmock_vendored)
target_compile_options(gmock_main_vendored PRIVATE -Wno-pedantic -Wno-error)
