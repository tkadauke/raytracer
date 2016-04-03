#include "raytracer/State.h"
#include "raytracer/primitives/ConvexHull.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"

#include <map>

using namespace raytracer;

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
