#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>

namespace render::detail {
  inline std::uint64_t saturatedAdd(std::uint64_t a, std::uint64_t b) {
    if (b > std::numeric_limits<std::uint64_t>::max() - a) {
      return std::numeric_limits<std::uint64_t>::max();
    }
    return a + b;
  }

  template<typename Actual>
  inline void validateResultCount(Actual actual, std::uint64_t expected, const char* message) {
    if (static_cast<std::uint64_t>(actual) != expected) {
      throw std::logic_error(message);
    }
  }
}
