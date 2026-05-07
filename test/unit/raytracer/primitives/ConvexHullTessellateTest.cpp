#include "gtest/gtest.h"
#include "raytracer/primitives/ConvexHull.h"
#include "core/geometry/Mesh.h"

namespace ConvexHullTessellateTest {
  using namespace raytracer;
using namespace render;

  TEST(ConvexHullTessellate, ShouldReturnEmptyMesh) {
    // CSG mesh booleans are not implemented; queued under roadmap §4.2.a.
    ConvexHull ch;
    auto mesh = ch.tessellate(0);
    EXPECT_TRUE(mesh->vertices().empty());
    EXPECT_TRUE(mesh->faces().empty());
  }
}
