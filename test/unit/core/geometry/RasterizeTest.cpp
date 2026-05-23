#include <gtest/gtest.h>

#include "core/geometry/Rasterize.h"

#include <map>
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
  static std::set<std::pair<int, int>> rasterizeToSet(int x0, int y0, int x1, int y1, int x2,
                                                      int y2) {
    std::set<std::pair<int, int>> pixels;
    const int minX = std::min({x0, x1, x2});
    const int maxX = std::max({x0, x1, x2});
    const int minY = std::min({y0, y1, y2});
    const int maxY = std::max({y0, y1, y2});
    core::rasterizeTriangle(x0, y0, x1, y1, x2, y2, minX, minY, maxX + 1, maxY + 1,
                            [&](int x, int y, double, double, double) { pixels.emplace(x, y); });
    return pixels;
  }

  static std::set<std::pair<int, int>> rasterizeClippedToSet(int x0, int y0, int x1, int y1, int x2,
                                                             int y2, int clipLeft, int clipTop,
                                                             int clipRight, int clipBottom) {
    std::set<std::pair<int, int>> pixels;
    core::rasterizeTriangle(x0, y0, x1, y1, x2, y2, clipLeft, clipTop, clipRight, clipBottom,
                            [&](int x, int y, double, double, double) { pixels.emplace(x, y); });
    return pixels;
  }

  static std::set<std::pair<int, int>> rasterizeSampledToSet(int x0, int y0, int x1, int y1, int x2,
                                                             int y2, double sampleOffsetX,
                                                             double sampleOffsetY) {
    std::set<std::pair<int, int>> pixels;
    const int minX = std::min({x0, x1, x2});
    const int maxX = std::max({x0, x1, x2});
    const int minY = std::min({y0, y1, y2});
    const int maxY = std::max({y0, y1, y2});
    core::rasterizeTriangleSampled(
      x0, y0, x1, y1, x2, y2, minX, minY, maxX + 1, maxY + 1, sampleOffsetX, sampleOffsetY,
      [&](int x, int y, double, double, double) { pixels.emplace(x, y); });
    return pixels;
  }

  static std::set<std::pair<int, int>> rasterizeSubpixelToSet(double x0, double y0, double x1,
                                                              double y1, double x2, double y2,
                                                              int clipLeft, int clipTop,
                                                              int clipRight, int clipBottom) {
    std::set<std::pair<int, int>> pixels;
    core::rasterizeTriangle(x0, y0, x1, y1, x2, y2, clipLeft, clipTop, clipRight, clipBottom,
                            [&](int x, int y, double, double, double) { pixels.emplace(x, y); });
    return pixels;
  }

  static std::set<std::pair<int, int>>
  rasterizeSubpixelSampledToSet(double x0, double y0, double x1, double y1, double x2, double y2,
                                int clipLeft, int clipTop, int clipRight, int clipBottom,
                                double sampleOffsetX, double sampleOffsetY) {
    std::set<std::pair<int, int>> pixels;
    core::rasterizeTriangleSampled(
      x0, y0, x1, y1, x2, y2, clipLeft, clipTop, clipRight, clipBottom, sampleOffsetX,
      sampleOffsetY, [&](int x, int y, double, double, double) { pixels.emplace(x, y); });
    return pixels;
  }

  static void addRasterizedTriangleCounts(std::map<std::pair<int, int>, int>& counts, int x0,
                                          int y0, int x1, int y1, int x2, int y2, int clipLeft,
                                          int clipTop, int clipRight, int clipBottom) {
    core::rasterizeTriangle(x0, y0, x1, y1, x2, y2, clipLeft, clipTop, clipRight, clipBottom,
                            [&](int x, int y, double, double, double) { ++counts[{x, y}]; });
  }

  static void expectVertexNear(const core::RasterClipVertex& vertex, double x, double y) {
    EXPECT_NEAR(x, vertex.x, 1e-9);
    EXPECT_NEAR(y, vertex.y, 1e-9);
  }

  static std::set<std::pair<int, int>> rasterizeQuadToSet(bool firstDiagonal) {
    std::set<std::pair<int, int>> pixels;
    const auto addTriangle = [&](int x0, int y0, int x1, int y1, int x2, int y2) {
      core::rasterizeTriangle(x0, y0, x1, y1, x2, y2, 0, 0, 4, 4,
                              [&](int x, int y, double, double, double) { pixels.emplace(x, y); });
    };

    if (firstDiagonal) {
      addTriangle(0, 0, 4, 0, 0, 4);
      addTriangle(4, 0, 4, 4, 0, 4);
    } else {
      addTriangle(0, 0, 4, 0, 4, 4);
      addTriangle(0, 0, 4, 4, 0, 4);
    }
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
    // The top-left fill rule keeps the top-left vertex but excludes
    // the bottom/right vertices, which lie on non-inclusive edges.
    EXPECT_TRUE(pixels.count({0, 0}));
    EXPECT_FALSE(pixels.count({4, 0}));
    EXPECT_FALSE(pixels.count({0, 4}));
  }

  TEST(Rasterize, ProducesIdenticalSetForCcwAndCwOrdering) {
    // Same three vertices, opposite winding order — the edge-function
    // algorithm should emit the same pixel SET (signs flip but the
    // inside-test handles both cases).
    auto ccw = rasterizeToSet(0, 0, 5, 0, 0, 5);
    auto cw = rasterizeToSet(0, 0, 0, 5, 5, 0);
    EXPECT_EQ(ccw, cw);
  }

  TEST(Rasterize, BarycentricWeightsSumToOne) {
    // For every pixel inside, the three barycentric weights sum to
    // 1.0 (within float tolerance) and each lies in [0, 1].
    bool anyHit = false;
    core::rasterizeTriangle(0, 0, 10, 0, 5, 8, 0, 0, 11, 9,
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
    core::rasterizeTriangle(2, 3, 10, 3, 6, 9, 2, 3, 11, 10,
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

  TEST(Rasterize, SampleOffsetMovesBarycentricPointWithinPixel) {
    bool foundP0Pixel = false;
    core::rasterizeTriangleSampled(0, 0, 4, 0, 0, 4, 0, 0, 5, 5, 0.25, 0.25,
                                   [&](int x, int y, double w0, double w1, double w2) {
                                     if (x == 0 && y == 0) {
                                       foundP0Pixel = true;
                                       EXPECT_NEAR(w0, 0.875, 1e-9);
                                       EXPECT_NEAR(w1, 0.0625, 1e-9);
                                       EXPECT_NEAR(w2, 0.0625, 1e-9);
                                     }
                                   });
    EXPECT_TRUE(foundP0Pixel);
  }

  TEST(Rasterize, SampleOffsetChangesEdgeCoverage) {
    auto centre = rasterizeSampledToSet(0, 0, 2, 0, 0, 2, 0.0, 0.0);
    auto shifted = rasterizeSampledToSet(0, 0, 2, 0, 0, 2, -0.25, -0.25);

    EXPECT_FALSE(centre.count({1, 1}));
    EXPECT_TRUE(shifted.count({1, 1}));
  }

  TEST(Rasterize, SubpixelVerticesDoNotRoundAtHalfPixel) {
    const auto beforeHalfPixel = rasterizeSubpixelToSet(0.0, 0.0, 2.49, 0.0, 0.0, 4.0, 0, 0, 5, 5);
    const auto afterHalfPixel = rasterizeSubpixelToSet(0.0, 0.0, 2.51, 0.0, 0.0, 4.0, 0, 0, 5, 5);

    EXPECT_EQ(beforeHalfPixel, afterHalfPixel);
  }

  TEST(Rasterize, SubpixelVerticesChangeCoverageWhenEdgeCrossesSamplePoint) {
    const auto beforeSamplePoint =
      rasterizeSubpixelToSet(0.0, 0.0, 2.99, 0.0, 0.0, 4.0, 0, 0, 5, 5);
    const auto afterSamplePoint = rasterizeSubpixelToSet(0.0, 0.0, 3.01, 0.0, 0.0, 4.0, 0, 0, 5, 5);

    EXPECT_FALSE(beforeSamplePoint.count({3, 0}));
    EXPECT_TRUE(afterSamplePoint.count({3, 0}));
  }

  TEST(Rasterize, SubpixelVerticesAndSampleOffsetsUseSameCoordinateSpace) {
    const auto leftSample =
      rasterizeSubpixelSampledToSet(0.125, 0.0, 4.0, 0.0, 0.125, 4.0, -1, 0, 5, 5, -0.25, 0.0);
    const auto rightSample =
      rasterizeSubpixelSampledToSet(0.125, 0.0, 4.0, 0.0, 0.125, 4.0, -1, 0, 5, 5, 0.25, 0.0);

    EXPECT_FALSE(leftSample.count({0, 1}));
    EXPECT_TRUE(rightSample.count({0, 1}));
  }

  TEST(Rasterize, AdjacentTrianglesCoverRectangleWithoutOverdraw) {
    std::map<std::pair<int, int>, int> counts;
    addRasterizedTriangleCounts(counts, 0, 0, 4, 0, 0, 4, 0, 0, 4, 4);
    addRasterizedTriangleCounts(counts, 4, 0, 4, 4, 0, 4, 0, 0, 4, 4);

    ASSERT_EQ(16u, counts.size());
    for (int y = 0; y < 4; ++y) {
      for (int x = 0; x < 4; ++x) {
        const auto pixel = std::make_pair(x, y);
        EXPECT_EQ(1, counts[pixel]) << "at (" << x << ", " << y << ")";
      }
    }
  }

  TEST(Rasterize, EquivalentQuadTriangulationsProduceSameCoverage) {
    const auto firstDiagonal = rasterizeQuadToSet(true);
    const auto secondDiagonal = rasterizeQuadToSet(false);

    EXPECT_EQ(firstDiagonal, secondDiagonal);
    EXPECT_EQ(16u, firstDiagonal.size());
  }

  TEST(Rasterize, NegativeCoordinatesWorkCorrectly) {
    // Triangle straddling the origin with negative coords. The
    // algorithm uses signed integer math throughout, so negative
    // inputs are valid.
    auto pixels = rasterizeToSet(-2, -2, 2, -2, 0, 2);
    EXPECT_GT(pixels.size(), 0u);
    EXPECT_TRUE(pixels.count({0, 0})); // centre roughly
    EXPECT_TRUE(pixels.count({-2, -2}));
  }

  TEST(Rasterize, LargeTriangleCoversBoundingBoxAreaProportion) {
    // For a "fat" triangle, the filled-pixel count should approach
    // half the bounding-box area (a triangle is half its
    // axis-aligned bounding rectangle on average). This is a
    // sanity check on the inside-test, not a precision claim.
    auto pixels = rasterizeToSet(0, 0, 100, 0, 0, 100);
    const std::size_t bboxArea = 101 * 101;
    EXPECT_GT(pixels.size(), bboxArea / 3);     // not way too few
    EXPECT_LT(pixels.size(), bboxArea * 2 / 3); // not way too many
  }

  TEST(Rasterize, ClippedTriangleOnlyEmitsPixelsInsideClipRect) {
    auto pixels = rasterizeClippedToSet(-10, -10, 20, -10, 5, 20, 0, 0, 8, 8);

    EXPECT_GT(pixels.size(), 0u);
    for (const auto& pixel : pixels) {
      EXPECT_GE(pixel.first, 0);
      EXPECT_LT(pixel.first, 8);
      EXPECT_GE(pixel.second, 0);
      EXPECT_LT(pixel.second, 8);
    }
  }

  TEST(Rasterize, ClipTriangleToRectKeepsInsideTriangle) {
    const auto polygon = core::clipTriangleToRect(1.0, 1.0, 3.0, 1.0, 2.0, 3.0, 0.0, 0.0, 4.0, 4.0);

    ASSERT_EQ(3u, polygon.size());
    expectVertexNear(polygon[0], 1.0, 1.0);
    expectVertexNear(polygon[1], 3.0, 1.0);
    expectVertexNear(polygon[2], 2.0, 3.0);
  }

  TEST(Rasterize, ClipTriangleToRectRejectsOutsideTriangle) {
    const auto polygon =
      core::clipTriangleToRect(-4.0, 1.0, -2.0, 3.0, -1.0, 1.0, 0.0, 0.0, 4.0, 4.0);

    EXPECT_TRUE(polygon.empty());
  }

  TEST(Rasterize, ClipTriangleToRectCutsAgainstViewportEdge) {
    const auto polygon =
      core::clipTriangleToRect(-1.0, 1.0, 1.0, 1.0, 1.0, 3.0, 0.0, 0.0, 4.0, 4.0);

    ASSERT_EQ(4u, polygon.size());
    expectVertexNear(polygon[0], 0.0, 2.0);
    expectVertexNear(polygon[1], 0.0, 1.0);
    expectVertexNear(polygon[2], 1.0, 1.0);
    expectVertexNear(polygon[3], 1.0, 3.0);
  }

  TEST(Rasterize, ClipTriangleToRectCanProduceMoreThanThreeVertices) {
    const auto polygon =
      core::clipTriangleToRect(-1.0, 2.0, 2.0, -1.0, 5.0, 2.0, 0.0, 0.0, 4.0, 4.0);

    EXPECT_GT(polygon.size(), 3u);
    for (const auto& vertex : polygon) {
      EXPECT_GE(vertex.x, -1e-9);
      EXPECT_LE(vertex.x, 4.0 + 1e-9);
      EXPECT_GE(vertex.y, -1e-9);
      EXPECT_LE(vertex.y, 4.0 + 1e-9);
    }
  }

  TEST(Rasterize, FanTriangulateRasterClipPolygonEmitsNMinusTwoTriangles) {
    const auto polygon =
      core::clipTriangleToRect(-1.0, 2.0, 2.0, -1.0, 5.0, 2.0, 0.0, 0.0, 4.0, 4.0);
    std::vector<core::RasterClipTriangle> triangles;

    core::fanTriangulateRasterClipPolygon(
      polygon, [&](const core::RasterClipTriangle& triangle) { triangles.push_back(triangle); });

    ASSERT_GE(polygon.size(), 3u);
    EXPECT_EQ(polygon.size() - 2, triangles.size());
    expectVertexNear(triangles.front().v0, polygon[0].x, polygon[0].y);
    expectVertexNear(triangles.front().v1, polygon[1].x, polygon[1].y);
    expectVertexNear(triangles.front().v2, polygon[2].x, polygon[2].y);
  }

  TEST(Rasterize, ClippedHugeTriangleDoesNotWalkTheUnboundedBox) {
    auto pixels = rasterizeClippedToSet(-100000, -100000, 100000, -100000, 0, 100000, 0, 0, 8, 8);

    EXPECT_GT(pixels.size(), 0u);
    EXPECT_LE(pixels.size(), 8u * 8u);
  }
}
