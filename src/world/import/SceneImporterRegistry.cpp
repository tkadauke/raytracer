#include "world/import/SceneImporterRegistry.h"

#include <QFileInfo>

#include <algorithm>
#include <utility>

namespace world {

  SceneImporterRegistry& SceneImporterRegistry::self() {
    static SceneImporterRegistry registry;
    return registry;
  }

  bool SceneImporterRegistry::registerImporter(const QString& formatName, Creator creator) {
    const QString normalized = normalizeFormat(formatName);
    auto existing = std::find_if(m_entries.begin(), m_entries.end(),
                                 [&](const Entry& entry) {
                                   return entry.formatName == normalized;
                                 });
    if (existing != m_entries.end()) {
      existing->creator = std::move(creator);
      return true;
    }

    m_entries.push_back({normalized, std::move(creator)});
    return true;
  }

  std::unique_ptr<SceneImporter>
  SceneImporterRegistry::createByFormat(const QString& formatName) const {
    const Entry* entry = findByFormat(formatName);
    return entry ? entry->creator() : nullptr;
  }

  std::unique_ptr<SceneImporter> SceneImporterRegistry::createForFile(const QString& filename) const {
    const Entry* entry = findByExtension(extensionForFile(filename));
    return entry ? entry->creator() : nullptr;
  }

  bool SceneImporterRegistry::hasFormat(const QString& formatName) const {
    return findByFormat(formatName) != nullptr;
  }

  bool SceneImporterRegistry::hasExtension(const QString& extension) const {
    return findByExtension(extension) != nullptr;
  }

  QStringList SceneImporterRegistry::formats() const {
    QStringList result;
    for (const auto& entry : m_entries) {
      result.push_back(entry.formatName);
    }
    return result;
  }

  QString SceneImporterRegistry::normalizeFormat(QString formatName) {
    formatName = formatName.trimmed().toLower();
    if (formatName.startsWith('.')) {
      formatName.remove(0, 1);
    }
    return formatName;
  }

  QString SceneImporterRegistry::extensionForFile(const QString& filename) {
    return normalizeFormat(QFileInfo(filename).suffix());
  }

  const SceneImporterRegistry::Entry*
  SceneImporterRegistry::findByFormat(const QString& formatName) const {
    const QString normalized = normalizeFormat(formatName);
    auto entry = std::find_if(m_entries.begin(), m_entries.end(), [&](const Entry& candidate) {
      return candidate.formatName == normalized;
    });
    return entry == m_entries.end() ? nullptr : &*entry;
  }

  const SceneImporterRegistry::Entry*
  SceneImporterRegistry::findByExtension(const QString& extension) const {
    const QString normalized = normalizeFormat(extension);
    for (const auto& entry : m_entries) {
      auto importer = entry.creator();
      const QStringList extensions = importer->supportedExtensions();
      for (const QString& supported : extensions) {
        if (normalizeFormat(supported) == normalized) {
          return &entry;
        }
      }
    }
    return nullptr;
  }

}
