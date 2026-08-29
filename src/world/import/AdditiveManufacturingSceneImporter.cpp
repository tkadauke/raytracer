#include "world/import/AdditiveManufacturingSceneImporter.h"

#include "core/formats/BinaryRead.h"
#include "core/geometry/Mesh.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/MeshPrimitive.h"
#include "render/textures/ConstantColorTexture.h"
#include "world/import/ImportedSceneDefaults.h"
#include "world/import/SceneImporterRegistry.h"
#include "world/objects/CompiledPrimitive.h"
#include "world/objects/Group.h"
#include "world/objects/Scene.h"

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QRegularExpression>
#include <QXmlStreamReader>

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

namespace {
  using core::formats::readUint16Le;
  using core::formats::readUint32Le;
  using core::formats::readVector3fLe;

  constexpr std::uint32_t ZipLocalFileHeaderSignature = 0x04034b50;
  constexpr std::uint16_t ZipStored = 0;

  struct MeshDocument {
    Mesh mesh;
    QString formatName;
    QJsonObject properties;
  };

  QByteArray readFile(const QString& filename) {
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
      throw std::runtime_error("unable to read additive manufacturing source");
    }
    return file.readAll();
  }

  Vector3d normalFor(const Vector3d& a, const Vector3d& b, const Vector3d& c) {
    const Vector3d normal = (b - a) ^ (c - a);
    const double length = normal.length();
    return length > 0.0 ? normal / length : Vector3d(0.0, 0.0, 1.0);
  }

  void addTriangle(Mesh& mesh, const Vector3d& a, const Vector3d& b, const Vector3d& c,
                   std::optional<Vector3d> suppliedNormal = std::nullopt) {
    const Vector3d normal = suppliedNormal.value_or(normalFor(a, b, c));
    const int base = static_cast<int>(mesh.vertices().size());
    mesh.addVertex(a, normal);
    mesh.addVertex(b, normal);
    mesh.addVertex(c, normal);
    mesh.addFace({base, base + 1, base + 2});
  }

  double attributeDouble(const QXmlStreamAttributes& attributes, const QString& name) {
    bool ok = false;
    const double value = attributes.value(name).toDouble(&ok);
    if (!ok) {
      throw std::runtime_error("3MF vertex attribute is not numeric");
    }
    return value;
  }

  int attributeInt(const QXmlStreamAttributes& attributes, const QString& name) {
    bool ok = false;
    const int value = attributes.value(name).toInt(&ok);
    if (!ok) {
      throw std::runtime_error("3MF triangle index attribute is not numeric");
    }
    return value;
  }

  Mesh parse3MfModelXml(const QByteArray& xml) {
    QXmlStreamReader reader(xml);
    std::vector<Vector3d> vertices;
    Mesh mesh;

    while (!reader.atEnd()) {
      reader.readNext();
      if (!reader.isStartElement())
        continue;

      const QStringView name = reader.name();
      const auto attributes = reader.attributes();
      if (name == QStringLiteral("vertex")) {
        vertices.emplace_back(attributeDouble(attributes, QStringLiteral("x")),
                              attributeDouble(attributes, QStringLiteral("y")),
                              attributeDouble(attributes, QStringLiteral("z")));
      } else if (name == QStringLiteral("triangle")) {
        const int v1 = attributeInt(attributes, QStringLiteral("v1"));
        const int v2 = attributeInt(attributes, QStringLiteral("v2"));
        const int v3 = attributeInt(attributes, QStringLiteral("v3"));
        if (v1 < 0 || v2 < 0 || v3 < 0 || v1 >= static_cast<int>(vertices.size()) ||
            v2 >= static_cast<int>(vertices.size()) || v3 >= static_cast<int>(vertices.size())) {
          throw std::runtime_error("3MF triangle references a missing vertex");
        }
        addTriangle(mesh, vertices[v1], vertices[v2], vertices[v3]);
      }
    }

    if (reader.hasError()) {
      throw std::runtime_error(reader.errorString().toStdString());
    }
    if (mesh.faces().empty()) {
      throw std::runtime_error("3MF model did not contain any triangles");
    }
    return mesh;
  }

  QByteArray firstStored3MfModel(const QByteArray& package) {
    int offset = 0;
    while (offset + 30 <= package.size()) {
      if (readUint32Le(package, offset) != ZipLocalFileHeaderSignature)
        break;

      const std::uint16_t method = readUint16Le(package, offset + 8);
      const std::uint32_t compressedSize = readUint32Le(package, offset + 18);
      const std::uint16_t nameLength = readUint16Le(package, offset + 26);
      const std::uint16_t extraLength = readUint16Le(package, offset + 28);
      const int nameOffset = offset + 30;
      const int dataOffset = nameOffset + nameLength + extraLength;
      const int nextOffset = dataOffset + static_cast<int>(compressedSize);
      if (nameOffset + nameLength > package.size() || dataOffset > package.size() ||
          nextOffset > package.size()) {
        throw std::runtime_error("3MF package has a truncated local file header");
      }

      const QString name =
        QString::fromUtf8(package.constData() + nameOffset, static_cast<int>(nameLength));
      if (name.endsWith(QStringLiteral(".model"), Qt::CaseInsensitive)) {
        if (method != ZipStored) {
          throw std::runtime_error("compressed 3MF package entries are not supported");
        }
        return package.mid(dataOffset, static_cast<int>(compressedSize));
      }

      offset = nextOffset;
    }

    throw std::runtime_error("3MF package did not contain a model entry");
  }

  MeshDocument parse3Mf(const QByteArray& bytes) {
    const QByteArray xml = bytes.startsWith("PK\003\004") ? firstStored3MfModel(bytes) : bytes;
    MeshDocument document;
    document.mesh = parse3MfModelXml(xml);
    document.formatName = QStringLiteral("3MF model");
    document.properties = QJsonObject{{"sourceFormat", "3mf"}};
    return document;
  }

  Mesh parseAsciiStl(const QByteArray& bytes) {
    Mesh mesh;
    const QString text = QString::fromUtf8(bytes);
    QStringList tokens = text.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    std::optional<Vector3d> facetNormal;
    std::vector<Vector3d> facetVertices;

    for (int i = 0; i < tokens.size(); ++i) {
      const QString token = tokens[i].toLower();
      if (token == QStringLiteral("facet") && i + 4 < tokens.size() &&
          tokens[i + 1].toLower() == QStringLiteral("normal")) {
        bool okX = false;
        bool okY = false;
        bool okZ = false;
        const double x = tokens[i + 2].toDouble(&okX);
        const double y = tokens[i + 3].toDouble(&okY);
        const double z = tokens[i + 4].toDouble(&okZ);
        if (okX && okY && okZ)
          facetNormal = Vector3d(x, y, z);
        i += 4;
      } else if (token == QStringLiteral("vertex") && i + 3 < tokens.size()) {
        bool okX = false;
        bool okY = false;
        bool okZ = false;
        const double x = tokens[i + 1].toDouble(&okX);
        const double y = tokens[i + 2].toDouble(&okY);
        const double z = tokens[i + 3].toDouble(&okZ);
        if (!okX || !okY || !okZ) {
          throw std::runtime_error("STL vertex coordinate is not numeric");
        }
        facetVertices.emplace_back(x, y, z);
        if (facetVertices.size() == 3) {
          addTriangle(mesh, facetVertices[0], facetVertices[1], facetVertices[2], facetNormal);
          facetVertices.clear();
          facetNormal.reset();
        }
        i += 3;
      }
    }

    if (mesh.faces().empty()) {
      throw std::runtime_error("ASCII STL did not contain any facets");
    }
    return mesh;
  }

  Mesh parseBinaryStl(const QByteArray& bytes) {
    if (bytes.size() < 84) {
      throw std::runtime_error("binary STL is too short");
    }
    const std::uint32_t triangleCount = readUint32Le(bytes, 80);
    const std::uint64_t expectedSize = 84ull + static_cast<std::uint64_t>(triangleCount) * 50ull;
    if (expectedSize > static_cast<std::uint64_t>(bytes.size())) {
      throw std::runtime_error("binary STL triangle table is truncated");
    }

    Mesh mesh;
    int offset = 84;
    for (std::uint32_t triangle = 0; triangle != triangleCount; ++triangle) {
      const Vector3d normal = readVector3fLe(bytes, offset);
      const Vector3d a = readVector3fLe(bytes, offset + 12);
      const Vector3d b = readVector3fLe(bytes, offset + 24);
      const Vector3d c = readVector3fLe(bytes, offset + 36);
      addTriangle(mesh, a, b, c, normal);
      offset += 50;
    }

    if (mesh.faces().empty()) {
      throw std::runtime_error("binary STL did not contain any triangles");
    }
    return mesh;
  }

  MeshDocument parseStl(const QByteArray& bytes) {
    MeshDocument document;
    document.mesh = bytes.startsWith("solid") ? parseAsciiStl(bytes) : parseBinaryStl(bytes);
    document.formatName = QStringLiteral("STL mesh");
    document.properties = QJsonObject{{"sourceFormat", "stl"}};
    return document;
  }

  BoundingBoxd boundsFor(const Mesh& mesh) {
    BoundingBoxd bounds;
    for (const auto& vertex : mesh.vertices()) {
      bounds.include(vertex.point);
    }
    return bounds;
  }

  void addDefaultView(Scene& scene, const Mesh& mesh) {
    // Order matches world::BoundsFramedViewSpec's member declaration order:
    // fallbackSize, minDistanceFloor, distanceMultiplier, positionDirection,
    // zoom, cameraId, cameraName, lightId, lightName, lightDirection.
    world::addBoundsFramedCameraAndLight(
      scene, boundsFor(mesh),
      world::BoundsFramedViewSpec{
        Vector3d(1.0, 1.0, 1.0), 1.0, 3.0, Vector3d(0.0, -1.0, 0.65), 1.2,
        QStringLiteral("additive-camera"), QStringLiteral("Additive Manufacturing Camera"),
        QStringLiteral("additive-light"), QStringLiteral("Additive Manufacturing Light"),
        Vector3d(-0.4, -0.6, -1.0),
      });
  }

  std::shared_ptr<render::Primitive> primitiveFor(Mesh mesh) {
    auto material = std::make_shared<render::MatteMaterial>(
      std::make_shared<render::ConstantColorTexture>(Colord(0.86, 0.68, 0.36)));
    material->setAmbientCoefficient(0.65);
    material->setDiffuseCoefficient(0.8);

    auto primitive = std::make_shared<render::MeshPrimitive>(
      std::move(mesh), render::MeshPrimitive::NormalMode::Flat);
    primitive->setMaterial(std::move(material));
    return primitive;
  }

  MeshDocument parseDocument(const QString& filename, const QByteArray& bytes) {
    const QString extension = QFileInfo(filename).suffix().toLower();
    if (extension == QStringLiteral("stl"))
      return parseStl(bytes);
    if (extension == QStringLiteral("3mf"))
      return parse3Mf(bytes);
    throw std::runtime_error("unsupported additive manufacturing extension");
  }
}

