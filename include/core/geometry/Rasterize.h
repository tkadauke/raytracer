#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace core {

  namespace detail {

    using RasterFixed = std::int64_t;

    // Screen coordinates and sample offsets are represented in 1/256ths of a
    // pixel while preparing edge equations. The single-sample path uses offset
    // (0, 0), exactly preserving the historical integer-pixel sample location;
    // MSAA callers pass offsets in [-0.5, 0.5] around that pixel center.
    //
    // Integer arithmetic uses int64_t throughout. Native `int` overflows for
    // large projected vertices because the edge function squares pixel deltas.
    constexpr RasterFixed kRasterSubpixelScale = 256;

    struct PreparedRasterTriangle {
      bool valid = false;
      int minX = 0;
      int maxX = -1;
      int minY = 0;
      int maxY = -1;
      RasterFixed area = 0;
      double invArea = 0.0;
      RasterFixed startW0 = 0;
      RasterFixed startW1 = 0;
      RasterFixed startW2 = 0;
      RasterFixed stepXW0 = 0;
      RasterFixed stepXW1 = 0;
      RasterFixed stepXW2 = 0;
      RasterFixed stepYW0 = 0;
      RasterFixed stepYW1 = 0;
      RasterFixed stepYW2 = 0;
      bool includeW0 = false;
      bool includeW1 = false;
      bool includeW2 = false;

      PreparedRasterTriangle(double x0, double y0, double x1, double y1, double x2, double y2,
                             int clipLeft, int clipTop, int clipRight, int clipBottom,
                             double sampleOffsetX, double sampleOffsetY) {
        if (clipLeft >= clipRight || clipTop >= clipBottom)
          return;

        const RasterFixed sampleX = toFixed(sampleOffsetX);
        const RasterFixed sampleY = toFixed(sampleOffsetY);

        const RasterFixed X0 = toFixed(x0);
        const RasterFixed X1 = toFixed(x1);
        const RasterFixed X2 = toFixed(x2);
        const RasterFixed Y0 = toFixed(y0);
        const RasterFixed Y1 = toFixed(y1);
        const RasterFixed Y2 = toFixed(y2);

        // Twice the signed area of the parent triangle. Sign indicates
        // winding; zero indicates collinear vertices and produces no pixels.
        area = (X1 - X0) * (Y2 - Y0) - (Y1 - Y0) * (X2 - X0);
        if (area == 0)
          return;

        const RasterFixed rawMinX = ceilDiv(std::min({X0, X1, X2}) - sampleX, kRasterSubpixelScale);
        const RasterFixed rawMaxX =
          floorDiv(std::max({X0, X1, X2}) - sampleX, kRasterSubpixelScale);
        const RasterFixed rawMinY = ceilDiv(std::min({Y0, Y1, Y2}) - sampleY, kRasterSubpixelScale);
        const RasterFixed rawMaxY =
          floorDiv(std::max({Y0, Y1, Y2}) - sampleY, kRasterSubpixelScale);

        const RasterFixed clipLeftFixed = clipLeft;
        const RasterFixed clipRightFixed = static_cast<RasterFixed>(clipRight) - 1;
        const RasterFixed clipTopFixed = clipTop;
        const RasterFixed clipBottomFixed = static_cast<RasterFixed>(clipBottom) - 1;
        if (rawMaxX < clipLeftFixed || rawMinX > clipRightFixed || rawMaxY < clipTopFixed ||
            rawMinY > clipBottomFixed) {
          return;
        }

        minX = static_cast<int>(std::max(rawMinX, clipLeftFixed));
        maxX = static_cast<int>(std::min(rawMaxX, clipRightFixed));
        minY = static_cast<int>(std::max(rawMinY, clipTopFixed));
        maxY = static_cast<int>(std::min(rawMaxY, clipBottomFixed));
        if (minX > maxX || minY > maxY)
          return;

        invArea = 1.0 / static_cast<double>(area);

        const RasterFixed X = static_cast<RasterFixed>(minX) * kRasterSubpixelScale + sampleX;
        const RasterFixed Y = static_cast<RasterFixed>(minY) * kRasterSubpixelScale + sampleY;

        startW0 = edge(X1, Y1, X2, Y2, X, Y);
        startW1 = edge(X2, Y2, X0, Y0, X, Y);
        startW2 = area - startW0 - startW1;

        stepXW0 = (Y1 - Y2) * kRasterSubpixelScale;
        stepXW1 = (Y2 - Y0) * kRasterSubpixelScale;
        stepXW2 = (Y0 - Y1) * kRasterSubpixelScale;

        stepYW0 = (X2 - X1) * kRasterSubpixelScale;
        stepYW1 = (X0 - X2) * kRasterSubpixelScale;
        stepYW2 = (X1 - X0) * kRasterSubpixelScale;

        if (area > 0) {
          includeW0 = isTopLeftEdge(X1, Y1, X2, Y2);
          includeW1 = isTopLeftEdge(X2, Y2, X0, Y0);
          includeW2 = isTopLeftEdge(X0, Y0, X1, Y1);
        } else {
          includeW0 = isTopLeftEdge(X2, Y2, X1, Y1);
          includeW1 = isTopLeftEdge(X0, Y0, X2, Y2);
          includeW2 = isTopLeftEdge(X1, Y1, X0, Y0);
        }

        valid = true;
      }

      bool contains(RasterFixed w0, RasterFixed w1, RasterFixed w2) const {
        return (area > 0)
                 ? containsPositiveEdge(w0, includeW0) && containsPositiveEdge(w1, includeW1) &&
                     containsPositiveEdge(w2, includeW2)
                 : containsNegativeEdge(w0, includeW0) && containsNegativeEdge(w1, includeW1) &&
                     containsNegativeEdge(w2, includeW2);
      }

    private:
      static RasterFixed toFixed(double value) {
        return static_cast<RasterFixed>(std::llround(value * kRasterSubpixelScale));
      }

      static RasterFixed floorDiv(RasterFixed numerator, RasterFixed denominator) {
        RasterFixed quotient = numerator / denominator;
        const RasterFixed remainder = numerator % denominator;
        if (remainder != 0 && ((remainder < 0) != (denominator < 0))) {
          --quotient;
        }
        return quotient;
      }

      static RasterFixed ceilDiv(RasterFixed numerator, RasterFixed denominator) {
        RasterFixed quotient = numerator / denominator;
        const RasterFixed remainder = numerator % denominator;
        if (remainder != 0 && ((remainder > 0) == (denominator > 0))) {
          ++quotient;
        }
        return quotient;
      }

      static bool isTopLeftEdge(RasterFixed ax, RasterFixed ay, RasterFixed bx, RasterFixed by) {
        const RasterFixed dx = bx - ax;
        const RasterFixed dy = by - ay;
        return dy < 0 || (dy == 0 && dx > 0);
      }

      static bool containsPositiveEdge(RasterFixed value, bool includeEdge) {
        return value > 0 || (value == 0 && includeEdge);
      }

      static bool containsNegativeEdge(RasterFixed value, bool includeEdge) {
        return value < 0 || (value == 0 && includeEdge);
      }

      static RasterFixed edge(RasterFixed ax, RasterFixed ay, RasterFixed bx, RasterFixed by,
                              RasterFixed px, RasterFixed py) {
        return (ax - px) * (by - py) - (ay - py) * (bx - px);
      }
    };

  } // namespace detail

  /**
  * @brief Filled-triangle rasterizer using the edge-function /
  *        barycentric-coordinate algorithm.
  *
  * Calls `plot(x, y, w0, w1, w2)` for every pixel inside the
  * triangle with vertices `(x0, y0)`, `(x1, y1)`, `(x2, y2)` and
  * inside the clip rectangle `[clipLeft, clipRight) ×
  * [clipTop, clipBottom)`. Vertex coordinates may be fractional;
  * the edge setup keeps them as subpixel fixed-point values instead
  * of rounding them to integer pixels first.
  * The barycentric weights `w0..w2` correspond to vertices
  * `p0..p2` respectively, are normalized to sum to 1.0, and are
  * the textbook input for per-vertex attribute interpolation
  * (z-depth, normals, UVs, colors).
  *
  * The algorithm prepares the triangle's clipped bounding box and
  * three edge-function values — the signed areas of the three
  * sub-triangles formed by the sample point and each pair of
  * vertices. It then increments those edge values across each row
  * instead of recomputing them from scratch for every candidate
  * pixel. A pixel is inside when all three sub-area signs match the
  * parent triangle's signed area (positive for CCW, negative for CW).
  * Samples exactly on an edge use a top-left fill rule: top or left
  * edges are included, bottom or right edges are excluded. That keeps
  * two triangles sharing an edge from both emitting the same pixel.
  * The barycentric weights then fall out of the sub-areas divided by
  * the parent area.
  *
  * Edge-function rasterization (Pineda 1988) is the modern
  * alternative to scanline rasterization: it parallelizes trivially
  * (every pixel is independent), handles arbitrary triangle
  * orientations without special cases, and produces barycentric
  * weights as a side effect of the inside-test. Hardware GPUs use
  * variants of it.
  *
  * `rasterizeTriangleSampled` evaluates the inside-test at
  * `(x + sampleOffsetX, y + sampleOffsetY)` while still reporting
  * the owning integer pixel `(x, y)`. The default
  * `rasterizeTriangle` wrapper uses `(0, 0)`, preserving the
  * historical single-sample behavior.
  *
  * Degenerate (zero-area) triangles produce no pixels.
  *
  * @tparam PlotFn callable with signature
  *         `void(int x, int y, double w0, double w1, double w2)`.
  */
  template<typename PlotFn>
  inline void rasterizeTriangleSampled(double x0, double y0, double x1, double y1, double x2,
                                       double y2, int clipLeft, int clipTop, int clipRight,
                                       int clipBottom, double sampleOffsetX, double sampleOffsetY,
                                       PlotFn&& plot) {
    const detail::PreparedRasterTriangle triangle(x0, y0, x1, y1, x2, y2, clipLeft, clipTop,
                                                  clipRight, clipBottom, sampleOffsetX,
                                                  sampleOffsetY);
    if (!triangle.valid)
      return;

    detail::RasterFixed rowW0 = triangle.startW0;
    detail::RasterFixed rowW1 = triangle.startW1;
    detail::RasterFixed rowW2 = triangle.startW2;

    for (int y = triangle.minY; y <= triangle.maxY; ++y) {
      detail::RasterFixed w0 = rowW0;
      detail::RasterFixed w1 = rowW1;
      detail::RasterFixed w2 = rowW2;

      for (int x = triangle.minX; x <= triangle.maxX; ++x) {
        if (triangle.contains(w0, w1, w2)) {
          plot(x, y, static_cast<double>(w0) * triangle.invArea,
               static_cast<double>(w1) * triangle.invArea,
               static_cast<double>(w2) * triangle.invArea);
        }

        w0 += triangle.stepXW0;
        w1 += triangle.stepXW1;
        w2 += triangle.stepXW2;
      }

      rowW0 += triangle.stepYW0;
      rowW1 += triangle.stepYW1;
      rowW2 += triangle.stepYW2;
    }
  }

  template<typename PlotFn>
  inline void rasterizeTriangleSampled(int x0, int y0, int x1, int y1, int x2, int y2, int clipLeft,
                                       int clipTop, int clipRight, int clipBottom,
                                       double sampleOffsetX, double sampleOffsetY, PlotFn&& plot) {
    rasterizeTriangleSampled(
      static_cast<double>(x0), static_cast<double>(y0), static_cast<double>(x1),
      static_cast<double>(y1), static_cast<double>(x2), static_cast<double>(y2), clipLeft, clipTop,
      clipRight, clipBottom, sampleOffsetX, sampleOffsetY, std::forward<PlotFn>(plot));
  }

  template<typename PlotFn>
  inline void rasterizeTriangle(double x0, double y0, double x1, double y1, double x2, double y2,
                                int clipLeft, int clipTop, int clipRight, int clipBottom,
                                PlotFn&& plot) {
    rasterizeTriangleSampled(x0, y0, x1, y1, x2, y2, clipLeft, clipTop, clipRight, clipBottom, 0.0,
                             0.0, std::forward<PlotFn>(plot));
  }

  template<typename PlotFn>
  inline void rasterizeTriangle(int x0, int y0, int x1, int y1, int x2, int y2, int clipLeft,
                                int clipTop, int clipRight, int clipBottom, PlotFn&& plot) {
    rasterizeTriangle(static_cast<double>(x0), static_cast<double>(y0), static_cast<double>(x1),
                      static_cast<double>(y1), static_cast<double>(x2), static_cast<double>(y2),
                      clipLeft, clipTop, clipRight, clipBottom, std::forward<PlotFn>(plot));
  }

} // namespace core
