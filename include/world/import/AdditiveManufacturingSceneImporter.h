#pragma once

#include "world/import/SceneImporter.h"

namespace world {

  /**
    * Imports small additive-manufacturing mesh delivery files as generated
    * scene geometry. STL triangles become a single mesh primitive; 3MF support
    * intentionally targets model geometry XML and uncompressed packages.
    */
  class AdditiveManufacturingSceneImporter : public SceneImporter {
  public:
    [[nodiscard]] QString name() const override;
    [[nodiscard]] QStringList supportedExtensions() const override;
    [[nodiscard]] ImportOptionSchemas optionSchema() const override;
    [[nodiscard]] ImportResult
    importFile(const QString& filename,
               const ImportOptions& options = ImportOptions()) const override;
  };

}
