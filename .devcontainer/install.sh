#!/usr/bin/env bash
# Install the toolchain described in modernize.md §3.10. The current Rakefile
# build is macOS-only (uses -framework flags and DYLD_FRAMEWORK_PATH), so this
# container is primarily useful for linting (clang-format / clang-tidy /
# cppcheck) today and for the upcoming CMake + Qt 6 migration tomorrow. Ruby
# and rake are installed so `rake check:cpp` works in the meantime.
set -euo pipefail

export DEBIAN_FRONTEND=noninteractive

sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  cmake \
  ninja-build \
  clang-18 \
  clang-tidy-18 \
  clang-format-18 \
  cppcheck \
  doxygen \
  graphviz \
  lcov \
  gcovr \
  ruby \
  rake \
  qt6-base-dev \
  qt6-tools-dev \
  qt6-tools-dev-tools \
  libgl1-mesa-dev

sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-18 100
sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-18 100
sudo update-alternatives --install /usr/bin/clang-tidy clang-tidy /usr/bin/clang-tidy-18 100
sudo update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format-18 100

sudo apt-get clean
sudo rm -rf /var/lib/apt/lists/*
