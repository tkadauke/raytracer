# Vendored GoogleTest amalgamated source. Replacement with FetchContent
# (modernize.md §3.2 / §3.4) is a separate priority item.
add_library(gtest_vendored STATIC
  src/gtest-all.cpp
)
# The fused header lives at gtest/gtest.h. gtest-all.cpp includes
# "gtest/gtest.h", so the include path needs to be the parent directory
# (PROJECT_SOURCE_DIR). We also expose CMAKE_CURRENT_SOURCE_DIR so test code
# can use the simpler #include "gtest.h" form, matching the existing
# convention.
target_include_directories(gtest_vendored
  PUBLIC
    ${PROJECT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_options(gtest_vendored PRIVATE
  -Wno-pedantic
  -Wno-error
)

# Cppcheck-style alias so callers can write target_link_libraries(... gtest).
add_library(gtest ALIAS gtest_vendored)
