include(FetchContent)

# Benchmarks build their own copy of Google Benchmark; tests still rely on
# the GoogleTest fetch from the top-level CMakeLists.txt.
set(BENCHMARK_ENABLE_TESTING       OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL       OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS   OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_WERROR        OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  benchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG        v1.8.3
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(benchmark)

file(GLOB BENCHMARK_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_executable(benchmarks ${BENCHMARK_SOURCES})
target_include_directories(benchmarks PRIVATE
  "${PROJECT_SOURCE_DIR}/src"
)
target_link_libraries(benchmarks PRIVATE
  raytracer
  benchmark::benchmark
  benchmark::benchmark_main
)
