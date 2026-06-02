#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "render/State.h"
#include "render/primitives/Intersection.h"
#include "render/primitives/Sphere.h"
#include "render/materials/MatteMaterial.h"
#include "core/math/RayPacket.h"
#include "test/mocks/raytracer/MockPrimitive.h"

namespace IntersectionTest {
  using namespace ::testing;
  using namespace render;
  using namespace render;
  using namespace render;

  TEST(Intersection, ShouldReturnClosestPrimitiveForIntersection) {
    Intersection i;
    auto primitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto primitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    i.add(primitive1);
    i.add(primitive2);
    EXPECT_CALL(*primitive1, calculateBoundingBox())
      .WillOnce(Return(BoundingBoxd(-Vector3d::one, Vector3d::one)));
    EXPECT_CALL(*primitive2, calculateBoundingBox())
      .WillOnce(Return(BoundingBoxd(-Vector3d::one, Vector3d::one)));
    EXPECT_CALL(*primitive1, intersect(_, _, _))
      .WillOnce(DoAll(AddHitPoints(HitPoint(primitive1.get(), 1.0, Vector3d(), Vector3d()),
                                   HitPoint(primitive1.get(), 4.0, Vector3d(), Vector3d())),
                      Return(primitive1.get())));
    EXPECT_CALL(*primitive2, intersect(_, _, _))
      .WillOnce(DoAll(AddHitPoints(HitPoint(primitive2.get(), 2.0, Vector3d(), Vector3d()),
                                   HitPoint(primitive2.get(), 5.0, Vector3d(), Vector3d())),
                      Return(primitive2.get())));

    Rayd ray(Vector3d(0, 0, 0), Vector3d(1, 0, 0));

    State state;
    HitPointInterval hitPoints;
    auto result = i.intersect(ray, hitPoints, state);

    ASSERT_EQ(primitive2.get(), result);
  }

  TEST(Intersection, ShouldReturnSelfIfIntersectionHasMaterial) {
    Intersection i;
    i.setMaterial(std::make_shared<MatteMaterial>());

    auto primitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto primitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    i.add(primitive1);
    i.add(primitive2);
    EXPECT_CALL(*primitive1, calculateBoundingBox())
      .WillOnce(Return(BoundingBoxd(-Vector3d::one, Vector3d::one)));
    EXPECT_CALL(*primitive2, calculateBoundingBox())
      .WillOnce(Return(BoundingBoxd(-Vector3d::one, Vector3d::one)));
    EXPECT_CALL(*primitive1, intersect(_, _, _))
      .WillOnce(DoAll(AddHitPoints(HitPoint(primitive1.get(), 1.0, Vector3d(), Vector3d()),
                                   HitPoint(primitive1.get(), 4.0, Vector3d(), Vector3d())),
                      Return(primitive1.get())));
    EXPECT_CALL(*primitive2, intersect(_, _, _))
      .WillOnce(DoAll(AddHitPoints(HitPoint(primitive2.get(), 2.0, Vector3d(), Vector3d()),
                                   HitPoint(primitive2.get(), 5.0, Vector3d(), Vector3d())),
                      Return(primitive2.get())));

    Rayd ray(Vector3d(0, 0, 0), Vector3d(1, 0, 0));

    State state;
    HitPointInterval hitPoints;
    auto result = i.intersect(ray, hitPoints, state);

    ASSERT_EQ(&i, result);
  }

  TEST(Intersection, ShouldComposeRay4PacketIntervalsForPacketHits) {
    Intersection intersection;
    intersection.setMaterial(std::make_shared<MatteMaterial>());
    intersection.add(std::make_shared<Sphere>(Vector3d(), 1));
    intersection.add(std::make_shared<Sphere>(Vector3d(), 1));
    const Ray4 rays(std::array<Rayd, Ray4::lanes>{
      Rayd(Vector3d(0, 0, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 0, -2), Vector3d(0, 1, 0)),
      Rayd(Vector3d(0, 0, 2), Vector3d(0, 0, 1)), Rayd(Vector3d(3, 0, -2), Vector3d(0, 0, 1))});
    std::array<State, Ray4::lanes> laneStates;
    PrimitivePacketState4 states{&laneStates[0], &laneStates[1], &laneStates[2], &laneStates[3]};

    const auto result = intersection.intersectPacketHits(rays, states);

    ASSERT_TRUE(result.hit(0));
    EXPECT_EQ(&intersection, result.primitive(0));
    EXPECT_FALSE(result.scalarFallback(0));
    EXPECT_FALSE(result.hit(1));
    EXPECT_FALSE(result.hit(2));
    EXPECT_FALSE(result.hit(3));
    for (const State& state : laneStates) {
      EXPECT_EQ(0u, state.packetHitScalarFallbacks);
    }
  }

