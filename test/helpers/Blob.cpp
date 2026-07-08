#include "test/helpers/Blob.h"

#include <algorithm>
#include <cmath>
#include <queue>

namespace {
  constexpr int kDx[] = {-1, 1, 0, 0};
  constexpr int kDy[] = {0, 0, -1, 1};
}

namespace testing {
  CentroidAndBbox computeCentroidAndBbox(const std::vector<Pixel>& points) {
    int minX = points.front().x, maxX = minX;
    int minY = points.front().y, maxY = minY;
    long long sumX = 0, sumY = 0;
    for (const auto& p : points) {
      minX = std::min(minX, p.x);
      maxX = std::max(maxX, p.x);
      minY = std::min(minY, p.y);
      maxY = std::max(maxY, p.y);
      sumX += p.x;
      sumY += p.y;
    }
    const int n = static_cast<int>(points.size());
    return {{static_cast<int>(sumX / n), static_cast<int>(sumY / n)},
            Recti(minX, minY, maxX - minX + 1, maxY - minY + 1)};
  }

  double computeRadialVariance(const std::vector<Pixel>& points, Pixel centroid) {
    if (points.empty())
      return 0.0;
    const double cx = centroid.x;
    const double cy = centroid.y;

    double sumD = 0.0;
    for (const auto& p : points) {
      const double dx = p.x - cx;
      const double dy = p.y - cy;
      sumD += std::sqrt(dx * dx + dy * dy);
    }
    const double mean = sumD / points.size();
    if (mean == 0.0)
      return 0.0;

    double sumSq = 0.0;
    for (const auto& p : points) {
      const double dx = p.x - cx;
      const double dy = p.y - cy;
      const double d = std::sqrt(dx * dx + dy * dy);
      sumSq += (d - mean) * (d - mean);
    }
    return std::sqrt(sumSq / points.size()) / mean;
  }

  Blob::Blob(std::vector<Pixel> pixels, int bufferWidth, int bufferHeight)
      : m_pixels(std::move(pixels)) {
    if (m_pixels.empty())
      return;

    auto [centroid, bbox] = computeCentroidAndBbox(m_pixels);
    m_centroid = centroid;
    m_bbox = bbox;

    // Boundary extraction — a pixel is on the boundary if at least
    // one of its 4-neighbours is outside the blob (or off the buffer).
    // Use a flat bool mask for O(1) membership checks instead of a
    // std::set — the buffer is small enough to allocate the mask
    // cheaply, and the inner loop is hot.
    std::vector<bool> mask(static_cast<size_t>(bufferWidth) * bufferHeight, false);
    for (const auto& p : m_pixels) {
      mask[static_cast<size_t>(p.y) * bufferWidth + p.x] = true;
    }

    for (const auto& p : m_pixels) {
      for (int k = 0; k < 4; ++k) {
        const int nx = p.x + kDx[k];
        const int ny = p.y + kDy[k];
        if (nx < 0 || nx >= bufferWidth || ny < 0 || ny >= bufferHeight ||
            !mask[static_cast<size_t>(ny) * bufferWidth + nx]) {
          m_boundary.push_back(p);
          break;
        }
      }
    }
  }

  double Blob::circularity() const {
    if (perimeter() == 0)
      return 0.0;
    const double a = area();
    const double p = perimeter();
    return 4.0 * M_PI * a / (p * p);
  }

  double Blob::aspectRatio() const {
    return m_bbox.aspectRatio();
  }

  double Blob::radialVariance() const {
    return computeRadialVariance(m_boundary, m_centroid);
  }

  double Blob::extent() const {
    const int bboxArea = m_bbox.width() * m_bbox.height();
    if (bboxArea == 0)
      return 0.0;
    return static_cast<double>(area()) / bboxArea;
  }

  std::vector<Blob> findAllBlobs(const Buffer<unsigned int>& buffer, const Colord& color) {
    const unsigned int target = color.rgb();
    const int w = buffer.width();
    const int h = buffer.height();
    std::vector<bool> visited(static_cast<size_t>(w) * h, false);
    std::vector<Blob> blobs;

    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        const size_t idx = static_cast<size_t>(y) * w + x;
        if (visited[idx] || buffer[y][x] != target)
          continue;

        // BFS flood-fill from this seed.
        std::vector<Pixel> pixels;
        std::queue<Pixel> queue;
        queue.push({x, y});
        visited[idx] = true;

        while (!queue.empty()) {
          const Pixel p = queue.front();
          queue.pop();
          pixels.push_back(p);

          for (int k = 0; k < 4; ++k) {
            const int nx = p.x + kDx[k];
            const int ny = p.y + kDy[k];
            if (nx < 0 || nx >= w || ny < 0 || ny >= h)
              continue;
            const size_t nIdx = static_cast<size_t>(ny) * w + nx;
            if (visited[nIdx] || buffer[ny][nx] != target)
              continue;
            visited[nIdx] = true;
            queue.push({nx, ny});
          }
        }

        blobs.emplace_back(std::move(pixels), w, h);
      }
    }

    return blobs;
  }

  std::optional<Blob> findLargestBlob(const Buffer<unsigned int>& buffer, const Colord& color) {
    auto blobs = findAllBlobs(buffer, color);
    if (blobs.empty())
      return std::nullopt;
    auto it = std::max_element(blobs.begin(), blobs.end(),
                               [](const Blob& a, const Blob& b) { return a.area() < b.area(); });
    return std::move(*it);
  }
}
