#pragma once

#include "world/import/SceneImporter.h"

#include <QString>

#include <functional>
#include <memory>
#include <vector>

namespace world {

  /**
    * Registry for scene importers keyed by format name and filename extension.
    */
  class SceneImporterRegistry {
  public:
    using Creator = std::function<std::unique_ptr<SceneImporter>()>;

    static SceneImporterRegistry& self();

    template<class Importer>
    bool registerClass(const QString& formatName) {
      return registerImporter(formatName, [] { return std::make_unique<Importer>(); });
    }

    bool registerImporter(const QString& formatName, Creator creator);

    [[nodiscard]] std::unique_ptr<SceneImporter> createByFormat(const QString& formatName) const;
    [[nodiscard]] std::unique_ptr<SceneImporter> createForFile(const QString& filename) const;
    [[nodiscard]] bool hasFormat(const QString& formatName) const;
    [[nodiscard]] bool hasExtension(const QString& extension) const;
    [[nodiscard]] QStringList formats() const;

  private:
    struct Entry {
      QString formatName;
      Creator creator;
    };

    [[nodiscard]] static QString normalizeFormat(QString formatName);
    [[nodiscard]] static QString extensionForFile(const QString& filename);
    [[nodiscard]] const Entry* findByFormat(const QString& formatName) const;
    [[nodiscard]] const Entry* findByExtension(const QString& extension) const;

    std::vector<Entry> m_entries;
  };

}
