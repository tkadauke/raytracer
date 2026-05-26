#pragma once

#include "world/import/SceneImporter.h"

namespace world {

  class StlSceneImporter : public SceneImporter {
  public:
    [[nodiscard]] QString name() const override;
    [[nodiscard]] QStringList supportedExtensions() const override;
    [[nodiscard]] ImportOptionSchemas optionSchema() const override;
    [[nodiscard]] ImportResult importFile(const QString& filename,
                                          const ImportOptions& options = ImportOptions())
      const override;
  };

}
