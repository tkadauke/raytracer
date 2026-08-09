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
set_source_files_properties(
  "${CMAKE_CURRENT_SOURCE_DIR}/BVHPacketBenchmark.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/BatchedRayBenchmark.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/BoundingBoxBenchmark.cpp"
  PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON
)
set_target_properties(benchmarks PROPERTIES
  UNITY_BUILD ON
  UNITY_BUILD_BATCH_SIZE 4
)
target_include_directories(benchmarks PRIVATE
  "${PROJECT_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(benchmarks PRIVATE
  raytracer
  benchmark::benchmark
  benchmark::benchmark_main
)
