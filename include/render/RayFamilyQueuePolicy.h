#pragma once

namespace render {
  class RayFamilyQueuePolicy {
  public:
    static constexpr int DefaultMaximumQueueSize = 300;

    RayFamilyQueuePolicy(int width, int height, int samplesPerPixel, int threads,
                         int maximumQueueSize = DefaultMaximumQueueSize);

    int queueSize() const;

  private:
    int samplePixelCount() const;

    int m_width;
    int m_height;
    int m_samplesPerPixel;
    int m_threads;
    int m_maximumQueueSize;
  };
}
