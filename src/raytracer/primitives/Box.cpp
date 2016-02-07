#include "raytracer/primitives/Box.h"
#include "core/math/Ray.h"
#include "core/math/HitPointInterval.h"
#include <cmath>
#include <iostream>

using namespace std;
using namespace raytracer;

Primitive* Box::intersect(const Ray& ray, HitPointInterval& hitPoints) {
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
        if (t1 > t2)
          return nullptr;
      }
    }
  }
  
  if (t1 < 0 && t2 < 0)
    return nullptr;

  if (parallel)
    for (int i = 0; i < 3; ++i)
      if (parallel & (1 << i))
        if (fabs(d[i] - t1 * ray.direction()[i]) > m_edge[i] || fabs(d[i] - t2 * ray.direction()[i]) > m_edge[i])
          return nullptr;

  hitPoints.add(HitPoint(t1, ray.at(t1), normal1),
                HitPoint(t2, ray.at(t2), normal2));
  return this;
}

BoundingBox Box::boundingBox() {
  return BoundingBox(m_center - m_edge, m_center + m_edge);
}
