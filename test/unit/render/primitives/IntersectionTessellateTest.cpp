#include "gtest/gtest.h"
#include "render/primitives/Intersection.h"
#include "core/geometry/Mesh.h"

namespace IntersectionTessellateTest {
  using namespace render;
using namespace render;
using namespace render;

  TEST(IntersectionTessellate, ShouldReturnEmptyMesh) {
    // CSG mesh booleans are not implemented; not implemented.
    Intersection isect;
    auto mesh = isect.tessellate(0);
    EXPECT_TRUE(mesh->vertices().empty());
    EXPECT_TRUE(mesh->faces().empty());
  }
}
