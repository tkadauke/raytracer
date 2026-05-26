#include <gtest/gtest.h>

#include "core/Buffer.h"
#include "core/Exception.h"
#include "core/formats/ldraw/LDrawFileResolver.h"
#include "core/formats/ldraw/LDrawGeometryCompiler.h"
#include "core/formats/ldraw/LDrawParseError.h"
#include "core/geometry/Mesh.h"
#include "core/math/HitPointInterval.h"
#include "engine/raytracer/Raytracer.h"
#include "engine/wireframe/Wireframe.h"
#include "render/State.h"
#include "render/cameras/PinholeCamera.h"
#include "render/materials/Material.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/Composite.h"
#include "render/primitives/Curve.h"
#include "render/primitives/Instance.h"
#include "render/primitives/MeshPrimitive.h"
#include "render/primitives/Scene.h"
#include "render/textures/ConstantColorTexture.h"
#include "render/textures/ImageTexture.h"
#include "test/helpers/ColorTestHelper.h"

#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

namespace LDrawGeometryCompilerTest {
  using namespace render;

  LDrawColorTable colorTable() {
    LDrawColorTable table;
    table.add(table.parseColourRecord("0 !COLOUR Bright_Red CODE 4 VALUE #C91A09 EDGE #111111", 1));
    table.add(
      table.parseColourRecord("0 !COLOUR Bright_Blue CODE 1 VALUE #0055BF EDGE #222222", 2));
    table.add(
      table.parseColourRecord("0 !COLOUR Bright_Green CODE 2 VALUE #237841 EDGE #333333", 3));
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

  int countPixels(const Buffer<Colord>& buffer, const Colord& color) {
    int count = 0;
    for (int y = 0; y < buffer.height(); ++y)
      for (int x = 0; x < buffer.width(); ++x)
        if (buffer[y][x] == color)
          ++count;
    return count;
  }

  string metadataValue(const Primitive& primitive, const string& key) {
    return primitive.metadataValue(key);
  }

  Vector3d faceNormal(const Mesh& mesh, const Mesh::Face& face) {
    const Vector3d v0 =
      mesh.vertices()[face[face.size() - 1]].point - mesh.vertices()[face[0]].point;
    const Vector3d v1 = mesh.vertices()[face[1]].point - mesh.vertices()[face[0]].point;
    return (v0 ^ v1).normalized();
  }

  void expectFirstFaceNormal(const shared_ptr<Mesh>& mesh, const Vector3d& expected) {
    ASSERT_EQ(1u, mesh->faces().size());
    EXPECT_EQ(expected, faceNormal(*mesh, mesh->faces()[0]));
    for (const auto& vertex : mesh->vertices())
      EXPECT_EQ(expected, vertex.normal);
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

  TEST(LDrawGeometryCompiler, InlineGeometryCarriesSourceLineColorAndBuildStepProvenance) {
    istringstream input("0 STEP\n"
                        "3 4 0 0 0 0 1 0 1 0 0\n");

    auto geometry = LDrawGeometryCompiler().compile(input, colorTable());
    auto primitive = onlyMeshPrimitive(geometry);

    EXPECT_EQ("ldraw", metadataValue(*primitive, "source.format"));
    EXPECT_EQ("<input>", metadataValue(*primitive, "ldraw.source"));
    EXPECT_EQ("2", metadataValue(*primitive, "ldraw.lineStart"));
    EXPECT_EQ("2", metadataValue(*primitive, "ldraw.lineEnd"));
    EXPECT_EQ("4", metadataValue(*primitive, "ldraw.colorCode"));
    EXPECT_EQ("2", metadataValue(*primitive, "ldraw.buildStep"));
    EXPECT_EQ("3", metadataValue(*primitive, "ldraw.command"));
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

  TEST(LDrawGeometryCompiler, TypeTwoEdgeLineProducesOverlayOnlyCurve) {
    LDrawColorContext context;
    context.currentColor = LDrawColorReference::fromCode(4);
    istringstream input("2 24 -1 0 0 1 0 0\n");

    auto geometry = LDrawGeometryCompiler().compile(input, colorTable(), context);

    ASSERT_EQ(1u, geometry->primitives().size());
    auto curve = dynamic_pointer_cast<Curve>(geometry->primitives().front());
    ASSERT_NE(nullptr, curve);
    auto mesh = curve->tessellate();
    ASSERT_NE(nullptr, mesh);
    EXPECT_TRUE(mesh->vertices().empty());
    EXPECT_TRUE(mesh->faces().empty());

    vector<Vector3d> starts;
    vector<Vector3d> ends;
    vector<optional<Colord>> colors;
    geometry->forEachCurveOverlaySegment(
      [&](const Vector3d& start, const Vector3d& end, const optional<Colord>& color) {
        starts.push_back(start);
        ends.push_back(end);
        colors.push_back(color);
      });

    ASSERT_EQ(1u, starts.size());
    EXPECT_EQ(Vector3d(-1, 0, 0), starts[0]);
    EXPECT_EQ(Vector3d(1, 0, 0), ends[0]);
    ASSERT_TRUE(colors[0].has_value());
    ASSERT_COLOR_NEAR(Colord::fromRGB(17, 17, 17), *colors[0], 0.001);
  }

  TEST(LDrawGeometryCompiler, TypeTwoEdgeOverlayCanBeEnabledAndDisabled) {
    LDrawColorContext context;
    context.currentColor = LDrawColorReference::fromCode(4);
    istringstream input("2 24 -0.5 0 0 0.5 0 0\n");
    auto scene = make_shared<Scene>(Colord::white());
    scene->setBackground(Colord::black());
    scene->add(LDrawGeometryCompiler().compile(input, colorTable(), context));
    auto camera = make_shared<PinholeCamera>(Vector3d(0, 0, -2), Vector3d::null);

    Buffer<Colord> disabled(40, 30);
    engine::wireframe::Wireframe solidOnly(camera, scene);
    solidOnly.setGeometryMode(engine::wireframe::Wireframe::GeometryMode::TessellatedEdges);
    solidOnly.render(disabled);
    EXPECT_EQ(0, countPixels(disabled, Colord::fromRGB(17, 17, 17)));

    Buffer<Colord> enabled(40, 30);
    engine::wireframe::Wireframe overlay(camera, scene);
    overlay.setGeometryMode(engine::wireframe::Wireframe::GeometryMode::CurveOverlay);
    overlay.render(enabled);
    EXPECT_GT(countPixels(enabled, Colord::fromRGB(17, 17, 17)), 0);
  }

  TEST(LDrawGeometryCompiler, CanSkipTypeTwoEdgeOverlayGeometry) {
    istringstream input("2 24 -1 0 0 1 0 0\n");
    LDrawColorContext context;
    context.currentColor = LDrawColorReference::fromCode(4);
    LDrawGeometryCompiler::Options options;
    options.includeEdgeOverlays = false;

    auto geometry = LDrawGeometryCompiler(nullptr, options).compile(input, colorTable(), context);

    EXPECT_TRUE(geometry->primitives().empty());
  }

  TEST(LDrawGeometryCompiler, ComputesUsableNormalsForLighting) {
    istringstream input("3 4 0 0 0 0 1 0 1 0 0\n");
    auto geometry = LDrawGeometryCompiler().compile(input, colorTable());
    auto primitive = onlyMeshPrimitive(geometry);

    for (const auto& vertex : primitive->mesh()->vertices()) {
      EXPECT_EQ(Vector3d(0, 0, 1), vertex.normal);
    }
  }

  TEST(LDrawGeometryCompiler, CanBuildSmoothMeshPrimitives) {
    istringstream input("4 4 0 0 0 0 1 0 1 1 0 1 0 0\n");
    auto geometry = LDrawGeometryCompiler(nullptr, 64, LDrawGeometryCompiler::NormalMode::Smooth)
                      .compile(input, colorTable());
    auto primitive = onlyMeshPrimitive(geometry);

    EXPECT_EQ(MeshPrimitive::NormalMode::Smooth, primitive->normalMode());
  }

  TEST(LDrawGeometryCompiler, BfcCertifyCcwKeepsLDrawFaceOrderAndFrontSidedMaterial) {
    istringstream input("0 BFC CERTIFY CCW\n"
                        "3 4 0 0 0 0 1 0 1 0 0\n");

    auto geometry = LDrawGeometryCompiler().compile(input, colorTable());
    auto primitive = onlyMeshPrimitive(geometry);

    EXPECT_EQ((Mesh::Face{0, 1, 2}), primitive->mesh()->faces()[0]);
    expectFirstFaceNormal(primitive->tessellate(), Vector3d(0, 0, 1));
    EXPECT_EQ(Material::Sidedness::Front, primitive->material()->sidedness());
  }

  TEST(LDrawGeometryCompiler, BfcCertifyCwReversesFaceOrderAndNormals) {
    istringstream input("0 BFC CERTIFY CW\n"
                        "3 4 0 0 0 0 1 0 1 0 0\n");

    auto geometry = LDrawGeometryCompiler().compile(input, colorTable());
    auto primitive = onlyMeshPrimitive(geometry);

    EXPECT_EQ((Mesh::Face{0, 2, 1}), primitive->mesh()->faces()[0]);
    expectFirstFaceNormal(primitive->tessellate(), Vector3d(0, 0, -1));
    EXPECT_EQ(Material::Sidedness::Front, primitive->material()->sidedness());
  }

  TEST(LDrawGeometryCompiler, BfcNoClipAndNoCertifyKeepTwoSidedMaterials) {
    istringstream input("0 BFC CERTIFY CCW NOCLIP\n"
                        "3 4 0 0 0 0 1 0 1 0 0\n"
                        "0 BFC NOCERTIFY CLIP\n"
                        "3 1 2 0 0 2 1 0 3 0 0\n");

    auto geometry = LDrawGeometryCompiler().compile(input, colorTable());

    ASSERT_EQ(2u, geometry->primitives().size());
    auto first = dynamic_pointer_cast<MeshPrimitive>(geometry->primitives().front());
    auto second = dynamic_pointer_cast<MeshPrimitive>(geometry->primitives().back());
    ASSERT_NE(nullptr, first);
    ASSERT_NE(nullptr, second);
    EXPECT_EQ(Material::Sidedness::TwoSided, first->material()->sidedness());
    EXPECT_EQ(Material::Sidedness::TwoSided, second->material()->sidedness());
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
    auto resolver =
      make_shared<LDrawFilesystemResolver>(vector<string>{"test/fixtures/ldraw/nested"});
    ifstream input("test/fixtures/ldraw/nested/root.ldr");

    auto geometry = LDrawGeometryCompiler(resolver).compile(input, colorTable());
    auto mesh = geometry->tessellate();

    ASSERT_EQ(3u, mesh->vertices().size());
    EXPECT_EQ(Vector3d(6, 23, 42), mesh->vertices()[0].point);
    EXPECT_EQ(Vector3d(6, 26, 42), mesh->vertices()[1].point);
    EXPECT_EQ(Vector3d(4, 23, 42), mesh->vertices()[2].point);
  }

  TEST(LDrawGeometryCompiler, NestedMpdSubmodelsRenderThroughRaytracer) {
    auto resolver = make_shared<MemoryResolver>(
      map<string, string>{{"child.dat", "0 // external child should lose to MPD-local child\n"},
                          {"leaf.dat", "0 // external leaf should lose to MPD-local leaf\n"}});
    istringstream input("0 FILE main.ldr\n"
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

  TEST(LDrawGeometryCompiler, MpdBlocksAndRecursiveReferencesCarryProvenance) {
    auto resolver =
      make_shared<MemoryResolver>(map<string, string>{{"external.dat", "3 4 0 0 0 0 1 0 1 0 0\n"}});
    istringstream input("0 FILE main.ldr\n"
                        "1 1 0 0 0 1 0 0 0 1 0 0 0 1 child.dat\n"
                        "0 NOFILE\n"
                        "0 FILE child.dat\n"
                        "0 STEP\n"
                        "1 2 0 0 0 1 0 0 0 1 0 0 0 1 external.dat\n"
                        "0 NOFILE\n");

    auto geometry = LDrawGeometryCompiler(resolver).compile(input, colorTable());
    ASSERT_EQ(1u, geometry->primitives().size());
    auto rootReference = dynamic_pointer_cast<Instance>(geometry->primitives().front());
    ASSERT_NE(nullptr, rootReference);

    EXPECT_EQ("<input>", metadataValue(*rootReference, "ldraw.source"));
    EXPECT_EQ("main.ldr", metadataValue(*rootReference, "ldraw.mpdBlock"));
    EXPECT_EQ("2", metadataValue(*rootReference, "ldraw.lineStart"));
    EXPECT_EQ("1", metadataValue(*rootReference, "ldraw.colorCode"));
    EXPECT_EQ("1", metadataValue(*rootReference, "ldraw.buildStep"));
    EXPECT_EQ("child.dat", metadataValue(*rootReference, "ldraw.referencedPart"));
    EXPECT_EQ("<input>", metadataValue(*rootReference, "ldraw.parentReferenceFile"));
    EXPECT_EQ("2", metadataValue(*rootReference, "ldraw.parentReferenceLine"));

    auto childComposite = dynamic_pointer_cast<Composite>(rootReference->primitive());
    ASSERT_NE(nullptr, childComposite);
    ASSERT_EQ(1u, childComposite->primitives().size());
    auto childReference = dynamic_pointer_cast<Instance>(childComposite->primitives().front());
    ASSERT_NE(nullptr, childReference);

    EXPECT_EQ("child.dat", metadataValue(*childReference, "ldraw.source"));
    EXPECT_EQ("child.dat", metadataValue(*childReference, "ldraw.mpdBlock"));
    EXPECT_EQ("6", metadataValue(*childReference, "ldraw.lineStart"));
    EXPECT_EQ("2", metadataValue(*childReference, "ldraw.colorCode"));
    EXPECT_EQ("2", metadataValue(*childReference, "ldraw.buildStep"));
    EXPECT_EQ("external.dat", metadataValue(*childReference, "ldraw.referencedPart"));
    EXPECT_EQ("child.dat", metadataValue(*childReference, "ldraw.parentReferenceFile"));
    EXPECT_EQ("6", metadataValue(*childReference, "ldraw.parentReferenceLine"));

    auto externalComposite = dynamic_pointer_cast<Composite>(childReference->primitive());
    ASSERT_NE(nullptr, externalComposite);
    auto leaf = onlyMeshPrimitive(externalComposite);
    EXPECT_EQ("external.dat", metadataValue(*leaf, "ldraw.source"));
    EXPECT_EQ("", metadataValue(*leaf, "ldraw.mpdBlock"));
    EXPECT_EQ("1", metadataValue(*leaf, "ldraw.lineStart"));
    EXPECT_EQ("4", metadataValue(*leaf, "ldraw.colorCode"));
    EXPECT_EQ("1", metadataValue(*leaf, "ldraw.buildStep"));
  }

  TEST(LDrawGeometryCompiler, DiagnosticsPathResolvesMpdLocalSubmodelsBeforeLibrary) {
    auto resolver = make_shared<MemoryResolver>(
      map<string, string>{{"child.dat", "0 // external child should lose to MPD-local child\n"}});
    istringstream input("0 FILE main.ldr\n"
                        "1 16 0 0 0 1 0 0 0 1 0 0 0 1 child.dat\n"
                        "0 NOFILE\n"
                        "0 FILE child.dat\n"
                        "0 BFC CERTIFY CCW\n"
                        "3 4 -1 -1 0 -1 1 0 1 -1 0\n"
                        "0 NOFILE\n");
    LDrawDiagnostics diagnostics;

    auto geometry = LDrawGeometryCompiler(resolver).compile(input, colorTable(), diagnostics);

    EXPECT_TRUE(diagnostics.entries().empty());
    EXPECT_EQ(0, resolver->openCalls);
    ASSERT_EQ(1u, geometry->primitives().size());
    EXPECT_FALSE(geometry->boundingBox().isInfinite());
  }

  TEST(LDrawGeometryCompiler, TypeOneColorSixteenInheritsReferenceColorAndDirectColorsOverride) {
    auto resolver =
      make_shared<MemoryResolver>(map<string, string>{{"child.dat", "3 16 0 0 0 1 0 0 0 1 0\n"
                                                                    "3 4 2 0 0 3 0 0 2 1 0\n"}});
    istringstream input("1 1 0 0 0 1 0 0 0 1 0 0 0 1 child.dat\n");
    auto geometry = LDrawGeometryCompiler(resolver).compile(input, colorTable());
    State state;

    HitPointInterval hitPoints;
    const Primitive* inheritedHit =
      geometry->intersect(Rayd(Vector4d(0.2, 0.2, -1.0), Vector3d(0, 0, 1)), hitPoints, state);
    ASSERT_NE(nullptr, inheritedHit);
    ASSERT_COLOR_NEAR(Colord::fromRGB(0, 85, 191), diffuseColor(inheritedHit->material()), 0.001);

    hitPoints = HitPointInterval();
    const Primitive* directHit =
      geometry->intersect(Rayd(Vector4d(2.2, 0.2, -1.0), Vector3d(0, 0, 1)), hitPoints, state);
    ASSERT_NE(nullptr, directHit);
    ASSERT_COLOR_NEAR(Colord::fromRGB(201, 26, 9), diffuseColor(directHit->material()), 0.001);
  }

  TEST(LDrawGeometryCompiler, BfcInvertNextInvertsOnlyTheNextTypeOneSubfile) {
    auto resolver =
      make_shared<MemoryResolver>(map<string, string>{{"child.dat", "0 BFC CERTIFY CCW\n"
                                                                    "3 16 0 0 0 0 1 0 1 0 0\n"}});
    istringstream input("0 BFC INVERTNEXT\n"
                        "0 // comments do not consume INVERTNEXT\n"
                        "1 4 0 0 0 1 0 0 0 1 0 0 0 1 child.dat\n"
                        "1 4 2 0 0 1 0 0 0 1 0 0 0 1 child.dat\n");

    auto geometry = LDrawGeometryCompiler(resolver).compile(input, colorTable());
    auto mesh = geometry->tessellate();

    ASSERT_EQ(2u, mesh->faces().size());
    EXPECT_EQ(Vector3d(0, 0, -1), faceNormal(*mesh, mesh->faces()[0]));
    EXPECT_EQ(Vector3d(0, 0, -1), mesh->vertices()[0].normal);
    EXPECT_EQ(Vector3d(0, 0, 1), faceNormal(*mesh, mesh->faces()[1]));
    EXPECT_EQ(Vector3d(0, 0, 1), mesh->vertices()[3].normal);
  }

  TEST(LDrawGeometryCompiler, ReferencedFilesAreParsedThroughResolverOnlyOnce) {
    auto resolver =
      make_shared<MemoryResolver>(map<string, string>{{"child.dat", "3 16 0 0 0 1 0 0 0 1 0\n"}});
    istringstream input("1 2 0 0 0 1 0 0 0 1 0 0 0 1 child.dat\n"
                        "1 2 5 0 0 1 0 0 0 1 0 0 0 1 CHILD.DAT\n");

    auto geometry = LDrawGeometryCompiler(resolver).compile(input, colorTable());

    EXPECT_EQ(1, resolver->openCalls);
    ASSERT_EQ(2u, geometry->primitives().size());
  }

  TEST(LDrawGeometryCompiler, CanFlattenIdentitySubfileHierarchy) {
    auto resolver =
      make_shared<MemoryResolver>(map<string, string>{{"child.dat", "3 16 0 0 0 1 0 0 0 1 0\n"}});
    istringstream input("1 2 0 0 0 1 0 0 0 1 0 0 0 1 child.dat\n");
    LDrawGeometryCompiler::Options options;
    options.preserveHierarchy = false;

    auto geometry = LDrawGeometryCompiler(resolver, options).compile(input, colorTable());

    ASSERT_EQ(1u, geometry->primitives().size());
    EXPECT_NE(nullptr, dynamic_pointer_cast<MeshPrimitive>(geometry->primitives().front()));
  }

  TEST(LDrawGeometryCompiler, RepeatedFixtureReferencesReuseCompiledPartInstances) {
    auto resolver =
      make_shared<LDrawFilesystemResolver>(vector<string>{"test/fixtures/ldraw/repeated"});
    ifstream input("test/fixtures/ldraw/repeated/root.ldr");
    LDrawGeometryCompiler compiler(resolver);

    auto geometry = compiler.compile(input, colorTable());
    auto stats = compiler.cacheStats();

    EXPECT_EQ(1u, stats.parsedSubfileMisses);
    EXPECT_EQ(1u, stats.compiledSubfileMisses);
    EXPECT_EQ(5u, stats.compiledSubfileHits);
    ASSERT_EQ(6u, geometry->primitives().size());

    shared_ptr<Primitive> sharedPart;
    for (const auto& primitive : geometry->primitives()) {
      auto instance = dynamic_pointer_cast<Instance>(primitive);
      ASSERT_NE(nullptr, instance);
      if (!sharedPart)
        sharedPart = instance->primitive();
      EXPECT_EQ(sharedPart, instance->primitive());
    }

    const auto& bbox = geometry->boundingBox();
    EXPECT_NEAR(0.0, bbox.min().x(), 1e-9);
    EXPECT_NEAR(0.0, bbox.min().y(), 1e-9);
    EXPECT_NEAR(0.0, bbox.min().z(), 1e-9);
    EXPECT_NEAR(11.0, bbox.max().x(), 1e-9);
    EXPECT_NEAR(1.0, bbox.max().y(), 1e-9);
    EXPECT_NEAR(0.0, bbox.max().z(), 1e-9);
  }

  TEST(LDrawGeometryCompiler, CacheKeySeparatesInheritedColorContexts) {
    auto resolver =
      make_shared<MemoryResolver>(map<string, string>{{"child.dat", "3 16 0 0 0 1 0 0 0 1 0\n"}});
    istringstream input("1 1 0 0 0 1 0 0 0 1 0 0 0 1 child.dat\n"
                        "1 2 2 0 0 1 0 0 0 1 0 0 0 1 child.dat\n"
                        "1 1 4 0 0 1 0 0 0 1 0 0 0 1 child.dat\n");
    LDrawGeometryCompiler compiler(resolver);

    auto geometry = compiler.compile(input, colorTable());
    auto first = dynamic_pointer_cast<Instance>(geometry->primitives().front());
    auto secondIt = next(geometry->primitives().begin());
    auto second = dynamic_pointer_cast<Instance>(*secondIt);
    auto third = dynamic_pointer_cast<Instance>(geometry->primitives().back());

    ASSERT_NE(nullptr, first);
    ASSERT_NE(nullptr, second);
    ASSERT_NE(nullptr, third);
    EXPECT_NE(first->primitive(), second->primitive());
    EXPECT_EQ(first->primitive(), third->primitive());

    auto stats = compiler.cacheStats();
    EXPECT_EQ(1u, stats.compiledSubfileHits);
    EXPECT_EQ(2u, stats.compiledSubfileMisses);
    EXPECT_EQ(1u, stats.parsedSubfileMisses);

    State state;
    HitPointInterval hitPoints;
    const Primitive* blueHit =
      geometry->intersect(Rayd(Vector4d(0.2, 0.2, -1.0), Vector3d(0, 0, 1)), hitPoints, state);
    ASSERT_NE(nullptr, blueHit);
    ASSERT_COLOR_NEAR(Colord::fromRGB(0, 85, 191), diffuseColor(blueHit->material()), 0.001);

    hitPoints = HitPointInterval();
    const Primitive* greenHit =
      geometry->intersect(Rayd(Vector4d(2.2, 0.2, -1.0), Vector3d(0, 0, 1)), hitPoints, state);
    ASSERT_NE(nullptr, greenHit);
    ASSERT_COLOR_NEAR(Colord::fromRGB(35, 120, 65), diffuseColor(greenHit->material()), 0.001);
  }

  TEST(LDrawGeometryCompiler, DetectsRecursiveSubfileCycles) {
    auto resolver = make_shared<MemoryResolver>(
      map<string, string>{{"a.dat", "1 16 0 0 0 1 0 0 0 1 0 0 0 1 b.dat\n"},
                          {"b.dat", "1 16 0 0 0 1 0 0 0 1 0 0 0 1 a.dat\n"}});
    istringstream input("1 16 0 0 0 1 0 0 0 1 0 0 0 1 a.dat\n");

    EXPECT_THROW(static_cast<void>(LDrawGeometryCompiler(resolver).compile(input, colorTable())),
                 Exception);
  }

  TEST(LDrawGeometryCompiler, EnforcesRecursionLimit) {
    auto resolver = make_shared<MemoryResolver>(map<string, string>{
      {"a.dat", "1 16 0 0 0 1 0 0 0 1 0 0 0 1 b.dat\n"}, {"b.dat", "3 16 0 0 0 1 0 0 0 1 0\n"}});
    istringstream input("1 16 0 0 0 1 0 0 0 1 0 0 0 1 a.dat\n");

    EXPECT_THROW(static_cast<void>(LDrawGeometryCompiler(resolver, 1).compile(input, colorTable())),
                 Exception);
  }

  TEST(LDrawGeometryCompiler, ReportsSkippedGeometryAndUnsupportedCommands) {
    istringstream input("0 !UNSUPPORTED_META value\n"
                        "2 24 0 0 0 1 0 0\n"
                        "5 24 0 0 0 1 0 0 0 1 0 1 1 0\n"
                        "9 unsupported command\n");
    LDrawDiagnostics diagnostics;
    LDrawColorContext context;
    context.currentColor = LDrawColorReference::fromCode(4);

    auto geometry = LDrawGeometryCompiler().compile(input, colorTable(), diagnostics, context);

    ASSERT_EQ(1u, geometry->primitives().size());
    EXPECT_NE(nullptr, dynamic_pointer_cast<Curve>(geometry->primitives().front()));
    ASSERT_EQ(3u, diagnostics.entries().size());
    EXPECT_EQ(LDrawDiagnosticCode::UnsupportedMetaCommand, diagnostics.entries()[0].code);
    EXPECT_EQ("<input>", diagnostics.entries()[0].file);
    EXPECT_EQ(1, diagnostics.entries()[0].lineNumber);
    EXPECT_EQ(LDrawDiagnosticCode::SkippedGeometry, diagnostics.entries()[1].code);
    EXPECT_EQ(3, diagnostics.entries()[1].lineNumber);
    EXPECT_EQ(LDrawDiagnosticCode::UnsupportedLineType, diagnostics.entries()[2].code);
    EXPECT_EQ(4, diagnostics.entries()[2].lineNumber);
  }

  TEST(LDrawGeometryCompiler, IgnoresCommonHeaderMetadata) {
    istringstream input("0 Brick 2 x 4\n"
                        "0 Name: 3001.dat\n"
                        "0 Author: LDraw\n"
                        "0 !LDRAW_ORG Part UPDATE\n"
                        "0 !LICENSE Redistributable under CCAL version 2.0\n"
                        "0 !HISTORY 2026-01-01 Updated header\n"
                        "0 BFC CERTIFY CCW\n"
                        "3 4 0 0 0 0 1 0 1 0 0\n");
    LDrawDiagnostics diagnostics;

    auto geometry = LDrawGeometryCompiler().compile(input, colorTable(), diagnostics);

    ASSERT_EQ(1u, geometry->primitives().size());
    EXPECT_TRUE(diagnostics.entries().empty());
  }

  TEST(LDrawGeometryCompiler, TexmapPlanarAssignsMeshUvsAndImageTextureMaterial) {
    auto resolver =
      make_shared<LDrawFilesystemResolver>(vector<string>{"test/fixtures/ldraw/texmap"});
    istringstream input("0 !TEXMAP START PLANAR -1 -1 0 1 -1 0 -1 1 0 checker.ppm\n"
                        "4 4 -1 -1 0 1 -1 0 1 1 0 -1 1 0\n"
                        "0 !TEXMAP END\n");

    auto geometry = LDrawGeometryCompiler(resolver).compile(input, colorTable());
    auto primitive = onlyMeshPrimitive(geometry);

    ASSERT_EQ(4u, primitive->mesh()->vertices().size());
    EXPECT_EQ(Vector2d(0, 0), primitive->mesh()->vertices()[0].uv);
    EXPECT_EQ(Vector2d(1, 0), primitive->mesh()->vertices()[1].uv);
    EXPECT_EQ(Vector2d(1, 1), primitive->mesh()->vertices()[2].uv);
    EXPECT_EQ(Vector2d(0, 1), primitive->mesh()->vertices()[3].uv);

    auto matte = dynamic_pointer_cast<MatteMaterial>(primitive->material());
    ASSERT_NE(nullptr, matte);
    EXPECT_NE(nullptr, dynamic_pointer_cast<render::ImageTexture>(matte->diffuseTexture()));
  }

  TEST(LDrawGeometryCompiler, TexmapPlanarRendersImageTextureThroughRaytracer) {
    auto resolver =
      make_shared<LDrawFilesystemResolver>(vector<string>{"test/fixtures/ldraw/texmap"});
    istringstream input("0 !TEXMAP START PLANAR -1 -1 0 1 -1 0 -1 1 0 checker.ppm\n"
                        "4 4 -1 -1 0 1 -1 0 1 1 0 -1 1 0\n"
                        "0 !TEXMAP END\n");
    auto scene = make_shared<Scene>(Colord::white());
    scene->setBackground(Colord::black());
    scene->add(LDrawGeometryCompiler(resolver).compile(input, colorTable()));

    Buffer<Colord> buffer(41, 41);
    auto camera = make_shared<PinholeCamera>(Vector3d(0, 0, -2), Vector3d::null);
    auto raytracer = make_shared<engine::raytracer::Raytracer>(camera, scene);
    raytracer->render(buffer);

    ASSERT_COLOR_NEAR(Colord::white(), buffer[20][20], 0.001);
  }

  TEST(LDrawGeometryCompiler, MissingTexmapTextureProducesDiagnostic) {
    auto resolver =
      make_shared<LDrawFilesystemResolver>(vector<string>{"test/fixtures/ldraw/texmap"});
    istringstream input("0 !TEXMAP START PLANAR -1 -1 0 1 -1 0 -1 1 0 missing.ppm\n"
                        "3 4 -1 -1 0 1 -1 0 -1 1 0\n"
                        "0 !TEXMAP END\n");
    LDrawDiagnostics diagnostics;

    auto geometry = LDrawGeometryCompiler(resolver).compile(input, colorTable(), diagnostics);

    EXPECT_EQ(1u, geometry->primitives().size());
    ASSERT_EQ(2u, diagnostics.entries().size());
    EXPECT_EQ(LDrawDiagnosticSeverity::Error, diagnostics.entries()[0].severity);
    EXPECT_EQ(LDrawDiagnosticCode::MissingTexture, diagnostics.entries()[0].code);
    EXPECT_EQ("missing.ppm", diagnostics.entries()[0].reference);
    EXPECT_EQ(LDrawDiagnosticCode::BfcAmbiguity, diagnostics.entries()[1].code);
  }

  TEST(LDrawGeometryCompiler, UnsupportedTexmapUsesFallbackGeometry) {
    istringstream input("0 !TEXMAP START CYLINDRICAL 0 0 0 1 0 0 0 1 0 360 unsupported.png\n"
                        "3 4 0 0 0 1 0 0 0 1 0\n"
                        "0 !TEXMAP FALLBACK\n"
                        "3 1 2 0 0 3 0 0 2 1 0\n"
                        "0 !TEXMAP END\n");
    LDrawDiagnostics diagnostics;

    auto geometry = LDrawGeometryCompiler().compile(input, colorTable(), diagnostics);

    ASSERT_EQ(1u, geometry->primitives().size());
    auto primitive = onlyMeshPrimitive(geometry);
    EXPECT_EQ(Vector3d(2, 0, 0), primitive->mesh()->vertices()[0].point);
    ASSERT_EQ(2u, diagnostics.entries().size());
    EXPECT_EQ(LDrawDiagnosticCode::UnsupportedTexmap, diagnostics.entries()[0].code);
    EXPECT_EQ(1, diagnostics.entries()[0].lineNumber);
    EXPECT_EQ(LDrawDiagnosticCode::BfcAmbiguity, diagnostics.entries()[1].code);
  }

  TEST(LDrawGeometryCompiler, ReportsColorFallbacksAndBfcTwoSidedTreatment) {
    istringstream input("0 BFC NOCERTIFY\n"
                        "3 999 0 0 0 0 1 0 1 0 0\n");
    LDrawDiagnostics diagnostics;

    auto geometry = LDrawGeometryCompiler().compile(input, colorTable(), diagnostics);

    auto primitive = onlyMeshPrimitive(geometry);
    EXPECT_EQ(Material::Sidedness::TwoSided, primitive->material()->sidedness());
    ASSERT_EQ(2u, diagnostics.entries().size());
    EXPECT_EQ(LDrawDiagnosticCode::ColorFallback, diagnostics.entries()[0].code);
    EXPECT_NE(string::npos, diagnostics.entries()[0].message.find("999"));
    EXPECT_EQ(LDrawDiagnosticCode::BfcAmbiguity, diagnostics.entries()[1].code);
    EXPECT_NE(string::npos, diagnostics.entries()[1].message.find("two-sided"));
  }

  TEST(LDrawGeometryCompiler, ReportsBfcFallbackWinding) {
    istringstream input("0 BFC CERTIFY CW\n"
                        "3 4 0 0 0 0 1 0 1 0 0\n");
    LDrawDiagnostics diagnostics;

    auto geometry = LDrawGeometryCompiler().compile(input, colorTable(), diagnostics);

    auto primitive = onlyMeshPrimitive(geometry);
    EXPECT_EQ((Mesh::Face{0, 2, 1}), primitive->mesh()->faces()[0]);
    ASSERT_EQ(1u, diagnostics.entries().size());
    EXPECT_EQ(LDrawDiagnosticCode::BfcAmbiguity, diagnostics.entries()[0].code);
    EXPECT_NE(string::npos, diagnostics.entries()[0].message.find("winding was reversed"));
  }

  TEST(LDrawGeometryCompiler, ReportsMissingSubfileWithReferenceAndSearchRoots) {
    auto resolver = make_shared<LDrawFilesystemResolver>(
      vector<string>{"test/fixtures/ldraw/nested", "test/fixtures/ldraw/missing"});
    istringstream input("1 16 0 0 0 1 0 0 0 1 0 0 0 1 missing.dat\n");
    LDrawDiagnostics diagnostics;

    EXPECT_THROW(
      static_cast<void>(LDrawGeometryCompiler(resolver).compile(input, colorTable(), diagnostics)),
      Exception);

    ASSERT_EQ(1u, diagnostics.entries().size());
    EXPECT_EQ(LDrawDiagnosticSeverity::Error, diagnostics.entries()[0].severity);
    EXPECT_EQ(LDrawDiagnosticCode::MissingSubfile, diagnostics.entries()[0].code);
    EXPECT_EQ("missing.dat", diagnostics.entries()[0].reference);
    EXPECT_EQ((vector<string>{".", "test/fixtures/ldraw/nested", "test/fixtures/ldraw/missing"}),
              diagnostics.entries()[0].searchedRoots);
  }

  TEST(LDrawGeometryCompiler, CanSkipMissingSubfilesWithDiagnostics) {
    auto resolver =
      make_shared<LDrawFilesystemResolver>(vector<string>{"test/fixtures/ldraw/nested"});
    istringstream input("1 16 0 0 0 1 0 0 0 1 0 0 0 1 missing.dat\n"
                        "3 4 0 0 0 0 1 0 1 0 0\n");
    LDrawDiagnostics diagnostics;
    LDrawGeometryCompiler::Options options;
    options.missingPartPolicy = LDrawGeometryCompiler::MissingPartPolicy::Skip;

    auto geometry =
      LDrawGeometryCompiler(resolver, options).compile(input, colorTable(), diagnostics);

    ASSERT_EQ(1u, geometry->primitives().size());
    EXPECT_NE(nullptr, dynamic_pointer_cast<MeshPrimitive>(geometry->primitives().front()));
    EXPECT_FALSE(geometry->boundingBox().isInfinite());
    ASSERT_EQ(2u, diagnostics.entries().size());
    EXPECT_EQ(LDrawDiagnosticCode::MissingSubfile, diagnostics.entries()[0].code);
  }

  TEST(LDrawGeometryCompiler, ReportsFatalParseDiagnostics) {
    istringstream input("3 0x02notrgb 0 0 0 0 1 0 1 0 0\n");
    LDrawDiagnostics diagnostics;

    EXPECT_THROW(
      static_cast<void>(LDrawGeometryCompiler().compile(input, colorTable(), diagnostics)),
      LDrawParseError);

    ASSERT_EQ(1u, diagnostics.entries().size());
    EXPECT_EQ(LDrawDiagnosticSeverity::Error, diagnostics.entries()[0].severity);
    EXPECT_EQ(LDrawDiagnosticCode::DirectColorParseFailure, diagnostics.entries()[0].code);
    EXPECT_EQ(1, diagnostics.entries()[0].lineNumber);
  }
}
