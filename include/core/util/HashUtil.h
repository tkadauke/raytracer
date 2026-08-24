#pragma once

#include <cstddef>

namespace core::util {

  /**
    * Folds @p value into @p seed using the boost::hash_combine formula.
    * Shared by the `std::hash` specializations for the math vector types,
    * `Quaternion`, `BoundingBox`, `Matrix`, and the raster tessellation /
    * MSAA cache-key hashers.
    */
  inline void hashCombine(std::size_t& seed, std::size_t value) noexcept {
    seed ^= value + std::size_t(0x9e3779b9) + (seed << 6) + (seed >> 2);
  }

}
