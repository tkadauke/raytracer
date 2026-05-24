#include "engine/graph/RenderGraphCompiler.h"

#include <algorithm>
#include <string>

namespace engine::graph {
  namespace {
    RenderExecutorKind executorFor(const RenderIntent& intent) {
      if (intent.defaultViewMode == RenderViewMode::Wireframe) {
        return RenderExecutorKind::Wireframe;
      }

      switch (intent.defaultExecutor) {
      case RenderExecutorPreference::Raytracer:
        return RenderExecutorKind::Raytracer;
      case RenderExecutorPreference::Rasterizer:
        return RenderExecutorKind::Rasterizer;
      case RenderExecutorPreference::Wireframe:
        return RenderExecutorKind::Wireframe;
      }
      return RenderExecutorKind::Raytracer;
    }

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
  }

  RenderPlan RenderGraphCompiler::compile(const RenderTargetSpec& rawTarget,
                                          const RenderIntent& intent) const {
    const RenderTargetSpec target = normalizedTarget(rawTarget);
    const RenderExecutorKind executor = executorFor(intent);

    RenderPlan plan;

    RenderResourceDescriptor beautyColor;
    beautyColor.id = "beauty_color";
    beautyColor.name = "Beauty color";
    beautyColor.type = RenderResourceType::Color;
    beautyColor.format = RenderResourceFormat::RGBDouble;
    beautyColor.width = target.width;
    beautyColor.height = target.height;
    beautyColor.sampleCount = target.sampleCount;
    beautyColor.domain = RenderResourceDomain::CPU;
    beautyColor.lifetime = RenderResourceLifetime::Transient;
    plan.addResource(beautyColor);

    RenderResourceDescriptor mainColor = beautyColor;
    mainColor.id = "main_color";
    mainColor.name = "Main color";
    mainColor.lifetime = RenderResourceLifetime::Exported;
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
    plan.addPass(tonemap);

    return plan;
  }
}
