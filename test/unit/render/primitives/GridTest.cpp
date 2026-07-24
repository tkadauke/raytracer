#include <gtest/gtest.h>

#include "core/math/RayPacket.h"
#include "render/State.h"
#include "render/primitives/Grid.h"
#include "render/primitives/Sphere.h"
#include "test/helpers/PrimitiveTestHelper.h"
#include "test/mocks/raytracer/MockPrimitive.h"

namespace GridTest {
  using namespace ::testing;
  using namespace render;
  using test::helpers::PacketStates4;
  using test::helpers::PacketStates8;

  // Convenience: wire a MockPrimitive's bounding box and (optionally) its
  // intersect/intersects expectations with sensible NiceMock-friendly defaults.
  std::shared_ptr<NiceMock<MockPrimitive>> primitiveAt(const BoundingBoxd& bbox) {
    auto p = std::make_shared<NiceMock<MockPrimitive>>();
    ON_CALL(*p, calculateBoundingBox()).WillByDefault(Return(bbox));
    return p;
  }

  TEST(Grid, ShouldStartEmpty) {
    Grid grid;
    ASSERT_TRUE(grid.primitives().empty());
  }

  TEST(Grid, ShouldNotIntersectIfBoundingBoxIsInfinite) {
    Grid grid;
    auto plane = primitiveAt(BoundingBoxd::infinity);
    grid.add(plane);
    grid.setup();

    EXPECT_CALL(*plane, intersect(_, _, _)).Times(0);

    Rayd ray(Vector3d(0, 0, -10), Vector3d(0, 0, 1));
    State state;
    HitPointInterval hits;
    ASSERT_EQ(nullptr, grid.intersect(ray, hits, state));
  }

  TEST(Grid, ShouldNotReportIntersectsIfBoundingBoxIsInfinite) {
    Grid grid;
    grid.add(primitiveAt(BoundingBoxd::infinity));
    grid.setup();

    Rayd ray(Vector3d(0, 0, -10), Vector3d(0, 0, 1));
    State state;
    ASSERT_FALSE(grid.intersects(ray, state));
  }

  TEST(Grid, ShouldDelegateToContainedPrimitiveOnHit) {
    Grid grid;
    auto p = primitiveAt(BoundingBoxd(Vector3d(-1, -1, -1), Vector3d(1, 1, 1)));
    grid.add(p);
    grid.setup();

    EXPECT_CALL(*p, intersect(_, _, _))
      .WillOnce(DoAll(AddHitPoint(HitPoint(p.get(), 9.0, Vector3d(0, 0, -1), Vector3d(0, 0, -1))),
                      Return(p.get())));

    Rayd ray(Vector3d(0, 0, -10), Vector3d(0, 0, 1));
    State state;
    HitPointInterval hits;
    ASSERT_EQ(p.get(), grid.intersect(ray, hits, state));
  }

  TEST(Grid, ShouldReturnNullWhenRayMissesGridBox) {
    Grid grid;
    auto p = primitiveAt(BoundingBoxd(Vector3d(-1, -1, -1), Vector3d(1, 1, 1)));
    grid.add(p);
    grid.setup();

    // Primitive::intersect must not be invoked for a ray that misses the
    // grid's outer bounding box altogether.
    EXPECT_CALL(*p, intersect(_, _, _)).Times(0);

    Rayd ray(Vector3d(10, 10, -10), Vector3d(0, 0, 1));
    State state;
    HitPointInterval hits;
    ASSERT_EQ(nullptr, grid.intersect(ray, hits, state));
  }

  TEST(Grid, ShouldReportIntersectsForShadowRayHittingPrimitive) {
    Grid grid;
    auto p = primitiveAt(BoundingBoxd(Vector3d(-1, -1, -1), Vector3d(1, 1, 1)));
    grid.add(p);
    grid.setup();

    EXPECT_CALL(*p, intersects(_, _)).WillOnce(Return(true));

    Rayd ray(Vector3d(0, 0, -10), Vector3d(0, 0, 1));
    State state;
    ASSERT_TRUE(grid.intersects(ray, state));
  }

  TEST(Grid, ShouldNotReportIntersectsForShadowRayMissingGridBox) {
    Grid grid;
    auto p = primitiveAt(BoundingBoxd(Vector3d(-1, -1, -1), Vector3d(1, 1, 1)));
    grid.add(p);
    grid.setup();

    EXPECT_CALL(*p, intersects(_, _)).Times(0);

    Rayd ray(Vector3d(10, 10, -10), Vector3d(0, 0, 1));
    State state;
    ASSERT_FALSE(grid.intersects(ray, state));
  }

