#include "raytracer/State.h"
#include "raytracer/primitives/Grid.h"
#include "core/math/Ray.h"
#include "core/math/HitPointInterval.h"

#include <cmath>

using namespace std;
using namespace raytracer;

inline float clamp(float x, float min, float max) {
  return (x < min ? min : (x > max ? max : x));
}

const Primitive* Grid::intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) const {
  if (m_boundingBox.isInfinite()) {
    return nullptr;
  }

  double ox = ray.origin().x();
  double oy = ray.origin().y();
  double oz = ray.origin().z();
  double dx = ray.direction().x();
  double dy = ray.direction().y();
  double dz = ray.direction().z();

  double x0 = m_boundingBox.min().x();
  double y0 = m_boundingBox.min().y();
  double z0 = m_boundingBox.min().z();
  double x1 = m_boundingBox.max().x();
  double y1 = m_boundingBox.max().y();
  double z1 = m_boundingBox.max().z();

  double tx_min, ty_min, tz_min;
  double tx_max, ty_max, tz_max;

  double a = 1.0 / dx;
  if (a >= 0) {
    tx_min = (x0 - ox) * a;
    tx_max = (x1 - ox) * a;
  }
  else {
    tx_min = (x1 - ox) * a;
    tx_max = (x0 - ox) * a;
  }

  double b = 1.0 / dy;
  if (b >= 0) {
    ty_min = (y0 - oy) * b;
    ty_max = (y1 - oy) * b;
  }
  else {
    ty_min = (y1 - oy) * b;
    ty_max = (y0 - oy) * b;
  }

  double c = 1.0 / dz;
  if (c >= 0) {
    tz_min = (z0 - oz) * c;
    tz_max = (z1 - oz) * c;
  }
  else {
    tz_min = (z1 - oz) * c;
    tz_max = (z0 - oz) * c;
  }

  double t0, t1;

  if (tx_min > ty_min)
    t0 = tx_min;
  else
    t0 = ty_min;

  if (tz_min > t0)
    t0 = tz_min;

  if (tx_max < ty_max)
    t1 = tx_max;
  else
    t1 = ty_max;

  if (tz_max < t1)
    t1 = tz_max;

  if (t0 > t1)
    return nullptr;

  Vector3d gridSize = m_boundingBox.max() - m_boundingBox.min();

  int x, y, z;
  Vector3d relativeOrigin;
  if (m_boundingBox.contains(ray.origin())) {
    relativeOrigin = ray.origin() - m_boundingBox.min();
  } else {
    relativeOrigin = ray.at(t0) - m_boundingBox.min();
  }
  x = clamp(relativeOrigin.x() * m_numX / gridSize.x(), 0, m_numX - 1);
  y = clamp(relativeOrigin.y() * m_numY / gridSize.y(), 0, m_numY - 1);
  z = clamp(relativeOrigin.z() * m_numZ / gridSize.z(), 0, m_numZ - 1);

  double dtx = (tx_max - tx_min) / m_numX;
  double dty = (ty_max - ty_min) / m_numY;
  double dtz = (tz_max - tz_min) / m_numZ;

  double   tx_next, ty_next, tz_next;
  int   ix_step, iy_step, iz_step;
  int   ix_stop, iy_stop, iz_stop;

  if (dx > 0) {
    tx_next = tx_min + (x + 1) * dtx;
    ix_step = +1;
    ix_stop = m_numX;
  } else {
    tx_next = tx_min + (m_numX - x) * dtx;
    ix_step = -1;
    ix_stop = -1;
  }

  if (dx == 0.0) {
    tx_next = numeric_limits<double>::max();
    ix_step = -1;
    ix_stop = -1;
  }


  if (dy > 0) {
    ty_next = ty_min + (y + 1) * dty;
    iy_step = +1;
    iy_stop = m_numY;
  } else {
    ty_next = ty_min + (m_numY - y) * dty;
    iy_step = -1;
    iy_stop = -1;
  }

  if (dy == 0.0) {
    ty_next = numeric_limits<double>::max();
    iy_step = -1;
    iy_stop = -1;
  }

  if (dz > 0) {
    tz_next = tz_min + (z + 1) * dtz;
    iz_step = +1;
    iz_stop = m_numZ;
  } else {
    tz_next = tz_min + (m_numZ - z) * dtz;
    iz_step = -1;
    iz_stop = -1;
  }

  if (dz == 0.0) {
    tz_next = numeric_limits<double>::max();
    iz_step = -1;
    iz_stop = -1;
  }


  // traverse the grid
  state.recordEvent(this, "Traversing grid");
  while (true) {
    const Primitive* primitive = m_cells[x + m_numX * y + m_numX * m_numY * z].get();

    if (tx_next < ty_next && tx_next < tz_next) {
      if (primitive) {
        HitPointInterval candidate;
        primitive = primitive->intersect(ray, candidate, state);
        if (candidate.minWithPositiveDistance().distance() - Rayd::epsilon < tx_next) {
          hitPoints = candidate;
          return primitive;
        }
      }

      tx_next += dtx;
      x += ix_step;

      if (x == ix_stop)
        return nullptr;
    } else if (ty_next < tz_next) {
      if (primitive) {
        HitPointInterval candidate;
        primitive = primitive->intersect(ray, candidate, state);
        if (candidate.minWithPositiveDistance().distance() - Rayd::epsilon < ty_next) {
          hitPoints = candidate;
          return primitive;
        }
      }

      ty_next += dty;
      y += iy_step;

      if (y == iy_stop)
        return nullptr;
    } else {
      if (primitive) {
        HitPointInterval candidate;
        primitive = primitive->intersect(ray, candidate, state);
        if (candidate.minWithPositiveDistance().distance() - Rayd::epsilon < tz_next) {
          hitPoints = candidate;
          return primitive;
        }
      }

      tz_next += dtz;
      z += iz_step;

      if (z == iz_stop)
        return nullptr;
    }
  }
}

