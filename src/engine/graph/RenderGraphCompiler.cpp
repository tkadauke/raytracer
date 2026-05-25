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

    RenderResourceDescriptor depthResource(const std::string& id, const std::string& name,
                                           const RenderTargetSpec& target,
                                           RenderResourceLifetime lifetime) {
      RenderResourceDescriptor depth;
      depth.id = id;
      depth.name = name;
      depth.type = RenderResourceType::Depth;
      depth.format = RenderResourceFormat::DepthDouble;
      depth.width = target.width;
      depth.height = target.height;
      depth.sampleCount = 1;
      depth.domain = RenderResourceDomain::CPU;
      depth.lifetime = lifetime;
      return depth;
    }

    RenderResourceDescriptor normalResource(const std::string& id, const std::string& name,
                                            const RenderTargetSpec& target,
                                            RenderResourceLifetime lifetime) {
      RenderResourceDescriptor normal;
      normal.id = id;
      normal.name = name;
      normal.type = RenderResourceType::Normal;
      normal.format = RenderResourceFormat::RGBDouble;
      normal.width = target.width;
      normal.height = target.height;
      normal.sampleCount = 1;
      normal.domain = RenderResourceDomain::CPU;
      normal.lifetime = lifetime;
      return normal;
    }

    RenderResourceDescriptor objectIdResource(const std::string& id, const std::string& name,
                                              const RenderTargetSpec& target,
                                              RenderResourceLifetime lifetime) {
      RenderResourceDescriptor objectId;
      objectId.id = id;
      objectId.name = name;
      objectId.type = RenderResourceType::ObjectId;
      objectId.format = RenderResourceFormat::UInt32;
      objectId.width = target.width;
      objectId.height = target.height;
      objectId.sampleCount = 1;
      objectId.domain = RenderResourceDomain::CPU;
      objectId.lifetime = lifetime;
      return objectId;
    }

    RenderResourceDescriptor materialIdResource(const std::string& id, const std::string& name,
                                                const RenderTargetSpec& target,
                                                RenderResourceLifetime lifetime) {
      RenderResourceDescriptor materialId;
      materialId.id = id;
      materialId.name = name;
      materialId.type = RenderResourceType::MaterialId;
      materialId.format = RenderResourceFormat::UInt32;
      materialId.width = target.width;
      materialId.height = target.height;
      materialId.sampleCount = 1;
      materialId.domain = RenderResourceDomain::CPU;
      materialId.lifetime = lifetime;
      return materialId;
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

    RenderPlan depthAOVPlan(const RenderTargetSpec& target, RenderExecutorKind executor) {
      RenderPlan plan;

      RenderPassNode depth;
      depth.id = "depth_aov";
      depth.name = "Depth AOV";
      depth.kind = RenderPassKind::AOV;
      depth.executor = executor;
      depth.features = {"main", "aov", "depth", executorFeature(executor)};
      depth.sceneView.selector = SceneSelector::all();
      depth.disabledBehavior = DisabledBehavior::SubstituteDefault;
      depth.canRunConcurrently = false;
      plan.addResourceProducer(
        depth, depthResource("depth_aov", "Depth AOV", target, RenderResourceLifetime::Transient));

      RenderPassNode visualize;
      visualize.id = "visualize_depth_aov";
      visualize.name = "Visualize depth AOV";
      visualize.kind = RenderPassKind::AOV;
      visualize.executor = RenderExecutorKind::PostProcess;
      visualize.features = {"main", "aov", "depth", "visualization", "postprocess"};
      visualize.reads.push_back({"depth_aov"});
      visualize.writes.push_back({"main_color"});
      visualize.sceneView.selector = SceneSelector::all();
      visualize.disabledBehavior = DisabledBehavior::SubstituteDefault;
      visualize.canRunConcurrently = false;
      RenderResourceDescriptor mainColor =
        colorResource("main_color", "Main color", target, RenderResourceLifetime::Exported);
      plan.routeResourceThroughPass("depth_aov", mainColor, visualize);

      return plan;
    }

    RenderPlan normalAOVPlan(const RenderTargetSpec& target, RenderExecutorKind executor) {
      RenderPlan plan;

      RenderPassNode normal;
      normal.id = "normal_aov";
      normal.name = "Normal AOV";
      normal.kind = RenderPassKind::AOV;
      normal.executor = executor;
      normal.features = {"main", "aov", "normal", executorFeature(executor)};
      normal.sceneView.selector = SceneSelector::all();
      normal.disabledBehavior = DisabledBehavior::SubstituteDefault;
      normal.canRunConcurrently = false;
      plan.addResourceProducer(normal, normalResource("normal_aov", "Normal AOV", target,
                                                      RenderResourceLifetime::Transient));

      RenderPassNode visualize;
      visualize.id = "visualize_normal_aov";
      visualize.name = "Visualize normal AOV";
      visualize.kind = RenderPassKind::AOV;
      visualize.executor = RenderExecutorKind::PostProcess;
      visualize.features = {"main", "aov", "normal", "visualization", "postprocess"};
      visualize.reads.push_back({"normal_aov"});
      visualize.writes.push_back({"main_color"});
      visualize.sceneView.selector = SceneSelector::all();
      visualize.disabledBehavior = DisabledBehavior::SubstituteDefault;
      visualize.canRunConcurrently = false;
      RenderResourceDescriptor mainColor =
        colorResource("main_color", "Main color", target, RenderResourceLifetime::Exported);
      plan.routeResourceThroughPass("normal_aov", mainColor, visualize);

      return plan;
    }

    RenderPlan objectIdAOVPlan(const RenderTargetSpec& target, RenderExecutorKind executor) {
      RenderPlan plan;

      RenderPassNode objectId;
      objectId.id = "object_id_aov";
      objectId.name = "Object ID AOV";
      objectId.kind = RenderPassKind::AOV;
      objectId.executor = executor;
      objectId.features = {"main", "aov", "object_id", executorFeature(executor)};
      objectId.sceneView.selector = SceneSelector::all();
      objectId.disabledBehavior = DisabledBehavior::SubstituteDefault;
      objectId.canRunConcurrently = false;
      plan.addResourceProducer(objectId, objectIdResource("object_id_aov", "Object ID AOV", target,
                                                          RenderResourceLifetime::Transient));

      RenderPassNode visualize;
      visualize.id = "visualize_object_id_aov";
      visualize.name = "Visualize object ID AOV";
      visualize.kind = RenderPassKind::AOV;
      visualize.executor = RenderExecutorKind::PostProcess;
      visualize.features = {"main", "aov", "object_id", "visualization", "postprocess"};
      visualize.reads.push_back({"object_id_aov"});
      visualize.writes.push_back({"main_color"});
      visualize.sceneView.selector = SceneSelector::all();
      visualize.disabledBehavior = DisabledBehavior::SubstituteDefault;
      visualize.canRunConcurrently = false;
      RenderResourceDescriptor mainColor =
        colorResource("main_color", "Main color", target, RenderResourceLifetime::Exported);
      plan.routeResourceThroughPass("object_id_aov", mainColor, visualize);

      return plan;
    }

    RenderPlan materialIdAOVPlan(const RenderTargetSpec& target, RenderExecutorKind executor) {
      RenderPlan plan;

      RenderPassNode materialId;
      materialId.id = "material_id_aov";
      materialId.name = "Material ID AOV";
      materialId.kind = RenderPassKind::AOV;
      materialId.executor = executor;
      materialId.features = {"main", "aov", "material_id", executorFeature(executor)};
      materialId.sceneView.selector = SceneSelector::all();
      materialId.disabledBehavior = DisabledBehavior::SubstituteDefault;
      materialId.canRunConcurrently = false;
      plan.addResourceProducer(materialId,
                               materialIdResource("material_id_aov", "Material ID AOV", target,
                                                  RenderResourceLifetime::Transient));

      RenderPassNode visualize;
      visualize.id = "visualize_material_id_aov";
      visualize.name = "Visualize material ID AOV";
      visualize.kind = RenderPassKind::AOV;
      visualize.executor = RenderExecutorKind::PostProcess;
      visualize.features = {"main", "aov", "material_id", "visualization", "postprocess"};
      visualize.reads.push_back({"material_id_aov"});
      visualize.writes.push_back({"main_color"});
      visualize.sceneView.selector = SceneSelector::all();
      visualize.disabledBehavior = DisabledBehavior::SubstituteDefault;
      visualize.canRunConcurrently = false;
      RenderResourceDescriptor mainColor =
        colorResource("main_color", "Main color", target, RenderResourceLifetime::Exported);
      plan.routeResourceThroughPass("material_id_aov", mainColor, visualize);

      return plan;
    }
  }

  RenderPlan RenderGraphCompiler::compile(const RenderTargetSpec& rawTarget,
                                          const RenderIntent& intent) const {
    const RenderTargetSpec target = normalizedTarget(rawTarget);
    const RenderExecutorKind executor = intent.defaultExecutorKind();

    if (intent.defaultViewMode == RenderViewMode::Depth) {
      return depthAOVPlan(target, executor);
    }
    if (intent.defaultViewMode == RenderViewMode::Normal) {
      return normalAOVPlan(target, executor);
    }
    if (intent.defaultViewMode == RenderViewMode::ObjectId) {
      return objectIdAOVPlan(target, executor);
    }
    if (intent.defaultViewMode == RenderViewMode::MaterialId) {
      return materialIdAOVPlan(target, executor);
    }

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
