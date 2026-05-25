#include <gtest/gtest.h>
#include "core/formats/ldraw/LDrawLibraryResolver.h"
#include "core/formats/ldraw/LDrawParseError.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <variant>

using namespace std;
namespace fs = std::filesystem;

namespace LDrawLibraryResolverTest {
  class TempTree {
  public:
    TempTree()
        : m_root(fs::temp_directory_path() /
                 fs::path(string("raytracer-ldraw-resolver-") +
                          to_string(::testing::UnitTest::GetInstance()->random_seed()) + "-" +
                          to_string(++s_nextId))) {
      fs::create_directories(m_root);
      std::error_code error;
      const fs::path canonical = fs::weakly_canonical(m_root, error);
      if (!error) {
        m_root = canonical;
      }
    }

    ~TempTree() {
      std::error_code error;
      fs::remove_all(m_root, error);
    }

    [[nodiscard]] fs::path root() const {
      return m_root;
    }

    fs::path write(const fs::path& relativePath, const string& contents) const {
      const fs::path path = m_root / relativePath;
      fs::create_directories(path.parent_path());
      ofstream output(path);
      output << contents;
      return path;
    }

  private:
    fs::path m_root;
    static int s_nextId;
  };

  int TempTree::s_nextId = 0;

  const LDrawSubfileReference& firstSubfileReference(const LDrawResolvedDocument& document) {
    for (const LDrawCommand& command : document.commands) {
      if (holds_alternative<LDrawSubfileReference>(command))
        return get<LDrawSubfileReference>(command);
    }
    ADD_FAILURE() << "Expected a subfile reference";
    static const LDrawSubfileReference fallback;
    return fallback;
  }

  TEST(LDrawLibraryResolver, ShouldFindDirectRelativeReferencesBesideModelFile) {
    TempTree tree;
    const fs::path model = tree.write("models/car.ldr", "1 16 0 0 0 1 0 0 0 1 0 0 0 1 wheel.dat\n");
    tree.write("models/wheel.dat", "0 Wheel beside model\n");

    LDrawLibraryResolver resolver;
    const auto document = resolver.load(model);
    const auto subfile = resolver.resolve(*document, firstSubfileReference(*document).filename);

    ASSERT_EQ((tree.root() / "models" / "wheel.dat"), subfile->path);
    ASSERT_EQ(2u, resolver.cacheSize());
  }

  TEST(LDrawLibraryResolver, ShouldFindOfficialLibraryStyleReferences) {
    TempTree tree;
    const fs::path model = tree.write("work/model.ldr", "0 Model\n");
    const fs::path library = tree.root() / "ldraw";
    tree.write("ldraw/parts/brick.dat", "0 Part\n");
    tree.write("ldraw/parts/s/inside.dat", "0 Subpart\n");
    tree.write("ldraw/p/primitive.dat", "0 Primitive\n");
    tree.write("ldraw/p/48/highres.dat", "0 High-res primitive\n");
    tree.write("ldraw/models/vehicle.ldr", "0 Library model\n");

    LDrawLibraryResolver resolver(library);
    const auto document = resolver.load(model);

    EXPECT_EQ((library / "parts" / "brick.dat"), resolver.resolve(*document, "brick.dat")->path);
    EXPECT_EQ((library / "parts" / "s" / "inside.dat"),
              resolver.resolve(*document, "inside.dat")->path);
    EXPECT_EQ((library / "p" / "primitive.dat"),
              resolver.resolve(*document, "primitive.dat")->path);
    EXPECT_EQ((library / "p" / "48" / "highres.dat"),
              resolver.resolve(*document, "highres.dat")->path);
    EXPECT_EQ((library / "models" / "vehicle.ldr"),
              resolver.resolve(*document, "vehicle.ldr")->path);
  }

