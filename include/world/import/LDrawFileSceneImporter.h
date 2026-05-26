#pragma once

#include "core/formats/ldraw/LDrawDiagnostic.h"
#include "world/import/SceneImporter.h"

#include <memory>
#include <vector>

class Scene;

namespace world {

  /**
    * Imports an LDraw `.ldr`, `.dat`, or `.mpd` file as a complete world scene.
    *
    * The importer builds an editable scene shell, resolves the LDraw source
    * into generated primitive geometry, and frames the active pinhole camera so
    * direct Modeler/importer use starts from a useful view.
    */
  class LDrawFileSceneImporter : public SceneImporter {
  public:
    [[nodiscard]] QString name() const override;
    [[nodiscard]] QStringList supportedExtensions() const override;
    [[nodiscard]] ImportOptionSchemas optionSchema() const override;
    [[nodiscard]] ImportResult
    importFile(const QString& filename,
               const ImportOptions& options = ImportOptions()) const override;

  private:
    struct ResolvedOptions {
      QString filePath;
      QString libraryRoot;
      QString normalMode;
      double scale{1.0};
      QString coordinateConversion;
      bool includeEdgeOverlays{true};
      int maxRecursion{64};
      QString missingPartPolicy;
    };

    [[nodiscard]] ResolvedOptions resolvedOptions(const QString& filename,
                                                  const ImportOptions& options) const;
    [[nodiscard]] QString defaultLibraryRoot() const;
    [[nodiscard]] std::unique_ptr<Scene> makeScene(const ResolvedOptions& options) const;
    [[nodiscard]] ImportSourceMetadata sourceMetadataFor(const QString& filename) const;
    [[nodiscard]] ImportDiagnostic importDiagnosticFor(const LDrawDiagnostic& diagnostic) const;
    void appendDiagnostics(ImportResult& result,
                           const std::vector<LDrawDiagnostic>& diagnostics) const;
  };

}