namespace world {

  QString AdditiveManufacturingSceneImporter::name() const {
    return QStringLiteral("additive");
  }

  QStringList AdditiveManufacturingSceneImporter::supportedExtensions() const {
    return {QStringLiteral("stl"), QStringLiteral("3mf")};
  }

  ImportOptionSchemas AdditiveManufacturingSceneImporter::optionSchema() const {
    return {};
  }

  ImportResult AdditiveManufacturingSceneImporter::importFile(const QString& filename,
                                                              const ImportOptions&) const {
    ImportSourceMetadata source;
    source.importerName = name();
    source.sourcePath = filename;

    try {
      MeshDocument document = parseDocument(filename, readFile(filename));
      source.formatName = document.formatName;
      source.properties = document.properties;

      auto scene = std::make_unique<Scene>();
      scene->setName(QFileInfo(filename).baseName());
      scene->setAmbient(Colord(0.25, 0.25, 0.25));
      scene->setBackground(Colord(0.08, 0.1, 0.12));
      addDefaultView(*scene, document.mesh);

      auto root = std::make_unique<Group>();
      root->setName(QStringLiteral("Additive Manufacturing Model"));
      root->setMetadata(document.properties);
      root->setMetadataValue("sourcePath", filename);

      auto surface = std::make_unique<CompiledPrimitive>(primitiveFor(std::move(document.mesh)));
      surface->setName(QStringLiteral("Mesh body"));
      surface->setMetadata(document.properties);
      root->addChild(std::move(surface));

      scene->addChild(std::move(root));
      return ImportResult(std::move(scene), source);
    } catch (const std::exception& error) {
      return ImportResult::failed(
        {ImportDiagnostic::error(
          QString("Unable to import additive manufacturing source: %1").arg(error.what()),
          filename)},
        source);
    }
  }

}

static bool additiveManufacturingImporterRegistered =
  world::SceneImporterRegistry::self().registerClass<world::AdditiveManufacturingSceneImporter>(
    "additive");
