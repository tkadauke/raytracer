#include "gtest/gtest.h"
#include "raytracer/primitives/Intersection.h"
#include "core/geometry/Mesh.h"

namespace IntersectionTessellateTest {
  using namespace raytracer;

  TEST(IntersectionTessellate, ShouldReturnEmptyMesh) {
    // CSG mesh booleans are not implemented; queued under roadmap §4.2.a.
    Intersection isect;
    auto mesh = isect.tessellate(0);
    EXPECT_TRUE(mesh->vertices().empty());
    EXPECT_TRUE(mesh->faces().empty());
  }
}
