#include "render/State.h"
#include "render/primitives/Primitive.h"
#include "core/geometry/Mesh.h"
#include "core/math/Ray.h"
#include "core/math/RayPacket.h"
#include "core/math/HitPointInterval.h"
#include "core/math/GJKSimplex.h"

#include <iostream>

using namespace render;

namespace {
  template<typename Packet, typename Result>
  Result intersectPacketScalarFallback(const Primitive& primitive, const Packet& rays, render::State& state) {
    Result result;
    for (std::size_t lane = 0; lane != Packet::lanes; ++lane) {
      HitPointInterval hitPoints;
      if (primitive.intersect(rays.rayd(lane), hitPoints, state)) {
        result.setHit(lane,
                      static_cast<float>(hitPoints.min().distance()),
                      static_cast<float>(hitPoints.max().distance()));
      }
    }
    return result;
  }
}

bool Primitive::intersects(const Rayd& ray, render::State& state) const {
  HitPointInterval hitPoints;
  intersect(ray, hitPoints, state);
  return !hitPoints.minWithPositiveDistance().isUndefined();
}

RayPacketIntersection4 Primitive::intersectPacket(const Ray4& rays, render::State& state) const {
  return intersectPacketScalarFallback<Ray4, RayPacketIntersection4>(*this, rays, state);
}

RayPacketIntersection8 Primitive::intersectPacket(const Ray8& rays, render::State& state) const {
  return intersectPacketScalarFallback<Ray8, RayPacketIntersection8>(*this, rays, state);
}

void Primitive::forEachLeaf(std::shared_ptr<render::Material> inheritedMaterial,
                            const LeafVisitor& visitor) const {
  auto own = material();
  visitor(this, own ? own : inheritedMaterial);
}

void Primitive::forEachLeafInBounds(const BoundsFilter&,
                                    std::shared_ptr<render::Material> inheritedMaterial,
                                    const LeafVisitor& visitor) const {
  forEachLeaf(inheritedMaterial, visitor);
}

Vector3d Primitive::farthestPoint(const Vector3d&) const {
  return Vector3d::undefined;
}

std::shared_ptr<Mesh> Primitive::tessellate(int) const {
  // Loud-but-non-fatal default. A subclass that genuinely can't be
  // tessellated (the infinite Plane, CSG operations awaiting the
  // mesh-boolean epic) overrides this to return an empty mesh
  // silently; primitives that *should* tessellate but haven't been
  // implemented yet hit this default and surface in the warning
  // stream so the gap is visible to anyone running an engine that
  // depends on it.
  std::cerr << "Primitive::tessellate: " << name() << " (or its concrete type) "
            << "did not override tessellate; returning empty mesh.\n";
  return std::make_shared<Mesh>();
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
    normal = Vector3d::null;
  }
  hitPoints.add(HitPoint(this, t, pointB, normal));
  return true;
}
