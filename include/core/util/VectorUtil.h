#pragma once

#include <cstddef>
#include <vector>

namespace core::util {

  template<typename T>
  T valueAt(const std::vector<T>& values, std::size_t index) {
    return index < values.size() ? values[index] : T{};
  }

}
