cmake_minimum_required(VERSION 3.28)
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
option(RAYTRACER_BUILD_EXAMPLES    "Build the example applications"  ON)
option(RAYTRACER_BUILD_TOOLS       "Build the CLI tools"             ON)
option(RAYTRACER_ENABLE_ASAN       "Build with AddressSanitizer + UBSan" OFF)
option(RAYTRACER_ENABLE_COVERAGE   "Build with gcov instrumentation" OFF)
option(RAYTRACER_ENABLE_FUZZING    "Build LibFuzzer harnesses (needs Clang)" OFF)

# Match the warning flags from the Rakefile.
add_compile_options(-W -Wall -pedantic -Wno-extra-semi)

if(NOT (CMAKE_BUILD_TYPE STREQUAL "Debug" OR RAYTRACER_ENABLE_COVERAGE))
  add_compile_options(-O3 -funroll-loops -mtune=native)
endif()

if(RAYTRACER_ENABLE_ASAN)
  add_compile_options(-g -fno-inline -fsanitize=address -fsanitize=undefined)
  add_link_options(-fsanitize=address -fsanitize=undefined)
endif()

if(RAYTRACER_ENABLE_COVERAGE)
  add_compile_options(-O1 -fprofile-arcs -ftest-coverage)
  add_link_options(-fprofile-arcs -ftest-coverage)
endif()

# Find Qt 5; Qt 6 migration is modernize.md §3.10 / §5.
# QtScript is required by src/world/objects/ScriptedSurface.cpp and was
# removed in Qt 6 — porting that to QJSEngine is part of the Qt 6 migration.
# On macOS, auto-discover the Homebrew Qt 5 prefix so the presets work without
# per-developer -DCMAKE_PREFIX_PATH overrides.
if(APPLE AND NOT DEFINED CMAKE_PREFIX_PATH AND NOT DEFINED Qt5_DIR)
  execute_process(
    COMMAND brew --prefix qt@5
    OUTPUT_VARIABLE _raytracer_qt5_brew_prefix
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE _raytracer_brew_result
  )
  if(_raytracer_brew_result EQUAL 0 AND _raytracer_qt5_brew_prefix)
    list(APPEND CMAKE_PREFIX_PATH "${_raytracer_qt5_brew_prefix}")
  endif()
endif()

find_package(Qt5 5.15 REQUIRED COMPONENTS Core Gui Widgets Script)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_subdirectory(src)

if(RAYTRACER_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(RAYTRACER_BUILD_TOOLS)
  add_subdirectory(tools)
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
