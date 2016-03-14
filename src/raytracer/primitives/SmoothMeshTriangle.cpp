#include "raytracer/State.h"
#include "raytracer/primitives/SmoothMeshTriangle.h"
#include "raytracer/primitives/Mesh.h"
#include "core/math/Ray.h"
#include "core/math/HitPointInterval.h"

using namespace raytracer;

static int mod3[] = { 0, 1, 2, 0, 1, 2 };

SmoothMeshTriangle::SmoothMeshTriangle(Mesh* mesh, int index0, int index1, int index2)
  : MeshTriangle(mesh, index0, index1, index2)
{
  const Vector3d& A = m_mesh->vertices[m_index0].point;
  const Vector3d& B = m_mesh->vertices[m_index1].point;
  const Vector3d& C = m_mesh->vertices[m_index2].point;
  const Vector3d c = B - A;
  const Vector3d b = C - A;
  const Vector3d N = b ^ c;
  
  if (fabs(N.x()) > fabs(N.y())) {
    if (fabs(N.x()) > fabs(N.z()))
      k = 0;
    else
      k = 2;
  } else {
    if (fabs(N.y()) > fabs(N.z()))
      k = 1;
    else
      k = 2;
  }
  int u, v;
  u = mod3[k + 1];
  v = mod3[k + 2];
  // precomp
  double krec = 1.0f / N[k];
  nu = N[u] * krec;
  nv = N[v] * krec;
  nd = N * A * krec;
  // first line equation
  double reci = 1.0 / (b[u] * c[v] - b[v] * c[u]);
  bnu = b[u] * reci;
  bnv = -b[v] * reci;
  // second line equation
  cnu = c[v] * reci;
  cnv = -c[u] * reci;
}

Primitive* SmoothMeshTriangle::intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) {
  int ku = mod3[k + 1];
  int kv = mod3[k + 2];
  
  const Vector4d& O = ray.origin();
  const Vector3d& D = ray.direction();
  const Vector3d& A = m_mesh->vertices[m_index0].point;
  
  const double lnd = 1.0f / (D[k] + nu * D[ku] + nv * D[kv]);
  
  const double t = (nd - O[k] - nu * O[ku] - nv * O[kv]) * lnd;
  if (t < 0) {
    state.miss("SmoothMeshTriangle, behind ray");
    return nullptr;
  }
  
  const double hu = O[ku] + t * D[ku] - A[ku];
  const double hv = O[kv] + t * D[kv] - A[kv];
  
  const double beta = hv * bnu + hu * bnv;
  if (beta < 0 || beta > 1) {
    state.miss("SmoothMeshTriangle, beta not in [0, 1]");
    return nullptr;
  }
  
  const double gamma = hu * cnu + hv * cnv;
  if (gamma < 0 || (beta + gamma) > 1) {
    state.miss("SmoothMeshTriangle, gamma < 0 or beta + gamma > 1");
    return nullptr;
  }
  
  hitPoints.add(HitPoint(this, t, ray.at(t), interpolateNormal(beta, gamma)));
  state.hit("SmoothMeshTriangle");
  return this;
}

bool SmoothMeshTriangle::intersects(const Rayd& ray) {
  int ku = mod3[k + 1];
  int kv = mod3[k + 2];
  
  const Vector4d& O = ray.origin();
  const Vector3d& D = ray.direction();
  const Vector3d& A = m_mesh->vertices[m_index0].point;
  
  const double lnd = 1.0f / (D[k] + nu * D[ku] + nv * D[kv]);
  
  const double t = (nd - O[k] - nu * O[ku] - nv * O[kv]) * lnd;
  if (t < 0)
    return false;
  
  const double hu = O[ku] + t * D[ku] - A[ku];
  const double hv = O[kv] + t * D[kv] - A[kv];
  
  const double beta = hv * bnu + hu * bnv;
  if (beta < 0 || beta > 1)
    return false;
  
  const double gamma = hu * cnu + hv * cnv;
  if (gamma < 0 || (beta + gamma) > 1)
    return false;
  
  return true;
}

Vector3d SmoothMeshTriangle::interpolateNormal(float beta, float gamma) const {
  Vector3d normal(
    m_mesh->vertices[m_index0].normal * (1 - beta - gamma) +
    m_mesh->vertices[m_index1].normal * beta +
    m_mesh->vertices[m_index2].normal * gamma
  );
  return normal.normalized();
}
