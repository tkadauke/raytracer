#include "world/objects/Curve.h"
#include "world/objects/ElementFactory.h"
#include "core/geometry/Polyline.h"
#include "render/primitives/Curve.h"

#include <cmath>

Curve::Curve(Element* parent)
    : Surface(parent),
      m_width(0.1),
      m_tessellationMode("ribbon") {
}

QVariantList Curve::points() const {
  QVariantList result;
  for (const auto& point : m_points) {
    result.append(QVariant::fromValue(QVariantList({point.x(), point.y(), point.z()})));
  }
  return result;
}

void Curve::setPoints(const QVariantList& points) {
  m_points.clear();
  for (const auto& pointValue : points) {
    const auto point = pointValue.toList();
    if (point.size() < 3)
      continue;

    m_points.emplace_back(point[0].toDouble(), point[1].toDouble(), point[2].toDouble());
  }
}

void Curve::setWidth(double width) {
  m_width = std::abs(width);
}

void Curve::setTessellationMode(const QString& mode) {
  if (mode.compare("tube", Qt::CaseInsensitive) == 0) {
    m_tessellationMode = "tube";
  } else {
    m_tessellationMode = "ribbon";
  }
}

std::shared_ptr<render::Primitive> Curve::toRaytracerPrimitive() const {
  const auto mode = m_tessellationMode == "tube" ? render::Curve::TessellationMode::Tube
                                                 : render::Curve::TessellationMode::Ribbon;
  return make_named<render::Curve>(core::Polyline(m_points), m_width, mode);
}

static bool dummy = ElementFactory::self().registerClass<Curve>("Curve");
