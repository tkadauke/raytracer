#include "raytracer/State.h"
#include "render/Stats.h"
#include "render/primitives/Grid.h"
#include "core/math/Ray.h"
#include "core/math/HitPointInterval.h"

#include <cassert>
#include <cmath>

using namespace std;
using namespace render;

inline float clamp(float x, float min, float max) {
  return (x < min ? min : (x > max ? max : x));
}

namespace {
  // Walk the uniform-grid cells along `ray` using the standard 3-axis DDA,
  // calling `visit(cell, tNext)` once per non-empty cell encountered. The
  // visitor returns true to stop traversal (it found what it was looking for)
  // or false to keep stepping. traverseGrid returns whichever of those won —
  // true if any visit returned true, false if the ray exited the grid.
  //
  // Both Grid::intersect and Grid::intersects used to inline this whole body
  // — slab-method bbox test, starting-cell computation, per-axis stepping
  // setup, and the DDA loop. Pulled out so a bug fix in any of those steps
  // (numerical edge cases, shadow-ray semantics, axis-aligned ray handling)
  // happens in one place.
  template<typename Visitor>
  bool traverseGrid(
    const Rayd& ray,
    const BoundingBoxd& bbox,
    int numX, int numY, int numZ,
    const std::vector<std::shared_ptr<Primitive>>& cells,
    Visitor visit
  ) {
    double ox = ray.origin().x();
    double oy = ray.origin().y();
    double oz = ray.origin().z();
    double dx = ray.direction().x();
    double dy = ray.direction().y();
    double dz = ray.direction().z();

    double x0 = bbox.min().x();
    double y0 = bbox.min().y();
    double z0 = bbox.min().z();
    double x1 = bbox.max().x();
    double y1 = bbox.max().y();
    double z1 = bbox.max().z();

    double tx_min, ty_min, tz_min;
    double tx_max, ty_max, tz_max;

    double a = 1.0 / dx;
    if (a >= 0) { tx_min = (x0 - ox) * a; tx_max = (x1 - ox) * a; }
    else        { tx_min = (x1 - ox) * a; tx_max = (x0 - ox) * a; }

    double b = 1.0 / dy;
    if (b >= 0) { ty_min = (y0 - oy) * b; ty_max = (y1 - oy) * b; }
    else        { ty_min = (y1 - oy) * b; ty_max = (y0 - oy) * b; }

    double c = 1.0 / dz;
    if (c >= 0) { tz_min = (z0 - oz) * c; tz_max = (z1 - oz) * c; }
    else        { tz_min = (z1 - oz) * c; tz_max = (z0 - oz) * c; }

    double t0 = tx_min > ty_min ? tx_min : ty_min;
    if (tz_min > t0) t0 = tz_min;

    double t1 = tx_max < ty_max ? tx_max : ty_max;
    if (tz_max < t1) t1 = tz_max;

    if (t0 > t1) return false;

    Vector3d gridSize = bbox.max() - bbox.min();

    int x, y, z;
    Vector3d relativeOrigin = bbox.contains(ray.origin())
      ? ray.origin() - bbox.min()
      : ray.at(t0) - bbox.min();
    x = clamp(relativeOrigin.x() * numX / gridSize.x(), 0, numX - 1);
    y = clamp(relativeOrigin.y() * numY / gridSize.y(), 0, numY - 1);
    z = clamp(relativeOrigin.z() * numZ / gridSize.z(), 0, numZ - 1);

    double dtx = (tx_max - tx_min) / numX;
    double dty = (ty_max - ty_min) / numY;
    double dtz = (tz_max - tz_min) / numZ;

    double tx_next, ty_next, tz_next;
    int ix_step, iy_step, iz_step;
    int ix_stop, iy_stop, iz_stop;

    if (dx > 0) { tx_next = tx_min + (x + 1) * dtx;        ix_step = +1; ix_stop = numX; }
    else        { tx_next = tx_min + (numX - x) * dtx;     ix_step = -1; ix_stop = -1;   }
    if (dx == 0.0) { tx_next = numeric_limits<double>::max(); ix_step = -1; ix_stop = -1; }

    if (dy > 0) { ty_next = ty_min + (y + 1) * dty;        iy_step = +1; iy_stop = numY; }
    else        { ty_next = ty_min + (numY - y) * dty;     iy_step = -1; iy_stop = -1;   }
    if (dy == 0.0) { ty_next = numeric_limits<double>::max(); iy_step = -1; iy_stop = -1; }

    if (dz > 0) { tz_next = tz_min + (z + 1) * dtz;        iz_step = +1; iz_stop = numZ; }
    else        { tz_next = tz_min + (numZ - z) * dtz;     iz_step = -1; iz_stop = -1;   }
    if (dz == 0.0) { tz_next = numeric_limits<double>::max(); iz_step = -1; iz_stop = -1; }

    while (true) {
      RAYTRACER_STATS_INC(gridTraversalSteps);
      const auto& cell = cells[x + numX * y + numX * numY * z];

      // Identify the next axis to step along, and the t at which we exit
      // the current cell along that axis. The visitor needs that t so it
      // can decide whether a hit it found is "in" this cell or "past" it.
      double currentTNext;
      double* axisNext;
      double axisDelta;
      int* axisVar;
      int axisStep, axisStop;

      if (tx_next < ty_next && tx_next < tz_next) {
        currentTNext = tx_next;
        axisNext = &tx_next; axisDelta = dtx;
        axisVar  = &x;       axisStep  = ix_step; axisStop = ix_stop;
      } else if (ty_next < tz_next) {
        currentTNext = ty_next;
        axisNext = &ty_next; axisDelta = dty;
        axisVar  = &y;       axisStep  = iy_step; axisStop = iy_stop;
      } else {
        currentTNext = tz_next;
        axisNext = &tz_next; axisDelta = dtz;
        axisVar  = &z;       axisStep  = iz_step; axisStop = iz_stop;
      }

      if (cell && visit(cell.get(), currentTNext)) {
        return true;
      }

      *axisNext += axisDelta;
      *axisVar  += axisStep;
      if (*axisVar == axisStop) return false;
    }
  }
}

