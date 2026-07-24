#include <gtest/gtest.h>
#include "render/State.h"
#include "render/primitives/MeshTriangle.h"
#include "core/geometry/Mesh.h"
#include "core/math/RayPacket.h"
#include "test/helpers/PrimitiveTestHelper.h"
#include "test/helpers/VectorTestHelper.h"

namespace MeshTriangleTest {
  using namespace ::testing;
  using namespace render;
  using test::helpers::PacketStates8;

  class ConcreteMeshTriangle : public MeshTriangle {
  public:
    inline ConcreteMeshTriangle(Mesh* mesh, int index0, int index1, int index2)
        : MeshTriangle(mesh, index0, index1, index2) {
    }

    const Primitive* intersect(const Rayd&, HitPointInterval&, State&) const override {
      return nullptr;
    }

  protected:
    Vector3d normalAtBarycentric(double, double) const override {
      return Vector3d::null;
    }
  };

  TEST(MeshTriangle, ShouldComputeBoundingBox) {
    Mesh mesh;

    mesh.addVertex(Vector3d(0, 0, 0), Vector3d::null);
    mesh.addVertex(Vector3d(0, 1, 0), Vector3d::null);
    mesh.addVertex(Vector3d(1, 0, 0), Vector3d::null);
    ConcreteMeshTriangle triangle(&mesh, 0, 1, 2);

    BoundingBoxd expected = BoundingBoxd(Vector3d(0, 0, 0), Vector3d(1, 1, 0)).grownByEpsilon();
    ASSERT_EQ(expected, triangle.boundingBox());
  }

  TEST(MeshTriangle, ShouldMaterializeRay8PacketHits) {
    Mesh mesh;
    mesh.addVertex(Vector3d(0, 0, 0), Vector3d::null, Vector2d(0, 0));
    mesh.addVertex(Vector3d(0, 1, 0), Vector3d::null, Vector2d(0, 1));
    mesh.addVertex(Vector3d(1, 0, 0), Vector3d::null, Vector2d(1, 0));
    ConcreteMeshTriangle triangle(&mesh, 0, 1, 2);
    const Ray8 rays(
      std::array<Rayd, Ray8::lanes>{Rayd(Vector3d(0.25, 0.25, -1), Vector3d(0, 0, 1)),
                                    Rayd(Vector3d(2, 0, -1), Vector3d(0, 0, 1)),
                                    Rayd(Vector3d(0.25, 0.25, -1), Vector3d(0, 0, -1)),
                                    Rayd(Vector3d(0.5, 0.25, -1), Vector3d(0, 0, 1)),
                                    Rayd(Vector3d(0.25, 0.5, -1), Vector3d(0, 0, 1)),
                                    Rayd(Vector3d(0.75, 0.75, -1), Vector3d(0, 0, 1)),
                                    Rayd(Vector3d(-0.25, 0.25, -1), Vector3d(0, 0, 1)),
                                    Rayd(Vector3d(0.1, 0.1, -2), Vector3d(0, 0, 2))});
    PacketStates8 ps;

    const auto result = triangle.intersectPacketHits(rays, ps.states);

    ASSERT_TRUE(result.hit(0));
    EXPECT_EQ(&triangle, result.primitive(0));
    EXPECT_EQ(Vector3d(0.25, 0.25, 0), result.hitPoint(0).point());
    EXPECT_EQ(1, result.hitPoint(0).distance());
    EXPECT_FALSE(result.hit(1));
    EXPECT_FALSE(result.hit(2));
    ASSERT_TRUE(result.hit(3));
    EXPECT_EQ(Vector3d(0.5, 0.25, 0), result.hitPoint(3).point());
    ASSERT_TRUE(result.hit(4));
    EXPECT_EQ(Vector3d(0.25, 0.5, 0), result.hitPoint(4).point());
    EXPECT_FALSE(result.hit(5));
    EXPECT_FALSE(result.hit(6));
    ASSERT_TRUE(result.hit(7));
    ASSERT_VECTOR_NEAR(Vector4d(0.1, 0.1, 0, 1), result.hitPoint(7).point(), 1e-6);
    EXPECT_EQ(1, ps.lanes[0].intersectionHits);
    EXPECT_EQ(1, ps.lanes[1].intersectionMisses);
    EXPECT_EQ(1, ps.lanes[2].intersectionMisses);
    EXPECT_EQ(1, ps.lanes[3].intersectionHits);
    EXPECT_EQ(1, ps.lanes[4].intersectionHits);
    EXPECT_EQ(1, ps.lanes[5].intersectionMisses);
    EXPECT_EQ(1, ps.lanes[6].intersectionMisses);
    EXPECT_EQ(1, ps.lanes[7].intersectionHits);
    for (const State& state : ps.lanes) {
      EXPECT_EQ(0u, state.packetHitScalarFallbacks);
    }
  }
}
