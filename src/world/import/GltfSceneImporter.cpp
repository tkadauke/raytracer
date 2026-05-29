#include "world/import/GltfSceneImporter.h"

#include "core/geometry/Mesh.h"
#include "core/formats/gltf/GltfReader.h"
#include "core/math/Matrix.h"
#include "core/math/Quaternion.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/MeshPrimitive.h"
#include "render/textures/ConstantColorTexture.h"
#include "render/textures/ImageTexture.h"
#include "render/textures/TintedTexture.h"
#include "render/textures/mappings/UVMapping2D.h"
#include "world/animation/Timeline.h"
#include "world/import/ImportedSceneDefaults.h"
#include "world/import/SceneImporterRegistry.h"
#include "world/objects/CompiledPrimitive.h"
#include "world/objects/DirectionalLight.h"
#include "world/objects/Group.h"
#include "world/objects/OrthographicCamera.h"
#include "world/objects/PinholeCamera.h"
#include "world/objects/PointLight.h"
#include "world/objects/Scene.h"
#include "world/objects/Transformable.h"

#include <QColor>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
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

    bool isWhite(const Colord& color) {
      return color.r() == 1.0 && color.g() == 1.0 && color.b() == 1.0;
    }

    render::ImageTextureFilter imageFilterFor(const core::gltf::Asset& asset,
                                              const core::gltf::Texture& texture) {
      if (!texture.sampler || *texture.sampler >= asset.samplers.size())
        return render::ImageTextureFilter::Bilinear;

      const auto& sampler = asset.samplers[*texture.sampler];
      if (sampler.minFilter) {
        switch (*sampler.minFilter) {
        case 9728:
        case 9984:
          return render::ImageTextureFilter::Nearest;
        case 9985:
        case 9986:
        case 9987:
          return render::ImageTextureFilter::Mipmap;
        default:
          break;
        }
      }
      if (sampler.magFilter && *sampler.magFilter == 9728)
        return render::ImageTextureFilter::Nearest;
      return render::ImageTextureFilter::Bilinear;
    }

    render::ImageTextureWrap imageWrapFor(const core::gltf::Asset& asset,
                                          const core::gltf::Texture& texture) {
      if (!texture.sampler || *texture.sampler >= asset.samplers.size())
        return render::ImageTextureWrap::Repeat;

      const auto& sampler = asset.samplers[*texture.sampler];
      if (sampler.wrapS == 33071 || sampler.wrapT == 33071)
        return render::ImageTextureWrap::Clamp;
      return render::ImageTextureWrap::Repeat;
    }

    std::shared_ptr<render::Texturec> imageTextureFor(const core::gltf::Asset& asset,
                                                      const core::gltf::TextureInfo& info) {
      if (info.index >= asset.textures.size())
        return nullptr;
      const auto& texture = asset.textures[info.index];
      if (!texture.source || *texture.source >= asset.images.size())
        return nullptr;

      const auto& image = asset.images[*texture.source];
      const auto filter = imageFilterFor(asset, texture);
      const auto wrap = imageWrapFor(asset, texture);
      if (!image.resolvedPath.empty()) {
        return render::ImageTexture::fromFile(new render::UVMapping2D, image.resolvedPath.string(),
                                              filter, wrap);
      }

      QImage decoded;
      if (!image.data.empty()) {
        decoded.loadFromData(image.data.data(), static_cast<int>(image.data.size()));
      }
      if (decoded.isNull())
        return nullptr;

      const QImage converted = decoded.convertToFormat(QImage::Format_RGBA8888);
      std::vector<Colord> pixels;
      pixels.reserve(static_cast<std::size_t>(converted.width() * converted.height()));
      for (int y = 0; y != converted.height(); ++y) {
        for (int x = 0; x != converted.width(); ++x) {
          const QColor color = QColor::fromRgba(converted.pixel(x, y));
          pixels.emplace_back(color.redF(), color.greenF(), color.blueF());
        }
      }
      return std::make_shared<render::ImageTexture>(new render::UVMapping2D, converted.width(),
                                                    converted.height(), pixels, filter, wrap);
    }

    std::shared_ptr<render::Texturec> baseColorTextureFor(const core::gltf::Asset& asset,
                                                          const core::gltf::Material& source) {
      const Colord baseColor(source.baseColorFactor[0], source.baseColorFactor[1],
                             source.baseColorFactor[2]);
      if (!source.baseColorTexture)
        return std::make_shared<render::ConstantColorTexture>(baseColor);

      auto texture = imageTextureFor(asset, *source.baseColorTexture);
      if (!texture)
        return std::make_shared<render::ConstantColorTexture>(baseColor);
      if (isWhite(baseColor))
        return texture;
      return std::make_shared<render::TintedTexture>(std::move(texture), baseColor);
    }

    void appendMaterialDiagnostics(ImportResult& result, const core::gltf::Asset& asset,
                                   const QString& sourcePath) {
      for (std::size_t i = 0; i < asset.materials.size(); ++i) {
        const auto& material = asset.materials[i];
        const QString where = QString("%1 materials[%2]").arg(sourcePath).arg(i);
        if (material.metallicFactor && *material.metallicFactor > 0.0) {
          result.addDiagnostic(ImportDiagnostic::warning(
            QString("glTF metallicFactor %1 is approximated as matte diffuse shading")
              .arg(*material.metallicFactor),
            where));
        }
        if (material.roughnessFactor) {
          result.addDiagnostic(ImportDiagnostic::warning(
            QString("glTF roughnessFactor %1 is approximated as matte diffuse shading")
              .arg(*material.roughnessFactor),
            where));
        }
        if (material.metallicRoughnessTexture) {
          result.addDiagnostic(ImportDiagnostic::warning(
            QStringLiteral("glTF metallicRoughnessTexture is not supported"), where));
        }
        if (material.alphaMode != "OPAQUE") {
          result.addDiagnostic(
            ImportDiagnostic::warning(QString("glTF alphaMode '%1' is not supported")
                                        .arg(QString::fromStdString(material.alphaMode)),
                                      where));
        }
        if (material.doubleSided) {
          result.addDiagnostic(ImportDiagnostic::warning(
            QStringLiteral("glTF doubleSided rendering is not supported"), where));
        }
        if (material.baseColorTexture && material.baseColorTexture->texCoord != 0) {
          result.addDiagnostic(ImportDiagnostic::warning(
            QString("glTF baseColorTexture texCoord %1 is not supported; TEXCOORD_0 is used")
              .arg(material.baseColorTexture->texCoord),
            where));
        }
        for (const std::string& feature : material.unsupportedFeatures) {
          result.addDiagnostic(ImportDiagnostic::warning(
            QString("glTF %1 is not supported").arg(QString::fromStdString(feature)), where));
        }
      }
    }

    void attachElementProvenance(Element& element, const QString& sourcePath,
                                 const QString& sourceIdValue, const QString& kind) {
      ImportProvenance provenance;
      provenance.sourceFile = sourcePath;
      provenance.sourceId = sourceIdValue;
      provenance.category = QJsonObject{{"gltfKind", kind}};
      setImportProvenance(element, provenance);
    }

    QJsonArray jsonArrayFor(const std::array<double, 3>& values) {
      return QJsonArray{values[0], values[1], values[2]};
    }

    Vector3d normalizedDirectionOrDefault(const Matrix4d& transform, const Vector3d& local) {
      const Vector3d direction = transform.transformDirection(local);
      if (direction.length() <= 1e-9 || direction.isUndefined())
        return Vector3d(0.0, 0.0, 1.0);
      return direction.normalized();
    }

    std::size_t accessorStride(const core::gltf::Asset& asset,
                               const core::gltf::Accessor& accessor) {
      if (!accessor.bufferView)
        return core::gltf::accessorElementByteSize(accessor);
      return asset.bufferViews[*accessor.bufferView].byteStride.value_or(
        core::gltf::accessorElementByteSize(accessor));
    }

    const std::uint8_t* accessorElementData(const core::gltf::Asset& asset,
                                            const core::gltf::Accessor& accessor,
                                            std::size_t element) {
      if (!accessor.bufferView)
        return nullptr;
      const auto& view = asset.bufferViews[*accessor.bufferView];
      const auto& buffer = asset.buffers[view.buffer];
      const std::size_t offset =
        view.byteOffset + accessor.byteOffset + element * accessorStride(asset, accessor);
      if (offset >= buffer.data.size())
        return nullptr;
      return buffer.data.data() + offset;
    }

    float readFloat32Le(const std::uint8_t* data) {
      std::uint32_t bits =
        static_cast<std::uint32_t>(data[0]) | (static_cast<std::uint32_t>(data[1]) << 8u) |
        (static_cast<std::uint32_t>(data[2]) << 16u) | (static_cast<std::uint32_t>(data[3]) << 24u);
      float value = 0.0f;
      std::memcpy(&value, &bits, sizeof(value));
      return value;
    }

    std::uint32_t readUint16Le(const std::uint8_t* data) {
      return static_cast<std::uint32_t>(data[0]) | (static_cast<std::uint32_t>(data[1]) << 8u);
    }

    std::uint32_t readUint32Le(const std::uint8_t* data) {
      return static_cast<std::uint32_t>(data[0]) | (static_cast<std::uint32_t>(data[1]) << 8u) |
             (static_cast<std::uint32_t>(data[2]) << 16u) |
             (static_cast<std::uint32_t>(data[3]) << 24u);
    }

    std::vector<Vector3d> readVec3Accessor(const core::gltf::Asset& asset,
                                           std::size_t accessorIndex, const char* semantic) {
      if (accessorIndex >= asset.accessors.size())
        throw std::runtime_error(
          QString("glTF %1 accessor is missing").arg(semantic).toStdString());
      const auto& accessor = asset.accessors[accessorIndex];
      if (accessor.componentType != core::gltf::ComponentType::Float32 ||
          accessor.type != core::gltf::AccessorType::Vec3) {
        throw std::runtime_error(
          QString("glTF %1 accessor must be FLOAT VEC3").arg(semantic).toStdString());
      }

      std::vector<Vector3d> values;
      values.reserve(accessor.count);
      for (std::size_t i = 0; i < accessor.count; ++i) {
        const std::uint8_t* data = accessorElementData(asset, accessor, i);
        if (!data)
          throw std::runtime_error(
            QString("glTF %1 accessor data is unavailable").arg(semantic).toStdString());
        values.emplace_back(readFloat32Le(data), readFloat32Le(data + 4), readFloat32Le(data + 8));
      }
      return values;
    }

    std::vector<Vector2d> readVec2Accessor(const core::gltf::Asset& asset,
                                           std::size_t accessorIndex, const char* semantic) {
      if (accessorIndex >= asset.accessors.size())
        throw std::runtime_error(
          QString("glTF %1 accessor is missing").arg(semantic).toStdString());
      const auto& accessor = asset.accessors[accessorIndex];
      if (accessor.componentType != core::gltf::ComponentType::Float32 ||
          accessor.type != core::gltf::AccessorType::Vec2) {
        throw std::runtime_error(
          QString("glTF %1 accessor must be FLOAT VEC2").arg(semantic).toStdString());
      }

      std::vector<Vector2d> values;
      values.reserve(accessor.count);
      for (std::size_t i = 0; i < accessor.count; ++i) {
        const std::uint8_t* data = accessorElementData(asset, accessor, i);
        if (!data)
          throw std::runtime_error(
            QString("glTF %1 accessor data is unavailable").arg(semantic).toStdString());
        values.emplace_back(readFloat32Le(data), readFloat32Le(data + 4));
      }
      return values;
    }

    std::vector<int> readIndexAccessor(const core::gltf::Asset& asset, std::size_t accessorIndex) {
      if (accessorIndex >= asset.accessors.size())
        throw std::runtime_error("glTF index accessor is missing");
      const auto& accessor = asset.accessors[accessorIndex];
      if (accessor.type != core::gltf::AccessorType::Scalar) {
        throw std::runtime_error("glTF index accessor must be SCALAR");
      }

      std::vector<int> values;
      values.reserve(accessor.count);
      for (std::size_t i = 0; i < accessor.count; ++i) {
        const std::uint8_t* data = accessorElementData(asset, accessor, i);
        if (!data)
          throw std::runtime_error("glTF index accessor data is unavailable");

        std::uint32_t value = 0;
        switch (accessor.componentType) {
        case core::gltf::ComponentType::Uint8:
          value = data[0];
          break;
        case core::gltf::ComponentType::Uint16:
          value = readUint16Le(data);
          break;
        case core::gltf::ComponentType::Uint32:
          value = readUint32Le(data);
          break;
        default:
          throw std::runtime_error(
            "glTF index accessor must use an unsigned integer component type");
        }
        if (value > static_cast<std::uint32_t>(std::numeric_limits<int>::max()))
          throw std::runtime_error("glTF index value is too large for Mesh");
        values.push_back(static_cast<int>(value));
      }
      return values;
    }

    std::shared_ptr<render::Material> materialFor(const core::gltf::Asset& asset,
                                                  std::optional<std::size_t> materialIndex) {
      if (!materialIndex || *materialIndex >= asset.materials.size())
        return nullptr;

      const auto& source = asset.materials[*materialIndex];
      auto material = std::make_shared<render::MatteMaterial>(baseColorTextureFor(asset, source));
      material->setAmbientCoefficient(0.65);
      material->setDiffuseCoefficient(0.8);
      return material;
    }

    std::shared_ptr<render::Primitive> primitiveFor(const core::gltf::Asset& asset,
                                                    const core::gltf::MeshPrimitive& primitive) {
      if (primitive.mode != 4)
        return nullptr;

      const auto position = primitive.attributes.find("POSITION");
      if (position == primitive.attributes.end())
        throw std::runtime_error("glTF mesh primitive is missing POSITION");

      const std::vector<Vector3d> positions = readVec3Accessor(asset, position->second, "POSITION");

      std::vector<Vector3d> normals(positions.size(), Vector3d::null);
      if (const auto normal = primitive.attributes.find("NORMAL");
          normal != primitive.attributes.end()) {
        normals = readVec3Accessor(asset, normal->second, "NORMAL");
        if (normals.size() != positions.size())
          throw std::runtime_error("glTF NORMAL accessor count must match POSITION");
      }

      std::vector<Vector2d> texcoords(positions.size(), Vector2d::null);
      if (const auto uv = primitive.attributes.find("TEXCOORD_0");
          uv != primitive.attributes.end()) {
        texcoords = readVec2Accessor(asset, uv->second, "TEXCOORD_0");
        if (texcoords.size() != positions.size())
          throw std::runtime_error("glTF TEXCOORD_0 accessor count must match POSITION");
      }

      ::Mesh mesh;
      for (std::size_t i = 0; i < positions.size(); ++i)
        mesh.addVertex(positions[i], normals[i], texcoords[i]);

      std::vector<int> indices;
      if (primitive.indices) {
        indices = readIndexAccessor(asset, *primitive.indices);
      } else {
        indices.reserve(positions.size());
        for (std::size_t i = 0; i < positions.size(); ++i) {
          if (i > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            throw std::runtime_error("glTF vertex index is too large for Mesh");
          indices.push_back(static_cast<int>(i));
        }
      }

      for (std::size_t i = 0; i + 2 < indices.size(); i += 3)
        mesh.addFace({indices[i], indices[i + 1], indices[i + 2]});

      const bool hasNormals = primitive.attributes.find("NORMAL") != primitive.attributes.end();
      if (!hasNormals)
        mesh.computeNormals();

      auto material = materialFor(asset, primitive.material);
      if (material) {
        render::MeshPrimitive::FaceMaterials faceMaterials(mesh.faces().size(), material);
        return std::make_shared<render::MeshPrimitive>(std::move(mesh), std::move(faceMaterials),
                                                       render::MeshPrimitive::NormalMode::Smooth);
      }

      return std::make_shared<render::MeshPrimitive>(std::move(mesh),
                                                     render::MeshPrimitive::NormalMode::Smooth);
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

      [[nodiscard]] const std::vector<ImportDiagnostic>& diagnostics() const {
        return m_diagnostics;
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

        addNodePayload(*rawGroup, nodeIndex, global);

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

      void addNodePayload(Group& group, std::size_t nodeIndex, const Matrix4d& global) {
        const auto& node = m_asset.nodes[nodeIndex];
        if (node.mesh && *node.mesh < m_asset.meshes.size())
          addMesh(group, *node.mesh);
        if (node.camera && *node.camera < m_asset.cameras.size())
          addCamera(group, nodeIndex, *node.camera, global);
        if (node.punctualLight && *node.punctualLight < m_asset.punctualLights.size())
          addPunctualLight(group, *node.punctualLight);
      }

      void addMesh(Group& parent, std::size_t meshIndex) {
        const auto& mesh = m_asset.meshes[meshIndex];
        for (std::size_t i = 0; i < mesh.primitives.size(); ++i) {
          auto primitive = primitiveFor(m_asset, mesh.primitives[i]);
          if (!primitive)
            continue;

          int triangleCount = 0;
          if (auto meshPrimitive = std::dynamic_pointer_cast<render::MeshPrimitive>(primitive))
            triangleCount = static_cast<int>(meshPrimitive->mesh()->faces().size());
          auto compiled = std::make_unique<CompiledPrimitive>(std::move(primitive));
          compiled->setId(QString("gltf-mesh-%1-primitive-%2").arg(meshIndex).arg(i));
          compiled->setName(mesh.name.empty()
                              ? QString("glTF Mesh %1 Primitive %2").arg(meshIndex).arg(i)
                              : QString::fromStdString(mesh.name));
          compiled->setMetadata(baseMetadata(m_sourcePath, sourceId("meshes", meshIndex), "mesh",
                                             static_cast<int>(meshIndex)));
          compiled->setMetadataValue("gltfPrimitiveIndex", static_cast<int>(i));
          compiled->setMetadataValue("gltfTriangleCount", triangleCount);
          parent.addChild(std::move(compiled));
        }
      }

      void addCamera(Group& group, std::size_t nodeIndex, std::size_t cameraIndex,
                     const Matrix4d& global) {
        const auto& camera = m_asset.cameras[cameraIndex];
        const Vector3d position = global.translationVector();
        const Vector3d forward = normalizedDirectionOrDefault(global, Vector3d(0.0, 0.0, -1.0));
        const Vector3d target = position + forward;
        const QString fallbackName = QString("glTF Camera %1").arg(cameraIndex);
        const QString name =
          camera.name.empty() ? fallbackName : QString::fromStdString(camera.name);
        const QString id = sourceId("cameras", cameraIndex);

        std::unique_ptr<Camera> worldCamera;
        if (camera.type == core::gltf::CameraType::Perspective) {
          auto pinhole = std::make_unique<PinholeCamera>();
          pinhole->setDistance(5.0);
          if (camera.perspective.yfov > 0.0 && std::isfinite(camera.perspective.yfov)) {
            pinhole->setZoom(3.0 / (pinhole->distance() * std::tan(camera.perspective.yfov / 2.0)));
          }
          worldCamera = std::move(pinhole);
        } else {
          auto orthographic = std::make_unique<OrthographicCamera>();
          if (camera.orthographic.ymag > 0.0 && std::isfinite(camera.orthographic.ymag)) {
            orthographic->setZoom(6.0 / camera.orthographic.ymag);
          }
          worldCamera = std::move(orthographic);
        }

        worldCamera->setId(QString("%1/camera").arg(sourceId("nodes", nodeIndex)));
        worldCamera->setName(name);
        worldCamera->setPosition(position);
        worldCamera->setTarget(target);
        worldCamera->setMetadata(
          baseMetadata(m_sourcePath, id, "camera", static_cast<int>(cameraIndex)));
        if (camera.type == core::gltf::CameraType::Perspective) {
          worldCamera->setMetadataValue("gltfCameraType", QStringLiteral("perspective"));
          worldCamera->setMetadataValue("gltfYfov", camera.perspective.yfov);
          worldCamera->setMetadataValue("gltfZNear", camera.perspective.znear);
          if (camera.perspective.aspectRatio)
            worldCamera->setMetadataValue("gltfAspectRatio", *camera.perspective.aspectRatio);
          if (camera.perspective.zfar)
            worldCamera->setMetadataValue("gltfZFar", *camera.perspective.zfar);
        } else {
          worldCamera->setMetadataValue("gltfCameraType", QStringLiteral("orthographic"));
          worldCamera->setMetadataValue("gltfXmag", camera.orthographic.xmag);
          worldCamera->setMetadataValue("gltfYmag", camera.orthographic.ymag);
          worldCamera->setMetadataValue("gltfZNear", camera.orthographic.znear);
          worldCamera->setMetadataValue("gltfZFar", camera.orthographic.zfar);
        }
        attachElementProvenance(*worldCamera, m_sourcePath, id, "camera");
        group.addChild(std::move(worldCamera));
      }

      void addPunctualLight(Group& group, std::size_t lightIndex) {
        const auto& light = m_asset.punctualLights[lightIndex];
        if (light.type == core::gltf::PunctualLightType::Spot) {
          m_diagnostics.push_back(ImportDiagnostic::warning(
            QString("glTF KHR_lights_punctual spot light %1 is unsupported").arg(lightIndex),
            m_sourcePath));
          return;
        }

        const QString fallbackName = QString("glTF Light %1").arg(lightIndex);
        const QString name = light.name.empty() ? fallbackName : QString::fromStdString(light.name);
        const QString id = sourceId("extensions/KHR_lights_punctual/lights", lightIndex);

        std::unique_ptr<Light> worldLight;
        if (light.type == core::gltf::PunctualLightType::Directional) {
          auto directional = std::make_unique<DirectionalLight>();
          directional->setDirection(Vector3d(0.0, 0.0, -1.0));
          worldLight = std::move(directional);
        } else {
          auto point = std::make_unique<PointLight>();
          worldLight = std::move(point);
        }

        worldLight->setId(id);
        worldLight->setName(name);
        worldLight->setColor(Colord(light.color[0], light.color[1], light.color[2]));
        worldLight->setIntensity(light.intensity);
        worldLight->setMetadata(
          baseMetadata(m_sourcePath, id, "light", static_cast<int>(lightIndex)));
        worldLight->setMetadataValue("gltfLightType",
                                     light.type == core::gltf::PunctualLightType::Directional
                                       ? QStringLiteral("directional")
                                       : QStringLiteral("point"));
        worldLight->setMetadataValue("gltfColor", jsonArrayFor(light.color));
        if (light.range) {
          worldLight->setMetadataValue("gltfRange", *light.range);
          m_diagnostics.push_back(ImportDiagnostic::warning(
            QString("glTF KHR_lights_punctual light %1 range is preserved as metadata only")
              .arg(lightIndex),
            m_sourcePath));
        }
        attachElementProvenance(*worldLight, m_sourcePath, id, "light");
        group.addChild(std::move(worldLight));
      }

      const core::gltf::Asset& m_asset;
      QString m_sourcePath;
      bool m_preserveHierarchy;
      std::vector<bool> m_active;
      std::vector<std::vector<QString>> m_nodeTargetIds;
      std::vector<ImportDiagnostic> m_diagnostics;
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
      {"background_color",
       ImportOptionType::String,
       "Background color",
       "Scene background as a CSS color name or hex color when importing a standalone glTF file.",
       "white",
       false,
       {}},
      {"ambient_color",
       ImportOptionType::String,
       "Ambient color",
       "Scene ambient fill light as a CSS color name or hex color when importing a standalone glTF "
       "file.",
       "#cccccc",
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
    std::vector<ImportDiagnostic> compilerDiagnostics;
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
      compilerDiagnostics = compiler.diagnostics();
    } catch (const std::exception& error) {
      return ImportResult::failed({ImportDiagnostic::error(error.what(), filename)}, source);
    }

    if (timeline) {
      auto scene = std::make_unique<Scene>(nullptr);
      scene->setName(root->name());
      scene->setMetadata(root->metadata());
      Element* importedRoot = root.get();
      scene->addChild(std::move(root));
      scene->setAnimation(std::move(timeline));
      if (importedRoot)
        configureImportedScene(*scene, *importedRoot, options);
      result.setRoot(std::move(scene));
    } else {
      result.setRoot(std::move(root));
    }
    appendDiagnostics(result, readResult.diagnostics, filename);
    appendMaterialDiagnostics(result, *readResult.asset, filename);
    for (const ImportDiagnostic& diagnostic : compilerDiagnostics)
      result.addDiagnostic(diagnostic);
    return result;
  }

  bool GltfSceneImporter::configureImportedRoot(Element& importedRoot,
                                                const ImportOptions& options) const {
    (void)options;
    orientImportedRoot(importedRoot);
    return true;
  }

  bool GltfSceneImporter::configureImportedScene(Scene& scene, Element& importedRoot,
                                                 const ImportOptions& options) const {
    const ImportedSceneDefaults defaults = importedSceneDefaults(options);
    defaults.applyTo(scene);
    configureImportedRoot(importedRoot, options);
    (void)defaults.frameCamera(scene);
    return true;
  }

  ImportedSceneDefaults
  GltfSceneImporter::importedSceneDefaults(const ImportOptions& options) const {
    ImportedSceneDefaults defaults;
    defaults.setBackgroundColorFromOption(options, "background_color");
    defaults.setAmbientColorFromOption(options, "ambient_color");
    defaults.setCameraDirection(Vector3d(0.0, 0.0, 1.0));
    return defaults;
  }

  void GltfSceneImporter::orientImportedRoot(Element& importedRoot) const {
    auto* transformable = qobject_cast<Transformable*>(&importedRoot);
    if (!transformable)
      return;
    if (transformable->metadataValue("coordinateConversion").toString() ==
        QStringLiteral("gltf_y_up_to_product_view_up")) {
      return;
    }

    const double pi = std::acos(-1.0);
    transformable->setRotation(transformable->rotation() + Vector3d(0.0, 0.0, pi));
    transformable->setMetadataValue("coordinateConversion", "gltf_y_up_to_product_view_up");
  }

}

static bool dummy =
  world::SceneImporterRegistry::self().registerClass<world::GltfSceneImporter>("gltf");