  TEST(Grid, ShouldHandleRayOriginInsideGrid) {
    // A primitive that fills the whole grid lives in every cell; the ray
    // starts at the origin inside cell [1,1,1] and the hit at t=1.0 lies in
    // cell [2,1,1]. The DDA visits both cells, so the mock returns the same
    // hit on every call and the grid returns it from cell [2,1,1] where
    // tx_next is past the hit distance.
    Grid grid;
    auto p = primitiveAt(BoundingBoxd(Vector3d(-2, -2, -2), Vector3d(2, 2, 2)));
    grid.add(p);
    grid.setup();

    EXPECT_CALL(*p, intersect(_, _, _))
      .WillRepeatedly(
        DoAll(AddHitPoint(HitPoint(p.get(), 1.0, Vector3d(1, 0, 0), Vector3d(1, 0, 0))),
              Return(p.get())));

    Rayd ray(Vector3d(0, 0, 0), Vector3d(1, 0, 0));
    State state;
    HitPointInterval hits;
    ASSERT_EQ(p.get(), grid.intersect(ray, hits, state));
  }

  TEST(Grid, ShouldTraverseAlongAxisAlignedRay) {
    // dx == 0 (and dz == 0) exercises the special-case branches at lines
    // 119-123, 152-156 in Grid.cpp where the DDA next-step is set to
    // numeric_limits::max().
    Grid grid;
    auto p = primitiveAt(BoundingBoxd(Vector3d(-1, -1, -1), Vector3d(1, 1, 1)));
    grid.add(p);
    grid.setup();

    EXPECT_CALL(*p, intersect(_, _, _))
      .WillOnce(DoAll(AddHitPoint(HitPoint(p.get(), 9.0, Vector3d(0, -1, 0), Vector3d(0, -1, 0))),
                      Return(p.get())));

    Rayd ray(Vector3d(0, -10, 0), Vector3d(0, 1, 0));
    State state;
    HitPointInterval hits;
    ASSERT_EQ(p.get(), grid.intersect(ray, hits, state));
  }

  TEST(Grid, ShouldKeepTraversingWhenFirstCellHasNoHit) {
    // Two primitives that span different cells — the ray crosses the empty
    // cells around the first one and lands on the second. Force the first
    // primitive's intersect to miss so the DDA has to advance.
    Grid grid;
    auto front = primitiveAt(BoundingBoxd(Vector3d(-5, -1, -1), Vector3d(-3, 1, 1)));
    auto back = primitiveAt(BoundingBoxd(Vector3d(3, -1, -1), Vector3d(5, 1, 1)));
    grid.add(front);
    grid.add(back);
    grid.setup();

    EXPECT_CALL(*front, intersect(_, _, _))
      .Times(AnyNumber())
      .WillRepeatedly(Return(static_cast<render::Primitive*>(nullptr)));
    EXPECT_CALL(*back, intersect(_, _, _))
      .WillOnce(
        DoAll(AddHitPoint(HitPoint(back.get(), 8.0, Vector3d(-1, 0, 0), Vector3d(-1, 0, 0))),
              Return(back.get())));

    Rayd ray(Vector3d(-10, 0, 0), Vector3d(1, 0, 0));
    State state;
    HitPointInterval hits;
    ASSERT_EQ(back.get(), grid.intersect(ray, hits, state));
  }

  TEST(Grid, ShouldHandlePrimitivesOverlappingTheSameCell) {
    // When two primitives' bounding boxes overlap a single cell, Grid::setup
    // collapses them into an internal Composite. Both primitives must still
    // be reachable through intersect.
    Grid grid;
    auto a = primitiveAt(BoundingBoxd(Vector3d(-1, -1, -1), Vector3d(1, 1, 1)));
    auto b = primitiveAt(BoundingBoxd(Vector3d(-0.5, -0.5, -0.5), Vector3d(0.5, 0.5, 0.5)));
    grid.add(a);
    grid.add(b);
    grid.setup();

    EXPECT_CALL(*a, intersect(_, _, _))
      .Times(AnyNumber())
      .WillRepeatedly(Return(static_cast<render::Primitive*>(nullptr)));
    EXPECT_CALL(*b, intersect(_, _, _))
      .WillOnce(DoAll(AddHitPoint(HitPoint(b.get(), 9.5, Vector3d(0, 0, -0.5), Vector3d(0, 0, -1))),
                      Return(b.get())));

    Rayd ray(Vector3d(0, 0, -10), Vector3d(0, 0, 1));
    State state;
    HitPointInterval hits;
    ASSERT_EQ(b.get(), grid.intersect(ray, hits, state));
  }

  TEST(Grid, ShouldHandleRayFromBehindPrimitive) {
    // Ray pointing in the negative direction — exercises the dx<0 / dy<0 /
    // dz<0 branches in the DDA.
    Grid grid;
    auto p = primitiveAt(BoundingBoxd(Vector3d(-1, -1, -1), Vector3d(1, 1, 1)));
    grid.add(p);
    grid.setup();

    EXPECT_CALL(*p, intersect(_, _, _))
      .WillOnce(DoAll(AddHitPoint(HitPoint(p.get(), 9.0, Vector3d(0, 0, 1), Vector3d(0, 0, 1))),
                      Return(p.get())));

    Rayd ray(Vector3d(0, 0, 10), Vector3d(0, 0, -1));
    State state;
    HitPointInterval hits;
    ASSERT_EQ(p.get(), grid.intersect(ray, hits, state));
  }

