#include "world/objects/LDrawSceneImporter.h"

#include "core/Exception.h"
#include "core/formats/ldraw/LDrawColorTable.h"
#include "core/formats/ldraw/LDrawFileResolver.h"
#include "core/formats/ldraw/LDrawGeometryCompiler.h"
#include "core/formats/ldraw/LDrawParser.h"
#include "core/math/Matrix.h"
#include "render/primitives/Composite.h"
#include "render/primitives/Primitive.h"
#include "world/objects/Group.h"
#include "world/objects/Surface.h"

#include <QJsonObject>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {
  class CompiledLDrawSurface : public Surface {
  public:
    explicit CompiledLDrawSurface(std::shared_ptr<render::Primitive> primitive,
                                  Element* parent = nullptr)
        : Surface(parent),
          m_primitive(std::move(primitive)) {
      setName("LDraw Geometry");
      setGenerated(true);
    }

  protected:
    std::shared_ptr<render::Primitive> toRaytracerPrimitive() const override {
      return m_primitive;
    }

  private:
    std::shared_ptr<render::Primitive> m_primitive;
  };

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

  Matrix4d transformForSubfileReference(const LDrawSubfileReference& reference) {
    const auto& m = reference.matrix;
    return Matrix4d(m[0], m[1], m[2], reference.translation.x(), m[3], m[4], m[5],
                    reference.translation.y(), m[6], m[7], m[8], reference.translation.z(), 0.0,
                    0.0, 0.0, 1.0);
  }

  bool isStepMeta(const LDrawCommand& command) {
    if (!std::holds_alternative<LDrawMetaCommand>(command))
      return false;

    const auto& meta = std::get<LDrawMetaCommand>(command);
    return meta.keyword == "STEP";
  }

  bool hasRenderablePolygon(const LDrawParser::Commands& commands) {
    for (const auto& command : commands) {
      if (std::holds_alternative<LDrawTriangle>(command) ||
          std::holds_alternative<LDrawQuad>(command)) {
        return true;
      }
    }
    return false;
  }

  std::string metadataBlockName(const std::string& sourceFile, const std::string& sourceBlock) {
    return sourceBlock.empty() ? sourceFile : sourceBlock;
  }

  QJsonObject baseMetadata(const std::string& sourceFile, const std::string& sourceBlock) {
    return QJsonObject{
      {"sourceFormat", "ldraw"},
      {"sourceFile", QString::fromStdString(sourceFile)},
      {"sourceBlock", QString::fromStdString(metadataBlockName(sourceFile, sourceBlock))},
    };
  }

  class PreservingImporter {
  public:
    PreservingImporter(std::shared_ptr<const LDrawFileResolver> resolver,
                       const LDrawColorTable& colors, LDrawGeometryCompiler::NormalMode normalMode,
                       int recursionLimit, LDrawDiagnostics& diagnostics)
        : m_resolver(std::move(resolver)),
          m_colors(colors),
          m_normalMode(normalMode),
          m_recursionLimit(recursionLimit),
          m_diagnostics(diagnostics) {
    }

    std::unique_ptr<Group> importRoot(const LDrawDocumentFile& file,
                                      const std::string& sourceFile) {
      auto group = std::make_unique<Group>();
      group->setName("LDraw Model");
      group->setMetadata(baseMetadata(sourceFile, file.filename));
      importCommands(file.commands, file.filename, sourceFile, *group, 0);
      return group;
    }

  private:
    void importCommands(const LDrawParser::Commands& commands, const std::string& sourceBlock,
                        const std::string& sourceFile, Group& parent, int depth) {
      if (depth >= m_recursionLimit) {
        throw Exception("LDraw subfile recursion limit exceeded while preserving hierarchy",
                        __FILE__, __LINE__);
      }

      int stepIndex = 1;
      auto step = makeStepGroup(sourceFile, sourceBlock, stepIndex);
      LDrawParser::Commands geometryCommands;

      auto flushGeometry = [&]() {
        if (!hasRenderablePolygon(geometryCommands)) {
          geometryCommands.clear();
          return;
        }

        LDrawGeometryCompiler compiler(m_resolver, m_recursionLimit, m_normalMode);
        auto primitive = compiler.compile(geometryCommands, m_colors, m_diagnostics);
        step->addChild(new CompiledLDrawSurface(std::move(primitive)));
        geometryCommands.clear();
      };

      for (const auto& command : commands) {
        if (isStepMeta(command)) {
          flushGeometry();
          attachStepIfUseful(parent, std::move(step));
          ++stepIndex;
          step = makeStepGroup(sourceFile, sourceBlock, stepIndex);
          continue;
        }

        if (std::holds_alternative<LDrawSubfileReference>(command)) {
          flushGeometry();
          auto submodel =
            importSubmodel(std::get<LDrawSubfileReference>(command), sourceFile, depth + 1);
          step->addChild(submodel.release());
          continue;
        }

        geometryCommands.push_back(command);
      }

      flushGeometry();
      attachStepIfUseful(parent, std::move(step));
    }

    std::unique_ptr<Group> importSubmodel(const LDrawSubfileReference& reference,
                                          const std::string& sourceFile, int depth) {
      const std::string fileKey = m_resolver->cacheKey(reference.filename);
      if (m_activeFiles.find(fileKey) != m_activeFiles.end()) {
        throw Exception("LDraw subfile cycle detected while preserving hierarchy: " +
                          reference.filename,
                        __FILE__, __LINE__);
      }

      auto input = m_resolver->open(reference.filename);
      if (!input) {
        LDrawDiagnostic diagnostic;
        diagnostic.severity = LDrawDiagnosticSeverity::Error;
        diagnostic.code = LDrawDiagnosticCode::MissingSubfile;
        diagnostic.file = sourceFile;
        diagnostic.lineNumber = reference.lineNumber;
        diagnostic.message = "resolver could not open subfile";
        diagnostic.reference = reference.filename;
        diagnostic.searchedRoots = m_resolver->searchRoots(reference.filename);
        m_diagnostics.add(std::move(diagnostic));
        throw Exception("LDraw resolver could not open subfile: " + reference.filename, __FILE__,
                        __LINE__);
      }

      const auto commands = LDrawParser().parse(*input);
      auto group = std::make_unique<Group>();
      group->setName(QString::fromStdString(reference.filename));
      group->setMatrix(transformForSubfileReference(reference));
      group->setMetadata(QJsonObject{
        {"sourceFormat", "ldraw"},
        {"sourceFile", QString::fromStdString(sourceFile)},
        {"sourceBlock", QString::fromStdString(reference.filename)},
        {"submodelName", QString::fromStdString(reference.filename)},
        {"sourceLine", reference.lineNumber},
      });

      m_activeFiles.insert(fileKey);
      importCommands(commands, reference.filename, sourceFile, *group, depth);
      m_activeFiles.erase(fileKey);

      return group;
    }

    static std::unique_ptr<Group> makeStepGroup(const std::string& sourceFile,
                                                const std::string& sourceBlock, int stepIndex) {
      auto group = std::make_unique<Group>();
      group->setName(QString("LDraw Step %1").arg(stepIndex));
      auto metadata = baseMetadata(sourceFile, sourceBlock);
      metadata["buildStepIndex"] = stepIndex;
      group->setMetadata(metadata);
      return group;
    }

    static void attachStepIfUseful(Group& parent, std::unique_ptr<Group> step) {
      if (!step->childElements().empty())
        parent.addChild(step.release());
    }

    std::shared_ptr<const LDrawFileResolver> m_resolver;
    const LDrawColorTable& m_colors;
    LDrawGeometryCompiler::NormalMode m_normalMode;
    int m_recursionLimit;
    LDrawDiagnostics& m_diagnostics;
    std::unordered_set<std::string> m_activeFiles;
  };
}

