#include "render/State.h"
#include "render/primitives/ClosedSolidUnion.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"
#include <array>
#include <cstdint>

using namespace render;

const Primitive* ClosedSolidUnion::intersect(const Rayd& ray, HitPointInterval& hitPoints,
                                             render::State& state) const {
  if (!boundingBoxIntersects(ray)) {
    return nullptr;
  }

  for (const auto& i : primitives()) {
    HitPointInterval candidate;
    i->intersect(ray, candidate, state);
    // Add hitpoints regardless of the result above. We also want to know about
    // objects that we have hit behind the origin, so we can correctly build
    // complex CSG models. This is especially important
    hitPoints = hitPoints + candidate;
  }
  hitPoints = hitPoints.merged();

  auto hitPoint = hitPoints.minWithPositiveDistance();
  if (hitPoint.isUndefined()) {
    return nullptr;
  } else {
    if (material()) {
      hitPoints.setPrimitive(this);
      return this;
    } else {
      return hitPoint.primitive();
    }
  }
}

PrimitivePacketHit4
ClosedSolidUnion::intersectPacketHits(const Ray4& rays, const PrimitivePacketState4& states) const {
  return intersectPacketIntervals(rays, states).closestHits(material() ? this : nullptr);
}

PrimitivePacketHit8
ClosedSolidUnion::intersectPacketHits(const Ray8& rays, const PrimitivePacketState8& states) const {
  return intersectPacketIntervals(rays, states).closestHits(material() ? this : nullptr);
}

template<typename Packet, typename StateArray, typename Result>
Result ClosedSolidUnion::intersectPacketIntervalsFor(const Packet& rays,
                                                     const StateArray& states) const {
  Result result;
  std::array<bool, Packet::lanes> activeLanes{};
  std::array<HitPointInterval, Packet::lanes> intervals{};
  std::array<bool, Packet::lanes> scalarFallbacks{};
  std::uint16_t activeMask = 0;

  for (std::size_t lane = 0; lane != Packet::lanes; ++lane) {
    activeLanes[lane] = states[lane] && boundingBoxIntersects(rays.rayd(lane));
    if (activeLanes[lane]) {
      activeMask |= static_cast<std::uint16_t>(1u << lane);
    }
  }
  if (activeMask == 0) {
    return result;
  }

  const StateArray activeStates = activePacketStates(states, activeMask);
  for (const auto& primitive : primitives()) {
    const auto candidate = primitive->intersectPacketIntervals(rays, activeStates);
    for (std::size_t lane = 0; lane != Packet::lanes; ++lane) {
      if (!activeLanes[lane] || !candidate.hasInterval(lane)) {
        continue;
      }

      intervals[lane] = intervals[lane] + candidate.interval(lane);
      scalarFallbacks[lane] = scalarFallbacks[lane] || candidate.scalarFallback(lane);
    }
  }

  for (std::size_t lane = 0; lane != Packet::lanes; ++lane) {
    if (!intervals[lane].empty()) {
      HitPointInterval merged = intervals[lane].merged();
      const HitPoint& hitPoint = merged.minWithPositiveDistance();
      const Primitive* primitive = hitPoint.primitive();
      if (!hitPoint.isUndefined() && material()) {
        merged.setPrimitive(this);
        primitive = this;
      }
      result.setInterval(lane, primitive, merged, scalarFallbacks[lane]);
    }
  }
  return result;
}

PrimitivePacketInterval4
ClosedSolidUnion::intersectPacketIntervals(const Ray4& rays,
                                           const PrimitivePacketState4& states) const {
  return intersectPacketIntervalsFor<Ray4, PrimitivePacketState4, PrimitivePacketInterval4>(rays,
                                                                                            states);
}

PrimitivePacketInterval8
ClosedSolidUnion::intersectPacketIntervals(const Ray8& rays,
                                           const PrimitivePacketState8& states) const {
  return intersectPacketIntervalsFor<Ray8, PrimitivePacketState8, PrimitivePacketInterval8>(rays,
                                                                                            states);
}

void ClosedSolidUnion::appendIntersectionSceneRecords(
  IntersectionSceneBuilder& builder, std::shared_ptr<render::Material> inheritedMaterial,
  const Matrix4d& pointMatrix, const Matrix3d& normalMatrix, const Primitive* inheritedObject,
  const Vector3d& motionDelta) const {
  if (hasFlattenableClosedSolidUnionChildBounds()) {
    Composite::appendIntersectionSceneRecords(builder, std::move(inheritedMaterial), pointMatrix,
                                              normalMatrix, inheritedObject, motionDelta);
    return;
  }

  addUnsupportedCompositeIntersectionSceneRecord(
    builder, std::move(inheritedMaterial), pointMatrix, normalMatrix, inheritedObject, motionDelta,
    "closed solid union CSG is not supported by GPU intersection scene compiler");
}

Vector3d ClosedSolidUnion::farthestPoint(const Vector3d& direction) const {
  for (const auto& primitive : primitives()) {
    Vector3d point = primitive->farthestPoint(direction);
    if (!point.isUndefined())
      return point;
  }
  return Vector3d::undefined;
}
