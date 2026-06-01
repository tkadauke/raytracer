#include "gtest/gtest.h"
#include "render/State.h"
#include "render/primitives/Sphere.h"
#include "core/math/Ray.h"
#include "core/math/RayPacket.h"
#include "core/math/HitPointInterval.h"
#include "test/helpers/VectorTestHelper.h"

namespace SphereTest {
  using namespace render;
  using namespace render;
  using namespace render;

  TEST(Sphere, ShouldInitializeWithValues) {
    Sphere sphere(Vector3d(), 1);
  }

  TEST(Sphere, ShouldIntersectWithRay) {
    Sphere sphere(Vector3d(), 1);
    Rayd ray(Vector3d(0, 0, -2), Vector3d(0, 0, 1));

    State state;
    HitPointInterval hitPoints;
    auto primitive = sphere.intersect(ray, hitPoints, state);
    ASSERT_EQ(primitive, &sphere);
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().normal());
    ASSERT_EQ(2u, hitPoints.points().size());
    ASSERT_EQ(1, hitPoints.min().distance());
    ASSERT_EQ(1, state.intersectionHits);
    ASSERT_EQ(0, state.intersectionMisses);
  }

  TEST(Sphere, ShouldNotIntersectWithMissingRay) {
    Sphere sphere(Vector3d(), 1);
    Rayd ray(Vector3d(0, 0, -2), Vector3d(0, 1, 0));

    State state;
    HitPointInterval hitPoints;
    auto primitive = sphere.intersect(ray, hitPoints, state);

    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }

  TEST(Sphere, ShouldNotIntersectIfSphereIsBehindRayOrigin) {
    Sphere sphere(Vector3d(), 1);
    Rayd ray(Vector3d(0, 0, 2), Vector3d(0, 0, 1));

    State state;
    HitPointInterval hitPoints;
    auto primitive = sphere.intersect(ray, hitPoints, state);

    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.minWithPositiveDistance().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }

  TEST(Sphere, ShouldReportBothHitpointsWhenRayOriginIsInsideSphere) {
    Sphere sphere(Vector3d(), 1);
    Rayd ray(Vector3d(0, 0, 0), Vector3d(0, 0, 1));

    State state;
    HitPointInterval hitPoints;
    auto primitive = sphere.intersect(ray, hitPoints, state);
    ASSERT_EQ(primitive, &sphere);
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 0, -1), hitPoints.min().normal());
    ASSERT_EQ(-1, hitPoints.min().distance());

    ASSERT_EQ(Vector3d(0, 0, 1), hitPoints.max().point());
    ASSERT_EQ(Vector3d(0, 0, 1), hitPoints.max().normal());
    ASSERT_EQ(1, hitPoints.max().distance());
    ASSERT_EQ(1, state.intersectionHits);
    ASSERT_EQ(0, state.intersectionMisses);
  }

  TEST(Sphere, ShouldIntersectRay4Packet) {
    Sphere sphere(Vector3d(), 1);
    const Ray4 rays(std::array<Rayf, 4>{
      Rayf(Vector3f(0, 0, -2), Vector3f(0, 0, 1)), Rayf(Vector3f(0, 0, -2), Vector3f(0, 1, 0)),
      Rayf(Vector3f(0, 0, 2), Vector3f(0, 0, 1)), Rayf(Vector3f(0, 0, 0), Vector3f(0, 0, 1))});

    State state;
    const auto result = sphere.intersectPacket(rays, state);

    ASSERT_TRUE(result.hit(0));
    ASSERT_FALSE(result.hit(1));
    ASSERT_FALSE(result.hit(2));
    ASSERT_TRUE(result.hit(3));
    ASSERT_EQ(0b1001, result.hitMask);
    ASSERT_EQ(1.0f, result.tNear[0]);
    ASSERT_EQ(3.0f, result.tFar[0]);
    ASSERT_EQ(-1.0f, result.tNear[3]);
    ASSERT_EQ(1.0f, result.tFar[3]);
    ASSERT_EQ(2, state.intersectionHits);
    ASSERT_EQ(2, state.intersectionMisses);
  }

  TEST(Sphere, ShouldMaterializeRay4PacketHits) {
    Sphere sphere(Vector3d(), 1);
    const Ray4 rays(std::array<Rayd, 4>{
      Rayd(Vector3d(0, 0, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 0, -2), Vector3d(0, 1, 0)),
      Rayd(Vector3d(0, 0, 2), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 0, 0), Vector3d(0, 0, 1))});
    std::array<State, Ray4::lanes> laneStates;
    PrimitivePacketState4 states{&laneStates[0], &laneStates[1], &laneStates[2], &laneStates[3]};

    const auto result = sphere.intersectPacketHits(rays, states);

    ASSERT_TRUE(result.hit(0));
    EXPECT_EQ(&sphere, result.primitive(0));
    EXPECT_EQ(Vector3d(0, 0, -1), result.hitPoint(0).point());
    EXPECT_EQ(Vector3d(0, 0, -1), result.hitPoint(0).normal());
    EXPECT_EQ(1, result.hitPoint(0).distance());
    EXPECT_FALSE(result.hit(1));
    EXPECT_FALSE(result.hit(2));
    ASSERT_TRUE(result.hit(3));
    EXPECT_EQ(Vector3d(0, 0, 1), result.hitPoint(3).point());
    EXPECT_EQ(Vector3d(0, 0, 1), result.hitPoint(3).normal());
    EXPECT_EQ(1, result.hitPoint(3).distance());
    EXPECT_EQ(1, laneStates[0].intersectionHits);
    EXPECT_EQ(1, laneStates[1].intersectionMisses);
    EXPECT_EQ(1, laneStates[2].intersectionMisses);
    EXPECT_EQ(1, laneStates[3].intersectionHits);
  }

  TEST(Sphere, ShouldReturnTrueForIntersectsIfThereIsAIntersectionWithRay) {
    Sphere sphere(Vector3d(), 1);
    Rayd ray(Vector3d(0, 0, -2), Vector3d(0, 0, 1));

    State state;
    ASSERT_TRUE(sphere.intersects(ray, state));
  }

  TEST(Sphere, ShouldReturnFalseForIntersectsWithMissingRay) {
    Sphere sphere(Vector3d(), 1);
    Rayd ray(Vector3d(0, 0, -2), Vector3d(0, 1, 0));

    State state;
    ASSERT_FALSE(sphere.intersects(ray, state));
  }

  TEST(Sphere, ShouldReturnFalseForIntersectsIfSphereIsBehindRayOrigin) {
    Sphere sphere(Vector3d(), 1);
    Rayd ray(Vector3d(0, 0, 2), Vector3d(0, 0, 1));

    State state;
    ASSERT_FALSE(sphere.intersects(ray, state));
  }

  TEST(Sphere, ShouldReturnTrueForIntersectsIfRayIsInsideSphere) {
    Sphere sphere(Vector3d(), 1);
    Rayd ray(Vector3d(), Vector3d(0, 0, 1));

    State state;
    ASSERT_TRUE(sphere.intersects(ray, state));
  }

  TEST(Sphere, ShouldReturnFarthestPoint) {
    Sphere sphere(Vector3d(), 1);
    auto direction = Vector3d(0.1, 1, 0.1).normalized();
    ASSERT_VECTOR_NEAR(direction, sphere.farthestPoint(direction), 0.001);
  }

  TEST(Sphere, ShouldReturnBoundingBox) {
    Sphere sphere(Vector3d(), 1);
    ASSERT_EQ(BoundingBoxd(Vector3d(-1, -1, -1), Vector3d(1, 1, 1)), sphere.boundingBox());
  }
}
