#include "engine/graph/RenderGraphCompiler.h"
#include "engine/graph/PostProcessPassState.h"
#include "engine/graph/RasterPassState.h"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>

namespace engine::graph {
  namespace {
    std::string beautyPassId(RenderExecutorKind executor) {
      switch (executor) {
      case RenderExecutorKind::Raytracer:
        return "raytrace_beauty";
      case RenderExecutorKind::Rasterizer:
        return "raster_beauty";
      case RenderExecutorKind::Wireframe:
        return "wireframe_beauty";
      case RenderExecutorKind::Composite:
      case RenderExecutorKind::PostProcess:
        break;
      }
      return "beauty";
    }

    std::string beautyPassName(RenderExecutorKind executor) {
      switch (executor) {
      case RenderExecutorKind::Raytracer:
        return "Raytraced beauty";
      case RenderExecutorKind::Rasterizer:
        return "Raster beauty";
      case RenderExecutorKind::Wireframe:
        return "Wireframe beauty";
      case RenderExecutorKind::Composite:
      case RenderExecutorKind::PostProcess:
        break;
      }
      return "Beauty";
    }

    RenderFeatureKind executorFeature(RenderExecutorKind executor) {
      switch (executor) {
      case RenderExecutorKind::Raytracer:
        return "raytracer";
      case RenderExecutorKind::Rasterizer:
        return "rasterizer";
      case RenderExecutorKind::Wireframe:
        return "wireframe";
      case RenderExecutorKind::Composite:
        return "composite";
      case RenderExecutorKind::PostProcess:
        return "postprocess";
      }
      return "unknown";
    }

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
      shadow.name = "Raster preview shadow map request";
      shadow.type = RenderResourceType::ShadowMap;
      shadow.format = RenderResourceFormat::Unknown;
      shadow.domain = RenderResourceDomain::CPU;
      shadow.lifetime = RenderResourceLifetime::Transient;
      return shadow;
    }

    std::string postProcessAAPassId(RenderPostProcessAA aa) {
      switch (aa) {
      case RenderPostProcessAA::FXAA:
        return "post_fxaa";
      case RenderPostProcessAA::SMAA:
        return "post_smaa";
      case RenderPostProcessAA::None:
      case RenderPostProcessAA::TAA:
        break;
      }
      return "post_aa";
    }

    std::string postProcessAAPassName(RenderPostProcessAA aa) {
      switch (aa) {
      case RenderPostProcessAA::FXAA:
        return "FXAA";
      case RenderPostProcessAA::SMAA:
        return "SMAA";
      case RenderPostProcessAA::None:
      case RenderPostProcessAA::TAA:
        break;
      }
      return "Post-process AA";
    }

    std::shared_ptr<const RenderPassState> postProcessAAState(RenderPostProcessAA aa) {
      switch (aa) {
      case RenderPostProcessAA::FXAA:
        return std::make_shared<FxaaPostProcessAAState>();
      case RenderPostProcessAA::SMAA:
        return std::make_shared<SmaaPostProcessAAState>();
      case RenderPostProcessAA::None:
      case RenderPostProcessAA::TAA:
        break;
      }
      return nullptr;
    }

    RenderResourceId tonemapInputResource(const RenderPlan& plan) {
      const auto* tonemap = plan.findPass("tonemap");
      if (!tonemap) {
        throw std::runtime_error("compiled render graph is missing tonemap pass");
      }
      return tonemap->singleRead().resource;
    }
  }

  RenderPlan RenderGraphCompiler::compile(const RenderTargetSpec& rawTarget,
                                          const RenderIntent& intent) const {
    const RenderTargetSpec target = normalizedTarget(rawTarget);
    const RenderExecutorKind executor = intent.defaultExecutorKind();

    RenderPlan plan;

    RenderResourceDescriptor beautyColor =
      colorResource("beauty_color", "Beauty color", target, RenderResourceLifetime::Transient);
    const bool usesPreviewShadows =
      executor == RenderExecutorKind::Rasterizer && intent.enablePreviewShadows;

    RenderPassNode beauty;
    beauty.id = beautyPassId(executor);
    beauty.name = beautyPassName(executor);
    beauty.kind = RenderPassKind::Beauty;
    beauty.executor = executor;
    beauty.features = {"main", "beauty", executorFeature(executor)};
    beauty.sceneView.selector = SceneSelector::all();
    beauty.disabledBehavior = DisabledBehavior::Error;
    beauty.canRunConcurrently = false;
    if (executor == RenderExecutorKind::Rasterizer && target.sampleCount != 1) {
      RasterBeautyPassState state;
      state.sampling().setMSAASamples(target.sampleCount);
      state.writeTo(beauty);
    }
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
      const RenderResourceId inputResource = tonemapInputResource(plan);
      RenderResourceDescriptor postAAColor = colorResource(
        "post_aa_color", "Postprocess AA color", target, RenderResourceLifetime::Transient);
      RenderPassNode postAA;
      postAA.id = postProcessAAPassId(intent.postProcessAA);
      postAA.name = postProcessAAPassName(intent.postProcessAA);
      postAA.kind = RenderPassKind::PostProcess;
      postAA.executor = RenderExecutorKind::PostProcess;
      postAA.features = {"main", "postprocess", "post_aa", executorFeature(executor),
                         toString(intent.postProcessAA)};
      postAA.reads.push_back({inputResource});
      postAA.writes.push_back({postAAColor.id});
      postAA.sceneView.selector = SceneSelector::all();
      postAA.state = postProcessAAState(intent.postProcessAA);
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

    return plan;
  }
}
