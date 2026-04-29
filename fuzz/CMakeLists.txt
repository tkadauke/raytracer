# LibFuzzer harness for the PLY parser. Requires Clang and the
# -fsanitize=fuzzer instrumentation, so it's gated behind a top-level
# option that defaults to OFF.
add_executable(fuzz_ply fuzz_ply.cpp)
target_link_libraries(fuzz_ply PRIVATE raytracer)
target_compile_options(fuzz_ply PRIVATE
  -g
  -O1
  -fsanitize=fuzzer,address,undefined
)
target_link_options(fuzz_ply PRIVATE
  -fsanitize=fuzzer,address,undefined
)
