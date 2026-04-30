# CLAUDE.md

This file provides guidance to Claude Code (and similar coding agents) when working in this repository.

## Project Overview

`raytracer` is a C++ raytracing library focused on speed and interactive rendering. It splits image rendering across multiple threads to render parts of an image in parallel and ships several runnable examples (including an interactive `SceneBrowser`) plus a CLI rendering tool. The library is templatized and built around pluggable cameras, materials, shapes, samplers, and view planes.

## Tech Stack

- C++17 (`g++` or `clang++`)
- Qt 5 (`QtCore`, `QtGui`, `QtWidgets`, `QtScript`) for widgets and example apps — Qt 6 migration is on the modernization roadmap (`docs/modernize.md` §3.10).
- CMake 3.28+ with Ninja as the primary build, driven by `CMakePresets.json`. The `Rakefile` is still in tree but only builds the examples and CLI tool; tests, fuzzers, benchmarks, and the Docker image all live under CMake.
- GoogleTest + GoogleMock 1.14 via CMake `FetchContent` (no longer vendored).
- Doxygen for API docs, published to GitHub Pages on push to master.
- `cppcheck` for static analysis; `clang-format` and `clang-tidy` configs are checked in (`.clang-format`, `.clang-tidy`).
- `gcovr`/`lcov` for coverage; LibFuzzer harness for the PLY parser; Google Benchmark for SSE3 microbenchmarks.
- SSE3 optimizations on supported targets.

## Repository Structure

- `include/` — public headers, grouped by module
  - `core/` — `Color`, `Buffer`, math (`Vector`, `Ray`, `Rect`), geometry, formats, util
  - `raytracer/` — `Raytracer`, `State`, plus `cameras/`, `primitives/`, `materials/`, `lights/`, `samplers/`, `textures/`, `viewplanes/`, `brdf/`
  - `world/` — scene/world abstractions
  - `widgets/` — Qt widget headers
- `src/` — implementations mirroring the `include/` layout
- `test/` — `unit/`, `functional/`, `helpers/`, `mocks/`, `abstract/`, `fixtures/`
- `benchmarks/` — Google Benchmark microbenchmarks (off by default; `-DRAYTRACER_BUILD_BENCHMARKS=ON` or `cmake --preset benchmark`)
- `fuzz/` — LibFuzzer harnesses (off by default; needs Clang; `cmake --preset fuzz`)
- `examples/` — `SceneBrowser`, `DifferenceRayTracer`, `GeneratedRayTracer`, `RefractingRayTracer`
- `tools/rendercli/` — command-line renderer
- `scripts/` — Ruby/JS helpers, including `render_docs.rb` and scene scripts
- `Dockerfile`, `.devcontainer/` — container image (rendercli only) and dev container
- `.github/workflows/` — CI, CodeQL, Dependabot, Doxygen Pages
- `CMakeLists.txt`, `CMakePresets.json`, `Rakefile`, `Doxyfile`, `.cppchecksuppress`, `.clang-format`, `.clang-tidy` — top-level config

## Common Commands

Configure and build (release):

    cmake --preset release
    cmake --build --preset release

Other presets: `debug`, `asan` (ASan + UBSan), `coverage` (gcov), `fuzz` (LibFuzzer, Clang only), `benchmark`.

Run the test suite via CTest:

    ctest --preset release

Run a single test binary directly with a gtest filter:

    build/release/test/unit/unit_tests --gtest_filter=PinholeCamera.*
    build/release/test/functional/functional_tests --gtest_filter=Sphere*

Build only certain targets:

    cmake --build --preset release --target rendercli
    cmake --build --preset release --target SceneBrowser
    cmake --build --preset benchmark --target benchmarks

The Rakefile still builds examples and the CLI tool (`rake examples tools`) but no longer runs the test suite — that path was retired when the vendored gtest/gmock copies were removed in favor of the FetchContent build.

Static analysis (cppcheck) is still wired through Rake:

    rake check:cpp

Documentation:

    doxygen Doxyfile        # writes docs/html/

## Architecture & Conventions

