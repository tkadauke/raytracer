#pragma once

#include "world/import/SceneImporter.h"

namespace world {

  /**
    * Imports a deliberately small native OpenSCAD subset into editable world
    * primitives and CSG nodes. Supported constructs are translate, rotate,
    * scale, union, difference, intersection, cube, sphere, and cylinder.
    */
  class OpenScadSceneImporter : public SceneImporter {
  public:
    [[nodiscard]] QString name() const override;
    [[nodiscard]] QStringList supportedExtensions() const override;
    [[nodiscard]] ImportOptionSchemas optionSchema() const override;
    [[nodiscard]] ImportResult
    importFile(const QString& filename,
               const ImportOptions& options = ImportOptions()) const override;
  };

}
