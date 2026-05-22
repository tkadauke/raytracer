#include <gtest/gtest.h>
#include "render/State.h"
#include "render/primitives/Plane.h"
#include "core/math/Ray.h"
#include "core/math/RayPacket.h"
#include "core/math/HitPointInterval.h"

namespace PlaneTest {
  using namespace render;

  Rayd toRayd(const Rayf& ray) {
    return Rayd(
      Vector3d(ray.origin().x(), ray.origin().y(), ray.origin().z()),
      Vector3d(ray.direction().x(), ray.direction().y(), ray.direction().z())
    );
  }

  TEST(Plane, ShouldInitializeWithValues) {
    Plane plane(Vector3d(0, 1, 0), 0);
  }
  
  TEST(Plane, ShouldIntersectWithRay) {
    Plane plane(Vector3d(0, 1, 0), 0);
    Rayd ray(Vector3d(0, 1, 0), Vector3d(0, -1, 0));
    
    State state;
    HitPointInterval hitPoints;
    auto primitive = plane.intersect(ray, hitPoints, state);
    ASSERT_EQ(primitive, &plane);
    ASSERT_EQ(Vector3d(0, 0, 0), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 1, 0), hitPoints.min().normal());
    ASSERT_EQ(1, hitPoints.min().distance());
    ASSERT_EQ(1, state.intersectionHits);
    ASSERT_EQ(0, state.intersectionMisses);
  }
  
  TEST(Plane, ShouldNotIntersectWithParallelRay) {
    Plane plane(Vector3d(0, 1, 0), 0);
    Rayd ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    State state;
    HitPointInterval hitPoints;
    auto primitive = plane.intersect(ray, hitPoints, state);
    
    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }
  
  TEST(Plane, ShouldNotIntersectIfPointIsBehindRayOrigin) {
    Plane plane(Vector3d(0, 1, 0), 0);
    Rayd ray(Vector3d(0, -1, 0), Vector3d(0, -1, 0));
    
    State state;
    HitPointInterval hitPoints;
    auto primitive = plane.intersect(ray, hitPoints, state);
    
    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }

  TEST(Plane, ShouldIntersectRay4PacketLikeScalarRays) {
    Plane plane(Vector3d(0, 1, 0), 0);
    const std::array<Rayf, 4> rayArray{
      Rayf(Vector3f(0, 1, 0), Vector3f(0, -1, 0)),
      Rayf(Vector3f(0, 1, 0), Vector3f(1, 0, 0)),
      Rayf(Vector3f(0, -1, 0), Vector3f(0, -1, 0)),
      Rayf(Vector3f(0, 2, 0), Vector3f(0, -2, 0))
    };

    State packetState;
    const auto result = plane.intersectPacket(Ray4(rayArray), packetState);

    for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
      State scalarState;
      HitPointInterval hitPoints;
      const auto primitive = plane.intersect(toRayd(rayArray[lane]), hitPoints, scalarState);
      ASSERT_EQ(primitive != nullptr, result.hit(lane)) << "lane " << lane;
      if (primitive != nullptr) {
        ASSERT_NEAR(hitPoints.min().distance(), result.tNear[lane], 1e-5) << "lane " << lane;
      }
    }
    ASSERT_EQ(2, packetState.intersectionHits);
    ASSERT_EQ(2, packetState.intersectionMisses);
  }
  
  TEST(Plane, ShouldReturnTrueForIntersectsIfThereIsAIntersection) {
    Plane plane(Vector3d(0, 1, 0), 0);
    Rayd ray(Vector3d(0, 1, 0), Vector3d(0, -1, 0));
    
    State state;
    ASSERT_TRUE(plane.intersects(ray, state));
  }
  
  TEST(Plane, ShouldReturnFalseForIntersectsIfThereIsNoIntersection) {
    Plane plane(Vector3d(0, 1, 0), 0);
    Rayd ray(Vector3d(0, -1, 0), Vector3d(0, -1, 0));
    
    State state;
    ASSERT_FALSE(plane.intersects(ray, state));
  }
  
  TEST(Plane, ShouldReturnBoundingBox) {
    // TODO
  }
}
