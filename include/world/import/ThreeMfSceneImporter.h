#pragma once

#include "world/import/SceneImporter.h"

namespace world {

  /**
    * Imports 3MF core model packages as grouped mesh instances.
    *
    * The first pass supports OPC ZIP containers, the core model part,
    * object meshes, build items, 3MF units/transforms, and base material
    * display colors. Unsupported production extensions are ignored.
    */
  class ThreeMfSceneImporter : public SceneImporter {
  public:
    [[nodiscard]] QString name() const override;
    [[nodiscard]] QStringList supportedExtensions() const override;
    [[nodiscard]] ImportOptionSchemas optionSchema() const override;
    [[nodiscard]] ImportResult
    importFile(const QString& filename,
               const ImportOptions& options = ImportOptions()) const override;
  };

}
