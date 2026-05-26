#include <gtest/gtest.h>

#include "core/formats/ldraw/LDrawColorTable.h"
#include "core/formats/ldraw/LDrawParseError.h"
#include "core/formats/ldraw/LDrawParser.h"
#include "render/materials/MatteMaterial.h"
#include "render/materials/PhongMaterial.h"
#include "render/materials/ReflectiveMaterial.h"
#include "render/materials/TransparentMaterial.h"
#include "render/textures/ConstantColorTexture.h"

#include "test/helpers/ColorTestHelper.h"

#include <filesystem>
#include <fstream>
#include <sstream>

using namespace std;
namespace fs = std::filesystem;

namespace LDrawColorTableTest {
  using namespace render;

  class TempTree {
  public:
    TempTree()
        : m_root(fs::temp_directory_path() /
                 fs::path(string("raytracer-ldraw-colors-") +
                          to_string(::testing::UnitTest::GetInstance()->random_seed()) + "-" +
                          to_string(++s_nextId))) {
      fs::create_directories(m_root);
      std::error_code error;
      const fs::path canonical = fs::weakly_canonical(m_root, error);
      if (!error)
        m_root = canonical;
    }

    ~TempTree() {
      std::error_code error;
      fs::remove_all(m_root, error);
    }

    [[nodiscard]] fs::path root() const {
      return m_root;
    }

    void write(const fs::path& relativePath, const string& contents) const {
      const fs::path path = m_root / relativePath;
      fs::create_directories(path.parent_path());
      ofstream output(path);
      output << contents;
    }

  private:
    fs::path m_root;
    static int s_nextId;
  };

  int TempTree::s_nextId = 0;

  Colord diffuseColor(const shared_ptr<Material>& material) {
    auto matte = dynamic_pointer_cast<MatteMaterial>(material);
    auto texture = dynamic_pointer_cast<ConstantColorTexture>(matte->diffuseTexture());
    return texture->color();
  }

  TEST(LDrawColorTable, ShouldParseColourRecordsFromLDConfigStream) {
    istringstream stream("0 !COLOUR Black CODE 0 VALUE #05131D EDGE #595959\n"
                         "0 !COLOUR Bright_Red CODE 4 VALUE #C91A09 EDGE #333333\n");
    LDrawColorTable table;
    table.parse(stream);

    const auto* red = table.find(4);

    ASSERT_NE(nullptr, red);
    EXPECT_EQ("Bright_Red", red->name);
    EXPECT_EQ(4, red->code);
    ASSERT_COLOR_NEAR(Colord::fromRGB(201, 26, 9), red->value, 0.001);
    EXPECT_EQ(LDrawColorReferenceKind::DirectRgb, red->edge.kind);
    ASSERT_COLOR_NEAR(Colord::fromRGB(51, 51, 51), red->edge.color, 0.001);
  }

  TEST(LDrawColorTable, ShouldLoadLDConfigFromLibraryRoot) {
    TempTree tree;
    tree.write("LDConfig.ldr",
               "0 !COLOUR Chrome_Silver CODE 383 VALUE #E0E0E0 EDGE #333333 CHROME\n");
    LDrawColorTable table;

    ASSERT_TRUE(table.loadLibraryConfig(tree.root().string()));

    const auto* chrome = table.find(383);
    ASSERT_NE(nullptr, chrome);
    EXPECT_EQ("Chrome_Silver", chrome->name);
    EXPECT_EQ(LDrawColorFinish::Chrome, chrome->finish);
  }

  TEST(LDrawColorTable, ShouldParseAlphaLuminanceAndFinishTags) {
    const auto color = LDrawColorTable().parseColourRecord(
      "0 !COLOUR Trans_Red CODE 36 VALUE #C91A09 EDGE #333333 ALPHA 128 LUMINANCE 15 CHROME", 3);

    EXPECT_TRUE(color.transparent());
    EXPECT_EQ(128, color.alpha);
    ASSERT_TRUE(color.luminance.has_value());
    EXPECT_EQ(15, *color.luminance);
    EXPECT_EQ(LDrawColorFinish::Chrome, color.finish);
    ASSERT_EQ(1u, color.finishTokens.size());
    EXPECT_EQ("CHROME", color.finishTokens[0]);
  }

