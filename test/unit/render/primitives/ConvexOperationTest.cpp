#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "render/State.h"
#include "render/primitives/ConvexOperation.h"
#include "render/materials/MatteMaterial.h"
#include "core/math/RayPacket.h"
#include "test/mocks/raytracer/MockPrimitive.h"

namespace testing {
  class MockConvexOperation : public render::ConvexOperation {
  public:
    Vector3d farthestPoint(const Vector3d& direction) const {
      return direction.x() < 0 ? Vector3d(-2, 0, 0) : Vector3d(2, 0, 0);
    }
  };
}

namespace ConvexOperationTest {
  using namespace ::testing;
  using namespace render;
  using namespace render;
  using namespace render;

  TEST(ConvexOperation, ShouldReturnSelfForConvexOperation) {
    MockConvexOperation i;
    auto primitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto primitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    i.add(primitive1);
    i.add(primitive2);
    EXPECT_CALL(*primitive1, calculateBoundingBox())
      .WillOnce(Return(BoundingBoxd(-Vector3d::one, Vector3d::one)));
    EXPECT_CALL(*primitive2, calculateBoundingBox())
      .WillOnce(Return(BoundingBoxd(Vector3d::one, Vector3d::one)));

    Rayd ray(Vector3d(-5, 0, 0), Vector3d(1, 0, 0));

    State state;
    HitPointInterval hitPoints;
    auto result = i.intersect(ray, hitPoints, state);

    ASSERT_EQ(Vector3d(-2, 0, 0), hitPoints.min().point());
    ASSERT_EQ(Vector3d(2, 0, 0), hitPoints.max().point());
    ASSERT_EQ(&i, result);
  }

  TEST(ConvexOperation, ShouldUseScalarCsgSemanticsForRay4PacketHits) {
    MockConvexOperation operation;
    auto primitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto primitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    operation.add(primitive1);
    operation.add(primitive2);
    EXPECT_CALL(*primitive1, calculateBoundingBox())
      .WillOnce(Return(BoundingBoxd(-Vector3d::one, Vector3d::one)));
    EXPECT_CALL(*primitive2, calculateBoundingBox())
      .WillOnce(Return(BoundingBoxd(Vector3d::one, Vector3d::one)));
    const Ray4 rays(std::array<Rayd, Ray4::lanes>{
      Rayd(Vector3d(-5, 0, 0), Vector3d(1, 0, 0)), Rayd(Vector3d(-5, 3, 0), Vector3d(1, 0, 0)),
      Rayd(Vector3d(5, 0, 0), Vector3d(1, 0, 0)), Rayd(Vector3d(0, 5, 0), Vector3d(1, 0, 0))});
    std::array<State, Ray4::lanes> laneStates;
    PrimitivePacketState4 states{&laneStates[0], &laneStates[1], &laneStates[2], &laneStates[3]};

    const auto result = operation.intersectPacketHits(rays, states);

    ASSERT_TRUE(result.hit(0));
    EXPECT_EQ(&operation, result.primitive(0));
    EXPECT_FALSE(result.hit(1));
    EXPECT_FALSE(result.hit(2));
    EXPECT_FALSE(result.hit(3));
    for (const State& state : laneStates) {
      EXPECT_EQ(1u, state.packetHitScalarFallbacks);
    }
  }

  TEST(ConvexOperation, ShouldUseScalarCsgSemanticsForRay8PacketHits) {
    MockConvexOperation operation;
    auto primitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto primitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    operation.add(primitive1);
    operation.add(primitive2);
    EXPECT_CALL(*primitive1, calculateBoundingBox())
      .WillOnce(Return(BoundingBoxd(-Vector3d::one, Vector3d::one)));
    EXPECT_CALL(*primitive2, calculateBoundingBox())
      .WillOnce(Return(BoundingBoxd(Vector3d::one, Vector3d::one)));
    const Ray8 rays(std::array<Rayd, Ray8::lanes>{
      Rayd(Vector3d(-5, 0, 0), Vector3d(1, 0, 0)), Rayd(Vector3d(-5, 3, 0), Vector3d(1, 0, 0)),
      Rayd(Vector3d(5, 0, 0), Vector3d(1, 0, 0)), Rayd(Vector3d(0, 5, 0), Vector3d(1, 0, 0)),
      Rayd(Vector3d(-6, 0, 0), Vector3d(1, 0, 0)), Rayd(Vector3d(6, 0, 0), Vector3d(-1, 0, 0)),
      Rayd(Vector3d(0, 0, 0), Vector3d(1, 0, 0)), Rayd(Vector3d(-4, 0, 0), Vector3d(1, 0, 0))});
    std::array<State, Ray8::lanes> laneStates;
    PrimitivePacketState8 states{&laneStates[0], &laneStates[1], &laneStates[2], &laneStates[3],
                                 &laneStates[4], &laneStates[5], &laneStates[6], &laneStates[7]};

    const auto result = operation.intersectPacketHits(rays, states);

    ASSERT_TRUE(result.hit(0));
    EXPECT_EQ(&operation, result.primitive(0));
    EXPECT_FALSE(result.hit(1));
    EXPECT_FALSE(result.hit(2));
    EXPECT_FALSE(result.hit(3));
    ASSERT_TRUE(result.hit(4));
    EXPECT_EQ(&operation, result.primitive(4));
    ASSERT_TRUE(result.hit(5));
    EXPECT_EQ(&operation, result.primitive(5));
    EXPECT_FALSE(result.hit(6));
    ASSERT_TRUE(result.hit(7));
    EXPECT_EQ(&operation, result.primitive(7));
    for (const State& state : laneStates) {
      EXPECT_EQ(1u, state.packetHitScalarFallbacks);
    }
  }

  TEST(ConvexOperation, ShouldNotReturnAnyPrimitiveRayOutsideBoundingBox) {
    MockConvexOperation i;
    auto primitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto primitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    i.add(primitive1);
    i.add(primitive2);
    EXPECT_CALL(*primitive1, calculateBoundingBox()).WillOnce(Return(BoundingBoxd::undefined));

    Rayd ray(Vector3d(0, 0, 0), Vector3d(1, 0, 0));

    State state;
    HitPointInterval hitPoints;
    auto result = i.intersect(ray, hitPoints, state);

    ASSERT_EQ(nullptr, result);
  }

  TEST(ConvexOperation, ShouldIntersectIfRayHits) {
    MockConvexOperation i;
    auto primitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto primitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    i.add(primitive1);
    i.add(primitive2);
    EXPECT_CALL(*primitive1, calculateBoundingBox())
      .WillOnce(Return(BoundingBoxd(-Vector3d::one, Vector3d::one)));
    EXPECT_CALL(*primitive2, calculateBoundingBox())
      .WillOnce(Return(BoundingBoxd(Vector3d::one, Vector3d::one)));

    Rayd ray(Vector3d(-5, 0, 0), Vector3d(1, 0, 0));

    State state;
    ASSERT_TRUE(i.intersects(ray, state));
  }

  TEST(ConvexOperation, ShouldNotIntersectIfRayOutsideBoundingBox) {
    MockConvexOperation i;
    auto primitive1 = std::make_shared<NiceMock<MockPrimitive>>();
    auto primitive2 = std::make_shared<NiceMock<MockPrimitive>>();
    i.add(primitive1);
    i.add(primitive2);
    EXPECT_CALL(*primitive1, calculateBoundingBox()).WillOnce(Return(BoundingBoxd::undefined));

    Rayd ray(Vector3d(0, 0, 0), Vector3d(1, 0, 0));

    State state;
    ASSERT_FALSE(i.intersects(ray, state));
  }
}
