cmake_minimum_required(VERSION 3.25)
project(raytracer
  VERSION 0.1.0
  DESCRIPTION "Multithreaded raytracing library with interactive Qt viewer"
  LANGUAGES CXX
)

# C++17 today; C++23 is a separate roadmap item (modernize.md §3.1).
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(RAYTRACER_BUILD_TESTS       "Build the test suite"            ON)
option(RAYTRACER_BUILD_MODELER     "Build the Qt Modeler application" ON)
option(RAYTRACER_BUILD_TOOLS       "Build the CLI tools"             ON)
option(RAYTRACER_BUILD_BENCHMARKS  "Build the Google Benchmark microbenchmarks" OFF)
option(RAYTRACER_ENABLE_ASAN       "Build with AddressSanitizer + UBSan" OFF)
option(RAYTRACER_ENABLE_COVERAGE   "Build with gcov instrumentation" OFF)
option(RAYTRACER_ENABLE_FUZZING    "Build LibFuzzer harnesses (needs Clang)" OFF)
option(RAYTRACER_ENABLE_STATS      "Per-render performance counters (modernize.md §3.7)" OFF)
option(RAYTRACER_ENABLE_MUTATION   "Build with the mull mutation-testing pass plugin (closes #28)" OFF)
option(RAYTRACER_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
if(APPLE)
  set(_RAYTRACER_METAL_WAVEFRONT_DEFAULT ON)
else()
  set(_RAYTRACER_METAL_WAVEFRONT_DEFAULT OFF)
endif()
if(DEFINED RAYTRACER_ENABLE_VULKAN_WAVEFRONT AND RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
  set(_RAYTRACER_VULKAN_WAVEFRONT_REQUIRED ON)
else()
  set(_RAYTRACER_VULKAN_WAVEFRONT_REQUIRED OFF)
endif()
option(RAYTRACER_ENABLE_METAL_WAVEFRONT
       "Enable Metal wavefront and full-GPU path-loop backend plumbing (macOS only)"
       ${_RAYTRACER_METAL_WAVEFRONT_DEFAULT})
option(RAYTRACER_DISABLE_METAL_WAVEFRONT
       "Force-disable Metal wavefront and full-GPU path-loop backend plumbing on macOS" OFF)
option(RAYTRACER_ENABLE_VULKAN_WAVEFRONT
       "Enable Vulkan wavefront and full-GPU path-loop backend plumbing (Linux only)" OFF)
option(RAYTRACER_DISABLE_VULKAN_WAVEFRONT
       "Force-disable Vulkan wavefront and full-GPU path-loop backend plumbing on Linux" OFF)

if(APPLE)
  if(RAYTRACER_DISABLE_METAL_WAVEFRONT)
    set(RAYTRACER_ENABLE_METAL_WAVEFRONT OFF CACHE BOOL
        "Enable Metal wavefront and full-GPU path-loop backend plumbing (macOS only)" FORCE)
  elseif(NOT RAYTRACER_ENABLE_METAL_WAVEFRONT)
    message(STATUS
      "Enabling Metal wavefront/full-GPU path-loop backend for macOS; "
      "set RAYTRACER_DISABLE_METAL_WAVEFRONT=ON to opt out")
    set(RAYTRACER_ENABLE_METAL_WAVEFRONT ON CACHE BOOL
        "Enable Metal wavefront and full-GPU path-loop backend plumbing (macOS only)" FORCE)
  endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  if(RAYTRACER_DISABLE_VULKAN_WAVEFRONT)
    set(RAYTRACER_ENABLE_VULKAN_WAVEFRONT OFF CACHE BOOL
        "Enable Vulkan wavefront and full-GPU path-loop backend plumbing (Linux only)" FORCE)
  else()
    find_package(Vulkan QUIET)
    if(NOT Vulkan_GLSLC_EXECUTABLE)
      find_program(Vulkan_GLSLC_EXECUTABLE NAMES glslc)
    endif()
    if(Vulkan_FOUND AND Vulkan_GLSLC_EXECUTABLE)
      if(NOT RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
        message(STATUS
          "Enabling Vulkan wavefront/full-GPU path-loop backend for Linux; "
          "set RAYTRACER_DISABLE_VULKAN_WAVEFRONT=ON to opt out")
      endif()
      set(RAYTRACER_ENABLE_VULKAN_WAVEFRONT ON CACHE BOOL
          "Enable Vulkan wavefront and full-GPU path-loop backend plumbing (Linux only)" FORCE)
    elseif(_RAYTRACER_VULKAN_WAVEFRONT_REQUIRED)
      set(RAYTRACER_ENABLE_VULKAN_WAVEFRONT ON CACHE BOOL
          "Enable Vulkan wavefront and full-GPU path-loop backend plumbing (Linux only)" FORCE)
    else()
      if(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
        message(STATUS
          "Disabling Vulkan wavefront/full-GPU path-loop backend because Vulkan or glslc "
          "was not found; set RAYTRACER_ENABLE_VULKAN_WAVEFRONT=ON to require it")
      endif()
      set(RAYTRACER_ENABLE_VULKAN_WAVEFRONT OFF CACHE BOOL
          "Enable Vulkan wavefront and full-GPU path-loop backend plumbing (Linux only)" FORCE)
    endif()
  endif()
endif()

if(DEFINED ENV{GITHUB_ACTIONS})
  set(_RAYTRACER_NATIVE_TUNING_DEFAULT OFF)
else()
  set(_RAYTRACER_NATIVE_TUNING_DEFAULT ON)
endif()
option(RAYTRACER_ENABLE_NATIVE_TUNING "Tune release builds for the current CPU" ${_RAYTRACER_NATIVE_TUNING_DEFAULT})

# Match the warning flags from the Rakefile, and make new warnings fail the
# build so CI catches them at the point of introduction.
# -Wno-ignored-attributes: GCC emits this when __m128/__m128d are used as
# template type arguments (the SSE Vector specializations inherit from
# Vector<N, T, __m128d, ...>). The attribute propagation warning is a false
# positive for this intentional SIMD-specialization pattern.
set(_RAYTRACER_WARNING_FLAGS -W -Wall -pedantic -Wno-extra-semi -Wno-ignored-attributes)
if(RAYTRACER_WARNINGS_AS_ERRORS)
  list(APPEND _RAYTRACER_WARNING_FLAGS -Werror)
endif()
add_compile_options(${_RAYTRACER_WARNING_FLAGS})

# Enable x86 SSE3 intrinsics across every config so the legacy vector/color
# specializations under include/core/math/vector/sse3/ and
# include/core/color/sse3/ are actually selected. ARM NEON is exposed through
# the target's ABI/compiler feature macros and does not use this x86-only flag.
# -mtune=native alone only changes scheduling, not the available instruction
# set, so without this the specializations fall back to the generic templates
# and tests like Vector3Test.ShouldHaveRightSize fail on x86 runners that don't
# ship -msse3 in their default ABI baseline.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64|AMD64|i[3-6]86)")
  add_compile_options(-msse3)
endif()

if(NOT (CMAKE_BUILD_TYPE STREQUAL "Debug" OR RAYTRACER_ENABLE_COVERAGE))
  add_compile_options(-O3 -funroll-loops)
  if(RAYTRACER_ENABLE_NATIVE_TUNING)
    add_compile_options(-mtune=native)
  endif()
endif()

if(RAYTRACER_ENABLE_ASAN)
  add_compile_options(-g -fno-inline -fsanitize=address -fsanitize=undefined)
  add_link_options(-fsanitize=address -fsanitize=undefined)
endif()

if(RAYTRACER_ENABLE_COVERAGE)
  add_compile_options(-O1 -fprofile-arcs -ftest-coverage)
  add_link_options(-fprofile-arcs -ftest-coverage)
endif()

if(RAYTRACER_ENABLE_MUTATION)
  # mull is a Clang-frontend pass plugin that generates mutated copies of
  # the program at compile time; mull-runner then exercises them against
  # the existing test suite. Requires Clang plus the matching mull plugin
  # (see .github/workflows/mutation.yml for the canonical install).
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "RAYTRACER_ENABLE_MUTATION requires Clang (have ${CMAKE_CXX_COMPILER_ID})")
  endif()
  if(NOT DEFINED MULL_PLUGIN)
    message(FATAL_ERROR
      "RAYTRACER_ENABLE_MUTATION requires -DMULL_PLUGIN=/path/to/mull-ir-frontend.so. "
      "On Ubuntu the apt package installs it under /usr/lib/mull-ir-frontend-<clang>.so.")
  endif()
  # -fpass-plugin loads the mull instrumentation pass; -O0 keeps the IR
  # readable so mull's mutation operators (arith / cmp / boundary) have
  # something to chew on before the optimizer collapses them away.
  add_compile_options(-fpass-plugin=${MULL_PLUGIN} -O0 -g)
endif()

if(RAYTRACER_ENABLE_METAL_WAVEFRONT)
  if(NOT APPLE)
    message(FATAL_ERROR "RAYTRACER_ENABLE_METAL_WAVEFRONT is only supported on macOS")
  endif()
  enable_language(OBJCXX)
  set(CMAKE_OBJCXX_STANDARD 17)
  set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
endif()

if(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "RAYTRACER_ENABLE_VULKAN_WAVEFRONT is only supported on Linux")
  endif()
  find_package(Vulkan REQUIRED)
  if(NOT Vulkan_GLSLC_EXECUTABLE)
    find_program(Vulkan_GLSLC_EXECUTABLE NAMES glslc)
  endif()
  if(NOT Vulkan_GLSLC_EXECUTABLE)
    message(FATAL_ERROR
      "RAYTRACER_ENABLE_VULKAN_WAVEFRONT requires glslc so Vulkan compute "
      "shaders can be compiled into SPIR-V at build time")
  endif()
endif()

# Find Qt 6. QJSEngine (used by ScriptedSurface) lives in Qt6::Qml.
# On macOS, auto-discover the Homebrew Qt 6 prefix so the presets work without
# per-developer -DCMAKE_PREFIX_PATH overrides.
if(APPLE AND NOT DEFINED CMAKE_PREFIX_PATH AND NOT DEFINED Qt6_DIR)
  execute_process(
    COMMAND brew --prefix qt
    OUTPUT_VARIABLE _raytracer_qt6_brew_prefix
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE _raytracer_brew_result
  )
  if(_raytracer_brew_result EQUAL 0 AND _raytracer_qt6_brew_prefix)
    list(APPEND CMAKE_PREFIX_PATH "${_raytracer_qt6_brew_prefix}")
  endif()
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Qml OpenGL Network)
find_package(ZLIB REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_subdirectory(src)

if(RAYTRACER_BUILD_MODELER)
  add_subdirectory(src/modeler)
endif()

if(RAYTRACER_BUILD_TOOLS)
  add_subdirectory(tools)
endif()

if(RAYTRACER_BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()

if(RAYTRACER_ENABLE_FUZZING)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "RAYTRACER_ENABLE_FUZZING requires Clang (have ${CMAKE_CXX_COMPILER_ID})")
  endif()
  add_subdirectory(fuzz)
endif()

if(RAYTRACER_BUILD_TESTS)
  enable_testing()
  include(FetchContent)
  set(BUILD_GMOCK   ON  CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(googletest)
  add_subdirectory(test)
endif()
