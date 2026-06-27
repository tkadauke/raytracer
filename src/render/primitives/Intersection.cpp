#include "render/State.h"
#include "render/primitives/Intersection.h"
#include "core/geometry/Mesh.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"
#include <QDebug>
#include <array>
#include <cstdint>
#include <utility>

using namespace render;

const Primitive* Intersection::intersect(const Rayd& ray, HitPointInterval& hitPoints,
                                         render::State& state) const {
  if (!boundingBoxIntersects(ray)) {
    return nullptr;
  }

  unsigned int numHits = 0;
  for (const auto& i : primitives()) {
    HitPointInterval candidate;
    if (i->intersect(ray, candidate, state)) {
      if (numHits) {
        hitPoints = hitPoints & candidate;
      } else {
        hitPoints = candidate;
      }
      numHits++;
    }
  }

  if (numHits != primitives().size() || hitPoints.empty()) {
    return nullptr;
  } else {
    if (material()) {
      hitPoints.setPrimitive(this);
      return this;
    } else {
      return hitPoints.minWithPositiveDistance().primitive();
    }
  }
}

PrimitivePacketHit4 Intersection::intersectPacketHits(const Ray4& rays,
                                                      const PrimitivePacketState4& states) const {
  return intersectPacketIntervals(rays, states).closestHits(material() ? this : nullptr);
}

PrimitivePacketHit8 Intersection::intersectPacketHits(const Ray8& rays,
                                                      const PrimitivePacketState8& states) const {
  return intersectPacketIntervals(rays, states).closestHits(material() ? this : nullptr);
}

template<typename Packet, typename StateArray, typename Result>
Result Intersection::intersectPacketIntervalsFor(const Packet& rays,
                                                 const StateArray& states) const {
  Result result;
  std::array<bool, Packet::lanes> activeLanes{};
  std::array<unsigned int, Packet::lanes> hitCounts{};
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

  for (const auto& primitive : primitives()) {
    std::uint16_t childMask = 0;
    for (std::size_t lane = 0; lane != Packet::lanes; ++lane) {
      if (activeLanes[lane]) {
        childMask |= static_cast<std::uint16_t>(1u << lane);
      }
    }
    if (childMask == 0) {
      break;
    }

    const StateArray childStates = activePacketStates(states, childMask);
    const auto candidate = primitive->intersectPacketIntervals(rays, childStates);
    for (std::size_t lane = 0; lane != Packet::lanes; ++lane) {
      if (!activeLanes[lane]) {
        continue;
      }

      if (!candidate.hit(lane)) {
        activeLanes[lane] = false;
        continue;
      }

      if (hitCounts[lane] != 0) {
        intervals[lane] = intervals[lane] & candidate.interval(lane);
      } else {
        intervals[lane] = candidate.interval(lane);
      }
      ++hitCounts[lane];
      scalarFallbacks[lane] = scalarFallbacks[lane] || candidate.scalarFallback(lane);
    }
  }

  const unsigned int requiredHits = static_cast<unsigned int>(primitives().size());
  for (std::size_t lane = 0; lane != Packet::lanes; ++lane) {
    if (hitCounts[lane] == requiredHits && !intervals[lane].empty()) {
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
Intersection::intersectPacketIntervals(const Ray4& rays,
                                       const PrimitivePacketState4& states) const {
  return intersectPacketIntervalsFor<Ray4, PrimitivePacketState4, PrimitivePacketInterval4>(rays,
                                                                                            states);
}

PrimitivePacketInterval8
Intersection::intersectPacketIntervals(const Ray8& rays,
                                       const PrimitivePacketState8& states) const {
  return intersectPacketIntervalsFor<Ray8, PrimitivePacketState8, PrimitivePacketInterval8>(rays,
                                                                                            states);
}

bool Intersection::intersects(const Rayd& ray, render::State& state) const {
  if (!boundingBoxIntersects(ray)) {
    return false;
  }

  for (const auto& i : primitives()) {
    if (!i->intersects(ray, state))
      return false;
  }

  return true;
}

void Intersection::appendIntersectionSceneRecords(
  IntersectionSceneBuilder& builder, std::shared_ptr<render::Material> inheritedMaterial,
  const Matrix4d& pointMatrix, const Matrix3d& normalMatrix,
  const Primitive* inheritedObject) const {
  addUnsupportedCompositeIntersectionSceneRecord(
    builder, std::move(inheritedMaterial), pointMatrix, normalMatrix, inheritedObject,
    "intersection CSG is not supported by GPU intersection scene compiler");
}

std::shared_ptr<Mesh> Intersection::tessellate(int) const {
  qWarning() << "Intersection::tessellate not implemented — CSG mesh booleans not implemented.";
  return std::make_shared<Mesh>();
}

BoundingBoxd Intersection::calculateBoundingBox() const {
  BoundingBoxd result;
  int num = 0;
  for (const auto& i : primitives()) {
    if (num++ == 0)
      result = i->boundingBox();
    else
      result &= i->boundingBox();
  }
  return result;
}
