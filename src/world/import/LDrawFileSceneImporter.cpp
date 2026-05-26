#include "world/import/LDrawFileSceneImporter.h"

#include "core/Exception.h"
#include "world/import/LDrawSceneImporter.h"
#include "world/import/SceneImporterRegistry.h"
#include "world/objects/DirectionalLight.h"
#include "world/objects/Group.h"
#include "world/objects/PinholeCamera.h"
#include "world/objects/Scene.h"

#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QRegularExpression>

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
       "none",
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
      {"preserve_hierarchy",
       ImportOptionType::Boolean,
       "Preserve hierarchy",
       "Keep LDraw subfile hierarchy in the compiled primitive where supported.",
       true,
       false,
       {}},
      {"background_color",
       ImportOptionType::String,
       "Background color",
       "Scene background as a CSS color name or hex color.",
       "white",
       false,
       {}},
      {"ambient_color",
       ImportOptionType::String,
       "Ambient color",
       "Scene ambient fill light as a CSS color name or hex color.",
       "#cccccc",
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
      if (!scene->frameActivePinholeCameraToContents(defaultCameraDirection())) {
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
      options.value("coordinate_conversion", "none").toString().trimmed().toLower();
    result.scale = options.value("scale", 1.0).toDouble();
    result.includeEdgeOverlays = options.value("include_edge_overlays", true).toBool();
    result.preserveHierarchy = options.value("preserve_hierarchy", true).toBool();
    result.maxRecursion = options.value("max_recursion", 64).toInt();
    result.backgroundColor = colorOption(options, "background_color", Colord::white());
    result.ambientColor = colorOption(options, "ambient_color", Colord(0.8, 0.8, 0.8));

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
    scene->setAmbient(options.ambientColor);
    scene->setBackground(options.backgroundColor);

    auto camera = std::make_unique<PinholeCamera>();
    camera->setId("camera");
    camera->setName("Camera");
    scene->addChild(std::move(camera));

    auto light = std::make_unique<DirectionalLight>();
    light->setId("light");
    light->setName("Light");
    light->setDirection(defaultLightDirection());
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
    metadata["preserveHierarchy"] = options.preserveHierarchy;
    metadata["includeEdgeOverlays"] = options.includeEdgeOverlays;
    metadata["maxRecursion"] = options.maxRecursion;
    metadata["missingPartPolicy"] = options.missingPartPolicy;
    if (!options.libraryRoot.isEmpty())
      metadata["libraryRoot"] = options.libraryRoot;
    model->setMetadata(metadata);
    scene->addChild(std::move(model));

    return scene;
  }

  Colord LDrawFileSceneImporter::colorOption(const ImportOptions& options, const QString& name,
                                             const Colord& fallback) const {
    const QVariant value = options.value(name);
    if (!value.isValid())
      return fallback;
    return parseColor(value.toString(), name);
  }

  Colord LDrawFileSceneImporter::parseColor(const QString& value, const QString& optionName) const {
    QString text = value.trimmed();
    if (text.isEmpty()) {
      throw std::invalid_argument(
        QString("LDraw %1 must be a color name or hex color").arg(optionName).toStdString());
    }

    if (text.startsWith("0x", Qt::CaseInsensitive))
      text = "#" + text.mid(2);

    static const QRegularExpression bareHex("^[0-9a-fA-F]{6}([0-9a-fA-F]{2})?$");
    if (bareHex.match(text).hasMatch())
      text = "#" + text;

    const QColor color(text);
    if (!color.isValid()) {
      throw std::invalid_argument(
        QString("LDraw %1 must be a color name or hex color").arg(optionName).toStdString());
    }

    return Colord(color.redF(), color.greenF(), color.blueF());
  }

  Vector3d LDrawFileSceneImporter::defaultCameraDirection() const {
    return Vector3d(0.0, 0.0, -1.0);
  }

  Vector3d LDrawFileSceneImporter::defaultLightDirection() const {
    return Vector3d(-0.35, 0.7, -1.0);
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
