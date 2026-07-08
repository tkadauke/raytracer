#include "test/helpers/Silhouette.h"

#include <algorithm>
#include <cmath>

namespace testing {
  Silhouette::Silhouette(std::vector<Pixel> points)
      : m_points(std::move(points)) {
    if (m_points.empty())
      return;

    auto [centroid, bbox] = computeCentroidAndBbox(m_points);
    m_centroid = centroid;
    m_bbox = bbox;
  }

  double Silhouette::aspectRatio() const {
    return m_bbox.aspectRatio();
  }

  double Silhouette::radialVariance() const {
    return computeRadialVariance(m_points, m_centroid);
  }

  Silhouette extractSilhouette(const Buffer<unsigned int>& buffer, const Colord& color) {
    const unsigned int target = color.rgb();
    const int w = buffer.width();
    const int h = buffer.height();
    std::vector<Pixel> points;

    // Row scan — leftmost + rightmost target pixel per row.
    for (int y = 0; y < h; ++y) {
      int leftmost = -1, rightmost = -1;
      for (int x = 0; x < w; ++x) {
        if (buffer[y][x] == target) {
          if (leftmost == -1)
            leftmost = x;
          rightmost = x;
        }
      }
      if (leftmost >= 0) {
        points.push_back({leftmost, y});
        if (rightmost != leftmost)
          points.push_back({rightmost, y});
      }
    }

    // Column scan — topmost + bottommost target pixel per column.
    // Combined with the row scan, this gives a dense sample set
    // around the silhouette regardless of which axis dominates the
    // shape's extent.
    for (int x = 0; x < w; ++x) {
      int topmost = -1, bottommost = -1;
      for (int y = 0; y < h; ++y) {
        if (buffer[y][x] == target) {
          if (topmost == -1)
            topmost = y;
          bottommost = y;
        }
      }
      if (topmost >= 0) {
        points.push_back({x, topmost});
        if (bottommost != topmost)
          points.push_back({x, bottommost});
      }
    }

    return Silhouette(std::move(points));
  }
}
