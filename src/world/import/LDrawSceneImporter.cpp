#include "world/import/LDrawSceneImporter.h"

#include "core/Exception.h"
#include "core/formats/ldraw/LDrawColorTable.h"
#include "core/formats/ldraw/LDrawFileResolver.h"
#include "core/formats/ldraw/LDrawGeometryCompiler.h"
#include "render/primitives/Composite.h"
#include "world/objects/CompiledPrimitive.h"
#include "world/objects/Element.h"
#include "world/objects/Group.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonValue>

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

  bool isLDrawMetadata(const QJsonObject& metadata) {
    return metadata.value("sourceFormat").toString().compare("LDraw", Qt::CaseInsensitive) == 0;
  }

  world::imports::LDrawImportOptions optionsFromMetadata(const QJsonObject& metadata,
                                                        const QString& libraryRootOverride,
                                                        const QString& sourceDirectory) {
    world::imports::LDrawImportOptions options;
    options.filePath =
      resolvedPath(metadata.value("sourcePath").toString(metadata.value("filePath").toString()),
                   sourceDirectory);

    const QString libraryPath = libraryRootOverride.isEmpty()
                                  ? metadata.value("libraryPath").toString()
                                  : libraryRootOverride;
    options.libraryPath = resolvedPath(libraryPath, sourceDirectory);

    const QString normalMode = metadata.value("normalMode").toString();
    options.smoothNormals = normalMode.compare("smooth", Qt::CaseInsensitive) == 0 ||
                            metadata.value("smoothNormals").toBool(false);
    return options;
  }

  std::shared_ptr<render::Primitive>
  compileLDraw(const world::imports::LDrawImportOptions& options,
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
    auto resolver = std::make_shared<LDrawFilesystemResolver>(
      searchDirectoriesFor(options.filePath, options.libraryPath));
    const auto normalMode = options.smoothNormals ? LDrawGeometryCompiler::NormalMode::Smooth
                                                  : LDrawGeometryCompiler::NormalMode::Flat;
    LDrawGeometryCompiler compiler(resolver, 64, normalMode);

    if (diagnostics) {
      LDrawDiagnostics localDiagnostics;
      auto primitive = compiler.compile(input, colors, localDiagnostics);
      diagnostics->insert(diagnostics->end(), localDiagnostics.entries().begin(),
                          localDiagnostics.entries().end());
      return primitive;
    }

    return compiler.compile(input, colors);
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
      if (isLDrawMetadata(metadata)) {
        attachLDrawImport(group,
                          optionsFromMetadata(metadata, libraryRootOverride, sourceDirectory),
                          diagnostics);
      }
    }

    for (Element* child : root->childElements()) {
      resolveLDrawAuthoringImports(child, libraryRootOverride, sourceDirectory, diagnostics);
    }
  }
}