  TEST(Intersection, ShouldComposeRay8PacketIntervalsForPacketHits) {
    Intersection intersection;
    intersection.setMaterial(std::make_shared<MatteMaterial>());
    intersection.add(std::make_shared<Sphere>(Vector3d(), 1));
    intersection.add(std::make_shared<Sphere>(Vector3d(), 1));
    const Ray8 rays(std::array<Rayd, Ray8::lanes>{
      Rayd(Vector3d(0, 0, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 0, -2), Vector3d(0, 1, 0)),
      Rayd(Vector3d(0, 0, 2), Vector3d(0, 0, 1)), Rayd(Vector3d(3, 0, -2), Vector3d(0, 0, 1)),
      Rayd(Vector3d(0, 0, -3), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 0, 3), Vector3d(0, 0, -1)),
      Rayd(Vector3d(0, 2, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(0.25, 0, -2), Vector3d(0, 0, 1))});
    std::array<State, Ray8::lanes> laneStates;
    PrimitivePacketState8 states{&laneStates[0], &laneStates[1], &laneStates[2], &laneStates[3],
                                 &laneStates[4], &laneStates[5], &laneStates[6], &laneStates[7]};

    const auto result = intersection.intersectPacketHits(rays, states);

    ASSERT_TRUE(result.hit(0));
    EXPECT_EQ(&intersection, result.primitive(0));
    EXPECT_FALSE(result.scalarFallback(0));
    EXPECT_FALSE(result.hit(1));
    EXPECT_FALSE(result.hit(2));
    EXPECT_FALSE(result.hit(3));
    ASSERT_TRUE(result.hit(4));
    EXPECT_EQ(&intersection, result.primitive(4));
    EXPECT_FALSE(result.scalarFallback(4));
    ASSERT_TRUE(result.hit(5));
    EXPECT_EQ(&intersection, result.primitive(5));
    EXPECT_FALSE(result.scalarFallback(5));
    EXPECT_FALSE(result.hit(6));
    ASSERT_TRUE(result.hit(7));
    EXPECT_EQ(&intersection, result.primitive(7));
    EXPECT_FALSE(result.scalarFallback(7));
    for (const State& state : laneStates) {
      EXPECT_EQ(0u, state.packetHitScalarFallbacks);
    }
  }

  TEST(Intersection, ShouldNotReturnAnyPrimitiveIfThereIsNoIntersection) {
    Intersection i;
    auto primitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto primitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    i.add(primitive1);
    i.add(primitive2);
    EXPECT_CALL(*primitive1, calculateBoundingBox())
      .WillOnce(Return(BoundingBoxd(-Vector3d::one, Vector3d::one)));
    EXPECT_CALL(*primitive2, calculateBoundingBox())
      .WillOnce(Return(BoundingBoxd(-Vector3d::one, Vector3d::one)));
    EXPECT_CALL(*primitive1, intersect(_, _, _))
      .WillOnce(Return(static_cast<render::Primitive*>(nullptr)));
    EXPECT_CALL(*primitive2, intersect(_, _, _))
      .WillOnce(Return(static_cast<render::Primitive*>(nullptr)));

    Rayd ray(Vector3d(0, 0, 0), Vector3d(1, 0, 0));

    State state;
    HitPointInterval hitPoints;
    auto result = i.intersect(ray, hitPoints, state);

    ASSERT_EQ(0, result);
  }

