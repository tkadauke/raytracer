#include "render/State.h"
#include "render/primitives/Union.h"
#include "core/geometry/Mesh.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"
#include <QDebug>
#include <array>
#include <cstdint>
#include <utility>

using namespace render;

const Primitive* Union::intersect(const Rayd& ray, HitPointInterval& hitPoints,
                                  render::State& state) const {
  if (!boundingBoxIntersects(ray)) {
    return nullptr;
  }

  for (const auto& i : primitives()) {
    HitPointInterval candidate;
    if (i->intersect(ray, candidate, state)) {
      hitPoints = hitPoints | candidate;
    }
  }

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

PrimitivePacketHit4 Union::intersectPacketHits(const Ray4& rays,
                                               const PrimitivePacketState4& states) const {
  return intersectPacketIntervals(rays, states).closestHits(material() ? this : nullptr);
}

PrimitivePacketHit8 Union::intersectPacketHits(const Ray8& rays,
                                               const PrimitivePacketState8& states) const {
  return intersectPacketIntervals(rays, states).closestHits(material() ? this : nullptr);
}

template<typename Packet, typename StateArray, typename Result>
Result Union::intersectPacketIntervalsFor(const Packet& rays, const StateArray& states) const {
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
      if (!activeLanes[lane] || !candidate.hit(lane)) {
        continue;
      }

      intervals[lane] = intervals[lane] | candidate.interval(lane);
      scalarFallbacks[lane] = scalarFallbacks[lane] || candidate.scalarFallback(lane);
    }
  }

  for (std::size_t lane = 0; lane != Packet::lanes; ++lane) {
    if (!intervals[lane].empty()) {
      HitPointInterval interval = intervals[lane];
      const HitPoint& hitPoint = interval.minWithPositiveDistance();
      const Primitive* primitive = hitPoint.primitive();
      if (!hitPoint.isUndefined() && material()) {
        interval.setPrimitive(this);
        primitive = this;
      }
      result.setInterval(lane, primitive, interval, scalarFallbacks[lane]);
    }
  }
  return result;
}

PrimitivePacketInterval4
Union::intersectPacketIntervals(const Ray4& rays, const PrimitivePacketState4& states) const {
  return intersectPacketIntervalsFor<Ray4, PrimitivePacketState4, PrimitivePacketInterval4>(rays,
                                                                                            states);
}

PrimitivePacketInterval8
Union::intersectPacketIntervals(const Ray8& rays, const PrimitivePacketState8& states) const {
  return intersectPacketIntervalsFor<Ray8, PrimitivePacketState8, PrimitivePacketInterval8>(rays,
                                                                                            states);
}

std::shared_ptr<Mesh> Union::tessellate(int) const {
  qWarning() << "Union::tessellate not implemented — CSG mesh booleans not implemented.";
  return std::make_shared<Mesh>();
}

void Union::appendIntersectionSceneRecords(IntersectionSceneBuilder& builder,
                                           std::shared_ptr<render::Material> inheritedMaterial,
                                           const Matrix4d& pointMatrix,
                                           const Matrix3d& normalMatrix,
                                           const Primitive* inheritedObject) const {
  if (hasPairwiseNonOverlappingFiniteChildBounds()) {
    Composite::appendIntersectionSceneRecords(builder, std::move(inheritedMaterial), pointMatrix,
                                              normalMatrix, inheritedObject);
    return;
  }

  addUnsupportedCompositeIntersectionSceneRecord(
    builder, std::move(inheritedMaterial), pointMatrix, normalMatrix, inheritedObject,
    "union CSG is not supported by GPU intersection scene compiler");
}

bool Union::intersects(const Rayd& ray, render::State& state) const {
  if (!boundingBoxIntersects(ray)) {
    return false;
  }

  for (const auto& i : primitives()) {
    if (i->intersects(ray, state)) {
      return true;
    }
  }
  return false;
}
