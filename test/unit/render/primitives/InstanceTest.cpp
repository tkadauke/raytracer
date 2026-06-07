#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "core/math/interpolation/Interpolation.h"
#include "render/State.h"
#include "render/animation/AnimationTrack.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/Instance.h"
#include "render/primitives/Sphere.h"
#include "test/mocks/raytracer/MockPrimitive.h"

namespace InstanceTest {
  using namespace ::testing;
  using namespace render;
  using namespace render;
  using namespace render;
  using core::math::interpolation::InterpolationMode;

  void expectVectorNear(const Vector3d& expected, const Vector3d& actual, double epsilon) {
    EXPECT_NEAR(expected.x(), actual.x(), epsilon);
    EXPECT_NEAR(expected.y(), actual.y(), epsilon);
    EXPECT_NEAR(expected.z(), actual.z(), epsilon);
  }

  TEST(Instance, ShouldReturnChildPrimitiveIfTransformedRayIntersects) {
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    Instance instance(primitive);
    EXPECT_CALL(*primitive, intersect(_, _, _))
      .WillOnce(DoAll(AddHitPoint(HitPoint(primitive.get(), 1.0, Vector3d(), Vector3d(1, 0, 0))),
                      Return(primitive.get())));

    Rayd ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));

    State state;
    HitPointInterval hitPoints;
    auto result = instance.intersect(ray, hitPoints, state);

    ASSERT_EQ(primitive.get(), result);
  }

  TEST(Instance, ShouldNotReturnAnyPrimitiveIfThereIsNoIntersection) {
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    Instance instance(primitive);
    EXPECT_CALL(*primitive, intersect(_, _, _))
      .WillOnce(Return(static_cast<render::Primitive*>(nullptr)));

    Rayd ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));

    State state;
    HitPointInterval hitPoints;
    auto result = instance.intersect(ray, hitPoints, state);

    ASSERT_EQ(nullptr, result);
  }

  TEST(Instance, ShouldTransformIntervalsWhenChildIntersectionIsBehindRay) {
    auto primitive = std::make_shared<Sphere>(Vector3d(), 1);
    Instance instance(primitive);
    instance.setMatrix(Matrix4d::translate(10.0, 0.0, 0.0));

    State state;
    HitPointInterval hitPoints;
    const auto result =
      instance.intersect(Rayd(Vector3d(10, 0, 2), Vector3d(0, 0, 1)), hitPoints, state);

    ASSERT_EQ(nullptr, result);
    ASSERT_FALSE(hitPoints.empty());
    EXPECT_EQ(Vector3d(10, 0, -1), hitPoints.min().point());
    EXPECT_EQ(Vector3d(10, 0, 1), hitPoints.max().point());
    EXPECT_EQ(-3, hitPoints.min().distance());
    EXPECT_EQ(-1, hitPoints.max().distance());
  }

  TEST(Instance, ShouldReturnTrueForIntersectsIfThereIsAIntersection) {
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    Instance instance(primitive);
    EXPECT_CALL(*primitive, intersects(_, _)).WillOnce(Return(true));

    Rayd ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));

    State state;
    ASSERT_TRUE(instance.intersects(ray, state));
  }

  TEST(Instance, ShouldReturnFalseForIntersectsIfThereIsNoIntersection) {
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    Instance instance(primitive);
    EXPECT_CALL(*primitive, intersects(_, _)).WillOnce(Return(false));

    Rayd ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));

    State state;
    ASSERT_FALSE(instance.intersects(ray, state));
  }

  TEST(Instance, ShouldReturnFarthestPoint) {
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    Instance instance(primitive);
    instance.setMatrix(Matrix3d::scale(2));
    EXPECT_CALL(*primitive, farthestPoint(_)).WillOnce(Return(Vector3d(1, 1, 1)));

    Vector3d expected(2, 2, 2);
    ASSERT_EQ(expected, instance.farthestPoint(Vector3d::one));
  }

  TEST(Instance, ShouldReturnBoundingBox) {
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    Instance instance(primitive);
    instance.setMatrix(Matrix3d::scale(2));
    EXPECT_CALL(*primitive, calculateBoundingBox())
      .WillOnce(Return(BoundingBoxd(Vector3d(-1, -1, -1), Vector3d(1, 1, 1))));

    BoundingBoxd expected(Vector3d(-2, -2, -2), Vector3d(2, 2, 2));
    ASSERT_EQ(expected, instance.boundingBox());
  }

  // ---- motion blur ---------------------------------------------------------

  TEST(Instance, ShouldDefaultToZeroVelocity) {
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    Instance instance(primitive);
    ASSERT_EQ(Vector3d::null, instance.velocity());
  }

  TEST(Instance, ShouldSetAndGetVelocity) {
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    Instance instance(primitive);
    instance.setVelocity(Vector3d(1, 2, 3));
    ASSERT_EQ(Vector3d(1, 2, 3), instance.velocity());
  }

  TEST(Instance, ShouldExpandBoundingBoxByMotionVector) {
    // Static bbox of [-1,1]^3; velocity (5, 0, 0) extends it to
    // include the +5 X position the primitive reaches at timeSample=1.
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    Instance instance(primitive);
    instance.setMatrix(Matrix4d());
    instance.setVelocity(Vector3d(5, 0, 0));
    EXPECT_CALL(*primitive, calculateBoundingBox())
      .WillOnce(Return(BoundingBoxd(Vector3d(-1, -1, -1), Vector3d(1, 1, 1))));

    BoundingBoxd bbox = instance.boundingBox();
    ASSERT_EQ(Vector3d(-1, -1, -1), bbox.min());
    ASSERT_EQ(Vector3d(6, 1, 1), bbox.max());
  }

  TEST(Instance, ShouldShiftRayByVelocityTimesTimeSampleAtIntersect) {
    // The motion-blur math shifts the ray's origin by -velocity *
    // timeSample before transforming into local space. With
    // velocity = (10, 0, 0) and timeSample = 0.5, a ray fired at
    // world origin (5, 0, 0) should reach the primitive's intersect
    // call at local origin (0, 0, 0) — the primitive is at +5 in
    // its motion-blurred world position, the ray sees it as
    // sitting at the local origin.
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    Instance instance(primitive);
    instance.setMatrix(Matrix4d());
    instance.setVelocity(Vector3d(10, 0, 0));

    Rayd capturedRay(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1));
    EXPECT_CALL(*primitive, intersect(_, _, _))
      .WillOnce(DoAll(SaveArg<0>(&capturedRay), Return(static_cast<render::Primitive*>(nullptr))));

    Rayd worldRay(Vector4d(5, 0, 0, 1), Vector3d(0, 0, 1));
    State state;
    state.timeSample = 0.5;
    HitPointInterval hitPoints;
    instance.intersect(worldRay, hitPoints, state);

    // Local origin should be (5,0,0) - 10*0.5*(1,0,0) = (0,0,0).
    ASSERT_NEAR(0.0, capturedRay.origin().x(), 1e-9);
    ASSERT_NEAR(0.0, capturedRay.origin().y(), 1e-9);
    ASSERT_NEAR(0.0, capturedRay.origin().z(), 1e-9);
  }

  TEST(Instance, ShouldTakeStaticFastPathWhenVelocityIsZero) {
    // Without velocity, intersect must produce the same ray
    // transformation as before — the timeSample value should have
    // no effect on the captured local ray.
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    Instance instance(primitive);
    instance.setMatrix(Matrix4d());
    // Default velocity = zero.

    Rayd capturedRay(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1));
    EXPECT_CALL(*primitive, intersect(_, _, _))
      .WillOnce(DoAll(SaveArg<0>(&capturedRay), Return(static_cast<render::Primitive*>(nullptr))));

    Rayd worldRay(Vector4d(5, 0, 0, 1), Vector3d(0, 0, 1));
    State state;
    state.timeSample = 0.5; // ignored on static path
    HitPointInterval hitPoints;
    instance.intersect(worldRay, hitPoints, state);

    // Ray should pass through identity-transformed: still at (5,0,0).
    ASSERT_NEAR(5.0, capturedRay.origin().x(), 1e-9);
  }

  TEST(Instance, ShouldSampleExplicitPositionTrackAtFramePlusTimeSample) {
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    Instance instance(primitive);
    instance.setMatrix(Matrix4d());
    instance.setMetadataValue("animation:evaluatedFrame", "10");
    instance.setAnimationTrack(
      "position",
      render::animation::AnimationTrack({{10.0, Vector3d(0, 0, 0)}, {11.0, Vector3d(10, 0, 0)}}));

    Rayd capturedRay(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1));
    EXPECT_CALL(*primitive, intersect(_, _, _))
      .WillOnce(DoAll(SaveArg<0>(&capturedRay), Return(static_cast<render::Primitive*>(nullptr))));

    State state;
    state.timeSample = 0.5;
    HitPointInterval hitPoints;
    instance.intersect(Rayd(Vector3d(5, 0, 0), Vector3d(0, 0, 1)), hitPoints, state);

    expectVectorNear(Vector3d(0, 0, 0), Vector3d(capturedRay.origin()), 1e-9);
  }

  TEST(Instance, ShouldComposeVelocityAfterExplicitPositionTrack) {
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    Instance instance(primitive);
    instance.setMatrix(Matrix4d());
    instance.setVelocity(Vector3d(2, 0, 0));
    instance.setMetadataValue("animation:evaluatedFrame", "10");
    instance.setAnimationTrack(
      "position",
      render::animation::AnimationTrack({{10.0, Vector3d(0, 0, 0)}, {11.0, Vector3d(10, 0, 0)}}));

    Rayd capturedRay(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1));
    EXPECT_CALL(*primitive, intersect(_, _, _))
      .WillOnce(DoAll(SaveArg<0>(&capturedRay), Return(static_cast<render::Primitive*>(nullptr))));

    State state;
    state.timeSample = 0.5;
    HitPointInterval hitPoints;
    instance.intersect(Rayd(Vector3d(6, 0, 0), Vector3d(0, 0, 1)), hitPoints, state);

    expectVectorNear(Vector3d(0, 0, 0), Vector3d(capturedRay.origin()), 1e-9);
  }

  TEST(Instance, ShouldLetVelocityTrackOverrideStaticVelocity) {
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    Instance instance(primitive);
    instance.setMatrix(Matrix4d());
    instance.setVelocity(Vector3d(100, 0, 0));
    instance.setMetadataValue("animation:evaluatedFrame", "10");
    instance.setAnimationTrack("velocity", render::animation::AnimationTrack(
                                             {{10.0, Vector3d(2, 0, 0)}, {11.0, Vector3d(2, 0, 0)}},
                                             InterpolationMode::Step));

    Rayd capturedRay(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1));
    EXPECT_CALL(*primitive, intersect(_, _, _))
      .WillOnce(DoAll(SaveArg<0>(&capturedRay), Return(static_cast<render::Primitive*>(nullptr))));

    State state;
    state.timeSample = 0.5;
    HitPointInterval hitPoints;
    instance.intersect(Rayd(Vector3d(1, 0, 0), Vector3d(0, 0, 1)), hitPoints, state);

    expectVectorNear(Vector3d(0, 0, 0), Vector3d(capturedRay.origin()), 1e-9);
  }

  TEST(Instance, ShouldMaterializeRay4PacketHitsThroughStaticTransform) {
    auto primitive = std::make_shared<Sphere>(Vector3d(), 1);
    Instance instance(primitive);
    instance.setMatrix(Matrix4d::translate(10.0, 0.0, 0.0));
    const Ray4 rays(std::array<Rayd, Ray4::lanes>{
      Rayd(Vector3d(10, 0, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(10, 0, -2), Vector3d(0, 1, 0)),
      Rayd(Vector3d(10, 0, 0), Vector3d(0, 0, 1)), Rayd(Vector3d(12, 0, -2), Vector3d(0, 0, 1))});
    std::array<State, Ray4::lanes> laneStates;
    PrimitivePacketState4 states{&laneStates[0], &laneStates[1], &laneStates[2], &laneStates[3]};

    const auto result = instance.intersectPacketHits(rays, states);

    ASSERT_TRUE(result.hit(0));
    EXPECT_EQ(primitive.get(), result.primitive(0));
    EXPECT_EQ(Vector3d(10, 0, -1), result.hitPoint(0).point());
    EXPECT_EQ(Vector3d(0, 0, -1), result.hitPoint(0).normal());
    EXPECT_EQ(1, result.hitPoint(0).distance());
    EXPECT_FALSE(result.hit(1));
    ASSERT_TRUE(result.hit(2));
    EXPECT_EQ(primitive.get(), result.primitive(2));
    EXPECT_EQ(Vector3d(10, 0, 1), result.hitPoint(2).point());
    EXPECT_EQ(Vector3d(0, 0, 1), result.hitPoint(2).normal());
    EXPECT_FALSE(result.hit(3));
  }

  TEST(Instance, ShouldMaterializeRay4PacketIntervalsThroughStaticTransform) {
    auto primitive = std::make_shared<Sphere>(Vector3d(), 1);
    Instance instance(primitive);
    instance.setMatrix(Matrix4d::translate(10.0, 0.0, 0.0));
    const Ray4 rays(std::array<Rayd, Ray4::lanes>{
      Rayd(Vector3d(10, 0, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(10, 0, -2), Vector3d(0, 1, 0)),
      Rayd(Vector3d(10, 0, 2), Vector3d(0, 0, 1)), Rayd(Vector3d(10, 0, 0), Vector3d(0, 0, 1))});
    std::array<State, Ray4::lanes> laneStates;
    PrimitivePacketState4 states{&laneStates[0], &laneStates[1], &laneStates[2], &laneStates[3]};

    const auto result = instance.intersectPacketIntervals(rays, states);

    ASSERT_TRUE(result.hit(0));
    ASSERT_TRUE(result.hasInterval(0));
    EXPECT_EQ(primitive.get(), result.primitive(0));
    EXPECT_EQ(Vector3d(10, 0, -1), result.interval(0).min().point());
    EXPECT_EQ(Vector3d(10, 0, 1), result.interval(0).max().point());
    EXPECT_EQ(1, result.interval(0).min().distance());
    EXPECT_EQ(3, result.interval(0).max().distance());
    EXPECT_FALSE(result.scalarFallback(0));

    EXPECT_FALSE(result.hit(1));
    EXPECT_FALSE(result.hasInterval(1));

    EXPECT_FALSE(result.hit(2));
    ASSERT_TRUE(result.hasInterval(2));
    EXPECT_EQ(Vector3d(10, 0, -1), result.interval(2).min().point());
    EXPECT_EQ(Vector3d(10, 0, 1), result.interval(2).max().point());
    EXPECT_FALSE(result.scalarFallback(2));

    ASSERT_TRUE(result.hit(3));
    ASSERT_TRUE(result.hasInterval(3));
    EXPECT_EQ(Vector3d(10, 0, -1), result.interval(3).min().point());
    EXPECT_EQ(Vector3d(10, 0, 1), result.interval(3).max().point());
    EXPECT_FALSE(result.scalarFallback(3));

    for (const auto& state : laneStates) {
      EXPECT_EQ(0u, state.packetHitScalarFallbacks);
    }
  }

  TEST(Instance, ShouldMaterializeRay8PacketHitsThroughStaticTransform) {
    auto primitive = std::make_shared<Sphere>(Vector3d(), 1);
    Instance instance(primitive);
    instance.setMatrix(Matrix4d::translate(10.0, 0.0, 0.0));
    const Ray8 rays(std::array<Rayd, Ray8::lanes>{
      Rayd(Vector3d(10, 0, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(10, 0, -2), Vector3d(0, 1, 0)),
      Rayd(Vector3d(10, 0, 0), Vector3d(0, 0, 1)), Rayd(Vector3d(12, 0, -2), Vector3d(0, 0, 1)),
      Rayd(Vector3d(10, 0, -3), Vector3d(0, 0, 1)), Rayd(Vector3d(10, 0, 2), Vector3d(0, 0, -1)),
      Rayd(Vector3d(10, 0, 0), Vector3d(0, 0, -1)), Rayd(Vector3d(10, 2, -2), Vector3d(0, 0, 1))});
    std::array<State, Ray8::lanes> laneStates;
    PrimitivePacketState8 states{&laneStates[0], &laneStates[1], &laneStates[2], &laneStates[3],
                                 &laneStates[4], &laneStates[5], &laneStates[6], &laneStates[7]};

    const auto result = instance.intersectPacketHits(rays, states);

    ASSERT_TRUE(result.hit(0));
    EXPECT_EQ(primitive.get(), result.primitive(0));
    EXPECT_EQ(Vector3d(10, 0, -1), result.hitPoint(0).point());
    EXPECT_EQ(Vector3d(0, 0, -1), result.hitPoint(0).normal());
    EXPECT_EQ(1, result.hitPoint(0).distance());
    EXPECT_FALSE(result.hit(1));
    ASSERT_TRUE(result.hit(2));
    EXPECT_EQ(primitive.get(), result.primitive(2));
    EXPECT_EQ(Vector3d(10, 0, 1), result.hitPoint(2).point());
    EXPECT_EQ(Vector3d(0, 0, 1), result.hitPoint(2).normal());
    EXPECT_FALSE(result.hit(3));
    ASSERT_TRUE(result.hit(4));
    EXPECT_EQ(Vector3d(10, 0, -1), result.hitPoint(4).point());
    ASSERT_TRUE(result.hit(5));
    EXPECT_EQ(Vector3d(10, 0, 1), result.hitPoint(5).point());
    ASSERT_TRUE(result.hit(6));
    EXPECT_EQ(Vector3d(10, 0, -1), result.hitPoint(6).point());
    EXPECT_FALSE(result.hit(7));
    for (const auto& state : laneStates) {
      EXPECT_EQ(0u, state.packetHitScalarFallbacks);
    }
  }

  TEST(Instance, ShouldMaterializeRay4PacketHitsThroughMovingTransform) {
    auto primitive = std::make_shared<Sphere>(Vector3d(), 1);
    Instance instance(primitive);
    instance.setMatrix(Matrix4d::translate(10.0, 0.0, 0.0));
    instance.setVelocity(Vector3d(2, 0, 0));
    const Ray4 rays(std::array<Rayd, Ray4::lanes>{
      Rayd(Vector3d(11, 0, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(10, 0, -2), Vector3d(0, 0, 1)),
      Rayd(Vector3d(12, 0, 2), Vector3d(0, 0, -1)), Rayd(Vector3d(14, 0, -2), Vector3d(0, 0, 1))});
    std::array<State, Ray4::lanes> laneStates;
    laneStates[0].timeSample = 0.5;
    laneStates[1].timeSample = 0.0;
    laneStates[2].timeSample = 1.0;
    laneStates[3].timeSample = 0.5;
    PrimitivePacketState4 states{&laneStates[0], &laneStates[1], &laneStates[2], &laneStates[3]};

    const auto result = instance.intersectPacketHits(rays, states);

    ASSERT_TRUE(result.hit(0));
    EXPECT_EQ(primitive.get(), result.primitive(0));
    EXPECT_EQ(Vector3d(11, 0, -1), result.hitPoint(0).point());
    EXPECT_FALSE(result.scalarFallback(0));
    ASSERT_TRUE(result.hit(1));
    EXPECT_EQ(Vector3d(10, 0, -1), result.hitPoint(1).point());
    EXPECT_FALSE(result.scalarFallback(1));
    ASSERT_TRUE(result.hit(2));
    EXPECT_EQ(Vector3d(12, 0, 1), result.hitPoint(2).point());
    EXPECT_FALSE(result.scalarFallback(2));
    EXPECT_FALSE(result.hit(3));
    for (const auto& state : laneStates) {
      EXPECT_EQ(0u, state.packetHitScalarFallbacks);
    }
  }

  TEST(Instance, ShouldMaterializeRay4PacketIntervalsThroughMovingTransform) {
    auto primitive = std::make_shared<Sphere>(Vector3d(), 1);
    Instance instance(primitive);
    instance.setMatrix(Matrix4d::translate(10.0, 0.0, 0.0));
    instance.setVelocity(Vector3d(2, 0, 0));
    const Ray4 rays(std::array<Rayd, Ray4::lanes>{
      Rayd(Vector3d(11, 0, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(10, 0, -2), Vector3d(0, 1, 0)),
      Rayd(Vector3d(12, 0, 2), Vector3d(0, 0, 1)), Rayd(Vector3d(11, 0, 0), Vector3d(0, 0, 1))});
    std::array<State, Ray4::lanes> laneStates;
    laneStates[0].timeSample = 0.5;
    laneStates[1].timeSample = 0.0;
    laneStates[2].timeSample = 1.0;
    laneStates[3].timeSample = 0.5;
    PrimitivePacketState4 states{&laneStates[0], &laneStates[1], &laneStates[2], &laneStates[3]};

    const auto result = instance.intersectPacketIntervals(rays, states);

    ASSERT_TRUE(result.hit(0));
    ASSERT_TRUE(result.hasInterval(0));
    EXPECT_EQ(primitive.get(), result.primitive(0));
    EXPECT_EQ(Vector3d(11, 0, -1), result.interval(0).min().point());
    EXPECT_EQ(Vector3d(11, 0, 1), result.interval(0).max().point());
    EXPECT_FALSE(result.scalarFallback(0));

    EXPECT_FALSE(result.hit(1));
    EXPECT_FALSE(result.hasInterval(1));

    EXPECT_FALSE(result.hit(2));
    ASSERT_TRUE(result.hasInterval(2));
    EXPECT_EQ(Vector3d(12, 0, -1), result.interval(2).min().point());
    EXPECT_EQ(Vector3d(12, 0, 1), result.interval(2).max().point());
    EXPECT_FALSE(result.scalarFallback(2));

    ASSERT_TRUE(result.hit(3));
    ASSERT_TRUE(result.hasInterval(3));
    EXPECT_EQ(Vector3d(11, 0, -1), result.interval(3).min().point());
    EXPECT_EQ(Vector3d(11, 0, 1), result.interval(3).max().point());
    EXPECT_FALSE(result.scalarFallback(3));

    for (const auto& state : laneStates) {
      EXPECT_EQ(0u, state.packetHitScalarFallbacks);
    }
  }

  TEST(Instance, ShouldMaterializeRay8PacketIntervalsThroughMovingTransform) {
    auto primitive = std::make_shared<Sphere>(Vector3d(), 1);
    Instance instance(primitive);
    instance.setMatrix(Matrix4d::translate(10.0, 0.0, 0.0));
    instance.setVelocity(Vector3d(2, 0, 0));
    const Ray8 rays(std::array<Rayd, Ray8::lanes>{
      Rayd(Vector3d(11, 0, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(10, 0, -2), Vector3d(0, 1, 0)),
      Rayd(Vector3d(12, 0, 2), Vector3d(0, 0, 1)), Rayd(Vector3d(11, 0, 0), Vector3d(0, 0, 1)),
      Rayd(Vector3d(11, 0, -3), Vector3d(0, 0, 1)), Rayd(Vector3d(12, 0, 3), Vector3d(0, 0, -1)),
      Rayd(Vector3d(10, 0, 0), Vector3d(0, 0, -1)), Rayd(Vector3d(11, 2, -2), Vector3d(0, 0, 1))});
    std::array<State, Ray8::lanes> laneStates;
    laneStates[0].timeSample = 0.5;
    laneStates[1].timeSample = 0.0;
    laneStates[2].timeSample = 1.0;
    laneStates[3].timeSample = 0.5;
    laneStates[4].timeSample = 0.5;
    laneStates[5].timeSample = 1.0;
    laneStates[6].timeSample = 0.0;
    laneStates[7].timeSample = 0.5;
    PrimitivePacketState8 states{&laneStates[0], &laneStates[1], &laneStates[2], &laneStates[3],
                                 &laneStates[4], &laneStates[5], &laneStates[6], &laneStates[7]};

    const auto result = instance.intersectPacketIntervals(rays, states);

    ASSERT_TRUE(result.hit(0));
    EXPECT_EQ(Vector3d(11, 0, -1), result.interval(0).min().point());
    EXPECT_FALSE(result.scalarFallback(0));
    EXPECT_FALSE(result.hit(1));
    ASSERT_TRUE(result.hasInterval(2));
    EXPECT_FALSE(result.hit(2));
    EXPECT_EQ(Vector3d(12, 0, -1), result.interval(2).min().point());
    ASSERT_TRUE(result.hit(3));
    EXPECT_EQ(Vector3d(11, 0, -1), result.interval(3).min().point());
    ASSERT_TRUE(result.hit(4));
    EXPECT_EQ(Vector3d(11, 0, -1), result.interval(4).min().point());
    ASSERT_TRUE(result.hit(5));
    EXPECT_EQ(Vector3d(12, 0, 1), result.interval(5).minWithPositiveDistance().point());
    ASSERT_TRUE(result.hit(6));
    EXPECT_EQ(Vector3d(10, 0, -1), result.interval(6).minWithPositiveDistance().point());
    EXPECT_FALSE(result.hit(7));
    for (const auto& state : laneStates) {
      EXPECT_EQ(0u, state.packetHitScalarFallbacks);
    }
  }

  TEST(Instance, ShouldMaterializeRay8PacketHitsThroughMovingTransform) {
    auto primitive = std::make_shared<Sphere>(Vector3d(), 1);
    Instance instance(primitive);
    instance.setMatrix(Matrix4d::translate(10.0, 0.0, 0.0));
    instance.setVelocity(Vector3d(2, 0, 0));
    const Ray8 rays(std::array<Rayd, Ray8::lanes>{
      Rayd(Vector3d(11, 0, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(10, 0, -2), Vector3d(0, 0, 1)),
      Rayd(Vector3d(12, 0, 2), Vector3d(0, 0, -1)), Rayd(Vector3d(14, 0, -2), Vector3d(0, 0, 1)),
      Rayd(Vector3d(11, 0, -3), Vector3d(0, 0, 1)), Rayd(Vector3d(12, 0, 3), Vector3d(0, 0, -1)),
      Rayd(Vector3d(10, 0, 0), Vector3d(0, 0, -1)), Rayd(Vector3d(11, 2, -2), Vector3d(0, 0, 1))});
    std::array<State, Ray8::lanes> laneStates;
    laneStates[0].timeSample = 0.5;
    laneStates[1].timeSample = 0.0;
    laneStates[2].timeSample = 1.0;
    laneStates[3].timeSample = 0.5;
    laneStates[4].timeSample = 0.5;
    laneStates[5].timeSample = 1.0;
    laneStates[6].timeSample = 0.0;
    laneStates[7].timeSample = 0.5;
    PrimitivePacketState8 states{&laneStates[0], &laneStates[1], &laneStates[2], &laneStates[3],
                                 &laneStates[4], &laneStates[5], &laneStates[6], &laneStates[7]};

    const auto result = instance.intersectPacketHits(rays, states);

    ASSERT_TRUE(result.hit(0));
    EXPECT_EQ(Vector3d(11, 0, -1), result.hitPoint(0).point());
    ASSERT_TRUE(result.hit(1));
    EXPECT_EQ(Vector3d(10, 0, -1), result.hitPoint(1).point());
    ASSERT_TRUE(result.hit(2));
    EXPECT_EQ(Vector3d(12, 0, 1), result.hitPoint(2).point());
    EXPECT_FALSE(result.hit(3));
    ASSERT_TRUE(result.hit(4));
    EXPECT_EQ(Vector3d(11, 0, -1), result.hitPoint(4).point());
    ASSERT_TRUE(result.hit(5));
    EXPECT_EQ(Vector3d(12, 0, 1), result.hitPoint(5).point());
    ASSERT_TRUE(result.hit(6));
    EXPECT_EQ(Vector3d(10, 0, -1), result.hitPoint(6).point());
    EXPECT_FALSE(result.hit(7));
    for (const auto& state : laneStates) {
      EXPECT_EQ(0u, state.packetHitScalarFallbacks);
    }
  }

  TEST(Instance, ShouldUseInstanceMaterialForRay4PacketHitsWhenOverridden) {
    auto primitive = std::make_shared<Sphere>(Vector3d(), 1);
    Instance instance(primitive);
    instance.setMaterial(std::make_shared<MatteMaterial>());
    const Ray4 rays(std::array<Rayd, Ray4::lanes>{
      Rayd(Vector3d(0, 0, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 0, -2), Vector3d(0, 1, 0)),
      Rayd(Vector3d(0, 0, 0), Vector3d(0, 0, 1)), Rayd(Vector3d(2, 0, -2), Vector3d(0, 0, 1))});
    std::array<State, Ray4::lanes> laneStates;
    PrimitivePacketState4 states{&laneStates[0], &laneStates[1], &laneStates[2], &laneStates[3]};

    const auto result = instance.intersectPacketHits(rays, states);

    ASSERT_TRUE(result.hit(0));
    EXPECT_EQ(&instance, result.primitive(0));
    EXPECT_FALSE(result.hit(1));
    ASSERT_TRUE(result.hit(2));
    EXPECT_EQ(&instance, result.primitive(2));
    EXPECT_FALSE(result.hit(3));
  }
}
