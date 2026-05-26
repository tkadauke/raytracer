#include <gtest/gtest.h>

#include "core/formats/ldraw/LDrawFileResolver.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace std;
namespace fs = std::filesystem;

namespace LDrawFileResolverTest {
  class TempTree {
  public:
    TempTree()
        : m_root(fs::temp_directory_path() /
                 fs::path(string("raytracer-ldraw-files-") +
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

  TEST(LDrawFilesystemResolver, ResolvesBackslashReferencesThroughSearchRoots) {
    TempTree tree;
    const fs::path part = tree.write("parts/s/3001s01.dat", "0 Subpart\n");
    LDrawFilesystemResolver resolver({(tree.root() / "parts").string()});

    auto input = resolver.open("s\\3001S01.DAT");

    ASSERT_NE(nullptr, input);
    string text;
    getline(*input, text);
    EXPECT_EQ("0 Subpart", text);
    EXPECT_EQ(part, fs::path(resolver.resolvePath("s\\3001S01.DAT")));
    EXPECT_NE(string::npos, resolver.cacheKey("s\\3001S01.DAT").find("3001s01.dat"));
  }

  TEST(LDrawFilesystemResolver, ReportsSearchRoots) {
    LDrawFilesystemResolver resolver({"test/fixtures/ldraw/nested", "test/fixtures/ldraw/missing"});

    EXPECT_EQ((vector<string>{".", "test/fixtures/ldraw/nested", "test/fixtures/ldraw/missing"}),
              resolver.searchRoots("missing.dat"));
  }

  TEST(LDrawFilesystemResolver, CachesRepeatedResolutionRequests) {
    TempTree tree;
    tree.write("parts/s/3001s01.dat", "0 Subpart\n");
    LDrawFilesystemResolver resolver({(tree.root() / "parts").string()});

    ASSERT_NE(nullptr, resolver.open("s\\3001S01.DAT"));
    EXPECT_FALSE(resolver.cacheKey("s/3001s01.dat").empty());
    EXPECT_FALSE(resolver.resolvePath("S/3001S01.DAT").empty());

    const auto stats = resolver.cacheStats();
    EXPECT_EQ(3u, stats.resolutionRequests);
    EXPECT_EQ(1u, stats.resolutionMisses);
  }

  TEST(LDrawFilesystemResolver, CachesMissingResolutionRequests) {
    TempTree tree;
    LDrawFilesystemResolver resolver({(tree.root() / "parts").string()});

    EXPECT_EQ(nullptr, resolver.open("missing.dat"));
    EXPECT_TRUE(resolver.resolvePath("MISSING.DAT").empty());

    const auto stats = resolver.cacheStats();
    EXPECT_EQ(2u, stats.resolutionRequests);
    EXPECT_EQ(1u, stats.resolutionMisses);
  }
}
