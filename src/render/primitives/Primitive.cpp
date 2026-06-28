#include "render/State.h"
#include "render/IntersectionSceneCompiler.h"
#include "render/materials/Material.h"
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
  Result intersectPacketScalarFallback(const Primitive& primitive, const Packet& rays,
                                       render::State& state) {
    Result result;
    for (std::size_t lane = 0; lane != Packet::lanes; ++lane) {
      HitPointInterval hitPoints;
      if (primitive.intersect(rays.rayd(lane), hitPoints, state)) {
        result.setHit(lane, static_cast<float>(hitPoints.min().distance()),
                      static_cast<float>(hitPoints.max().distance()));
      }
    }
    return result;
  }

  template<typename Packet, typename StateArray, typename Result>
  Result intersectPacketHitsScalarFallback(const Primitive& primitive, const Packet& rays,
                                           const StateArray& states) {
    Result result;
    for (std::size_t lane = 0; lane != Packet::lanes; ++lane) {
      if (!states[lane]) {
        continue;
      }
      State& state = *states[lane];
      state.packetHitScalarFallback(&primitive, "Primitive::intersectPacketHits");
      HitPointInterval hitPoints;
      const Primitive* hitPrimitive = primitive.intersect(rays.rayd(lane), hitPoints, state);
      if (!hitPrimitive) {
        continue;
      }

      const HitPoint hitPoint = hitPoints.minWithPositiveDistance();
      if (!hitPoint.isUndefined()) {
        result.setHit(lane, hitPrimitive, hitPoint, true);
      }
    }
    return result;
  }

  template<typename Packet, typename StateArray, typename Result>
  Result intersectPacketIntervalsScalarFallback(const Primitive& primitive, const Packet& rays,
                                                const StateArray& states) {
    Result result;
    for (std::size_t lane = 0; lane != Packet::lanes; ++lane) {
      if (!states[lane]) {
        continue;
      }
      State& state = *states[lane];
      state.packetHitScalarFallback(&primitive, "Primitive::intersectPacketIntervals");
      HitPointInterval hitPoints;
      const Primitive* hitPrimitive = primitive.intersect(rays.rayd(lane), hitPoints, state);
      if (hitPrimitive || !hitPoints.empty()) {
        result.setInterval(lane, hitPrimitive, hitPoints, true);
      }
    }
    return result;
  }
}

const Primitive* Primitive::TransformedLeaf::objectPrimitive() const {
  return object ? object : primitive;
}

Vector3d Primitive::TransformedLeaf::transformPoint(const Vector3d& point) const {
  return pointMatrix.transformPoint(point);
}

Vector3d Primitive::TransformedLeaf::transformNormal(const Vector3d& normal) const {
  return normalMatrix * normal;
}

BoundingBoxd Primitive::TransformedLeaf::boundingBox() const {
  const BoundingBoxd& bounds = primitive->boundingBox();
  if (!bounds.isValid() || bounds.isUndefined() || bounds.isInfinite()) {
    return bounds;
  }

  BoundingBoxd result;
  for (const Vector3d& vertex : bounds.vertices()) {
    const Vector3d transformed = transformPoint(vertex);
    result.include(transformed);
    if (motionDelta != Vector3d::null) {
      result.include(transformed + motionDelta);
    }
  }
  return result;
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

PrimitivePacketHit4 Primitive::intersectPacketHits(const Ray4& rays,
                                                   const PrimitivePacketState4& states) const {
  return intersectPacketHitsScalarFallback<Ray4, PrimitivePacketState4, PrimitivePacketHit4>(
    *this, rays, states);
}

PrimitivePacketHit8 Primitive::intersectPacketHits(const Ray8& rays,
                                                   const PrimitivePacketState8& states) const {
  return intersectPacketHitsScalarFallback<Ray8, PrimitivePacketState8, PrimitivePacketHit8>(
    *this, rays, states);
}

PrimitivePacketInterval4
Primitive::intersectPacketIntervals(const Ray4& rays, const PrimitivePacketState4& states) const {
  return intersectPacketIntervalsScalarFallback<Ray4, PrimitivePacketState4,
                                                PrimitivePacketInterval4>(*this, rays, states);
}

PrimitivePacketInterval8
Primitive::intersectPacketIntervals(const Ray8& rays, const PrimitivePacketState8& states) const {
  return intersectPacketIntervalsScalarFallback<Ray8, PrimitivePacketState8,
                                                PrimitivePacketInterval8>(*this, rays, states);
}

PrimitivePacketState4 Primitive::activePacketStates(const PrimitivePacketState4& states,
                                                    std::uint16_t activeMask) {
  PrimitivePacketState4 result = states;
  for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
    if ((activeMask & (1u << lane)) == 0) {
      result[lane] = nullptr;
    }
  }
  return result;
}

