#include "render/State.h"
#include "render/primitives/FlatMeshTriangle.h"
#include "render/primitives/Composite.h"
#include "render/primitives/detail/TriangleIntersection.h"
#include "core/geometry/Mesh.h"
#include "core/math/Ray.h"
#include "core/math/HitPointInterval.h"

using namespace render;

void FlatMeshTriangle::build(const Mesh* mesh, Composite* composite,
                             std::shared_ptr<render::Material> material) {
  for (const auto& triangle : *mesh) {
    auto primitive =
      std::make_shared<FlatMeshTriangle>(mesh, triangle[0], triangle[1], triangle[2]);
    primitive->setMaterial(material);
    composite->add(primitive);
  }
}

const Primitive* FlatMeshTriangle::intersect(const Rayd& ray, HitPointInterval& hitPoints,
                                             render::State& state) const {
  Vector3d v0(m_mesh->vertices()[m_index0].point);
  Vector3d v1(m_mesh->vertices()[m_index1].point);
  Vector3d v2(m_mesh->vertices()[m_index2].point);

  const auto solution = detail::solveTriangleBarycentric(v0, v1, v2, ray);
  const double beta = solution.beta;

  if (beta < 0.0 || beta > 1.0) {
    state.miss(this, "FlatMeshTriangle, beta not in [0, 1]");
    return nullptr;
  }

  const double gamma = solution.gamma;

  if (gamma < 0.0 || gamma > 1.0) {
    state.miss(this, "FlatMeshTriangle, gamma not in [0, 1]");
    return nullptr;
  }

  if (beta + gamma > 1.0) {
    state.miss(this, "FlatMeshTriangle, beta + gamma > 1");
    return nullptr;
  }

  double t = solution.distance;

  if (t < 0.0001) {
    state.miss(this, "FlatMeshTriangle, behind ray");
    return nullptr;
  }

  Vector3d hitPoint = ray.at(t);
  const double alpha = 1.0 - beta - gamma;
  const Vector2d uv = m_mesh->vertices()[m_index0].uv * alpha +
                      m_mesh->vertices()[m_index1].uv * beta +
                      m_mesh->vertices()[m_index2].uv * gamma;
  hitPoints.add(HitPoint(this, t, hitPoint, m_normal, uv));
  state.hit(this, "FlatMeshTriangle");
  return this;
}

std::shared_ptr<Mesh> FlatMeshTriangle::tessellate(int) const {
  auto mesh = std::make_shared<Mesh>();
  const auto& v0 = m_mesh->vertices()[m_index0];
  const auto& v1 = m_mesh->vertices()[m_index1];
  const auto& v2 = m_mesh->vertices()[m_index2];
  mesh->addVertex(v0.point, m_normal, v0.uv);
  mesh->addVertex(v1.point, m_normal, v1.uv);
  mesh->addVertex(v2.point, m_normal, v2.uv);
  mesh->addFace({0, 1, 2}, m_faceMetadata);
  return mesh;
}

Vector3d FlatMeshTriangle::normalAtBarycentric(double, double) const {
  return m_normal;
}

double FlatMeshTriangle::minimumHitDistance() const {
  return 0.0001;
}

Vector3d FlatMeshTriangle::computeNormal() const {
  Vector3d n0(m_mesh->vertices()[m_index0].normal);
  Vector3d n1(m_mesh->vertices()[m_index1].normal);
  Vector3d n2(m_mesh->vertices()[m_index2].normal);
  const Vector3d sourceNormal = (n0 + n1 + n2).normalizedOrZero(1.0e-12);
  if (sourceNormal != Vector3d::null)
    return sourceNormal;

  const Vector3d& p0 = m_mesh->vertices()[m_index0].point;
  const Vector3d& p1 = m_mesh->vertices()[m_index1].point;
  const Vector3d& p2 = m_mesh->vertices()[m_index2].point;
  return ((p1 - p0) ^ (p2 - p0)).normalizedOrZero(1.0e-12);
}
