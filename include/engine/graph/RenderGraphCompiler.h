#pragma once

#include "engine/graph/RenderPlan.h"
#include "engine/graph/RenderSceneAnalysis.h"

#include <optional>
#include <set>
#include <string>
#include <vector>

namespace engine::graph {
  class RenderAOVDefinition;
  class RenderExecutorDefinition;

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
    struct SubviewOutputBinding {
      std::string name;
      RenderResourceId colorResource;
      RenderResourceId depthResource;
    };

    RenderPassNode beautyPass(const RenderExecutorDefinition& executorDefinition,
                              const SceneView& sceneView, const RenderTargetSpec& target,
                              const RenderIntent& intent,
                              std::vector<RenderFeatureKind> extraFeatures = {}) const;
    RenderPlan compileWithSubviewDepth(const RenderTargetSpec& target, const RenderIntent& intent,
                                       const RenderSceneAnalysis& sceneAnalysis,
                                       int renderToTextureDepth) const;
    bool beautyPassNeedsExplicitReadback(const RenderPassNode& pass) const;
    bool passNeedsExplicitReadback(const RenderPassNode& pass) const;
    bool rasterVisibilityCullingRequested(const RenderIntent& intent) const;
    RenderResourceDescriptor visibilitySetResource(const RenderTargetSpec& target) const;
    RenderPassNode visibilityCullingPass(const SceneView& sceneView,
                                         const RenderIntent& intent) const;
    void addRasterVisibilityInput(RenderPlan& plan, const RenderTargetSpec& target,
                                  RenderPassNode& pass, const SceneView& sceneView,
                                  const RenderIntent& intent) const;
    RenderResourceDescriptor hybridShadowMaskResource(const RenderTargetSpec& target) const;
    RenderPassNode hybridShadowMaskPass(const SceneView& sceneView,
                                        const RenderIntent& intent) const;
    RenderPassNode hybridShadowCompositePass(RenderResourceId colorResource,
                                             RenderResourceId maskResource,
                                             RenderResourceId outputResource) const;
    RenderResourceId addHybridShadowComposite(RenderPlan& plan, const RenderTargetSpec& target,
                                              RenderResourceId inputResource) const;
    RenderResourceDescriptor readbackResource(const RenderResourceDescriptor& source,
                                              RenderResourceId id, std::string name,
                                              RenderResourceLifetime lifetime) const;
    RenderPassNode readbackPass(RenderPassId id, std::string name, RenderResourceId inputResource,
                                RenderResourceId outputResource,
                                std::vector<RenderFeatureKind> baseFeatures,
                                std::vector<RenderFeatureKind> extraFeatures = {}) const;
    RenderPassNode readbackPass(RenderResourceId inputResource,
                                RenderResourceId outputResource) const;
    RenderPassNode tonemapPass(RenderResourceId inputResource,
                               RenderResourceId outputResource) const;
    RenderResourceId addSelectorOverrideBranches(RenderPlan& plan, const RenderTargetSpec& target,
                                                 RenderResourceId baseInputResource,
                                                 const RenderIntent& frameIntent,
                                                 const RenderSceneAnalysis& sceneAnalysis) const;
    RenderResourceId addSelectorOverrideBranch(RenderPlan& plan, const RenderTargetSpec& target,
                                               RenderResourceId baseInputResource,
                                               const RenderIntent& frameIntent,
                                               const RenderViewOverride& viewOverride,
                                               std::size_t overrideIndex) const;
    RenderIntent selectorOverrideIntent(const RenderIntent& frameIntent,
                                        const RenderViewOverride& viewOverride) const;
    SceneView selectorOverrideSceneView(const RenderIntent& branchIntent,
                                        const RenderViewOverride& viewOverride) const;
    RenderPassNode
    selectorCompositePass(RenderPassId id, std::string name, RenderResourceId baseResource,
                          RenderResourceId foregroundResource, RenderResourceId stencilResource,
                          RenderResourceId outputResource, const SceneView& sceneView,
                          std::vector<RenderFeatureKind> features) const;
    RenderResourceDescriptor selectorColorResource(const RenderTargetSpec& target,
                                                   RenderResourceId id, std::string name) const;
    RenderPlan aovViewPlan(const RenderTargetSpec& target, RenderExecutorKind executor,
                           const RenderAOVDefinition& aov, const SceneView& sceneView,
                           const RenderIntent& intent) const;
    void addAuxiliaryAOVExport(RenderPlan& plan, const RenderTargetSpec& target,
                               RenderExecutorKind executor, RenderViewMode viewMode,
                               RenderViewMode defaultViewMode, const SceneView& sceneView,
                               const RenderIntent& intent) const;
    void addAuxiliaryAOVExports(RenderPlan& plan, const RenderTargetSpec& target,
                                RenderExecutorKind executor, const RenderIntent& intent) const;
    RenderPlan compileStencilCompositeView(const RenderTargetSpec& target,
                                           const RenderIntent& intent) const;
    void addAutomaticFeatureSubviews(RenderIntent& intent,
                                     const RenderSceneAnalysis& sceneAnalysis) const;
    std::vector<SubviewOutputBinding>
    addSubviewBranches(RenderPlan& plan, const RenderTargetSpec& target, const RenderIntent& intent,
                       const RenderSceneAnalysis& sceneAnalysis, int renderToTextureDepth) const;
    void addSubviewReceiverInputs(RenderPlan& plan,
                                  const std::vector<SubviewOutputBinding>& subviewOutputs,
                                  const RenderSceneAnalysis& sceneAnalysis) const;
    void validateSubviewReceivers(const RenderIntent& intent,
                                  const RenderSceneAnalysis& sceneAnalysis) const;
    void addSubviewRecursionLimitDiagnostics(RenderPlan& plan, const RenderIntent& intent) const;
    RenderPassNode subviewRecursionLimitDiagnosticPass(const RenderSubviewIntent& subview,
                                                       std::size_t index, int recursionLimit,
                                                       std::set<std::string>& usedPrefixes) const;
    void addAutomaticFeatureSubviewComposites(RenderPlan& plan, const RenderTargetSpec& target,
                                              const RenderIntent& intent,
                                              const RenderSceneAnalysis& sceneAnalysis,
                                              RenderResourceId& mainInputResource) const;
    bool addAutomaticFeatureSubviewComposite(RenderPlan& plan, const RenderTargetSpec& target,
                                             const RenderSubviewIntent& subview,
                                             const std::string& prefix,
                                             const std::string& displayName,
                                             const RenderSceneAnalysis& sceneAnalysis,
                                             RenderResourceId& mainInputResource) const;
    RenderPassNode subviewCompositePass(const std::string& prefix, const std::string& displayName,
                                        const RenderFeatureKind& subviewFeature,
                                        const RenderFeatureKind& receiverFeature,
                                        const RenderResourceId& baseColor,
                                        const RenderResourceId& subviewColor,
                                        const std::optional<RenderResourceId>& baseDepth,
                                        const std::optional<RenderResourceId>& subviewDepth,
                                        const RenderResourceId& receiverMask,
                                        const RenderResourceId& outputColor) const;
    void addReceiverMaskDependency(RenderPlan& plan, const RenderTargetSpec& target,
                                   const RenderIntent& intent,
                                   const RenderSceneAnalysis::SceneSurfaceMarker& receiver,
                                   const std::string& prefix, const std::string& displayName,
                                   const RenderFeatureKind& receiverFeature,
                                   const RenderPassId& consumerPassId) const;
    RenderResourceDescriptor receiverMaskResource(const RenderTargetSpec& target,
                                                  const std::string& prefix,
                                                  const std::string& displayName,
                                                  const RenderFeatureKind& receiverFeature,
                                                  bool conservative) const;
    RenderPassNode receiverMaskPass(const RenderIntent& intent,
                                    const RenderSceneAnalysis::SceneSurfaceMarker& receiver,
                                    const std::string& prefix, const std::string& displayName,
                                    const RenderFeatureKind& receiverFeature,
                                    bool conservative) const;
    bool receiverMaskRequiresConservativeRasterState(const RenderIntent& intent) const;
    RenderIntent subviewRenderIntent(const RenderIntent& frameIntent,
                                     const RenderSubviewIntent& subview) const;
    RenderPlan prefixedSubviewPlan(const RenderPlan& branch, const std::string& prefix,
                                   const std::string& displayName, const std::string& subviewName,
                                   const RenderFeatureKind& subviewFeature) const;
    std::string subviewPrefix(const RenderSubviewIntent& subview, std::size_t index,
                              std::set<std::string>& usedPrefixes) const;
    RenderFeatureKind subviewFeature(const std::string& prefix) const;
    std::string subviewDisplayName(const RenderSubviewIntent& subview, std::size_t index) const;
    std::string sanitizeSubviewIdentifier(const std::string& name) const;
    RenderResourceId prefixedResourceId(const std::string& prefix,
                                        const RenderResourceId& id) const;
    RenderPassId prefixedPassId(const std::string& prefix, const RenderPassId& id) const;
    void addFeature(RenderPassNode& pass, RenderFeatureKind feature) const;
  };
}