PrimitivePacketState8 Primitive::activePacketStates(const PrimitivePacketState8& states,
                                                    std::uint16_t activeMask) {
  PrimitivePacketState8 result = states;
  for (std::size_t lane = 0; lane != Ray8::lanes; ++lane) {
    if ((activeMask & (1u << lane)) == 0) {
      result[lane] = nullptr;
    }
  }
  return result;
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

void Primitive::forEachTransformedLeaf(const TransformedLeafVisitor& visitor) const {
  forEachTransformedLeaf(nullptr, Matrix4d(), Matrix3d(), visitor);
}

void Primitive::forEachTransformedLeaf(std::shared_ptr<render::Material> inheritedMaterial,
                                       const Matrix4d& pointMatrix, const Matrix3d& normalMatrix,
                                       const TransformedLeafVisitor& visitor) const {
  auto own = material();
  visitor({this, own ? own : inheritedMaterial, pointMatrix, normalMatrix});
}

void Primitive::forEachTransformedLeafInBounds(const BoundsFilter& boundsFilter,
                                               const TransformedLeafVisitor& visitor) const {
  forEachTransformedLeafInBounds(boundsFilter, nullptr, Matrix4d(), Matrix3d(), visitor);
}

void Primitive::forEachTransformedLeafInBounds(const BoundsFilter& boundsFilter,
                                               std::shared_ptr<render::Material> inheritedMaterial,
                                               const Matrix4d& pointMatrix,
                                               const Matrix3d& normalMatrix,
                                               const TransformedLeafVisitor& visitor) const {
  auto own = material();
  TransformedLeaf leaf{this, own ? own : inheritedMaterial, pointMatrix, normalMatrix};
  if (boundsFilter(leaf.boundingBox())) {
    visitor(leaf);
  }
}

void Primitive::appendIntersectionSceneRecord(IntersectionSceneBuilder& builder,
                                              const TransformedLeaf& leaf) const {
  builder.addUnsupportedPrimitive(leaf,
                                  "primitive is not supported by GPU intersection scene compiler");
}

void Primitive::appendIntersectionSceneRecords(IntersectionSceneBuilder& builder,
                                               std::shared_ptr<render::Material> inheritedMaterial,
                                               const Matrix4d& pointMatrix,
                                               const Matrix3d& normalMatrix,
                                               const Primitive* inheritedObject,
                                               const Vector3d& motionDelta) const {
  auto own = material();
  const Primitive* object = own ? this : inheritedObject;
  const TransformedLeaf leaf{
    this, own ? own : inheritedMaterial, pointMatrix, normalMatrix, object, motionDelta};
  if (leaf.material && !leaf.material->supportsPackedWavefrontIntersection()) {
    builder.addUnsupportedPrimitive(leaf,
                                    leaf.material->packedWavefrontIntersectionUnsupportedReason());
    return;
  }

  appendIntersectionSceneRecord(builder, leaf);
}

void Primitive::forEachCurveOverlaySegment(const CurveOverlaySegmentVisitor&) const {
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
  const double squaredMachineEpsilon =
    std::numeric_limits<double>::epsilon() * std::numeric_limits<double>::epsilon();
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
