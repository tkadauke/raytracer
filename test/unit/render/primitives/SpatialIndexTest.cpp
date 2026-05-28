#include <gtest/gtest.h>

#include "core/math/HitPointInterval.h"
#include "render/State.h"
#include "render/primitives/BVH.h"
#include "render/primitives/Composite.h"
#include "render/primitives/Grid.h"
#include "render/primitives/SpatialIndex.h"
#include "render/primitives/Sphere.h"

namespace SpatialIndexTest {
  using namespace render;

  TEST(SpatialIndex, CompositeProvidesFlatFallbackContract) {
    Composite composite;
    SpatialIndex& index = composite;
    auto sphere = std::make_shared<Sphere>(Vector3d::null, 1.0);

    index.add(sphere);
    index.setup();

    ASSERT_EQ(sphere, index.primitives().front());
    ASSERT_EQ(composite.boundingBox(), index.bounds());

    Rayd ray(Vector3d(0, 0, -10), Vector3d(0, 0, 1));
    State state;
    HitPointInterval hitPoints;
    EXPECT_EQ(sphere.get(), index.intersect(ray, hitPoints, state));
  }

  TEST(SpatialIndex, BVHProvidesAcceleratedContract) {
    BVH bvh;
    SpatialIndex& index = bvh;
    auto sphere = std::make_shared<Sphere>(Vector3d::null, 1.0);

    index.add(sphere);
    index.setup();

    ASSERT_EQ(sphere, index.primitives().front());
    ASSERT_EQ(bvh.boundingBox(), index.bounds());

    Rayd ray(Vector3d(0, 0, -10), Vector3d(0, 0, 1));
    State state;
    HitPointInterval hitPoints;
    EXPECT_EQ(sphere.get(), index.intersect(ray, hitPoints, state));
    EXPECT_TRUE(index.intersects(ray, state));
  }

  TEST(SpatialIndex, GridProvidesAcceleratedContract) {
    Grid grid;
    SpatialIndex& index = grid;
    auto sphere = std::make_shared<Sphere>(Vector3d::null, 1.0);

    index.add(sphere);
    index.setup();

    ASSERT_EQ(sphere, index.primitives().front());
    ASSERT_EQ(grid.boundingBox(), index.bounds());

    Rayd ray(Vector3d(0, 0, -10), Vector3d(0, 0, 1));
    State state;
    HitPointInterval hitPoints;
    EXPECT_EQ(sphere.get(), index.intersect(ray, hitPoints, state));
    EXPECT_TRUE(index.intersects(ray, state));
  }
}
