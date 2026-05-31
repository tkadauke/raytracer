#include "RenderCliSceneLoader.h"

#include "world/import/LDrawFileSceneImporter.h"
#include "world/import/SceneImporterRegistry.h"
#include "world/import/LDrawSceneImporter.h"
#include "world/import/ImportResult.h"
#include "world/objects/DirectionalLight.h"
#include "world/objects/Element.h"
#include "world/objects/LDrawSceneImporter.h"
#include "world/objects/PinholeCamera.h"
#include "world/objects/Scene.h"

#include <QFileInfo>

#include <iostream>
#include <map>
#include <stdexcept>
#include <utility>

RenderCliSceneLoader::RenderCliSceneLoader(RenderCliSceneLoadOptions options)
    : m_options(std::move(options)) {
}

void RenderCliSceneLoader::printImportDiagnostics(const world::ImportResult& result) const {
  struct WarningSummary {
    world::ImportDiagnostic first;
    int count = 0;
  };

  std::map<QString, WarningSummary> warnings;
  for (const auto& diagnostic : result.diagnostics()) {
    if (!diagnostic.isError()) {
      auto& summary = warnings[diagnostic.message];
      if (summary.count == 0)
        summary.first = diagnostic;
      ++summary.count;
      continue;
    }

    std::cerr << "import error";
    if (!diagnostic.source.isEmpty()) {
      std::cerr << " " << diagnostic.source.toStdString();
      if (diagnostic.line > 0) {
        std::cerr << ":" << diagnostic.line;
        if (diagnostic.column > 0) {
          std::cerr << ":" << diagnostic.column;
        }
      }
    }
    std::cerr << ": " << diagnostic.message.toStdString() << '\n';
  }

  for (const auto& entry : warnings) {
    const auto& summary = entry.second;
    const auto& diagnostic = summary.first;
    std::cerr << "import warning";
    if (!diagnostic.source.isEmpty()) {
      std::cerr << " " << diagnostic.source.toStdString();
      if (diagnostic.line > 0) {
        std::cerr << ":" << diagnostic.line;
        if (diagnostic.column > 0) {
          std::cerr << ":" << diagnostic.column;
        }
      }
    }
    std::cerr << ": " << diagnostic.message.toStdString();
    if (summary.count > 1)
      std::cerr << " (" << (summary.count - 1) << " similar warnings suppressed)";
    std::cerr << '\n';
  }
}

void RenderCliSceneLoader::printLDrawDiagnostics(
  const std::vector<LDrawDiagnostic>& diagnostics) const {
  struct WarningSummary {
    LDrawDiagnostic first;
    int count = 0;
  };

  std::map<std::pair<LDrawDiagnosticCode, std::string>, WarningSummary> warnings;
  for (const auto& diagnostic : diagnostics) {
    if (diagnostic.severity == LDrawDiagnosticSeverity::Error) {
      std::cerr << diagnostic.toString() << '\n';
      continue;
    }

    const auto key = std::make_pair(diagnostic.code, diagnostic.message);
    auto& summary = warnings[key];
    if (summary.count == 0)
      summary.first = diagnostic;
    ++summary.count;
  }

  for (const auto& entry : warnings) {
    const auto& summary = entry.second;
    std::cerr << summary.first.toString();
    if (summary.count > 1)
      std::cerr << " (" << (summary.count - 1) << " similar warnings suppressed)";
    std::cerr << '\n';
  }
}

std::unique_ptr<Scene> RenderCliSceneLoader::load() const {
  if (m_options.ldrawInput) {
    return loadLDrawScene();
  }

  std::unique_ptr<world::SceneImporter> importer;
  if (!m_options.importFormat.isEmpty()) {
    importer = world::SceneImporterRegistry::self().createByFormat(m_options.importFormat);
    if (!importer) {
      throw std::runtime_error(QString("No scene importer registered for format: %1")
                                 .arg(m_options.importFormat)
                                 .toStdString());
    }
  } else if (world::SceneImporterRegistry::self().hasExtension(
               QFileInfo(m_options.filename).suffix())) {
    importer = world::SceneImporterRegistry::self().createForFile(m_options.filename);
  }

  if (importer) {
    const world::ImportOptions importOptions =
      importer->name() == "ldraw" ? ldrawImportOptions() : m_options.importOptions;
    return loadImportedScene(*importer, importOptions, "Unable to import input scene");
  }

  auto scene = std::make_unique<Scene>(nullptr);
  if (!scene->load(m_options.filename, m_options.ldrawLibraryRoot))
    throw std::runtime_error(
      QString("Unable to load input scene: %1").arg(m_options.filename).toStdString());
  printLDrawDiagnostics(scene->importDiagnostics());
  return scene;
}

