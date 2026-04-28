# raytracer

This is a raytracing library that is focused on speed and interactive rendering. It uses multiple threads to render parts of an image in parallel.

I started this project in my spare time in order to refresh my C++ skills and learn about the Google Test framework. Turns out you CAN write tests in C++ :-)

## prerequisites

To compile, you need:

* A C++17 compiler (`g++` or `clang++`)
* Ruby and `rake` (the build is driven by a `Rakefile`)
* Qt 5 — on macOS this is `brew install qt@5`

The `Rakefile` currently hardcodes a Homebrew Qt 5 prefix; you may need to adjust the `QT_BASE` constant at the top of the file to match your local install.

## compile

To build everything (examples, tools, and tests):

    rake

To build only the examples or only the CLI tools:

    rake examples
    rake tools

To run the test suites:

    rake test               # build + run unit and functional tests
    rake test:units         # only unit tests
    rake test:functionals   # only functional tests
    ONLY=PinholeCamera.* rake test:units   # filter via gtest

The most useful example to launch interactively is

    examples/SceneBrowser/SceneBrowser

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
