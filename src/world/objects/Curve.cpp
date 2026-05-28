#include "world/objects/Curve.h"

#include "world/objects/ElementFactory.h"

#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>

namespace {
  QJsonArray vectorToJson(const Vector3d& vector) {
    return QJsonArray({vector.x(), vector.y(), vector.z()});
  }

  Vector3d vectorFromJson(const QJsonValue& value) {
    const auto array = value.toArray();
    return Vector3d(array[0].toDouble(), array[1].toDouble(), array[2].toDouble());
  }

  QJsonValue attributeToJson(const core::Curve::AttributeValue& value) {
    if (const auto* boolValue = std::get_if<bool>(&value))
      return *boolValue;
    if (const auto* intValue = std::get_if<int>(&value))
      return *intValue;
    if (const auto* doubleValue = std::get_if<double>(&value))
      return *doubleValue;
    if (const auto* stringValue = std::get_if<std::string>(&value))
      return QString::fromStdString(*stringValue);
    if (const auto* vectorValue = std::get_if<Vector3d>(&value))
      return vectorToJson(*vectorValue);
    return QJsonValue();
  }

  core::Curve::AttributeValue attributeFromJson(const QJsonValue& value) {
    if (value.isBool())
      return value.toBool();
    if (value.isDouble()) {
      const double numeric = value.toDouble();
      const int integer = value.toInt();
      if (numeric == static_cast<double>(integer))
        return integer;
      return numeric;
    }
    if (value.isArray())
      return vectorFromJson(value);
    return value.toString().toStdString();
  }

  QJsonObject attributesToJson(const core::Curve::AttributeMap& attributes) {
    QJsonObject result;
    for (const auto& [name, value] : attributes)
      result[QString::fromStdString(name)] = attributeToJson(value);
    return result;
  }

  void readAttributes(core::Polyline& polyline, const QJsonObject& attributes) {
    for (auto it = attributes.begin(); it != attributes.end(); ++it)
      polyline.setAttribute(it.key().toStdString(), attributeFromJson(it.value()));
  }

  void readSegmentAttributes(core::Polyline& polyline, std::size_t segmentIndex,
                             const QJsonObject& attributes) {
    if (segmentIndex >= polyline.segmentCount())
      return;

    for (auto it = attributes.begin(); it != attributes.end(); ++it)
      polyline.setSegmentAttribute(segmentIndex, it.key().toStdString(),
                                   attributeFromJson(it.value()));
  }
}

Curve::Curve(Element* parent)
    : Surface(parent),
      m_width(0.0),
      m_tessellationMode(QStringLiteral("ribbon")) {
}

const core::Polyline& Curve::polyline() const {
  return m_polyline;
}

void Curve::setPolyline(const core::Polyline& polyline) {
  m_polyline = polyline;
}

double Curve::width() const {
  return m_width;
}

void Curve::setWidth(double width) {
  m_width = std::max(0.0, width);
}

QString Curve::tessellationMode() const {
  return m_tessellationMode;
}

void Curve::setTessellationMode(const QString& mode) {
  const QString normalized = mode.toLower();
  m_tessellationMode =
    normalized == QStringLiteral("tube") ? QStringLiteral("tube") : QStringLiteral("ribbon");
}

void Curve::read(const QJsonObject& json) {
  QJsonObject surfaceJson = json;
  surfaceJson.remove("points");
  surfaceJson.remove("curveAttributes");
  surfaceJson.remove("segmentAttributes");
  Surface::read(surfaceJson);

  std::vector<Vector3d> points;
  for (const auto& pointValue : json["points"].toArray())
    points.push_back(vectorFromJson(pointValue));

  core::Polyline polyline(points);
  readAttributes(polyline, json["curveAttributes"].toObject());

  const auto segmentAttributes = json["segmentAttributes"].toArray();
  for (int i = 0; i != segmentAttributes.size(); ++i)
    readSegmentAttributes(polyline, static_cast<std::size_t>(i), segmentAttributes[i].toObject());

  setPolyline(polyline);
}

void Curve::write(QJsonObject& json) {
  Surface::write(json);

  QJsonArray points;
  for (const auto& point : m_polyline.points())
    points.append(vectorToJson(point));
  json["points"] = points;

  const auto curveAttributes = attributesToJson(m_polyline.attributes());
  if (!curveAttributes.isEmpty())
    json["curveAttributes"] = curveAttributes;

  QJsonArray segmentAttributes;
  for (std::size_t i = 0; i != m_polyline.segmentCount(); ++i)
    segmentAttributes.append(attributesToJson(m_polyline.segmentAttributes(i)));
  if (!segmentAttributes.isEmpty())
    json["segmentAttributes"] = segmentAttributes;
}

std::shared_ptr<render::Primitive> Curve::toRaytracerPrimitive() const {
  return make_named<render::Curve>(m_polyline, m_width, renderMode());
}

render::Curve::TessellationMode Curve::renderMode() const {
  if (m_tessellationMode == QStringLiteral("tube"))
    return render::Curve::TessellationMode::Tube;
  return render::Curve::TessellationMode::Ribbon;
}

static bool dummy = ElementFactory::self().registerClass<Curve>("Curve");
