#include "world/import/GltfSceneImporter.h"

#include "core/formats/gltf/GltfReader.h"
#include "core/math/Matrix.h"
#include "core/math/Quaternion.h"
#include "world/animation/Timeline.h"
#include "world/import/SceneImporterRegistry.h"
#include "world/objects/Scene.h"
#include "world/objects/Group.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace world {
  namespace {
    QString sourceId(const char* kind, std::size_t index) {
      return QString("%1/%2").arg(QString::fromLatin1(kind)).arg(index);
    }

    QString childTargetId(const QString& parentId, std::size_t nodeIndex) {
      return QString("%1/nodes/%2").arg(parentId).arg(nodeIndex);
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

    std::optional<core::math::interpolation::InterpolationMode>
    worldInterpolationMode(const std::string& gltfInterpolation) {
      if (gltfInterpolation == "STEP")
        return core::math::interpolation::InterpolationMode::Step;
      if (gltfInterpolation == "LINEAR")
        return core::math::interpolation::InterpolationMode::Linear;
      return std::nullopt;
    }

    QJsonArray jsonArray(std::initializer_list<double> values) {
      QJsonArray result;
      for (const double value : values)
        result.append(value);
      return result;
    }

    std::optional<double> accessorFloat(const core::gltf::Asset& asset,
                                        const core::gltf::Accessor& accessor, std::size_t element,
                                        std::size_t component) {
      if (!accessor.bufferView || accessor.componentType != core::gltf::ComponentType::Float32)
        return std::nullopt;
      const std::size_t componentCount = core::gltf::accessorTypeComponentCount(accessor.type);
      if (component >= componentCount || *accessor.bufferView >= asset.bufferViews.size())
        return std::nullopt;

      const auto& view = asset.bufferViews[*accessor.bufferView];
      if (view.buffer >= asset.buffers.size())
        return std::nullopt;
      const auto& buffer = asset.buffers[view.buffer];
      const std::size_t elementSize = core::gltf::accessorElementByteSize(accessor);
      const std::size_t stride = view.byteStride.value_or(elementSize);
      const std::size_t offset =
        view.byteOffset + accessor.byteOffset + element * stride + component * sizeof(float);
      if (offset > buffer.data.size() || sizeof(float) > buffer.data.size() - offset)
        return std::nullopt;

      float value = 0.0f;
      std::memcpy(&value, buffer.data.data() + offset, sizeof(float));
      if (!std::isfinite(value))
        return std::nullopt;
      return static_cast<double>(value);
    }

    std::optional<std::vector<double>> accessorScalars(const core::gltf::Asset& asset,
                                                       std::size_t accessorIndex) {
      if (accessorIndex >= asset.accessors.size())
        return std::nullopt;
      const auto& accessor = asset.accessors[accessorIndex];
      if (accessor.type != core::gltf::AccessorType::Scalar)
        return std::nullopt;

      std::vector<double> values;
      values.reserve(accessor.count);
      for (std::size_t i = 0; i < accessor.count; ++i) {
        auto value = accessorFloat(asset, accessor, i, 0);
        if (!value)
          return std::nullopt;
        values.push_back(*value);
      }
      return values;
    }

    std::optional<std::vector<QJsonValue>> accessorVectorValues(const core::gltf::Asset& asset,
                                                                std::size_t accessorIndex,
                                                                const std::string& targetPath) {
      if (accessorIndex >= asset.accessors.size())
        return std::nullopt;
      const auto& accessor = asset.accessors[accessorIndex];
      const core::gltf::AccessorType expectedType =
        targetPath == "rotation" ? core::gltf::AccessorType::Vec4 : core::gltf::AccessorType::Vec3;
      if (accessor.type != expectedType)
        return std::nullopt;

      std::vector<QJsonValue> values;
      values.reserve(accessor.count);
      for (std::size_t i = 0; i < accessor.count; ++i) {
        if (targetPath == "rotation") {
          auto x = accessorFloat(asset, accessor, i, 0);
          auto y = accessorFloat(asset, accessor, i, 1);
          auto z = accessorFloat(asset, accessor, i, 2);
          auto w = accessorFloat(asset, accessor, i, 3);
          if (!x || !y || !z || !w)
            return std::nullopt;
          const Quaterniond rotation(*w, *x, *y, *z);
          const Vector3d euler = rotation.normalized().toEulerAngles();
          values.push_back(jsonArray({euler.x(), euler.y(), euler.z()}));
        } else {
          auto x = accessorFloat(asset, accessor, i, 0);
          auto y = accessorFloat(asset, accessor, i, 1);
          auto z = accessorFloat(asset, accessor, i, 2);
          if (!x || !y || !z)
            return std::nullopt;
          values.push_back(jsonArray({*x, *y, *z}));
        }
      }
      return values;
    }

    QString worldPropertyForTargetPath(const std::string& targetPath) {
      if (targetPath == "translation")
        return QStringLiteral("position");
      if (targetPath == "rotation")
        return QStringLiteral("rotation");
      if (targetPath == "scale")
        return QStringLiteral("scale");
      return {};
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
            m_active(asset.nodes.size(), false),
            m_nodeTargetIds(asset.nodes.size()) {
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
        rawScene->setId(id);
        rawScene->setMetadata(
          baseMetadata(m_sourcePath, id, "scene", static_cast<int>(sceneIndex)));
        if (m_asset.defaultScene && *m_asset.defaultScene == sceneIndex)
          rawScene->setMetadataValue("defaultScene", true);
        attachProvenance(*rawScene, m_sourcePath, id, "scene");
        parent.addChild(std::move(sceneGroup));

        for (const std::size_t node : scene.nodes) {
          if (node < m_asset.nodes.size())
            addNode(*rawScene, node, Matrix4d(), id);
        }
      }

      void addAllNodes(Group& parent) {
        for (std::size_t i = 0; i < m_asset.nodes.size(); ++i)
          addNode(parent, i, Matrix4d(), QStringLiteral("asset"));
      }

      [[nodiscard]] const std::vector<std::vector<QString>>& nodeTargetIds() const {
        return m_nodeTargetIds;
      }

    private:
      void addNode(Group& parent, std::size_t nodeIndex, const Matrix4d& parentTransform,
                   const QString& parentTargetId) {
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
        const QString targetId = childTargetId(parentTargetId, nodeIndex);
        rawGroup->setId(targetId);
        rawGroup->setMetadata(baseMetadata(m_sourcePath, id, "node", static_cast<int>(nodeIndex)));
        rawGroup->setMetadataValue("gltfChildCount", static_cast<int>(node.children.size()));
        rawGroup->setMetadataValue("gltfAnimationChannelCount", 0);
        attachProvenance(*rawGroup, m_sourcePath, id, "node");
        parent.addChild(std::move(group));
        rawGroup->setMatrix(m_preserveHierarchy ? local : global);
        m_nodeTargetIds[nodeIndex].push_back(targetId);

        m_active[nodeIndex] = true;
        for (const std::size_t child : node.children) {
          if (child >= m_asset.nodes.size())
            continue;
          if (m_preserveHierarchy)
            addNode(*rawGroup, child, global, targetId);
          else
            addNode(parent, child, global, parentTargetId);
        }
        m_active[nodeIndex] = false;
      }

      const core::gltf::Asset& m_asset;
      QString m_sourcePath;
      bool m_preserveHierarchy;
      std::vector<bool> m_active;
      std::vector<std::vector<QString>> m_nodeTargetIds;
    };

    class AnimationCompiler {
    public:
      AnimationCompiler(const core::gltf::Asset& asset,
                        const std::vector<std::vector<QString>>& nodeTargetIds)
          : m_asset(asset),
            m_nodeTargetIds(nodeTargetIds) {
      }

      std::unique_ptr<world::Timeline> compile(ImportResult& result, Group& root,
                                               const QString& sourcePath) {
        std::vector<world::AnimationTrack> tracks;
        int endFrame = 0;
        for (std::size_t animationIndex = 0; animationIndex < m_asset.animations.size();
             ++animationIndex) {
          const auto& animation = m_asset.animations[animationIndex];
          for (std::size_t channelIndex = 0; channelIndex < animation.channels.size();
               ++channelIndex) {
            compileChannel(animationIndex, channelIndex, animation, tracks, endFrame, result, root,
                           sourcePath);
          }
        }

        if (tracks.empty())
          return nullptr;
        return std::make_unique<world::Timeline>(0, std::max(0, endFrame), 24.0, std::move(tracks));
      }

    private:
      void addWarning(ImportResult& result, const QString& sourcePath,
                      const QString& message) const {
        result.addDiagnostic(ImportDiagnostic::warning(message, sourcePath));
      }

      void appendNodeMetadata(Group& root, std::size_t nodeIndex,
                              const QJsonObject& channelMetadata) const {
        if (nodeIndex >= m_nodeTargetIds.size())
          return;
        for (const auto& targetId : m_nodeTargetIds[nodeIndex]) {
          auto* element = root.findById(targetId);
          if (!element)
            continue;
          QJsonArray channels = element->metadataValue("gltfAnimationChannels").toArray();
          channels.append(channelMetadata);
          element->setMetadataValue("gltfAnimationChannels", channels);
          element->setMetadataValue("gltfAnimationChannelCount", channels.size());
        }
      }

      void compileChannel(std::size_t animationIndex, std::size_t channelIndex,
                          const core::gltf::Animation& animation,
                          std::vector<world::AnimationTrack>& tracks, int& endFrame,
                          ImportResult& result, Group& root, const QString& sourcePath) {
        const auto& channel = animation.channels[channelIndex];
        if (channel.sampler >= animation.samplers.size())
          return;

        const auto& sampler = animation.samplers[channel.sampler];
        QJsonObject metadata;
        metadata["animationIndex"] = static_cast<int>(animationIndex);
        metadata["animationName"] = QString::fromStdString(animation.name);
        metadata["channelIndex"] = static_cast<int>(channelIndex);
        metadata["samplerIndex"] = static_cast<int>(channel.sampler);
        metadata["targetPath"] = QString::fromStdString(channel.target.path);
        metadata["interpolation"] = QString::fromStdString(sampler.interpolation);
        metadata["inputAccessor"] = static_cast<int>(sampler.input);
        metadata["outputAccessor"] = static_cast<int>(sampler.output);

        if (!channel.target.node) {
          metadata["represented"] = false;
          metadata["reason"] = "animation channel has no node target";
          addWarning(result, sourcePath,
                     QString("glTF animation %1 channel %2 has no node target")
                       .arg(animationIndex)
                       .arg(channelIndex));
          return;
        }
        const QString property = worldPropertyForTargetPath(channel.target.path);
        if (property.isEmpty()) {
          metadata["represented"] = false;
          metadata["reason"] = "unsupported target path";
          appendNodeMetadata(root, *channel.target.node, metadata);
          addWarning(result, sourcePath,
                     QString("glTF animation %1 channel %2 targets unsupported path '%3'")
                       .arg(animationIndex)
                       .arg(channelIndex)
                       .arg(QString::fromStdString(channel.target.path)));
          return;
        }

        auto interpolation = worldInterpolationMode(sampler.interpolation);
        if (!interpolation) {
          metadata["represented"] = false;
          metadata["reason"] = "unsupported interpolation";
          appendNodeMetadata(root, *channel.target.node, metadata);
          addWarning(result, sourcePath,
                     QString("glTF animation %1 channel %2 uses unsupported interpolation '%3'")
                       .arg(animationIndex)
                       .arg(channelIndex)
                       .arg(QString::fromStdString(sampler.interpolation)));
          return;
        }

        auto times = accessorScalars(m_asset, sampler.input);
        auto values = accessorVectorValues(m_asset, sampler.output, channel.target.path);
        if (!times || !values || times->size() != values->size() || times->empty()) {
          metadata["represented"] = false;
          metadata["reason"] = "unsupported accessor data";
          appendNodeMetadata(root, *channel.target.node, metadata);
          addWarning(result, sourcePath,
                     QString("glTF animation %1 channel %2 has unsupported sampler accessor data")
                       .arg(animationIndex)
                       .arg(channelIndex));
          return;
        }

        std::vector<world::AnimationKeyframe> keyframes;
        keyframes.reserve(times->size());
        for (std::size_t i = 0; i < times->size(); ++i) {
          const int frame = static_cast<int>(std::lround((*times)[i] * 24.0));
          endFrame = std::max(endFrame, frame);
          keyframes.push_back({frame, (*values)[i]});
        }

        if (*channel.target.node >= m_nodeTargetIds.size())
          return;
        metadata["represented"] = true;
        metadata["worldProperty"] = property;
        appendNodeMetadata(root, *channel.target.node, metadata);
        for (const auto& targetId : m_nodeTargetIds[*channel.target.node]) {
          tracks.emplace_back(targetId, property, keyframes, *interpolation);
        }
      }

      const core::gltf::Asset& m_asset;
      const std::vector<std::vector<QString>>& m_nodeTargetIds;
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

    std::unique_ptr<world::Timeline> timeline;
    ImportResult result;
    result.setSource(source);
    try {
      GroupCompiler compiler(*readResult.asset, filename,
                             options.value("preserve_hierarchy", true).toBool());
      if (!readResult.asset->scenes.empty()) {
        for (std::size_t i = 0; i < readResult.asset->scenes.size(); ++i)
          compiler.addScene(*root, i);
      } else {
        compiler.addAllNodes(*root);
      }
      if (!readResult.asset->animations.empty()) {
        root->setMetadataValue("gltfAnimationCount",
                               static_cast<int>(readResult.asset->animations.size()));
        AnimationCompiler animationCompiler(*readResult.asset, compiler.nodeTargetIds());
        timeline = animationCompiler.compile(result, *root, filename);
      }
    } catch (const std::exception& error) {
      return ImportResult::failed({ImportDiagnostic::error(error.what(), filename)}, source);
    }

    if (timeline) {
      auto scene = std::make_unique<Scene>(nullptr);
      scene->setName(root->name());
      scene->setMetadata(root->metadata());
      scene->addChild(std::move(root));
      scene->setAnimation(std::move(timeline));
      result.setRoot(std::move(scene));
    } else {
      result.setRoot(std::move(root));
    }
    appendDiagnostics(result, readResult.diagnostics, filename);
    return result;
  }

}

static bool dummy =
  world::SceneImporterRegistry::self().registerClass<world::GltfSceneImporter>("gltf");
