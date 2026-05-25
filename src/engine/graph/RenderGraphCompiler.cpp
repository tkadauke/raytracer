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

namespace engine::graph {
  namespace {
    RenderTargetSpec normalizedTarget(RenderTargetSpec target) {
      target.sampleCount = std::max(1, target.sampleCount);
      return target;
    }

    RenderResourceDescriptor colorResource(const std::string& id, const std::string& name,
                                           const RenderTargetSpec& target,
                                           RenderResourceLifetime lifetime) {
      RenderResourceDescriptor color;
      color.id = id;
      color.name = name;
      color.type = RenderResourceType::Color;
      color.format = RenderResourceFormat::RGBDouble;
      color.width = target.width;
      color.height = target.height;
      color.sampleCount = target.sampleCount;
      color.domain = RenderResourceDomain::CPU;
      color.lifetime = lifetime;
      return color;
    }

    RenderResourceDescriptor previewShadowResource() {
      RenderResourceDescriptor shadow;
      shadow.id = "preview_shadow_map";
      shadow.name = "Raster preview shadow map";
      shadow.type = RenderResourceType::ShadowMap;
      shadow.format = RenderResourceFormat::DepthDouble;
      shadow.width = 256;
      shadow.height = 256;
      shadow.sampleCount = 1;
      shadow.domain = RenderResourceDomain::CPU;
      shadow.lifetime = RenderResourceLifetime::PersistentCache;
      return shadow;
    }

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
                                   bool mainPass) {
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
      pass.sceneView.selector = SceneSelector::all();
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
      pass.reads.push_back({inputResource});
      pass.writes.push_back({outputResource});
      pass.sceneView.selector = SceneSelector::all();
      pass.disabledBehavior = DisabledBehavior::SubstituteDefault;
      pass.canRunConcurrently = false;
      return pass;
    }

    RenderPlan aovViewPlan(const RenderTargetSpec& target, RenderExecutorKind executor,
                           const RenderAOVDefinition& aov) {
      RenderPlan plan;

      const RenderResourceId aovId = aov.resourceId();
      RenderPassNode producer = aovProducerPass(aov, executor, true);
      applyTargetSamplingToRasterPass(producer, target);
      plan.addResourceProducer(std::move(producer),
                               aov.resourceDescriptor(target, RenderResourceLifetime::Transient));

      RenderResourceDescriptor mainColor =
        colorResource("main_color", "Main color", target, RenderResourceLifetime::Exported);
      plan.routeResourceThroughPass(aovId, mainColor,
                                    aovVisualizationPass(aov, aovId, mainColor.id, true));
      return plan;
    }

    void addAuxiliaryAOVExport(RenderPlan& plan, const RenderTargetSpec& target,
                               RenderExecutorKind executor, RenderViewMode viewMode,
                               RenderViewMode defaultViewMode) {
      const auto* aov = renderAOVDefinition(viewMode);
      if (!aov) {
        throw std::runtime_error("view mode '" + std::string(toString(viewMode)) +
                                 "' is not an AOV export");
      }

      if (viewMode == defaultViewMode || plan.findResource(aov->resourceId()) ||
          plan.findResource(aov->previewColorResourceId())) {
        return;
      }

      const RenderResourceId aovId = aov->resourceId();
      const RenderResourceId previewId = aov->previewColorResourceId();
      RenderPassNode producer = aovProducerPass(*aov, executor, false);
      applyTargetSamplingToRasterPass(producer, target);
      plan.addResourceProducer(std::move(producer),
                               aov->resourceDescriptor(target, RenderResourceLifetime::Exported));
      plan.routeResourceThroughPass(aovId,
                                    colorResource(previewId, aov->title() + " AOV preview", target,
                                                  RenderResourceLifetime::Exported),
                                    aovVisualizationPass(*aov, aovId, previewId, false));
    }

