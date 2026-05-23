#pragma once

#include "RasterPipelineTypes.h"
#include "RasterShadowMaps.h"

#include "core/Color.h"
#include "core/math/Vector.h"
#include "render/lights/Light.h"
#include "render/primitives/Scene.h"

#include <algorithm>
#include <vector>

namespace engine::raster::detail {

  // Ambient coefficient used by the built-in raster fragment path. It mirrors
  // MatteMaterial's default ambient behavior so unlit surfaces receive the
  // scene ambient contribution at full albedo strength.
  inline constexpr double kAmbientCoefficient = 1.0;

  // Per-pass light payload for the built-in fragment path. Shadow maps are
  // bound here once when the pass state is frozen, so fragment shading does not
  // search the frame-level shadow-map collection for every light contribution.
  struct PreparedRasterLight {
    const render::Light* light;
    Colord radiance;
    const DirectionalShadowMap* shadowMap;
  };

  // Built-in material shader for the fixed-function raster path. It deliberately
  // stays modest: raster material albedo plus ambient and direct Lambertian
  // lights, optionally masked by the frame's directional shadow maps.
  class MaterialEvaluator {
  public:
    explicit MaterialEvaluator(const render::Scene* scene, const ShadowMaps* shadowMaps)
        : m_scene(scene) {
      m_lights.reserve(static_cast<std::size_t>(m_scene->lights().size()));
      for (const auto& light : m_scene->lights()) {
        const render::Light* lightPtr = light.get();
        m_lights.push_back({lightPtr, lightPtr->radiance(),
                            shadowMaps ? shadowMaps->forLight(lightPtr) : nullptr});
      }
    }

    Colord shade(const RasterTriangle& triangle, const InterpolatedFragment& fragment) const {
      return shade(triangle.rasterMaterial, triangle.primitive, fragment.worldPos, fragment.normal,
                   fragment.uv);
    }

    Colord shade(const RasterMaterial& rasterMaterial, const render::Primitive* primitive,
                 const Vector3d& worldPos, const Vector3d& normal, const Vector2d& uv) const {
      const Vector3d n = normal.normalized();
      const Colord albedo = rasterMaterial.albedo(primitive, worldPos, n, uv);

      // Raster shadow maps only mask direct diffuse light. Ambient remains
      // visible because it models light not explained by the direct-light pass.
      Colord shaded = m_scene->ambient() * kAmbientCoefficient * albedo;
      for (const auto& light : m_lights) {
        const Vector3d lightDir = light.light->direction(worldPos);
        const double nDotL = std::max(0.0, n * lightDir);
        if (nDotL > 0.0) {
          const double visibility =
            light.shadowMap ? light.shadowMap->visibility(worldPos, n, lightDir) : 1.0;
          if (visibility > 0.0)
            shaded += albedo * light.radiance * nDotL * visibility;
        }
      }
      return shaded;
    }

  private:
    const render::Scene* m_scene;
    std::vector<PreparedRasterLight> m_lights;
  };

}
