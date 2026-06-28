#include "render/State.h"
#include "render/primitives/Difference.h"
#include "core/geometry/Mesh.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"
#include <QDebug>
#include <array>
#include <cstdint>
#include <utility>

using namespace render;

const Primitive* Difference::intersect(const Rayd& ray, HitPointInterval& hitPoints,
                                       render::State& state) const {
  if (!boundingBoxIntersects(ray)) {
    return nullptr;
  }

  bool firstElement = true;

  for (const auto& i : primitives()) {
    HitPointInterval candidate;
    if (i->intersect(ray, candidate, state)) {
      if (firstElement) {
        hitPoints = candidate;
      } else {
        hitPoints = hitPoints - candidate;
      }
    } else if (firstElement) {
      return nullptr;
    }
    firstElement = false;
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

PrimitivePacketHit4 Difference::intersectPacketHits(const Ray4& rays,
                                                    const PrimitivePacketState4& states) const {
  return intersectPacketIntervals(rays, states).closestHits(material() ? this : nullptr);
}

PrimitivePacketHit8 Difference::intersectPacketHits(const Ray8& rays,
                                                    const PrimitivePacketState8& states) const {
  return intersectPacketIntervals(rays, states).closestHits(material() ? this : nullptr);
}

template<typename Packet, typename StateArray, typename Result>
Result Difference::intersectPacketIntervalsFor(const Packet& rays, const StateArray& states) const {
  Result result;
  std::array<bool, Packet::lanes> activeLanes{};
  std::array<bool, Packet::lanes> firstChildHit{};
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

  bool firstElement = true;
  for (const auto& primitive : primitives()) {
    std::uint16_t childMask = activeMask;
    if (!firstElement) {
      childMask = 0;
      for (std::size_t lane = 0; lane != Packet::lanes; ++lane) {
        if (activeLanes[lane] && firstChildHit[lane]) {
          childMask |= static_cast<std::uint16_t>(1u << lane);
        }
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

      if (firstElement) {
        if (candidate.hit(lane)) {
          intervals[lane] = candidate.interval(lane);
          scalarFallbacks[lane] = scalarFallbacks[lane] || candidate.scalarFallback(lane);
          firstChildHit[lane] = true;
        }
        continue;
      }

      if (firstChildHit[lane] && candidate.hit(lane)) {
        intervals[lane] = intervals[lane] - candidate.interval(lane);
        scalarFallbacks[lane] = scalarFallbacks[lane] || candidate.scalarFallback(lane);
      }
    }
    firstElement = false;
  }

  for (std::size_t lane = 0; lane != Packet::lanes; ++lane) {
    if (firstChildHit[lane] && !intervals[lane].empty()) {
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
Difference::intersectPacketIntervals(const Ray4& rays, const PrimitivePacketState4& states) const {
  return intersectPacketIntervalsFor<Ray4, PrimitivePacketState4, PrimitivePacketInterval4>(rays,
                                                                                            states);
}

PrimitivePacketInterval8
Difference::intersectPacketIntervals(const Ray8& rays, const PrimitivePacketState8& states) const {
  return intersectPacketIntervalsFor<Ray8, PrimitivePacketState8, PrimitivePacketInterval8>(rays,
                                                                                            states);
}

// Shadow implementation of Composite, which generates spourious shadows of
// differential objects
bool Difference::intersects(const Rayd& ray, render::State& state) const {
  return Primitive::intersects(ray, state);
}

void Difference::appendIntersectionSceneRecords(IntersectionSceneBuilder& builder,
                                                std::shared_ptr<render::Material> inheritedMaterial,
                                                const Matrix4d& pointMatrix,
                                                const Matrix3d& normalMatrix,
                                                const Primitive* inheritedObject,
                                                const Vector3d& motionDelta) const {
  addUnsupportedCompositeIntersectionSceneRecord(
    builder, std::move(inheritedMaterial), pointMatrix, normalMatrix, inheritedObject, motionDelta,
    "difference CSG is not supported by GPU intersection scene compiler");
}

std::shared_ptr<Mesh> Difference::tessellate(int) const {
  qWarning() << "Difference::tessellate not implemented — CSG mesh booleans not implemented.";
  return std::make_shared<Mesh>();
}

BoundingBoxd Difference::calculateBoundingBox() const {
  if (primitives().size() > 0) {
    return primitives().front()->boundingBox();
  } else {
    return BoundingBoxd();
  }
}
