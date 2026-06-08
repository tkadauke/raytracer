#pragma once

namespace render {
  struct WavefrontIntersectionQueryTiming {
    double uploadSeconds{0.0};
    double kernelSeconds{0.0};
    double readbackSeconds{0.0};

    void add(const WavefrontIntersectionQueryTiming& other) {
      uploadSeconds += other.uploadSeconds;
      kernelSeconds += other.kernelSeconds;
      readbackSeconds += other.readbackSeconds;
    }
  };
}
