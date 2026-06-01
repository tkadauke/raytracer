#include "render/RayCaster.h"
#include "render/State.h"
#include "render/materials/PortalMaterial.h"
#include "core/math/HitPoint.h"

using namespace std;
using namespace render;

void PortalMaterial::setMatrix(const Matrix4d& matrix) {
  m_originMatrix = matrix.inverted();
  m_directionMatrix = Matrix3d(m_originMatrix);
}

Colord PortalMaterial::shade(const render::RayCaster* raycaster, const render::Scene&,
                             const Rayd& ray, const HitPoint& hitPoint,
                             render::State& state) const {
  return state.withThroughput(state.throughput * m_filterColor.max(), [&] {
    return raycaster->rayColor(redirectedRay(hitPoint, -ray.direction()), state) * m_filterColor;
  });
}

render::WhittedShadeResult PortalMaterial::shadeWhitted(const render::RayCaster*,
                                                        const render::Scene&, const Rayd& ray,
                                                        const HitPoint& hitPoint,
                                                        render::State&) const {
  render::WhittedShadeResult result;
  result.continuations.push_back(render::WhittedContinuation{
    redirectedRay(hitPoint, -ray.direction()), m_filterColor, m_filterColor.max()});
  return result;
}

Colord PortalMaterial::evalBsdf(const HitPoint&, const Vector3d&, const Vector3d&) const {
  return Colord::black();
}

render::MaterialBsdfSample PortalMaterial::sampleBsdf(const HitPoint& hitPoint, const Vector3d& wi,
                                                      const Vector2d&) const {
  render::MaterialBsdfSample result;
  result.continuationRay = redirectedRay(hitPoint, wi);
  result.direction = result.continuationRay->direction().normalized();
  result.value = m_filterColor;
  result.pdf = 1.0;
  result.isDelta = true;
  return result;
}

double PortalMaterial::bsdfPdf(const HitPoint&, const Vector3d&, const Vector3d&) const {
  return 0.0;
}

Rayd PortalMaterial::redirectedRay(const HitPoint& hitPoint, const Vector3d& wi) const {
  return transformedRay(Rayd(hitPoint.point(), -wi).epsilonShifted());
}