    void addAuxiliaryAOVExports(RenderPlan& plan, const RenderTargetSpec& target,
                                RenderExecutorKind executor, const RenderIntent& intent) {
      std::set<RenderViewMode> seen;
      for (RenderViewMode viewMode : intent.exportedAOVs) {
        if (!seen.insert(viewMode).second) {
          continue;
        }
        addAuxiliaryAOVExport(plan, target, executor, viewMode, intent.defaultViewMode);
      }
    }
  }

  RenderPlan RenderGraphCompiler::compile(const RenderTargetSpec& rawTarget,
                                          const RenderIntent& intent) const {
    const RenderTargetSpec target = normalizedTarget(rawTarget);
    const RenderExecutorKind executor = intent.defaultExecutorKind();
    const auto* executorDefinition = renderExecutorDefinition(executor);
    if (!executorDefinition) {
      throw std::runtime_error("default render executor cannot produce a beauty pass");
    }

    if (const auto* aov = renderAOVDefinition(intent.defaultViewMode)) {
      RenderPlan plan = aovViewPlan(target, executor, *aov);
      addAuxiliaryAOVExports(plan, target, executor, intent);
      return plan;
    }

    RenderPlan plan;

    RenderResourceDescriptor beautyColor =
      colorResource("beauty_color", "Beauty color", target, RenderResourceLifetime::Transient);
    const bool usesPreviewShadows =
      executor == RenderExecutorKind::Rasterizer && intent.enablePreviewShadows;

    RenderPassNode beauty;
    beauty.id = executorDefinition->beautyPassId();
    beauty.name = executorDefinition->beautyPassName();
    beauty.kind = RenderPassKind::Beauty;
    beauty.executor = executor;
    beauty.features = {"main", "beauty", executorDefinition->feature()};
    beauty.sceneView.selector = SceneSelector::all();
    beauty.disabledBehavior = DisabledBehavior::Error;
    beauty.canRunConcurrently = false;
    applyTargetSamplingToRasterPass(beauty, target);
    plan.addResourceProducer(beauty, beautyColor);

    RenderPassNode tonemap;
    tonemap.id = "tonemap";
    tonemap.name = "Tone map";
    tonemap.kind = RenderPassKind::Tonemap;
    tonemap.executor = RenderExecutorKind::PostProcess;
    tonemap.features = {"main", "tonemap", "postprocess"};
    tonemap.reads.push_back({"beauty_color"});
    tonemap.writes.push_back({"main_color"});
    tonemap.sceneView.selector = SceneSelector::all();
    tonemap.disabledBehavior = DisabledBehavior::Passthrough;
    tonemap.canRunConcurrently = false;
    RenderResourceDescriptor mainColor =
      colorResource("main_color", "Main color", target, RenderResourceLifetime::Exported);
    plan.routeResourceThroughPass("beauty_color", mainColor, tonemap);

    if (usesPreviewShadows) {
      RenderPassNode shadows;
      shadows.id = "raster_preview_shadows";
      shadows.name = "Raster preview shadows";
      shadows.kind = RenderPassKind::Shadow;
      shadows.executor = RenderExecutorKind::Rasterizer;
      shadows.features = {"main", "preview_shadows", "shadow_maps", "rasterizer"};
      shadows.sceneView.selector = SceneSelector::all();
      RasterShadowPassState::previewDefaults().writeTo(shadows);
      shadows.disabledBehavior = DisabledBehavior::SubstituteDefault;
      shadows.canRunConcurrently = false;
      plan.connectProducerToConsumer(shadows, previewShadowResource(), beauty.id);
    }

    if (intent.usesGraphImagePostProcessAA()) {
      const auto* postAADefinition = postProcessAADefinition(intent.postProcessAA);
      if (!postAADefinition) {
        throw std::runtime_error(
          "requested post-process AA mode cannot be compiled as a graph pass");
      }
      const RenderResourceId inputResource = tonemapInputResource(plan);
      RenderResourceDescriptor postAAColor = colorResource(
        "post_aa_color", "Postprocess AA color", target, RenderResourceLifetime::Transient);
      RenderPassNode postAA;
      postAA.id = postAADefinition->passId();
      postAA.name = postAADefinition->passName();
      postAA.kind = RenderPassKind::PostProcess;
      postAA.executor = RenderExecutorKind::PostProcess;
      postAA.features = {"main", "postprocess", "post_aa", executorDefinition->feature(),
                         postAADefinition->feature()};
      postAA.reads.push_back({inputResource});
      postAA.writes.push_back({postAAColor.id});
      postAA.sceneView.selector = SceneSelector::all();
      postAA.state = postAADefinition->createState();
      postAA.disabledBehavior = DisabledBehavior::Passthrough;
      postAA.canRunConcurrently = false;
      plan.routeResourceThroughPass(inputResource, postAAColor, postAA);
    }

    if (intent.enableWireframeOverlay) {
      const RenderResourceId inputResource = tonemapInputResource(plan);
      RenderResourceDescriptor overlayColor =
        colorResource("overlay_color", "Overlay color", target, RenderResourceLifetime::Transient);
      RenderPassNode overlay;
      overlay.id = "wireframe_overlay";
      overlay.name = "Wireframe overlay";
      overlay.kind = RenderPassKind::Overlay;
      overlay.executor = RenderExecutorKind::Wireframe;
      overlay.features = {"main", "overlay", "wireframe"};
      overlay.reads.push_back({inputResource});
      overlay.writes.push_back({overlayColor.id});
      overlay.sceneView.selector = SceneSelector::all();
      overlay.disabledBehavior = DisabledBehavior::Passthrough;
      overlay.canRunConcurrently = false;
      plan.routeResourceThroughPass(inputResource, overlayColor, overlay);
    }

    addAuxiliaryAOVExports(plan, target, executor, intent);

    return plan;
  }
}
