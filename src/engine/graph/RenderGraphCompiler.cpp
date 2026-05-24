#include "engine/graph/RenderGraphCompiler.h"

#include <algorithm>
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

    RenderResourceDescriptor colorResource(const std::string& id,
                                           const std::string& name,
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
  }

  RenderPlan RenderGraphCompiler::compile(const RenderTargetSpec& rawTarget,
                                          const RenderIntent& intent) const {
    const RenderTargetSpec target = normalizedTarget(rawTarget);
    const RenderExecutorKind executor = intent.defaultExecutorKind();

    RenderPlan plan;

    RenderResourceDescriptor beautyColor =
      colorResource("beauty_color", "Beauty color", target, RenderResourceLifetime::Transient);
    plan.addResource(beautyColor);

    std::string tonemapInputResource = "beauty_color";
    if (intent.enableWireframeOverlay) {
      RenderResourceDescriptor overlayColor =
        colorResource("overlay_color", "Overlay color", target, RenderResourceLifetime::Transient);
      plan.addResource(overlayColor);
      tonemapInputResource = "overlay_color";
    }

    RenderResourceDescriptor mainColor =
      colorResource("main_color", "Main color", target, RenderResourceLifetime::Exported);
    plan.addResource(mainColor);

    RenderPassNode beauty;
    beauty.id = beautyPassId(executor);
    beauty.name = beautyPassName(executor);
    beauty.kind = RenderPassKind::Beauty;
    beauty.executor = executor;
    beauty.features = {"main", "beauty", executorFeature(executor)};
    beauty.writes.push_back({"beauty_color"});
    beauty.sceneView.selector = SceneSelector::all();
    beauty.disabledBehavior = DisabledBehavior::Error;
    beauty.canRunConcurrently = false;
    plan.addPass(beauty);

    if (intent.enableWireframeOverlay) {
      RenderPassNode overlay;
      overlay.id = "wireframe_overlay";
      overlay.name = "Wireframe overlay";
      overlay.kind = RenderPassKind::Overlay;
      overlay.executor = RenderExecutorKind::Wireframe;
      overlay.features = {"main", "overlay", "wireframe"};
      overlay.reads.push_back({"beauty_color"});
      overlay.writes.push_back({"overlay_color"});
      overlay.sceneView.selector = SceneSelector::all();
      overlay.disabledBehavior = DisabledBehavior::Passthrough;
      overlay.canRunConcurrently = false;
      plan.addPass(overlay);
    }

    RenderPassNode tonemap;
    tonemap.id = "tonemap";
    tonemap.name = "Tone map";
    tonemap.kind = RenderPassKind::Tonemap;
    tonemap.executor = RenderExecutorKind::PostProcess;
    tonemap.features = {"main", "tonemap", "postprocess"};
    tonemap.reads.push_back({tonemapInputResource});
    tonemap.writes.push_back({"main_color"});
    tonemap.sceneView.selector = SceneSelector::all();
    tonemap.disabledBehavior = DisabledBehavior::Passthrough;
    tonemap.canRunConcurrently = false;
    plan.addPass(tonemap);

    return plan;
  }
}
