#pragma once

#include "engine/graph/RenderGraphCompiler.h"
#include "engine/graph/RenderSceneAnalysis.h"
#include "engine/raster/RasterBackend.h"

#include <optional>
#include <string>
#include <vector>

namespace engine::graph {
  /**
    * Shared render-graph request resolver for front ends.
    *
    * A scene owns durable `RenderIntent`; rendercli, Modeler preview, and
    * render dialogs add temporary caller/UI overrides. Keeping that layering in
    * one class prevents each front end from inventing slightly different graph
    * defaults.
    */
  class RenderGraphRequest {
  public:
    RenderGraphRequest();
    explicit RenderGraphRequest(RenderIntent baseIntent);

    const RenderIntent& baseIntent() const;
    RenderGraphRequest& setBaseIntent(RenderIntent intent);
    const RenderSceneAnalysis& sceneAnalysis() const;
    RenderGraphRequest& setSceneAnalysis(RenderSceneAnalysis analysis);
    RenderGraphRequest& clearSceneAnalysis();

    RenderGraphRequest& setExecutorOverride(RenderExecutorPreference executor);
    RenderGraphRequest& clearExecutorOverride();
    RenderGraphRequest& setExecutorShortcut(RenderExecutorPreference executor,
                                            bool wireframeExecutorSelectsWireframeView = true);
    RenderGraphRequest& clearExecutorShortcut();

    RenderGraphRequest& setViewModeOverride(RenderViewMode viewMode);
    RenderGraphRequest& clearViewModeOverride();
    RenderGraphRequest& setCameraOverride(RenderCameraRef camera);
    RenderGraphRequest& clearCameraOverride();
    RenderGraphRequest& setShadingProfileOverride(ShadingProfileRef profile);
    RenderGraphRequest& clearShadingProfileOverride();
    RenderGraphRequest& setShadingProfileParameterOverride(std::string key,
                                                           ShadingProfileParameterValue value);
    RenderGraphRequest& setPostProcessAAOverride(RenderPostProcessAA aa);
    RenderGraphRequest& clearPostProcessAAOverride();
    RenderGraphRequest& setWireframeOverlayOverride(bool enabled);
    RenderGraphRequest& clearWireframeOverlayOverride();
    RenderGraphRequest& setCurveOverlayOverride(bool enabled);
    RenderGraphRequest& clearCurveOverlayOverride();
    RenderGraphRequest& setPreviewShadowsOverride(bool enabled);
    RenderGraphRequest& clearPreviewShadowsOverride();
    RenderGraphRequest& setRasterBackendOverride(engine::raster::RasterBackend backend);
    RenderGraphRequest& clearRasterBackendOverride();
    RenderGraphRequest& requestExportedAOV(RenderViewMode viewMode);
    RenderGraphRequest& addViewOverride(RenderViewOverride viewOverride);

    RenderIntent resolvedIntent() const;
    RenderPlan compile(const RenderTargetSpec& target) const;

  private:
    RenderIntent m_baseIntent;
    RenderSceneAnalysis m_sceneAnalysis{RenderSceneAnalysis::unknownScene()};
    std::optional<RenderExecutorPreference> m_executorOverride;
    std::optional<RenderExecutorPreference> m_executorShortcut;
    bool m_wireframeExecutorSelectsWireframeView{true};
    std::optional<RenderViewMode> m_viewModeOverride;
    std::optional<RenderCameraRef> m_cameraOverride;
    std::optional<ShadingProfileRef> m_shadingProfileOverride;
    ShadingProfileParameters m_shadingProfileParameterOverrides;
    std::optional<RenderPostProcessAA> m_postProcessAAOverride;
    std::optional<bool> m_wireframeOverlayOverride;
    std::optional<bool> m_curveOverlayOverride;
    std::optional<bool> m_previewShadowsOverride;
    std::optional<engine::raster::RasterBackend> m_rasterBackendOverride;
    std::vector<RenderViewMode> m_exportedAOVs;
    std::vector<RenderViewOverride> m_viewOverrides;
  };
}
