#include <gtest/gtest.h>

#include "core/formats/AssetResolver.h"

#include <filesystem>
#include <fstream>
#include <string>

using namespace std;
namespace fs = std::filesystem;

namespace AssetResolverTest {
  class TempTree {
  public:
    TempTree()
        : m_root(fs::temp_directory_path() /
                 fs::path(string("raytracer-asset-resolver-") +
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

    fs::path write(const fs::path& relativePath, const string& contents = "") const {
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

  TEST(AssetResolver, ShouldSearchCurrentFileDirectoryBeforeConfiguredRoots) {
    TempTree tree;
    const fs::path model = tree.write("models/model.scene");
    const fs::path localAsset = tree.write("models/shared.mesh", "local");
    tree.write("library/shared.mesh", "library");

    const core::AssetResolver resolver({tree.root() / "library"});
    const core::ResolvedAsset asset = resolver.resolve("shared.mesh", model);

    EXPECT_EQ(localAsset, asset.path);
  }

  TEST(AssetResolver, ShouldSearchConfiguredRootsInOrderForDuplicateNames) {
    TempTree tree;
    const fs::path firstAsset = tree.write("first/texture.png", "first");
    tree.write("second/texture.png", "second");

    const core::AssetResolver resolver({tree.root() / "first", tree.root() / "second"});
    const core::ResolvedAsset asset = resolver.resolve("texture.png");

    EXPECT_EQ(firstAsset, asset.path);
  }

  TEST(AssetResolver, ShouldReturnStableResolvedIdentitiesForEquivalentReferences) {
    TempTree tree;
    tree.write("assets/mesh.bin");

    const core::AssetResolver resolver({tree.root() / "assets"});
    const core::ResolvedAsset direct = resolver.resolve("mesh.bin");
    const core::ResolvedAsset normalized = resolver.resolve("nested/../mesh.bin");

    EXPECT_EQ(direct.path, normalized.path);
    EXPECT_EQ(direct.identity, normalized.identity);
  }

  TEST(AssetResolver, ShouldReportRequestedPathAndSearchedRootsWhenMissing) {
    TempTree tree;
    const fs::path model = tree.write("models/model.scene");
    const fs::path rootA = tree.root() / "assets-a";
    const fs::path rootB = tree.root() / "assets-b";

    const core::AssetResolver resolver({rootA, rootB});

    try {
      static_cast<void>(resolver.resolve("missing/texture.png", model));
      FAIL() << "Expected AssetResolutionError";
    } catch (const core::AssetResolutionError& error) {
      const string message = error.what();
      EXPECT_EQ("missing/texture.png", error.requestedPath());
      ASSERT_EQ(3u, error.searchedRoots().size());
      EXPECT_NE(string::npos, message.find("missing/texture.png"));
      EXPECT_NE(string::npos, message.find((tree.root() / "models").string()));
      EXPECT_NE(string::npos, message.find(rootA.string()));
      EXPECT_NE(string::npos, message.find(rootB.string()));
    }
  }

  TEST(AssetResolver, ShouldUseExactCaseMatchingByDefault) {
    TempTree tree;
    tree.write("assets/MixedCase.PNG");

    const core::AssetResolver resolver({tree.root() / "assets"});

    EXPECT_THROW(static_cast<void>(resolver.resolve("mixedcase.png")), core::AssetResolutionError);
  }

  TEST(AssetResolver, ShouldResolveCaseInsensitivelyWhenConfigured) {
    TempTree tree;
    const fs::path original = tree.write("assets/MixedCase.PNG");

    const core::AssetResolver resolver({tree.root() / "assets"},
                                       core::AssetCaseSensitivity::CaseInsensitive);
    const core::ResolvedAsset lower = resolver.resolve("mixedcase.png");
    const core::ResolvedAsset originalCase = resolver.resolve("MixedCase.PNG");

    EXPECT_EQ(original, lower.path);
    EXPECT_EQ(lower.identity, originalCase.identity);
  }

}
