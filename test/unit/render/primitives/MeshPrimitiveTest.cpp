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
#include "render/primitives/Composite.h"
#include "render/primitives/Instance.h"
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

  Mesh makeTriangleMeshWithZeroNormals() {
    Mesh mesh;
    mesh.addVertex(Vector3d(0, 0, 0), Vector3d::null);
    mesh.addVertex(Vector3d(1, 0, 0), Vector3d::null);
    mesh.addVertex(Vector3d(0, 1, 0), Vector3d::null);
    mesh.addFace({0, 1, 2});
    return mesh;
  }

  Mesh makeMeshWithDegenerateAndValidTriangles() {
    Mesh mesh;
    mesh.addVertex(Vector3d(0, 0, 0), Vector3d::null);
    mesh.addVertex(Vector3d(1, 0, 0), Vector3d::null);
    mesh.addVertex(Vector3d(2, 0, 0), Vector3d::null);
    mesh.addVertex(Vector3d(0, 1, 0), Vector3d::null);
    mesh.addFace({0, 1, 2});
    mesh.addFace({0, 1, 3});
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

  std::shared_ptr<Scene> sceneWithPrimitive(std::shared_ptr<Primitive> primitive) {
    auto scene = std::make_shared<Scene>(Colord::white());
    scene->setBackground(Colord::black());
    scene->add(std::move(primitive));
    return scene;
  }

  std::shared_ptr<Scene> sceneWithMeshPrimitive(std::shared_ptr<MeshPrimitive> primitive) {
    return sceneWithPrimitive(std::move(primitive));
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

  TEST(MeshPrimitive, BuildsFlatLeavesForTrianglesWithZeroSourceNormals) {
    MeshPrimitive primitive(makeTriangleMeshWithZeroNormals(), MeshPrimitive::NormalMode::Flat);

    ASSERT_EQ(1u, primitive.leaves().size());
    auto mesh = primitive.tessellate();
    ASSERT_EQ(3u, mesh->vertices().size());
    EXPECT_DOUBLE_EQ(1.0, mesh->vertices()[0].normal.squaredLength());
  }

  TEST(MeshPrimitive, SkipsDegenerateTriangles) {
    MeshPrimitive primitive(makeMeshWithDegenerateAndValidTriangles(),
                            MeshPrimitive::NormalMode::Flat);

    EXPECT_EQ(1u, primitive.leaves().size());
  }

  TEST(MeshPrimitive, SkipsDegenerateSmoothTrianglesBeforePrecomputation) {
    Mesh mesh;
    mesh.addVertex(Vector3d(0, 0, 0), Vector3d(0, 0, 1));
    mesh.addVertex(Vector3d(1, 0, 0), Vector3d(0, 0, 1));
    mesh.addVertex(Vector3d(2, 0, 0), Vector3d(0, 0, 1));
    mesh.addFace({0, 1, 2});

    MeshPrimitive primitive(std::move(mesh), MeshPrimitive::NormalMode::Smooth);

    EXPECT_TRUE(primitive.leaves().empty());
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

  TEST(MeshPrimitive, ShouldMaterializeRay4PacketHitsFromTriangleLeaves) {
    MeshPrimitive primitive(makeQuadMesh(), MeshPrimitive::NormalMode::Flat);
    const Ray4 rays(std::array<Rayd, Ray4::lanes>{
      Rayd(Vector3d(0, 0, -1), Vector3d(0, 0, 1)), Rayd(Vector3d(2, 0, -1), Vector3d(0, 0, 1)),
      Rayd(Vector3d(-0.5, 0, -1), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 0, 1), Vector3d(0, 0, 1))});
    std::array<State, Ray4::lanes> laneStates;
    PrimitivePacketState4 states{&laneStates[0], &laneStates[1], &laneStates[2], &laneStates[3]};

    const auto result = primitive.intersectPacketHits(rays, states);

    ASSERT_TRUE(result.hit(0));
    EXPECT_NE(nullptr, result.primitive(0));
    EXPECT_EQ(Vector3d(0, 0, 0), result.hitPoint(0).point());
    EXPECT_EQ(1, result.hitPoint(0).distance());
    EXPECT_FALSE(result.hit(1));
    ASSERT_TRUE(result.hit(2));
    EXPECT_NE(nullptr, result.primitive(2));
    EXPECT_EQ(Vector3d(-0.5, 0, 0), result.hitPoint(2).point());
    EXPECT_EQ(1, result.hitPoint(2).distance());
    EXPECT_FALSE(result.hit(3));
  }

  TEST(MeshPrimitive, ShouldUsePrimitiveMaterialFallbackForRay4PacketHits) {
    MeshPrimitive primitive(makeQuadMesh(), MeshPrimitive::NormalMode::Flat);
    primitive.setMaterial(std::make_shared<MatteMaterial>());
    const Ray4 rays(std::array<Rayd, Ray4::lanes>{
      Rayd(Vector3d(0, 0, -1), Vector3d(0, 0, 1)), Rayd(Vector3d(2, 0, -1), Vector3d(0, 0, 1)),
      Rayd(Vector3d(-0.5, 0, -1), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 0, 1), Vector3d(0, 0, 1))});
    std::array<State, Ray4::lanes> laneStates;
    PrimitivePacketState4 states{&laneStates[0], &laneStates[1], &laneStates[2], &laneStates[3]};

    const auto result = primitive.intersectPacketHits(rays, states);

    ASSERT_TRUE(result.hit(0));
    EXPECT_EQ(&primitive, result.primitive(0));
    EXPECT_FALSE(result.hit(1));
    ASSERT_TRUE(result.hit(2));
    EXPECT_EQ(&primitive, result.primitive(2));
    EXPECT_FALSE(result.hit(3));
  }

  TEST(MeshPrimitive, FaceMaterialsOverridePrimitiveMaterialOnHitsAndLeaves) {
    auto red = matte(Colord::red());
    auto blue = matte(Colord::blue());
    auto fallback = matte(Colord::green());
    MeshPrimitive primitive(makeTwoFaceMesh(), MeshPrimitive::FaceMaterials{red, blue});
    primitive.setMaterial(fallback);
    State state;

    HitPointInterval leftHits;
    const Primitive* left =
      primitive.intersect(Rayd(Vector3d(-0.5, 0.0, -1.0), Vector3d(0, 0, 1)), leftHits, state);
    ASSERT_NE(nullptr, left);
    EXPECT_EQ(red, left->material());

    HitPointInterval rightHits;
    const Primitive* right =
      primitive.intersect(Rayd(Vector3d(0.5, 0.0, -1.0), Vector3d(0, 0, 1)), rightHits, state);
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
    const Primitive* right =
      primitive.intersect(Rayd(Vector3d(0.5, 0.0, -1.0), Vector3d(0, 0, 1)), rightHits, state);

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

    const Primitive* hit =
      grid.intersect(Rayd(Vector3d(0.5, 0.0, -1.0), Vector3d(0, 0, 1)), hits, state);

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

  TEST(MeshPrimitive, RasterizerPreservesNestedMaterialThroughInstance) {
    auto primitive = std::make_shared<MeshPrimitive>(makeQuadMesh());
    primitive->setMaterial(matte(Colord::red()));
    auto composite = std::make_shared<Composite>();
    composite->add(primitive);
    auto instance = std::make_shared<Instance>(composite);

    engine::raster::Rasterizer rasterizer(
      std::make_shared<PinholeCamera>(Vector3d(0, 0, -3), Vector3d::null),
      sceneWithPrimitive(instance));
    Buffer<Colord> buffer(64, 64);

    rasterizer.render(buffer);

    EXPECT_GT(countPixels(buffer, Colord::red()), 0);
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