- Headers live under `include/<module>/` and implementations under the matching `src/<module>/` path. Tests under `test/unit/<module>/` and `test/functional/` mirror the same tree.
- Public headers use `#pragma once` and forward-declare collaborators where possible (see `include/raytracer/Raytracer.h`).
- The library leans on templates (e.g. `Vector<T>`, `Buffer<T>`, `Colord`) and pluggable strategies — cameras, materials, primitives, samplers, and view planes are all interface-driven. The project's design philosophy is templates over virtual dispatch; do not introduce `virtual` hierarchies in the name of modernization.
- `Raytracer` owns a `Camera` (via `std::shared_ptr`) and a non-owning `Scene*`, exposes `render(Buffer<unsigned int>&)`, supports cancellation, and uses a pimpl (`struct Private`) to hide threading details. Other widget classes follow the same pimpl pattern; their destructors must be defined out-of-line in the matching `.cpp` so the `unique_ptr<Private>` deleter sees the complete type.
- Qt: AUTOMOC and AUTOUIC are enabled at the top level; do not edit moc/uic outputs in the build tree. Headers with `Q_OBJECT` must be self-sufficient (include `<QVariant>` etc. directly) — AUTOMOC compiles the moc output in its own translation unit and won't see the .cpp's includes.
- C++ standard: `-std=c++17` (release adds `-O3 -funroll-loops -mtune=native`; coverage drops to `-O1` plus gcov flags). Warning flags: `-W -Wall -pedantic -Wno-extra-semi`. C++23 upgrade is a roadmap item (`docs/modernize.md` §3.1).
- `cppcheck` suppressions are tracked in `.cppchecksuppress` (project-relative paths only).

## Testing conventions

- Framework: GoogleTest 1.14 + GMock, fetched via CMake `FetchContent`. Use `MOCK_METHOD(retval, name, (args), (specs))` — the deprecated `MOCK_METHODn` / `MOCK_CONST_METHODn` macros were removed.
- Two suites: `test/unit/` (`unit_tests` binary) and `test/functional/` (`functional_tests` binary). Functional tests load fixtures from `test/fixtures/` at runtime; CTest runs them with `WORKING_DIRECTORY` set to the project root.
- Test sources are `*Test.cpp`. Test helpers live in `test/helpers/`, mocks in `test/mocks/`, abstract base tests in `test/abstract/`, fixtures in `test/fixtures/`.
- Includes use the canonical form: `#include <gtest/gtest.h>`, `#include <gmock/gmock.h>`, `#include "test/mocks/MockPrimitive.h"`.
- Run a focused subset:
  ```
  build/release/test/unit/unit_tests --gtest_filter=Quartic*
  ```
- Machine-readable test inventory:
  ```
  ctest --preset release --show-only=json-v1 > tests.json
  ```

## Performance contract

The project's hot paths are the `Vector`, `Matrix`, and `Color` math primitives plus their SSE3 specializations under `include/core/math/vector/sse3/` and `include/core/color/sse3/`. Any change that touches those files (including refactors that look "obviously equivalent") must:

1. Run the relevant Google Benchmark suite before and after (`cmake --preset benchmark && cmake --build --preset benchmark --target benchmarks && ./build/benchmark/benchmarks/benchmarks`).
2. Include the before/after numbers in the PR description.
3. Verify the SSE3 specialization is still being selected by inspecting the generated assembly or using `__builtin_cpu_supports` in a sanity check.

The roadmap item under `docs/modernize.md` §3.4 is to grow the benchmark suite to cover all SSE3 hot paths; until then, any regression that the existing benchmarks miss is on the author of the change.

## Notes / Gotchas

- macOS auto-detects Homebrew Qt 5 via `brew --prefix qt@5`; other platforms need `-DCMAKE_PREFIX_PATH=/path/to/qt5` (Linux: `apt install qtbase5-dev qtscript5-dev`).
- Qt 5 widget tests construct `QApplication`; on Linux CI runners they need `xvfb-run` to provide a virtual display server.
- `RAYTRACER_ENABLE_FUZZING` requires Clang and the matching libFuzzer runtime. Apple Command Line Tools doesn't ship libFuzzer; install full Xcode or use `clang-18` from Homebrew.
- The PLY parser is the only untrusted-input surface in the library. Treat PLY files as untrusted; parsing errors must throw `PlyParseError` (or another `Exception`), never crash the process. The `fuzz_ply` harness exists to keep that invariant honest.
- The `Rakefile` is in retirement: it still builds examples and tools but does not build the test suite. Use the CMake build for everything else.
- The most useful example to launch interactively is `examples/SceneBrowser/SceneBrowser` (built via either CMake or Rake).
