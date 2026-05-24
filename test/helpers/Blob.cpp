#include "test/helpers/Blob.h"

#include <algorithm>
#include <cmath>
#include <queue>

namespace testing {
  Blob::Blob(std::vector<Pixel> pixels, int bufferWidth, int bufferHeight)
      : m_pixels(std::move(pixels)) {
    if (m_pixels.empty())
      return;

    // Bounding box + centroid in a single pass.
    int minX = m_pixels.front().x, maxX = minX;
    int minY = m_pixels.front().y, maxY = minY;
    long long sumX = 0, sumY = 0;
    for (const auto& p : m_pixels) {
      minX = std::min(minX, p.x);
      maxX = std::max(maxX, p.x);
      minY = std::min(minY, p.y);
      maxY = std::max(maxY, p.y);
      sumX += p.x;
      sumY += p.y;
    }
    const int n = static_cast<int>(m_pixels.size());
    m_centroid = {static_cast<int>(sumX / n), static_cast<int>(sumY / n)};
    m_bbox = Recti(minX, minY, maxX - minX + 1, maxY - minY + 1);

    // Boundary extraction — a pixel is on the boundary if at least
    // one of its 4-neighbours is outside the blob (or off the buffer).
    // Use a flat bool mask for O(1) membership checks instead of a
    // std::set — the buffer is small enough to allocate the mask
    // cheaply, and the inner loop is hot.
    std::vector<bool> mask(static_cast<size_t>(bufferWidth) * bufferHeight, false);
    for (const auto& p : m_pixels) {
      mask[static_cast<size_t>(p.y) * bufferWidth + p.x] = true;
    }

    constexpr int dx[] = {-1, 1, 0, 0};
    constexpr int dy[] = {0, 0, -1, 1};
    for (const auto& p : m_pixels) {
      for (int k = 0; k < 4; ++k) {
        const int nx = p.x + dx[k];
        const int ny = p.y + dy[k];
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
    if (m_bbox.width() == 0)
      return 0.0;
    return static_cast<double>(m_bbox.height()) / m_bbox.width();
  }

  double Blob::radialVariance() const {
    if (m_boundary.empty())
      return 0.0;
    const double cx = m_centroid.x;
    const double cy = m_centroid.y;

    double sumD = 0.0;
    for (const auto& p : m_boundary) {
      const double dx = p.x - cx;
      const double dy = p.y - cy;
      sumD += std::sqrt(dx * dx + dy * dy);
    }
    const double mean = sumD / m_boundary.size();
    if (mean == 0.0)
      return 0.0;

    double sumSq = 0.0;
    for (const auto& p : m_boundary) {
      const double dx = p.x - cx;
      const double dy = p.y - cy;
      const double d = std::sqrt(dx * dx + dy * dy);
      sumSq += (d - mean) * (d - mean);
    }
    return std::sqrt(sumSq / m_boundary.size()) / mean;
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

    constexpr int dx[] = {-1, 1, 0, 0};
    constexpr int dy[] = {0, 0, -1, 1};

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
            const int nx = p.x + dx[k];
            const int ny = p.y + dy[k];
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