  TEST(LDrawColorTable, ShouldPreserveMaterialFinishTokens) {
    const auto color =
      LDrawColorTable().parseColourRecord("0 !COLOUR Glitter CODE 100 VALUE #123456 EDGE #333333 "
                                          "MATERIAL GLITTER VALUE #FFFFFF FRACTION 0.17",
                                          1);

    EXPECT_EQ(LDrawColorFinish::Glitter, color.finish);
    ASSERT_EQ(6u, color.finishTokens.size());
    EXPECT_EQ("MATERIAL", color.finishTokens[0]);
    EXPECT_EQ("GLITTER", color.finishTokens[1]);
  }

  TEST(LDrawColorTable, ShouldResolveDirectRgbCodes) {
    LDrawColorTable table;

    ASSERT_TRUE(LDrawColorTable::isDirectRgbCode(0x02a1b2c3));
    ASSERT_COLOR_NEAR(Colord::fromRGB(0xa1, 0xb2, 0xc3), table.colorForCode(0x02a1b2c3), 0.001);
  }

  TEST(LDrawParser, ShouldParseDirectRgbColorFields) {
    const auto command = LDrawParser().parseLine("3 0x02a1b2c3 0 0 0 1 0 0 0 1 0", 1);
    const auto& triangle = get<LDrawTriangle>(command);

    EXPECT_EQ(0x02a1b2c3, triangle.color);
  }

  TEST(LDrawColorTable, ShouldResolveCurrentAndEdgeColorsFromContext) {
    LDrawColorTable table;
    table.add(table.parseColourRecord("0 !COLOUR Blue CODE 1 VALUE #0055BF EDGE #111111", 1));
    table.add(table.parseColourRecord("0 !COLOUR Red CODE 4 VALUE #C91A09 EDGE #222222", 2));

    LDrawColorContext context;
    context.currentColor = LDrawColorReference::fromCode(4);
    context.edgeColor = LDrawColorReference::fromDirectRgb(Colord::fromRGB(17, 17, 17));

    ASSERT_COLOR_NEAR(Colord::fromRGB(201, 26, 9), table.colorForCode(16, context), 0.001);
    ASSERT_COLOR_NEAR(Colord::fromRGB(17, 17, 17), table.colorForCode(24, context), 0.001);
  }

  TEST(LDrawColorTable, ShouldResolveLineColor24FromActivePartColorEdge) {
    LDrawColorTable table;
    table.add(table.parseColourRecord("0 !COLOUR Red CODE 4 VALUE #C91A09 EDGE #222222", 1));

    LDrawColorContext context;
    context.currentColor = LDrawColorReference::fromCode(4);
    context.edgeColor = LDrawColorReference::fromDirectRgb(Colord::fromRGB(17, 17, 17));

    ASSERT_COLOR_NEAR(Colord::fromRGB(34, 34, 34), table.edgeColorForCode(24, context), 0.001);
  }

  TEST(LDrawColorTable, ShouldResolveDirectEdgeColors) {
    LDrawColorTable table;

    ASSERT_COLOR_NEAR(Colord::fromRGB(0xa1, 0xb2, 0xc3), table.edgeColorForCode(0x02a1b2c3), 0.001);
  }

  TEST(LDrawColorTable, ShouldBuildSubfileContextFromReferenceColor) {
    LDrawColorTable table;
    table.add(table.parseColourRecord("0 !COLOUR Red CODE 4 VALUE #C91A09 EDGE #222222", 1));

    LDrawColorContext parent;
    parent.currentColor = LDrawColorReference::fromCode(7);
    parent.edgeColor = LDrawColorReference::fromDirectRgb(Colord::fromRGB(17, 17, 17));

    const auto child = table.contextForSubfile(4, parent);

    ASSERT_COLOR_NEAR(Colord::fromRGB(201, 26, 9), table.colorForCode(16, child), 0.001);
    ASSERT_COLOR_NEAR(Colord::fromRGB(34, 34, 34), table.colorForCode(24, child), 0.001);
  }

