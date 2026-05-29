#pragma once

#include <string>

namespace engine::raster::detail {
  struct DirectionalShadowCascade;
  class DirectionalShadowMap;
  class ShadowMaps;
}

namespace render {
  class Scene;
}

namespace engine::raster::detail {
  /**
    * Supported subset for shader-side OpenGL shadow sampling.
    *
    * The first shader path intentionally handles one hard-filtered directional
    * shadow cascade. More complex cascades and filters can expand this object
    * without hiding capability decisions inside draw code.
    */
  class OpenGLShadowSamplingPlan {
  public:
    static OpenGLShadowSamplingPlan from(const ShadowMaps* shadowMaps);

    bool enabled() const;
    const std::string& disabledReason() const;
    bool canShadeSceneDirectLighting(const render::Scene* scene) const;
    std::string shaderLightingDisabledReason(const render::Scene* scene) const;
    std::string traceMessage() const;
    std::string traceMessage(const render::Scene* scene) const;
    const DirectionalShadowMap* shadowMap() const;
    const DirectionalShadowCascade* cascade() const;

  private:
    static OpenGLShadowSamplingPlan disabled(std::string reason);
    static OpenGLShadowSamplingPlan enabled(const DirectionalShadowMap& shadowMap,
                                            const DirectionalShadowCascade& cascade);

    const DirectionalShadowMap* m_shadowMap{nullptr};
    const DirectionalShadowCascade* m_cascade{nullptr};
    std::string m_disabledReason;
  };
}
