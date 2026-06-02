#include <gtest/gtest.h>

#include "render/TilePlan.h"

#include <set>
#include <utility>

namespace TilePlanTest {
  using namespace render;

  void expectRect(const Recti& rect, int left, int top, int width, int height) {
    EXPECT_EQ(left, rect.left());
    EXPECT_EQ(top, rect.top());
    EXPECT_EQ(width, rect.width());
    EXPECT_EQ(height, rect.height());
  }

  TEST(TilePlan, ShouldPartitionWholeBufferWithoutOverlap) {
    const TilePlan plan = TilePlan::forBuffer(7, 5, 6);

    std::set<std::pair<int, int>> pixels;
    int area = 0;
    for (int row = 0; row != plan.rows(); ++row) {
      for (int col = 0; col != plan.cols(); ++col) {
        const Recti rect = plan.rect(row, col);
        area += rect.width() * rect.height();
        for (int y = rect.top(); y != rect.bottom(); ++y) {
          for (int x = rect.left(); x != rect.right(); ++x) {
            ASSERT_TRUE(pixels.insert({x, y}).second);
          }
        }
      }
    }

    ASSERT_EQ(35, area);
    ASSERT_EQ(35u, pixels.size());
  }

  TEST(TilePlan, ShouldClampTileCountToPixelCount) {
    const TilePlan plan = TilePlan::forBuffer(3, 2, 100);

    ASSERT_EQ(2, plan.rows());
    ASSERT_EQ(3, plan.cols());
    ASSERT_EQ(6u, plan.size());
  }

  TEST(TilePlan, ShouldMapPixelsBackToOwningTile) {
    const TilePlan plan = TilePlan::forBuffer(7, 5, 6);

    ASSERT_EQ(0, plan.columnForX(0));
    ASSERT_EQ(0, plan.columnForX(1));
    ASSERT_EQ(1, plan.columnForX(2));
    ASSERT_EQ(1, plan.columnForX(3));
    ASSERT_EQ(2, plan.columnForX(4));
    ASSERT_EQ(2, plan.columnForX(6));

    ASSERT_EQ(0, plan.rowForY(0));
    ASSERT_EQ(0, plan.rowForY(1));
    ASSERT_EQ(1, plan.rowForY(2));
    ASSERT_EQ(1, plan.rowForY(4));
  }

  TEST(TilePlan, ShouldTreatOneRequestedTileAsFullFrame) {
    const TilePlan plan = TilePlan::forBuffer(4, 3, 1);

    ASSERT_TRUE(plan.isSingleTile());
    EXPECT_EQ(4, plan.maxTileWidth());
    EXPECT_EQ(3, plan.maxTileHeight());
    EXPECT_EQ(12, plan.maxTilePixels());
    EXPECT_DOUBLE_EQ(12.0, plan.averageTilePixels());
    expectRect(plan.rect(0, 0), 0, 0, 4, 3);
    expectRect(plan.fullRect(), 0, 0, 4, 3);
  }

  TEST(TilePlan, ShouldReportTilePixelShape) {
    const TilePlan plan = TilePlan::forBuffer(7, 5, 6);

    EXPECT_EQ(3, plan.maxTileWidth());
    EXPECT_EQ(3, plan.maxTileHeight());
    EXPECT_EQ(9, plan.maxTilePixels());
    EXPECT_DOUBLE_EQ(35.0 / 6.0, plan.averageTilePixels());
  }

  TEST(TilePlan, ShouldReturnEmptyPlanForEmptyBuffer) {
    const TilePlan plan = TilePlan::forBuffer(0, 3, 4);

    ASSERT_TRUE(plan.empty());
    ASSERT_EQ(0u, plan.size());
    EXPECT_EQ(0, plan.maxTileWidth());
    EXPECT_EQ(0, plan.maxTileHeight());
    EXPECT_EQ(0, plan.maxTilePixels());
    EXPECT_DOUBLE_EQ(0.0, plan.averageTilePixels());
  }
}
