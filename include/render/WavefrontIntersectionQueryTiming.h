#pragma once

#include <string>

namespace render {
  struct WavefrontIntersectionQueryTiming {
    double uploadSeconds{0.0};
    double kernelSeconds{0.0};
    double readbackSeconds{0.0};
    std::string executionPath;

    void add(const WavefrontIntersectionQueryTiming& other) {
      uploadSeconds += other.uploadSeconds;
      kernelSeconds += other.kernelSeconds;
      readbackSeconds += other.readbackSeconds;
      recordExecutionPath(other.executionPath);
    }

    void recordExecutionPath(const std::string& path) {
      if (path.empty()) {
        return;
      }
      if (executionPath.empty() || executionPath == path) {
        executionPath = path;
        return;
      }
      executionPath = "mixed";
    }
  };
}
