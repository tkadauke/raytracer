#include <gtest/gtest.h>

#include "core/Buffer.h"
#include "core/Exception.h"
#include "core/formats/ldraw/LDrawFileResolver.h"
#include "core/formats/ldraw/LDrawGeometryCompiler.h"
#include "core/geometry/Mesh.h"
#include "core/math/HitPointInterval.h"
#include "engine/raytracer/Raytracer.h"
#include "render/State.h"
#include "render/cameras/PinholeCamera.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/Composite.h"
#include "render/primitives/MeshPrimitive.h"
#include "render/primitives/Scene.h"
#include "render/textures/ConstantColorTexture.h"
#include "test/helpers/ColorTestHelper.h"

#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>

using namespace std;

namespace LDrawGeometryCompilerTest {
  using namespace render;

  LDrawColorTable colorTable() {
    LDrawColorTable table;
    table.add(table.parseColourRecord("0 !COLOUR Bright_Red CODE 4 VALUE #C91A09 EDGE #333333", 1));
    table.add(table.parseColourRecord("0 !COLOUR Bright_Blue CODE 1 VALUE #0055BF EDGE #333333", 2));
    table.add(table.parseColourRecord("0 !COLOUR Bright_Green CODE 2 VALUE #237841 EDGE #333333", 3));
    return table;
  }

  class MemoryResolver : public LDrawFileResolver {
  public:
    explicit MemoryResolver(map<string, string> files)
        : m_files(std::move(files)) {
    }

    unique_ptr<istream> open(const string& filename) const override {
      ++openCalls;
      auto it = m_files.find(cacheKey(filename));
      if (it == m_files.end())
        return nullptr;
      return make_unique<istringstream>(it->second);
    }

    string cacheKey(const string& filename) const override {
      string key = filename;
      for (auto& c : key) {
        if (c == '\\')
          c = '/';
        else
          c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
      }
      return key;
    }

    mutable int openCalls = 0;

  private:
    map<string, string> m_files;
  };

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

  TEST(LDrawGeometryCompiler, TypeOneAppliesTranslationAndMatrixToReferencedGeometry) {
    auto resolver = make_shared<LDrawFilesystemResolver>(
      vector<string>{"test/fixtures/ldraw/nested"});
    ifstream input("test/fixtures/ldraw/nested/root.ldr");

    auto geometry = LDrawGeometryCompiler(resolver).compile(input, colorTable());
    auto mesh = geometry->tessellate();

    ASSERT_EQ(3u, mesh->vertices().size());
    EXPECT_EQ(Vector3d(6, 23, 42), mesh->vertices()[0].point);
    EXPECT_EQ(Vector3d(6, 26, 42), mesh->vertices()[1].point);
    EXPECT_EQ(Vector3d(4, 23, 42), mesh->vertices()[2].point);
  }

  TEST(LDrawGeometryCompiler, NestedMpdSubmodelsRenderThroughRaytracer) {
    auto resolver = make_shared<MemoryResolver>(map<string, string>{
      {"child.dat", "0 // external child should lose to MPD-local child\n"},
      {"leaf.dat", "0 // external leaf should lose to MPD-local leaf\n"}});
    istringstream input(
      "0 FILE main.ldr\n"
      "1 16 0 0 0 1 0 0 0 1 0 0 0 1 child.dat\n"
      "0 NOFILE\n"
      "0 FILE child.dat\n"
      "1 16 0 0 0 1 0 0 0 1 0 0 0 1 leaf.dat\n"
      "0 NOFILE\n"
      "0 FILE leaf.dat\n"
      "3 4 -1 -1 0 -1 1 0 1 -1 0\n"
      "0 NOFILE\n");
    auto scene = make_shared<Scene>(Colord::white());
    scene->setBackground(Colord::black());
    scene->add(LDrawGeometryCompiler(resolver).compile(input, colorTable()));

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
    EXPECT_EQ(0, resolver->openCalls);
  }

  TEST(LDrawGeometryCompiler, TypeOneColorSixteenInheritsReferenceColorAndDirectColorsOverride) {
    auto resolver = make_shared<MemoryResolver>(map<string, string>{
      {"child.dat",
       "3 16 0 0 0 1 0 0 0 1 0\n"
       "3 4 2 0 0 3 0 0 2 1 0\n"}});
    istringstream input("1 1 0 0 0 1 0 0 0 1 0 0 0 1 child.dat\n");
    auto geometry = LDrawGeometryCompiler(resolver).compile(input, colorTable());
    State state;

    HitPointInterval hitPoints;
    const Primitive* inheritedHit = geometry->intersect(
      Rayd(Vector4d(0.2, 0.2, -1.0), Vector3d(0, 0, 1)), hitPoints, state);
    ASSERT_NE(nullptr, inheritedHit);
    ASSERT_COLOR_NEAR(Colord::fromRGB(0, 85, 191), diffuseColor(inheritedHit->material()), 0.001);

    hitPoints = HitPointInterval();
    const Primitive* directHit = geometry->intersect(
      Rayd(Vector4d(2.2, 0.2, -1.0), Vector3d(0, 0, 1)), hitPoints, state);
    ASSERT_NE(nullptr, directHit);
    ASSERT_COLOR_NEAR(Colord::fromRGB(201, 26, 9), diffuseColor(directHit->material()), 0.001);
  }

  TEST(LDrawGeometryCompiler, ReferencedFilesAreParsedThroughResolverOnlyOnce) {
    auto resolver = make_shared<MemoryResolver>(map<string, string>{
      {"child.dat", "3 16 0 0 0 1 0 0 0 1 0\n"}});
    istringstream input(
      "1 2 0 0 0 1 0 0 0 1 0 0 0 1 child.dat\n"
      "1 2 5 0 0 1 0 0 0 1 0 0 0 1 CHILD.DAT\n");

    auto geometry = LDrawGeometryCompiler(resolver).compile(input, colorTable());

    EXPECT_EQ(1, resolver->openCalls);
    ASSERT_EQ(2u, geometry->primitives().size());
  }

  TEST(LDrawGeometryCompiler, DetectsRecursiveSubfileCycles) {
    auto resolver = make_shared<MemoryResolver>(map<string, string>{
      {"a.dat", "1 16 0 0 0 1 0 0 0 1 0 0 0 1 b.dat\n"},
      {"b.dat", "1 16 0 0 0 1 0 0 0 1 0 0 0 1 a.dat\n"}});
    istringstream input("1 16 0 0 0 1 0 0 0 1 0 0 0 1 a.dat\n");

    EXPECT_THROW(LDrawGeometryCompiler(resolver).compile(input, colorTable()), Exception);
  }

  TEST(LDrawGeometryCompiler, EnforcesRecursionLimit) {
    auto resolver = make_shared<MemoryResolver>(map<string, string>{
      {"a.dat", "1 16 0 0 0 1 0 0 0 1 0 0 0 1 b.dat\n"},
      {"b.dat", "3 16 0 0 0 1 0 0 0 1 0\n"}});
    istringstream input("1 16 0 0 0 1 0 0 0 1 0 0 0 1 a.dat\n");

    EXPECT_THROW(LDrawGeometryCompiler(resolver, 1).compile(input, colorTable()), Exception);
  }
}
