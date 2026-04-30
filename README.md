# raytracer

This is a raytracing library that is focused on speed and interactive rendering. It uses multiple threads to render parts of an image in parallel.

I started this project in my spare time in order to refresh my C++ skills and learn about the Google Test framework. Turns out you CAN write tests in C++ :-)

## prerequisites

To compile, you need:

* A C++17 compiler (`g++` or `clang++`)
* CMake 3.28+ and Ninja
* Qt 5 — on macOS, `brew install qt@5`; on Debian/Ubuntu, `apt install qtbase5-dev qtscript5-dev`

Ruby and `rake` are optional — they're only needed if you want to use the convenience wrappers documented below. The Rake tasks shell out to CMake under the hood; using `cmake` directly works just as well.

## compile

The everyday entry point builds everything and runs the test suite:

    rake

Other Rake wrappers:

    rake build       # cmake --preset debug && cmake --build --preset debug
    rake release     # cmake --preset release && cmake --build --preset release
    rake test        # rake build + ctest --preset debug --output-on-failure

To run the underlying CMake commands directly:

    cmake --preset release
    cmake --build --preset release
    ctest --preset release

To filter tests with gtest, run a test binary directly:

    build/release/test/unit/unit_tests --gtest_filter=PinholeCamera.*
    build/release/test/functional/functional_tests --gtest_filter=Sphere*

The most useful example to launch interactively is

    build/release/examples/SceneBrowser/SceneBrowser

Other useful Rake tasks: `rake docs:render` regenerates the Doxygen example images (auto-builds rendercli via CMake on first run); `rake check:cpp` runs cppcheck against the CMake `compile_commands.json`; `rake stats` counts test/code lines.

## features

* Templatized library
* Pluggable cameras:
  * Pinhole camera
  * Orthographic camera
  * Spherical camera
  * Fish eye camera
* Pluggable materials
  * Matte
  * Phong
  * Reflective
  * Transparent
  * Portal (like in the game)
* Pluggable view planes for different interactive experiences (mostly for fun)
* Pluggable shapes
  * Sphere, Box, Plane, Disk, Rectangle, Triangle, Mesh
* Shape compositions
  * Composition, Union, Intersection, Difference, Instancing
* Support for the PLY mesh format
* SSE3 optimizations

## hack

* Fork
* Use the coding style (a `.clang-format` is checked in)
* Write code
* Write tests
* Send a pull request

See [`CLAUDE.md`](CLAUDE.md) for repository conventions and common commands, and [`docs/modernize.md`](docs/modernize.md) for the 2026 modernization roadmap (CMake migration, Qt 6, GitHub Actions CI, supply-chain hardening).