  TEST(Intersection, ShouldNotReturnAnyPrimitiveIfNotAllChildrenIntersect) {
    Intersection i;
    auto primitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto primitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    i.add(primitive1);
    i.add(primitive2);
    EXPECT_CALL(*primitive1, calculateBoundingBox())
      .WillOnce(Return(BoundingBoxd(-Vector3d::one, Vector3d::one)));
    EXPECT_CALL(*primitive2, calculateBoundingBox())
      .WillOnce(Return(BoundingBoxd(-Vector3d::one, Vector3d::one)));
    EXPECT_CALL(*primitive1, intersect(_, _, _))
      .WillOnce(DoAll(AddHitPoints(HitPoint(primitive1.get(), 1.0, Vector3d(), Vector3d()),
                                   HitPoint(primitive1.get(), 4.0, Vector3d(), Vector3d())),
                      Return(primitive1.get())));
    EXPECT_CALL(*primitive2, intersect(_, _, _))
      .WillOnce(Return(static_cast<render::Primitive*>(nullptr)));

    Rayd ray(Vector3d(0, 0, 0), Vector3d(1, 0, 0));

    State state;
    HitPointInterval hitPoints;
    auto result = i.intersect(ray, hitPoints, state);

    ASSERT_EQ(0, result);
  }

  TEST(Intersection, ShouldNotReturnAnyPrimitiveIfNotAllChildrenIntersectInOverlappingIntervals) {
    Intersection i;
    auto primitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto primitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    i.add(primitive1);
    i.add(primitive2);
    EXPECT_CALL(*primitive1, calculateBoundingBox())
      .WillOnce(Return(BoundingBoxd(-Vector3d::one, Vector3d::one)));
    EXPECT_CALL(*primitive2, calculateBoundingBox())
      .WillOnce(Return(BoundingBoxd(-Vector3d::one, Vector3d::one)));
    EXPECT_CALL(*primitive1.get(), intersect(_, _, _))
      .WillOnce(DoAll(AddHitPoints(HitPoint(primitive1.get(), 1.0, Vector3d(), Vector3d()),
                                   HitPoint(primitive1.get(), 2.0, Vector3d(), Vector3d())),
                      Return(primitive1.get())));
    EXPECT_CALL(*primitive2.get(), intersect(_, _, _))
      .WillOnce(DoAll(AddHitPoints(HitPoint(primitive2.get(), 3.0, Vector3d(), Vector3d()),
                                   HitPoint(primitive2.get(), 4.0, Vector3d(), Vector3d())),
                      Return(primitive2.get())));

    Rayd ray(Vector3d(0, 0, 0), Vector3d(1, 0, 0));

    State state;
    HitPointInterval hitPoints;
    auto result = i.intersect(ray, hitPoints, state);

    ASSERT_EQ(0, result);
  }

  TEST(Intersection, ShouldNotReturnAnyPrimitiveIfRayOutsideOfBoundingBox) {
    Intersection i;
    auto primitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto primitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    i.add(primitive1);
    i.add(primitive2);
    EXPECT_CALL(*primitive1, calculateBoundingBox()).WillOnce(Return(BoundingBoxd::undefined));
    EXPECT_CALL(*primitive2, calculateBoundingBox()).WillOnce(Return(BoundingBoxd::undefined));

    Rayd ray(Vector3d(0, 0, 0), Vector3d(1, 0, 0));

    State state;
    HitPointInterval hitPoints;
    auto result = i.intersect(ray, hitPoints, state);

    ASSERT_EQ(0, result);
  }

  TEST(Intersection, ShouldReturnTrueForIntersectsIfAllOfTheChildPrimitivesIntersect) {
    Intersection i;
    auto primitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto primitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    i.add(primitive1);
    i.add(primitive2);
    EXPECT_CALL(*primitive1, calculateBoundingBox())
      .WillOnce(Return(BoundingBoxd(-Vector3d::one, Vector3d::one)));
    EXPECT_CALL(*primitive2, calculateBoundingBox())
      .WillOnce(Return(BoundingBoxd(-Vector3d::one, Vector3d::one)));
    EXPECT_CALL(*primitive1, intersects(_, _)).WillOnce(Return(true));
    EXPECT_CALL(*primitive2, intersects(_, _)).WillOnce(Return(true));

    Rayd ray(Vector3d(0, 0, 0), Vector3d(1, 0, 0));

    State state;
    bool result = i.intersects(ray, state);

    ASSERT_TRUE(result);
  }

