#include "world/import/LDrawFileSceneImporter.h"

#include "core/Exception.h"
#include "world/import/LDrawSceneImporter.h"
#include "world/import/SceneImporterRegistry.h"
#include "world/objects/DirectionalLight.h"
#include "world/objects/Group.h"
#include "world/objects/PinholeCamera.h"
#include "world/objects/Scene.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonObject>

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <stdexcept>

namespace world {

  QString LDrawFileSceneImporter::name() const {
    return "ldraw";
  }

  QStringList LDrawFileSceneImporter::supportedExtensions() const {
    return {"ldr", "dat", "mpd"};
  }

  ImportOptionSchemas LDrawFileSceneImporter::optionSchema() const {
    return {
      {"library_root",
       ImportOptionType::DirectoryPath,
       "LDraw library root",
       "Directory containing parts/, p/, and LDConfig.ldr.",
       defaultLibraryRoot(),
       false,
       {}},
      {"normal_mode",
       ImportOptionType::Choice,
       "Normal mode",
       "Imported polygon normal mode.",
       "flat",
       false,
       {"flat", "smooth"}},
      {"missing_part_policy",
       ImportOptionType::Choice,
       "Missing part policy",
       "How unresolved subfiles are handled.",
       "skip",
       false,
       {"error", "skip"}},
      {"coordinate_conversion",
       ImportOptionType::Choice,
       "Coordinate conversion",
       "Coordinate conversion applied before rendering.",
       "ldraw_to_raytracer",
       false,
       {"ldraw_to_raytracer", "none"}},
      {"scale",
       ImportOptionType::Double,
       "Scale",
       "Scale applied to imported LDraw units.",
       1.0,
       false,
       {}},
      {"include_edge_overlays",
       ImportOptionType::Boolean,
       "Include edge overlays",
       "Import LDraw type-2 edge/detail lines as curve overlays.",
       true,
       false,
       {}},
    };
  }

  ImportResult LDrawFileSceneImporter::importFile(const QString& filename,
                                                  const ImportOptions& options) const {
    const ImportSourceMetadata source = sourceMetadataFor(filename);
    std::vector<LDrawDiagnostic> ldrawDiagnostics;

    try {
      const ResolvedOptions resolved = resolvedOptions(filename, options);
      auto scene = makeScene(resolved);
      world::imports::resolveLDrawAuthoringImports(
        scene.get(), resolved.libraryRoot, QFileInfo(filename).absolutePath(), &ldrawDiagnostics);
      if (!scene->frameActivePinholeCameraToContents(Vector3d(0.75, 0.45, -1.0))) {
        ldrawDiagnostics.push_back({LDrawDiagnosticSeverity::Warning,
                                    LDrawDiagnosticCode::SkippedGeometry,
                                    filename.toStdString(),
                                    -1,
                                    "imported model bounds did not produce a camera frame",
                                    {},
                                    {}});
      }
      scene->setImportDiagnostics(ldrawDiagnostics);

      ImportResult result(std::move(scene), source);
      appendDiagnostics(result, ldrawDiagnostics);
      return result;
    } catch (const Exception& error) {
      auto result = ImportResult::failed(
        {ImportDiagnostic::error(QString::fromStdString(error.message()), filename)}, source);
      appendDiagnostics(result, ldrawDiagnostics);
      return result;
    } catch (const std::exception& error) {
      auto result = ImportResult::failed(
        {ImportDiagnostic::error(QString::fromLocal8Bit(error.what()), filename)}, source);
      appendDiagnostics(result, ldrawDiagnostics);
      return result;
    }
  }

