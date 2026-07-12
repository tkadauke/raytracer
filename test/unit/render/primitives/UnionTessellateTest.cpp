#include "gtest/gtest.h"
#include "render/primitives/Union.h"
#include "core/geometry/Mesh.h"

namespace UnionTessellateTest {
  using namespace render;

  TEST(UnionTessellate, ShouldReturnEmptyMesh) {
    // CSG mesh booleans are not implemented; not implemented.
    Union u;
    auto mesh = u.tessellate(0);
    EXPECT_TRUE(mesh->vertices().empty());
    EXPECT_TRUE(mesh->faces().empty());
  }
}
