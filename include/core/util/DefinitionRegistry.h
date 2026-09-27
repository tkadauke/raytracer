#pragma once

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace core::util {

  // Looks up the first entry in a static registry of `const T*` definitions
  // matching `predicate`, or nullptr if none match.
  template<typename T, typename Predicate>
  const T* findDefinition(const std::vector<const T*>& all, Predicate&& predicate) {
    const auto it = std::find_if(all.begin(), all.end(), predicate);
    return it == all.end() ? nullptr : *it;
  }

  // Same as findDefinition, but throws std::runtime_error(errorMessage) instead
  // of returning nullptr when no entry matches.
  template<typename T, typename Predicate>
  const T& requireDefinition(const std::vector<const T*>& all, Predicate&& predicate,
                             const char* errorMessage) {
    const T* found = findDefinition(all, predicate);
    if (!found) {
      throw std::runtime_error(errorMessage);
    }
    return *found;
  }

}
