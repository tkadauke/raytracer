# Modernization Roadmap — 2026

> **Scope:** `tkadauke/raytracer` — a C++ raytracing library with a Qt-based interactive viewer.  
> **Date:** 2026-04-27  
> **Author:** Winston (green-acres modernization initiative)

---

## 1. Executive Summary

- **Migrate the build from a bespoke Rakefile to CMake 3.28+ with FetchContent** — the current Ruby/Rake build has hardcoded Homebrew paths, no cross-platform support, no package-manager integration, and cannot be consumed by IDEs or CI without friction.
- **Replace vendored gtest/gmock (circa 2005-era headers) with GoogleTest 1.14 via CMake FetchContent** — removes ~3 MB of checked-in test framework code, enables upstream bug fixes, and unlocks modern test runners.
- **Add a GitHub Actions CI pipeline** — there is zero CI today; every defect is detected manually. A working matrix build (GCC 13, Clang 18, macOS/arm64) with caching and coverage upload is achievable in a single sprint.
- **Harden the supply chain** — no Dependabot, no SBOM, no SAST, no secret scanning. Enable all four in one afternoon; they are free for public repos.
- **Replace Qt 4 with Qt 6** — Qt 4 reached end-of-life in December 2015. The codebase has already moved to Qt 5 paths in the Rakefile (`QT_BASE` points to a Qt5 Homebrew prefix); completing the move to Qt 6 eliminates a decade of unpatched CVEs.

---

## 2. Current State

| Dimension | Current |
|---|---|
| Language standard | C++17 (`-std=c++17` in Rakefile) |
| Compiler | `g++` (system default, no pinned version) |
| Build system | Ruby Rake with custom dependency tracking |
| CI | None |
| Test framework | GoogleTest + GoogleMock vendored directly into `gtest/` and `gmock/` directories (circa 2005 headers, no version tag) |
| Test count | 138 test files across unit and functional suites |
| GUI toolkit | Qt 5 (Rakefile references Qt 5.15.8 via Homebrew) — README still advertises Qt 4 |
| Static analysis | `cppcheck` (invoked manually via `rake check:cpp`); suppression list has hardcoded absolute macOS developer paths |
| Coverage | `lcov` + `genhtml`, manually invoked via `rake test:coverage` |
| Documentation | Doxygen (`Doxyfile` present, output not committed) |
| Container | None |
| SBOM / SCA | None |
| SAST | None |
| Dependabot | None |
| Secret scanning | None |
| pre-commit hooks | None |
| CLAUDE.md | Present and accurate |

**Key observations:**

1. The `cppcheck` suppression list embeds `/Users/tkadauke/code/raytracer/...` paths — it silently suppresses nothing on any other machine, defeating the purpose.
2. `meta::StaticIf` and `NullType` are custom re-implementations of `std::conditional` and `std::monostate`, both of which landed in C++17. They can be deleted.
3. SSE3 intrinsic paths in `include/core/math/vector/sse3/` and `include/core/color/sse3/` are gated on `#ifdef __SSE__` — correct, but they are only tested implicitly; there are no targeted SIMD regression tests.
4. There is only one commit in the visible log, so git history depth is shallow; this is likely a mirror rather than the full history.

---

## 3. Recommendations

### 3.1 Language / Compiler / Runtime

**Target:** C++23 with GCC 13 / Clang 18 as the minimum compiler toolchain.

**Why C++23 now:**
- `std::print`, `std::expected`, `std::mdspan`, and `std::flat_map` are all relevant to a rendering library.
- All three major compilers (GCC 13, Clang 17+, MSVC 19.38+) have sufficient C++23 support for a project of this size.

**Migration path:**

~~1. **`constexpr` / `noexcept` / `[[nodiscard]]` sweep of all math primitives** (`Vector`, `Matrix`, `Ray`, `BoundingBox`, `Quaternion`, `Range` + SSE3 specializations).~~ ✅ **Done.** All pure arithmetic operators and getters are `constexpr noexcept`; `sqrt`/`abs`/trig-calling methods are `noexcept`; all value-returning functions carry `[[nodiscard]]`; a compile-time `static_assert` block proves constexpr evaluation. SSE3 specializations get `noexcept` + `[[nodiscard]]` only (SIMD intrinsics not constexpr-eligible). See CHANGELOG.md for details.

