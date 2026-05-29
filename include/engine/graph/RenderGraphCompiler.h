#pragma once

#include "engine/graph/RenderPlan.h"
#include "engine/graph/RenderSceneAnalysis.h"

#include <string>
#include <vector>

namespace engine::graph {
  /**
    * Dimensions and sampling shape of the image a render plan targets.
    *
    * Graph compilation is intentionally independent of drawing pixels, but the
    * compiler still needs the render target shape so it can declare resource
    * descriptors with concrete dimensions.
    */
  struct RenderTargetSpec {
    int width{0};
    int height{0};
    int sampleCount{1};

    RenderTargetSpec normalized() const;
    RenderResourceDescriptor colorResource(RenderResourceId id, std::string name,
                                           RenderResourceLifetime lifetime) const;
  };

  /**
    * Builds a declarative render plan from user-facing render intent.
    *
    * This first compiler slice emits a whole-frame beauty pass backed by one
    * existing engine executor, optionally routes that image through wireframe
    * or curve-overlay passes, then emits a tonemap pass that writes the exported
    * color resource. Later slices will expand scene features into shadow,
    * postprocess, composite, AOV, and history-resource passes.
    */
  class RenderGraphCompiler {
  public:
    /**
      * Compiles the graph for @p target and @p intent when scene facts are not
      * available. Unknown scene facts preserve conservative feature expansion.
      *
      * The returned plan can be inspected, exported, disabled through
      * `RenderGraphOverrides`, or handed to `GraphRenderEngine` for execution.
      */
    RenderPlan compile(const RenderTargetSpec& target, const RenderIntent& intent) const;

    /**
      * Compiles the graph for @p target, @p intent, and @p sceneAnalysis.
      *
      * The returned plan can be inspected, exported, disabled through
      * `RenderGraphOverrides`, or handed to `GraphRenderEngine` for execution.
      */
    RenderPlan compile(const RenderTargetSpec& target, const RenderIntent& intent,
                       const RenderSceneAnalysis& sceneAnalysis) const;

  private:
    RenderPassNode beautyPass(RenderExecutorKind executor, const SceneView& sceneView,
                              const RenderTargetSpec& target, const RenderIntent& intent,
                              std::vector<RenderFeatureKind> extraFeatures = {}) const;
    bool beautyPassNeedsExplicitReadback(const RenderPassNode& pass) const;
    RenderPassNode readbackPass(RenderResourceId inputResource,
                                RenderResourceId outputResource) const;
    RenderPassNode tonemapPass(RenderResourceId inputResource,
                               RenderResourceId outputResource) const;
    RenderPlan compileStencilCompositeView(const RenderTargetSpec& target,
                                           const RenderIntent& intent) const;
  };
}
