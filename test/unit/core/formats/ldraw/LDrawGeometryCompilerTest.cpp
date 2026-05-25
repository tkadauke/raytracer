#include <gtest/gtest.h>

#include "core/Buffer.h"
#include "core/formats/ldraw/LDrawGeometryCompiler.h"
#include "core/geometry/Mesh.h"
#include "engine/raytracer/Raytracer.h"
#include "render/cameras/PinholeCamera.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/Composite.h"
#include "render/primitives/MeshPrimitive.h"
#include "render/primitives/Scene.h"
#include "render/textures/ConstantColorTexture.h"
#include "test/helpers/ColorTestHelper.h"

#include <memory>
#include <sstream>

using namespace std;

namespace LDrawGeometryCompilerTest {
  using namespace render;

  LDrawColorTable colorTable() {
    LDrawColorTable table;
    table.add(table.parseColourRecord("0 !COLOUR Bright_Red CODE 4 VALUE #C91A09 EDGE #333333", 1));
    table.add(table.parseColourRecord("0 !COLOUR Bright_Blue CODE 1 VALUE #0055BF EDGE #333333", 2));
    return table;
  }

  Colord diffuseColor(const shared_ptr<Material>& material) {
    auto matte = dynamic_pointer_cast<MatteMaterial>(material);
    auto texture = dynamic_pointer_cast<ConstantColorTexture>(matte->diffuseTexture());
    return texture->color();
  }

  shared_ptr<MeshPrimitive> onlyMeshPrimitive(const shared_ptr<Composite>& composite) {
    EXPECT_EQ(1u, composite->primitives().size());
    auto primitive = dynamic_pointer_cast<MeshPrimitive>(composite->primitives().front());
    EXPECT_NE(nullptr, primitive);
    return primitive;
  }

  TEST(LDrawGeometryCompiler, TypeThreeTriangleProducesOneMeshTriangleWithPoints) {
    istringstream input("3 4 0 0 0 0 1 0 1 0 0\n");
    auto geometry = LDrawGeometryCompiler().compile(input, colorTable());
    auto primitive = onlyMeshPrimitive(geometry);

    ASSERT_EQ(3u, primitive->mesh()->vertices().size());
    ASSERT_EQ(1u, primitive->mesh()->faces().size());
    EXPECT_EQ(Vector3d(0, 0, 0), primitive->mesh()->vertices()[0].point);
    EXPECT_EQ(Vector3d(0, 1, 0), primitive->mesh()->vertices()[1].point);
    EXPECT_EQ(Vector3d(1, 0, 0), primitive->mesh()->vertices()[2].point);
    EXPECT_EQ((Mesh::Face{0, 1, 2}), primitive->mesh()->faces()[0]);
    EXPECT_EQ(1u, primitive->leaves().size());
  }

  TEST(LDrawGeometryCompiler, TypeFourQuadProducesTwoWoundMeshTriangles) {
    istringstream input("4 1 0 0 0 0 1 0 1 1 0 1 0 0\n");
    auto geometry = LDrawGeometryCompiler().compile(input, colorTable());
    auto primitive = onlyMeshPrimitive(geometry);

    ASSERT_EQ(4u, primitive->mesh()->vertices().size());
    ASSERT_EQ(1u, primitive->mesh()->faces().size());
    EXPECT_EQ((Mesh::Face{0, 1, 2, 3}), primitive->mesh()->faces()[0]);

    auto tessellated = primitive->tessellate();
    ASSERT_EQ(2u, tessellated->faces().size());
    EXPECT_EQ((Mesh::Face{0, 1, 2}), tessellated->faces()[0]);
    EXPECT_EQ((Mesh::Face{3, 4, 5}), tessellated->faces()[1]);
    EXPECT_EQ(Vector3d(0, 0, 1), primitive->mesh()->vertices()[0].normal);
  }

  TEST(LDrawGeometryCompiler, AssignsMaterialFromCommandColorCode) {
    istringstream input("3 4 0 0 0 0 1 0 1 0 0\n");
    auto geometry = LDrawGeometryCompiler().compile(input, colorTable());
    auto primitive = onlyMeshPrimitive(geometry);

    ASSERT_COLOR_NEAR(Colord::fromRGB(201, 26, 9), diffuseColor(primitive->material()), 0.001);
  }

  TEST(LDrawGeometryCompiler, ResolvesCurrentColorFromContext) {
    LDrawColorContext context;
    context.currentColor = LDrawColorReference::fromCode(1);
    istringstream input("3 16 0 0 0 0 1 0 1 0 0\n");

    auto geometry = LDrawGeometryCompiler().compile(input, colorTable(), context);
    auto primitive = onlyMeshPrimitive(geometry);

    ASSERT_COLOR_NEAR(Colord::fromRGB(0, 85, 191), diffuseColor(primitive->material()), 0.001);
  }

  TEST(LDrawGeometryCompiler, ComputesUsableNormalsForLighting) {
    istringstream input("3 4 0 0 0 0 1 0 1 0 0\n");
    auto geometry = LDrawGeometryCompiler().compile(input, colorTable());
    auto primitive = onlyMeshPrimitive(geometry);

    for (const auto& vertex : primitive->mesh()->vertices()) {
      EXPECT_EQ(Vector3d(0, 0, 1), vertex.normal);
    }
  }

  TEST(LDrawGeometryCompiler, InlineGeometryRendersThroughRaytracer) {
    istringstream input("3 4 -1 -1 0 -1 1 0 1 -1 0\n");
    auto scene = make_shared<Scene>(Colord::white());
    scene->setBackground(Colord::black());
    scene->add(LDrawGeometryCompiler().compile(input, colorTable()));

    Buffer<Colord> buffer(40, 30);
    auto camera = make_shared<PinholeCamera>(Vector3d(0, 0, -1), Vector3d::null);
    auto raytracer = make_shared<engine::raytracer::Raytracer>(camera, scene);
    raytracer->render(buffer);

    int visiblePixels = 0;
    for (int y = 0; y < buffer.height(); ++y) {
      for (int x = 0; x < buffer.width(); ++x) {
        if (buffer[y][x] != Colord::black())
          ++visiblePixels;
      }
    }

    EXPECT_GT(visiblePixels, 0);
  }
}