~~1a. **Replace Meyer's-singleton static factories with `inline` variables** on all math types (`Vector2/3/4`, `Ray`, `BoundingBox` sentinels).~~ ✅ **Done.** `null()`, `one()`, `epsilon()`, `undefined()`, `minusInfinity()`, `plusInfinity()` on every vector type plus `Ray::undefined()` and `BoundingBox::undefined()`/`infinity()` converted from Meyer's-singleton functions to `inline const` (SSE3 types) or `inline constexpr` (generic templates) static data members. All ~258 call sites updated (parentheses removed). `HitPoint::undefined()` preserved. See CHANGELOG.md for details.

2. Change the Rakefile / future CMakeLists flag from `-std=c++17` to `-std=c++23`.
3. Delete `include/core/meta/StaticIf.h` and `include/core/meta/NullType.h`; replace all usages with `std::conditional_t` and `void`/`std::monostate`.
4. Replace manual `typedef` aliases throughout `include/` with `using` declarations.
5. Switch `DivisionByZeroException` to inherit from `std::exception` and use `std::expected<T, E>` at call sites where the current design throws into template-heavy paths (the renderer hot loop should never throw; push exceptions to the boundary).
6. Pin the compiler in CI: `gcc-13` and `clang-18` packages on Ubuntu 24.04.

**Minimum compiler matrix:**

| Compiler | Version | Rationale |
|---|---|---|
| GCC | 13 | Ships in Ubuntu 24.04 LTS |
| Clang | 18 | Ships in Ubuntu 24.04 LTS; best sanitizer support |
| Apple Clang | 15+ | Xcode 15, macOS 14+ |

---

### 3.2 Dependencies

**Current state:** No package manager. GoogleTest and GoogleMock are vendored as raw source. Qt is consumed from a hardcoded Homebrew prefix.

**Recommendations:**

1. **Remove `gtest/` and `gmock/` directories.** Replace with CMake `FetchContent`:

   ```cmake
   include(FetchContent)
   FetchContent_Declare(
     googletest
     GIT_REPOSITORY https://github.com/google/googletest.git
     GIT_TAG        v1.14.0   # pin by tag; rotate quarterly
   )
   FetchContent_MakeAvailable(googletest)
   ```

2. **Adopt vcpkg or Conan for Qt 6.** Both integrate cleanly with CMake and produce reproducible builds without hardcoded paths. vcpkg is recommended because of its first-class GitHub Actions integration and manifest mode (`vcpkg.json`).

   ```json
   {
     "name": "raytracer",
     "version": "0.1.0",
     "dependencies": [
       { "name": "qtbase", "version>=": "6.6.0" },
       { "name": "benchmark", "version>=": "1.8.3" }
     ]
   }
   ```

3. **Enable Dependabot for GitHub Actions action versions** (`.github/dependabot.yml`):

   ```yaml
   version: 2
   updates:
     - package-ecosystem: github-actions
       directory: /
       schedule:
         interval: weekly
     - package-ecosystem: vcpkg
       directory: /
       schedule:
         interval: weekly
   ```

4. **Fix the cppcheck suppression file.** Remove all absolute paths. Use project-relative paths or, better, move suppressions inline with `// cppcheck-suppress` comments at the point of suppression.

---

### 3.3 Build System

**Replace Rake with CMake 3.28.**

The Rake build is not portable (macOS-only path assumptions, Ruby dependency, manual `.moc`/`.uic` invocation). CMake is the industry standard for C++ libraries, is understood by CLion, VS Code CMake Tools, Xcode, and Visual Studio, and handles Qt's `AUTOMOC`/`AUTOUIC` automatically.

**Skeleton CMakeLists.txt top level:**

```cmake
cmake_minimum_required(VERSION 3.28)
project(raytracer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(RAYTRACER_BUILD_TESTS     "Build test suite"     ON)
option(RAYTRACER_BUILD_MODELER  "Build Modeler"        ON)
option(RAYTRACER_ENABLE_ASAN     "Enable AddressSanitizer" OFF)
option(RAYTRACER_ENABLE_COVERAGE "Enable coverage instrumentation" OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

add_subdirectory(src)
if(RAYTRACER_BUILD_MODELER)
  add_subdirectory(src/modeler)
endif()
if(RAYTRACER_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()
```

**Key CMake modernization points:**

- Use `target_compile_features(raytracer PUBLIC cxx_std_23)` — not global `CMAKE_CXX_FLAGS`.
- Expose the library as an `INTERFACE` target (header-only) since most logic lives in `include/` templates:
  ```cmake
  add_library(raytracer INTERFACE)
  target_include_directories(raytracer INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  )
  ```
- Use `cmake --preset` with `CMakePresets.json` for developer and CI configurations.
- Enable `CMAKE_EXPORT_COMPILE_COMMANDS=ON` in the debug preset for clang-tidy integration.

---

