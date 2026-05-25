#include "engine/raster/detail/RasterMaterialEvaluator.h"

#include "core/math/Ray.h"
#include "render/State.h"
#include "render/cameras/Camera.h"
#include "render/lights/Light.h"
#include "render/primitives/Scene.h"

#include <algorithm>
#include <cmath>

namespace engine::raster::detail {

  MaterialEvaluator::MaterialEvaluator(const render::Scene* scene, const ShadowMaps* shadowMaps,
                                       const render::Camera* camera)
      : m_scene(scene),
        m_camera(camera) {
    m_lights.reserve(static_cast<std::size_t>(m_scene->lights().size()));
    for (const auto& light : m_scene->lights()) {
      const render::Light* lightPtr = light.get();
      const DirectionalShadowMap* shadowMap = shadowMaps ? shadowMaps->forLight(lightPtr) : nullptr;
      m_lights.push_back(
        {lightPtr, lightPtr->radiance(), shadowMap, shadowMaps != nullptr && shadowMap == nullptr});
    }
  }

  RasterFragment MaterialEvaluator::shade(const RasterTriangle& triangle, int x, int y,
                                          const InterpolatedFragment& fragment) const {
    return shade(triangle.rasterMaterial, triangle.primitive, fragment.worldPos, fragment.normal,
                 fragment.uv, triangle.uvDx, triangle.uvDy, triangle.tangentFrame, x, y);
  }

  RasterFragment
  MaterialEvaluator::shade(const RasterMaterial& rasterMaterial, const render::Primitive* primitive,
                           const Vector3d& worldPos, const Vector3d& normal, const Vector2d& uv,
                           const Vector2d& uvDx, const Vector2d& uvDy,
                           const RasterTangentFrame& tangentFrame, int x, int y) const {
    const Vector3d baseNormal = normal.normalized();
    const Vector3d n =
      rasterMaterial.lightingNormal(primitive, worldPos, baseNormal, uv, uvDx, uvDy, tangentFrame);
    const Colord albedo = rasterMaterial.albedo(primitive, worldPos, baseNormal, uv, uvDx, uvDy);
    const double alpha = rasterMaterial.alpha(primitive, worldPos, baseNormal, uv, uvDx, uvDy);
    const bool hasSpecular = rasterMaterial.hasSpecular() && m_camera;
    const Vector3d viewDir =
      hasSpecular ? (-m_camera->rayForPixel(x, y).direction()).normalized() : Vector3d::undefined;

    // Raster shadow maps only mask direct material light. Ambient remains
    // visible because it models light not explained by the direct-light pass.
    Colord shaded = m_scene->ambient() * rasterMaterial.ambientCoefficient() * albedo;
    for (const auto& light : m_lights) {
      const Vector3d lightDir = light.light->direction(worldPos);
      const double nDotL = std::max(0.0, n * lightDir);
      if (nDotL > 0.0) {
        const double visibility = visibilityFor(light, worldPos, n, lightDir);
        if (visibility > 0.0) {
          Colord direct = albedo * rasterMaterial.diffuseCoefficient() * light.radiance * nDotL;
          if (hasSpecular) {
            const Vector3d lobeDirection = (-lightDir + n * 2.0 * nDotL).normalized();
            const double lobeDotView = std::max(0.0, lobeDirection * viewDir);
            if (lobeDotView > 0.0) {
              direct += rasterMaterial.specularColor() * rasterMaterial.specularCoefficient() *
                        std::pow(lobeDotView, rasterMaterial.specularExponent()) * light.radiance *
                        nDotL;
            }
          }
          shaded += direct * visibility;
        }
      }
    }
    return {shaded, alpha};
  }

  double MaterialEvaluator::visibilityFor(const PreparedRasterLight& light,
                                          const Vector3d& worldPos, const Vector3d& normal,
                                          const Vector3d& lightDir) const {
    if (light.shadowMap) {
      return light.shadowMap->visibility(worldPos, normal, lightDir);
    }
    if (!light.traceVisibility) {
      return 1.0;
    }

    render::State state;
    return m_scene->intersects(Rayd(worldPos, lightDir).epsilonShifted(), state) ? 0.0 : 1.0;
  }
}
