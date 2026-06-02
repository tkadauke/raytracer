#include "render/RayFamilyQueuePolicy.h"

#include <algorithm>
#include <limits>

namespace render {
  RayFamilyQueuePolicy::RayFamilyQueuePolicy(int width, int height, int samplesPerPixel,
                                             int threads, int maximumQueueSize)
      : m_width(width),
        m_height(height),
        m_samplesPerPixel(samplesPerPixel),
        m_threads(threads),
        m_maximumQueueSize(maximumQueueSize) {
  }

  int RayFamilyQueuePolicy::queueSize() const {
    const int pixelSizedQueue = std::max(1, samplePixelCount() / 384);
    const int cappedQueue = std::min(std::max(1, m_maximumQueueSize), pixelSizedQueue);
    return std::max(std::max(1, m_threads), cappedQueue);
  }

  int RayFamilyQueuePolicy::samplePixelCount() const {
    const long long samplePixels = static_cast<long long>(std::max(0, m_width)) *
                                   static_cast<long long>(std::max(0, m_height)) *
                                   static_cast<long long>(std::max(1, m_samplesPerPixel));
    return static_cast<int>(std::min<long long>(samplePixels, std::numeric_limits<int>::max()));
  }
}
