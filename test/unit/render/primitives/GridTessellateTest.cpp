#include <gtest/gtest.h>
#include <memory>
#include "render/primitives/Grid.h"
#include "render/primitives/Box.h"
#include "core/geometry/Mesh.h"

namespace GridTessellateTest {
  using namespace raytracer;
using namespace render;

  // Grid inherits tessellate() from Composite and provides no override —
  // this is intentional: the acceleration structure cells are implementation
  // details, but the geometry they contain is exactly the set of child
  // primitives, which Composite::tessellate already handles correctly.

  TEST(GridTessellate, ShouldTessellateChildrenViaInheritedCompositeMethod) {
    Grid grid;
    grid.add(std::make_shared<Box>(Vector3d(), Vector3d(1, 1, 1)));
    auto mesh = grid.tessellate(0);
    ASSERT_NE(nullptr, mesh);
    // A single box produces 24 vertices and 6 faces
    ASSERT_EQ(24u, mesh->vertices().size());
    ASSERT_EQ(6u, mesh->faces().size());
  }
}
