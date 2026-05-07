#include "render/State.h"
#include "render/Stats.h"
#include "render/primitives/Sphere.h"
#include "core/geometry/Mesh.h"
#include "core/math/Constants.h"
#include "core/math/Ray.h"
#include "core/math/HitPointInterval.h"
#include <cmath>

using namespace std;
using namespace render;

const Primitive* Sphere::intersect(const Rayd& ray, HitPointInterval& hitPoints, render::State& state) const {
  RAYTRACER_STATS_INC(raySphereIntersect);
  const Vector3d& o = ray.origin() - m_origin, d = ray.direction();
  
  double od = o * d, dd = d * d;
  double discriminant = od * od - dd * (o * o - m_radius * m_radius);
  
  if (discriminant < 0) {
    state.miss(this, "Sphere, ray miss");
    return nullptr;
  } else if (discriminant > 0) {
    double discriminantRoot = sqrt(discriminant);
    double t1 = (-od - discriminantRoot) / dd;
    double t2 = (-od + discriminantRoot) / dd;
    
    Vector3d hitPoint1 = ray.at(t1),
             hitPoint2 = ray.at(t2);
    
    hitPoints.add(
      HitPoint(this, t1, hitPoint1, (hitPoint1 - m_origin) / m_radius),
      HitPoint(this, t2, hitPoint2, (hitPoint2 - m_origin) / m_radius)
    );
    
    if (t1 <= 0 && t2 <= 0) {
      state.miss(this, "Sphere, behind ray");
      return nullptr;
    } else {
      state.hit(this, "Sphere");
      return this;
    }
  }
  state.miss(this, "Sphere, ray miss");
  return nullptr;
}

bool Sphere::intersects(const Rayd& ray, render::State& state) const {
  RAYTRACER_STATS_INC(raySphereIntersects);
  const Vector3d& o = ray.origin() - m_origin, d = ray.direction();
  
  double od = o * d, dd = d * d;
  double discriminant = od * od - dd * (o * o - m_radius * m_radius);
  
  if (discriminant < 0) {
    state.shadowMiss(this, "Sphere, ray miss");
    return false;
  } else if (discriminant > 0) {
    double discriminantRoot = sqrt(discriminant);
    double t1 = (-od - discriminantRoot) / dd;
    double t2 = (-od + discriminantRoot) / dd;
    if (t1 <= 0 && t2 <= 0) {
      state.shadowMiss(this, "Sphere, behind ray");
      return false;
    }
    
    state.shadowHit(this, "Sphere");
    return true;
  }
  
  state.shadowMiss(this, "Sphere, ray miss");
  return false;
}

shared_ptr<Mesh> Sphere::tessellate(int lod) const {
  const int lonSegs = 16 << lod;   // 16, 32, 64, …
  const int latBands = 8 << lod;   // 8, 16, 32, …

  auto mesh = make_shared<Mesh>();

  // Build a (latBands+1) × (lonSegs+1) vertex grid. Polar rows share the
  // same 3D point but carry distinct u-values — avoids a UV pinch at the poles
  // without needing special-case fan triangles.
  for (int lat = 0; lat <= latBands; ++lat) {
    double theta = -PI / 2.0 + lat * PI / latBands;  // [-π/2, π/2]
    double cosTheta = std::cos(theta);
    double sinTheta = std::sin(theta);
    double v = static_cast<double>(lat) / latBands;  // [0, 1] south→north

    for (int lon = 0; lon <= lonSegs; ++lon) {
      double phi = lon * TAU / lonSegs;              // [0, 2π]
      double cosPhi = std::cos(phi);
      double sinPhi = std::sin(phi);
      double u = static_cast<double>(lon) / lonSegs; // [0, 1] around

      Vector3d normal(cosTheta * cosPhi, sinTheta, cosTheta * sinPhi);
      Vector3d point = m_origin + normal * m_radius;
      mesh->addVertex(point, normal, Vector2d(u, v));
    }
  }

  // One quad per (lat-band, lon-segment) cell — uniform topology, no fans.
  for (int lat = 0; lat < latBands; ++lat) {
    for (int lon = 0; lon < lonSegs; ++lon) {
      int row     = lat       * (lonSegs + 1);
      int nextRow = (lat + 1) * (lonSegs + 1);
      mesh->addFace({ row + lon, row + lon + 1, nextRow + lon + 1, nextRow + lon });
    }
  }

  return mesh;
}

BoundingBoxd Sphere::calculateBoundingBox() const {
  Vector3d radius(m_radius, m_radius, m_radius);
  return BoundingBoxd(m_origin - radius, m_origin + radius);
}

Vector3d Sphere::farthestPoint(const Vector3d& direction) const {
  return m_origin + direction.normalized() * m_radius;
}
