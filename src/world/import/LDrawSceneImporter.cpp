#include "world/import/LDrawSceneImporter.h"

#include "core/Exception.h"
#include "core/formats/ldraw/LDrawColorTable.h"
#include "core/formats/ldraw/LDrawFileResolver.h"
#include "core/formats/ldraw/LDrawGeometryCompiler.h"
#include "core/math/Matrix.h"
#include "render/primitives/Composite.h"
#include "render/primitives/Instance.h"
#include "world/objects/CompiledPrimitive.h"
#include "world/objects/Element.h"
#include "world/objects/Group.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonValue>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <vector>

namespace {
  QString resolvedPath(const QString& path, const QString& sourceDirectory) {
    if (path.isEmpty())
      return path;

    QFileInfo info(path);
    if (info.isAbsolute() || sourceDirectory.isEmpty())
      return path;

    return QDir(sourceDirectory).filePath(path);
  }

  std::vector<std::string> searchDirectoriesFor(const QString& modelFilePath,
                                                const QString& libraryPath) {
    namespace fs = std::filesystem;

    std::vector<std::string> directories;
    const fs::path modelPath(modelFilePath.toStdString());
    if (!modelPath.parent_path().empty()) {
      directories.push_back(modelPath.parent_path().string());
    }

    if (!libraryPath.isEmpty()) {
      const fs::path root(libraryPath.toStdString());
      directories.push_back((root / "parts").string());
      directories.push_back((root / "parts" / "s").string());
      directories.push_back((root / "p").string());
      directories.push_back((root / "p" / "48").string());
      directories.push_back((root / "models").string());
    }

    return directories;
  }

  bool isLDrawImportDirective(const QJsonObject& metadata) {
    if (metadata.value("sourceFormat").toString().compare("LDraw", Qt::CaseInsensitive) != 0)
      return false;

    const QJsonValue sourcePath = metadata.value("sourcePath");
    if (sourcePath.isString() && !sourcePath.toString().trimmed().isEmpty())
      return true;

    const QJsonValue filePath = metadata.value("filePath");
    return filePath.isString() && !filePath.toString().trimmed().isEmpty();
  }

  bool boolValue(const QJsonObject& metadata, const QString& key, bool fallback) {
    const QJsonValue value = metadata.value(key);
    return value.isBool() ? value.toBool() : fallback;
  }

  int intValue(const QJsonObject& metadata, const QString& key, int fallback) {
    const QJsonValue value = metadata.value(key);
    if (!value.isDouble())
      return fallback;
    const double number = value.toDouble();
    if (!std::isfinite(number) || std::floor(number) != number)
      return fallback;
    return static_cast<int>(number);
  }

  double doubleValue(const QJsonObject& metadata, const QString& key, double fallback) {
    const QJsonValue value = metadata.value(key);
    return value.isDouble() && std::isfinite(value.toDouble()) ? value.toDouble() : fallback;
  }

  QString stringValue(const QJsonObject& metadata, const QString& primary,
                      const QString& secondary = QString()) {
    const QJsonValue primaryValue = metadata.value(primary);
    if (primaryValue.isString())
      return primaryValue.toString();
    if (!secondary.isEmpty()) {
      const QJsonValue secondaryValue = metadata.value(secondary);
      if (secondaryValue.isString())
        return secondaryValue.toString();
    }
    return QString();
  }

  world::imports::LDrawImportOptions::CoordinateConversion
  coordinateConversionFromString(QString value) {
    value = value.trimmed().toLower();
    value.remove('_');
    value.remove('-');
    if (value == "ldrawtoraytracer" || value == "raytracer" || value == "yup")
      return world::imports::LDrawImportOptions::CoordinateConversion::LDrawToRaytracer;
    return world::imports::LDrawImportOptions::CoordinateConversion::None;
  }

  world::imports::LDrawImportOptions::MissingPartPolicy missingPartPolicyFromString(QString value) {
    value = value.trimmed().toLower();
    if (value == "skip" || value == "ignore")
      return world::imports::LDrawImportOptions::MissingPartPolicy::Skip;
    return world::imports::LDrawImportOptions::MissingPartPolicy::Error;
  }

  Matrix4d importTransformFor(const world::imports::LDrawImportOptions& options) {
    Matrix4d transform;
    if (options.coordinateConversion ==
        world::imports::LDrawImportOptions::CoordinateConversion::LDrawToRaytracer) {
      transform.setCell(0, 0, options.scale);
      transform.setCell(1, 1, 0.0);
      transform.setCell(1, 2, -options.scale);
      transform.setCell(2, 1, options.scale);
      transform.setCell(2, 2, 0.0);
    } else {
      transform = Matrix4d(Matrix3d::scale(options.scale, options.scale, options.scale));
    }
    return transform;
  }