  TEST(LDrawLibraryResolver, ShouldResolveReferencesCaseInsensitively) {
    TempTree tree;
    const fs::path model = tree.write("model.ldr", "0 Model\n");
    tree.write("ldraw/parts/MixedCasePart.DAT", "0 Part\n");

    LDrawLibraryResolver resolver(tree.root() / "ldraw");
    const auto document = resolver.load(model);
    const auto subfile = resolver.resolve(*document, "mixedcasepart.dat");

    ASSERT_EQ((tree.root() / "ldraw" / "parts" / "MixedCasePart.DAT"), subfile->path);
  }

  TEST(LDrawLibraryResolver, ShouldReturnCachedDocumentsForRepeatedReferences) {
    TempTree tree;
    const fs::path model = tree.write("model.ldr", "0 Model\n");
    tree.write("ldraw/parts/3001.dat", "0 Brick\n");

    LDrawLibraryResolver resolver(tree.root() / "ldraw");
    const auto document = resolver.load(model);
    const auto first = resolver.resolve(*document, "3001.dat");
    const auto second = resolver.resolve(*document, "3001.dat");

    ASSERT_EQ(first.get(), second.get());
    ASSERT_EQ(2u, resolver.cacheSize());
  }

  TEST(LDrawLibraryResolver, ShouldRecursivelyLoadSubfilesIntoCache) {
    TempTree tree;
    const fs::path model = tree.write("model.ldr", "1 16 0 0 0 1 0 0 0 1 0 0 0 1 3001.dat\n"
                                                   "1 16 0 0 0 1 0 0 0 1 0 0 0 1 3001.dat\n");
    tree.write("ldraw/parts/3001.dat", "1 16 0 0 0 1 0 0 0 1 0 0 0 1 stud.dat\n");
    tree.write("ldraw/p/stud.dat", "0 Stud\n");

    LDrawLibraryResolver resolver(tree.root() / "ldraw");
    resolver.loadWithSubfiles(model);

    ASSERT_EQ(3u, resolver.cacheSize());
  }

  TEST(LDrawLibraryResolver, ShouldDetectRecursiveSubfileCycles) {
    TempTree tree;
    const fs::path model = tree.write("model.ldr", "1 16 0 0 0 1 0 0 0 1 0 0 0 1 a.dat\n");
    tree.write("ldraw/parts/a.dat", "1 16 0 0 0 1 0 0 0 1 0 0 0 1 b.dat\n");
    tree.write("ldraw/parts/b.dat", "1 16 0 0 0 1 0 0 0 1 0 0 0 1 a.dat\n");

    LDrawLibraryResolver resolver(tree.root() / "ldraw");

    try {
      resolver.loadWithSubfiles(model);
      FAIL() << "Expected LDrawParseError";
    } catch (const LDrawParseError& error) {
      ASSERT_NE(string::npos, error.message().find("Cycle detected"));
      ASSERT_NE(string::npos, error.message().find("a.dat"));
      ASSERT_NE(string::npos, error.message().find("b.dat"));
    }
  }

  TEST(LDrawLibraryResolver, ShouldReportMissingFileWithRequestedNameAndSearchedRoots) {
    TempTree tree;
    const fs::path model = tree.write("models/model.ldr", "0 Model\n");

    LDrawLibraryResolver resolver(tree.root() / "ldraw");
    const auto document = resolver.load(model);

    try {
      resolver.resolve(*document, "missing.dat");
      FAIL() << "Expected LDrawParseError";
    } catch (const LDrawParseError& error) {
      ASSERT_NE(string::npos, error.message().find("missing.dat"));
      ASSERT_NE(string::npos, error.message().find((tree.root() / "models").string()));
      ASSERT_NE(string::npos, error.message().find((tree.root() / "ldraw" / "parts").string()));
      ASSERT_NE(string::npos,
                error.message().find((tree.root() / "ldraw" / "parts" / "s").string()));
      ASSERT_NE(string::npos, error.message().find((tree.root() / "ldraw" / "p").string()));
      ASSERT_NE(string::npos, error.message().find((tree.root() / "ldraw" / "p" / "48").string()));
      ASSERT_NE(string::npos, error.message().find((tree.root() / "ldraw" / "models").string()));
    }
  }
}