  TEST(LDrawColorTable, ShouldPreserveInheritedSubfileContextForColor16) {
    LDrawColorTable table;
    table.add(table.parseColourRecord("0 !COLOUR Red CODE 4 VALUE #C91A09 EDGE #222222", 1));

    LDrawColorContext parent;
    parent.currentColor = LDrawColorReference::fromCode(4);
    parent.edgeColor = LDrawColorReference::fromDirectRgb(Colord::fromRGB(17, 17, 17));

    const auto child = table.contextForSubfile(16, parent);

    ASSERT_COLOR_NEAR(Colord::fromRGB(201, 26, 9), table.colorForCode(16, child), 0.001);
    ASSERT_COLOR_NEAR(Colord::fromRGB(17, 17, 17), table.colorForCode(24, child), 0.001);
  }

  TEST(LDrawColorTable, ShouldUseParentEdgeColorForSubfileColor24) {
    LDrawColorTable table;

    LDrawColorContext parent;
    parent.currentColor = LDrawColorReference::fromCode(7);
    parent.edgeColor = LDrawColorReference::fromDirectRgb(Colord::fromRGB(17, 34, 51));

    const auto child = table.contextForSubfile(24, parent);

    ASSERT_COLOR_NEAR(Colord::fromRGB(17, 34, 51), table.colorForCode(16, child), 0.001);
    ASSERT_COLOR_NEAR(Colord::fromRGB(17, 34, 51), table.colorForCode(24, child), 0.001);
  }

  TEST(LDrawColorTable, ShouldMapPlasticToPhongMaterial) {
    LDrawColorTable table;
    table.add(table.parseColourRecord("0 !COLOUR Red CODE 4 VALUE #C91A09 EDGE #333333", 1));

    auto material = table.materialForCode(4);
    auto phong = dynamic_pointer_cast<PhongMaterial>(material);

    ASSERT_NE(nullptr, phong);
    EXPECT_DOUBLE_EQ(0.35, phong->specularCoefficient());
    ASSERT_COLOR_NEAR(Colord::fromRGB(201, 26, 9), diffuseColor(material), 0.001);
  }

  TEST(LDrawColorTable, ShouldMapTransparentColorsToTransparentMaterial) {
    LDrawColorTable table;
    table.add(table.parseColourRecord(
      "0 !COLOUR Trans_Red CODE 36 VALUE #C91A09 EDGE #333333 ALPHA 128", 1));

    auto material = dynamic_pointer_cast<TransparentMaterial>(table.materialForCode(36));

    ASSERT_NE(nullptr, material);
    EXPECT_NEAR(1.0 - 128.0 / 255.0, material->transmissionCoefficient(), 0.001);
  }

  TEST(LDrawColorTable, ShouldMapRubberToMatteMaterial) {
    LDrawColorTable table;
    table.add(table.parseColourRecord(
      "0 !COLOUR Black_Rubber CODE 256 VALUE #05131D EDGE #595959 RUBBER", 1));

    auto material = dynamic_pointer_cast<MatteMaterial>(table.materialForCode(256));

    ASSERT_NE(nullptr, material);
    EXPECT_DOUBLE_EQ(0.85, material->diffuseCoefficient());
  }

  TEST(LDrawColorTable, ShouldMapMetalFinishesToReflectiveMaterials) {
    LDrawColorTable table;
    table.add(table.parseColourRecord(
      "0 !COLOUR Chrome_Silver CODE 383 VALUE #E0E0E0 EDGE #333333 CHROME", 1));
    table.add(table.parseColourRecord(
      "0 !COLOUR Flat_Silver CODE 179 VALUE #898788 EDGE #333333 MATTE_METALLIC", 2));

    auto chrome = dynamic_pointer_cast<ReflectiveMaterial>(table.materialForCode(383));
    auto matteMetal = dynamic_pointer_cast<ReflectiveMaterial>(table.materialForCode(179));

    ASSERT_NE(nullptr, chrome);
    ASSERT_NE(nullptr, matteMetal);
    EXPECT_DOUBLE_EQ(0.8, chrome->reflectionCoefficient());
    EXPECT_DOUBLE_EQ(0.25, matteMetal->reflectionCoefficient());
  }

  TEST(LDrawColorTable, ShouldThrowForMalformedColourRecords) {
    ASSERT_THROW(
      LDrawColorTable().parseColourRecord("0 !COLOUR Red CODE bad VALUE #C91A09 EDGE #333333", 1),
      LDrawParseError);
    ASSERT_THROW(
      LDrawColorTable().parseColourRecord("0 !COLOUR Red CODE 4 VALUE red EDGE #333333", 1),
      LDrawParseError);
  }
}
