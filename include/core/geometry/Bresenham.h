#pragma once

#include <cstdlib>
#include <utility>

namespace core {

  /**
  * Bresenham's all-octants integer line rasterizer.
  *
  * Calls `plot(x, y)` for each pixel along the line from `(x0, y0)`
  * to `(x1, y1)` inclusive. The plot callback is the only side
  * effect — bounds checking, blending, and colour selection are the
  * caller's responsibility (typical `plot` is a lambda capturing a
  * buffer reference and a colour).
  *
  * Single-pixel lines (`x0 == x1 && y0 == y1`) plot exactly one
  * pixel. Order-dependence is avoided: the pixel set produced by
  * `drawLine(a, b, ...)` is identical to that produced by
  * `drawLine(b, a, ...)` modulo iteration order, so wireframe
  * rendering doesn't depend on the direction of edge traversal in
  * the source mesh.
  *
  * The algorithm picks the longer axis as the "fast" axis and
  * walks one pixel per step along it, accumulating the orthogonal
  * step via the classic error term `2·dy - dx`. This keeps every
  * inner-loop computation in integer arithmetic — no division, no
  * floating-point — which is the historical reason Bresenham's
  * algorithm is taught as the canonical line rasterizer.
  *
  * @tparam PlotFn callable with signature `void(int x, int y)`.
  */
  template<typename PlotFn>
  inline void drawLine(int x0, int y0, int x1, int y1, PlotFn&& plot) {
    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;

    int x = x0;
    int y = y0;

    if (dx >= dy) {
      // X is the major axis. Step one pixel in x per iteration; step
      // in y only when the accumulated error crosses zero.
      int err = 2 * dy - dx;
      for (int i = 0; i <= dx; ++i) {
        plot(x, y);
        if (err > 0) {
          y += sy;
          err -= 2 * dx;
        }
        err += 2 * dy;
        x += sx;
      }
    } else {
      // Y is the major axis — symmetric case.
      int err = 2 * dx - dy;
      for (int i = 0; i <= dy; ++i) {
        plot(x, y);
        if (err > 0) {
          x += sx;
          err -= 2 * dy;
        }
        err += 2 * dx;
        y += sy;
      }
    }
  }

} // namespace core
