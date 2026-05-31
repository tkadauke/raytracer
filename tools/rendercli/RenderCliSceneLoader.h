#pragma once

#include "world/import/ImportOptions.h"
#include "world/objects/StepVisibilityEvaluator.h"
#include "core/formats/ldraw/LDrawDiagnostic.h"

#include <memory>
#include <vector>

#include <QString>

class Scene;

namespace world {
  class ImportResult;
  class SceneImporter;
}

struct RenderCliSceneLoadOptions {
  QString filename;
  QString importFormat;
  world::ImportOptions importOptions;
  QString ldrawLibraryRoot;
  bool ldrawInput{false};
  bool ldrawPreserveAuthoringHierarchy{false};
  double ldrawScale{1.0};
  QString ldrawCoordinateConversion{"none"};
  bool ldrawPreserveHierarchy{true};
  QString ldrawNormalMode{"flat"};
  bool ldrawIncludeEdgeOverlays{true};
  int ldrawMaxRecursion{64};
  QString ldrawMissingPartPolicy{"skip"};
  QString ldrawBackgroundColor;
  StepPlaybackStyle stepPlaybackStyle;
};

class RenderCliSceneLoader {
public:
  explicit RenderCliSceneLoader(RenderCliSceneLoadOptions options);

  [[nodiscard]] std::unique_ptr<Scene> load() const;

private:
  [[nodiscard]] std::unique_ptr<Scene> loadImportedScene(const world::SceneImporter& importer,
                                                         const world::ImportOptions& importOptions,
                                                         const QString& failureMessage) const;
  [[nodiscard]] std::unique_ptr<Scene> loadLDrawScene() const;
  [[nodiscard]] std::unique_ptr<Scene> loadLDrawAuthoringScene() const;
  [[nodiscard]] world::ImportOptions ldrawImportOptions() const;
  void printImportDiagnostics(const world::ImportResult& result) const;
  void printLDrawDiagnostics(const std::vector<LDrawDiagnostic>& diagnostics) const;

  RenderCliSceneLoadOptions m_options;
};