bool Grid::intersects(const Rayd& ray, State& state) const {
  if (m_boundingBox.isInfinite()) {
    return false;
  }

  double ox = ray.origin().x();
  double oy = ray.origin().y();
  double oz = ray.origin().z();
  double dx = ray.direction().x();
  double dy = ray.direction().y();
  double dz = ray.direction().z();

  double x0 = m_boundingBox.min().x();
  double y0 = m_boundingBox.min().y();
  double z0 = m_boundingBox.min().z();
  double x1 = m_boundingBox.max().x();
  double y1 = m_boundingBox.max().y();
  double z1 = m_boundingBox.max().z();

  double tx_min, ty_min, tz_min;
  double tx_max, ty_max, tz_max;

  double a = 1.0 / dx;
  if (a >= 0) {
    tx_min = (x0 - ox) * a;
    tx_max = (x1 - ox) * a;
  }
  else {
    tx_min = (x1 - ox) * a;
    tx_max = (x0 - ox) * a;
  }

  double b = 1.0 / dy;
  if (b >= 0) {
    ty_min = (y0 - oy) * b;
    ty_max = (y1 - oy) * b;
  }
  else {
    ty_min = (y1 - oy) * b;
    ty_max = (y0 - oy) * b;
  }

  double c = 1.0 / dz;
  if (c >= 0) {
    tz_min = (z0 - oz) * c;
    tz_max = (z1 - oz) * c;
  }
  else {
    tz_min = (z1 - oz) * c;
    tz_max = (z0 - oz) * c;
  }

  double t0, t1;

  if (tx_min > ty_min)
    t0 = tx_min;
  else
    t0 = ty_min;

  if (tz_min > t0)
    t0 = tz_min;

  if (tx_max < ty_max)
    t1 = tx_max;
  else
    t1 = ty_max;

  if (tz_max < t1)
    t1 = tz_max;

  if (t0 > t1)
    return false;

  Vector3d gridSize = m_boundingBox.max() - m_boundingBox.min();

  int x, y, z;
  Vector3d relativeOrigin;
  if (m_boundingBox.contains(ray.origin())) {
    relativeOrigin = ray.origin() - m_boundingBox.min();
  } else {
    relativeOrigin = ray.at(t0) - m_boundingBox.min();
  }
  x = clamp(relativeOrigin.x() * m_numX / gridSize.x(), 0, m_numX - 1);
  y = clamp(relativeOrigin.y() * m_numY / gridSize.y(), 0, m_numY - 1);
  z = clamp(relativeOrigin.z() * m_numZ / gridSize.z(), 0, m_numZ - 1);

  double dtx = (tx_max - tx_min) / m_numX;
  double dty = (ty_max - ty_min) / m_numY;
  double dtz = (tz_max - tz_min) / m_numZ;

  double   tx_next, ty_next, tz_next;
  int   ix_step, iy_step, iz_step;
  int   ix_stop, iy_stop, iz_stop;

  if (dx > 0) {
    tx_next = tx_min + (x + 1) * dtx;
    ix_step = +1;
    ix_stop = m_numX;
  } else {
    tx_next = tx_min + (m_numX - x) * dtx;
    ix_step = -1;
    ix_stop = -1;
  }

  if (dx == 0.0) {
    tx_next = numeric_limits<double>::max();
    ix_step = -1;
    ix_stop = -1;
  }


  if (dy > 0) {
    ty_next = ty_min + (y + 1) * dty;
    iy_step = +1;
    iy_stop = m_numY;
  } else {
    ty_next = ty_min + (m_numY - y) * dty;
    iy_step = -1;
    iy_stop = -1;
  }

  if (dy == 0.0) {
    ty_next = numeric_limits<double>::max();
    iy_step = -1;
    iy_stop = -1;
  }

  if (dz > 0) {
    tz_next = tz_min + (z + 1) * dtz;
    iz_step = +1;
    iz_stop = m_numZ;
  } else {
    tz_next = tz_min + (m_numZ - z) * dtz;
    iz_step = -1;
    iz_stop = -1;
  }

  if (dz == 0.0) {
    tz_next = numeric_limits<double>::max();
    iz_step = -1;
    iz_stop = -1;
  }


  // traverse the grid

  while (true) {
    auto primitive = m_cells[x + m_numX * y + m_numX * m_numY * z];

    if (tx_next < ty_next && tx_next < tz_next) {
      if (primitive && primitive->intersects(ray, state))
        return true;

      tx_next += dtx;
      x += ix_step;

      if (x == ix_stop)
        return false;
    } else if (ty_next < tz_next) {
      if (primitive && primitive->intersects(ray, state))
        return true;

      ty_next += dty;
      y += iy_step;

      if (y == iy_stop)
        return false;
    } else {
      if (primitive && primitive->intersects(ray, state))
        return true;

      tz_next += dtz;
      z += iz_step;

      if (z == iz_stop)
        return false;
    }
  }
}

