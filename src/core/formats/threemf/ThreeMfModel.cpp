#include "core/formats/threemf/ThreeMfModel.h"

#include <QXmlStreamReader>

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>

namespace core::threemf {
  namespace {
    QString localName(const QXmlStreamReader& xml) {
      return xml.name().toString();
    }

    QString attribute(const QXmlStreamReader& xml, const QString& name) {
      return xml.attributes().value(name).toString();
    }

    int intAttribute(const QXmlStreamReader& xml, const QString& name, int fallback = 0) {
      bool ok = false;
      const int value = attribute(xml, name).toInt(&ok);
      return ok ? value : fallback;
    }

    double doubleAttribute(const QXmlStreamReader& xml, const QString& name,
                           double fallback = 0.0) {
      bool ok = false;
      const double value = attribute(xml, name).toDouble(&ok);
      return ok && std::isfinite(value) ? value : fallback;
    }

    Unit unitFromString(QString value) {
      value = value.trimmed().toLower();
      if (value == "micron")
        return Unit::Micron;
      if (value == "centimeter")
        return Unit::Centimeter;
      if (value == "inch")
        return Unit::Inch;
      if (value == "foot")
        return Unit::Foot;
      if (value == "meter")
        return Unit::Meter;
      return Unit::Millimeter;
    }

    QString colorString(QString value) {
      value = value.trimmed();
      if (value.startsWith('#'))
        value.remove(0, 1);
      return value;
    }

    int hexByte(const QString& value, int offset, int fallback) {
      if (offset + 2 > value.size())
        return fallback;
      bool ok = false;
      const int byte = value.mid(offset, 2).toInt(&ok, 16);
      return ok ? byte : fallback;
    }

    Colord colorFromDisplayColor(const QString& displayColor) {
      const QString color = colorString(displayColor);
      if (color.size() != 6 && color.size() != 8)
        return Colord::white();

      return Colord(hexByte(color, 0, 255) / 255.0, hexByte(color, 2, 255) / 255.0,
                    hexByte(color, 4, 255) / 255.0);
    }

    Matrix4d transformFromString(const QString& transform) {
      QStringList fields = transform.simplified().split(' ', Qt::SkipEmptyParts);
      if (fields.empty())
        return Matrix4d();
      if (fields.size() != 12)
        throw ThreeMfModelError("3MF build item transform must contain 12 numbers");

      std::array<double, 12> values{};
      for (int i = 0; i != fields.size(); ++i) {
        bool ok = false;
        values[i] = fields[i].toDouble(&ok);
        if (!ok || !std::isfinite(values[i]))
          throw ThreeMfModelError("3MF build item transform contains a non-numeric value");
      }

      // 3MF stores affine transforms as row-major 4x3 matrices. The final row
      // contains translation, while this renderer uses a 4x4 column-translation
      // affine matrix for point transforms.
      return Matrix4d(values[0], values[1], values[2], values[9], values[3], values[4], values[5],
                      values[10], values[6], values[7], values[8], values[11], 0.0, 0.0, 0.0, 1.0);
    }

    std::optional<MaterialResource> triangleMaterial(const QXmlStreamReader& xml,
                                                     const Model& model) {
      const int pid = intAttribute(xml, "pid", -1);
      const int p1 = intAttribute(xml, "p1", -1);
      if (pid < 0 || p1 < 0)
        return std::nullopt;

      const auto found = model.materials.find({pid, p1});
      if (found == model.materials.end())
        return std::nullopt;
      return found->second;
    }

    MaterialResource materialFromBase(const QXmlStreamReader& xml, int resourceId, int index) {
      MaterialResource material;
      material.id = resourceId;
      material.index = index;
      material.name = attribute(xml, "name");
      material.color = colorFromDisplayColor(attribute(xml, "displaycolor"));
      return material;
    }

