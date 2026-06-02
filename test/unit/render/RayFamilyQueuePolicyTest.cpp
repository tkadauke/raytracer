#include <gtest/gtest.h>

#include "render/RayFamilyQueuePolicy.h"

namespace render {
  namespace {
    TEST(RayFamilyQueuePolicy, KeepsHistoricalRenderCliDefaultForStandardImage) {
      const RayFamilyQueuePolicy policy(640, 480, 1, 8);

      EXPECT_EQ(300, policy.queueSize());
    }

    TEST(RayFamilyQueuePolicy, KeepsAtLeastOneTilePerWorkerForSmallImages) {
      const RayFamilyQueuePolicy policy(32, 16, 1, 10);

      EXPECT_EQ(10, policy.queueSize());
    }

    TEST(RayFamilyQueuePolicy, IncludesSampleCountInAutomaticQueueSize) {
      const RayFamilyQueuePolicy policy(128, 96, 8, 4);

      EXPECT_EQ(256, policy.queueSize());
    }

    TEST(RayFamilyQueuePolicy, HonorsCustomMaximumQueueSize) {
      const RayFamilyQueuePolicy policy(640, 480, 1, 8, 64);

      EXPECT_EQ(64, policy.queueSize());
    }

    TEST(RayFamilyQueuePolicy, ClampsInvalidInputsToAUsableQueue) {
      const RayFamilyQueuePolicy policy(0, -1, 0, -4);

      EXPECT_EQ(1, policy.queueSize());
    }
  }
}
