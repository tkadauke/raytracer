#include <gtest/gtest.h>

#include "core/formats/ArchiveAssetResolver.h"
#include "test/helpers/ImporterTestHelper.h"

namespace ArchiveAssetResolverTest {

  namespace {
    std::filesystem::path fixture() {
      return std::filesystem::path(
        test::importers::importerFixturePath("3mf/minimal.3mf").toStdString());
    }
  }

  TEST(ArchiveAssetResolver, ShouldResolveEntryFromArchiveRoot) {
    core::ArchiveAssetResolver resolver;
    resolver.addArchive(fixture());

    const core::ResolvedArchiveAsset asset = resolver.resolve("3D/3dmodel.model");

    EXPECT_EQ("3D/3dmodel.model", asset.entryName.toStdString());
    EXPECT_FALSE(asset.data.isEmpty());
    EXPECT_NE(std::string::npos, asset.identity.find("!/3D/3dmodel.model"));
  }

  TEST(ArchiveAssetResolver, ShouldResolveRelativeToCurrentEntryDirectory) {
    core::ArchiveAssetResolver resolver;
    resolver.addArchive(fixture());

    const core::ResolvedArchiveAsset asset = resolver.resolve("3dmodel.model", "3D/other.model");

    EXPECT_EQ("3D/3dmodel.model", asset.entryName.toStdString());
  }

  TEST(ArchiveAssetResolver, ShouldNormalizeBackslashesAndDotSegments) {
    core::ArchiveAssetResolver resolver;
    resolver.addArchive(fixture());

    const auto direct = resolver.resolve("3D/3dmodel.model");
    const auto messy = resolver.resolve("x\\..\\3D\\3dmodel.model");

    EXPECT_EQ(direct.identity, messy.identity);
  }

  TEST(ArchiveAssetResolver, ShouldMatchCaseOnlyWhenConfigured) {
    core::ArchiveAssetResolver exact;
    exact.addArchive(fixture());
    EXPECT_THROW(static_cast<void>(exact.resolve("3d/3DMODEL.model")),
                 core::AssetResolutionError);

    core::ArchiveAssetResolver relaxed(core::AssetCaseSensitivity::CaseInsensitive);
    relaxed.addArchive(fixture());
    EXPECT_EQ("3D/3dmodel.model",
              relaxed.resolve("3d/3DMODEL.model").entryName.toStdString());
  }

  TEST(ArchiveAssetResolver, ShouldReportRequestedPathAndArchivesWhenMissing) {
    core::ArchiveAssetResolver resolver;
    resolver.addArchive(fixture());

    try {
      static_cast<void>(resolver.resolve("missing/part.dat"));
      FAIL() << "Expected AssetResolutionError";
    } catch (const core::AssetResolutionError& error) {
      EXPECT_EQ("missing/part.dat", error.requestedPath());
      ASSERT_EQ(1u, error.searchedRoots().size());
      EXPECT_NE(std::string::npos, std::string(error.what()).find("minimal.3mf"));
    }
  }

  TEST(ArchiveAssetResolver, ShouldThrowWhenArchiveIsUnreadable) {
    core::ArchiveAssetResolver resolver;
    EXPECT_THROW(resolver.addArchive("/nonexistent/archive.zip"),
                 core::threemf::ThreeMfPackageError);
    EXPECT_TRUE(resolver.empty());
  }

}
