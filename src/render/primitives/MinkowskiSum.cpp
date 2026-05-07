#include "raytracer/State.h"
#include "render/primitives/MinkowskiSum.h"
#include "core/geometry/Mesh.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"
#include <QDebug>

using namespace render;

std::shared_ptr<Mesh> MinkowskiSum::tessellate(int) const {
  qWarning() << "MinkowskiSum::tessellate not implemented — CSG mesh booleans queued under roadmap §4.2.a.";
  return std::make_shared<Mesh>();
}

BoundingBoxd MinkowskiSum::calculateBoundingBox() const {
  if (primitives().size() > 0) {
    Primitives::const_iterator it = primitives().begin();
    BoundingBoxd bbox = (*it++)->boundingBox();
    for (; it != primitives().end(); ++it) {
      bbox = bbox + (*it)->boundingBox();
    }
    return bbox;
  } else {
    return BoundingBoxd();
  }
}

Vector3d MinkowskiSum::farthestPoint(const Vector3d& direction) const {
  Vector3d result;
  for (const auto& primitive : primitives()) {
    result += primitive->farthestPoint(direction);
  }
  return result;
}