const Primitive* Grid::intersect(const Rayd& ray, HitPointInterval& hitPoints, raytracer::State& state) const {
  if (m_boundingBox.isInfinite()) {
    return nullptr;
  }

  state.recordEvent(this, "Traversing grid");

  const Primitive* hit = nullptr;
  traverseGrid(ray, m_boundingBox, m_numX, m_numY, m_numZ, m_cells,
    [&](const Primitive* cell, double tNext) {
      HitPointInterval candidate;
      const Primitive* primitive = cell->intersect(ray, candidate, state);
      if (candidate.minWithPositiveDistance().distance() - Rayd::epsilon < tNext) {
        hitPoints = candidate;
        hit = primitive;
        return true;
      }
      return false;
    });
  return hit;
}

bool Grid::intersects(const Rayd& ray, raytracer::State& state) const {
  if (m_boundingBox.isInfinite()) {
    return false;
  }

  return traverseGrid(ray, m_boundingBox, m_numX, m_numY, m_numZ, m_cells,
    [&](const Primitive* cell, double /*tNext*/) {
      return cell->intersects(ray, state);
    });
}

void Grid::setup() {
  m_boundingBox = boundingBox();
  if (m_boundingBox.isInfinite()) {
    return;
  }

  Vector3d gridSize = m_boundingBox.max() - m_boundingBox.min();

  // Cells-per-axis density factor — the grid produces roughly
  // kGridDensityMultiplier^3 × primitiveCount cells in total, which trades
  // memory for shorter expected traversal length. 2.0 is the textbook value
  // (see Suffern, "Ray Tracing from the Ground Up", §22.3) and matches the
  // density used by every existing scene in the test suite; tweaks here
  // need a benchmark run.
  static constexpr double kGridDensityMultiplier = 2.0;

  // Degenerate-axis handling. A bbox can be near-zero-thickness in
  // one or more axes (a flat Rectangle in its plane, a Disk on its
  // plane). The textbook s = cbrt(vol/N) collapses to ~0 in that
  // case, and 2 * size / s for the *other* (non-degenerate) axes
  // diverges — for a Rectangle we'd compute ~262k cells along x and
  // z, then m_numX * m_numY * m_numZ overflows int and m_cells.reserve
  // crashes. Treat axes whose extent is well below the largest axis
  // as degenerate (one cell along them), and compute s from the
  // non-degenerate axes only.
  const double maxAxis = std::max({gridSize.x(), gridSize.y(), gridSize.z()});
  const double degenerateThreshold = maxAxis * 1e-6;

  // Effective extent — replace degenerate axes with `maxAxis` so the
  // characteristic-cell-size cube root doesn't collapse. The
  // degenerate axes still get one cell below; this only affects `s`.
  auto eff = [&](double v) {
    return v < degenerateThreshold ? maxAxis : v;
  };
  double s = std::cbrt(eff(gridSize.x()) * eff(gridSize.y()) * eff(gridSize.z())
                       / primitives().size());

  auto cellCount = [&](double size) -> int {
    if (size < degenerateThreshold) return 1;
    return static_cast<int>(kGridDensityMultiplier * size / s) + 1;
  };
  m_numX = cellCount(gridSize.x());
  m_numY = cellCount(gridSize.y());
  m_numZ = cellCount(gridSize.z());

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

    // Cell index for a primitive corner along a single axis. When
    // the axis was deemed degenerate above (numCells == 1), every
    // primitive lives in cell 0 — short-circuit before dividing by
    // a near-zero gridSize, which would otherwise produce NaN.
    auto cellIndex = [](double rel, int numCells, double size) -> int {
      if (numCells <= 1) return 0;
      return static_cast<int>(clamp(rel * numCells / size, 0.0f, float(numCells - 1)));
    };
    int xmin = cellIndex(relativeMin.x(), m_numX, gridSize.x());
    int ymin = cellIndex(relativeMin.y(), m_numY, gridSize.y());
    int zmin = cellIndex(relativeMin.z(), m_numZ, gridSize.z());
    int xmax = cellIndex(relativeMax.x(), m_numX, gridSize.x());
    int ymax = cellIndex(relativeMax.y(), m_numY, gridSize.y());
    int zmax = cellIndex(relativeMax.z(), m_numZ, gridSize.z());

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
            // counts[index] >= 2 means the count==1 branch above already
            // wrapped the existing primitive in a Composite. Capture the
            // cast so a wrong-type cell trips the assertion instead of a
            // silent null-pointer dereference.
            auto composite = std::dynamic_pointer_cast<Composite>(m_cells[index]);
            assert(composite && "Grid cell with count > 1 must hold a Composite");
            composite->add(primitive);
          }
          counts[index]++;
        }
      }
    }
  }

  m_boundingBox = m_boundingBox.grownByEpsilon();
}
