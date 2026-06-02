#include "render/primitives/Curve.h"

#include "core/geometry/Mesh.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"
#include "core/math/RayPacket.h"
#include "render/State.h"

#include <algorithm>
#include <cmath>

using namespace render;

namespace {
  constexpr double kSegmentEpsilon = 1e-12;
  constexpr double kPi = 3.141592653589793238462643383279502884;

  bool isUsableSegment(const Vector3d& start, const Vector3d& end) {
    return (end - start).squaredLength() > kSegmentEpsilon * kSegmentEpsilon;
  }

  Vector3d perpendicularTo(const Vector3d& direction) {
    const Vector3d reference = std::abs(direction.y()) < 0.9 ? Vector3d::up() : Vector3d::right();
    return (reference ^ direction).normalized();
  }

  void addRibbonSegment(Mesh& mesh, const Vector3d& start, const Vector3d& end, double halfWidth,
                        double v0, double v1, const std::optional<Colord>& color) {
    const Vector3d direction = (end - start).normalized();
    const Vector3d side = perpendicularTo(direction) * halfWidth;
    const Vector3d normal = (direction ^ side).normalized();
    const int base = static_cast<int>(mesh.vertices().size());

    mesh.addVertex(start - side, normal, Vector2d(0, v0));
    mesh.addVertex(end - side, normal, Vector2d(0, v1));
    mesh.addVertex(end + side, normal, Vector2d(1, v1));
    mesh.addVertex(start + side, normal, Vector2d(1, v0));
    if (color)
      mesh.addFace({base, base + 1, base + 2, base + 3}, *color);
    else
      mesh.addFace({base, base + 1, base + 2, base + 3});
  }

  void addTubeSegment(Mesh& mesh, const Vector3d& start, const Vector3d& end, double radius,
                      int sides, double v0, double v1, const std::optional<Colord>& color) {
    const Vector3d direction = (end - start).normalized();
    const Vector3d u = perpendicularTo(direction);
    const Vector3d v = direction ^ u;
    const int base = static_cast<int>(mesh.vertices().size());

    for (int ring = 0; ring != 2; ++ring) {
      const Vector3d center = ring == 0 ? start : end;
      const double vCoord = ring == 0 ? v0 : v1;
      for (int i = 0; i != sides; ++i) {
        const double theta = 2.0 * kPi * static_cast<double>(i) / static_cast<double>(sides);
        const Vector3d normal = (u * std::cos(theta) + v * std::sin(theta)).normalized();
        mesh.addVertex(center + normal * radius, normal,
                       Vector2d(static_cast<double>(i) / static_cast<double>(sides), vCoord));
      }
    }

    for (int i = 0; i != sides; ++i) {
      const int next = (i + 1) % sides;
      if (color)
        mesh.addFace({base + i, base + next, base + sides + next, base + sides + i}, *color);
      else
        mesh.addFace({base + i, base + next, base + sides + next, base + sides + i});
    }
  }
}

const Primitive* Curve::intersect(const Rayd&, HitPointInterval&, render::State& state) const {
  state.miss(this, "Curve, ray intersection not implemented");
  return nullptr;
}

PrimitivePacketHit4 Curve::intersectPacketHits(const Ray4&,
                                               const PrimitivePacketState4& states) const {
  PrimitivePacketHit4 result;
  for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
    State fallbackState;
    State& state = states[lane] ? *states[lane] : fallbackState;
    state.miss(this, "Curve, ray intersection not implemented");
  }
  return result;
}

PrimitivePacketHit8 Curve::intersectPacketHits(const Ray8&,
                                               const PrimitivePacketState8& states) const {
  PrimitivePacketHit8 result;
  for (std::size_t lane = 0; lane != Ray8::lanes; ++lane) {
    State fallbackState;
    State& state = states[lane] ? *states[lane] : fallbackState;
    state.miss(this, "Curve, ray intersection not implemented");
  }
  return result;
}

void Curve::forEachCurveOverlaySegment(const CurveOverlaySegmentVisitor& visitor) const {
  for (const auto segment : m_polyline) {
    if (!isUsableSegment(segment.start, segment.end))
      continue;

    const auto color =
      m_segmentColorMap ? m_segmentColorMap->colorFor(segment.attributes) : std::optional<Colord>();
    visitor(segment.start, segment.end, color);
  }
}

std::shared_ptr<Mesh> Curve::tessellate(int lod) const {
  auto mesh = std::make_shared<Mesh>();
  if (m_width <= 0.0 || m_polyline.segmentCount() == 0)
    return mesh;

  const double halfWidth = m_width * 0.5;
  const int tubeSides = std::max(8, 8 << std::max(0, lod));
  double traveled = 0.0;

  for (const auto segment : m_polyline) {
    const Vector3d& start = segment.start;
    const Vector3d& end = segment.end;
    if (!isUsableSegment(start, end))
      continue;

    const double length = (end - start).length();
    const double v0 = traveled;
    const double v1 = traveled + length;
    const auto color =
      m_segmentColorMap ? m_segmentColorMap->colorFor(segment.attributes) : std::optional<Colord>();
    if (m_mode == TessellationMode::Tube)
      addTubeSegment(*mesh, start, end, halfWidth, tubeSides, v0, v1, color);
    else
      addRibbonSegment(*mesh, start, end, halfWidth, v0, v1, color);
    traveled = v1;
  }

  return mesh;
}

BoundingBoxd Curve::calculateBoundingBox() const {
  BoundingBoxd box = m_polyline.bounds();
  if (!box.isValid())
    return BoundingBoxd::undefined;

  const double halfWidth = std::max(0.0, m_width) * 0.5;
  return box.grownBy(Vector3d(halfWidth, halfWidth, halfWidth)).grownByEpsilon();
}
