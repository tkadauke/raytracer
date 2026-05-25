#include "engine/graph/RenderGraphCompiler.h"
#include "engine/graph/RenderAOV.h"
#include "engine/graph/RenderExecutor.h"
#include "engine/graph/PostProcessPassState.h"
#include "engine/graph/RasterPassState.h"

#include <algorithm>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace engine::graph {
  namespace {
    RenderResourceId tonemapInputResource(const RenderPlan& plan) {
      const auto* tonemap = plan.findPass("tonemap");
      if (!tonemap) {
        throw std::runtime_error("compiled render graph is missing tonemap pass");
      }
      return tonemap->singleRead().resource;
    }

    void applyTargetSamplingToRasterPass(RenderPassNode& pass, const RenderTargetSpec& target) {
      if (pass.executor != RenderExecutorKind::Rasterizer || target.sampleCount == 1) {
        return;
      }

      RasterBeautyPassState state;
      state.sampling().setMSAASamples(target.sampleCount);
      state.writeTo(pass);
    }

    RenderPassNode aovProducerPass(const RenderAOVDefinition& aov, RenderExecutorKind executor,
                                   const SceneView& sceneView, bool mainPass) {
      const auto* executorDefinition = renderExecutorDefinition(executor);
      if (!executorDefinition) {
        throw std::runtime_error("executor cannot produce AOV pass");
      }
      RenderPassNode pass;
      pass.id = aov.resourceId();
      pass.name = aov.title() + " AOV";
      pass.kind = RenderPassKind::AOV;
      pass.executor = executor;
      pass.features = mainPass ? std::vector<RenderFeatureKind>{"main", "aov", aov.feature(),
                                                                executorDefinition->feature()}
                               : std::vector<RenderFeatureKind>{"aov", "export", aov.feature(),
                                                                executorDefinition->feature()};
      pass.sceneView = sceneView;
      pass.disabledBehavior = DisabledBehavior::SubstituteDefault;
      pass.canRunConcurrently = false;
      return pass;
    }

    RenderPassNode aovVisualizationPass(const RenderAOVDefinition& aov,
                                        const RenderResourceId& inputResource,
                                        const RenderResourceId& outputResource, bool mainPass) {
      RenderPassNode pass;
      pass.id = "visualize_" + aov.resourceId();
      pass.name = "Visualize " + aov.title() + " AOV";
      pass.kind = RenderPassKind::AOV;
      pass.executor = RenderExecutorKind::PostProcess;
      pass.features = mainPass ? std::vector<RenderFeatureKind>{"main", "aov", aov.feature(),
                                                                "visualization", "postprocess"}
                               : std::vector<RenderFeatureKind>{"aov", "export", aov.feature(),
                                                                "visualization", "postprocess"};
      pass.addRead(inputResource);
      pass.addWrite(outputResource);
      pass.sceneView.selector = SceneSelector::all();
      pass.disabledBehavior = DisabledBehavior::SubstituteDefault;
      pass.canRunConcurrently = false;
      return pass;
    }

    RenderPlan aovViewPlan(const RenderTargetSpec& target, RenderExecutorKind executor,
                           const RenderAOVDefinition& aov, const SceneView& sceneView) {
      RenderPlan plan;

      const RenderResourceId aovId = aov.resourceId();
      RenderPassNode producer = aovProducerPass(aov, executor, sceneView, true);
      if (aov.usesRasterTargetSampling()) {
        applyTargetSamplingToRasterPass(producer, target);
      }
      plan.addResourceProducer(std::move(producer),
                               aov.resourceDescriptor(target, RenderResourceLifetime::Transient));

      RenderResourceDescriptor mainColor =
        target.colorResource("main_color", "Main color", RenderResourceLifetime::Exported);
      plan.routeResourceThroughPass(aovId, mainColor,
                                    aovVisualizationPass(aov, aovId, mainColor.id, true));
      return plan;
    }

    void addAuxiliaryAOVExport(RenderPlan& plan, const RenderTargetSpec& target,
                               RenderExecutorKind executor, RenderViewMode viewMode,
                               RenderViewMode defaultViewMode, const SceneView& sceneView) {
      const auto* aov = renderAOVDefinition(viewMode);
      if (!aov) {
        throw std::runtime_error("view mode '" + std::string(toString(viewMode)) +
                                 "' is not an AOV export");
      }

      if (viewMode == defaultViewMode || plan.findResource(aov->previewColorResourceId())) {
        return;
      }

      const RenderResourceId aovId = aov->resourceId();
      const RenderResourceId previewId = aov->previewColorResourceId();
      if (!plan.findResource(aovId)) {
        RenderPassNode producer = aovProducerPass(*aov, executor, sceneView, false);
        if (aov->usesRasterTargetSampling()) {
          applyTargetSamplingToRasterPass(producer, target);
        }
        plan.addResourceProducer(std::move(producer),
                                 aov->resourceDescriptor(target, RenderResourceLifetime::Exported));
      }
      plan.addResource(target.colorResource(previewId, aov->title() + " AOV preview",
                                            RenderResourceLifetime::Exported));
      plan.addPass(aovVisualizationPass(*aov, aovId, previewId, false));
    }

    void addAuxiliaryAOVExports(RenderPlan& plan, const RenderTargetSpec& target,
                                RenderExecutorKind executor, const RenderIntent& intent) {
      const SceneView sceneView = intent.defaultSceneView();
      std::set<RenderViewMode> seen;
      for (RenderViewMode viewMode : intent.exportedAOVs) {
        if (!seen.insert(viewMode).second) {
          continue;
        }
        addAuxiliaryAOVExport(plan, target, executor, viewMode, intent.defaultViewMode, sceneView);
      }
    }
  }

  RenderTargetSpec RenderTargetSpec::normalized() const {
    RenderTargetSpec result = *this;
    result.sampleCount = std::max(1, result.sampleCount);
    return result;
  }

  RenderResourceDescriptor RenderTargetSpec::colorResource(RenderResourceId id, std::string name,
                                                           RenderResourceLifetime lifetime) const {
    RenderResourceDescriptor color;
    color.id = std::move(id);
    color.name = std::move(name);
    color.type = RenderResourceType::Color;
    color.format = RenderResourceFormat::RGBDouble;
    color.width = width;
    color.height = height;
    color.sampleCount = sampleCount;
    color.domain = RenderResourceDomain::CPU;
    color.lifetime = lifetime;
    return color;
  }

  RenderPassNode
  RenderGraphCompiler::beautyPass(RenderExecutorKind executor, const SceneView& sceneView,
                                  const RenderTargetSpec& target,
                                  std::vector<RenderFeatureKind> extraFeatures) const {
    const auto* executorDefinition = renderExecutorDefinition(executor);
    if (!executorDefinition) {
      throw std::runtime_error("render executor cannot produce a beauty pass");
    }

    RenderPassNode pass;
    pass.id = executorDefinition->beautyPassId();
    pass.name = executorDefinition->beautyPassName();
    pass.kind = RenderPassKind::Beauty;
    pass.executor = executor;
    pass.features = {"main", "beauty", executorDefinition->feature()};
    pass.features.insert(pass.features.end(), extraFeatures.begin(), extraFeatures.end());
    pass.sceneView = sceneView;
    pass.disabledBehavior = DisabledBehavior::Error;
    pass.canRunConcurrently = false;
    applyTargetSamplingToRasterPass(pass, target);
    return pass;
  }

  RenderPassNode RenderGraphCompiler::tonemapPass(RenderResourceId inputResource,
                                                  RenderResourceId outputResource) const {
    RenderPassNode pass;
    pass.id = "tonemap";
    pass.name = "Tone map";
    pass.kind = RenderPassKind::Tonemap;
    pass.executor = RenderExecutorKind::PostProcess;
    pass.features = {"main", "tonemap", "postprocess"};
    pass.addRead(std::move(inputResource));
    pass.addWrite(std::move(outputResource));
    pass.sceneView.selector = SceneSelector::all();
    pass.disabledBehavior = DisabledBehavior::Passthrough;
    pass.canRunConcurrently = false;
    return pass;
  }

  RenderPlan RenderGraphCompiler::compileStencilCompositeView(const RenderTargetSpec& target,
                                                              const RenderIntent& intent) const {
    const SceneView sceneView = intent.defaultSceneView();
    RenderPlan plan;

    plan.addResourceProducer(
      beautyPass(RenderExecutorKind::Rasterizer, sceneView, target, {"stencil_composite_base"}),
      target.colorResource("base_color", "Base color", RenderResourceLifetime::Transient));
    plan.addResourceProducer(beautyPass(RenderExecutorKind::Wireframe, sceneView, target,
                                        {"stencil_composite_foreground"}),
                             target.colorResource("foreground_color", "Foreground color",
                                                  RenderResourceLifetime::Transient));

    const auto* stencilAOV = renderAOVDefinition(RenderViewMode::Stencil);
    if (!stencilAOV) {
      throw std::runtime_error("stencil composite view requires a stencil AOV definition");
    }
    plan.addResourceProducer(
      aovProducerPass(*stencilAOV, RenderExecutorKind::Rasterizer, sceneView, true),
      stencilAOV->resourceDescriptor(target, RenderResourceLifetime::Transient));

    RenderPassNode composite;
    composite.id = "stencil_composite";
    composite.name = "Stencil composite";
    composite.kind = RenderPassKind::Composite;
    composite.executor = RenderExecutorKind::Composite;
    composite.features = {"main", "composite", "stencil_composite"};
    composite.addRead("base_color");
    composite.addRead("foreground_color");
    composite.addRead(stencilAOV->resourceId());
    composite.addWrite("composited_color");
    composite.sceneView.selector = SceneSelector::all();
    composite.disabledBehavior = DisabledBehavior::SubstituteDefault;
    composite.canRunConcurrently = false;
    plan.addResource(target.colorResource("composited_color", "Composited color",
                                          RenderResourceLifetime::Transient));
    plan.addPass(composite);

    plan.routeResourceThroughPass(
      "composited_color",
      target.colorResource("main_color", "Main color", RenderResourceLifetime::Exported),
      tonemapPass("composited_color", "main_color"));

    addAuxiliaryAOVExports(plan, target, RenderExecutorKind::Rasterizer, intent);
    return plan;
  }

  RenderPlan RenderGraphCompiler::compile(const RenderTargetSpec& target,
                                          const RenderIntent& intent) const {
    return compile(target, intent, RenderSceneAnalysis::unknownScene());
  }

  RenderPlan RenderGraphCompiler::compile(const RenderTargetSpec& rawTarget,
                                          const RenderIntent& intent,
                                          const RenderSceneAnalysis& sceneAnalysis) const {
    const RenderTargetSpec target = rawTarget.normalized();
    const RenderIntent frameIntent = intent.withWholeFrameOverridesApplied();
    frameIntent.requireWholeFrameOnly("RenderGraphCompiler");

    if (frameIntent.defaultViewMode == RenderViewMode::StencilComposite) {
      return compileStencilCompositeView(target, frameIntent);
    }

    const RenderExecutorKind executor = frameIntent.defaultExecutorKind();
    const auto* executorDefinition = renderExecutorDefinition(executor);
    if (!executorDefinition) {
      throw std::runtime_error("default render executor cannot produce a beauty pass");
    }

    if (const auto* aov = renderAOVDefinition(frameIntent.defaultViewMode)) {
      RenderPlan plan = aovViewPlan(target, executor, *aov, frameIntent.defaultSceneView());
      addAuxiliaryAOVExports(plan, target, executor, frameIntent);
      return plan;
    }

    RenderPlan plan;

    RenderResourceDescriptor beautyColor =
      target.colorResource("beauty_color", "Beauty color", RenderResourceLifetime::Transient);
    const bool usesPreviewShadows =
      sceneAnalysis.shouldCompileRasterPreviewShadows(executor, frameIntent);

    RenderPassNode beauty = beautyPass(executor, frameIntent.defaultSceneView(), target);
    plan.addResourceProducer(beauty, beautyColor);

    RenderResourceDescriptor mainColor =
      target.colorResource("main_color", "Main color", RenderResourceLifetime::Exported);
    plan.routeResourceThroughPass("beauty_color", mainColor,
                                  tonemapPass("beauty_color", "main_color"));

    if (usesPreviewShadows) {
      RenderPassNode shadows;
      shadows.id = "raster_preview_shadows";
      shadows.name = "Raster preview shadows";
      shadows.kind = RenderPassKind::Shadow;
      shadows.executor = RenderExecutorKind::Rasterizer;
      shadows.features = {"main", "preview_shadows", "shadow_maps", "rasterizer"};
      shadows.sceneView = frameIntent.defaultSceneView();
      const RasterShadowPassState shadowState = RasterShadowPassState::previewDefaults();
      shadowState.writeTo(shadows);
      shadows.disabledBehavior = DisabledBehavior::SubstituteDefault;
      shadows.canRunConcurrently = false;
      plan.connectProducerToConsumer(
        shadows,
        shadowState.shadows().resourceDescriptor("preview_shadow_map", "Raster preview shadow map"),
        beauty.id);
    }

    if (frameIntent.usesGraphImagePostProcessAA()) {
      const auto* postAADefinition = postProcessAADefinition(frameIntent.postProcessAA);
      if (!postAADefinition) {
        throw std::runtime_error(
          "requested post-process AA mode cannot be compiled as a graph pass");
      }
      const RenderResourceId inputResource = tonemapInputResource(plan);
      RenderResourceDescriptor postAAColor = target.colorResource(
        "post_aa_color", "Postprocess AA color", RenderResourceLifetime::Transient);
      RenderPassNode postAA;
      postAA.id = postAADefinition->passId();
      postAA.name = postAADefinition->passName();
      postAA.kind = RenderPassKind::PostProcess;
      postAA.executor = RenderExecutorKind::PostProcess;
      postAA.features = {"main", "postprocess", "post_aa", executorDefinition->feature(),
                         postAADefinition->feature()};
      postAA.addRead(inputResource);
      postAA.addWrite(postAAColor.id);
      postAA.sceneView.selector = SceneSelector::all();
      postAA.state = postAADefinition->createState();
      postAA.disabledBehavior = DisabledBehavior::Passthrough;
      postAA.canRunConcurrently = false;
      plan.routeResourceThroughPass(inputResource, postAAColor, postAA);
    }

    if (frameIntent.enableWireframeOverlay) {
      const RenderResourceId inputResource = tonemapInputResource(plan);
      RenderResourceDescriptor overlayColor =
        target.colorResource("overlay_color", "Overlay color", RenderResourceLifetime::Transient);
      RenderPassNode overlay;
      overlay.id = "wireframe_overlay";
      overlay.name = "Wireframe overlay";
      overlay.kind = RenderPassKind::Overlay;
      overlay.executor = RenderExecutorKind::Wireframe;
      overlay.features = {"main", "overlay", "wireframe", "wireframe_overlay"};
      overlay.addRead(inputResource);
      overlay.addWrite(overlayColor.id);
      overlay.sceneView = frameIntent.defaultSceneView();
      overlay.disabledBehavior = DisabledBehavior::Passthrough;
      overlay.canRunConcurrently = false;
      plan.routeResourceThroughPass(inputResource, overlayColor, overlay);
    }

    if (frameIntent.enableCurveOverlay) {
      const RenderResourceId inputResource = tonemapInputResource(plan);
      RenderResourceDescriptor overlayColor = target.colorResource(
        "curve_overlay_color", "Curve overlay color", RenderResourceLifetime::Transient);
      RenderPassNode overlay;
      overlay.id = "curve_overlay";
      overlay.name = "Curve overlay";
      overlay.kind = RenderPassKind::Overlay;
      overlay.executor = RenderExecutorKind::Wireframe;
      overlay.features = {"main", "overlay", "curve", "curve_overlay", "wireframe"};
      overlay.addRead(inputResource);
      overlay.addWrite(overlayColor.id);
      overlay.sceneView = frameIntent.defaultSceneView();
      overlay.disabledBehavior = DisabledBehavior::Passthrough;
      overlay.canRunConcurrently = false;
      plan.routeResourceThroughPass(inputResource, overlayColor, overlay);
    }

    addAuxiliaryAOVExports(plan, target, executor, frameIntent);

    return plan;
  }
}
