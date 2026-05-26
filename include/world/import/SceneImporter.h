#pragma once

#include "world/import/ImportOptions.h"
#include "world/import/ImportResult.h"

#include <QString>
#include <QStringList>

namespace world {

  /**
    * Shared interface for file-format importers that produce editable world
    * scene roots.
    */
  class SceneImporter {
  public:
    virtual ~SceneImporter();

    [[nodiscard]] virtual QString name() const = 0;
    /**
      * @returns lowercase filename extensions without a leading dot.
      */
    [[nodiscard]] virtual QStringList supportedExtensions() const = 0;
    [[nodiscard]] virtual ImportOptionSchemas optionSchema() const = 0;
    [[nodiscard]] virtual ImportResult importFile(const QString& filename,
                                                  const ImportOptions& options = ImportOptions())
      const = 0;
  };

}
