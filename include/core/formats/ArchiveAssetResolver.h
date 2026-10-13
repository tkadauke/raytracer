#pragma once

#include "core/formats/AssetResolver.h"
#include "core/formats/threemf/ThreeMfPackage.h"

#include <QByteArray>
#include <QString>

#include <filesystem>
#include <string>
#include <vector>

namespace core {

  /**
    * An asset found inside a ZIP archive.
    *
    * `identity` is `<archive>!/<entry>` so it stays distinct from filesystem
    * identities and is stable across equivalent references.
    */
  struct ResolvedArchiveAsset {
    std::filesystem::path archivePath;
    QString entryName;
    QByteArray data;
    std::string identity;
  };

  /**
    * Resolves importer asset references against ZIP archives (for example an
    * LDraw `complete.zip` library or a zipped glTF bundle) the same way
    * AssetResolver resolves them against directories.
    *
    * References are looked up relative to the directory of `currentEntry`
    * first (when given), then at each archive's root, in the order archives
    * were added. A miss throws AssetResolutionError listing the archives
    * searched.
    */
  class ArchiveAssetResolver {
  public:
    explicit ArchiveAssetResolver(
      AssetCaseSensitivity caseSensitivity = AssetCaseSensitivity::Exact);

    /// Indexes @p archivePath. @throws threemf::ThreeMfPackageError if unreadable.
    void addArchive(const std::filesystem::path& archivePath);
    [[nodiscard]] std::vector<std::filesystem::path> archives() const;
    [[nodiscard]] bool empty() const;

    [[nodiscard]] ResolvedArchiveAsset resolve(const std::string& requestedPath,
                                               const QString& currentEntry = {}) const;

  private:
    struct Archive {
      std::filesystem::path path;
      threemf::ThreeMfPackage package;
    };

    AssetCaseSensitivity m_caseSensitivity;
    std::vector<Archive> m_archives;
  };

}