  LDrawFileSceneImporter::ResolvedOptions
  LDrawFileSceneImporter::resolvedOptions(const QString& filename,
                                          const ImportOptions& options) const {
    ResolvedOptions result;
    result.filePath = QFileInfo(filename).absoluteFilePath();
    result.libraryRoot = options.value("library_root", defaultLibraryRoot()).toString();
    if (!result.libraryRoot.isEmpty())
      result.libraryRoot = QFileInfo(result.libraryRoot).absoluteFilePath();
    result.normalMode = options.value("normal_mode", "flat").toString().trimmed().toLower();
    result.missingPartPolicy =
      options.value("missing_part_policy", "skip").toString().trimmed().toLower();
    result.coordinateConversion =
      options.value("coordinate_conversion", "ldraw_to_raytracer").toString().trimmed().toLower();
    result.scale = options.value("scale", 1.0).toDouble();
    result.includeEdgeOverlays = options.value("include_edge_overlays", true).toBool();
    result.maxRecursion = options.value("max_recursion", 64).toInt();

    if (result.normalMode != "flat" && result.normalMode != "smooth")
      throw std::invalid_argument("LDraw normal_mode must be 'flat' or 'smooth'");
    if (result.missingPartPolicy != "error" && result.missingPartPolicy != "skip")
      throw std::invalid_argument("LDraw missing_part_policy must be 'error' or 'skip'");
    if (result.coordinateConversion != "ldraw_to_raytracer" &&
        result.coordinateConversion != "none")
      throw std::invalid_argument(
        "LDraw coordinate_conversion must be 'ldraw_to_raytracer' or 'none'");
    if (result.scale <= 0.0)
      throw std::invalid_argument("LDraw scale must be a positive number");
    if (result.maxRecursion <= 0)
      throw std::invalid_argument("LDraw max_recursion must be positive");

    return result;
  }

  QString LDrawFileSceneImporter::defaultLibraryRoot() const {
    if (const char* env = std::getenv("LDRAWDIR")) {
      if (*env)
        return QString::fromLocal8Bit(env);
    }

    const QString documentsRoot = QDir::home().filePath("Documents/ldraw");
    if (QFileInfo::exists(documentsRoot))
      return documentsRoot;

    return QString();
  }

  std::unique_ptr<Scene> LDrawFileSceneImporter::makeScene(const ResolvedOptions& options) const {
    const QFileInfo fileInfo(options.filePath);
    auto scene = std::make_unique<Scene>();
    scene->setName(fileInfo.completeBaseName().isEmpty() ? QString("LDraw Import")
                                                         : fileInfo.completeBaseName());
    scene->setBackground(Colord(0.02, 0.04, 0.08));

    auto camera = std::make_unique<PinholeCamera>();
    camera->setId("camera");
    camera->setName("Camera");
    scene->addChild(std::move(camera));

    auto light = std::make_unique<DirectionalLight>();
    light->setId("light");
    light->setName("Light");
    light->setDirection(Vector3d(-0.5, -1.0, -0.5));
    scene->addChild(std::move(light));

    auto model = std::make_unique<Group>();
    model->setId("ldraw-model");
    model->setName(fileInfo.completeBaseName().isEmpty() ? QString("LDraw Model")
                                                         : fileInfo.completeBaseName());
    QJsonObject metadata;
    metadata["sourceFormat"] = "LDraw";
    metadata["sourcePath"] = options.filePath;
    metadata["normalMode"] = options.normalMode;
    metadata["scale"] = options.scale;
    metadata["coordinateConversion"] = options.coordinateConversion;
    metadata["preserveHierarchy"] = true;
    metadata["includeEdgeOverlays"] = options.includeEdgeOverlays;
    metadata["maxRecursion"] = options.maxRecursion;
    metadata["missingPartPolicy"] = options.missingPartPolicy;
    if (!options.libraryRoot.isEmpty())
      metadata["libraryRoot"] = options.libraryRoot;
    model->setMetadata(metadata);
    scene->addChild(std::move(model));

    return scene;
  }

  ImportSourceMetadata LDrawFileSceneImporter::sourceMetadataFor(const QString& filename) const {
    ImportSourceMetadata source;
    source.importerName = name();
    source.formatName = "LDraw";
    source.sourcePath = filename;
    source.properties = {{"extension", QFileInfo(filename).suffix().toLower()}};
    return source;
  }

  ImportDiagnostic
  LDrawFileSceneImporter::importDiagnosticFor(const LDrawDiagnostic& diagnostic) const {
    const QString message = QString::fromStdString(diagnostic.message);
    const QString source = QString::fromStdString(diagnostic.file);
    const int line = diagnostic.lineNumber > 0 ? diagnostic.lineNumber : -1;
    if (diagnostic.severity == LDrawDiagnosticSeverity::Error)
      return ImportDiagnostic::error(message, source, line);
    return ImportDiagnostic::warning(message, source, line);
  }

  void
  LDrawFileSceneImporter::appendDiagnostics(ImportResult& result,
                                            const std::vector<LDrawDiagnostic>& diagnostics) const {
    for (const auto& diagnostic : diagnostics) {
      result.addDiagnostic(importDiagnosticFor(diagnostic));
    }
  }

}

static bool dummy =
  world::SceneImporterRegistry::self().registerClass<world::LDrawFileSceneImporter>("ldraw");
