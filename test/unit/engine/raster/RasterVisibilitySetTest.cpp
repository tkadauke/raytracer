#include <gtest/gtest.h>

#include "engine/raster/RasterVisibilitySet.h"

namespace RasterVisibilitySetTest {
  using engine::raster::RasterVisibilitySet;

  TEST(RasterVisibilitySet, TracksVisibleAndRejectedLeaves) {
    RasterVisibilitySet set;
    set.addVisibleLeaf(12, 5);
    set.addRejectedLeaf(RasterVisibilitySet::RejectionReason::Frustum, 8, 3);

    EXPECT_TRUE(set.leafVisible(0));
    EXPECT_FALSE(set.leafVisible(1));
    EXPECT_TRUE(set.leafVisible(2));
    EXPECT_EQ(2u, set.leafCount());
    EXPECT_EQ(5u, set.leafFaceCount(0));
    EXPECT_EQ(3u, set.leafFaceCount(1));
    EXPECT_EQ(0u, set.leafFaceCount(2));
    EXPECT_EQ(20u, set.inputTriangleCount());
    EXPECT_EQ(1u, set.visibleLeafCount());
    EXPECT_EQ(12u, set.visibleTriangleCount());
    EXPECT_EQ(1u, set.rejectedLeafCount());
    EXPECT_EQ(8u, set.rejectedTriangleCount());
    EXPECT_EQ(1u, set.rejectedLeafCount(RasterVisibilitySet::RejectionReason::Frustum));
    EXPECT_EQ(8u, set.rejectedTriangleCount(RasterVisibilitySet::RejectionReason::Frustum));
    EXPECT_EQ(0u, set.rejectedLeafCount(RasterVisibilitySet::RejectionReason::Backface));
    EXPECT_EQ(0u, set.rejectedTriangleCount(RasterVisibilitySet::RejectionReason::Backface));
  }

  TEST(RasterVisibilitySet, StoresOptionalVisibleLeafOrder) {
    RasterVisibilitySet set;
    EXPECT_FALSE(set.hasVisibleLeafOrder());

    set.setVisibleLeafOrder({2, 0});

    ASSERT_TRUE(set.hasVisibleLeafOrder());
    ASSERT_EQ(2u, set.visibleLeafOrder().size());
    EXPECT_EQ(2u, set.visibleLeafOrder()[0]);
    EXPECT_EQ(0u, set.visibleLeafOrder()[1]);
  }

  TEST(RasterVisibilitySet, StoresVisibleLeafTileCoverage) {
    RasterVisibilitySet set;
    set.setTileGrid(64, 32, 16, 16);
    set.addVisibleLeaf(3, 1);
    set.addVisibleLeaf(2, 1);
    set.addRejectedLeaf(RasterVisibilitySet::RejectionReason::Frustum, 5, 1);

    ASSERT_TRUE(set.hasTileGrid());
    EXPECT_EQ(4, set.tileGrid().columns);
    EXPECT_EQ(2, set.tileGrid().rows);
    EXPECT_EQ(8u, set.tileGrid().tileCount());

    set.setVisibleLeafTiles(0, {3, 2, 2, 99});
    set.setVisibleLeafTiles(2, {1});

    EXPECT_EQ(2u, set.visibleLeafTileReferenceCount());
    EXPECT_EQ(2u, set.coveredTileCount());
    EXPECT_EQ(1u, set.tileUncertainVisibleLeafCount());
  }
}
