#pragma once

#include <vector>

#include <QString>

#include "core/formats/ldraw/LDrawDiagnostic.h"

class Element;
class Group;

namespace world::imports {
  struct LDrawImportOptions {
    enum class CoordinateConversion { None, LDrawToRaytracer };
    enum class MissingPartPolicy { Error, Skip };

    QString filePath;
    QString libraryPath;
    double scale = 1.0;
    CoordinateConversion coordinateConversion = CoordinateConversion::None;
    bool preserveHierarchy = true;
    bool smoothNormals = false;
    bool includeEdgeOverlays = true;
    int maxRecursion = 64;
    MissingPartPolicy missingPartPolicy = MissingPartPolicy::Error;
  };

  void attachLDrawImport(Group* group, const LDrawImportOptions& options,
                         std::vector<LDrawDiagnostic>* diagnostics = nullptr);

  void resolveLDrawAuthoringImports(Element* root, const QString& libraryRootOverride = QString(),
                                    const QString& sourceDirectory = QString(),
                                    std::vector<LDrawDiagnostic>* diagnostics = nullptr);
}
