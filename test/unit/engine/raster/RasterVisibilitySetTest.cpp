#include <gtest/gtest.h>

#include "engine/raster/RasterVisibilitySet.h"

namespace RasterVisibilitySetTest {
  using engine::raster::RasterVisibilitySet;

  TEST(RasterVisibilitySet, TracksVisibleAndRejectedLeaves) {
    RasterVisibilitySet set;
    set.addVisibleLeaf(12);
    set.addRejectedLeaf(RasterVisibilitySet::RejectionReason::Frustum, 8);

    EXPECT_TRUE(set.leafVisible(0));
    EXPECT_FALSE(set.leafVisible(1));
    EXPECT_TRUE(set.leafVisible(2));
    EXPECT_EQ(2u, set.leafCount());
    EXPECT_EQ(20u, set.inputTriangleCount());
    EXPECT_EQ(1u, set.visibleLeafCount());
    EXPECT_EQ(12u, set.visibleTriangleCount());
    EXPECT_EQ(1u, set.rejectedLeafCount());
    EXPECT_EQ(8u, set.rejectedTriangleCount());
    EXPECT_EQ(1u, set.rejectedLeafCount(RasterVisibilitySet::RejectionReason::Frustum));
    EXPECT_EQ(8u, set.rejectedTriangleCount(RasterVisibilitySet::RejectionReason::Frustum));
  }
}
