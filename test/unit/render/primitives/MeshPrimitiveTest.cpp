#include <gtest/gtest.h>

#include "core/Buffer.h"
#include "core/geometry/MeshAsset.h"
#include "core/geometry/Mesh.h"
#include "core/math/HitPointInterval.h"
#include "engine/raster/Rasterizer.h"
#include "engine/wireframe/Wireframe.h"
#include "render/State.h"
#include "render/cameras/PinholeCamera.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/BVH.h"
#include "render/primitives/FlatMeshTriangle.h"
#include "render/primitives/Grid.h"
#include "render/primitives/MeshPrimitive.h"
#include "render/primitives/Scene.h"
#include "render/primitives/SmoothMeshTriangle.h"
#include "render/textures/ConstantColorTexture.h"

#include <memory>

namespace MeshPrimitiveTest {
  using namespace ::testing;
  using namespace render;

  Mesh makeQuadMesh() {
    Mesh mesh;
    mesh.addVertex(Vector3d(-1, -1, 0), Vector3d(0, 0, 1), Vector2d(0, 0));
    mesh.addVertex(Vector3d(-1, 1, 0), Vector3d(0, 1, 1).normalized(), Vector2d(0, 1));
    mesh.addVertex(Vector3d(1, 1, 0), Vector3d(1, 0, 1).normalized(), Vector2d(1, 1));
    mesh.addVertex(Vector3d(1, -1, 0), Vector3d(1, 1, 1).normalized(), Vector2d(1, 0));
    mesh.addFace({0, 1, 2, 3});
    return mesh;
  }

  Mesh makeTwoFaceMesh() {
    Mesh mesh;
    mesh.addVertex(Vector3d(-1, -1, 0), Vector3d(0, 0, 1), Vector2d(0, 0));
    mesh.addVertex(Vector3d(-1, 1, 0), Vector3d(0, 0, 1), Vector2d(0, 1));
    mesh.addVertex(Vector3d(0, 1, 0), Vector3d(0, 0, 1), Vector2d(0.5, 1));
    mesh.addVertex(Vector3d(0, -1, 0), Vector3d(0, 0, 1), Vector2d(0.5, 0));
    mesh.addVertex(Vector3d(1, 1, 0), Vector3d(0, 0, 1), Vector2d(1, 1));
    mesh.addVertex(Vector3d(1, -1, 0), Vector3d(0, 0, 1), Vector2d(1, 0));
    mesh.addFace({0, 1, 2, 3});
    mesh.addFace({3, 2, 4, 5});
    return mesh;
  }