  TEST(Grid, ShouldMaterializeRay4PacketHitsThroughDdaWithoutScalarFallback) {
    Grid grid;
    auto sphere = std::make_shared<Sphere>(Vector3d(), 1.0);
    grid.add(sphere);
    grid.setup();
    const Ray4 rays(std::array<Rayd, Ray4::lanes>{
      Rayd(Vector3d(0, 0, -3), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 3, -3), Vector3d(0, 0, 1)),
      Rayd(Vector3d(0, 0, 3), Vector3d(0, 0, -1)), Rayd(Vector3d(3, 0, -3), Vector3d(0, 0, 1))});
    PacketStates4 ps;

    const auto result = grid.intersectPacketHits(rays, ps.states);

    ASSERT_TRUE(result.hit(0));
    EXPECT_EQ(sphere.get(), result.primitive(0));
    EXPECT_FALSE(result.scalarFallback(0));
    EXPECT_FALSE(result.hit(1));
    ASSERT_TRUE(result.hit(2));
    EXPECT_EQ(sphere.get(), result.primitive(2));
    EXPECT_FALSE(result.scalarFallback(2));
    EXPECT_FALSE(result.hit(3));
    for (const State& state : ps.lanes) {
      EXPECT_EQ(0u, state.packetHitScalarFallbacks);
    }
  }

  TEST(Grid, ShouldMaterializeRay8PacketHitsThroughDdaWithoutScalarFallback) {
    Grid grid;
    auto sphere = std::make_shared<Sphere>(Vector3d(), 1.0);
    grid.add(sphere);
    grid.setup();
    const Ray8 rays(std::array<Rayd, Ray8::lanes>{
      Rayd(Vector3d(0, 0, -3), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 3, -3), Vector3d(0, 0, 1)),
      Rayd(Vector3d(0, 0, 3), Vector3d(0, 0, -1)), Rayd(Vector3d(3, 0, -3), Vector3d(0, 0, 1)),
      Rayd(Vector3d(0, 0, -4), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 0, 4), Vector3d(0, 0, -1)),
      Rayd(Vector3d(0, 2, -3), Vector3d(0, 0, 1)), Rayd(Vector3d(0.5, 0, -3), Vector3d(0, 0, 1))});
    PacketStates8 ps;

    const auto result = grid.intersectPacketHits(rays, ps.states);

    ASSERT_TRUE(result.hit(0));
    EXPECT_EQ(sphere.get(), result.primitive(0));
    EXPECT_FALSE(result.scalarFallback(0));
    EXPECT_FALSE(result.hit(1));
    ASSERT_TRUE(result.hit(2));
    EXPECT_EQ(sphere.get(), result.primitive(2));
    EXPECT_FALSE(result.scalarFallback(2));
    EXPECT_FALSE(result.hit(3));
    ASSERT_TRUE(result.hit(4));
    EXPECT_EQ(sphere.get(), result.primitive(4));
    EXPECT_FALSE(result.scalarFallback(4));
    ASSERT_TRUE(result.hit(5));
    EXPECT_EQ(sphere.get(), result.primitive(5));
    EXPECT_FALSE(result.scalarFallback(5));
    EXPECT_FALSE(result.hit(6));
    ASSERT_TRUE(result.hit(7));
    EXPECT_EQ(sphere.get(), result.primitive(7));
    EXPECT_FALSE(result.scalarFallback(7));
    for (const State& state : ps.lanes) {
      EXPECT_EQ(0u, state.packetHitScalarFallbacks);
    }
  }

  TEST(Grid, ShouldIgnorePrimitivesWithEmptyBoundingBoxOnSetup) {
    // A primitive that reports an empty bbox shouldn't break setup — Grid::
    // setup has an explicit early-continue on empty/undefined bboxes.
    Grid grid;
    auto real = primitiveAt(BoundingBoxd(Vector3d(-1, -1, -1), Vector3d(1, 1, 1)));
    auto empty = std::make_shared<NiceMock<MockPrimitive>>();
    ON_CALL(*empty, calculateBoundingBox()).WillByDefault(Return(BoundingBoxd()));
    grid.add(real);
    grid.add(empty);
    ASSERT_NO_THROW(grid.setup());
  }

  TEST(Grid, ShouldHandleDegenerateAxisOnSetup) {
    // A primitive whose bbox is zero-thickness on one axis (a flat
    // Rectangle in its plane, a Disk on its plane) used to overflow
    // Grid::setup: the textbook s = cbrt(vol/N) collapses to ~0 along
    // a degenerate axis, producing ~262k cells along the others, and
    // m_numX * m_numY * m_numZ overflows int. The fix treats axes
    // below `1e-6 × maxAxis` as degenerate and assigns one cell along
    // them.
    Grid grid;
    // Bbox spanning [-1, 1] in X and Z, exact-zero thickness in Y.
    grid.add(primitiveAt(BoundingBoxd(Vector3d(-1, 0, -1), Vector3d(1, 0, 1))));
    ASSERT_NO_THROW(grid.setup());
  }
}
