#pragma once

#include "world/import/SceneImporter.h"

namespace world {

  /**
    * Imports the project's native scene JSON through the generic importer path.
    *
    * The `json` format is intentionally registered for the importer fixture
    * extension `rtjson`, leaving ordinary `.json` scene files on the historic
    * direct `Scene::load` path unless callers explicitly request `json`.
    */
  class JsonSceneImporter : public SceneImporter {
  public:
    [[nodiscard]] QString name() const override;
    [[nodiscard]] QStringList supportedExtensions() const override;
    [[nodiscard]] ImportOptionSchemas optionSchema() const override;
    [[nodiscard]] ImportResult importFile(const QString& filename,
                                          const ImportOptions& options = ImportOptions())
      const override;
  };

}
