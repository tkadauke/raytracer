#include "test/helpers/Silhouette.h"

#include <algorithm>
#include <cmath>

namespace testing {
  Silhouette::Silhouette(std::vector<Pixel> points)
    : m_points(std::move(points))
  {
    if (m_points.empty()) return;

    int minX = m_points.front().x, maxX = minX;
    int minY = m_points.front().y, maxY = minY;
    long long sumX = 0, sumY = 0;
    for (const auto& p : m_points) {
      minX = std::min(minX, p.x);
      maxX = std::max(maxX, p.x);
      minY = std::min(minY, p.y);
      maxY = std::max(maxY, p.y);
      sumX += p.x;
      sumY += p.y;
    }
    const int n = static_cast<int>(m_points.size());
    m_centroid = {static_cast<int>(sumX / n), static_cast<int>(sumY / n)};
    m_bbox = Recti(minX, minY, maxX - minX + 1, maxY - minY + 1);
  }

  double Silhouette::aspectRatio() const {
    if (m_bbox.width() == 0) return 0.0;
    return static_cast<double>(m_bbox.height()) / m_bbox.width();
  }

  double Silhouette::radialVariance() const {
    if (m_points.empty()) return 0.0;
    const double cx = m_centroid.x;
    const double cy = m_centroid.y;

    double sumD = 0.0;
    for (const auto& p : m_points) {
      const double dx = p.x - cx;
      const double dy = p.y - cy;
      sumD += std::sqrt(dx * dx + dy * dy);
    }
    const double mean = sumD / m_points.size();
    if (mean == 0.0) return 0.0;

    double sumSq = 0.0;
    for (const auto& p : m_points) {
      const double dx = p.x - cx;
      const double dy = p.y - cy;
      const double d = std::sqrt(dx * dx + dy * dy);
      sumSq += (d - mean) * (d - mean);
    }
    return std::sqrt(sumSq / m_points.size()) / mean;
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
          if (leftmost == -1) leftmost = x;
          rightmost = x;
        }
      }
      if (leftmost >= 0) {
        points.push_back({leftmost, y});
        if (rightmost != leftmost) points.push_back({rightmost, y});
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
          if (topmost == -1) topmost = y;
          bottommost = y;
        }
      }
      if (topmost >= 0) {
        points.push_back({x, topmost});
        if (bottommost != topmost) points.push_back({x, bottommost});
      }
    }

    return Silhouette(std::move(points));
  }
}
