#include "gtest/gtest.h"
#include "render/primitives/MinkowskiSum.h"
#include "core/geometry/Mesh.h"

namespace MinkowskiSumTessellateTest {
  using namespace render;
using namespace render;
using namespace render;

  TEST(MinkowskiSumTessellate, ShouldReturnEmptyMesh) {
    // CSG mesh booleans are not implemented; not implemented.
    MinkowskiSum ms;
    auto mesh = ms.tessellate(0);
    EXPECT_TRUE(mesh->vertices().empty());
    EXPECT_TRUE(mesh->faces().empty());
  }
}
