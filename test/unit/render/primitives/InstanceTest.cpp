#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "render/State.h"
#include "render/primitives/Instance.h"
#include "test/mocks/raytracer/MockPrimitive.h"

namespace InstanceTest {
  using namespace ::testing;
  using namespace render;
using namespace render;
using namespace render;
  
  TEST(Instance, ShouldReturnChildPrimitiveIfTransformedRayIntersects) {
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    Instance instance(primitive);
    EXPECT_CALL(*primitive, intersect(_, _, _)).WillOnce(
      DoAll(
        AddHitPoint(HitPoint(primitive.get(), 1.0, Vector3d(), Vector3d(1, 0, 0))),
        Return(primitive.get())
      )
    );
    
    Rayd ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    State state;
    HitPointInterval hitPoints;
    auto result = instance.intersect(ray, hitPoints, state);
    
    ASSERT_EQ(primitive.get(), result);
  }
  
  TEST(Instance, ShouldNotReturnAnyPrimitiveIfThereIsNoIntersection) {
    auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
    Instance instance(primitive);
    EXPECT_CALL(*primitive, intersect(_, _, _)).WillOnce(Return(static_cast<render::Primitive*>(nullptr)));
    
    Rayd ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    State state;
    HitPointInterval hitPoints;
    auto result = instance.intersect(ray, hitPoints, state);
    
    ASSERT_EQ(nullptr, result);
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
    EXPECT_CALL(*primitive, calculateBoundingBox()).WillOnce(Return(BoundingBoxd(Vector3d(-1, -1, -1), Vector3d(1, 1, 1))));

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
    EXPECT_CALL(*primitive, calculateBoundingBox()).WillOnce(
      Return(BoundingBoxd(Vector3d(-1, -1, -1), Vector3d(1, 1, 1))));

    BoundingBoxd bbox = instance.boundingBox();
    ASSERT_EQ(Vector3d(-1, -1, -1), bbox.min());
    ASSERT_EQ(Vector3d( 6,  1,  1), bbox.max());
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
      .WillOnce(DoAll(SaveArg<0>(&capturedRay),
                      Return(static_cast<render::Primitive*>(nullptr))));

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
      .WillOnce(DoAll(SaveArg<0>(&capturedRay),
                      Return(static_cast<render::Primitive*>(nullptr))));

    Rayd worldRay(Vector4d(5, 0, 0, 1), Vector3d(0, 0, 1));
    State state;
    state.timeSample = 0.5;  // ignored on static path
    HitPointInterval hitPoints;
    instance.intersect(worldRay, hitPoints, state);

    // Ray should pass through identity-transformed: still at (5,0,0).
    ASSERT_NEAR(5.0, capturedRay.origin().x(), 1e-9);
  }
}
