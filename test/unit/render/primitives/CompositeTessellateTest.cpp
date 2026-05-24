#include <gtest/gtest.h>
#include <memory>
#include "render/primitives/Composite.h"
#include "render/primitives/Box.h"
#include "render/primitives/Disk.h"
#include "core/geometry/Mesh.h"

namespace CompositeTessellateTest {
  using namespace render;
  using namespace render;
  using namespace render;

  TEST(CompositeTessellate, ShouldReturnEmptyMeshForNoChildren) {
    Composite composite;
    auto mesh = composite.tessellate(0);
    ASSERT_NE(nullptr, mesh);
    ASSERT_EQ(0u, mesh->vertices().size());
    ASSERT_EQ(0u, mesh->faces().size());
  }

  TEST(CompositeTessellate, ShouldTessellateOneChild) {
    Composite composite;
    composite.add(std::make_shared<Box>(Vector3d(), Vector3d(1, 1, 1)));
    auto mesh = composite.tessellate(0);
    // Box produces 24 vertices and 6 quad faces
    ASSERT_EQ(24u, mesh->vertices().size());
    ASSERT_EQ(6u, mesh->faces().size());
  }

  TEST(CompositeTessellate, ShouldCombineVertexCountsFromTwoChildren) {
    Composite composite;
    composite.add(std::make_shared<Box>(Vector3d(), Vector3d(1, 1, 1)));
    composite.add(std::make_shared<Box>(Vector3d(5, 0, 0), Vector3d(1, 1, 1)));
    auto mesh = composite.tessellate(0);
    // Two boxes → 24 + 24 = 48 vertices, 6 + 6 = 12 faces
    ASSERT_EQ(48u, mesh->vertices().size());
    ASSERT_EQ(12u, mesh->faces().size());
  }

  TEST(CompositeTessellate, ShouldRemapFaceIndicesForSecondChild) {
    Composite composite;
    composite.add(std::make_shared<Box>(Vector3d(), Vector3d(1, 1, 1)));
    composite.add(std::make_shared<Box>(Vector3d(5, 0, 0), Vector3d(1, 1, 1)));
    auto mesh = composite.tessellate(0);

    int vertexCount = static_cast<int>(mesh->vertices().size());
    for (const auto& face : mesh->faces()) {
      for (int idx : face) {
        EXPECT_GE(idx, 0) << "face index must be non-negative";
        EXPECT_LT(idx, vertexCount) << "face index must be < vertex count";
      }
    }
  }

  TEST(CompositeTessellate, ShouldCombineHeterogeneousChildren) {
    // Box produces 24 verts, Disk at lod=0 produces 17 verts → total 41
    Composite composite;
    composite.add(std::make_shared<Box>(Vector3d(), Vector3d(1, 1, 1)));
    composite.add(std::make_shared<Disk>(Vector3d(), Vector3d(0, 1, 0), 1.0));
    auto mesh = composite.tessellate(0);
    ASSERT_EQ(24u + 17u, mesh->vertices().size());
  }

  TEST(CompositeTessellate, ShouldPassLodToChildren) {
    Composite composite;
    composite.add(std::make_shared<Disk>(Vector3d(), Vector3d(0, 1, 0), 1.0));
    // lod=1 → 32 segments → 33 vertices
    auto mesh = composite.tessellate(1);
    ASSERT_EQ(33u, mesh->vertices().size());
  }
}
