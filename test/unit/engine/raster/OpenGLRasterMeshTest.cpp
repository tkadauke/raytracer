#include <gtest/gtest.h>

#include "engine/raster/detail/OpenGLRasterMesh.h"
#include "render/cameras/PinholeCamera.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "render/primitives/Triangle.h"
#include "render/textures/ConstantColorTexture.h"

#include <atomic>
#include <memory>

namespace OpenGLRasterMeshTest {
  using engine::raster::detail::OpenGLRasterMeshBuilder;

  std::shared_ptr<render::PinholeCamera> camera() {
    return std::make_shared<render::PinholeCamera>(Vector3d(0, 0, -5), Vector3d::null);
  }

  std::shared_ptr<render::MatteMaterial> matte(const Colord& color) {
    return std::make_shared<render::MatteMaterial>(
      std::make_shared<render::ConstantColorTexture>(color));
  }

  TEST(OpenGLRasterMesh, BuildsIndexedVerticesFromVisibleTriangle) {
    auto scene = std::make_shared<render::Scene>(Colord::black());
    auto triangle = std::make_shared<render::Triangle>(Vector3d(-1, -1, 0), Vector3d(1, -1, 0),
                                                       Vector3d(0, 1, 0));
    triangle->setMaterial(matte(Colord::red()));
    scene->add(triangle);
    std::atomic<bool> cancelled{false};

    const auto mesh =
      OpenGLRasterMeshBuilder(scene.get(), camera(), 0, Recti(64, 48), cancelled).build();

    ASSERT_FALSE(mesh.empty());
    EXPECT_EQ(1u, mesh.triangleCount());
    ASSERT_EQ(3u, mesh.vertices().size());
    EXPECT_EQ((std::vector<std::uint32_t>{0, 1, 2}), mesh.indices());
    for (const auto& vertex : mesh.vertices()) {
      EXPECT_GE(vertex.x, -1.0f);
      EXPECT_LE(vertex.x, 1.0f);
      EXPECT_GE(vertex.y, -1.0f);
      EXPECT_LE(vertex.y, 1.0f);
      EXPECT_FLOAT_EQ(1.0f, vertex.r);
      EXPECT_FLOAT_EQ(0.0f, vertex.g);
      EXPECT_FLOAT_EQ(0.0f, vertex.b);
    }
  }

  TEST(OpenGLRasterMesh, HonorsRasterLodDuringTessellation) {
    auto scene = std::make_shared<render::Scene>(Colord::black());
    scene->add(std::make_shared<render::Sphere>(Vector3d::null, 1.0));
    std::atomic<bool> cancelled{false};

    const auto lod0 =
      OpenGLRasterMeshBuilder(scene.get(), camera(), 0, Recti(64, 48), cancelled).build();
    const auto lod1 =
      OpenGLRasterMeshBuilder(scene.get(), camera(), 1, Recti(64, 48), cancelled).build();

    EXPECT_FALSE(lod0.empty());
    EXPECT_GT(lod1.triangleCount(), lod0.triangleCount());
  }

  TEST(OpenGLRasterMesh, StopsWhenCancelled) {
    auto scene = std::make_shared<render::Scene>(Colord::black());
    scene->add(std::make_shared<render::Sphere>(Vector3d::null, 1.0));
    std::atomic<bool> cancelled{true};

    const auto mesh =
      OpenGLRasterMeshBuilder(scene.get(), camera(), 0, Recti(64, 48), cancelled).build();

    EXPECT_TRUE(mesh.empty());
  }
}
