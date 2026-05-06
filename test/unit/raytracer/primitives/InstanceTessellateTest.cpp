#include <gtest/gtest.h>
#include <cmath>
#include <memory>
#include "raytracer/primitives/Instance.h"
#include "raytracer/primitives/Box.h"
#include "core/math/Matrix.h"
#include "core/geometry/Mesh.h"

namespace InstanceTessellateTest {
  using namespace raytracer;

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
      EXPECT_NEAR(original->vertices()[i].point.x() + 10.0,
                  transformed->vertices()[i].point.x(), kEps)
        << "vertex " << i << " x";
      EXPECT_NEAR(original->vertices()[i].point.y(),
                  transformed->vertices()[i].point.y(), kEps)
        << "vertex " << i << " y";
      EXPECT_NEAR(original->vertices()[i].point.z(),
                  transformed->vertices()[i].point.z(), kEps)
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
      EXPECT_NEAR(original->vertices()[i].uv.x(),
                  transformed->vertices()[i].uv.x(), kEps);
      EXPECT_NEAR(original->vertices()[i].uv.y(),
                  transformed->vertices()[i].uv.y(), kEps);
    }
  }
}
