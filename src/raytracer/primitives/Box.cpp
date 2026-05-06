#include "raytracer/State.h"
#include "raytracer/primitives/Box.h"
#include "core/geometry/Mesh.h"
#include "core/math/Ray.h"
#include "core/math/HitPointInterval.h"
#include <cmath>

using namespace std;
using namespace raytracer;

const Primitive* Box::intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) const {
  int parallel = 0;
  bool found = false;
  Vector3d d = m_center - ray.origin();
  double t1 = 0, t2 = 0;
  Vector3d normal1, normal2;

  for (int i = 0; i < 3; ++i) {
    if (fabs(ray.direction()[i]) < 0.0001) {
      parallel |= 1 << i;
    } else {
      double dir = (ray.direction()[i] > 0.0) ? 1.0 : -1.0;
      double es = (ray.direction()[i] > 0.0) ? m_edge[i] : -m_edge[i];
      double invDi = 1.0 / ray.direction()[i];

      if (!found) {
        normal1[i] = -dir;
        normal2[i] = dir;
        t1 = (d[i] - es) * invDi;
        t2 = (d[i] + es) * invDi;
        found = true;
      } else {
        double s = (d[i] - es) * invDi;
        if (s > t1) {
          normal1 = Vector3d();
          normal1[i] = -dir;
          t1 = s;
        }
        s = (d[i] + es) * invDi;
        if (s < t2) {
          normal2 = Vector3d();
          normal2[i] = dir;
          t2 = s;
        }
        if (t1 > t2) {
          state.miss(this, "Box, ray miss");
          return nullptr;
        }
      }
    }
  }

  if (parallel)
    for (int i = 0; i < 3; ++i)
      if (parallel & (1 << i))
        if (fabs(d[i] - t1 * ray.direction()[i]) > m_edge[i] || fabs(d[i] - t2 * ray.direction()[i]) > m_edge[i]) {
          state.miss(this, "Box, ray parallel");
          return nullptr;
        }

  hitPoints.add(
    HitPoint(this, t1, ray.at(t1), normal1),
    HitPoint(this, t2, ray.at(t2), normal2)
  );

  if (t1 < 0 && t2 < 0) {
    state.miss(this, "Box, behind ray");
    return nullptr;
  }

  state.hit(this, "Box");
  return this;
}

bool Box::intersects(const Rayd& ray, State&) const {
  return boundingBox().intersects(ray);
}

BoundingBoxd Box::calculateBoundingBox() const {
  return BoundingBoxd(m_center - m_edge, m_center + m_edge);
}

Vector3d Box::farthestPoint(const Vector3d& direction) const {
  return m_center + Vector3d(
    direction.x() < 0.0 ? -m_edge.x() : m_edge.x(),
    direction.y() < 0.0 ? -m_edge.y() : m_edge.y(),
    direction.z() < 0.0 ? -m_edge.z() : m_edge.z()
  );
}

std::shared_ptr<Mesh> Box::tessellate(int) const {
  // Box is polyhedral — every level of detail produces the same six-face
  // mesh. 24 vertices total: each face contributes its own four because
  // sharing across faces would force a single normal/UV per shared corner,
  // which is wrong for both flat shading and per-face texturing.
  auto mesh = std::make_shared<Mesh>();

  const double cx = m_center.x(), cy = m_center.y(), cz = m_center.z();
  const double ex = m_edge.x(),   ey = m_edge.y(),   ez = m_edge.z();

  // Helper: add four vertices for one face plus the quad face index.
  // `addQuad` picks UVs per corner so each face's texture spans
  // exactly [0, 1]² with `u` along the first cross-edge and `v`
  // along the second. Face winding goes CCW when viewed from
  // outside the box — adopting the OpenGL / glTF convention so a
  // future rasterizer can back-face-cull on the standard sign.
  auto addQuad = [&](const Vector3d& normal,
                     const Vector3d& v0, const Vector3d& v1,
                     const Vector3d& v2, const Vector3d& v3) {
    int base = static_cast<int>(mesh->vertices().size());
    mesh->addVertex(v0, normal, Vector2d(0, 0));
    mesh->addVertex(v1, normal, Vector2d(1, 0));
    mesh->addVertex(v2, normal, Vector2d(1, 1));
    mesh->addVertex(v3, normal, Vector2d(0, 1));
    mesh->addFace({base, base + 1, base + 2, base + 3});
  };

  // The eight corners. Naming `pXYZ` where each {0,1} is the sign of
  // (-edge, +edge) along that axis.
  const Vector3d p000(cx - ex, cy - ey, cz - ez);
  const Vector3d p001(cx - ex, cy - ey, cz + ez);
  const Vector3d p010(cx - ex, cy + ey, cz - ez);
  const Vector3d p011(cx - ex, cy + ey, cz + ez);
  const Vector3d p100(cx + ex, cy - ey, cz - ez);
  const Vector3d p101(cx + ex, cy - ey, cz + ez);
  const Vector3d p110(cx + ex, cy + ey, cz - ez);
  const Vector3d p111(cx + ex, cy + ey, cz + ez);

  // Faces, each with its outward normal. The four corners are listed
  // CCW from outside; pick `u`-direction first then `v`-direction.
  // Vector3d::up() in this codebase is (0, -1, 0), so the "+y face"
  // (mathematically the bottom of the box in world coords) and the
  // "-y face" (top) follow that convention; see the existing
  // intersect() above for the matching sign discipline.

  // +X face — outward normal (+1, 0, 0). Viewed from +X looking
  // toward -X: +Y is to the right (in math), +Z is up.
  addQuad(Vector3d( 1,  0,  0), p100, p110, p111, p101);
  // -X face — outward (-1, 0, 0). Viewed from -X looking toward +X:
  // +Z is to the right, +Y is up.
  addQuad(Vector3d(-1,  0,  0), p000, p001, p011, p010);

  // +Y face — outward (0, +1, 0). World convention: +Y is "down."
  // Viewed from +Y: +X to the right, +Z up.
  addQuad(Vector3d( 0,  1,  0), p010, p011, p111, p110);
  // -Y face — outward (0, -1, 0). Viewed from -Y: +Z to the right,
  // +X up.
  addQuad(Vector3d( 0, -1,  0), p000, p100, p101, p001);

  // +Z face — outward (0, 0, +1). Viewed from +Z: +X to the right,
  // +Y up.
  addQuad(Vector3d( 0,  0,  1), p001, p101, p111, p011);
  // -Z face — outward (0, 0, -1). Viewed from -Z: +X to the left,
  // +Y up.
  addQuad(Vector3d( 0,  0, -1), p010, p110, p100, p000);

  return mesh;
}