### 3.4 Testing

**Current state:** 138 test files, GoogleTest/GMock, two runners (unit, functional), manual `lcov` coverage. No coverage threshold enforcement. No mutation testing. No fuzz tests.

**Recommendations:**

1. **Coverage target: 80% line coverage, enforced in CI.**  
   Use `gcovr` (not `lcov`) — it produces Cobertura XML directly consumable by Codecov and GitHub Actions summary.

   ```bash
   gcovr --root . --exclude 'gtest/.*' --exclude 'gmock/.*' \
         --xml-pretty -o coverage.xml \
         --fail-under-line 80
   ```

2. **Add SIMD regression tests.** The SSE3 paths in `Vector3<float>`, `Vector4<float>`, etc. are not explicitly tested under the `#ifdef __SSE__` branch. Add parametric tests that exercise both the generic and SSE3 implementations with the same inputs.

3. **Fuzz the PLY parser.** The PLY format reader in `include/core/formats/ply/` parses external data — it is the highest-risk attack surface in the library. Add a LibFuzzer target:

   ```cmake
   if(RAYTRACER_ENABLE_FUZZING)
     add_executable(fuzz_ply fuzz/fuzz_ply.cpp)
     target_link_libraries(fuzz_ply PRIVATE raytracer)
     target_compile_options(fuzz_ply PRIVATE -fsanitize=fuzzer,address)
     target_link_options(fuzz_ply PRIVATE -fsanitize=fuzzer,address)
   endif()
   ```

4. **Adopt Google Benchmark for microbenchmarks.** The renderer has SSE3 hot paths with a clear performance contract. Add `benchmarks/` alongside `test/` to catch regressions:

   ```cmake
   FetchContent_Declare(benchmark
     GIT_REPOSITORY https://github.com/google/benchmark.git
     GIT_TAG        v1.8.3
   )
   ```

5. **Mutation testing with `mull-runner`** (LLVM-based). Run quarterly, not on every PR — it is slow but reveals gaps in test assertions. Target the `include/core/math/` directory first.

---

### 3.4.a Functional test infrastructure

**Current state:** `test/functional/` has 91 tests across 35 files driven by a custom Given/When/Then framework (`test/functional/support/FeatureTest.h` + the `GIVEN/WHEN/THEN` macros in `GivenWhenThen.h`). Reads like English; three rough edges:

1. **String-keyed step lookup at runtime.** Step names live in a `std::map<std::string, Step*>` populated at static-init time by `bool dummy = registerGiven(...)` initialisers. A typo or stale copy-paste prints `WARNING: 'given' step '...' is not defined!` to `stderr` but the `TEST_F` reports green. Real silent-failure path — invisible in CI summaries.
2. **Hardcoded to one engine.** `RaytracerFeatureTest::render()` always constructs `engine::raytracer::Raytracer`. The Wireframe engine has only unit tests today; future path tracer / software rasterizer / GL viewport will too unless the fixture grows engine pluralism.
3. **Hardcoded `redDiffuse` + `objectVisible/objectSize` semantics.** `objectVisible` literally counts red pixels in the buffer; tests can't easily assert non-red rendering. The `ShapeRecognition::recognizeCircle` heuristic is the only escape hatch and lives in `test/helpers/`.

**Coverage gaps from recent work:**

- `ThinLensCamera`, `TiltShiftCamera`, `EquirectangularCamera` — no functional tests (units exist).
- `MatteMaterial`, `PhongMaterial` — no functional tests (Reflective + Portal do).
- `LinearTonemap` / `ReinhardTonemap` / `AcesTonemap` — none.
- `JitteredSampler` / `RegularSampler` / `RandomSampler` — none at integration level.
- Wireframe engine — only unit tests, nothing at the scene-render level.
- `BSDF` interface (just landed, §3.R6 phase 1) — no integration smoke.
- ~~`PointLight` — no end-to-end shadow-boundary test.~~ ✅ **Done.** Functional test added in `test/functional/render/lights/PointLightTest.cpp` for PR #57.
- Layout drift: empty `test/functional/raytracer/` directory; `MinkowskiSumTest.cpp` is mis-filed under `steps/` despite being a test, not steps.

**Proposed sub-items**, ordered by dependency:

#### F. Layout cleanup ✅ **Done.**

- ~~Remove empty `test/functional/raytracer/` directory.~~ ✅ Removed.
- ~~Move `test/functional/steps/MinkowskiSumTest.cpp` into `test/functional/render/primitives/`.~~ ✅ The file was misnamed — it contained step definitions, not test cases (the real `MinkowskiSumTest.cpp` already lived at `render/primitives/`). Renamed to `MinkowskiSumSteps.cpp` to match its siblings (`BoxSteps.cpp`, `SphereSteps.cpp`, …).

