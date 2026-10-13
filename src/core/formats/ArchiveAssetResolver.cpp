#include "core/formats/ArchiveAssetResolver.h"

#include <QDir>

#include <utility>

namespace core {

  namespace {
    QString normalizedEntryName(const std::string& requestedPath, const QString& currentEntry) {
      QString name = threemf::normalizedPartName(QString::fromStdString(requestedPath));
      if (!currentEntry.isEmpty()) {
        const QString directory = QDir::cleanPath(
          QDir(threemf::normalizedPartName(currentEntry)).filePath(".."));
        if (directory != ".")
          name = directory + "/" + name;
      }
      return QDir::cleanPath(name);
    }

    QString findEntry(const threemf::ThreeMfPackage& package, const QString& wanted,
                      AssetCaseSensitivity caseSensitivity) {
      if (package.contains(wanted))
        return wanted;
      if (caseSensitivity == AssetCaseSensitivity::Exact)
        return {};
      for (const QString& name : package.partNames()) {
        if (name.compare(wanted, Qt::CaseInsensitive) == 0)
          return name;
      }
      return {};
    }
  }

  ArchiveAssetResolver::ArchiveAssetResolver(AssetCaseSensitivity caseSensitivity)
      : m_caseSensitivity(caseSensitivity) {
  }

  void ArchiveAssetResolver::addArchive(const std::filesystem::path& archivePath) {
    m_archives.push_back(
      {normalizeExistingPath(archivePath),
       threemf::ThreeMfPackage::read(QString::fromStdString(archivePath.string()))});
  }

  std::vector<std::filesystem::path> ArchiveAssetResolver::archives() const {
    std::vector<std::filesystem::path> paths;
    for (const Archive& archive : m_archives)
      paths.push_back(archive.path);
    return paths;
  }

  bool ArchiveAssetResolver::empty() const {
    return m_archives.empty();
  }

  ResolvedArchiveAsset ArchiveAssetResolver::resolve(const std::string& requestedPath,
                                                     const QString& currentEntry) const {
    std::vector<QString> candidates;
    if (!currentEntry.isEmpty())
      candidates.push_back(normalizedEntryName(requestedPath, currentEntry));
    candidates.push_back(normalizedEntryName(requestedPath, {}));

    for (const Archive& archive : m_archives) {
      for (const QString& candidate : candidates) {
        const QString entry = findEntry(archive.package, candidate, m_caseSensitivity);
        if (entry.isEmpty())
          continue;
        return {archive.path, entry, archive.package.part(entry),
                archive.path.generic_string() + "!/" + entry.toStdString()};
      }
    }

    throw AssetResolutionError(requestedPath, archives());
  }

}
