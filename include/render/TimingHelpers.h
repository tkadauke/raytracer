#pragma once

#include <chrono>

namespace render::detail {
  inline double secondsBetween(std::chrono::steady_clock::time_point start,
                               std::chrono::steady_clock::time_point end) {
    return std::chrono::duration<double>(end - start).count();
  }
}
