#include <gtest/gtest.h>

#include "engine/raster/RasterVisibilitySceneCache.h"
#include "render/primitives/Sphere.h"

namespace RasterVisibilitySceneCacheTest {
  TEST(RasterVisibilitySceneCache, ReusesPrimitiveLodMeshStats) {
    engine::raster::RasterVisibilitySceneCache cache;
    render::Sphere sphere(Vector3d::null, 1.0);

    const auto first = cache.meshStatsFor(sphere, 0);
    EXPECT_FALSE(first.hit);
    EXPECT_GT(first.stats.triangleCount, 0u);
    EXPECT_GT(first.stats.faceCount, 0u);
    ASSERT_NE(nullptr, first.stats.mesh);
    EXPECT_EQ(1u, cache.size());

    const auto second = cache.meshStatsFor(sphere, 0);
    EXPECT_TRUE(second.hit);
    EXPECT_EQ(first.stats.triangleCount, second.stats.triangleCount);
    EXPECT_EQ(first.stats.faceCount, second.stats.faceCount);
    EXPECT_EQ(first.stats.mesh, second.stats.mesh);
    EXPECT_EQ(1u, cache.size());

    const auto differentLod = cache.meshStatsFor(sphere, 1);
    EXPECT_FALSE(differentLod.hit);
    EXPECT_GT(differentLod.stats.triangleCount, first.stats.triangleCount);
    EXPECT_EQ(2u, cache.size());
  }

  TEST(RasterVisibilitySceneCache, ClearsCachedMeshStats) {
    engine::raster::RasterVisibilitySceneCache cache;
    render::Sphere sphere(Vector3d::null, 1.0);

    EXPECT_FALSE(cache.meshStatsFor(sphere, 0).hit);
    ASSERT_EQ(1u, cache.size());

    cache.clear();

    EXPECT_EQ(0u, cache.size());
    EXPECT_FALSE(cache.meshStatsFor(sphere, 0).hit);
  }
}
