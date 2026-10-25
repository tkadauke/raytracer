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

  inline std::uint64_t saturatedProduct(std::uint64_t lhs, std::uint64_t rhs) {
    if (lhs != 0 && rhs > std::numeric_limits<std::uint64_t>::max() / lhs) {
      return std::numeric_limits<std::uint64_t>::max();
    }
    return lhs * rhs;
  }

  inline std::uint64_t checkedAdd(std::uint64_t a, std::uint64_t b, const std::string& message) {
    if (a > std::numeric_limits<std::uint64_t>::max() - b) {
      throw std::overflow_error(message);
    }
    return a + b;
  }

  inline std::uint32_t checkedU32(std::uint64_t value, const char* label) {
    if (value > std::numeric_limits<std::uint32_t>::max()) {
      throw std::overflow_error(std::string(label) + " exceeds GPU 32-bit count range");
    }
    return static_cast<std::uint32_t>(value);
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