  world::imports::LDrawImportOptions optionsFromMetadata(const QJsonObject& metadata,
                                                         const QString& libraryRootOverride,
                                                         const QString& sourceDirectory) {
    world::imports::LDrawImportOptions options;
    options.filePath =
      resolvedPath(stringValue(metadata, "sourcePath", "filePath"), sourceDirectory);

    const QString libraryPath = libraryRootOverride.isEmpty()
                                  ? stringValue(metadata, "libraryRoot", "libraryPath")
                                  : libraryRootOverride;
    options.libraryPath = resolvedPath(libraryPath, sourceDirectory);
    options.scale = doubleValue(metadata, "scale", options.scale);
    if (options.scale <= 0.0)
      options.scale = 1.0;
    options.coordinateConversion =
      coordinateConversionFromString(stringValue(metadata, "coordinateConversion"));
    options.preserveHierarchy = boolValue(metadata, "preserveHierarchy", options.preserveHierarchy);

    const QString normalMode = stringValue(metadata, "normalMode");
    options.smoothNormals = normalMode.compare("smooth", Qt::CaseInsensitive) == 0 ||
                            metadata.value("smoothNormals").toBool(false);
    options.includeEdgeOverlays =
      boolValue(metadata, "includeEdgeOverlays", options.includeEdgeOverlays);
    options.maxRecursion = intValue(metadata, "maxRecursion", options.maxRecursion);
    if (options.maxRecursion <= 0)
      options.maxRecursion = 64;
    options.missingPartPolicy =
      missingPartPolicyFromString(stringValue(metadata, "missingPartPolicy"));
    return options;
  }

  std::shared_ptr<render::Primitive> compileLDraw(const world::imports::LDrawImportOptions& options,
                                                  std::vector<LDrawDiagnostic>* diagnostics) {
    if (options.filePath.isEmpty()) {
      throw Exception("LDraw import sourcePath must not be empty", __FILE__, __LINE__);
    }

    std::ifstream input(options.filePath.toStdString());
    if (!input) {
      std::ostringstream message;
      message << "Unable to read LDraw model '" << options.filePath.toStdString() << "'";
      throw Exception(message.str(), __FILE__, __LINE__);
    }

    LDrawColorTable colors;
    colors.loadLibraryConfig(options.libraryPath.toStdString());
    auto resolver = std::make_shared<LDrawFilesystemResolver>(
      searchDirectoriesFor(options.filePath, options.libraryPath));
    const auto normalMode = options.smoothNormals ? LDrawGeometryCompiler::NormalMode::Smooth
                                                  : LDrawGeometryCompiler::NormalMode::Flat;
    LDrawGeometryCompiler::Options compilerOptions;
    compilerOptions.recursionLimit = options.maxRecursion;
    compilerOptions.normalMode = normalMode;
    compilerOptions.includeEdgeOverlays = options.includeEdgeOverlays;
    compilerOptions.preserveHierarchy = options.preserveHierarchy;
    compilerOptions.missingPartPolicy =
      options.missingPartPolicy == world::imports::LDrawImportOptions::MissingPartPolicy::Skip
        ? LDrawGeometryCompiler::MissingPartPolicy::Skip
        : LDrawGeometryCompiler::MissingPartPolicy::Error;
    LDrawGeometryCompiler compiler(resolver, compilerOptions);

    std::shared_ptr<render::Primitive> primitive;
    if (diagnostics) {
      LDrawDiagnostics localDiagnostics;
      primitive = compiler.compile(input, colors, localDiagnostics);
      diagnostics->insert(diagnostics->end(), localDiagnostics.entries().begin(),
                          localDiagnostics.entries().end());
    } else {
      primitive = compiler.compile(input, colors);
    }

    if (options.scale == 1.0 && options.coordinateConversion ==
                                  world::imports::LDrawImportOptions::CoordinateConversion::None) {
      return primitive;
    }

    auto transformed = std::make_shared<render::Instance>(primitive);
    transformed->setMatrix(importTransformFor(options));
    return transformed;
  }

  void removeGeneratedChildren(Element* element) {
    const auto children = element->childElements();
    for (Element* child : children) {
      if (child->isGenerated()) {
        element->removeChild(child);
        delete child;
      }
    }
  }
}

namespace world::imports {
  void attachLDrawImport(Group* group, const LDrawImportOptions& options,
                         std::vector<LDrawDiagnostic>* diagnostics) {
    removeGeneratedChildren(group);

    auto compiled = std::make_unique<CompiledPrimitive>(compileLDraw(options, diagnostics));
    compiled->setId(group->id() + ":compiled-geometry");
    compiled->setName(group->name().isEmpty() ? QString("Imported Geometry")
                                              : group->name() + " Geometry");
    group->addChild(std::move(compiled));
  }

  void resolveLDrawAuthoringImports(Element* root, const QString& libraryRootOverride,
                                    const QString& sourceDirectory,
                                    std::vector<LDrawDiagnostic>* diagnostics) {
    if (auto* group = qobject_cast<Group*>(root)) {
      const QJsonObject metadata = group->metadata();
      if (isLDrawImportDirective(metadata)) {
        attachLDrawImport(
          group, optionsFromMetadata(metadata, libraryRootOverride, sourceDirectory), diagnostics);
      }
    }

    for (Element* child : root->childElements()) {
      resolveLDrawAuthoringImports(child, libraryRootOverride, sourceDirectory, diagnostics);
    }
  }
}
