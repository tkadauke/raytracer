#include "render/State.h"
#include "render/primitives/ConvexOperation.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"

#include <utility>

using namespace render;

const Primitive* ConvexOperation::intersect(const Rayd& ray, HitPointInterval& hitPoints,
                                            render::State& state) const {
  return intersectConvexRay(ray, hitPoints, state);
}

const Primitive* ConvexOperation::intersectConvexRay(const Rayd& ray, HitPointInterval& hitPoints,
                                                     render::State& state) const {
  if (!boundingBoxIntersects(ray)) {
    state.miss(this, "ConvexOperation, bounding box miss");
    return nullptr;
  }

  // unlike the intersection methods for box, sphere, etc, convexIntersect()
  // only returns a single (closest) hitpoint. So we need to shoot a ray in the
  // opposite direction as well.

  // calculate hitpoint in ray direction
  if (convexIntersect(ray, hitPoints)) {
    // if it's a hit, do it again in the opposite direction
    HitPointInterval opposite;
    // If we double the hit distance and take the longest possible length in
    // the bounding box, we should be beyond the other side of the object
    Rayd oppositeRay(ray.at(hitPoints.min().distance() * 2.0 + boundingBox().size().length()),
                     -ray.direction());

    // fire!
    convexIntersect(oppositeRay, opposite);
    HitPoint oppositePoint = opposite.min();
    // now, we have to project that point on the opposite side back to the
    // original ray to find the real distance value
    oppositePoint.setDistance(ray.projectedDistance(oppositePoint.point()));

    // add it to the interval
    hitPoints.add(oppositePoint);
    // and merge both points into a single interval. we can do that, since this
    // is by definition a convex object, so there can be only a single interval
    hitPoints = hitPoints.merged();

    auto hitPoint = hitPoints.minWithPositiveDistance();
    if (hitPoint.isUndefined()) {
      state.miss(this, "ConvexOperation, ray miss");
      return nullptr;
    } else {
      state.hit(this, "ConvexOperation");

      hitPoints.setPrimitive(this);
      return this;
    }
  }
  state.miss(this, "ConvexOperation, ray miss");
  return nullptr;
}

template<typename Packet, typename StateArray, typename Result>
Result ConvexOperation::intersectPacketHitsFor(const Packet& rays, const StateArray& states) const {
  Result result;
  for (std::size_t lane = 0; lane != Packet::lanes; ++lane) {
    if (!states[lane]) {
      continue;
    }
    State& state = *states[lane];
    HitPointInterval hitPoints;
    const Primitive* primitive = intersectConvexRay(rays.rayd(lane), hitPoints, state);
    const HitPoint hitPoint = hitPoints.minWithPositiveDistance();
    if (primitive && !hitPoint.isUndefined()) {
      result.setHit(lane, primitive, hitPoint);
    }
  }
  return result;
}

PrimitivePacketHit4
ConvexOperation::intersectPacketHits(const Ray4& rays, const PrimitivePacketState4& states) const {
  return intersectPacketHitsFor<Ray4, PrimitivePacketState4, PrimitivePacketHit4>(rays, states);
}

PrimitivePacketHit8
ConvexOperation::intersectPacketHits(const Ray8& rays, const PrimitivePacketState8& states) const {
  return intersectPacketHitsFor<Ray8, PrimitivePacketState8, PrimitivePacketHit8>(rays, states);
}

template<typename Packet, typename StateArray, typename Result>
Result ConvexOperation::intersectPacketIntervalsFor(const Packet& rays,
                                                    const StateArray& states) const {
  Result result;
  for (std::size_t lane = 0; lane != Packet::lanes; ++lane) {
    if (!states[lane]) {
      continue;
    }
    State& state = *states[lane];
    HitPointInterval hitPoints;
    const Primitive* primitive = intersectConvexRay(rays.rayd(lane), hitPoints, state);
    if (primitive || !hitPoints.empty()) {
      result.setInterval(lane, primitive, hitPoints);
    }
  }
  return result;
}

PrimitivePacketInterval4
ConvexOperation::intersectPacketIntervals(const Ray4& rays,
                                          const PrimitivePacketState4& states) const {
  return intersectPacketIntervalsFor<Ray4, PrimitivePacketState4, PrimitivePacketInterval4>(rays,
                                                                                            states);
}

PrimitivePacketInterval8
ConvexOperation::intersectPacketIntervals(const Ray8& rays,
                                          const PrimitivePacketState8& states) const {
  return intersectPacketIntervalsFor<Ray8, PrimitivePacketState8, PrimitivePacketInterval8>(rays,
                                                                                            states);
}

bool ConvexOperation::intersects(const Rayd& ray, render::State&) const {
  if (!boundingBoxIntersects(ray)) {
    return false;
  }

  HitPointInterval hitPoints;
  return convexIntersect(ray, hitPoints);
}

void ConvexOperation::appendIntersectionSceneRecords(
  IntersectionSceneBuilder& builder, std::shared_ptr<render::Material> inheritedMaterial,
  const Matrix4d& pointMatrix, const Matrix3d& normalMatrix, const Primitive* inheritedObject,
  const Vector3d& motionDelta) const {
  addUnsupportedCompositeIntersectionSceneRecord(
    builder, std::move(inheritedMaterial), pointMatrix, normalMatrix, inheritedObject, motionDelta,
    "convex CSG is not supported by GPU intersection scene compiler");
}
