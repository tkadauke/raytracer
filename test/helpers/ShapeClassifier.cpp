#include "test/helpers/ShapeClassifier.h"

#include "test/helpers/Silhouette.h"

#include <cmath>

namespace testing {
  bool ShapeClassifier::isCircle(const Buffer<unsigned int>& buffer) const {
    const auto silhouette = extractSilhouette(buffer, m_targetColor);
    if (silhouette.sampleCount() < 16) return false;

    const double aspect = silhouette.aspectRatio();
    if (std::abs(aspect - 1.0) > 0.20) return false;

    return silhouette.radialVariance() < 0.10;
  }

  bool ShapeClassifier::isRectangle(const Buffer<unsigned int>& buffer) const {
    const auto silhouette = extractSilhouette(buffer, m_targetColor);
    if (silhouette.sampleCount() < 16) return false;

    const double rv = silhouette.radialVariance();
    return rv >= 0.10 && rv <= 0.30;
  }
}
