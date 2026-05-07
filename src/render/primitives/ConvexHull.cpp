#include "raytracer/State.h"
#include "render/primitives/ConvexHull.h"
#include "core/geometry/Mesh.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"
#include <QDebug>

#include <map>

using namespace render;

std::shared_ptr<Mesh> ConvexHull::tessellate(int) const {
  qWarning() << "ConvexHull::tessellate not implemented — CSG mesh booleans queued under roadmap §4.2.a.";
  return std::make_shared<Mesh>();
}

Vector3d ConvexHull::farthestPoint(const Vector3d& direction) const {
  Rayd ray(Vector3d::null(), direction);

  std::map<double, Vector3d> points;
  for (const auto& primitive : primitives()) {
    Vector3d point = primitive->farthestPoint(direction);
    double distance = ray.projectedDistance(point);
    points[distance] = point;
  }
  return points.rbegin()->second;
}
