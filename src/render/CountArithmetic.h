#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace render::detail {
  inline std::uint64_t saturatedAdd(std::uint64_t a, std::uint64_t b) {
    if (b > std::numeric_limits<std::uint64_t>::max() - a) {
      return std::numeric_limits<std::uint64_t>::max();
    }
    return a + b;
  }

  inline std::uint64_t checkedAdd(std::uint64_t a, std::uint64_t b, const std::string& message) {
    if (a > std::numeric_limits<std::uint64_t>::max() - b) {
      throw std::overflow_error(message);
    }
    return a + b;
  }

  inline std::uint64_t checkedProduct(std::uint64_t count, std::uint64_t bytesPerItem,
                                      const std::string& message) {
    if (bytesPerItem != 0 && count > std::numeric_limits<std::uint64_t>::max() / bytesPerItem) {
      throw std::overflow_error(message);
    }
    return count * bytesPerItem;
  }

  template<typename Actual>
  inline void validateResultCount(Actual actual, std::uint64_t expected, const char* message) {
    if (static_cast<std::uint64_t>(actual) != expected) {
      throw std::logic_error(message);
    }
  }
}