LDrawSceneImporter::Result LDrawSceneImporter::importFile(const Options& options) const {
  if (options.filePath.isEmpty()) {
    throw Exception("LDraw import filePath must not be empty", __FILE__, __LINE__);
  }

  if (!options.preserveHierarchy) {
    auto model = std::make_unique<Group>();
    model->setName("LDraw Import");
    QJsonObject metadata;
    metadata["sourceFormat"] = "LDraw";
    metadata["sourcePath"] = options.filePath;
    metadata["normalMode"] = options.smoothNormals ? "smooth" : "flat";
    if (!options.libraryPath.isEmpty()) {
      metadata["libraryPath"] = options.libraryPath;
    }
    model->setMetadata(metadata);
    return Result{std::move(model), {}};
  }

  std::ifstream input(options.filePath.toStdString());
  if (!input) {
    std::ostringstream message;
    message << "Unable to read LDraw model '" << options.filePath.toStdString() << "'";
    throw Exception(message.str(), __FILE__, __LINE__);
  }

  LDrawDiagnostics diagnostics;
  const auto document = LDrawParser().parseDocument(input);
  auto filesystemResolver = std::make_shared<LDrawFilesystemResolver>(
    searchDirectoriesFor(options.filePath, options.libraryPath));
  std::shared_ptr<const LDrawFileResolver> resolver = filesystemResolver;
  if (document.isMultipart()) {
    resolver = std::make_shared<LDrawMpdFileResolver>(document, filesystemResolver);
  }

  LDrawColorTable colors;
  colors.loadLibraryConfig(options.libraryPath.toStdString());
  const auto normalMode = options.smoothNormals ? LDrawGeometryCompiler::NormalMode::Smooth
                                                : LDrawGeometryCompiler::NormalMode::Flat;
  PreservingImporter importer(resolver, colors, normalMode, options.recursionLimit, diagnostics);
  auto root = importer.importRoot(document.mainFile(), options.filePath.toStdString());
  return Result{std::move(root), diagnostics.entries()};
}
