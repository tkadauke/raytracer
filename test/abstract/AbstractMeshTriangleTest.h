#ifndef ABSTRACT_MESH_TRIANGLE_TEST_H
#define ABSTRACT_MESH_TRIANGLE_TEST_H

#include "core/geometry/Mesh.h"
#include "core/math/Ray.h"
#include "core/math/RayPacket.h"
#include "core/math/HitPointInterval.h"
#include "render/State.h"
#include "render/primitives/Primitive.h"
#include "test/helpers/VectorTestHelper.h"

namespace testing {
  using namespace render;

  template<class MT>
  struct AbstractMeshTriangleTest : public ::testing::Test {
    inline void SetUp() {
      mesh.addVertex(Vector3d(-1, -1, 0), Vector3d(0, 0, 1), Vector2d(0, 0));
      mesh.addVertex(Vector3d(-1, 1, 0), Vector3d(0, 0, 1), Vector2d(0, 1));
      mesh.addVertex(Vector3d(1, -1, 0), Vector3d(0, 0, 1), Vector2d(1, 0));
    }

    Mesh mesh;
  };

  TYPED_TEST_SUITE_P(AbstractMeshTriangleTest);

  TYPED_TEST_P(AbstractMeshTriangleTest, ShouldInitializeWithValues) {
    TypeParam triangle(&this->mesh, 0, 1, 2);
  }

