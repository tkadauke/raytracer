# CLAUDE.md

This file provides guidance to Claude Code (and similar coding agents) when working in this repository.

## Project Overview

`raytracer` is a C++ raytracing library focused on speed and interactive rendering. It splits image rendering across multiple threads to render parts of an image in parallel and ships several runnable examples (including an interactive `SceneBrowser`) plus a CLI rendering tool. The library is templatized and built around pluggable cameras, materials, shapes, samplers, and view planes.

## Tech Stack

- C++17 (`g++` or `clang++`)
- Qt 5 (`QtCore`, `QtGui`, `QtWidgets`, `QtScript`) for widgets and example apps — Qt 6 migration is on the modernization roadmap (`docs/modernize.md` §3.10).
- CMake 3.28+ with Ninja as the primary build, driven by `CMakePresets.json`. The `Rakefile` is a thin layer of project utilities that wraps the CMake presets (`rake build` / `rake release` / `rake test`) and hosts the cppcheck, inline-method, line-stat, and Doxygen-image-render tasks.
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

The Rakefile is now a thin layer of project utilities: `rake build` / `rake release` wrap the CMake presets, `rake test` builds and runs the test suite, `rake docs:render` self-bootstraps a release build of rendercli to regenerate the Doxygen example images, and `rake check:cpp` / `check:inline` / `stats` round it out. All actual compilation lives under CMake.

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

**Performance-regression tests**: where ratios between alternatives are large (≥5×) and stable across debug/release/CI, encode them as ratio assertions in the regular unit-test suite — see `test/unit/render/primitives/BVHPerformanceTest.cpp` for the canonical pattern. Ratio-based assertions tolerate environmental noise (debug-vs-release, CI-vs-dev) far better than absolute timings, and run as part of `ctest` so they catch regressions automatically. Pair with the precision Google Benchmark for tuning. Don't add ratio tests where the expected ratio is small (<2×) — flaky failures will erode the suite's trust.

## CHANGELOG convention

Behavior-affecting changes — anything users (developers included) would notice — get a one-line entry in `CHANGELOG.md` under the `## Unreleased` heading. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) with `Added` / `Changed` / `Fixed` / `Deprecated` / `Removed` / `Security` subsections. Each entry ends with the author's attribution — for AI agents, that's the model name (e.g., `— Claude Opus 4.7`). Pure refactors that preserve behavior, internal-only test additions, and CI-only tooling tweaks don't need an entry.

This is the audit trail per `docs/modernize.md` §3.11 — agent-introduced changes should be greppable from the changelog.

## Roadmap convention

Whenever a PR / commit lands work that advances a bullet in `docs/roadmap.md`, mark that bullet in the same change. The convention (per §9 of the roadmap itself, "items completed get crossed off here") is:

- Strike through the part that's now done with `~~...~~`.
- Append `✅ **Done.** <one-line note with PR/commit ref>` — keep the why/where, not just the fact.
- For partial completions, mark the specific portion done and leave the remainder un-struck so it's still visible as TODO.

The same rule applies to `docs/modernize.md` for engineering-hygiene work. Both files are *living* — they describe the current state of the project, not a frozen plan from when they were written.

If a change ships something that isn't in the roadmap and probably should be, add the bullet first (in the right pillar/theme), then mark it done in the same PR. Don't let undocumented features accumulate.

**Don't let roadmap labels bleed into user-facing documentation.** Phrases like "pre-R1 behavior preserved" or "post-3.10 cleanup" are meaningless to a reader who hasn't read the roadmap. In docstrings, comments, and PR descriptions, describe the actual behavior ("pass-through, hard clamp at 1.0," "Qt 6 migration, QtScript→QJSEngine"). The CHANGELOG is the one place where roadmap references are appropriate — that's the audit trail linking commits to roadmap items, and entries there explicitly reference the section ("closes roadmap §3.R1").

## Interactive documentation widgets

Interactive JS widgets under `scripts/docs/*.js` should follow the rules in
`scripts/README.md`. The completed modernization plan is archived at
`docs/plans/complete/widgets.md`:

