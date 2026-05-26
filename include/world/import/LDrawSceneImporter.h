#pragma once

#include <vector>

#include <QString>

#include "core/formats/ldraw/LDrawDiagnostic.h"

class Element;
class Group;

namespace world::imports {
  struct LDrawImportOptions {
    QString filePath;
    QString libraryPath;
    bool smoothNormals = false;
  };

  void attachLDrawImport(Group* group, const LDrawImportOptions& options,
                         std::vector<LDrawDiagnostic>* diagnostics = nullptr);

  void resolveLDrawAuthoringImports(Element* root, const QString& libraryRootOverride = QString(),
                                    const QString& sourceDirectory = QString(),
                                    std::vector<LDrawDiagnostic>* diagnostics = nullptr);
}
