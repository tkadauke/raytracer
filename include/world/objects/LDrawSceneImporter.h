#pragma once

#include "core/formats/ldraw/LDrawDiagnostic.h"
#include "world/objects/Element.h"

#include <memory>
#include <vector>

#include <QString>

/**
  * Imports LDraw authoring structure into the generic world scene graph.
  *
  * When hierarchy preservation is enabled, `0 STEP` sections and type-1
  * submodel references become ordinary `Group` / `Collection` nodes with
  * importer metadata. When disabled, import returns the existing flattened
  * `Group` metadata surface so callers can keep the historical render path.
  */
class LDrawSceneImporter {
public:
  struct Options {
    QString filePath;
    QString libraryPath;
    bool preserveHierarchy = true;
    bool smoothNormals = false;
    int recursionLimit = 64;
  };

  struct Result {
    std::unique_ptr<Element> root;
    std::vector<LDrawDiagnostic> diagnostics;
  };

  [[nodiscard]] Result importFile(const Options& options) const;
};
