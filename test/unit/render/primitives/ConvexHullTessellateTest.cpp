#include "gtest/gtest.h"
#include "render/primitives/ConvexHull.h"
#include "core/geometry/Mesh.h"

namespace ConvexHullTessellateTest {
  using namespace render;
  using namespace render;
  using namespace render;

  TEST(ConvexHullTessellate, ShouldReturnEmptyMesh) {
    // CSG mesh booleans are not implemented; not implemented.
    ConvexHull ch;
    auto mesh = ch.tessellate(0);
    EXPECT_TRUE(mesh->vertices().empty());
    EXPECT_TRUE(mesh->faces().empty());
  }
}
