#pragma once

#include <cstdint>

namespace core {
  [[nodiscard]] inline constexpr int countSetBits(std::uint16_t mask) noexcept {
    int count = 0;
    while (mask != 0) {
      mask &= static_cast<std::uint16_t>(mask - 1u);
      ++count;
    }
    return count;
  }
}
