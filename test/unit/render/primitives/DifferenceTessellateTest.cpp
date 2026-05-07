#include "gtest/gtest.h"
#include "render/primitives/Difference.h"
#include "core/geometry/Mesh.h"

namespace DifferenceTessellateTest {
  using namespace render;
using namespace render;
using namespace render;

  TEST(DifferenceTessellate, ShouldReturnEmptyMesh) {
    // CSG mesh booleans are not implemented; queued under roadmap §4.2.a.
    Difference diff;
    auto mesh = diff.tessellate(0);
    EXPECT_TRUE(mesh->vertices().empty());
    EXPECT_TRUE(mesh->faces().empty());
  }
}