#### A. ~~Replace string-keyed Given/When/Then with typed fixture methods~~ Cucumber-style regex steps with hard-fail on miss ✅ **Done.**

The original plan was to drop the macros and convert step bodies into named protected methods on the fixture (`givenCenteredSphere()`, `whenILookAtOrigin()`, …) so typos became compile errors. After review, pivoted to keep the natural-language registration style but make it parameterisable like Cucumber's Ruby DSL — patterns are now regular expressions, capture groups expose `const std::smatch& match` to the step body, and `given/when/then` calls that don't match exactly one registered pattern raise `GTEST_FAIL` instead of the old `cerr << "WARNING"` stderr line. Same diagnostic value as the typed-method variant (typos surface as failures, not silent passes); plus parameter-passing for free.

Existing 91 functional tests didn't need any migration — none of their step strings contain regex metacharacters, so they match as literals against the same input. New `FeatureTestSelfTest.cpp` pins the regex-capture, missing-step, and ambiguity behaviours (7 tests).

#### B. Parameterise the fixture over engine type *(~3 days)*

- Rename `RaytracerFeatureTest` to `EngineFeatureTest<Engine>`. Engine type becomes the template parameter; the constructor news up `std::make_shared<Engine>(scene, camera)` instead of hardcoded `Raytracer`.
- Existing tests where the assertion only makes sense for a raytracer — recursion, TIR, ambient lighting, soft shadows, recursive reflections, transparency — keep raytracer-specific assertions and explicit `EngineFeatureTest<Raytracer>` instantiation.
- Tests for *engine-agnostic* properties — primitive visibility, camera framing, view-frustum culling, per-primitive sphere/box/torus visibility — become typed tests via `TYPED_TEST_SUITE_P`, instantiated for both `Raytracer` and `Wireframe`.
- Assertion adapter: a virtual `objectVisible() / objectSize()` per engine. `Raytracer` overload counts shaded red pixels (current behaviour); `Wireframe` overload counts edge-colour pixels and runs `ShapeRecognition` for silhouette shape.

**Why:** "respect the new multi-engine world" concretely means a `Sphere` test should pass on Raytracer (red shaded sphere) AND Wireframe (recognisable circle of silhouette edges) without two parallel test files.

#### C. Wireframe functional coverage *(~1 day, builds on B)*

- Empty scene → buffer matches background everywhere.
- Sphere → `ShapeRecognition::recognizeCircle` passes on the silhouette.
- Box → recognisable rectangular outline.
- LOD knob: higher LOD strictly increases edge-pixel count for a Sphere (Box is LOD-invariant).
- Camera-frustum culling: behind-camera primitives produce no edges.
- Cancel-during-render: pre-cancel produces only the cleared background.

#### E. Cover the new-abstraction surface *(~3 days)*

- ~~**Matte + Phong materials** — full given/when/then coverage matching `ReflectiveMaterial`'s existing pattern.~~ ✅ **Done.** `MatteMaterialTest` pins texture-color passthrough, ambient-coefficient linearity, and the no-illumination contract; `PhongMaterialTest` pins the head-on specular highlight and its absence under matte.
- ~~**ThinLensCamera focus-plane invariant** — "a sphere on the focus plane is sharp; an off-plane sphere is blurred" via `ShapeRecognition` edge-pixel-density delta.~~ ✅ **Done.** `ThinLensCameraTest.FocalPlaneContractSharpVsBlurred` compares focused vs. defocused silhouette edge-transition counts.
- **TiltShiftCamera + EquirectangularCamera** — visibility + framing tests at parity with `PinholeCamera`.
- ~~**Tonemap monotonicity** — render the same HDR scene through `LinearTonemap` / `ReinhardTonemap` / `AcesTonemap`, assert the max LDR pixel value ordering across the built-in operators.~~ ✅ **Done.** PR #58 adds `TonemapMonotonicityTest` for an HDR Lambertian render; the shipped Narkowicz ACES fit has brighter midtones than Reinhard, so the pinned max-channel order is `Linear ≥ ACES ≥ Reinhard`.
- **Sampler determinism** — `RegularSampler` produces bit-identical output across runs for a deterministic scene; `JitteredSampler` produces statistically-uniform sub-pixel coverage (test the histogram, not the bytes); `RandomSampler` differs across runs at fixed seed only when re-seeded.
- ~~**PointLight shadow boundary** — shadow edge falls at the geometrically expected angle for a known-position light + occluder.~~ ✅ **Done.** PR #57 adds `PointLightTest.ShadowBoundaryFallsAtGeometricallyPredictedLocation`, sampling just inside and outside the tangent-predicted boundary.
- **BSDF integration smoke** — render the canonical Reflective + Transparent scenes via the post-§3.R6 paths, expect outputs identical to the pre-R6 baseline within sampler tolerance.

