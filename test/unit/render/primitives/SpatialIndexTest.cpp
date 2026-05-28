#include <gtest/gtest.h>

#include "core/math/HitPointInterval.h"
#include "render/State.h"
#include "render/primitives/AccelerationPolicy.h"
#include "render/primitives/BVH.h"
#include "render/primitives/Composite.h"
#include "render/primitives/Grid.h"
#include "render/primitives/SpatialIndex.h"
#include "render/primitives/SpatialIndexFactory.h"
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

  TEST(SpatialIndex, FactoryCreatesPlainCompositeFallback) {
    auto index = makeSpatialIndex(SpatialIndexKind::Linear);
    auto primitive = spatialIndexPrimitive(index);

    ASSERT_NE(nullptr, index);
    ASSERT_NE(nullptr, primitive);
    EXPECT_NE(nullptr, std::dynamic_pointer_cast<Composite>(primitive));
  }

  TEST(SpatialIndex, FactoryCreatedIndexesShareOneCallerContract) {
    for (auto kind : {SpatialIndexKind::Linear, SpatialIndexKind::Grid, SpatialIndexKind::BVH}) {
      auto index = makeSpatialIndex(kind);
      auto sphere = std::make_shared<Sphere>(Vector3d::null, 1.0);

      index->add(sphere);
      index->setup();

      Rayd ray(Vector3d(0, 0, -10), Vector3d(0, 0, 1));
      State state;
      HitPointInterval hitPoints;
      EXPECT_EQ(sphere.get(), index->intersect(ray, hitPoints, state));
      EXPECT_TRUE(index->intersects(ray, state));
      EXPECT_EQ(spatialIndexPrimitive(index)->boundingBox(), index->bounds());
    }
  }

  TEST(AccelerationPolicy, AutomaticConservativelySelectsGrid) {
    const AccelerationAnalysis analysis{24};
    const auto decision = AccelerationPolicy::automatic().choose(analysis);

    EXPECT_EQ(AccelerationMode::Automatic, decision.requestedMode);
    EXPECT_EQ(SpatialIndexKind::Grid, decision.spatialIndexKind);
    EXPECT_STREQ("automatic_conservative_grid", decision.reason);
    EXPECT_EQ("requested=automatic selected=grid reason=automatic_conservative_grid",
              diagnosticString(decision));
  }

  TEST(AccelerationPolicy, ManualOverrideSelectsRequestedIndex) {
    const AccelerationAnalysis analysis{24};
    const auto decision = AccelerationPolicy::manual(SpatialIndexKind::BVH).choose(analysis);

    EXPECT_EQ(AccelerationMode::BVH, decision.requestedMode);
    EXPECT_EQ(SpatialIndexKind::BVH, decision.spatialIndexKind);
    EXPECT_STREQ("manual_override", decision.reason);
  }
}
