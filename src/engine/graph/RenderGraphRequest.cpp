#include "engine/graph/RenderGraphRequest.h"

#include "engine/graph/RenderGraphCompiler.h"

#include <algorithm>
#include <utility>

namespace engine::graph {
  RenderGraphRequest::RenderGraphRequest() = default;

  RenderGraphRequest::RenderGraphRequest(RenderIntent baseIntent)
      : m_baseIntent(std::move(baseIntent)) {
  }

  const RenderIntent& RenderGraphRequest::baseIntent() const {
    return m_baseIntent;
  }

  RenderGraphRequest& RenderGraphRequest::setBaseIntent(RenderIntent intent) {
    m_baseIntent = std::move(intent);
    return *this;
  }

  const RenderSceneAnalysis& RenderGraphRequest::sceneAnalysis() const {
    return m_sceneAnalysis;
  }

  RenderGraphRequest& RenderGraphRequest::setSceneAnalysis(RenderSceneAnalysis analysis) {
    m_sceneAnalysis = std::move(analysis);
    return *this;
  }

  RenderGraphRequest& RenderGraphRequest::clearSceneAnalysis() {
    m_sceneAnalysis = RenderSceneAnalysis::unknownScene();
    return *this;
  }

  RenderGraphRequest& RenderGraphRequest::setExecutorOverride(RenderExecutorPreference executor) {
    m_executorOverride = executor;
    return *this;
  }

  RenderGraphRequest& RenderGraphRequest::clearExecutorOverride() {
    m_executorOverride.reset();
    return *this;
  }

  RenderGraphRequest&
  RenderGraphRequest::setExecutorShortcut(RenderExecutorPreference executor,
                                          bool wireframeExecutorSelectsWireframeView) {
    m_executorShortcut = executor;
    m_wireframeExecutorSelectsWireframeView = wireframeExecutorSelectsWireframeView;
    return *this;
  }

  RenderGraphRequest& RenderGraphRequest::clearExecutorShortcut() {
    m_executorShortcut.reset();
    m_wireframeExecutorSelectsWireframeView = true;
    return *this;
  }

  RenderGraphRequest& RenderGraphRequest::setViewModeOverride(RenderViewMode viewMode) {
    m_viewModeOverride = viewMode;
    return *this;
  }

  RenderGraphRequest& RenderGraphRequest::clearViewModeOverride() {
    m_viewModeOverride.reset();
    return *this;
  }

  RenderGraphRequest& RenderGraphRequest::setCameraOverride(RenderCameraRef camera) {
    m_cameraOverride = std::move(camera);
    return *this;
  }

  RenderGraphRequest& RenderGraphRequest::clearCameraOverride() {
    m_cameraOverride.reset();
    return *this;
  }

  RenderGraphRequest& RenderGraphRequest::setShadingProfileOverride(ShadingProfileRef profile) {
    m_shadingProfileOverride = std::move(profile);
    return *this;
  }

  RenderGraphRequest& RenderGraphRequest::clearShadingProfileOverride() {
    m_shadingProfileOverride.reset();
    return *this;
  }

  RenderGraphRequest&
  RenderGraphRequest::setShadingProfileParameterOverride(std::string key,
                                                         ShadingProfileParameterValue value) {
    m_shadingProfileParameterOverrides.insert_or_assign(std::move(key), std::move(value));
    return *this;
  }

  RenderGraphRequest& RenderGraphRequest::setPostProcessAAOverride(RenderPostProcessAA aa) {
    m_postProcessAAOverride = aa;
    return *this;
  }

  RenderGraphRequest& RenderGraphRequest::clearPostProcessAAOverride() {
    m_postProcessAAOverride.reset();
    return *this;
  }

  RenderGraphRequest& RenderGraphRequest::setWireframeOverlayOverride(bool enabled) {
    m_wireframeOverlayOverride = enabled;
    return *this;
  }

  RenderGraphRequest& RenderGraphRequest::clearWireframeOverlayOverride() {
    m_wireframeOverlayOverride.reset();
    return *this;
  }

  RenderGraphRequest& RenderGraphRequest::setCurveOverlayOverride(bool enabled) {
    m_curveOverlayOverride = enabled;
    return *this;
  }

  RenderGraphRequest& RenderGraphRequest::clearCurveOverlayOverride() {
    m_curveOverlayOverride.reset();
    return *this;
  }

  RenderGraphRequest& RenderGraphRequest::setPreviewShadowsOverride(bool enabled) {
    m_previewShadowsOverride = enabled;
    return *this;
  }

  RenderGraphRequest& RenderGraphRequest::clearPreviewShadowsOverride() {
    m_previewShadowsOverride.reset();
    return *this;
  }

  RenderGraphRequest& RenderGraphRequest::requestExportedAOV(RenderViewMode viewMode) {
    if (std::find(m_exportedAOVs.begin(), m_exportedAOVs.end(), viewMode) == m_exportedAOVs.end()) {
      m_exportedAOVs.push_back(viewMode);
    }
    return *this;
  }

  RenderGraphRequest& RenderGraphRequest::addViewOverride(RenderViewOverride viewOverride) {
    m_viewOverrides.push_back(std::move(viewOverride));
    return *this;
  }

  RenderIntent RenderGraphRequest::resolvedIntent() const {
    RenderIntent intent = m_baseIntent;

    if (m_executorOverride) {
      intent.setDefaultExecutor(*m_executorOverride);
    } else if (m_executorShortcut) {
      intent.setDefaultExecutor(*m_executorShortcut);
    }

    if (m_viewModeOverride) {
      intent.setDefaultViewMode(*m_viewModeOverride);
    } else if (m_executorShortcut == RenderExecutorPreference::Wireframe &&
               m_wireframeExecutorSelectsWireframeView) {
      intent.setDefaultViewMode(RenderViewMode::Wireframe);
    }

    if (m_cameraOverride) {
      intent.setDefaultCamera(*m_cameraOverride);
    }
    if (m_shadingProfileOverride) {
      intent.setDefaultShadingProfile(*m_shadingProfileOverride);
    }
    for (const auto& [key, value] : m_shadingProfileParameterOverrides) {
      intent.setDefaultShadingProfileParameter(key, value);
    }
    if (m_postProcessAAOverride) {
      intent.setPostProcessAA(*m_postProcessAAOverride);
    }
    if (m_wireframeOverlayOverride) {
      intent.setWireframeOverlayEnabled(*m_wireframeOverlayOverride);
    }
    if (m_curveOverlayOverride) {
      intent.enableCurveOverlay = *m_curveOverlayOverride;
    }
    if (m_previewShadowsOverride) {
      intent.setPreviewShadowsEnabled(*m_previewShadowsOverride);
    }
    for (const auto viewMode : m_exportedAOVs) {
      intent.requestExportedAOV(viewMode);
    }
    intent.viewOverrides.insert(intent.viewOverrides.end(), m_viewOverrides.begin(),
                                m_viewOverrides.end());
    return intent;
  }

  RenderPlan RenderGraphRequest::compile(const RenderTargetSpec& target) const {
    RenderGraphCompiler compiler;
    return compiler.compile(target, resolvedIntent(), m_sceneAnalysis);
  }
}
