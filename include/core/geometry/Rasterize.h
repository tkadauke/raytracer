#pragma once

#include <algorithm>
#include <cstdint>
#include <utility>

namespace core {

/**
  * @brief Filled-triangle rasterizer using the edge-function /
  *        barycentric-coordinate algorithm.
  *
  * Calls `plot(x, y, w0, w1, w2)` for every pixel inside the
  * triangle with vertices `(x0, y0)`, `(x1, y1)`, `(x2, y2)` and
  * inside the clip rectangle `[clipLeft, clipRight) ×
  * [clipTop, clipBottom)`.
  * The barycentric weights `w0..w2` correspond to vertices
  * `p0..p2` respectively, are normalised to sum to 1.0, and are
  * the textbook input for per-vertex attribute interpolation
  * (z-depth, normals, UVs, colours).
  *
  * The algorithm walks the triangle's clipped bounding box and for
  * each candidate pixel computes three edge-function values — the
  * signed areas of the three sub-triangles formed by the pixel and
  * each pair of vertices. A pixel is inside iff all three sub-area
  * signs match the parent triangle's signed area (positive for CCW,
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
                              int clipLeft,
                              int clipTop,
                              int clipRight,
                              int clipBottom,
                              PlotFn&& plot) {
  if (clipLeft >= clipRight || clipTop >= clipBottom) return;

  // Twice the signed area of the parent triangle. Sign indicates
  // winding (positive = CCW, negative = CW); zero indicates the
  // three points are collinear — nothing to fill.
  //
  // Integer arithmetic in int64_t throughout. The native `int`
  // overflows for triangle vertices at large screen coordinates
  // (which the rasterizer's near-plane clipper can produce when a
  // clipped vertex projects close to a viewport edge): the edge
  // function squares pixel deltas, so coords above ~46k overflow
  // a signed 32-bit int. int64_t lifts that to ~3 billion, well
  // beyond any realistic post-clip screen coordinate.
  using I = std::int64_t;
  const I X0 = x0, X1 = x1, X2 = x2, Y0 = y0, Y1 = y1, Y2 = y2;
  const I area = (X1 - X0) * (Y2 - Y0) - (Y1 - Y0) * (X2 - X0);
  if (area == 0) return;

  const int minX = std::max(std::min({x0, x1, x2}), clipLeft);
  const int maxX = std::min(std::max({x0, x1, x2}), clipRight - 1);
  const int minY = std::max(std::min({y0, y1, y2}), clipTop);
  const int maxY = std::min(std::max({y0, y1, y2}), clipBottom - 1);
  if (minX > maxX || minY > maxY) return;

  const double invArea = 1.0 / static_cast<double>(area);

  for (int y = minY; y <= maxY; ++y) {
    for (int x = minX; x <= maxX; ++x) {
      const I X = x, Y = y;
      // Edge functions: twice the signed sub-area opposite each
      // vertex. Computed via the same formula as `area` above with
      // the pixel substituted for the missing vertex.
      const I w0 = (X1 - X) * (Y2 - Y) - (Y1 - Y) * (X2 - X);  // opposite p0
      const I w1 = (X2 - X) * (Y0 - Y) - (Y2 - Y) * (X0 - X);  // opposite p1
      const I w2 = area - w0 - w1;                              // opposite p2

      // Inside iff all sub-area signs match the parent's.
      const bool inside = (area > 0)
        ? (w0 >= 0 && w1 >= 0 && w2 >= 0)
        : (w0 <= 0 && w1 <= 0 && w2 <= 0);

      if (inside) {
        plot(x, y, static_cast<double>(w0) * invArea,
                   static_cast<double>(w1) * invArea,
                   static_cast<double>(w2) * invArea);
      }
    }
  }
}

}  // namespace core
