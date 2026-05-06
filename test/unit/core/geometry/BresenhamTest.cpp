#include <gtest/gtest.h>
#include "core/geometry/Bresenham.h"

#include <set>
#include <utility>
#include <vector>

namespace BresenhamTest {

  using PixelSet = std::set<std::pair<int, int>>;

  // Helper: collect every pixel `drawLine` plots into a set.
  static PixelSet plotted(int x0, int y0, int x1, int y1) {
    PixelSet pixels;
    core::drawLine(x0, y0, x1, y1, [&](int x, int y) {
      pixels.emplace(x, y);
    });
    return pixels;
  }

  // Helper: collect every pixel into a vector to check ordering.
  static std::vector<std::pair<int, int>> plottedInOrder(int x0, int y0, int x1, int y1) {
    std::vector<std::pair<int, int>> pixels;
    core::drawLine(x0, y0, x1, y1, [&](int x, int y) {
      pixels.emplace_back(x, y);
    });
    return pixels;
  }

  TEST(Bresenham, SinglePixelLineEmitsOnePixel) {
    auto pixels = plotted(5, 7, 5, 7);
    EXPECT_EQ(1u, pixels.size());
    EXPECT_TRUE(pixels.count({5, 7}));
  }

  TEST(Bresenham, HorizontalLineEmitsEveryPixelInRange) {
    auto pixels = plottedInOrder(2, 4, 7, 4);
    ASSERT_EQ(6u, pixels.size());
    for (int x = 2, i = 0; x <= 7; ++x, ++i) {
      EXPECT_EQ(std::make_pair(x, 4), pixels[i]);
    }
  }

  TEST(Bresenham, VerticalLineEmitsEveryPixelInRange) {
    auto pixels = plottedInOrder(3, 1, 3, 5);
    ASSERT_EQ(5u, pixels.size());
    for (int y = 1, i = 0; y <= 5; ++y, ++i) {
      EXPECT_EQ(std::make_pair(3, y), pixels[i]);
    }
  }

  TEST(Bresenham, DiagonalLineEmitsExpectedPixelCount) {
    // 45-degree line from (0,0) to (5,5): 6 pixels (inclusive
    // endpoints), diagonal each step.
    auto pixels = plottedInOrder(0, 0, 5, 5);
    ASSERT_EQ(6u, pixels.size());
    for (int i = 0; i <= 5; ++i)
      EXPECT_EQ(std::make_pair(i, i), pixels[i]);
  }

  TEST(Bresenham, ReversedDirectionEmitsSamePixelSet) {
    // The pixel SET (not order) should be identical regardless of
    // which endpoint is first. This is the important invariant for
    // wireframe rendering — rendering is independent of edge
    // traversal direction.
    EXPECT_EQ(plotted(0, 0, 7, 3), plotted(7, 3, 0, 0));
    EXPECT_EQ(plotted(-3, 5, 4, -2), plotted(4, -2, -3, 5));
  }

  TEST(Bresenham, MajorAxisIsAlwaysTheLongerAxis) {
    // For a (10, 3) line, x is the major axis: pixel count along x
    // dominates. Total pixels = max(|dx|, |dy|) + 1 = 11.
    auto pixels = plotted(0, 0, 10, 3);
    EXPECT_EQ(11u, pixels.size());

    // For a (3, 10) line, y is major: 11 pixels too.
    auto pixels2 = plotted(0, 0, 3, 10);
    EXPECT_EQ(11u, pixels2.size());
  }

  TEST(Bresenham, NegativeCoordinatesHandledCorrectly) {
    auto pixels = plotted(-5, -3, -1, -1);
    EXPECT_EQ(5u, pixels.size());
    EXPECT_TRUE(pixels.count({-5, -3}));
    EXPECT_TRUE(pixels.count({-1, -1}));
  }

  TEST(Bresenham, AllEightOctantsPlotMatchingEndpoints) {
    // For a fixed |dx| and |dy|, all eight octants (every sign
    // combination of dx, dy and the major-axis swap) should produce
    // a line that includes both endpoints.
    const int positions[][4] = {
      { 0,  0,  7,  3}, { 0,  0, -7,  3}, { 0,  0,  7, -3}, { 0,  0, -7, -3},
      { 0,  0,  3,  7}, { 0,  0, -3,  7}, { 0,  0,  3, -7}, { 0,  0, -3, -7},
    };
    for (const auto& p : positions) {
      auto pixels = plotted(p[0], p[1], p[2], p[3]);
      EXPECT_TRUE(pixels.count({p[0], p[1]})) << "missing start for "
        << p[0] << "," << p[1] << " -> " << p[2] << "," << p[3];
      EXPECT_TRUE(pixels.count({p[2], p[3]})) << "missing end for "
        << p[0] << "," << p[1] << " -> " << p[2] << "," << p[3];
    }
  }

  TEST(Bresenham, OutputPixelsAreContiguous) {
    // Every consecutive plotted pixel pair must be 8-connected
    // (max move of 1 in x or y per step). This is the geometric
    // property that makes Bresenham output look like a line.
    auto pixels = plottedInOrder(0, 0, 17, 11);
    ASSERT_GE(pixels.size(), 2u);
    for (std::size_t i = 1; i < pixels.size(); ++i) {
      int dx = std::abs(pixels[i].first - pixels[i - 1].first);
      int dy = std::abs(pixels[i].second - pixels[i - 1].second);
      EXPECT_LE(dx, 1) << "pixel jump at i=" << i;
      EXPECT_LE(dy, 1) << "pixel jump at i=" << i;
      EXPECT_GT(dx + dy, 0) << "duplicate pixel at i=" << i;
    }
  }
}
