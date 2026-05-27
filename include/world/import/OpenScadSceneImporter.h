#pragma once

#include "world/import/ImportedSceneDefaults.h"
#include "world/import/SceneImporter.h"

namespace world {

  /**
    * Imports a deliberately small native OpenSCAD subset into editable world
    * primitives and CSG nodes by default, or compiles through OpenSCAD when
    * compile import options are supplied. Supported native constructs are
    * translate, rotate, scale, union, difference, intersection, cube, sphere,
    * and cylinder.
    */
  class OpenScadSceneImporter : public SceneImporter {
  public:
    [[nodiscard]] QString name() const override;
    [[nodiscard]] QStringList supportedExtensions() const override;
    [[nodiscard]] ImportOptionSchemas optionSchema() const override;
    [[nodiscard]] ImportOptionSchemas
    editableSourceParameters(const QString& filename, const ImportOptions& options) const override;
    [[nodiscard]] ImportResult
    importFile(const QString& filename,
               const ImportOptions& options = ImportOptions()) const override;
    [[nodiscard]] bool wrapDirectImportInSourceAsset() const override;
    bool configureImportedScene(Scene& scene, Element& importedRoot,
                                const ImportOptions& options) const override;

  private:
    [[nodiscard]] ImportedSceneDefaults importedSceneDefaults(const ImportOptions& options) const;
    void orientImportedRoot(Element& importedRoot) const;
  };

}