- Spatial values that move (points, vertices, edge endpoints, ray origins) get
  visible draggable handles at the thing being moved.
- Scalar values get labeled sliders or segmented controls.
- Do not add new whole-widget drag gestures; `DragHandler` is legacy-only during
  migration.
- Build new widgets on shared `figure.js` primitives before adding local raw-DOM
  infrastructure.
- Keep widget CSS scoped to a widget root or library-owned SVG class. Never add
  global SVG element rules that can leak across widgets.
- Use shared `figure.js` stroke-width constants instead of widget-local numeric
  `stroke-width` literals.
- Use US English spelling in widget labels, documentation, comments, tests, and
  changelog entries: "color", "behavior", "labeled", "gray".

## Adding a new visible-output feature

Cameras, materials, primitives, lights, textures, viewplanes — anything
whose effect is visible in a rendered image. This repo is an
education-focused codebase, so visible features are first-class
documentation surfaces; they aren't done when the math compiles.

The `ThinLensCamera` commit is the canonical worked example of this
checklist (see `git log --grep='ThinLensCamera'`). When you implement a
new one, expect to touch all four of these layers:

### 1. Two parallel class hierarchies

The library has a runtime side under `include/raytracer/` (used by the
renderer) and an editable-scene-graph side under `include/world/`
(used by the GUI editor). Most visible features need both:

- `include/raytracer/<group>/<Feature>.h` + matching `.cpp` — the
  math. Self-registers with the runtime factory (e.g.
  `CameraFactory::self().registerClass<...>("...")`).
- `include/world/objects/<Feature>.h` + matching `.cpp` — the
  editable wrapper. Inherits from the matching `world::` base, exposes
  parameters as `Q_PROPERTY` (so `PropertyEditorWidget` builds spinbox
  controls automatically), provides a `toRaytracer*()` factory that
  builds the runtime instance. Self-registers with `ElementFactory`.

If you skip the world wrapper, your feature is invisible in
`SceneBrowser` and `GeneratedRayTracer` — only `rendercli` against a
hand-edited JSON can reach it.

### 2. Tests on both sides

- Runtime side: pin the geometric / numerical invariants. For cameras:
  things like "all rays for a given pixel converge at the focal plane"
  or "direction is unit-length" — these are the load-bearing
  guarantees of the class. Use a deterministic-input overload (e.g.
  `rayForPixelWithLens(x, y, lensU, lensV)`) so the tests don't depend
  on a stochastic state.
- World side: pin the canned defaults, the property clamps, and the
  `toRaytracer*()` factory dispatch.
- Auto-install or auto-detect behavior gets BOTH branches tested
  (e.g. ThinLens auto-installs a sampler IF the incoming viewplane
  has the factory default; the test pins both "yes installs" and
  "no, leaves alone").

### 3. Documentation that earns the education label

Match the canonical placement in `SphericalCamera.h` / `PhongMaterial.h`:

- One **class-level** `@image html <feature>.png` showing the canonical
  default render.
- A **per-setter** `<table><tr>...<td>@image html ...</td>...</tr></table>`
  on each `setX()` docstring with 5 swept values. Don't dump all the
  tables at the class level — Doxygen renders each method's docstring
  as its own block, so per-setter placement is what makes "what does
  setApertureRadius do?" answerable visually in place.
- Same image references on **both** the raytracer-side and world-side
  headers. The wrappers are what users read when exploring in the GUI.
- A doc-render driver under `scripts/docs/<feature>.rb` that produces
  the referenced PNG filenames, plus any helper scenes added to
  `scripts/render_docs.rb` (e.g. `dof_scene` for ThinLens).
- If the math has a non-obvious geometric idea (one not visible in the
  rendered output), add a JS interactive widget under
  `scripts/docs/<feature>_<concept>.js` and embed via
  `@htmlonly <script src="..."></script> @endhtmlonly` at the matching
  spot in the header. Skip widgets where the rendered PNGs already
  show the effect — they'd just duplicate.
