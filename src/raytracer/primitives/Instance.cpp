#include "raytracer/State.h"
#include "raytracer/primitives/Instance.h"
#include "core/math/Ray.h"
#include "core/math/HitPointInterval.h"

#include <vector>

using namespace std;
using namespace raytracer;

const Primitive* Instance::intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) const {
  // Static fast path — no motion blur math when velocity is zero.
  // Most instances in any given scene fall through here, so the
  // branch keeps the cost of the new feature to one comparison per
  // ray for unanimated geometry.
  if (m_velocity == Vector3d::null()) {
    const Primitive* result = m_primitive->intersect(instancedRay(ray), hitPoints, state);
    if (result) {
      hitPoints = hitPoints.transform(m_pointMatrix, m_normalMatrix);
      if (Primitive::material()) {
        return this;
      } else {
        return result;
      }
    }
    return nullptr;
  }

  // Motion-blur path — the instance has translated by `velocity *
  // timeSample` since t=0. Equivalently, the ray has translated by
  // the negative of that in the instance's frame: shift the ray's
  // world origin by `-velocity * timeSample` before transforming
  // into local space (direction is unaffected by translation), then
  // build a `pointMatrix_at_t = pointMatrix + translate(velocity *
  // timeSample)` to map the resulting hit points back to world.
  Vector3d shift = m_velocity * state.timeSample;
  Vector4d shift4(shift.x(), shift.y(), shift.z(), 0.0);
  Rayd localRay(
    m_originMatrix * (ray.origin() - shift4),
    m_directionMatrix * ray.direction()
  );

  const Primitive* result = m_primitive->intersect(localRay, hitPoints, state);
  if (result) {
    Matrix4d pointMatrixAtTime = m_pointMatrix;
    pointMatrixAtTime.setCell(0, 3, m_pointMatrix.cell(0, 3) + shift.x());
    pointMatrixAtTime.setCell(1, 3, m_pointMatrix.cell(1, 3) + shift.y());
    pointMatrixAtTime.setCell(2, 3, m_pointMatrix.cell(2, 3) + shift.z());
    hitPoints = hitPoints.transform(pointMatrixAtTime, m_normalMatrix);
    if (Primitive::material()) {
      return this;
    } else {
      return result;
    }
  }
  return nullptr;
}

bool Instance::intersects(const Rayd& ray, State& state) const {
  if (m_velocity == Vector3d::null()) {
    return m_primitive->intersects(instancedRay(ray), state);
  }
  Vector3d shift = m_velocity * state.timeSample;
  Vector4d shift4(shift.x(), shift.y(), shift.z(), 0.0);
  Rayd localRay(
    m_originMatrix * (ray.origin() - shift4),
    m_directionMatrix * ray.direction()
  );
  return m_primitive->intersects(localRay, state);
}

void Instance::setMatrix(const Matrix4d& matrix) {
  m_pointMatrix = matrix;
  m_originMatrix = matrix.inverted();
  m_directionMatrix = Matrix3d(m_originMatrix);
  m_normalMatrix = m_directionMatrix.transposed();
}

void Instance::setVelocity(const Vector3d& velocity) {
  m_velocity = velocity;
}

std::shared_ptr<Material> Instance::material() const {
  auto parent = Primitive::material();
  if (parent)
    return parent;
  else
    return m_primitive->material();
}

BoundingBoxd Instance::calculateBoundingBox() const {
  BoundingBoxd original = m_primitive->boundingBox();
  vector<Vector3d> vertices;
  original.getVertices(vertices);

  BoundingBoxd result;
  for (const auto& vertex : vertices) {
    result.include(m_pointMatrix * Vector4d(vertex));
  }

  // For animated instances, the bbox must cover every position the
  // primitive occupies during the shutter — include the
  // end-of-shutter position too. Without this, a bbox-based
  // accelerator (Grid) could miss rays whose timeSample puts them
  // outside the static bbox.
  if (m_velocity != Vector3d::null()) {
    BoundingBoxd shifted;
    for (const auto& vertex : vertices) {
      shifted.include(Vector3d(m_pointMatrix * Vector4d(vertex)) + m_velocity);
    }
    result.include(shifted);
  }

  return result;
}

Vector3d Instance::farthestPoint(const Vector3d& direction) const {
  return m_pointMatrix * m_primitive->farthestPoint(m_directionMatrix * direction);
}
