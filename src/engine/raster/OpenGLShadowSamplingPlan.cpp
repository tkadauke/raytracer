#include "engine/raster/detail/OpenGLShadowSamplingPlan.h"

#include "engine/raster/detail/RasterShadowMaps.h"

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
    if (shadowMap.filterRadius() != 0) {
      return disabled("shader-side OpenGL shadows require hard filtering");
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

  std::string OpenGLShadowSamplingPlan::traceMessage() const {
    if (enabled()) {
      return "OpenGL raster shadow sampling is eligible for shader-side binding; current pass "
             "still uses CPU-prepared shadow visibility";
    }
    return "OpenGL raster shadow sampling falls back to CPU-prepared visibility: " +
           m_disabledReason;
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
