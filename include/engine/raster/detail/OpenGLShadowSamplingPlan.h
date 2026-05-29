#pragma once

#include <string>

namespace engine::raster::detail {
  struct DirectionalShadowCascade;
  class DirectionalShadowMap;
  class ShadowMaps;

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
