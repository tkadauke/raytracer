#pragma once

#include "world/import/ImportedSceneDefaults.h"
#include "world/import/SceneImporter.h"

namespace world {

  /**
    * Imports glTF scenes and nodes as editable Group hierarchy.
    */
  class GltfSceneImporter : public SceneImporter {
  public:
    [[nodiscard]] QString name() const override;
    [[nodiscard]] QStringList supportedExtensions() const override;
    [[nodiscard]] ImportOptionSchemas optionSchema() const override;
    [[nodiscard]] ImportResult
    importFile(const QString& filename,
               const ImportOptions& options = ImportOptions()) const override;
    bool configureImportedRoot(Element& importedRoot, const ImportOptions& options) const override;
    bool configureImportedScene(Scene& scene, Element& importedRoot,
                                const ImportOptions& options) const override;

  private:
    [[nodiscard]] ImportedSceneDefaults importedSceneDefaults(const ImportOptions& options) const;
    void orientImportedRoot(Element& importedRoot) const;
  };

}
