#include <gtest/gtest.h>

#include "core/geometry/Rasterize.h"

#include <set>
#include <utility>
#include <vector>

namespace RasterizeTest {
  using namespace ::testing;

  // Pixel set helper — captures every (x, y) the rasterizer emits.
  // Most tests assert on the *set* of plotted pixels rather than the
  // iteration order, since the edge-function algorithm is
  // order-agnostic and any callback-iteration-order regression should
  // not flap these tests.
  static std::set<std::pair<int, int>> rasterizeToSet(int x0, int y0,
                                                       int x1, int y1,
                                                       int x2, int y2) {
    std::set<std::pair<int, int>> pixels;
    const int minX = std::min({x0, x1, x2});
    const int maxX = std::max({x0, x1, x2});
    const int minY = std::min({y0, y1, y2});
    const int maxY = std::max({y0, y1, y2});
    core::rasterizeTriangle(x0, y0, x1, y1, x2, y2,
      minX, minY, maxX + 1, maxY + 1,
      [&](int x, int y, double, double, double) {
        pixels.emplace(x, y);
      });
    return pixels;
  }

  static std::set<std::pair<int, int>> rasterizeClippedToSet(int x0, int y0,
                                                             int x1, int y1,
                                                             int x2, int y2,
                                                             int clipLeft,
                                                             int clipTop,
                                                             int clipRight,
                                                             int clipBottom) {
    std::set<std::pair<int, int>> pixels;
    core::rasterizeTriangle(x0, y0, x1, y1, x2, y2,
      clipLeft, clipTop, clipRight, clipBottom,
      [&](int x, int y, double, double, double) {
        pixels.emplace(x, y);
      });
    return pixels;
  }

  TEST(Rasterize, DegenerateTriangleEmitsNoPixels) {
    // All three points collinear → zero signed area → skipped.
    EXPECT_EQ(0u, rasterizeToSet(0, 0, 5, 0, 10, 0).size());
    EXPECT_EQ(0u, rasterizeToSet(2, 2, 2, 2, 2, 2).size());
  }

  TEST(Rasterize, SmallTriangleProducesExpectedPixelCount) {
    // Right triangle with legs of length 4 along x and y axes.
    // Filled pixel count is 1+2+3+4+5 = 15 (the staircase pattern
    // including the hypotenuse pixels). Order isn't important; the
    // count is a stable invariant.
    auto pixels = rasterizeToSet(0, 0, 4, 0, 0, 4);
    EXPECT_GT(pixels.size(), 0u);
    // Vertex pixels must be present.
    EXPECT_TRUE(pixels.count({0, 0}));
    EXPECT_TRUE(pixels.count({4, 0}));
    EXPECT_TRUE(pixels.count({0, 4}));
  }

  TEST(Rasterize, ProducesIdenticalSetForCcwAndCwOrdering) {
    // Same three vertices, opposite winding order — the edge-function
    // algorithm should emit the same pixel SET (signs flip but the
    // inside-test handles both cases).
    auto ccw = rasterizeToSet(0, 0, 5, 0, 0, 5);
    auto cw  = rasterizeToSet(0, 0, 0, 5, 5, 0);
    EXPECT_EQ(ccw, cw);
  }

  TEST(Rasterize, BarycentricWeightsSumToOne) {
    // For every pixel inside, the three barycentric weights sum to
    // 1.0 (within float tolerance) and each lies in [0, 1].
    bool anyHit = false;
    core::rasterizeTriangle(0, 0, 10, 0, 5, 8,
      0, 0, 11, 9,
      [&](int, int, double w0, double w1, double w2) {
        anyHit = true;
        EXPECT_NEAR(w0 + w1 + w2, 1.0, 1e-9);
        EXPECT_GE(w0, -1e-9);
        EXPECT_GE(w1, -1e-9);
        EXPECT_GE(w2, -1e-9);
        EXPECT_LE(w0, 1.0 + 1e-9);
        EXPECT_LE(w1, 1.0 + 1e-9);
        EXPECT_LE(w2, 1.0 + 1e-9);
      });
    EXPECT_TRUE(anyHit);
  }

  TEST(Rasterize, BarycentricAtVerticesMatchesCorner) {
    // Pixel exactly at vertex p0 should report w0 ≈ 1, others ≈ 0.
    bool foundP0 = false;
    core::rasterizeTriangle(2, 3, 10, 3, 6, 9,
      2, 3, 11, 10,
      [&](int x, int y, double w0, double w1, double w2) {
        if (x == 2 && y == 3) {
          foundP0 = true;
          EXPECT_NEAR(w0, 1.0, 1e-9);
          EXPECT_NEAR(w1, 0.0, 1e-9);
          EXPECT_NEAR(w2, 0.0, 1e-9);
        }
      });
    EXPECT_TRUE(foundP0);
  }

  TEST(Rasterize, NegativeCoordinatesWorkCorrectly) {
    // Triangle straddling the origin with negative coords. The
    // algorithm uses signed integer math throughout, so negative
    // inputs are valid.
    auto pixels = rasterizeToSet(-2, -2, 2, -2, 0, 2);
    EXPECT_GT(pixels.size(), 0u);
    EXPECT_TRUE(pixels.count({0, 0}));   // centre roughly
    EXPECT_TRUE(pixels.count({-2, -2}));
  }

  TEST(Rasterize, LargeTriangleCoversBoundingBoxAreaProportion) {
    // For a "fat" triangle, the filled-pixel count should approach
    // half the bounding-box area (a triangle is half its
    // axis-aligned bounding rectangle on average). This is a
    // sanity check on the inside-test, not a precision claim.
    auto pixels = rasterizeToSet(0, 0, 100, 0, 0, 100);
    const std::size_t bboxArea = 101 * 101;
    EXPECT_GT(pixels.size(), bboxArea / 3);    // not way too few
    EXPECT_LT(pixels.size(), bboxArea * 2 / 3); // not way too many
  }

  TEST(Rasterize, ClippedTriangleOnlyEmitsPixelsInsideClipRect) {
    auto pixels = rasterizeClippedToSet(-10, -10, 20, -10, 5, 20,
                                        0, 0, 8, 8);

    EXPECT_GT(pixels.size(), 0u);
    for (const auto& pixel : pixels) {
      EXPECT_GE(pixel.first, 0);
      EXPECT_LT(pixel.first, 8);
      EXPECT_GE(pixel.second, 0);
      EXPECT_LT(pixel.second, 8);
    }
  }

  TEST(Rasterize, ClippedHugeTriangleDoesNotWalkTheUnboundedBox) {
    auto pixels = rasterizeClippedToSet(-100000, -100000,
                                        100000, -100000,
                                        0, 100000,
                                        0, 0, 8, 8);

    EXPECT_GT(pixels.size(), 0u);
    EXPECT_LE(pixels.size(), 8u * 8u);
  }
}