std::unique_ptr<Scene>
RenderCliSceneLoader::loadImportedScene(const world::SceneImporter& importer,
                                        const world::ImportOptions& importOptions,
                                        const QString& failureMessage) const {
  world::ImportResult result = importer.importFile(m_options.filename, importOptions);
  printImportDiagnostics(result);
  if (result.failed()) {
    throw std::runtime_error(
      QString("%1: %2").arg(failureMessage, m_options.filename).toStdString());
  }

  auto root = result.takeRoot();
  if (auto* sceneRoot = qobject_cast<Scene*>(root.get())) {
    root.release();
    return std::unique_ptr<Scene>(sceneRoot);
  }

  auto scene = std::make_unique<Scene>(nullptr);
  Element* importedRoot = root.get();
  scene->addChild(std::move(root));
  if (importedRoot && importer.configureImportedScene(*scene, *importedRoot, importOptions)) {
    scene->resolveElementReferences();
    return scene;
  }

  throw std::runtime_error(
    QString("Importer did not return a scene root: %1").arg(m_options.filename).toStdString());
}

world::ImportOptions RenderCliSceneLoader::ldrawImportOptions() const {
  world::ImportOptions options = m_options.importOptions;
  if (!m_options.ldrawLibraryRoot.isEmpty() && !options.contains("library_root")) {
    options.setValue("library_root", m_options.ldrawLibraryRoot);
  }
  options.setValue("scale", m_options.ldrawScale);
  options.setValue("coordinate_conversion", m_options.ldrawCoordinateConversion);
  options.setValue("preserve_hierarchy", m_options.ldrawPreserveHierarchy);
  options.setValue("normal_mode", m_options.ldrawNormalMode);
  options.setValue("include_edge_overlays", m_options.ldrawIncludeEdgeOverlays);
  options.setValue("max_recursion", m_options.ldrawMaxRecursion);
  options.setValue("missing_part_policy", m_options.ldrawMissingPartPolicy);
  if (!m_options.ldrawBackgroundColor.isEmpty()) {
    options.setValue("background_color", m_options.ldrawBackgroundColor);
  }
  return options;
}

std::unique_ptr<Scene> RenderCliSceneLoader::loadLDrawScene() const {
  if (m_options.ldrawPreserveAuthoringHierarchy) {
    return loadLDrawAuthoringScene();
  }

  world::LDrawFileSceneImporter importer;
  return loadImportedScene(importer, ldrawImportOptions(), "Unable to import LDraw input");
}

std::unique_ptr<Scene> RenderCliSceneLoader::loadLDrawAuthoringScene() const {
  auto scene = std::make_unique<Scene>(nullptr);
  scene->setName("LDraw Import");
  scene->setAmbient(Colord(0.8, 0.8, 0.8));
  scene->setBackground(Colord::white());

  auto camera = std::make_unique<PinholeCamera>();
  camera->setId("camera");
  camera->setName("Camera");
  scene->addChild(std::move(camera));

  auto light = std::make_unique<DirectionalLight>();
  light->setId("light");
  light->setName("Light");
  light->setDirection(Vector3d(-0.35, 0.7, -1.0));
  scene->addChild(std::move(light));

  std::vector<LDrawDiagnostic> diagnostics;
  LDrawSceneImporter importer;
  LDrawSceneImporter::Options options;
  options.filePath = m_options.filename;
  options.libraryPath = m_options.ldrawLibraryRoot;
  options.preserveHierarchy = true;
  options.smoothNormals = m_options.ldrawNormalMode == "smooth";
  options.recursionLimit = m_options.ldrawMaxRecursion;
  auto result = importer.importFile(options);
  result.root->setId("ldraw-model");
  result.root->setName("LDraw Model");
  diagnostics = std::move(result.diagnostics);
  scene->addChild(std::move(result.root));

  world::imports::resolveLDrawAuthoringImports(scene.get(), m_options.ldrawLibraryRoot, QString(),
                                               &diagnostics);
  scene->setImportDiagnostics(std::move(diagnostics));
  printLDrawDiagnostics(scene->importDiagnostics());

  if (!scene->frameActivePinholeCameraToContents(m_options.stepPlaybackStyle,
                                                 Vector3d(0.0, 0.0, -1.0))) {
    std::cerr << "LDraw warning: imported model bounds did not produce a camera frame\n";
  }

  return scene;
}