void Grid::setup() {
  m_boundingBox = boundingBox();
  if (m_boundingBox.isInfinite()) {
    return;
  }

  Vector3d gridSize = m_boundingBox.max() - m_boundingBox.min();
  double multiplier = 2.0;
  float s = pow(gridSize.x() * gridSize.y() * gridSize.z() / primitives().size(), 0.3333333);
  m_numX = multiplier * gridSize.x() / s + 1;
  m_numY = multiplier * gridSize.y() / s + 1;
  m_numZ = multiplier * gridSize.z() / s + 1;

  int numCells = m_numX * m_numY * m_numZ;
  m_cells.reserve(numCells);
  for (int i = 0; i != numCells; ++i)
    m_cells.push_back(0);

  vector<int> counts(numCells);

  for (const auto& primitive : primitives()) {
    BoundingBoxd bbox = primitive->boundingBox();
    if (bbox.isUndefined() || bbox.isEmpty())
      continue;

    Vector3d relativeMin = bbox.min() - m_boundingBox.min();
    Vector3d relativeMax = bbox.max() - m_boundingBox.min();
    if (relativeMin.isUndefined() || relativeMax.isUndefined())
      continue;

    int xmin = clamp(relativeMin.x() * m_numX / gridSize.x(), 0, m_numX - 1);
    int ymin = clamp(relativeMin.y() * m_numY / gridSize.y(), 0, m_numY - 1);
    int zmin = clamp(relativeMin.z() * m_numZ / gridSize.z(), 0, m_numZ - 1);
    int xmax = clamp(relativeMax.x() * m_numX / gridSize.x(), 0, m_numX - 1);
    int ymax = clamp(relativeMax.y() * m_numY / gridSize.y(), 0, m_numY - 1);
    int zmax = clamp(relativeMax.z() * m_numZ / gridSize.z(), 0, m_numZ - 1);

    for (int z = zmin; z <= zmax; ++z) {
      for (int y = ymin; y <= ymax; ++y) {
        for (int x = xmin; x <= xmax; ++x) {
          int index = x + m_numX * y + m_numX * m_numY * z;

          if (counts[index] == 0) {
            m_cells[index] = primitive;
          } else if (counts[index] == 1) {
            auto composite = std::make_shared<Composite>();
            composite->add(m_cells[index]);
            composite->add(primitive);
            m_cells[index] = composite;
          } else {
            std::dynamic_pointer_cast<Composite>(m_cells[index])->add(primitive);
          }
          counts[index]++;
        }
      }
    }
  }

  m_boundingBox = m_boundingBox.grownByEpsilon();
}