    void addTriangle(ObjectMesh& object, const QXmlStreamReader& xml,
                     const std::vector<Vector3d>& vertices, const Model& model) {
      const int v1 = intAttribute(xml, "v1", -1);
      const int v2 = intAttribute(xml, "v2", -1);
      const int v3 = intAttribute(xml, "v3", -1);
      if (v1 < 0 || v2 < 0 || v3 < 0 || v1 >= static_cast<int>(vertices.size()) ||
          v2 >= static_cast<int>(vertices.size()) || v3 >= static_cast<int>(vertices.size())) {
        throw ThreeMfModelError("3MF triangle references an invalid vertex index");
      }

      object.mesh.addFace({v1, v2, v3});
      object.faceMaterials.push_back(triangleMaterial(xml, model));
    }
  }

  ThreeMfModelError::ThreeMfModelError(const std::string& message)
      : std::runtime_error(message) {
  }

  double Model::unitScaleInMeters() const {
    switch (unit) {
    case Unit::Micron:
      return 0.000001;
    case Unit::Millimeter:
      return 0.001;
    case Unit::Centimeter:
      return 0.01;
    case Unit::Inch:
      return 0.0254;
    case Unit::Foot:
      return 0.3048;
    case Unit::Meter:
      return 1.0;
    }
    return 0.001;
  }

  QString Model::unitName() const {
    switch (unit) {
    case Unit::Micron:
      return "micron";
    case Unit::Millimeter:
      return "millimeter";
    case Unit::Centimeter:
      return "centimeter";
    case Unit::Inch:
      return "inch";
    case Unit::Foot:
      return "foot";
    case Unit::Meter:
      return "meter";
    }
    return "millimeter";
  }

  Model ThreeMfModelParser::parse(const QByteArray& xmlBytes) const {
    QXmlStreamReader xml(xmlBytes);
    Model model;

    int baseMaterialsId = -1;
    int baseMaterialIndex = 0;
    bool inVertices = false;
    bool inBuild = false;
    std::optional<ObjectMesh> object;
    std::vector<Vector3d> vertices;

    while (!xml.atEnd()) {
      xml.readNext();
      if (xml.isStartElement()) {
        const QString name = localName(xml);
        if (name == "model") {
          model.unit = unitFromString(attribute(xml, "unit"));
        } else if (name == "basematerials") {
          baseMaterialsId = intAttribute(xml, "id", -1);
          baseMaterialIndex = 0;
        } else if (name == "base" && baseMaterialsId >= 0) {
          auto material = materialFromBase(xml, baseMaterialsId, baseMaterialIndex++);
          model.materials[{material.id, material.index}] = material;
        } else if (name == "object") {
          object = ObjectMesh();
          object->id = intAttribute(xml, "id");
          object->name = attribute(xml, "name");
          vertices.clear();
        } else if (name == "vertices") {
          inVertices = true;
        } else if (name == "vertex" && inVertices) {
          vertices.emplace_back(doubleAttribute(xml, "x"), doubleAttribute(xml, "y"),
                                doubleAttribute(xml, "z"));
        } else if (name == "triangles" && object) {
          for (const auto& vertex : vertices)
            object->mesh.addVertex(vertex, Vector3d::null);
        } else if (name == "triangle" && object) {
          addTriangle(*object, xml, vertices, model);
        } else if (name == "build") {
          inBuild = true;
        } else if (name == "item" && inBuild) {
          BuildItem item;
          item.objectId = intAttribute(xml, "objectid");
          item.transform = transformFromString(attribute(xml, "transform"));
          model.buildItems.push_back(item);
        }
      } else if (xml.isEndElement()) {
        const QString name = localName(xml);
        if (name == "basematerials") {
          baseMaterialsId = -1;
        } else if (name == "vertices") {
          inVertices = false;
        } else if (name == "object" && object) {
          if (!object->mesh.faces().empty()) {
            object->mesh.computeNormals();
            model.objects[object->id] = std::move(*object);
          }
          object.reset();
          vertices.clear();
        } else if (name == "build") {
          inBuild = false;
        }
      }
    }

    if (xml.hasError()) {
      std::ostringstream message;
      message << "Unable to parse 3MF model XML: " << xml.errorString().toStdString();
      throw ThreeMfModelError(message.str());
    }

    return model;
  }

}