#### D. Reference-image regression tests *(~2 days, lands after B + §3.R6)*

- Curated scene set: `sphere_matte`, `sphere_phong`, `sphere_reflective`, `sphere_transparent_TIR`, `csg_difference`, `mesh_ply_bunny` (or smaller stand-in).
- Render through each engine that supports the scene (Wireframe skips transparent / TIR; Raytracer renders all).
- Diff against committed PNG using SSIM (or PSNR with an explicit threshold) — sampler stochasticity needs a tolerance, not exact equality. SSIM threshold should be tight enough to catch the kind of regression a refactor introduces (R6's BSDF dispatch shift would otherwise slip through). Implementations live in the `roadmap.md` §4.11.h (image quality metrics) library.
- `ctest` integration: failure emits the diff image to a CI artifact for triage.
- Lands AFTER §3.R6 because the BSDF refactor would otherwise churn the reference set during the refactor itself.

#### G. ~~Replace `ShapeRecognition` heuristic with a real classical-CV classifier~~ ✅ **Done.**

The old `test/helpers/ShapeRecognition` was a 1-D row-projection heuristic — it accepted diamonds as squares, lemons as circles, and the `recognizeRect` "all line lengths equal" check passed any vertically-symmetric blob. Replaced by three layered classical-CV primitives in `test/helpers/`:

1. ✅ **`Blob` + connected components** (BFS flood-fill) for cases that care about interior fill — `findAllBlobs` / `findLargestBlob`.
2. ✅ **`Silhouette` + outer-extreme extraction** (leftmost+rightmost per row, topmost+bottommost per column) for engine-agnostic shape descriptors — same value for a Raytracer-rendered solid disk and a Wireframe-rendered circle outline.
3. ✅ **`ShapeClassifier`** with `isCircle` and `isRectangle` predicates composed from `Silhouette` descriptors (radial variance, bounding-box aspect ratio).

Side effect of the replacement: nine functional tests that had been silently passing under the old "lemons are circles" heuristic became real failures and were corrected to assert visibility (`"i should see something"`) instead of asserting shape — fish-eye and spherical projections distort sphere silhouettes off-circle, portal-redirected rays show only fragments, convex hulls of side-by-side boxes are elongated hexagons, vertical cylinders have rectangular silhouettes, side-on tori are elongated rings. The shape claims in those tests were always wrong; the old classifier was just lying for them.

Hu moments, Fourier descriptors, Hough Circle Transform, and friends queued in roadmap §4.11.d for follow-up educational additions.

**Total:** ~11–12 days, interleaves with feature work. F → A → B unblocks C, E, D in parallel.

**Cross-references:**

- §3.R0 (`docs/roadmap.md`) covers the *unit*-test gaps in the integrator + materials + texture mappings. This sub-item is the *functional*-test counterpart and is independent.
- §3.R5b (recently completed) is what made multi-engine functional pluralism even possible — the `engine::raytracer::Raytracer` / `engine::wireframe::Wireframe` split removes the hardcoded raytracer assumption from the type system, so item B becomes a clean parameterisation rather than a refactor of cross-namespace dependencies.

---

### 3.5 CI/CD

**Current state:** No CI whatsoever.

**Target:** GitHub Actions with a build matrix, pinned action SHAs, caching, coverage upload, and artifact signing.

**Recommended workflow structure (`.github/workflows/ci.yml`):**

```yaml
name: CI
on:
  push:
    branches: [master]
  pull_request:

permissions:
  contents: read
  id-token: write   # for OIDC artifact signing

jobs:
  build:
    strategy:
      matrix:
        os: [ubuntu-24.04, macos-14]
        compiler: [gcc-13, clang-18]
        exclude:
          - os: macos-14
            compiler: gcc-13   # Apple toolchain only on macOS
    runs-on: ${{ matrix.os }}
    steps:
      - uses: actions/checkout@11bd71901bbe5b1630ceea73d27597364c9af683  # v4.2.2
      - uses: actions/cache@5a3ec84eff668545956fd18022155c47e93c3b21   # v4.2.3
        with:
          path: ~/.cache/vcpkg
          key: vcpkg-${{ runner.os }}-${{ hashFiles('vcpkg.json') }}
      - name: Install Qt6
        uses: jurplel/install-qt-action@v4
        with:
          version: 6.7.0
      - name: Configure
        run: cmake --preset ci-${{ matrix.compiler }}
      - name: Build
        run: cmake --build --preset ci-${{ matrix.compiler }} --parallel
      - name: Test
        run: ctest --preset ci-${{ matrix.compiler }} --output-on-failure
      - name: Upload coverage
        if: matrix.os == 'ubuntu-24.04' && matrix.compiler == 'clang-18'
        uses: codecov/codecov-action@v4
        with:
          files: build/coverage.xml
```

**Key CI hygiene rules:**

- **Pin all actions to full commit SHAs**, never floating tags (`@v4`). Renovate or Dependabot will update them automatically.
- Enable **OIDC-based artifact signing** with `sigstore/cosign-installer` instead of static signing keys.
- **Cache the vcpkg binary cache** using the `vcpkg.json` hash as the cache key.
- **Separate jobs for lint and build** — lint (`clang-tidy`, `clang-format --dry-run`) should fail fast without waiting for compilation.

---

### 3.6 Code Quality

**Recommendations:**

1. **clang-format.** Add a `.clang-format` file at the repo root. The current style is roughly LLVM-based with 2-space indentation. Enforce in CI:

   ```bash
   clang-format --dry-run --Werror $(find include src test -name '*.h' -o -name '*.cpp')
   ```

2. **clang-tidy.** Add `.clang-tidy` with these checks enabled as a minimum:

   ```yaml
   Checks: >
     clang-diagnostic-*,
     clang-analyzer-*,
     cppcoreguidelines-*,
     modernize-use-nullptr,
     modernize-use-override,
     modernize-use-using,
     modernize-loop-convert,
     readability-const-return-type,
     performance-unnecessary-copy-initialization
   ```

   The `meta::StaticIf` and `typedef` usage will generate actionable `modernize-use-using` and `modernize-use-std-conditional` warnings — use these as the migration backlog.

3. **Sanitizers in CI.** Add a dedicated sanitizer job:

   ```yaml
   - name: Build with ASan+UBSan
     run: cmake --preset sanitize && cmake --build --preset sanitize
   - name: Run sanitized tests
     run: ASAN_OPTIONS=detect_leaks=1 ctest --preset sanitize
   ```

   The SSE3 intrinsic paths should be tested under sanitizers — `__m128` alignment requirements have caught subtle bugs in similar libraries.

4. **Fix the `.cppchecksuppress` file.** Replace hardcoded absolute paths with project-relative paths. Until then, `cppcheck` is effectively disabled on any machine that is not the original author's laptop.

---

### 3.7 Observability

This is a library and offline renderer — traditional application observability (structured logging, traces) is largely not applicable. However:

1. **Rendering progress events.** The tile-based parallel renderer should expose a callback or observer interface for progress reporting. The current `Modeler` GUI polls render progress; an event-driven design makes headless rendering scriptable.

2. **Performance counters.** Add optional compile-time instrumentation (gated on `RAYTRACER_ENABLE_STATS`) to count ray-box, ray-sphere intersection calls, BVH traversal steps, and cache misses per render. Output as JSON to stderr. This is lightweight and enables regression detection without a full OpenTelemetry SDK.

3. **OpenTelemetry** is out of scope for a library of this nature. Skip it.

---

### 3.8 Security & Supply Chain

| Control | Tool | Action |
|---|---|---|
| Dependency updates | Dependabot (GitHub) | Enable via `.github/dependabot.yml` (see §3.2) |
| Secret scanning | GitHub built-in | Enable in repo Settings → Security — free, zero-config |
| SAST | CodeQL | Add `.github/workflows/codeql.yml` with `cpp` language |
| SBOM generation | `syft` v1+ | Add to release workflow: `syft . -o spdx-json > sbom.spdx.json` |
| Artifact signing | `cosign` + OIDC | Sign release archives; no static keys needed |
| Vulnerability scanning | `trivy` or `grype` | Scan the SBOM on every release |

**CodeQL workflow (minimal):**

```yaml
name: CodeQL
on:
  push:
    branches: [master]
  schedule:
    - cron: '0 6 * * 1'   # weekly on Monday

jobs:
  analyze:
    runs-on: ubuntu-24.04
    permissions:
      security-events: write
    steps:
      - uses: actions/checkout@11bd71901bbe5b1630ceea73d27597364c9af683
      - uses: github/codeql-action/init@v3
        with:
          languages: cpp
      - uses: github/codeql-action/autobuild@v3
      - uses: github/codeql-action/analyze@v3
```

**PLY parser risk note:** External mesh files are the only untrusted input surface. Until fuzz testing is in place (§3.4), document in `CLAUDE.md` that PLY files must be treated as untrusted and that parsing errors must not crash the process.

---

### 3.9 Container / Deployment

There is currently no container image or Dockerfile. If the goal is to make the renderer usable in cloud/CI environments without local Qt installs, add a multi-stage Dockerfile:

```dockerfile
# syntax=docker/dockerfile:1.7
FROM ubuntu:24.04 AS builder
RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake ninja-build git g++-13 libqt6-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake --preset release -G Ninja && cmake --build --preset release

# Headless-only image — Modeler excluded
FROM gcr.io/distroless/cc-debian12 AS runtime
COPY --from=builder /src/build/release/tools/rendercli/rendercli /usr/local/bin/rendercli
ENTRYPOINT ["/usr/local/bin/rendercli"]
```

**Key points:**

- Multi-stage build keeps the runtime image to ~20 MB (distroless, no shell).
- The GUI (`Modeler`) is intentionally excluded from the runtime image — it requires a display server and is not suitable for headless cloud use.
- Sign the image with `cosign` in the release workflow using OIDC (no key storage needed).
- Publish to `ghcr.io/tkadauke/raytracer:sha-<git-sha>` and `ghcr.io/tkadauke/raytracer:latest`.

---

### 3.10 Developer Experience

1. **Dev container (`.devcontainer/devcontainer.json`).** A pre-built dev container eliminates the "works on my Mac" Qt path problem entirely. Base on `mcr.microsoft.com/devcontainers/cpp:ubuntu-24.04`; install `qt6-base-dev`, `cmake`, `ninja-build`, `clang-18`, `clang-tidy-18`, `clang-format-18`.

2. **pre-commit hooks.** Add `.pre-commit-config.yaml`:

   ```yaml
   repos:
     - repo: https://github.com/pre-commit/mirrors-clang-format
       rev: v18.1.8
       hooks:
         - id: clang-format
           types_or: [c++, c]
     - repo: https://github.com/pocc/pre-commit-hooks
       rev: v1.3.5
       hooks:
         - id: clang-tidy
           args: [--fix-errors]
   ```

3. **Update the README.** The README still says "Qt4" and "MacPorts". Update it to reflect Qt 6, CMake, and the dev container path. Remove the outdated Rake instructions once CMake is the primary build.

4. **CMake presets (`CMakePresets.json`).** Provide named presets so `cmake --preset debug` and `cmake --preset release` work out of the box for new contributors. Include a `ci-clang-18` and `ci-gcc-13` preset consumed by GitHub Actions.

5. **docs site.** The Doxygen setup is in place but docs are never published. Add a GitHub Actions job that runs `doxygen` and deploys to GitHub Pages on every push to `master`:

   ```yaml
   - name: Deploy Doxygen to Pages
     uses: peaceiris/actions-gh-pages@v4
     with:
       github_token: ${{ secrets.GITHUB_TOKEN }}
       publish_dir: docs/html
   ```

---

### 3.11 AI / Agent Readiness

**CLAUDE.md is present and accurate.** This is the most important prerequisite for agent-driven development — most repos lack it entirely. The current file correctly:

- Describes the tech stack and directory layout
- Lists working build/test commands
- States what agents should avoid (adding CMake alongside Rake without consensus, vendoring more libs, adding Qt 4 code)
- Calls out SSE3 benchmark requirements

**Remaining gaps to address:**

1. **Add a `### Testing conventions` section to CLAUDE.md** explaining the unit/functional split and how to run a single test with `--gtest_filter`.

2. **Add an agent changelog convention.** Agents that modify behavior-affecting code should append a one-line entry to `CHANGELOG.md` (create the file) with the change summary and the model/agent name. This makes it easy to audit agent-introduced changes.

3. **Add a `### Performance contract` section** to CLAUDE.md explicitly stating that any change to the math primitives (`Vector`, `Matrix`, `Color`) requires a benchmark run before and after, and the results must be included in the PR description. This prevents silent regressions from agents that optimize for correctness only.

4. **Machine-readable test inventory.** Once CMake+CTest is in place, `ctest --show-only=json-v1 > tests.json` produces a JSON inventory of all tests. Agents can use this to scope their changes and verify coverage without running the full suite.

5. **Consider a `AGENTS.md`** (separate from `CLAUDE.md`) for non-Claude agents (Codex, Gemini, etc.) that do not read `CLAUDE.md` by convention. Mirror the same content; both files should be identical or `AGENTS.md` should `include` / link to `CLAUDE.md`.

---

## 4. Prioritized Roadmap

| Priority | Item | Effort | Impact |
|---|---|---|---|
| P0 | Migrate build to CMake 3.28 with FetchContent for GoogleTest | L (3–5 days) | Critical — unblocks all other work; enables CI, IDE support, sanitizers |
| P0 | Add GitHub Actions CI (build matrix + test run) | M (1–2 days) | Critical — zero CI today; every PR is untested |
| P0 | Fix `.cppchecksuppress` absolute paths | S (1 hour) | High — currently silently disabled on all machines except the author's |
| P0 | Enable GitHub secret scanning and CodeQL | S (2 hours) | High — free, zero-config, catches supply-chain issues |
| P1 | Replace `meta::StaticIf` / `NullType` with `std::conditional_t` | S (half day) | Medium — code simplification, demonstrates C++17→23 upgrade |
| P1 | Adopt `.clang-format` and enforce in CI | S (1 day) | High — prevents style drift in agent/contributor PRs |
| P1 | Add `.clang-tidy` configuration and lint job in CI | M (2 days) | High — surfaces modernization opportunities systematically |
| P1 | Add ASan + UBSan CI job | S (1 day) | High — SSE3 alignment bugs are silent without sanitizers |
| P1 | Replace vendored gtest/gmock with FetchContent | S (half day, after CMake migration) | Medium — removes 3 MB of stale headers |
| P1 | Add devcontainer definition | S (half day) | Medium — eliminates Qt path setup friction for new contributors |
| P1 | Update README (Qt 4 → Qt 6, Rake → CMake) | S (1 hour) | Medium — reduces contributor confusion |
| P1 | Enable Dependabot for GitHub Actions | S (30 min) | Medium — keeps action SHAs current automatically |
| P2 | Migrate from Qt 5.15 to Qt 6.7 | L (3–5 days) | High — Qt 5 EOL 2026; security exposure grows monthly |
| P2 | Add PLY fuzz target (LibFuzzer) | M (2 days) | High — only untrusted input surface |
| P2 | Add Google Benchmark microbenchmark suite | M (2–3 days) | Medium — codifies the SSE3 performance contract |
| P2 | Generate and publish SBOM (`syft`) on release | S (half day) | Medium — supply chain hygiene, increasingly required |
| P2 | Multi-stage Dockerfile + publish to ghcr.io | M (1–2 days) | Low-Medium — useful for headless/cloud rendering workflows |
| P2 | Doxygen → GitHub Pages publishing in CI | S (1 day) | Low — nice to have; docs already exist in Doxyfile |
| P2 | Mutation testing with `mull-runner` | L (3+ days) | Low — high effort, useful for identifying test gaps quarterly |

**Effort key:** S = small (<1 day), M = medium (1–3 days), L = large (3–5+ days)

---

## 5. Risks & Non-Goals

### Risks

- **CMake migration may break the macOS GUI workflow.** Qt's `AUTOMOC`/`AUTOUIC` in CMake handles `.moc` and `.uic` generation differently from the Rake `rule` blocks. Carefully test `Modeler` compilation before removing the Rake fallback. Run both in parallel for at least one sprint.
- **SSE3 paths are compiler and platform sensitive.** Any build system change must preserve `-msse3` (or equivalent) for the vector/color specializations. Verify with `static_assert(__builtin_cpu_supports("sse3"))` in a test, or accept the generic fallback.
- **Qt 5 → Qt 6 API breaks.** `QtScript` (listed in `QT_FRAMEWORKS` in the Rakefile) was removed in Qt 6. Any scene scripting that depends on it will need to migrate to `QJSEngine` or a non-Qt scripting layer.
- **`lcov` → `gcovr` migration.** Coverage reports generated by gcovr may differ slightly from existing lcov baselines. Establish a new baseline before enforcing a threshold.

### Non-Goals

- **Windows support.** There are no Windows contributors or Windows CI runners. Do not add Windows-specific CMake logic speculatively.
- **OpenTelemetry / distributed tracing.** This is a library, not a service. Structured logging and render-time counters (§3.7) are sufficient.
- **Complete rewrite in Rust or another language.** The C++ template/SSE3 design is intentional and performant. Modernize within the language, not away from it.
- **Runtime polymorphism refactor.** The project's explicit design philosophy is templates over virtual dispatch. Do not introduce `virtual` hierarchies in the name of modernization.
- **CI on every platform simultaneously.** Start with Ubuntu 24.04 only; add macOS arm64 once the CMake build is stable. FreeBSD and Windows can wait indefinitely.
