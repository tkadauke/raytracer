#include "world/import/GltfSceneImporter.h"

#include "core/formats/gltf/GltfReader.h"
#include "core/math/Matrix.h"
#include "core/math/Quaternion.h"
#include "world/import/SceneImporterRegistry.h"
#include "world/objects/Group.h"

#include <QFileInfo>
#include <QJsonObject>

#include <memory>
#include <stdexcept>
#include <vector>

namespace world {
  namespace {
    QString sourceId(const char* kind, std::size_t index) {
      return QString("%1/%2").arg(QString::fromLatin1(kind)).arg(index);
    }

    QJsonObject baseMetadata(const QString& sourcePath, const QString& sourceIdValue,
                             const QString& kind, int index) {
      return QJsonObject{
        {GroupMetadata::sourceFormatKey(), QStringLiteral("glTF")},
        {GroupMetadata::sourceIdKey(), sourceIdValue},
        {"sourcePath", sourcePath},
        {"gltfKind", kind},
        {"gltfIndex", index},
      };
    }

    void attachProvenance(Group& group, const QString& sourcePath, const QString& sourceIdValue,
                          const QString& kind) {
      ImportProvenance provenance;
      provenance.sourceFile = sourcePath;
      provenance.sourceId = sourceIdValue;
      provenance.category = QJsonObject{{"gltfKind", kind}};
      setImportProvenance(group, provenance);
    }

    Matrix4d matrixFromGltf(const std::array<double, 16>& values) {
      return Matrix4d(values[0], values[4], values[8], values[12], values[1], values[5], values[9],
                      values[13], values[2], values[6], values[10], values[14], values[3],
                      values[7], values[11], values[15]);
    }

    Matrix4d localTransformFor(const core::gltf::Node& node) {
      if (node.matrix)
        return matrixFromGltf(*node.matrix);

      const Matrix4d translation =
        Matrix4d::translate(node.translation[0], node.translation[1], node.translation[2]);
      const Quaterniond rotation(node.rotation[3], node.rotation[0], node.rotation[1],
                                 node.rotation[2]);
      const Matrix4d scale = Matrix4d(Matrix3d::scale(node.scale[0], node.scale[1], node.scale[2]));
      return translation * rotation.normalized().toMatrix4() * scale;
    }

    ImportDiagnostic importDiagnosticFor(const core::gltf::Diagnostic& diagnostic,
                                         const QString& source) {
      const QString message = QString::fromStdString(diagnostic.toString());
      if (diagnostic.severity == core::gltf::DiagnosticSeverity::Error)
        return ImportDiagnostic::error(message, source);
      return ImportDiagnostic::warning(message, source);
    }

    void appendDiagnostics(ImportResult& result, const core::gltf::Diagnostics& diagnostics,
                           const QString& source) {
      for (const auto& diagnostic : diagnostics.entries())
        result.addDiagnostic(importDiagnosticFor(diagnostic, source));
    }

    class GroupCompiler {
    public:
      GroupCompiler(const core::gltf::Asset& asset, QString sourcePath, bool preserveHierarchy)
          : m_asset(asset),
            m_sourcePath(std::move(sourcePath)),
            m_preserveHierarchy(preserveHierarchy),
            m_active(asset.nodes.size(), false) {
      }

      void addScene(Group& parent, std::size_t sceneIndex) {
        if (sceneIndex >= m_asset.scenes.size())
          return;

        const auto& scene = m_asset.scenes[sceneIndex];
        auto sceneGroup = std::make_unique<Group>();
        Group* rawScene = sceneGroup.get();
        rawScene->setName(scene.name.empty() ? QString("glTF Scene %1").arg(sceneIndex)
                                             : QString::fromStdString(scene.name));
        const QString id = sourceId("scenes", sceneIndex);
        rawScene->setMetadata(
          baseMetadata(m_sourcePath, id, "scene", static_cast<int>(sceneIndex)));
        if (m_asset.defaultScene && *m_asset.defaultScene == sceneIndex)
          rawScene->setMetadataValue("defaultScene", true);
        attachProvenance(*rawScene, m_sourcePath, id, "scene");
        parent.addChild(std::move(sceneGroup));

        for (const std::size_t node : scene.nodes) {
          if (node < m_asset.nodes.size())
            addNode(*rawScene, node, Matrix4d());
        }
      }

