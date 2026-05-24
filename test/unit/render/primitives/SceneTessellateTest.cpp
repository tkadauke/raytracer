#include <gtest/gtest.h>
#include <memory>
#include "render/primitives/Scene.h"
#include "render/primitives/Box.h"
#include "core/geometry/Mesh.h"

namespace SceneTessellateTest {
  using namespace render;
  using namespace render;
  using namespace render;

  // Scene inherits tessellate() from Composite and provides no override —
  // this is intentional: lights are not part of the geometry tree and are
  // therefore not tessellated. The child primitives added via add() are the
  // only geometry, and Composite::tessellate handles those correctly.

  TEST(SceneTessellate, ShouldTessellateChildrenViaInheritedCompositeMethod) {
    Scene scene;
    scene.add(std::make_shared<Box>(Vector3d(), Vector3d(1, 1, 1)));
    auto mesh = scene.tessellate(0);
    ASSERT_NE(nullptr, mesh);
    // A single box produces 24 vertices and 6 faces
    ASSERT_EQ(24u, mesh->vertices().size());
    ASSERT_EQ(6u, mesh->faces().size());
  }
}
