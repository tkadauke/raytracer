#include <gtest/gtest.h>

#include "engine/raster/RasterVisibilitySceneCache.h"
#include "render/materials/MatteMaterial.h"
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
    EXPECT_FALSE(cache.transformedBoundsFor(sphere, Matrix4d()).hit);
    ASSERT_EQ(1u, cache.size());
    ASSERT_EQ(1u, cache.transformedBoundsSize());

    cache.clear();

    EXPECT_EQ(0u, cache.size());
    EXPECT_EQ(0u, cache.transformedBoundsSize());
    EXPECT_FALSE(cache.meshStatsFor(sphere, 0).hit);
  }

  TEST(RasterVisibilitySceneCache, ReusesPrimitiveTransformBounds) {
    engine::raster::RasterVisibilitySceneCache cache;
    render::Sphere sphere(Vector3d::null, 1.0);

    const Matrix4d identity;
    const auto first = cache.transformedBoundsFor(sphere, identity);
    EXPECT_FALSE(first.hit);
    EXPECT_TRUE(first.bounds.isValid());
    EXPECT_EQ(Vector3d(-1.0, -1.0, -1.0), first.bounds.min());
    EXPECT_EQ(Vector3d(1.0, 1.0, 1.0), first.bounds.max());
    EXPECT_EQ(1u, cache.transformedBoundsSize());

    const auto second = cache.transformedBoundsFor(sphere, identity);
    EXPECT_TRUE(second.hit);
    EXPECT_EQ(first.bounds.min(), second.bounds.min());
    EXPECT_EQ(first.bounds.max(), second.bounds.max());
    EXPECT_EQ(1u, cache.transformedBoundsSize());

    const auto translated = cache.transformedBoundsFor(sphere, Matrix4d::translate(2.0, 0.0, 0.0));
    EXPECT_FALSE(translated.hit);
    EXPECT_EQ(Vector3d(1.0, -1.0, -1.0), translated.bounds.min());
    EXPECT_EQ(Vector3d(3.0, 1.0, 1.0), translated.bounds.max());
    EXPECT_EQ(2u, cache.transformedBoundsSize());
  }

  TEST(RasterVisibilitySceneCache, ReusesMaterialCullabilityUntilSidednessChanges) {
    engine::raster::RasterVisibilitySceneCache cache;
    auto material = std::make_shared<render::MatteMaterial>();
    material->setSidedness(render::Material::Sidedness::Front);

    const auto first = cache.materialCullabilityFor(material);
    EXPECT_FALSE(first.hit);
    EXPECT_EQ(engine::raster::Rasterizer::CullMode::Back, first.cullability.defaultCullMode);
    EXPECT_EQ(1u, cache.materialCullabilitySize());

    const auto second = cache.materialCullabilityFor(material);
    EXPECT_TRUE(second.hit);
    EXPECT_EQ(engine::raster::Rasterizer::CullMode::Back, second.cullability.defaultCullMode);
    EXPECT_EQ(1u, cache.materialCullabilitySize());

    material->setSidedness(render::Material::Sidedness::Back);
    const auto changed = cache.materialCullabilityFor(material);
    EXPECT_FALSE(changed.hit);
    EXPECT_EQ(engine::raster::Rasterizer::CullMode::Front, changed.cullability.defaultCullMode);
    EXPECT_EQ(2u, cache.materialCullabilitySize());
  }
}
