#include <gtest/gtest.h>

#include "engine/raster/detail/OpenGLRasterMesh.h"
#include "engine/raster/detail/RasterShadowMaps.h"
#include "render/cameras/PinholeCamera.h"
#include "render/lights/DirectionalLight.h"
#include "render/materials/MatteMaterial.h"
#include "render/materials/PhongMaterial.h"
#include "render/materials/TransparentMaterial.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "render/primitives/Triangle.h"
#include "render/textures/CheckerBoardTexture.h"
#include "render/textures/ConstantColorTexture.h"
#include "render/textures/ImageTexture.h"
#include "render/textures/TintedTexture.h"
#include "render/textures/UVColorTexture.h"
#include "render/textures/mappings/UVMapping2D.h"
#include "render/viewplanes/ViewPlane.h"

#include <atomic>
#include <memory>

namespace OpenGLRasterMeshTest {
  using engine::raster::Rasterizer;
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

    const auto mesh = OpenGLRasterMeshBuilder(scene.get(), camera(), 0, Recti(64, 48),
                                              Rasterizer::CullMode::Both, false, cancelled)
                        .build();

    ASSERT_FALSE(mesh.empty());
    EXPECT_EQ(1u, mesh.triangleCount());
    ASSERT_EQ(3u, mesh.vertices().size());
    EXPECT_EQ((std::vector<std::uint32_t>{0, 1, 2}), mesh.indices());
    ASSERT_EQ(1u, mesh.batches().size());
    EXPECT_EQ(0u, mesh.batches()[0].indexOffset);
    EXPECT_EQ(3u, mesh.batches()[0].indexCount);
    EXPECT_EQ(engine::raster::detail::RasterAlbedoShaderMode::VertexColor,
              mesh.batches()[0].albedo.mode);
    for (const auto& vertex : mesh.vertices()) {
      EXPECT_GE(vertex.x, -1.0f);
      EXPECT_LE(vertex.x, 1.0f);
      EXPECT_GE(vertex.y, -1.0f);
      EXPECT_LE(vertex.y, 1.0f);
      EXPECT_GT(vertex.w, 0.0f);
      EXPECT_FLOAT_EQ(1.0f, vertex.r);
      EXPECT_FLOAT_EQ(0.0f, vertex.g);
      EXPECT_FLOAT_EQ(0.0f, vertex.b);
      EXPECT_FLOAT_EQ(1.0f, vertex.a);
      EXPECT_FLOAT_EQ(1.0f, vertex.alphaScale);
      EXPECT_FLOAT_EQ(0.0f, vertex.albedoMode);
    }
  }

  TEST(OpenGLRasterMesh, CarriesUVColorShaderModeAndUVCoordinates) {
    auto scene = std::make_shared<render::Scene>(Colord::black());
    auto triangle = std::make_shared<render::Triangle>(Vector3d(-1, -1, 0), Vector3d(1, -1, 0),
                                                       Vector3d(0, 1, 0));
    triangle->setMaterial(
      std::make_shared<render::MatteMaterial>(std::make_shared<render::UVColorTexture>()));
    scene->add(triangle);
    std::atomic<bool> cancelled{false};

    const auto mesh = OpenGLRasterMeshBuilder(scene.get(), camera(), 0, Recti(64, 48),
                                              Rasterizer::CullMode::Both, false, cancelled)
                        .build();

    ASSERT_FALSE(mesh.empty());
    ASSERT_EQ(3u, mesh.vertices().size());
    EXPECT_FLOAT_EQ(0.0f, mesh.vertices()[0].u);
    EXPECT_FLOAT_EQ(0.0f, mesh.vertices()[0].v);
    EXPECT_FLOAT_EQ(1.0f, mesh.vertices()[1].u);
    EXPECT_FLOAT_EQ(0.0f, mesh.vertices()[1].v);
    EXPECT_FLOAT_EQ(0.0f, mesh.vertices()[2].u);
    EXPECT_FLOAT_EQ(1.0f, mesh.vertices()[2].v);
    for (const auto& vertex : mesh.vertices()) {
      EXPECT_FLOAT_EQ(1.0f, vertex.alphaScale);
      EXPECT_FLOAT_EQ(1.0f, vertex.albedoMode);
    }
    ASSERT_EQ(1u, mesh.batches().size());
    EXPECT_EQ(engine::raster::detail::RasterAlbedoShaderMode::UVColor,
              mesh.batches()[0].albedo.mode);
  }

  TEST(OpenGLRasterMesh, BatchesImageTextureShaderSource) {
    auto scene = std::make_shared<render::Scene>(Colord::black());
    auto texture = std::make_shared<render::ImageTexture>(new render::UVMapping2D(2.0, 3.0), 1, 1,
                                                          std::vector<Colord>{Colord::white()});
    auto triangle = std::make_shared<render::Triangle>(Vector3d(-1, -1, 0), Vector3d(1, -1, 0),
                                                       Vector3d(0, 1, 0));
    triangle->setMaterial(std::make_shared<render::MatteMaterial>(texture));
    scene->add(triangle);
    std::atomic<bool> cancelled{false};

    const auto mesh = OpenGLRasterMeshBuilder(scene.get(), camera(), 0, Recti(64, 48),
                                              Rasterizer::CullMode::Both, false, cancelled)
                        .build();

    ASSERT_FALSE(mesh.empty());
    ASSERT_EQ(1u, mesh.batches().size());
    const auto& source = mesh.batches()[0].albedo;
    EXPECT_EQ(engine::raster::detail::RasterAlbedoShaderMode::ImageTexture, source.mode);
    EXPECT_EQ(texture.get(), source.image);
    EXPECT_EQ(2.0, source.uScale);
    EXPECT_EQ(3.0, source.vScale);
    for (const auto& vertex : mesh.vertices()) {
      EXPECT_FLOAT_EQ(2.0f, vertex.albedoMode);
    }
  }

  TEST(OpenGLRasterMesh, BatchesTintedImageTextureShaderSource) {
    auto scene = std::make_shared<render::Scene>(Colord::black());
    auto image = std::make_shared<render::ImageTexture>(new render::UVMapping2D(2.0, 3.0), 1, 1,
                                                        std::vector<Colord>{Colord::white()});
    auto tinted = std::make_shared<render::TintedTexture>(image, Colord(0.5, 0.25, 0.75));
    auto triangle = std::make_shared<render::Triangle>(Vector3d(-1, -1, 0), Vector3d(1, -1, 0),
                                                       Vector3d(0, 1, 0));
    triangle->setMaterial(std::make_shared<render::MatteMaterial>(tinted));
    scene->add(triangle);
    std::atomic<bool> cancelled{false};

    const auto mesh = OpenGLRasterMeshBuilder(scene.get(), camera(), 0, Recti(64, 48),
                                              Rasterizer::CullMode::Both, false, cancelled)
                        .build();

    ASSERT_FALSE(mesh.empty());
    ASSERT_EQ(1u, mesh.batches().size());
    const auto& source = mesh.batches()[0].albedo;
    EXPECT_EQ(engine::raster::detail::RasterAlbedoShaderMode::ImageTexture, source.mode);
    EXPECT_EQ(image.get(), source.image);
    EXPECT_EQ(Colord(0.5, 0.25, 0.75), source.tint);
  }

  TEST(OpenGLRasterMesh, BatchesUVCheckerShaderSource) {
    auto scene = std::make_shared<render::Scene>(Colord::black());
    auto checker = std::make_shared<render::CheckerBoardTexture>(
      new render::UVMapping2D(2.0, 4.0),
      std::make_shared<render::ConstantColorTexture>(Colord::red()),
      std::make_shared<render::ConstantColorTexture>(Colord::blue()));
    auto triangle = std::make_shared<render::Triangle>(Vector3d(-1, -1, 0), Vector3d(1, -1, 0),
                                                       Vector3d(0, 1, 0));
    triangle->setMaterial(std::make_shared<render::MatteMaterial>(checker));
    scene->add(triangle);
    std::atomic<bool> cancelled{false};

    const auto mesh = OpenGLRasterMeshBuilder(scene.get(), camera(), 0, Recti(64, 48),
                                              Rasterizer::CullMode::Both, false, cancelled)
                        .build();

    ASSERT_FALSE(mesh.empty());
    ASSERT_EQ(1u, mesh.batches().size());
    const auto& source = mesh.batches()[0].albedo;
    EXPECT_EQ(engine::raster::detail::RasterAlbedoShaderMode::UVChecker, source.mode);
    EXPECT_EQ(2.0, source.uScale);
    EXPECT_EQ(4.0, source.vScale);
    EXPECT_EQ(Colord::red(), source.checkerBright);
    EXPECT_EQ(Colord::blue(), source.checkerDark);
    for (const auto& vertex : mesh.vertices()) {
      EXPECT_FLOAT_EQ(3.0f, vertex.albedoMode);
    }
  }

  TEST(OpenGLRasterMesh, CarriesRasterMaterialAlpha) {
    auto scene = std::make_shared<render::Scene>(Colord::black());
    auto triangle = std::make_shared<render::Triangle>(Vector3d(-1, -1, 0), Vector3d(1, -1, 0),
                                                       Vector3d(0, 1, 0));
    auto material = std::make_shared<render::TransparentMaterial>(
      std::make_shared<render::ConstantColorTexture>(Colord::red()));
    material->setTransmissionCoefficient(0.75);
    triangle->setMaterial(material);
    scene->add(triangle);
    std::atomic<bool> cancelled{false};

    const auto mesh = OpenGLRasterMeshBuilder(scene.get(), camera(), 0, Recti(64, 48),
                                              Rasterizer::CullMode::Both, false, cancelled)
                        .build();

    ASSERT_FALSE(mesh.empty());
    ASSERT_EQ(3u, mesh.vertices().size());
    for (const auto& vertex : mesh.vertices()) {
      EXPECT_FLOAT_EQ(0.25f, vertex.a);
      EXPECT_FLOAT_EQ(0.25f, vertex.alphaScale);
    }
  }

  TEST(OpenGLRasterMesh, CarriesAmbientAndDirectLightingFactor) {
    auto scene = std::make_shared<render::Scene>(Colord(0.25, 0.5, 0.75));
    scene->addLight(
      std::make_shared<render::DirectionalLight>(Vector3d(0, 0, 1), Colord(0.5, 0.25, 0.0)));
    auto triangle = std::make_shared<render::Triangle>(Vector3d(-1, -1, 0), Vector3d(1, -1, 0),
                                                       Vector3d(0, 1, 0));
    triangle->setMaterial(matte(Colord::red()));
    scene->add(triangle);
    std::atomic<bool> cancelled{false};

    const auto mesh = OpenGLRasterMeshBuilder(scene.get(), camera(), 0, Recti(64, 48),
                                              Rasterizer::CullMode::Both, false, cancelled)
                        .build();

    ASSERT_FALSE(mesh.empty());
    ASSERT_EQ(3u, mesh.vertices().size());
    for (const auto& vertex : mesh.vertices()) {
      EXPECT_FLOAT_EQ(0.25f, vertex.ambientR);
      EXPECT_FLOAT_EQ(0.5f, vertex.ambientG);
      EXPECT_FLOAT_EQ(0.75f, vertex.ambientB);
      EXPECT_FLOAT_EQ(0.5f, vertex.directR);
      EXPECT_FLOAT_EQ(0.25f, vertex.directG);
      EXPECT_FLOAT_EQ(0.0f, vertex.directB);
    }
  }

  TEST(OpenGLRasterMesh, AppliesExternalShadowMapsToDirectLighting) {
    auto scene = std::make_shared<render::Scene>(Colord(0.25, 0.25, 0.25));
    auto light =
      std::make_shared<render::DirectionalLight>(Vector3d(0, 0, 1), Colord(1.0, 1.0, 1.0));
    scene->addLight(light);
    auto triangle = std::make_shared<render::Triangle>(Vector3d(-1, -1, 0), Vector3d(1, -1, 0),
                                                       Vector3d(0, 1, 0));
    triangle->setMaterial(matte(Colord::red()));
    scene->add(triangle);

    auto shadowCamera = std::make_shared<engine::raster::detail::DirectionalShadowCamera>(
      Vector3d::null, light->direction(), 2.0);
    shadowCamera->setViewPlane(std::make_shared<render::ViewPlane>());
    shadowCamera->viewPlane()->setup(Matrix4d(), Recti(4, 4));
    auto depthBuffer = std::make_unique<Buffer<double>>(4, 4);
    depthBuffer->clear(0.0);
    std::vector<engine::raster::detail::DirectionalShadowCascade> cascades;
    cascades.push_back({shadowCamera, std::move(depthBuffer), 0.0, 10.0});
    engine::raster::detail::ShadowMaps shadowMaps;
    shadowMaps.add(engine::raster::detail::DirectionalShadowMap(
      light.get(), camera(), std::move(cascades), 0.0, 0.0, 0, Rasterizer::ShadowFilterMode::PCF));
    std::atomic<bool> cancelled{false};

    const auto mesh =
      OpenGLRasterMeshBuilder(scene.get(), camera(), 0, Recti(64, 48), Rasterizer::CullMode::Both,
                              false, cancelled, &shadowMaps)
        .build();

    ASSERT_FALSE(mesh.empty());
    ASSERT_EQ(3u, mesh.vertices().size());
    for (const auto& vertex : mesh.vertices()) {
      EXPECT_FLOAT_EQ(0.25f, vertex.ambientR);
      EXPECT_FLOAT_EQ(0.25f, vertex.ambientG);
      EXPECT_FLOAT_EQ(0.25f, vertex.ambientB);
      EXPECT_FLOAT_EQ(0.0f, vertex.directR);
      EXPECT_FLOAT_EQ(0.0f, vertex.directG);
      EXPECT_FLOAT_EQ(0.0f, vertex.directB);
    }
  }

  TEST(OpenGLRasterMesh, CarriesPhongSpecularLighting) {
    auto scene = std::make_shared<render::Scene>(Colord::black());
    scene->addLight(
      std::make_shared<render::DirectionalLight>(Vector3d(0, 0, -1), Colord(0.2, 0.4, 0.6)));
    auto triangle = std::make_shared<render::Triangle>(
      Vector3d(-0.25, -0.25, 0), Vector3d(0, 0.25, 0), Vector3d(0.25, -0.25, 0));
    auto material = std::make_shared<render::PhongMaterial>(
      std::make_shared<render::ConstantColorTexture>(Colord::black()));
    material->setAmbientCoefficient(0.0);
    material->setDiffuseCoefficient(0.0);
    material->setSpecularColor(Colord::white());
    material->setSpecularCoefficient(0.5);
    material->setExponent(1.0);
    triangle->setMaterial(material);
    scene->add(triangle);
    std::atomic<bool> cancelled{false};

    const auto mesh = OpenGLRasterMeshBuilder(scene.get(), camera(), 0, Recti(64, 48),
                                              Rasterizer::CullMode::Both, false, cancelled)
                        .build();

    ASSERT_FALSE(mesh.empty());
    ASSERT_EQ(3u, mesh.vertices().size());
    for (const auto& vertex : mesh.vertices()) {
      EXPECT_FLOAT_EQ(0.0f, vertex.ambientR);
      EXPECT_FLOAT_EQ(0.0f, vertex.directR);
      EXPECT_GT(vertex.specularR, 0.0f);
      EXPECT_GT(vertex.specularG, vertex.specularR);
      EXPECT_GT(vertex.specularB, vertex.specularG);
    }
  }

  TEST(OpenGLRasterMesh, HonorsRasterLodDuringTessellation) {
    auto scene = std::make_shared<render::Scene>(Colord::black());
    scene->add(std::make_shared<render::Sphere>(Vector3d::null, 1.0));
    std::atomic<bool> cancelled{false};

    const auto lod0 = OpenGLRasterMeshBuilder(scene.get(), camera(), 0, Recti(64, 48),
                                              Rasterizer::CullMode::Both, false, cancelled)
                        .build();
    const auto lod1 = OpenGLRasterMeshBuilder(scene.get(), camera(), 1, Recti(64, 48),
                                              Rasterizer::CullMode::Both, false, cancelled)
                        .build();

    EXPECT_FALSE(lod0.empty());
    EXPECT_GT(lod1.triangleCount(), lod0.triangleCount());
  }

  TEST(OpenGLRasterMesh, StopsWhenCancelled) {
    auto scene = std::make_shared<render::Scene>(Colord::black());
    scene->add(std::make_shared<render::Sphere>(Vector3d::null, 1.0));
    std::atomic<bool> cancelled{true};

    const auto mesh = OpenGLRasterMeshBuilder(scene.get(), camera(), 0, Recti(64, 48),
                                              Rasterizer::CullMode::Both, false, cancelled)
                        .build();

    EXPECT_TRUE(mesh.empty());
  }

  TEST(OpenGLRasterMesh, AppliesCullModeOverrideDuringPreparation) {
    auto scene = std::make_shared<render::Scene>(Colord::black());
    auto triangle = std::make_shared<render::Triangle>(Vector3d(-1, -1, 0), Vector3d(1, -1, 0),
                                                       Vector3d(0, 1, 0));
    triangle->setMaterial(matte(Colord::red()));
    scene->add(triangle);
    std::atomic<bool> cancelled{false};

    const auto unculled = OpenGLRasterMeshBuilder(scene.get(), camera(), 0, Recti(64, 48),
                                                  Rasterizer::CullMode::Both, true, cancelled)
                            .build();
    const auto backfaceCulled = OpenGLRasterMeshBuilder(scene.get(), camera(), 0, Recti(64, 48),
                                                        Rasterizer::CullMode::Back, true, cancelled)
                                  .build();

    EXPECT_FALSE(unculled.empty());
    EXPECT_TRUE(backfaceCulled.empty());
  }
}
