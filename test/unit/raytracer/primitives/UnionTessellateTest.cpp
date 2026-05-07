#include "gtest/gtest.h"
#include "raytracer/primitives/Union.h"
#include "core/geometry/Mesh.h"

namespace UnionTessellateTest {
  using namespace raytracer;
using namespace render;

  TEST(UnionTessellate, ShouldReturnEmptyMesh) {
    // CSG mesh booleans are not implemented; queued under roadmap §4.2.a.
    Union u;
    auto mesh = u.tessellate(0);
    EXPECT_TRUE(mesh->vertices().empty());
    EXPECT_TRUE(mesh->faces().empty());
  }
}
