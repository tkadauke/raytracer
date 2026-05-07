#pragma once

#include <algorithm>
#include <utility>

namespace core {

/**
  * @brief Filled-triangle rasterizer using the edge-function /
  *        barycentric-coordinate algorithm.
  *
  * Calls `plot(x, y, w0, w1, w2)` for every pixel inside the
  * triangle with vertices `(x0, y0)`, `(x1, y1)`, `(x2, y2)`.
  * The barycentric weights `w0..w2` correspond to vertices
  * `p0..p2` respectively, are normalised to sum to 1.0, and are
  * the textbook input for per-vertex attribute interpolation
  * (z-depth, normals, UVs, colours).
  *
  * The algorithm walks the triangle's bounding box and for each
  * candidate pixel computes three edge-function values — the signed
  * areas of the three sub-triangles formed by the pixel and each
  * pair of vertices. A pixel is inside iff all three sub-area signs
  * match the parent triangle's signed area (positive for CCW,
  * negative for CW). The barycentric weights then fall out of the
  * sub-areas divided by the parent area — no extra computation
  * needed.
  *
  * Edge-function rasterization (Pineda 1988) is the modern
  * alternative to scanline rasterization: it parallelises trivially
  * (every pixel is independent), handles arbitrary triangle
  * orientations without special cases, and produces barycentric
  * weights as a side effect of the inside-test. Hardware GPUs use
  * variants of it.
  *
  * Degenerate (zero-area) triangles produce no pixels.
  *
  * @tparam PlotFn callable with signature
  *         `void(int x, int y, double w0, double w1, double w2)`.
  */
template <typename PlotFn>
inline void rasterizeTriangle(int x0, int y0,
                              int x1, int y1,
                              int x2, int y2,
                              PlotFn&& plot) {
  const int minX = std::min({x0, x1, x2});
  const int maxX = std::max({x0, x1, x2});
  const int minY = std::min({y0, y1, y2});
  const int maxY = std::max({y0, y1, y2});

  // Twice the signed area of the parent triangle. Sign indicates
  // winding (positive = CCW, negative = CW); zero indicates the
  // three points are collinear — nothing to fill.
  const int area = (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0);
  if (area == 0) return;

  const double invArea = 1.0 / area;

  for (int y = minY; y <= maxY; ++y) {
    for (int x = minX; x <= maxX; ++x) {
      // Edge functions: twice the signed sub-area opposite each
      // vertex. Computed via the same formula as `area` above with
      // the pixel substituted for the missing vertex.
      const int w0 = (x1 - x) * (y2 - y) - (y1 - y) * (x2 - x);  // opposite p0
      const int w1 = (x2 - x) * (y0 - y) - (y2 - y) * (x0 - x);  // opposite p1
      const int w2 = area - w0 - w1;                              // opposite p2

      // Inside iff all sub-area signs match the parent's.
      const bool inside = (area > 0)
        ? (w0 >= 0 && w1 >= 0 && w2 >= 0)
        : (w0 <= 0 && w1 <= 0 && w2 <= 0);

      if (inside) {
        plot(x, y, w0 * invArea, w1 * invArea, w2 * invArea);
      }
    }
  }
}

}  // namespace core
