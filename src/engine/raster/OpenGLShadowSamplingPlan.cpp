#include "engine/raster/detail/OpenGLShadowSamplingPlan.h"

#include "engine/raster/detail/RasterShadowMaps.h"
#include "render/primitives/Scene.h"

#include <utility>

namespace engine::raster::detail {

  OpenGLShadowSamplingPlan OpenGLShadowSamplingPlan::from(const ShadowMaps* shadowMaps) {
    if (!shadowMaps || shadowMaps->empty()) {
      return disabled("no directional shadow maps");
    }

    const auto& directional = shadowMaps->directionalMaps();
    if (directional.size() != 1) {
      return disabled("shader-side OpenGL shadows require exactly one directional shadow map");
    }

    const DirectionalShadowMap& shadowMap = directional.front();
    if (shadowMap.cascades().size() != 1) {
      return disabled("shader-side OpenGL shadows require exactly one directional shadow cascade");
    }
    if (shadowMap.filterMode() == Rasterizer::ShadowFilterMode::PCSS) {
      return disabled("shader-side OpenGL shadows do not support PCSS filtering yet");
    }
    if (shadowMap.filterRadius() > 4) {
      return disabled("shader-side OpenGL shadows support PCF filter radius up to 4");
    }
    if (shadowMap.slopeBias() != 0.0) {
      return disabled("shader-side OpenGL shadows require constant bias");
    }
    const DirectionalShadowCascade& cascade = shadowMap.cascades().front();
    if (!cascade.camera || !cascade.depthBuffer) {
      return disabled("shader-side OpenGL shadows require a complete shadow cascade");
    }

    return enabled(shadowMap, cascade);
  }

  bool OpenGLShadowSamplingPlan::enabled() const {
    return m_shadowMap != nullptr && m_cascade != nullptr;
  }

  const std::string& OpenGLShadowSamplingPlan::disabledReason() const {
    return m_disabledReason;
  }

  bool OpenGLShadowSamplingPlan::canShadeSceneDirectLighting(const render::Scene* scene) const {
    return shaderLightingDisabledReason(scene).empty();
  }

  std::string
  OpenGLShadowSamplingPlan::shaderLightingDisabledReason(const render::Scene* scene) const {
    if (!enabled()) {
      return m_disabledReason;
    }
    if (!scene) {
      return "shader-side OpenGL shadows require a scene";
    }

    const auto& lights = scene->lights();
    if (lights.size() != 1) {
      return "shader-side OpenGL shadows require exactly one scene light until light-channel "
             "splitting lands";
    }
    if (lights.front().get() != m_shadowMap->light()) {
      return "shader-side OpenGL shadows require the eligible shadow map to own the only scene "
             "light";
    }
    return {};
  }

  std::string OpenGLShadowSamplingPlan::traceMessage() const {
    if (enabled()) {
      return "OpenGL raster shadow sampling is eligible for shader-side binding; current pass "
             "still uses CPU-prepared shadow visibility";
    }
    return "OpenGL raster shadow sampling falls back to CPU-prepared visibility: " +
           m_disabledReason;
  }

  std::string OpenGLShadowSamplingPlan::traceMessage(const render::Scene* scene) const {
    if (!enabled()) {
      return traceMessage();
    }

    const std::string reason = shaderLightingDisabledReason(scene);
    if (!reason.empty()) {
      return "OpenGL raster shadow sampling falls back to CPU-prepared visibility: " + reason;
    }

    return "OpenGL raster shadow sampling uses shader-side binding for one directional shadow map";
  }

  const DirectionalShadowMap* OpenGLShadowSamplingPlan::shadowMap() const {
    return m_shadowMap;
  }

  const DirectionalShadowCascade* OpenGLShadowSamplingPlan::cascade() const {
    return m_cascade;
  }

  OpenGLShadowSamplingPlan OpenGLShadowSamplingPlan::disabled(std::string reason) {
    OpenGLShadowSamplingPlan plan;
    plan.m_disabledReason = std::move(reason);
    return plan;
  }

  OpenGLShadowSamplingPlan
  OpenGLShadowSamplingPlan::enabled(const DirectionalShadowMap& shadowMap,
                                    const DirectionalShadowCascade& cascade) {
    OpenGLShadowSamplingPlan plan;
    plan.m_shadowMap = &shadowMap;
    plan.m_cascade = &cascade;
    return plan;
  }

}