  TEST(Intersection, ShouldReturnFalseForIntersectsIfThereIsNoIntersection) {
    Intersection i;
    auto primitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto primitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    i.add(primitive1);
    i.add(primitive2);
    EXPECT_CALL(*primitive1, calculateBoundingBox())
      .WillOnce(Return(BoundingBoxd(-Vector3d::one, Vector3d::one)));
    EXPECT_CALL(*primitive2, calculateBoundingBox())
      .WillOnce(Return(BoundingBoxd(-Vector3d::one, Vector3d::one)));
    EXPECT_CALL(*primitive1, intersects(_, _)).WillOnce(Return(false));

    Rayd ray(Vector3d(0, 0, 0), Vector3d(1, 0, 0));

    State state;
    bool result = i.intersects(ray, state);

    ASSERT_FALSE(result);
  }

  TEST(Intersection, ShouldReturnFalseForIntersectsIfNotAllChildrenIntersect) {
    Intersection i;
    auto primitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto primitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    i.add(primitive1);
    i.add(primitive2);
    EXPECT_CALL(*primitive1, calculateBoundingBox())
      .WillOnce(Return(BoundingBoxd(-Vector3d::one, Vector3d::one)));
    EXPECT_CALL(*primitive2, calculateBoundingBox())
      .WillOnce(Return(BoundingBoxd(-Vector3d::one, Vector3d::one)));
    EXPECT_CALL(*primitive1, intersects(_, _)).WillOnce(Return(true));
    EXPECT_CALL(*primitive2, intersects(_, _)).WillOnce(Return(false));

    Rayd ray(Vector3d(0, 0, 0), Vector3d(1, 0, 0));

    State state;
    bool result = i.intersects(ray, state);

    ASSERT_FALSE(result);
  }

  TEST(Intersection, ShouldReturnFalseForIntersectsIfRayOutsideBoundingBox) {
    Intersection i;
    auto primitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto primitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    i.add(primitive1);
    i.add(primitive2);
    EXPECT_CALL(*primitive1, calculateBoundingBox()).WillOnce(Return(BoundingBoxd::undefined));
    EXPECT_CALL(*primitive2, calculateBoundingBox()).WillOnce(Return(BoundingBoxd::undefined));

    Rayd ray(Vector3d(0, 0, 0), Vector3d(1, 0, 0));

    State state;
    bool result = i.intersects(ray, state);

    ASSERT_FALSE(result);
  }

  TEST(Intersection, ShouldReturnBoundingBoxWithOneChild) {
    Intersection i;
    auto mockPrimitive = std::make_shared<NiceMock<MockPrimitive>>();
    i.add(mockPrimitive);

    BoundingBoxd bbox(Vector3d(-1, -1, -1), Vector3d(1, 1, 1));
    EXPECT_CALL(*mockPrimitive, calculateBoundingBox()).WillOnce(Return(bbox));

    ASSERT_EQ(bbox, i.boundingBox());
  }

  TEST(Intersection, ShouldReturnBoundingBoxWithMultipleChildren) {
    Intersection i;
    auto mockPrimitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto mockPrimitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    i.add(mockPrimitive1);
    i.add(mockPrimitive2);

    EXPECT_CALL(*mockPrimitive1, calculateBoundingBox())
      .WillOnce(Return(BoundingBoxd(Vector3d(-1, -1, -1), Vector3d(1, 1, 1))));
    EXPECT_CALL(*mockPrimitive2, calculateBoundingBox())
      .WillOnce(Return(BoundingBoxd(Vector3d(0, 0, 0), Vector3d(2, 2, 2))));

    BoundingBoxd expected(Vector3d(0, 0, 0), Vector3d(1, 1, 1));
    ASSERT_EQ(expected, i.boundingBox());
  }
}