  std::shared_ptr<MatteMaterial> matte(const Colord& color) {
    return std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(color));
  }

  int countPixels(const Buffer<Colord>& buffer, const Colord& color) {
    int count = 0;
    for (int y = 0; y < buffer.height(); ++y)
      for (int x = 0; x < buffer.width(); ++x)
        if (buffer[y][x] == color)
          ++count;
    return count;
  }

  int countNonBackground(const Buffer<Colord>& buffer, const Colord& background) {
    return buffer.width() * buffer.height() - countPixels(buffer, background);
  }

  std::shared_ptr<Scene> sceneWithMeshPrimitive(std::shared_ptr<MeshPrimitive> primitive) {
    auto scene = std::make_shared<Scene>(Colord::white());
    scene->setBackground(Colord::black());
    scene->add(std::move(primitive));
    return scene;
  }

  TEST(MeshPrimitive, OwnsMeshStorageForBuiltLeaves) {
    auto mesh = std::make_shared<Mesh>(makeQuadMesh());
    MeshPrimitive primitive(mesh, MeshPrimitive::NormalMode::Smooth);
    mesh.reset();

    Rayd ray(Vector3d(0, 0, -1), Vector3d(0, 0, 1));
    State state;
    HitPointInterval hits;

    ASSERT_NE(nullptr, primitive.intersect(ray, hits, state));
    ASSERT_EQ(Vector3d(0, 0, 0), hits.min().point());
  }

  TEST(MeshPrimitive, KeepsSharedMeshAssetAliveForBuiltLeaves) {
    auto asset = std::make_shared<core::MeshAsset>(makeQuadMesh());
    MeshPrimitive primitive(asset, MeshPrimitive::NormalMode::Smooth);
    asset.reset();

    Rayd ray(Vector3d(0, 0, -1), Vector3d(0, 0, 1));
    State state;
    HitPointInterval hits;

    ASSERT_NE(nullptr, primitive.intersect(ray, hits, state));
    ASSERT_EQ(Vector3d(0, 0, 0), hits.min().point());
  }

  TEST(MeshPrimitive, BuildsFlatTriangleLeaves) {
    MeshPrimitive primitive(makeQuadMesh(), MeshPrimitive::NormalMode::Flat);

    ASSERT_EQ(2u, primitive.leaves().size());
    for (const auto& leaf : primitive.leaves()) {
      ASSERT_NE(nullptr, std::dynamic_pointer_cast<FlatMeshTriangle>(leaf));
    }
  }

  TEST(MeshPrimitive, BuildsSmoothTriangleLeavesByDefault) {
    MeshPrimitive primitive(makeQuadMesh());

    ASSERT_EQ(2u, primitive.leaves().size());
    for (const auto& leaf : primitive.leaves()) {
      ASSERT_NE(nullptr, std::dynamic_pointer_cast<SmoothMeshTriangle>(leaf));
    }
  }

  TEST(MeshPrimitive, ComputesBoundingBoxFromTriangleLeaves) {
    MeshPrimitive primitive(makeQuadMesh());

    const BoundingBoxd expected =
      BoundingBoxd(Vector3d(-1, -1, 0), Vector3d(1, 1, 0)).grownByEpsilon();
    ASSERT_EQ(expected, primitive.boundingBox());
  }

  TEST(MeshPrimitive, TessellatesSourceFacesIntoTriangleMesh) {
    MeshPrimitive primitive(makeQuadMesh(), MeshPrimitive::NormalMode::Smooth);

    auto mesh = primitive.tessellate();

    ASSERT_EQ(6u, mesh->vertices().size());
    ASSERT_EQ(2u, mesh->faces().size());
    EXPECT_EQ((Mesh::Face{0, 1, 2}), mesh->faces()[0]);
    EXPECT_EQ((Mesh::Face{3, 4, 5}), mesh->faces()[1]);
  }

  TEST(MeshPrimitive, FlatTessellationUsesOneNormalPerTriangle) {
    MeshPrimitive primitive(makeQuadMesh(), MeshPrimitive::NormalMode::Flat);

    auto mesh = primitive.tessellate();

    ASSERT_EQ(6u, mesh->vertices().size());
    EXPECT_EQ(mesh->vertices()[0].normal, mesh->vertices()[1].normal);
    EXPECT_EQ(mesh->vertices()[1].normal, mesh->vertices()[2].normal);
    EXPECT_EQ(mesh->vertices()[3].normal, mesh->vertices()[4].normal);
    EXPECT_EQ(mesh->vertices()[4].normal, mesh->vertices()[5].normal);
  }

  TEST(MeshPrimitive, SmoothTessellationPreservesVertexNormals) {
    MeshPrimitive primitive(makeQuadMesh(), MeshPrimitive::NormalMode::Smooth);

    auto mesh = primitive.tessellate();

    ASSERT_EQ(6u, mesh->vertices().size());
    EXPECT_NE(mesh->vertices()[0].normal, mesh->vertices()[1].normal);
    EXPECT_NE(mesh->vertices()[1].normal, mesh->vertices()[2].normal);
  }

  TEST(MeshPrimitive, MaterialAssignedAfterConstructionAppliesToHitsAndLeaves) {
    MeshPrimitive primitive(makeQuadMesh());
    auto material = std::make_shared<MatteMaterial>();
    primitive.setMaterial(material);

    Rayd ray(Vector3d(0, 0, -1), Vector3d(0, 0, 1));
    State state;
    HitPointInterval hits;

    ASSERT_EQ(&primitive, primitive.intersect(ray, hits, state));

    int leafCount = 0;
    primitive.forEachLeaf([&](const Primitive*, std::shared_ptr<Material> leafMaterial) {
      ++leafCount;
      EXPECT_EQ(material, leafMaterial);
    });
    EXPECT_EQ(2, leafCount);
  }

  TEST(MeshPrimitive, FaceMaterialsOverridePrimitiveMaterialOnHitsAndLeaves) {
    auto red = matte(Colord::red());
    auto blue = matte(Colord::blue());
    auto fallback = matte(Colord::green());
    MeshPrimitive primitive(makeTwoFaceMesh(), MeshPrimitive::FaceMaterials{red, blue});
    primitive.setMaterial(fallback);
    State state;

    HitPointInterval leftHits;
    const Primitive* left = primitive.intersect(
      Rayd(Vector3d(-0.5, 0.0, -1.0), Vector3d(0, 0, 1)), leftHits, state);
    ASSERT_NE(nullptr, left);
    EXPECT_EQ(red, left->material());

    HitPointInterval rightHits;
    const Primitive* right = primitive.intersect(
      Rayd(Vector3d(0.5, 0.0, -1.0), Vector3d(0, 0, 1)), rightHits, state);
    ASSERT_NE(nullptr, right);
    EXPECT_EQ(blue, right->material());

    int redLeaves = 0;
    int blueLeaves = 0;
    primitive.forEachLeaf([&](const Primitive*, std::shared_ptr<Material> material) {
      if (material == red)
        ++redLeaves;
      if (material == blue)
        ++blueLeaves;
    });
    EXPECT_EQ(2, redLeaves);
    EXPECT_EQ(2, blueLeaves);
  }

  TEST(MeshPrimitive, MissingFaceMaterialsUsePrimitiveFallback) {
    auto red = matte(Colord::red());
    auto fallback = matte(Colord::green());
    MeshPrimitive primitive(makeTwoFaceMesh(), MeshPrimitive::FaceMaterials{red});
    primitive.setMaterial(fallback);
    State state;

    HitPointInterval rightHits;
    const Primitive* right = primitive.intersect(
      Rayd(Vector3d(0.5, 0.0, -1.0), Vector3d(0, 0, 1)), rightHits, state);

    ASSERT_EQ(&primitive, right);
    EXPECT_EQ(fallback, right->material());
  }

  TEST(MeshPrimitive, IntersectsInsideGridTraversal) {
    auto primitive = std::make_shared<MeshPrimitive>(makeQuadMesh());
    Grid grid;
    grid.add(primitive);
    grid.setup();

    Rayd ray(Vector3d(0, 0, -1), Vector3d(0, 0, 1));
    State state;
    HitPointInterval hits;

    ASSERT_NE(nullptr, grid.intersect(ray, hits, state));
    ASSERT_EQ(Vector3d(0, 0, 0), hits.min().point());
  }

  TEST(MeshPrimitive, PreservesFaceMaterialsInsideGridTraversal) {
    auto red = matte(Colord::red());
    auto blue = matte(Colord::blue());
    auto primitive =
      std::make_shared<MeshPrimitive>(makeTwoFaceMesh(), MeshPrimitive::FaceMaterials{red, blue});
    Grid grid;
    grid.add(primitive);
    grid.setup();
    State state;
    HitPointInterval hits;

    const Primitive* hit = grid.intersect(
      Rayd(Vector3d(0.5, 0.0, -1.0), Vector3d(0, 0, 1)), hits, state);

    ASSERT_NE(nullptr, hit);
    EXPECT_EQ(blue, hit->material());
  }

  TEST(MeshPrimitive, IntersectsInsideBVHTraversal) {
    auto primitive = std::make_shared<MeshPrimitive>(makeQuadMesh());
    BVH bvh;
    bvh.add(primitive);
    bvh.setup();

    Rayd ray(Vector3d(0, 0, -1), Vector3d(0, 0, 1));
    State state;
    HitPointInterval hits;

    ASSERT_NE(nullptr, bvh.intersect(ray, hits, state));
    ASSERT_EQ(Vector3d(0, 0, 0), hits.min().point());
  }

  TEST(MeshPrimitive, PreservesFaceMaterialsInsideBVHTraversal) {
    auto red = matte(Colord::red());
    auto blue = matte(Colord::blue());
    auto primitive =
      std::make_shared<MeshPrimitive>(makeTwoFaceMesh(), MeshPrimitive::FaceMaterials{red, blue});
    BVH bvh;
    bvh.add(primitive);
    bvh.setup();
    State state;
    HitPointInterval hits;

    const Primitive* hit =
      bvh.intersect(Rayd(Vector3d(-0.5, 0.0, -1.0), Vector3d(0, 0, 1)), hits, state);

    ASSERT_NE(nullptr, hit);
    EXPECT_EQ(red, hit->material());
  }

  TEST(MeshPrimitive, RendersThroughRasterizer) {
    auto primitive = std::make_shared<MeshPrimitive>(makeQuadMesh());
    primitive->setMaterial(matte(Colord::red()));
    engine::raster::Rasterizer rasterizer(
      std::make_shared<PinholeCamera>(Vector3d(0, 0, -3), Vector3d::null),
      sceneWithMeshPrimitive(primitive));
    Buffer<Colord> buffer(64, 64);

    rasterizer.render(buffer);

    EXPECT_GT(countNonBackground(buffer, Colord::black()), 0);
  }

  TEST(MeshPrimitive, RendersThroughWireframe) {
    auto primitive = std::make_shared<MeshPrimitive>(makeQuadMesh());
    engine::wireframe::Wireframe wireframe(
      std::make_shared<PinholeCamera>(Vector3d(0, 0, -3), Vector3d::null),
      sceneWithMeshPrimitive(primitive));
    Buffer<Colord> buffer(64, 64);

    wireframe.render(buffer);

    EXPECT_GT(countPixels(buffer, Colord::white()), 0);
  }
}