      void addAllNodes(Group& parent) {
        for (std::size_t i = 0; i < m_asset.nodes.size(); ++i)
          addNode(parent, i, Matrix4d());
      }

    private:
      void addNode(Group& parent, std::size_t nodeIndex, const Matrix4d& parentTransform) {
        if (m_active[nodeIndex])
          throw std::invalid_argument("glTF node hierarchy contains a cycle");

        const auto& node = m_asset.nodes[nodeIndex];
        const Matrix4d local = localTransformFor(node);
        const Matrix4d global = parentTransform * local;

        auto group = std::make_unique<Group>();
        Group* rawGroup = group.get();
        rawGroup->setName(node.name.empty() ? QString("glTF Node %1").arg(nodeIndex)
                                            : QString::fromStdString(node.name));
        const QString id = sourceId("nodes", nodeIndex);
        rawGroup->setMetadata(baseMetadata(m_sourcePath, id, "node", static_cast<int>(nodeIndex)));
        rawGroup->setMetadataValue("gltfChildCount", static_cast<int>(node.children.size()));
        attachProvenance(*rawGroup, m_sourcePath, id, "node");
        parent.addChild(std::move(group));
        rawGroup->setMatrix(m_preserveHierarchy ? local : global);

        m_active[nodeIndex] = true;
        for (const std::size_t child : node.children) {
          if (child >= m_asset.nodes.size())
            continue;
          if (m_preserveHierarchy)
            addNode(*rawGroup, child, global);
          else
            addNode(parent, child, global);
        }
        m_active[nodeIndex] = false;
      }

      const core::gltf::Asset& m_asset;
      QString m_sourcePath;
      bool m_preserveHierarchy;
      std::vector<bool> m_active;
    };
  }

  QString GltfSceneImporter::name() const {
    return "gltf";
  }

  QStringList GltfSceneImporter::supportedExtensions() const {
    return {"gltf", "glb"};
  }

  ImportOptionSchemas GltfSceneImporter::optionSchema() const {
    return {
      {"preserve_hierarchy",
       ImportOptionType::Boolean,
       "Preserve hierarchy",
       "Keep glTF scene and node parent-child hierarchy as Groups.",
       true,
       false,
       {}},
    };
  }

  ImportResult GltfSceneImporter::importFile(const QString& filename,
                                             const ImportOptions& options) const {
    ImportSourceMetadata source;
    source.importerName = name();
    source.formatName = "glTF";
    source.sourcePath = filename;

    const auto readResult = core::gltf::Reader::readFile(filename.toStdString());
    if (!readResult.asset) {
      ImportResult result = ImportResult::failed({}, source);
      appendDiagnostics(result, readResult.diagnostics, filename);
      return result;
    }

    const QFileInfo fileInfo(filename);
    auto root = std::make_unique<Group>();
    root->setName(fileInfo.completeBaseName().isEmpty() ? QStringLiteral("glTF Import")
                                                        : fileInfo.completeBaseName());
    const QString rootSourceId = QStringLiteral("asset");
    root->setMetadata(baseMetadata(filename, rootSourceId, "asset", -1));
    attachProvenance(*root, filename, rootSourceId, "asset");

    try {
      GroupCompiler compiler(*readResult.asset, filename,
                             options.value("preserve_hierarchy", true).toBool());
      if (!readResult.asset->scenes.empty()) {
        for (std::size_t i = 0; i < readResult.asset->scenes.size(); ++i)
          compiler.addScene(*root, i);
      } else {
        compiler.addAllNodes(*root);
      }
    } catch (const std::exception& error) {
      return ImportResult::failed({ImportDiagnostic::error(error.what(), filename)}, source);
    }

    ImportResult result(std::move(root), source);
    appendDiagnostics(result, readResult.diagnostics, filename);
    return result;
  }

}

static bool dummy =
  world::SceneImporterRegistry::self().registerClass<world::GltfSceneImporter>("gltf");
