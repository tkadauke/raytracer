#include "engine/graph/RenderGraphCompiler.h"
#include "engine/graph/RenderAOV.h"
#include "engine/graph/RenderExecutor.h"
#include "engine/graph/PostProcessPassState.h"
#include "engine/graph/RasterPassState.h"
#include "engine/graph/RaytracerPassState.h"
#include "engine/graph/WireframePassState.h"

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

    void applyEngineOptionsToPass(RenderPassNode& pass, int rasterTargetSampleCount,
                                  const RenderIntent& intent) {
      if (pass.executor == RenderExecutorKind::Raytracer && pass.kind == RenderPassKind::Beauty) {
        intent.engineOptions.raytracer().beautyPassState().writeTo(pass);
      } else if (pass.executor == RenderExecutorKind::Rasterizer) {
        intent.engineOptions.rasterizer()
          .beautyPassState(rasterTargetSampleCount, intent.postProcessAA,
                           !intent.usesGraphImagePostProcessAA(), false)
          .writeTo(pass);
      } else if (pass.executor == RenderExecutorKind::Wireframe) {
        intent.engineOptions.wireframe().passState().writeTo(pass);
      }
    }

    RenderPassNode aovProducerPass(const RenderAOVDefinition& aov, RenderExecutorKind executor,
                                   const SceneView& sceneView, bool mainPass,
                                   const RenderTargetSpec& target, const RenderIntent& intent) {
      const auto* executorDefinition = renderExecutorDefinition(executor);
      if (!executorDefinition) {
        throw std::runtime_error("executor cannot produce AOV pass");
      }
      if (!aov.supportsExecutor(executor)) {
        throw std::runtime_error("view mode '" + std::string(toString(aov.viewMode())) +
                                 "' is not supported by executor '" +
                                 std::string(toString(executor)) + "'");
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
      applyEngineOptionsToPass(pass, aov.usesRasterTargetSampling() ? target.sampleCount : 1,
                               intent);
      aov.configureProducerPass(pass);
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
                                  const RenderTargetSpec& target, const RenderIntent& intent,
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
    applyEngineOptionsToPass(pass, target.sampleCount, intent);
    return pass;
  }

  bool RenderGraphCompiler::beautyPassNeedsExplicitReadback(const RenderPassNode& pass) const {
    return pass.kind == RenderPassKind::Beauty && passNeedsExplicitReadback(pass);
  }

  bool RenderGraphCompiler::passNeedsExplicitReadback(const RenderPassNode& pass) const {
    const auto* state = RasterBeautyPassState::fromPass(pass);
    return state && state->execution().backend().isOpenGL();
  }

  RenderResourceDescriptor
  RenderGraphCompiler::readbackResource(const RenderResourceDescriptor& source, RenderResourceId id,
                                        std::string name, RenderResourceLifetime lifetime) const {
    RenderResourceDescriptor result = source;
    result.id = std::move(id);
    result.name = std::move(name);
    result.lifetime = lifetime;
    return result;
  }

  RenderPassNode
  RenderGraphCompiler::readbackPass(RenderPassId id, std::string name,
                                    RenderResourceId inputResource, RenderResourceId outputResource,
                                    std::vector<RenderFeatureKind> baseFeatures,
                                    std::vector<RenderFeatureKind> extraFeatures) const {
    RenderPassNode pass;
    pass.id = std::move(id);
    pass.name = std::move(name);
    pass.kind = RenderPassKind::Readback;
    pass.executor = RenderExecutorKind::PostProcess;
    pass.features = std::move(baseFeatures);
    pass.features.push_back("readback");
    pass.features.push_back("transfer");
    pass.features.insert(pass.features.end(), extraFeatures.begin(), extraFeatures.end());
    pass.addRead(std::move(inputResource));
    pass.addWrite(std::move(outputResource));
    pass.supportedResourceDomains = {RenderResourceDomain::CPU, RenderResourceDomain::GPU};
    pass.sceneView.selector = SceneSelector::all();
    pass.disabledBehavior = DisabledBehavior::Error;
    pass.canRunConcurrently = false;
    return pass;
  }

  RenderPassNode RenderGraphCompiler::readbackPass(RenderResourceId inputResource,
                                                   RenderResourceId outputResource) const {
    return readbackPass("beauty_readback", "Beauty readback", std::move(inputResource),
                        std::move(outputResource), {"main"});
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

  RenderPlan RenderGraphCompiler::aovViewPlan(const RenderTargetSpec& target,
                                              RenderExecutorKind executor,
                                              const RenderAOVDefinition& aov,
                                              const SceneView& sceneView,
                                              const RenderIntent& intent) const {
    RenderPlan plan;

    const RenderResourceId aovId = aov.resourceId();
    RenderPassNode producer = aovProducerPass(aov, executor, sceneView, true, target, intent);
    RenderResourceDescriptor aovResource =
      aov.resourceDescriptor(target, RenderResourceLifetime::Transient);
    plan.addResourceProducer(producer, aovResource);

    RenderResourceId visualizationInput = aovId;
    if (passNeedsExplicitReadback(producer)) {
      const RenderResourceId readbackId = aovId + "_readback";
      RenderResourceDescriptor readbackDescriptor = readbackResource(
        aovResource, readbackId, aov.title() + " AOV readback", RenderResourceLifetime::Transient);
      RenderPassNode readback =
        readbackPass("readback_" + aovId, "Read back " + aov.title() + " AOV", aovId, readbackId,
                     {"main", "aov", aov.feature()});
      plan.addResourceProducer(std::move(readback), std::move(readbackDescriptor));
      visualizationInput = readbackId;
    }

    RenderResourceDescriptor mainColor =
      target.colorResource("main_color", "Main color", RenderResourceLifetime::Exported);
    plan.routeResourceThroughPass(
      visualizationInput, mainColor,
      aovVisualizationPass(aov, visualizationInput, mainColor.id, true));
    return plan;
  }

  void RenderGraphCompiler::addAuxiliaryAOVExport(RenderPlan& plan, const RenderTargetSpec& target,
                                                  RenderExecutorKind executor,
                                                  RenderViewMode viewMode,
                                                  RenderViewMode defaultViewMode,
                                                  const SceneView& sceneView,
                                                  const RenderIntent& intent) const {
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
    RenderResourceId visualizationInput = aovId;
    if (!plan.findResource(aovId)) {
      RenderPassNode producer = aovProducerPass(*aov, executor, sceneView, false, target, intent);
      if (passNeedsExplicitReadback(producer)) {
        const RenderResourceId sourceId = aovId + "_source";
        RenderResourceDescriptor sourceDescriptor = readbackResource(
          aov->resourceDescriptor(target, RenderResourceLifetime::Transient), sourceId,
          aov->title() + " AOV source", RenderResourceLifetime::Transient);
        plan.addResourceProducer(producer, std::move(sourceDescriptor));

        RenderPassNode readback =
          readbackPass("readback_" + aovId, "Read back " + aov->title() + " AOV", sourceId, aovId,
                       {"aov", "export", aov->feature()});
        plan.addResourceProducer(std::move(readback),
                                 aov->resourceDescriptor(target, RenderResourceLifetime::Exported));
      } else {
        plan.addResourceProducer(std::move(producer),
                                 aov->resourceDescriptor(target, RenderResourceLifetime::Exported));
      }
    }
    plan.addResourceProducer(aovVisualizationPass(*aov, visualizationInput, previewId, false),
                             target.colorResource(previewId, aov->title() + " AOV preview",
                                                  RenderResourceLifetime::Exported));
  }

  void RenderGraphCompiler::addAuxiliaryAOVExports(RenderPlan& plan, const RenderTargetSpec& target,
                                                   RenderExecutorKind executor,
                                                   const RenderIntent& intent) const {
    const SceneView sceneView = intent.defaultSceneView();
    std::set<RenderViewMode> seen;
    for (RenderViewMode viewMode : intent.exportedAOVs) {
      if (!seen.insert(viewMode).second) {
        continue;
      }
      addAuxiliaryAOVExport(plan, target, executor, viewMode, intent.defaultViewMode, sceneView,
                            intent);
    }
  }

  RenderPlan RenderGraphCompiler::compileStencilCompositeView(const RenderTargetSpec& target,
                                                              const RenderIntent& intent) const {
    const SceneView sceneView = intent.defaultSceneView();
    RenderPlan plan;

    RenderPassNode base = beautyPass(RenderExecutorKind::Rasterizer, sceneView, target, intent,
                                     {"stencil_composite_base"});
    RenderResourceDescriptor baseColor =
      target.colorResource("base_color", "Base color", RenderResourceLifetime::Transient);
    plan.addResourceProducer(base, baseColor);
    RenderResourceId baseCompositeInput = baseColor.id;
    if (beautyPassNeedsExplicitReadback(base)) {
      RenderResourceDescriptor baseReadback = target.colorResource(
        "base_readback_color", "Base readback color", RenderResourceLifetime::Transient);
      RenderPassNode readback =
        readbackPass("readback_base_color", "Read back base color", baseColor.id, baseReadback.id,
                     {"main", "stencil_composite"});
      plan.addResourceProducer(std::move(readback), baseReadback);
      baseCompositeInput = "base_readback_color";
    }

    plan.addResourceProducer(beautyPass(RenderExecutorKind::Wireframe, sceneView, target, intent,
                                        {"stencil_composite_foreground"}),
                             target.colorResource("foreground_color", "Foreground color",
                                                  RenderResourceLifetime::Transient));

    const auto* stencilAOV = renderAOVDefinition(RenderViewMode::Stencil);
    if (!stencilAOV) {
      throw std::runtime_error("stencil composite view requires a stencil AOV definition");
    }

    RenderPassNode stencilProducer =
      aovProducerPass(*stencilAOV, RenderExecutorKind::Rasterizer, sceneView, true, target, intent);
    RenderResourceDescriptor stencilResource =
      stencilAOV->resourceDescriptor(target, RenderResourceLifetime::Transient);
    RenderResourceId stencilCompositeInput = stencilResource.id;
    if (passNeedsExplicitReadback(stencilProducer)) {
      stencilProducer.id = "stencil_composite_mask";
      stencilProducer.name = "Stencil composite mask";
      stencilProducer.features.push_back("stencil_composite_mask");
      stencilResource.id = "stencil_composite_mask_source";
      stencilResource.name = "Stencil composite mask source";
      plan.addResourceProducer(std::move(stencilProducer), stencilResource);

      RenderResourceDescriptor stencilReadback =
        readbackResource(stencilResource, "stencil_composite_mask", "Stencil composite mask",
                         RenderResourceLifetime::Transient);
      RenderPassNode readback = readbackPass(
        "readback_stencil_composite_mask", "Read back stencil composite mask", stencilResource.id,
        stencilReadback.id, {"main", "stencil_composite"}, {"aov", stencilAOV->feature()});
      plan.addResourceProducer(std::move(readback), std::move(stencilReadback));
      stencilCompositeInput = "stencil_composite_mask";
    } else {
      plan.addResourceProducer(std::move(stencilProducer), std::move(stencilResource));
    }

    RenderPassNode composite;
    composite.id = "stencil_composite";
    composite.name = "Stencil composite";
    composite.kind = RenderPassKind::Composite;
    composite.executor = RenderExecutorKind::Composite;
    composite.features = {"main", "composite", "stencil_composite"};
    composite.addRead(std::move(baseCompositeInput));
    composite.addRead("foreground_color");
    composite.addRead(std::move(stencilCompositeInput));
    composite.addWrite("composited_color");
    composite.sceneView.selector = SceneSelector::all();
    composite.disabledBehavior = DisabledBehavior::SubstituteDefault;
    composite.canRunConcurrently = false;
    plan.addResourceProducer(std::move(composite),
                             target.colorResource("composited_color", "Composited color",
                                                  RenderResourceLifetime::Transient));

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
    frameIntent.requireNoSubviews("RenderGraphCompiler");

    if (frameIntent.defaultViewMode == RenderViewMode::StencilComposite) {
      return compileStencilCompositeView(target, frameIntent);
    }

    const RenderExecutorKind executor = frameIntent.defaultExecutorKind();
    const auto* executorDefinition = renderExecutorDefinition(executor);
    if (!executorDefinition) {
      throw std::runtime_error("default render executor cannot produce a beauty pass");
    }

    if (const auto* aov = renderAOVDefinition(frameIntent.defaultViewMode)) {
      RenderPlan plan =
        this->aovViewPlan(target, executor, *aov, frameIntent.defaultSceneView(), frameIntent);
      addAuxiliaryAOVExports(plan, target, executor, frameIntent);
      return plan;
    }

    RenderPlan plan;

    RenderResourceDescriptor beautyColor =
      target.colorResource("beauty_color", "Beauty color", RenderResourceLifetime::Transient);
    const bool usesPreviewShadows =
      sceneAnalysis.shouldCompileRasterPreviewShadows(executor, frameIntent);

    RenderPassNode beauty =
      beautyPass(executor, frameIntent.defaultSceneView(), target, frameIntent);
    plan.addResourceProducer(beauty, beautyColor);

    RenderResourceId mainInputResource = beautyColor.id;
    if (beautyPassNeedsExplicitReadback(beauty)) {
      RenderResourceDescriptor readbackColor = target.colorResource(
        "beauty_readback_color", "Beauty readback color", RenderResourceLifetime::Transient);
      RenderPassNode readback = readbackPass(beautyColor.id, readbackColor.id);
      plan.addResourceProducer(std::move(readback), readbackColor);
      mainInputResource = "beauty_readback_color";
    }

    RenderResourceDescriptor mainColor =
      target.colorResource("main_color", "Main color", RenderResourceLifetime::Exported);
    plan.routeResourceThroughPass(mainInputResource, mainColor,
                                  tonemapPass(mainInputResource, "main_color"));

    if (usesPreviewShadows) {
      RenderPassNode shadows;
      shadows.id = "raster_preview_shadows";
      shadows.name = "Raster preview shadows";
      shadows.kind = RenderPassKind::Shadow;
      shadows.executor = RenderExecutorKind::Rasterizer;
      shadows.features = {"main", "preview_shadows", "shadow_maps", "rasterizer"};
      shadows.sceneView = frameIntent.defaultSceneView();
      const RasterShadowPassState shadowState =
        frameIntent.engineOptions.rasterizer().shadowPassState();
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
      frameIntent.engineOptions.wireframe().passState().writeTo(overlay);
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
      frameIntent.engineOptions.wireframe().passState().writeTo(overlay);
      overlay.disabledBehavior = DisabledBehavior::Passthrough;
      overlay.canRunConcurrently = false;
      plan.routeResourceThroughPass(inputResource, overlayColor, overlay);
    }

    addAuxiliaryAOVExports(plan, target, executor, frameIntent);

    return plan;
  }
}
