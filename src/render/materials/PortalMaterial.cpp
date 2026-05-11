#include "render/RayCaster.h"
#include "render/State.h"
#include "render/materials/PortalMaterial.h"
#include "render/RayCaster.h"
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
    return raycaster->rayColor(transformedRay(ray.from(hitPoint.point()).epsilonShifted()), state) *
           m_filterColor;
  });
}
