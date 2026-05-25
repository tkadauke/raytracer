#include <gtest/gtest.h>

#include "core/geometry/Mesh.h"
#include "core/math/HitPointInterval.h"
#include "render/State.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/BVH.h"
#include "render/primitives/FlatMeshTriangle.h"
#include "render/primitives/Grid.h"
#include "render/primitives/MeshPrimitive.h"
#include "render/primitives/SmoothMeshTriangle.h"

#include <memory>

namespace MeshPrimitiveTest {
  using namespace ::testing;
  using namespace render;

  Mesh makeQuadMesh() {
    Mesh mesh;
    mesh.addVertex(Vector3d(-1, -1, 0), Vector3d(0, 0, 1), Vector2d(0, 0));
    mesh.addVertex(Vector3d(-1, 1, 0), Vector3d(0, 1, 1).normalized(), Vector2d(0, 1));
    mesh.addVertex(Vector3d(1, 1, 0), Vector3d(1, 0, 1).normalized(), Vector2d(1, 1));
    mesh.addVertex(Vector3d(1, -1, 0), Vector3d(1, 1, 1).normalized(), Vector2d(1, 0));
    mesh.addFace({0, 1, 2, 3});
    return mesh;
  }

  TEST(MeshPrimitive, OwnsMeshStorageForBuiltLeaves) {
    auto mesh = std::make_shared<Mesh>(makeQuadMesh());
    MeshPrimitive primitive(mesh, MeshPrimitive::NormalMode::Smooth);
    mesh.reset();

    Rayd ray(Vector3d(0, 0, -1), Vector3d(0, 0, 1));
    State state;
    HitPointInterval hits;

    ASSERT_NE(nullptr, primitive.intersect(ray, hits, state));
    ASSERT_EQ(Vector3d(0, 0, 0), hits.min().point());
  }

  TEST(MeshPrimitive, BuildsFlatTriangleLeaves) {
    MeshPrimitive primitive(makeQuadMesh(), MeshPrimitive::NormalMode::Flat);

    ASSERT_EQ(2u, primitive.leaves().size());
    for (const auto& leaf : primitive.leaves()) {
      ASSERT_NE(nullptr, std::dynamic_pointer_cast<FlatMeshTriangle>(leaf));
    }
  }

  TEST(MeshPrimitive, BuildsSmoothTriangleLeavesByDefault) {
    MeshPrimitive primitive(makeQuadMesh());

    ASSERT_EQ(2u, primitive.leaves().size());
    for (const auto& leaf : primitive.leaves()) {
      ASSERT_NE(nullptr, std::dynamic_pointer_cast<SmoothMeshTriangle>(leaf));
    }
  }

  TEST(MeshPrimitive, ComputesBoundingBoxFromTriangleLeaves) {
    MeshPrimitive primitive(makeQuadMesh());

    const BoundingBoxd expected =
      BoundingBoxd(Vector3d(-1, -1, 0), Vector3d(1, 1, 0)).grownByEpsilon();
    ASSERT_EQ(expected, primitive.boundingBox());
  }

  TEST(MeshPrimitive, TessellatesSourceFacesIntoTriangleMesh) {
    MeshPrimitive primitive(makeQuadMesh(), MeshPrimitive::NormalMode::Smooth);

    auto mesh = primitive.tessellate();

    ASSERT_EQ(6u, mesh->vertices().size());
    ASSERT_EQ(2u, mesh->faces().size());
    EXPECT_EQ((Mesh::Face{0, 1, 2}), mesh->faces()[0]);
    EXPECT_EQ((Mesh::Face{3, 4, 5}), mesh->faces()[1]);
  }

  TEST(MeshPrimitive, FlatTessellationUsesOneNormalPerTriangle) {
    MeshPrimitive primitive(makeQuadMesh(), MeshPrimitive::NormalMode::Flat);

    auto mesh = primitive.tessellate();

    ASSERT_EQ(6u, mesh->vertices().size());
    EXPECT_EQ(mesh->vertices()[0].normal, mesh->vertices()[1].normal);
    EXPECT_EQ(mesh->vertices()[1].normal, mesh->vertices()[2].normal);
    EXPECT_EQ(mesh->vertices()[3].normal, mesh->vertices()[4].normal);
    EXPECT_EQ(mesh->vertices()[4].normal, mesh->vertices()[5].normal);
  }

  TEST(MeshPrimitive, SmoothTessellationPreservesVertexNormals) {
    MeshPrimitive primitive(makeQuadMesh(), MeshPrimitive::NormalMode::Smooth);

    auto mesh = primitive.tessellate();

    ASSERT_EQ(6u, mesh->vertices().size());
    EXPECT_NE(mesh->vertices()[0].normal, mesh->vertices()[1].normal);
    EXPECT_NE(mesh->vertices()[1].normal, mesh->vertices()[2].normal);
  }

  TEST(MeshPrimitive, MaterialAssignedAfterConstructionAppliesToHitsAndLeaves) {
    MeshPrimitive primitive(makeQuadMesh());
    auto material = std::make_shared<MatteMaterial>();
    primitive.setMaterial(material);

    Rayd ray(Vector3d(0, 0, -1), Vector3d(0, 0, 1));
    State state;
    HitPointInterval hits;

    ASSERT_EQ(&primitive, primitive.intersect(ray, hits, state));

    int leafCount = 0;
    primitive.forEachLeaf([&](const Primitive*, std::shared_ptr<Material> leafMaterial) {
      ++leafCount;
      EXPECT_EQ(material, leafMaterial);
    });
    EXPECT_EQ(2, leafCount);
  }

  TEST(MeshPrimitive, IntersectsInsideGridTraversal) {
    auto primitive = std::make_shared<MeshPrimitive>(makeQuadMesh());
    Grid grid;
    grid.add(primitive);
    grid.setup();

    Rayd ray(Vector3d(0, 0, -1), Vector3d(0, 0, 1));
    State state;
    HitPointInterval hits;

    ASSERT_NE(nullptr, grid.intersect(ray, hits, state));
    ASSERT_EQ(Vector3d(0, 0, 0), hits.min().point());
  }

  TEST(MeshPrimitive, IntersectsInsideBVHTraversal) {
    auto primitive = std::make_shared<MeshPrimitive>(makeQuadMesh());
    BVH bvh;
    bvh.add(primitive);
    bvh.setup();

    Rayd ray(Vector3d(0, 0, -1), Vector3d(0, 0, 1));
    State state;
    HitPointInterval hits;

    ASSERT_NE(nullptr, bvh.intersect(ray, hits, state));
    ASSERT_EQ(Vector3d(0, 0, 0), hits.min().point());
  }
}
