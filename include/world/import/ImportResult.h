#pragma once

#include "world/import/ImportDiagnostic.h"

#include <QJsonObject>
#include <QString>

#include <memory>
#include <vector>

class Element;
class Group;
class Scene;

namespace world {

  /**
    * Identifies where an imported root came from. Importers can store
    * format-specific IDs, library paths, or parser details in properties.
    */
  struct ImportSourceMetadata {
    QString importerName;
    QString formatName;
    QString sourcePath;
    QJsonObject properties;
  };

  /**
    * Move-only result for a scene import operation.
    *
    * A successful result owns either a Scene or Group root through Element.
    * Diagnostics are preserved on both success and failure so importers can
    * return partial warnings or actionable error details.
    */
  class ImportResult {
  public:
    ImportResult();
    explicit ImportResult(std::unique_ptr<Element> root,
                          ImportSourceMetadata source = ImportSourceMetadata());
    ~ImportResult();

    ImportResult(const ImportResult&) = delete;
    ImportResult& operator=(const ImportResult&) = delete;
    ImportResult(ImportResult&&) noexcept;
    ImportResult& operator=(ImportResult&&) noexcept;

    [[nodiscard]] static ImportResult failed(std::vector<ImportDiagnostic> diagnostics,
                                             ImportSourceMetadata source = ImportSourceMetadata());

    [[nodiscard]] bool succeeded() const;
    [[nodiscard]] bool failed() const;
    [[nodiscard]] bool hasRoot() const;
    [[nodiscard]] bool hasErrors() const;
    [[nodiscard]] bool hasWarnings() const;

    [[nodiscard]] Element* root() const;
    [[nodiscard]] Scene* sceneRoot() const;
    [[nodiscard]] Group* groupRoot() const;
    [[nodiscard]] std::unique_ptr<Element> takeRoot();
    void setRoot(std::unique_ptr<Element> root);

    [[nodiscard]] const std::vector<ImportDiagnostic>& diagnostics() const;
    void addDiagnostic(const ImportDiagnostic& diagnostic);

    [[nodiscard]] const ImportSourceMetadata& source() const;
    void setSource(ImportSourceMetadata source);

  private:
    std::unique_ptr<Element> m_root;
    std::vector<ImportDiagnostic> m_diagnostics;
    ImportSourceMetadata m_source;
  };

}
