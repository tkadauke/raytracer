#pragma once

#include "engine/raster/detail/RasterPipelineTypes.h"
#include "engine/raster/detail/RasterShadowMaps.h"

#include "core/Color.h"
#include "core/math/Vector.h"

#include <vector>

namespace render {
  class Camera;
  class Light;
  class Primitive;
  class Scene;
}

namespace engine::raster::detail {

  // Per-pass light payload for the built-in fragment path. Shadow maps are
  // bound here once when the pass state is frozen, so fragment shading does not
  // search the frame-level shadow-map collection for every light contribution.
  struct PreparedRasterLight {
    const render::Light* light;
    Colord radiance;
    const DirectionalShadowMap* shadowMap;
    bool traceVisibility;
  };

  // Built-in material shader for the fixed-function raster path. It handles the
  // direct-lighting subset the rasterizer can preview: Matte ambient/diffuse
  // terms and Phong-style specular highlights, optionally masked by the frame's
  // directional shadow maps or by a per-light visibility fallback when a light
  // has no shadow-map resource.
  class MaterialEvaluator {
  public:
    explicit MaterialEvaluator(const render::Scene* scene, const ShadowMaps* shadowMaps,
                               const render::Camera* camera);

    RasterFragment shade(const RasterTriangle& triangle, int x, int y,
                         const InterpolatedFragment& fragment) const;

    RasterFragment shade(const RasterMaterial& rasterMaterial, const render::Primitive* primitive,
                         const Vector3d& worldPos, const Vector3d& normal, const Vector2d& uv,
                         const Vector2d& uvDx, const Vector2d& uvDy,
                         const RasterTangentFrame& tangentFrame, int x, int y) const;

  private:
    double visibilityFor(const PreparedRasterLight& light, const Vector3d& worldPos,
                         const Vector3d& normal, const Vector3d& lightDir) const;

    const render::Scene* m_scene;
    const render::Camera* m_camera;
    std::vector<PreparedRasterLight> m_lights;
  };

}
