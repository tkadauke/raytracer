#pragma once

#include <optional>
#include <string>

namespace engine::graph {
  /**
    * User-facing tracing execution intent. This describes the broad CPU,
    * hybrid, GPU, or policy-selected execution preference and is separate from
    * narrower backend-service controls such as wavefront intersection backend.
    */
  enum class TracingExecutionPreference { Auto, CPU, Hybrid, GPU };

  [[nodiscard]] inline const char*
  tracingExecutionPreferenceName(TracingExecutionPreference preference) {
    switch (preference) {
    case TracingExecutionPreference::Auto:
      return "auto";
    case TracingExecutionPreference::CPU:
      return "cpu";
    case TracingExecutionPreference::Hybrid:
      return "hybrid";
    case TracingExecutionPreference::GPU:
      return "gpu";
    }
    return "auto";
  }

  [[nodiscard]] inline std::optional<TracingExecutionPreference>
  tracingExecutionPreferenceFromString(const std::string& value) {
    if (value == "auto")
      return TracingExecutionPreference::Auto;
    if (value == "cpu")
      return TracingExecutionPreference::CPU;
    if (value == "hybrid")
      return TracingExecutionPreference::Hybrid;
    if (value == "gpu")
      return TracingExecutionPreference::GPU;
    return std::nullopt;
  }
}
