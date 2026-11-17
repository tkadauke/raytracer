#pragma once

#include <string_view>

namespace render {
  namespace detail {
    [[nodiscard]] inline bool executionPathStartsWith(std::string_view path,
                                                      std::string_view prefix) noexcept {
      return path.size() >= prefix.size() && path.compare(0, prefix.size(), prefix) == 0;
    }
  }

  /**
    * Returns whether a backend execution-path label identifies GPU work.
    * Backend labels use a platform prefix, or the generic `gpu` label.
    */
  [[nodiscard]] inline bool executionPathUsesGpu(std::string_view path) noexcept {
    return path == "gpu" || detail::executionPathStartsWith(path, "metal") ||
           detail::executionPathStartsWith(path, "vulkan") ||
           detail::executionPathStartsWith(path, "full_gpu");
  }

  /**
    * Returns whether a diffuse-path-loop label identifies GPU work.
    * Platform names may be embedded in decorated compaction labels, while
    * the backend-neutral full-GPU label has one canonical spelling.
    */
  [[nodiscard]] inline bool diffusePathExecutionPathUsesGpu(std::string_view path) noexcept {
    return path == "full_gpu_subset" || path.find("metal") != std::string_view::npos ||
           path.find("vulkan") != std::string_view::npos;
  }
}