  TYPED_TEST_P(AbstractMeshTriangleTest, ShouldIntersectWithRay) {
    TypeParam triangle(&this->mesh, 0, 1, 2);
    Rayd ray(Vector3d(0, 0, -1), Vector3d(0, 0, 1));

    State state;
    HitPointInterval hitPoints;
    auto primitive = triangle.intersect(ray, hitPoints, state);
    ASSERT_EQ(primitive, &triangle);
    ASSERT_EQ(Vector3d(0, 0, 0), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 0, 1), hitPoints.min().normal());
    ASSERT_EQ(1, hitPoints.min().distance());
    ASSERT_EQ(1, state.intersectionHits);
    ASSERT_EQ(0, state.intersectionMisses);
  }

  TYPED_TEST_P(AbstractMeshTriangleTest, ShouldNotIntersectWithMissingRay) {
    TypeParam triangle(&this->mesh, 0, 1, 2);
    Rayd ray(Vector3d(0, 4, -1), Vector3d(0, 0, 1));

    State state;
    HitPointInterval hitPoints;
    auto primitive = triangle.intersect(ray, hitPoints, state);

    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }

  TYPED_TEST_P(AbstractMeshTriangleTest, ShouldNotIntersectIfPointIsBehindRayOrigin) {
    TypeParam triangle(&this->mesh, 0, 1, 2);
    Rayd ray(Vector3d(0, 0, -1), Vector3d(0, 0, -1));

    State state;
    HitPointInterval hitPoints;
    auto primitive = triangle.intersect(ray, hitPoints, state);

    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
    ASSERT_EQ(0, state.intersectionHits);
    ASSERT_EQ(1, state.intersectionMisses);
  }

  TYPED_TEST_P(AbstractMeshTriangleTest, ShouldIntersectRay4PacketLikeScalarRays) {
    TypeParam triangle(&this->mesh, 0, 1, 2);
    const std::array<Rayf, 4> rayArray{Rayf(Vector3f(0, 0, -1), Vector3f(0, 0, 1)),
                                       Rayf(Vector3f(0, 4, -1), Vector3f(0, 0, 1)),
                                       Rayf(Vector3f(0, 0, -1), Vector3f(0, 0, -1)),
                                       Rayf(Vector3f(-0.5f, -0.5f, -1), Vector3f(0, 0, 1))};

    State packetState;
    const auto result = triangle.intersectPacket(Ray4(rayArray), packetState);

    for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
      State scalarState;
      HitPointInterval hitPoints;
      const auto primitive = triangle.intersect(Rayd(rayArray[lane]), hitPoints, scalarState);
      ASSERT_EQ(primitive != nullptr, result.hit(lane)) << "lane " << lane;
      if (primitive != nullptr) {
        ASSERT_NEAR(hitPoints.min().distance(), result.tNear[lane], 1e-5) << "lane " << lane;
      }
    }
    ASSERT_EQ(2, packetState.intersectionHits);
    ASSERT_EQ(2, packetState.intersectionMisses);
  }

  TYPED_TEST_P(AbstractMeshTriangleTest, ShouldMaterializeRay4PacketHitsLikeScalarRays) {
    TypeParam triangle(&this->mesh, 0, 1, 2);
    const std::array<Rayf, 4> rayArray{Rayf(Vector3f(0, 0, -1), Vector3f(0, 0, 1)),
                                       Rayf(Vector3f(0, 4, -1), Vector3f(0, 0, 1)),
                                       Rayf(Vector3f(0, 0, -1), Vector3f(0, 0, -1)),
                                       Rayf(Vector3f(-0.5f, -0.5f, -1), Vector3f(0, 0, 1))};

    std::array<State, Ray4::lanes> laneStates;
    PrimitivePacketState4 states{&laneStates[0], &laneStates[1], &laneStates[2], &laneStates[3]};
    const auto result = triangle.intersectPacketHits(Ray4(rayArray), states);

    for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
      State scalarState;
      HitPointInterval hitPoints;
      const auto primitive = triangle.intersect(Rayd(rayArray[lane]), hitPoints, scalarState);
      ASSERT_EQ(primitive != nullptr, result.hit(lane)) << "lane " << lane;
      if (primitive == nullptr) {
        continue;
      }

      const HitPoint& expected = hitPoints.min();
      const HitPoint& actual = result.hitPoint(lane);
      ASSERT_EQ(&triangle, result.primitive(lane)) << "lane " << lane;
      ASSERT_NEAR(expected.distance(), actual.distance(), 1e-5) << "lane " << lane;
      ASSERT_VECTOR_NEAR(expected.point(), actual.point(), 1e-5);
      ASSERT_VECTOR_NEAR(expected.normal(), actual.normal(), 1e-5);
      ASSERT_VECTOR_NEAR(expected.uv(), actual.uv(), 1e-5);
      ASSERT_EQ(1, laneStates[lane].intersectionHits) << "lane " << lane;
      ASSERT_EQ(0, laneStates[lane].intersectionMisses) << "lane " << lane;
    }
    ASSERT_EQ(1, laneStates[1].intersectionMisses);
    ASSERT_EQ(1, laneStates[2].intersectionMisses);
  }

  TYPED_TEST_P(AbstractMeshTriangleTest, ShouldReturnTrueForIntersectsIfThereIsAIntersection) {
    TypeParam triangle(&this->mesh, 0, 1, 2);
    Rayd ray(Vector3d(0, 0, -1), Vector3d(0, 0, 1));

    State state;
    ASSERT_TRUE(triangle.intersects(ray, state));
  }

  TYPED_TEST_P(AbstractMeshTriangleTest, ShouldReturnFalseForIntersectsIfThereIsNoIntersection) {
    TypeParam triangle(&this->mesh, 0, 1, 2);
    Rayd ray(Vector3d(0, 0, -1), Vector3d(0, 0, -1));

    State state;
    ASSERT_FALSE(triangle.intersects(ray, state));
  }

  REGISTER_TYPED_TEST_SUITE_P(AbstractMeshTriangleTest, ShouldInitializeWithValues,
                              ShouldIntersectWithRay, ShouldNotIntersectWithMissingRay,
                              ShouldNotIntersectIfPointIsBehindRayOrigin,
                              ShouldIntersectRay4PacketLikeScalarRays,
                              ShouldMaterializeRay4PacketHitsLikeScalarRays,
                              ShouldReturnTrueForIntersectsIfThereIsAIntersection,
                              ShouldReturnFalseForIntersectsIfThereIsNoIntersection);
}

#endif
