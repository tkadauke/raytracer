#include "raytracer/State.h"
#include "raytracer/primitives/Primitive.h"
#include "core/math/Ray.h"
#include "core/math/HitPointInterval.h"
#include "core/math/GJKSimplex.h"

using namespace raytracer;

bool Primitive::intersects(const Rayd& ray, State& state) const {
  HitPointInterval hitPoints;
  intersect(ray, hitPoints, state);
  return !hitPoints.minWithPositiveDistance().isUndefined();
}

Vector3d Primitive::farthestPoint(const Vector3d&) const {
  return Vector3d::undefined();
}

// G. v.d. Bergen. Ray Casting against General Convex Objects with Application
// to Continuous Collision Detection. 2004.
//
// The implementation of this method is borrowed from
// https://github.com/DanielChappuis/reactphysics3d
bool Primitive::convexIntersect(const Rayd& ray, HitPointInterval& hitPoints) const {
  const double squaredMachineEpsilon = std::numeric_limits<double>::epsilon() * std::numeric_limits<double>::epsilon();
  const double epsilon = 0.0000001;

  // If the points of the segment are two close, return no hit
  if (ray.direction().squaredLength() < squaredMachineEpsilon)
    return false;

  // Create a simplex set
  GJKSimplex simplex;

  Vector3d n;
  double t = 0.0;
  Vector3d lowerBound = ray.origin();
  Vector3d supportPoint = farthestPoint(ray.direction());
  Vector3d v = lowerBound - supportPoint;
  double squaredDistance = v.squaredLength();
  int i = 0;

  // GJK Algorithm loop
  while (squaredDistance > epsilon && i++ < 45) {
    // Compute the support points
    supportPoint = farthestPoint(v);
    Vector3d w = lowerBound - supportPoint;

    double vDotW = v * w;

    if (vDotW > 0.0) {
      double vDotR = v * ray.direction();

      if (vDotR >= -squaredMachineEpsilon) {
        return false;
      } else {
        // We have found a better lower bound for the hit point along the ray
        t = t - vDotW / vDotR;
        lowerBound = ray.at(t);
        w = lowerBound - supportPoint;
        n = v;
      }
    }

    // Add the new support point to the simplex
    if (!simplex.isPointInGJKSimplex(w) && !simplex.isFull()) {
      simplex.addPoint(w, lowerBound, supportPoint);
    }

    // Compute the closest point
    if (simplex.computeClosestPoint(v)) {
      squaredDistance = v.squaredLength();
    } else {
      squaredDistance = 0.0;
    }
  }

  // If the origin was inside the shape, we return no hit
  if (t < std::numeric_limits<double>::epsilon())
    return false;

  // Compute the closet points of both objects (without the margins)
  Vector3d pointA;
  Vector3d pointB;
  simplex.computeClosestPointsOfAandB(pointA, pointB);

  Vector3d normal;
  if (n.squaredLength() >= squaredMachineEpsilon) {
    // The normal vector is valid
    normal = n.normalized();
  } else {
    // Degenerated normal vector, we return a zero normal vector
    normal = Vector3d::null();
  }
  hitPoints.add(HitPoint(this, t, pointB, normal));
  return true;
}