- Filenames in `@image html` must EXACTLY match what the doc-render
  script produces. Hard-code the float→string conversions in the .rb
  driver as strings (e.g. `radii = ["0.0", "0.2", ...]`) rather than
  computing via `(i * 0.1).to_s` — IEEE 754 round-trip artifacts like
  `0.30000000000000004` will break the references silently.

### 4. Empirical-testing surfaces in three example apps

Tests pin contracts but they don't let the user *see* the feature.
Wire it into:

- **`rendercli`** — automatic via the `ElementFactory` registration;
  the user can hand-edit a JSON scene with `"type": "<Feature>"` and
  render headlessly. Add a demo scene file under
  `examples/GeneratedRayTracer/scenes/<feature>_demo.json` so this
  path is one command away.
- **`GeneratedRayTracer`** — needs a menu entry. Add an action,
  `add<Feature>()` slot, header field, and menu wiring in
  `MainWindow.cpp`. The demo scene file from above also loads here
  via File → Open. The `Q_PROPERTY`s on the world wrapper auto-build
  spinbox controls in the right-side property editor.
- **`SceneBrowser`** — the camera/viewplane dropdown lists everything
  registered with the corresponding factory, so the new type
  auto-appears. If the feature has user-tunable parameters, register
  a sidebar editor with `<Feature>ParameterWidgetFactory` (see
  `PinholeCameraParameterWidget` for the pattern). For features that
  benefit from a custom built-in scene, add one under
  `examples/SceneBrowser/<Feature>Scene.cpp` registered via
  `SceneFactory`.

### 5. Verify visually as you go

Tests can pass and the rendered output can still be wrong (silhouettes
all black because of a Ruby DSL nesting mistake; speckled output
because of a sampler-clobber bug; focus on the wrong sphere because
of an eye-vs-camera-position semantic). Render the scene through each
of the three example apps before claiming done. Screenshots from a
real run are the load-bearing test for visible features.

### Don't silently override caller choices

If your new code has a sensible default for some setting (sampler
count, primitive density, …), check whether the caller already
supplied a non-default before applying yours. The canonical pattern
from `ThinLensCamera::setViewPlane`:

```cpp
if (viewPlane()->sampler()->numSamples() <= 1) {
  // factory default — install our better default
} else {
  // caller already chose; respect their choice
}
```

Test BOTH branches. The asymmetric class of bugs caught by this rule
is "the GUI's UI setting is silently ignored" — which tests against
the non-GUI flow won't notice.

### User-facing parameter names follow user mental models

When a user-facing parameter has both a "natural" interpretation and
a "math-side" interpretation, pick the natural one. ThinLensCamera's
`focalDistance` is measured from the user-facing camera position
(matching every photography app), not the internal eye/pinhole point
— even though the math is one term simpler if measured from the eye.
A thin layer of "translate from user model to math model" in the
implementation is fine; surfacing the math model in the API is not.

## Notes / Gotchas

- macOS auto-detects Homebrew Qt 5 via `brew --prefix qt@5`; other platforms need `-DCMAKE_PREFIX_PATH=/path/to/qt5` (Linux: `apt install qtbase5-dev qtscript5-dev`).
- Qt 5 widget tests construct `QApplication`; on Linux CI runners they need `xvfb-run` to provide a virtual display server.
- `RAYTRACER_ENABLE_FUZZING` requires Clang and the matching libFuzzer runtime. Apple Command Line Tools doesn't ship libFuzzer; install full Xcode or use `clang-18` from Homebrew.
- The PLY parser is the only untrusted-input surface in the library. Treat PLY files as untrusted; parsing errors must throw `PlyParseError` (or another `Exception`), never crash the process. The `fuzz_ply` harness exists to keep that invariant honest.
- The `Rakefile` no longer compiles anything itself; every build path shells out to `cmake --preset` under the hood.
- The most useful example to launch interactively is `examples/SceneBrowser/SceneBrowser` (built via either CMake or Rake).
