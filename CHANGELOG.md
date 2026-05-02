# Changelog

All notable behaviour-affecting changes to this project will be documented in
this file. The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

Each entry attributes the change to its author (a person or an AI agent name).
This convention exists so agent-driven changes can be audited end-to-end —
see `docs/modernize.md` §3.11 and `CLAUDE.md` for the rules.

## Unreleased

### Added

- `CMake 3.28` build with per-target preset (`debug`, `release`, `asan`, `coverage`, `fuzz`, `benchmark`) running alongside the legacy Rakefile. The Rakefile is now a thin wrapper around `cmake --preset` and hosts a few project-utility tasks (`check:cpp`, `check:inline`, `stats`, `docs:render`). — Claude Opus 4.7
- GitHub Actions CI matrix: Ubuntu 24.04 (gcc-13, clang-18) and macOS 14 (Apple Clang); separate jobs for ASan + UBSan, coverage (with a 60 % line-coverage CI floor), CodeQL, container-image build, the PLY LibFuzzer harness, and Doxygen → GitHub Pages. — Claude Opus 4.7
- LibFuzzer harness `fuzz/fuzz_ply.cpp` for the PLY parser (the only untrusted-input surface), gated on `RAYTRACER_ENABLE_FUZZING` and exercised in CI for 60 s on every push. — Claude Opus 4.7
- Google Benchmark microbenchmark suite under `benchmarks/` with a starter `VectorBenchmark.cpp` covering the Vector hot path; gated on `RAYTRACER_BUILD_BENCHMARKS`. — Claude Opus 4.7
- `Dockerfile` (multi-stage Ubuntu builder + distroless runtime) packaging the headless `rendercli`. CI builds the image on every push. — Claude Opus 4.7
- Devcontainer (`.devcontainer/`) targeting Ubuntu 24.04 with cmake / ninja / clang-18 / Qt 6 dev packages. — Claude Opus 4.7
- `.clang-format` and `.clang-tidy` configurations matching the existing style; lint enforcement remains advisory in CI until a bulk reformat lands. — Claude Opus 4.7
- Dependabot configuration for GitHub Actions and pre-commit weekly updates. — Claude Opus 4.7
- `.pre-commit-config.yaml` with generic hygiene hooks (trailing whitespace, EOF newline, merge-conflict marker, large-file, mixed-line-ending, yaml/json validity) plus `clang-format` in check-only mode (modernize.md §3.10). — Claude Opus 4.7
- Per-render performance counters gated on `RAYTRACER_ENABLE_STATS` (modernize.md §3.7). Thread-safe atomic increments (`memory_order_relaxed`) instrumenting `Sphere::intersect`/`intersects`, `BoundingBox::intersects`, and the `Grid` DDA traversal step; `Raytracer::render` resets at start and dumps a one-line JSON snapshot of the totals to `stderr` at end. Compile-time off by default — `RAYTRACER_STATS_INC` expands to `(void)0` so production builds carry zero overhead. — Claude Opus 4.7
- `.github/workflows/release.yml` cuts a GitHub release on `v*` tags: builds rendercli, generates an SPDX SBOM via syft (`anchore/sbom-action`), keyless-signs it with cosign through Sigstore OIDC, runs Trivy CVE scan on the SBOM (fails on HIGH/CRITICAL), and attaches the SBOM + `.sig` + `.cert` to the release. CI gains a per-commit SBOM artifact (`sbom` job) so reviewers can inspect the dependency surface without waiting for a tag (modernize.md §3.8 / closes #27). — Claude Opus 4.7
- Comprehensive shading-behaviour tests for `MatteMaterial`, `PhongMaterial`, `ReflectiveMaterial`, `TransparentMaterial` (#22). — Claude Opus 4.7
- Unit tests for `Grid` (#19, +13 tests), `Raytracer` orchestration (#20, +11 tests), and SSE3-vs-generic SIMD regression tests for the math primitives. — Claude Opus 4.7

### Changed

- `Factory<Base>::create()` now returns `std::unique_ptr<Base>` instead of a raw `Base*`, making the ownership transfer explicit at every call site (#17). — Claude Opus 4.7
- `Raytracer::setScene` and the constructors now take `std::shared_ptr<raytracer::Scene>`; the Raytracer co-owns its scene with callers, fixing the silent scene leak in `SceneBrowser`/`RenderWindow` on every scene change (#17). — Claude Opus 4.7
- `Element::addChild` / `insertChild` now have `std::unique_ptr<Element>` overloads alongside the raw-pointer ones, so adoption-from-factory paths are typed for ownership transfer while drag-and-drop re-parenting in `SceneModel` keeps using the raw form (#17). — Claude Opus 4.7
- `Singleton<T>` now uses the Meyers' singleton pattern (function-local static), guaranteed thread-safe per C++11 (#5). — Claude Opus 4.7
- `Factory::registerClass` now stores creators in `std::map<Identifier, std::unique_ptr<BaseCreator>>`, eliminating the leak when an id is registered twice (#10). — Claude Opus 4.7
- `Grid::intersect` and `Grid::intersects` now share a single template DDA traversal (`traverseGrid`); the per-method bodies dropped from ~200 lines each to a thin visitor lambda (#11). — Claude Opus 4.7
- Replaced vendored GoogleTest 1.7-era source with GoogleTest 1.14 via CMake `FetchContent`; deleted ~52,000 lines of in-tree gtest/gmock; migrated `MOCK_METHODn` macros to the modern `MOCK_METHOD` form. — Claude Opus 4.7

### Fixed

- **Numerical stability in `Quartic::solve`** — when the resolvent cubic returned a real root that landed just below zero due to FP rounding, the absolute-epsilon `isAlmostZero` check rejected it and the solver reported zero quartic roots. Use a tolerance scaled to the operand magnitudes; the `(1, -16, 86, -176, 105)` quartic and the four Torus tests it cascaded into now pass. — Claude Opus 4.7
- **`Quadric::solve` degenerate case** — when the leading coefficient is zero the equation is linear, not quadratic. The old code blindly divided by `2*a`, leaking a NaN that x86 happened to slip through `OpenCylinder`'s y-range check. Added an explicit linear branch (handles the cylinder-axis ray). — Claude Opus 4.7
- **Data race on `RenderTask::active`** — written by the worker thread, read by the main thread without synchronisation. Switched to `std::atomic<bool>` (#6). — Claude Opus 4.7
- **`QThreadPool` memory leak** — held as a raw pointer in `Raytracer::Private` with no destructor; switched to `std::unique_ptr<QThreadPool>` (#7). — Claude Opus 4.7
- **`OpenCylinder` zero-radius silent corruption** — `1.0 / radius` produced `+Infinity`, leaking into surface normals and corrupting all subsequent shading. Now throws `DivisionByZeroException` when constructed with `radius == 0` (#9). — Claude Opus 4.7
- **`Grid::setup` unchecked `dynamic_pointer_cast`** — capture the cast and assert non-null with an explicit message before dispatch; converts a silent null-deref into a debuggable assertion failure (#8). — Claude Opus 4.7
- **`Grid::setup` cube-root precision** — `pow(x, 0.3333333)` truncated to seven significant digits; switched to `std::cbrt` (#12). — Claude Opus 4.7
- **`Vector3<double>` SSE3 specialisation never selected on x86 CI** — release flags only had `-mtune=native`, not `-msse3`. Without `__SSE3__` the SSE3 specialisation was dropped and `Vector3<double>` was 24 bytes instead of 32. Added `-msse3` for x86 family CPUs in `CMakeLists.txt`. — Claude Opus 4.7
- **`SSE3 Vector3<double>` and `Vector4<double>` private ctor type typo** — second parameter was `__m128` (single-precision) instead of `__m128d`; Apple Clang silently coerced it but gcc-13 / clang-18 on Linux rejected the `m_vector[1] = vec1` assignment. — Claude Opus 4.7
- **Linux build under libstdc++**: 83 headers were missing transitive includes (`<memory>`, `<algorithm>`, `<string>`, `<list>`, `<vector>`, `<functional>`) that Apple libc++ pulls in implicitly. — Claude Opus 4.7
- **`__cxa_throw` ABI signature conflict on Linux** — libstdc++ and libc++abi forward-declare `__cxa_throw` with different second-parameter types. The override-based exception-backtrace mechanism wasn't called by anyone; deleted it and kept the SIGSEGV trap. — Claude Opus 4.7
- `random_shuffle` ambiguity under libstdc++: project's own `::random_shuffle` (in the global namespace) collided with libstdc++'s deprecated `std::random_shuffle`; qualified the call sites with `::`. — Claude Opus 4.7

### Removed

- Vendored `gtest/` and `gmock/` source trees (~52,000 lines), replaced with GoogleTest 1.14 via `FetchContent`. The Rakefile no longer builds the test suite — `cmake --preset release && ctest` does. — Claude Opus 4.7
- Custom `meta::StaticIf`, `meta::NullType`, `meta::IsNullType`, `meta::TypesEqual` templates — predated C++17 and were re-implementations of `std::conditional_t` / `void` / `std::is_same_v`. — Claude Opus 4.7
- The whole compile pipeline from the `Rakefile` (Qt path constants, `.moc`/`ui_*.h`/`.o` rules, header-dependency scanner, per-example link blocks) — replaced with thin `cmake --preset` wrappers. ~150 lines deleted. — Claude Opus 4.7
- 58 redundant `#include "<X>.moc"` lines from .cpp files — leftover from the old manual-moc workflow; AUTOMOC handles moc generation now. — Claude Opus 4.7
- Custom `Ray.cpp` (just two static-member specialisations); inlined into `Ray.h` as C++17 `inline` variables. — Claude Opus 4.7
