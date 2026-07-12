#include "gtest/gtest.h"
#include "render/primitives/Plane.h"
#include "core/geometry/Mesh.h"

namespace PlaneTessellateTest {
  using namespace render;

  TEST(PlaneTessellate, ShouldReturnEmptyMesh) {
    // Plane is infinite and has no finite tessellation — empty Mesh by design.
    Plane plane(Vector3d(0, 1, 0), 0);
    auto mesh = plane.tessellate(0);
    EXPECT_TRUE(mesh->vertices().empty());
    EXPECT_TRUE(mesh->faces().empty());
  }
}
