#include "gtest/gtest.h"
#include "raytracer/primitives/MinkowskiSum.h"
#include "core/geometry/Mesh.h"

namespace MinkowskiSumTessellateTest {
  using namespace raytracer;
using namespace render;

  TEST(MinkowskiSumTessellate, ShouldReturnEmptyMesh) {
    // CSG mesh booleans are not implemented; queued under roadmap §4.2.a.
    MinkowskiSum ms;
    auto mesh = ms.tessellate(0);
    EXPECT_TRUE(mesh->vertices().empty());
    EXPECT_TRUE(mesh->faces().empty());
  }
}
