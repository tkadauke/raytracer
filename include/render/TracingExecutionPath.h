#pragma once

#include <string_view>

namespace render {
  /** Returns whether an execution-path label identifies GPU work. */
  [[nodiscard]] inline bool executionPathUsesGpu(std::string_view path) noexcept {
    const auto startsWith = [path](std::string_view prefix) {
      return path.size() >= prefix.size() && path.compare(0, prefix.size(), prefix) == 0;
    };

    return path == "gpu" || startsWith("metal") || startsWith("vulkan") || startsWith("full_gpu");
  }
}
