#include <gtest/gtest.h>
#include <cmath>
#include <memory>
#include "render/primitives/Instance.h"
#include "render/primitives/Box.h"
#include "render/primitives/Grid.h"
#include "render/primitives/Scene.h"
#include "core/math/Matrix.h"
#include "core/geometry/Mesh.h"

namespace InstanceTessellateTest {
  using namespace render;

  static constexpr double kEps = 1e-9;

  TEST(InstanceTessellate, ShouldReturnNonNullMeshForTessellableChild) {
    auto box = std::make_shared<Box>(Vector3d(), Vector3d(1, 1, 1));
    Instance instance(box);
    auto mesh = instance.tessellate(0);
    ASSERT_NE(nullptr, mesh);
  }

  TEST(InstanceTessellate, ShouldPreserveVertexCountFromWrappedPrimitive) {
    auto box = std::make_shared<Box>(Vector3d(), Vector3d(1, 1, 1));
    Instance instance(box);
    auto mesh = instance.tessellate(0);
    ASSERT_EQ(24u, mesh->vertices().size());
    ASSERT_EQ(6u, mesh->faces().size());
  }

  TEST(InstanceTessellate, ShouldTranslateVertexPoints) {
    auto box = std::make_shared<Box>(Vector3d(), Vector3d(1, 1, 1));
    Instance instance(box);
    instance.setMatrix(Matrix4d::translate(10.0, 0.0, 0.0));

    auto original = box->tessellate(0);
    auto transformed = instance.tessellate(0);

    ASSERT_EQ(original->vertices().size(), transformed->vertices().size());
    for (std::size_t i = 0; i < original->vertices().size(); ++i) {
      EXPECT_NEAR(original->vertices()[i].point.x() + 10.0, transformed->vertices()[i].point.x(),
                  kEps)
        << "vertex " << i << " x";
      EXPECT_NEAR(original->vertices()[i].point.y(), transformed->vertices()[i].point.y(), kEps)
        << "vertex " << i << " y";
      EXPECT_NEAR(original->vertices()[i].point.z(), transformed->vertices()[i].point.z(), kEps)
        << "vertex " << i << " z";
    }
  }

  TEST(InstanceTessellate, ShouldRotateNormals) {
    auto box = std::make_shared<Box>(Vector3d(), Vector3d(1, 1, 1));
    Instance instance(box);
    // 90-degree rotation around Z: maps (1,0,0) → (0,1,0)
    instance.setMatrix(Matrix3d::rotateZ(Angled::fromDegrees(90)));

    auto mesh = instance.tessellate(0);
    // First 4 vertices belong to the +Z face: original normal = (0,0,1)
    // A rotation around Z does not change (0,0,1), so normals stay the same
    for (int i = 0; i < 4; ++i)
      EXPECT_NEAR(1.0, mesh->vertices()[i].normal.length(), kEps) << "vertex " << i;
  }

  // End-to-end test: Composite-of-Grid-of-Instance tessellation
  // should preserve the Instance transform. This is the path the
  // world-side scene conversion takes — Scene::toRaytracerScene wraps
  // surfaces in Grid(Composite), and the Wireframe engine calls
  // tessellate on the resulting Scene.
  TEST(InstanceTessellate, ShouldPreserveRotationThroughGridAndScene) {
    auto box = std::make_shared<Box>(Vector3d::null, Vector3d(1, 0.5, 0.25));
    auto instance = std::make_shared<Instance>(box);
    instance->setMatrix(Matrix3d::rotateY(Angled::fromDegrees(90)));

    // Mimic Scene::toRaytracerScene: wrap in a Grid, then in a
    // render::Scene.
    auto grid = std::make_shared<Grid>();
    grid->add(instance);
    grid->setup();

    auto scene = std::make_shared<Scene>();
    scene->add(grid);

    auto orig = box->tessellate(0);
    auto throughScene = scene->tessellate(0);
    ASSERT_EQ(orig->vertices().size(), throughScene->vertices().size());

    for (std::size_t i = 0; i < orig->vertices().size(); ++i) {
      const auto& p = orig->vertices()[i].point;
      const auto& q = throughScene->vertices()[i].point;
      EXPECT_NEAR(p.z(), q.x(), 1e-9) << "vertex " << i << " x via scene";
      EXPECT_NEAR(p.y(), q.y(), 1e-9) << "vertex " << i << " y via scene";
      EXPECT_NEAR(-p.x(), q.z(), 1e-9) << "vertex " << i << " z via scene";
    }
  }

  TEST(InstanceTessellate, ShouldRotateVertexPositions) {
    // Regression test for the wireframe-vs-raytracer alignment bug.
    // After a 90-degree rotation around Y, every vertex (x, y, z)
    // should map to (z, y, -x) (right-hand rule, +Y rotation).
    auto box = std::make_shared<Box>(Vector3d::null, Vector3d(1, 0.5, 0.25));
    Instance instance(box);
    instance.setMatrix(Matrix3d::rotateY(Angled::fromDegrees(90)));

    auto orig = box->tessellate(0);
    auto rotated = instance.tessellate(0);
    ASSERT_EQ(orig->vertices().size(), rotated->vertices().size());

    for (std::size_t i = 0; i < orig->vertices().size(); ++i) {
      const auto& p = orig->vertices()[i].point;
      const auto& q = rotated->vertices()[i].point;
      EXPECT_NEAR(p.z(), q.x(), 1e-9) << "vertex " << i << " x";
      EXPECT_NEAR(p.y(), q.y(), 1e-9) << "vertex " << i << " y";
      EXPECT_NEAR(-p.x(), q.z(), 1e-9) << "vertex " << i << " z";
    }
  }

  TEST(InstanceTessellate, ShouldPreserveNormalUnitLengthAfterNonUniformScale) {
    auto box = std::make_shared<Box>(Vector3d(), Vector3d(1, 1, 1));
    Instance instance(box);
    // Non-uniform scale: x×2, y×1, z×3
    instance.setMatrix(Matrix3d::scale(2.0, 1.0, 3.0));

    auto mesh = instance.tessellate(0);
    for (const auto& v : mesh->vertices())
      EXPECT_NEAR(1.0, v.normal.length(), kEps);
  }

  TEST(InstanceTessellate, ShouldPreserveUVCoordinates) {
    auto box = std::make_shared<Box>(Vector3d(), Vector3d(1, 1, 1));
    Instance instance(box);
    instance.setMatrix(Matrix4d::translate(5.0, 5.0, 5.0));

    auto original = box->tessellate(0);
    auto transformed = instance.tessellate(0);

    for (std::size_t i = 0; i < original->vertices().size(); ++i) {
      EXPECT_NEAR(original->vertices()[i].uv.x(), transformed->vertices()[i].uv.x(), kEps);
      EXPECT_NEAR(original->vertices()[i].uv.y(), transformed->vertices()[i].uv.y(), kEps);
    }
  }
}
