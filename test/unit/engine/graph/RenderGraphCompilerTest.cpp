#include <gtest/gtest.h>

#include "engine/graph/PostProcessPassState.h"
#include "engine/graph/RasterPassState.h"
#include "engine/graph/RenderGraphCompiler.h"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace RenderGraphCompilerTest {
  using namespace engine::graph;

  bool hasFeature(const RenderPassNode& pass, const std::string& feature) {
    return std::find(pass.features.begin(), pass.features.end(), feature) != pass.features.end();
  }

  TEST(RenderTargetSpec, BuildsNormalizedColorResourceDescriptors) {
    const RenderTargetSpec raw{320, 180, 0};
    const RenderTargetSpec target = raw.normalized();
    const RenderResourceDescriptor resource =
      target.colorResource("main_color", "Main color", RenderResourceLifetime::Exported);

    EXPECT_EQ(1, target.sampleCount);
    EXPECT_EQ("main_color", resource.id);
    EXPECT_EQ("Main color", resource.name);
    EXPECT_EQ(RenderResourceType::Color, resource.type);
    EXPECT_EQ(RenderResourceFormat::RGBDouble, resource.format);
    EXPECT_EQ(320, resource.width);
    EXPECT_EQ(180, resource.height);
    EXPECT_EQ(1, resource.sampleCount);
    EXPECT_EQ(RenderResourceDomain::CPU, resource.domain);
    EXPECT_EQ(RenderResourceLifetime::Exported, resource.lifetime);
  }

  TEST(RenderGraphCompiler, CompilesDefaultRaytracedBeautyPlan) {
    RenderGraphCompiler compiler;
    RenderIntent intent;

    const RenderPlan plan = compiler.compile({320, 180, 1}, intent);

    ASSERT_EQ(2u, plan.resources().size());
    EXPECT_EQ("beauty_color", plan.resources()[0].id);
    EXPECT_EQ(RenderResourceType::Color, plan.resources()[0].type);
    EXPECT_EQ(RenderResourceLifetime::Transient, plan.resources()[0].lifetime);
    EXPECT_EQ(320, plan.resources()[0].width);
    EXPECT_EQ(180, plan.resources()[0].height);
    EXPECT_EQ("main_color", plan.resources()[1].id);
    EXPECT_EQ(RenderResourceType::Color, plan.resources()[1].type);
    EXPECT_EQ(RenderResourceLifetime::Exported, plan.resources()[1].lifetime);
    EXPECT_EQ(320, plan.resources()[1].width);
    EXPECT_EQ(180, plan.resources()[1].height);

    ASSERT_EQ(2u, plan.passes().size());
    EXPECT_EQ("raytrace_beauty", plan.passes()[0].id);
    EXPECT_EQ(RenderPassKind::Beauty, plan.passes()[0].kind);
    EXPECT_EQ(RenderExecutorKind::Raytracer, plan.passes()[0].executor);
    ASSERT_EQ(1u, plan.passes()[0].writes.size());
    EXPECT_EQ("beauty_color", plan.passes()[0].writes[0].resource);
    EXPECT_EQ("tonemap", plan.passes()[1].id);
    EXPECT_EQ(RenderPassKind::Tonemap, plan.passes()[1].kind);
    EXPECT_EQ(RenderExecutorKind::PostProcess, plan.passes()[1].executor);
    EXPECT_EQ(DisabledBehavior::Passthrough, plan.passes()[1].disabledBehavior);
    ASSERT_EQ(1u, plan.passes()[1].reads.size());
    ASSERT_EQ(1u, plan.passes()[1].writes.size());
    EXPECT_EQ("beauty_color", plan.passes()[1].reads[0].resource);
    EXPECT_EQ("main_color", plan.passes()[1].writes[0].resource);
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, UsesRasterExecutorPreference) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;

    const RenderPlan plan = compiler.compile({64, 64, 1}, intent);

    ASSERT_EQ(2u, plan.passes().size());
    EXPECT_EQ("raster_beauty", plan.passes()[0].id);
    EXPECT_EQ(RenderExecutorKind::Rasterizer, plan.passes()[0].executor);
    EXPECT_EQ("tonemap", plan.passes()[1].id);
  }

  TEST(RenderGraphCompiler, WireframeViewModeSelectsWireframeExecutor) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.defaultViewMode = RenderViewMode::Wireframe;

    const RenderPlan plan = compiler.compile({64, 64, 1}, intent);

    ASSERT_EQ(2u, plan.passes().size());
    EXPECT_EQ("wireframe_beauty", plan.passes()[0].id);
    EXPECT_EQ(RenderExecutorKind::Wireframe, plan.passes()[0].executor);
    EXPECT_EQ("tonemap", plan.passes()[1].id);
  }

  TEST(RenderGraphCompiler, DepthViewModeCompilesDepthAOVPlan) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.defaultViewMode = RenderViewMode::Depth;

    const RenderPlan plan = compiler.compile({64, 32, 4}, intent);

    ASSERT_EQ(2u, plan.resources().size());
    ASSERT_NE(nullptr, plan.findResource("depth_aov"));
    EXPECT_EQ(RenderResourceType::Depth, plan.findResource("depth_aov")->type);
    EXPECT_EQ(RenderResourceFormat::DepthDouble, plan.findResource("depth_aov")->format);
    EXPECT_EQ(RenderResourceLifetime::Transient, plan.findResource("depth_aov")->lifetime);
    EXPECT_EQ(1, plan.findResource("depth_aov")->sampleCount);
    ASSERT_NE(nullptr, plan.findResource("main_color"));
    EXPECT_EQ(RenderResourceType::Color, plan.findResource("main_color")->type);
    EXPECT_EQ(RenderResourceLifetime::Exported, plan.findResource("main_color")->lifetime);

    ASSERT_EQ(2u, plan.passes().size());
    EXPECT_EQ("depth_aov", plan.passes()[0].id);
    EXPECT_EQ(RenderPassKind::AOV, plan.passes()[0].kind);
    EXPECT_EQ(RenderExecutorKind::Rasterizer, plan.passes()[0].executor);
    ASSERT_NE(nullptr, plan.passes()[0].state);
    EXPECT_EQ(4, RasterBeautyPassState::fromPass(plan.passes()[0])->sampling().msaaSamples());
    EXPECT_TRUE(hasFeature(plan.passes()[0], "depth"));
    ASSERT_EQ(1u, plan.passes()[0].writes.size());
    EXPECT_EQ("depth_aov", plan.passes()[0].writes[0].resource);

    EXPECT_EQ("visualize_depth_aov", plan.passes()[1].id);
    EXPECT_EQ(RenderPassKind::AOV, plan.passes()[1].kind);
    EXPECT_EQ(RenderExecutorKind::PostProcess, plan.passes()[1].executor);
    EXPECT_TRUE(hasFeature(plan.passes()[1], "visualization"));
    ASSERT_EQ(1u, plan.passes()[1].reads.size());
    ASSERT_EQ(1u, plan.passes()[1].writes.size());
    EXPECT_EQ("depth_aov", plan.passes()[1].reads[0].resource);
    EXPECT_EQ("main_color", plan.passes()[1].writes[0].resource);
    EXPECT_EQ(nullptr, plan.findPass("tonemap"));
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, StencilViewModeCompilesStencilAOVPlan) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.defaultViewMode = RenderViewMode::Stencil;

    const RenderPlan plan = compiler.compile({64, 32, 4}, intent);

    ASSERT_EQ(2u, plan.resources().size());
    ASSERT_NE(nullptr, plan.findResource("stencil_aov"));
    EXPECT_EQ(RenderResourceType::Stencil, plan.findResource("stencil_aov")->type);
    EXPECT_EQ(RenderResourceFormat::UInt8, plan.findResource("stencil_aov")->format);
    EXPECT_EQ(RenderResourceLifetime::Transient, plan.findResource("stencil_aov")->lifetime);
    EXPECT_EQ(1, plan.findResource("stencil_aov")->sampleCount);
    ASSERT_NE(nullptr, plan.findResource("main_color"));
    EXPECT_EQ(RenderResourceType::Color, plan.findResource("main_color")->type);
    EXPECT_EQ(RenderResourceLifetime::Exported, plan.findResource("main_color")->lifetime);

    ASSERT_EQ(2u, plan.passes().size());
    EXPECT_EQ("stencil_aov", plan.passes()[0].id);
    EXPECT_EQ(RenderPassKind::AOV, plan.passes()[0].kind);
    EXPECT_EQ(RenderExecutorKind::Rasterizer, plan.passes()[0].executor);
    EXPECT_EQ(nullptr, plan.passes()[0].state);
    EXPECT_TRUE(hasFeature(plan.passes()[0], "stencil"));
    ASSERT_EQ(1u, plan.passes()[0].writes.size());
    EXPECT_EQ("stencil_aov", plan.passes()[0].writes[0].resource);

    EXPECT_EQ("visualize_stencil_aov", plan.passes()[1].id);
    EXPECT_EQ(RenderPassKind::AOV, plan.passes()[1].kind);
    EXPECT_EQ(RenderExecutorKind::PostProcess, plan.passes()[1].executor);
    EXPECT_TRUE(hasFeature(plan.passes()[1], "visualization"));
    ASSERT_EQ(1u, plan.passes()[1].reads.size());
    ASSERT_EQ(1u, plan.passes()[1].writes.size());
    EXPECT_EQ("stencil_aov", plan.passes()[1].reads[0].resource);
    EXPECT_EQ("main_color", plan.passes()[1].writes[0].resource);
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, StencilCompositeViewModeCompilesHybridPlan) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Raytracer;
    intent.defaultViewMode = RenderViewMode::StencilComposite;
    intent.exportedAOVs = {RenderViewMode::Stencil};

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent);

    ASSERT_NE(nullptr, plan.findResource("base_color"));
    ASSERT_NE(nullptr, plan.findResource("foreground_color"));
    ASSERT_NE(nullptr, plan.findResource("stencil_aov"));
    ASSERT_NE(nullptr, plan.findResource("composited_color"));
    ASSERT_NE(nullptr, plan.findResource("main_color"));
    ASSERT_NE(nullptr, plan.findResource("stencil_aov_color"));
    EXPECT_EQ(RenderResourceType::Stencil, plan.findResource("stencil_aov")->type);
    EXPECT_EQ(RenderResourceLifetime::Transient, plan.findResource("stencil_aov")->lifetime);
    EXPECT_EQ(RenderResourceLifetime::Exported, plan.findResource("main_color")->lifetime);

    const auto* raster = plan.findPass("raster_beauty");
    ASSERT_NE(nullptr, raster);
    EXPECT_EQ(RenderPassKind::Beauty, raster->kind);
    EXPECT_EQ(RenderExecutorKind::Rasterizer, raster->executor);
    EXPECT_TRUE(hasFeature(*raster, "stencil_composite_base"));
    ASSERT_EQ(1u, raster->writes.size());
    EXPECT_EQ("base_color", raster->writes.front().resource);

    const auto* wireframe = plan.findPass("wireframe_beauty");
    ASSERT_NE(nullptr, wireframe);
    EXPECT_EQ(RenderExecutorKind::Wireframe, wireframe->executor);
    EXPECT_TRUE(hasFeature(*wireframe, "stencil_composite_foreground"));
    ASSERT_EQ(1u, wireframe->writes.size());
    EXPECT_EQ("foreground_color", wireframe->writes.front().resource);

    const auto* stencil = plan.findPass("stencil_aov");
    ASSERT_NE(nullptr, stencil);
    EXPECT_EQ(RenderPassKind::AOV, stencil->kind);
    EXPECT_EQ(RenderExecutorKind::Rasterizer, stencil->executor);
    ASSERT_EQ(1u, stencil->writes.size());
    EXPECT_EQ("stencil_aov", stencil->writes.front().resource);

    const auto* composite = plan.findPass("stencil_composite");
    ASSERT_NE(nullptr, composite);
    EXPECT_EQ(RenderPassKind::Composite, composite->kind);
    EXPECT_EQ(RenderExecutorKind::Composite, composite->executor);
    EXPECT_TRUE(hasFeature(*composite, "stencil_composite"));
    ASSERT_EQ(3u, composite->reads.size());
    EXPECT_EQ("base_color", composite->reads[0].resource);
    EXPECT_EQ("foreground_color", composite->reads[1].resource);
    EXPECT_EQ("stencil_aov", composite->reads[2].resource);
    ASSERT_EQ(1u, composite->writes.size());
    EXPECT_EQ("composited_color", composite->writes.front().resource);

    const auto* visualizeStencil = plan.findPass("visualize_stencil_aov");
    ASSERT_NE(nullptr, visualizeStencil);
    EXPECT_EQ(RenderPassKind::AOV, visualizeStencil->kind);
    EXPECT_EQ(RenderExecutorKind::PostProcess, visualizeStencil->executor);
    ASSERT_EQ(1u, visualizeStencil->reads.size());
    EXPECT_EQ("stencil_aov", visualizeStencil->reads.front().resource);
    ASSERT_EQ(1u, visualizeStencil->writes.size());
    EXPECT_EQ("stencil_aov_color", visualizeStencil->writes.front().resource);

    const auto* tonemap = plan.findPass("tonemap");
    ASSERT_NE(nullptr, tonemap);
    ASSERT_EQ(1u, tonemap->reads.size());
    EXPECT_EQ("composited_color", tonemap->reads.front().resource);
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, NormalViewModeCompilesNormalAOVPlan) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Raytracer;
    intent.defaultViewMode = RenderViewMode::Normal;

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent);

    ASSERT_EQ(2u, plan.resources().size());
    ASSERT_NE(nullptr, plan.findResource("normal_aov"));
    EXPECT_EQ(RenderResourceType::Normal, plan.findResource("normal_aov")->type);
    EXPECT_EQ(RenderResourceFormat::RGBDouble, plan.findResource("normal_aov")->format);
    EXPECT_EQ(RenderResourceLifetime::Transient, plan.findResource("normal_aov")->lifetime);
    ASSERT_NE(nullptr, plan.findResource("main_color"));
    EXPECT_EQ(RenderResourceType::Color, plan.findResource("main_color")->type);
    EXPECT_EQ(RenderResourceLifetime::Exported, plan.findResource("main_color")->lifetime);

    ASSERT_EQ(2u, plan.passes().size());
    EXPECT_EQ("normal_aov", plan.passes()[0].id);
    EXPECT_EQ(RenderPassKind::AOV, plan.passes()[0].kind);
    EXPECT_EQ(RenderExecutorKind::Raytracer, plan.passes()[0].executor);
    EXPECT_TRUE(hasFeature(plan.passes()[0], "normal"));
    ASSERT_EQ(1u, plan.passes()[0].writes.size());
    EXPECT_EQ("normal_aov", plan.passes()[0].writes[0].resource);

    EXPECT_EQ("visualize_normal_aov", plan.passes()[1].id);
    EXPECT_EQ(RenderPassKind::AOV, plan.passes()[1].kind);
    EXPECT_EQ(RenderExecutorKind::PostProcess, plan.passes()[1].executor);
    EXPECT_TRUE(hasFeature(plan.passes()[1], "visualization"));
    ASSERT_EQ(1u, plan.passes()[1].reads.size());
    ASSERT_EQ(1u, plan.passes()[1].writes.size());
    EXPECT_EQ("normal_aov", plan.passes()[1].reads[0].resource);
    EXPECT_EQ("main_color", plan.passes()[1].writes[0].resource);
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, ObjectIdViewModeCompilesObjectIdAOVPlan) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Raytracer;
    intent.defaultViewMode = RenderViewMode::ObjectId;

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent);

    ASSERT_EQ(2u, plan.resources().size());
    ASSERT_NE(nullptr, plan.findResource("object_id_aov"));
    EXPECT_EQ(RenderResourceType::ObjectId, plan.findResource("object_id_aov")->type);
    EXPECT_EQ(RenderResourceFormat::UInt32, plan.findResource("object_id_aov")->format);
    EXPECT_EQ(RenderResourceLifetime::Transient, plan.findResource("object_id_aov")->lifetime);
    ASSERT_NE(nullptr, plan.findResource("main_color"));
    EXPECT_EQ(RenderResourceType::Color, plan.findResource("main_color")->type);
    EXPECT_EQ(RenderResourceLifetime::Exported, plan.findResource("main_color")->lifetime);

    ASSERT_EQ(2u, plan.passes().size());
    EXPECT_EQ("object_id_aov", plan.passes()[0].id);
    EXPECT_EQ(RenderPassKind::AOV, plan.passes()[0].kind);
    EXPECT_EQ(RenderExecutorKind::Raytracer, plan.passes()[0].executor);
    EXPECT_TRUE(hasFeature(plan.passes()[0], "object_id"));
    ASSERT_EQ(1u, plan.passes()[0].writes.size());
    EXPECT_EQ("object_id_aov", plan.passes()[0].writes[0].resource);

    EXPECT_EQ("visualize_object_id_aov", plan.passes()[1].id);
    EXPECT_EQ(RenderPassKind::AOV, plan.passes()[1].kind);
    EXPECT_EQ(RenderExecutorKind::PostProcess, plan.passes()[1].executor);
    EXPECT_TRUE(hasFeature(plan.passes()[1], "visualization"));
    ASSERT_EQ(1u, plan.passes()[1].reads.size());
    ASSERT_EQ(1u, plan.passes()[1].writes.size());
    EXPECT_EQ("object_id_aov", plan.passes()[1].reads[0].resource);
    EXPECT_EQ("main_color", plan.passes()[1].writes[0].resource);
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, MaterialIdViewModeCompilesMaterialIdAOVPlan) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Raytracer;
    intent.defaultViewMode = RenderViewMode::MaterialId;

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent);

    ASSERT_EQ(2u, plan.resources().size());
    ASSERT_NE(nullptr, plan.findResource("material_id_aov"));
    EXPECT_EQ(RenderResourceType::MaterialId, plan.findResource("material_id_aov")->type);
    EXPECT_EQ(RenderResourceFormat::UInt32, plan.findResource("material_id_aov")->format);
    EXPECT_EQ(RenderResourceLifetime::Transient, plan.findResource("material_id_aov")->lifetime);
    ASSERT_NE(nullptr, plan.findResource("main_color"));
    EXPECT_EQ(RenderResourceType::Color, plan.findResource("main_color")->type);
    EXPECT_EQ(RenderResourceLifetime::Exported, plan.findResource("main_color")->lifetime);

    ASSERT_EQ(2u, plan.passes().size());
    EXPECT_EQ("material_id_aov", plan.passes()[0].id);
    EXPECT_EQ(RenderPassKind::AOV, plan.passes()[0].kind);
    EXPECT_EQ(RenderExecutorKind::Raytracer, plan.passes()[0].executor);
    EXPECT_TRUE(hasFeature(plan.passes()[0], "material_id"));
    ASSERT_EQ(1u, plan.passes()[0].writes.size());
    EXPECT_EQ("material_id_aov", plan.passes()[0].writes[0].resource);

    EXPECT_EQ("visualize_material_id_aov", plan.passes()[1].id);
    EXPECT_EQ(RenderPassKind::AOV, plan.passes()[1].kind);
    EXPECT_EQ(RenderExecutorKind::PostProcess, plan.passes()[1].executor);
    EXPECT_TRUE(hasFeature(plan.passes()[1], "visualization"));
    ASSERT_EQ(1u, plan.passes()[1].reads.size());
    ASSERT_EQ(1u, plan.passes()[1].writes.size());
    EXPECT_EQ("material_id_aov", plan.passes()[1].reads[0].resource);
    EXPECT_EQ("main_color", plan.passes()[1].writes[0].resource);
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, WorldPositionViewModeCompilesWorldPositionAOVPlan) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Raytracer;
    intent.defaultViewMode = RenderViewMode::WorldPosition;

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent);

    ASSERT_EQ(2u, plan.resources().size());
    ASSERT_NE(nullptr, plan.findResource("world_position_aov"));
    EXPECT_EQ(RenderResourceType::WorldPosition, plan.findResource("world_position_aov")->type);
    EXPECT_EQ(RenderResourceFormat::RGBDouble, plan.findResource("world_position_aov")->format);
    EXPECT_EQ(RenderResourceLifetime::Transient, plan.findResource("world_position_aov")->lifetime);
    ASSERT_NE(nullptr, plan.findResource("main_color"));
    EXPECT_EQ(RenderResourceType::Color, plan.findResource("main_color")->type);
    EXPECT_EQ(RenderResourceLifetime::Exported, plan.findResource("main_color")->lifetime);

    ASSERT_EQ(2u, plan.passes().size());
    EXPECT_EQ("world_position_aov", plan.passes()[0].id);
    EXPECT_EQ(RenderPassKind::AOV, plan.passes()[0].kind);
    EXPECT_EQ(RenderExecutorKind::Raytracer, plan.passes()[0].executor);
    EXPECT_TRUE(hasFeature(plan.passes()[0], "world_position"));
    ASSERT_EQ(1u, plan.passes()[0].writes.size());
    EXPECT_EQ("world_position_aov", plan.passes()[0].writes[0].resource);

    EXPECT_EQ("visualize_world_position_aov", plan.passes()[1].id);
    EXPECT_EQ(RenderPassKind::AOV, plan.passes()[1].kind);
    EXPECT_EQ(RenderExecutorKind::PostProcess, plan.passes()[1].executor);
    EXPECT_TRUE(hasFeature(plan.passes()[1], "visualization"));
    ASSERT_EQ(1u, plan.passes()[1].reads.size());
    ASSERT_EQ(1u, plan.passes()[1].writes.size());
    EXPECT_EQ("world_position_aov", plan.passes()[1].reads[0].resource);
    EXPECT_EQ("main_color", plan.passes()[1].writes[0].resource);
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, AddsRequestedAOVExportsAsSideBranches) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.exportedAOVs = {RenderViewMode::Depth, RenderViewMode::Stencil, RenderViewMode::Normal,
                           RenderViewMode::Depth};

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent);

    ASSERT_NE(nullptr, plan.findResource("depth_aov"));
    EXPECT_EQ(RenderResourceType::Depth, plan.findResource("depth_aov")->type);
    EXPECT_EQ(RenderResourceLifetime::Exported, plan.findResource("depth_aov")->lifetime);
    ASSERT_NE(nullptr, plan.findResource("depth_aov_color"));
    EXPECT_EQ(RenderResourceType::Color, plan.findResource("depth_aov_color")->type);
    EXPECT_EQ(RenderResourceLifetime::Exported, plan.findResource("depth_aov_color")->lifetime);
    ASSERT_NE(nullptr, plan.findResource("normal_aov"));
    EXPECT_EQ(RenderResourceType::Normal, plan.findResource("normal_aov")->type);
    ASSERT_NE(nullptr, plan.findResource("normal_aov_color"));
    ASSERT_NE(nullptr, plan.findResource("stencil_aov"));
    EXPECT_EQ(RenderResourceType::Stencil, plan.findResource("stencil_aov")->type);
    ASSERT_NE(nullptr, plan.findResource("stencil_aov_color"));

    const auto* depthPass = plan.findPass("depth_aov");
    ASSERT_NE(nullptr, depthPass);
    EXPECT_TRUE(hasFeature(*depthPass, "export"));
    EXPECT_EQ(RenderExecutorKind::Raytracer, depthPass->executor);
    ASSERT_EQ(1u, depthPass->writes.size());
    EXPECT_EQ("depth_aov", depthPass->writes.front().resource);

    const auto* visualizeDepth = plan.findPass("visualize_depth_aov");
    ASSERT_NE(nullptr, visualizeDepth);
    EXPECT_TRUE(hasFeature(*visualizeDepth, "visualization"));
    ASSERT_EQ(1u, visualizeDepth->reads.size());
    ASSERT_EQ(1u, visualizeDepth->writes.size());
    EXPECT_EQ("depth_aov", visualizeDepth->reads.front().resource);
    EXPECT_EQ("depth_aov_color", visualizeDepth->writes.front().resource);

    ASSERT_EQ(1u, plan.consumersOf("depth_aov").size());
    EXPECT_EQ("visualize_depth_aov", plan.consumersOf("depth_aov").front()->id);
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, AppliesWholeFrameViewOverrideBeforeSynthesizingNodes) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Raytracer;

    RenderViewOverride override;
    override.selector = SceneSelector::all();
    override.executor = RenderExecutorPreference::Rasterizer;
    override.viewMode = RenderViewMode::Depth;
    override.shadingProfile = ShadingProfileRef{"clay", {}};
    override.shadingProfile->parameters.emplace("levels", ShadingProfileParameterValue(3.0));
    override.camera = RenderCameraRef{"inspection-camera", std::nullopt};
    intent.viewOverrides.push_back(override);

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent);

    ASSERT_EQ(2u, plan.passes().size());
    EXPECT_EQ("depth_aov", plan.passes()[0].id);
    EXPECT_EQ(RenderExecutorKind::Rasterizer, plan.passes()[0].executor);
    ASSERT_TRUE(plan.passes()[0].sceneView.camera.has_value());
    ASSERT_TRUE(plan.passes()[0].sceneView.camera->sceneCameraId.has_value());
    EXPECT_EQ("inspection-camera", *plan.passes()[0].sceneView.camera->sceneCameraId);
    ASSERT_TRUE(plan.passes()[0].sceneView.shadingProfile.has_value());
    EXPECT_EQ("clay", plan.passes()[0].sceneView.shadingProfile->name);
    EXPECT_EQ(ShadingProfileParameterValue(3.0),
              plan.passes()[0].sceneView.shadingProfile->parameters.at("levels"));
    EXPECT_EQ("visualize_depth_aov", plan.passes()[1].id);
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, RejectsSelectorSpecificOverridesUntilScenePartitioningExists) {
    RenderGraphCompiler compiler;
    RenderIntent intent;

    RenderViewOverride override;
    override.selector = SceneSelector::objectName("Monitor");
    override.executor = RenderExecutorPreference::Wireframe;
    intent.viewOverrides.push_back(override);

    try {
      compiler.compile({64, 32, 1}, intent);
      FAIL() << "Expected selector-specific graph compilation rejection";
    } catch (const std::runtime_error& error) {
      const std::string message = error.what();
      EXPECT_NE(std::string::npos, message.find("selector-specific render intent"));
      EXPECT_NE(std::string::npos, message.find("object_name: Monitor"));
    }
  }

  TEST(RenderGraphCompiler, NormalizesNonPositiveSampleCount) {
    RenderGraphCompiler compiler;
    RenderIntent intent;

    const RenderPlan plan = compiler.compile({64, 64, 0}, intent);

    ASSERT_EQ(2u, plan.resources().size());
    EXPECT_EQ(1, plan.resources()[0].sampleCount);
    EXPECT_EQ(1, plan.resources()[1].sampleCount);
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, RasterTargetSampleCountBecomesRasterPassState) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;

    const RenderPlan plan = compiler.compile({64, 64, 4}, intent);

    ASSERT_EQ(2u, plan.passes().size());
    EXPECT_EQ("raster_beauty", plan.passes()[0].id);
    ASSERT_NE(nullptr, plan.passes()[0].state);
    EXPECT_EQ(4, RasterBeautyPassState::fromPass(plan.passes()[0])->sampling().msaaSamples());
    EXPECT_EQ(4, plan.resources()[0].sampleCount);
    EXPECT_EQ(4, plan.resources()[1].sampleCount);
  }

  TEST(RenderGraphCompiler, TonemapPassCanBeDisabledWithPassthrough) {
    RenderGraphCompiler compiler;
    RenderIntent intent;

    RenderGraphOverrides overrides;
    overrides.disabledPasses.insert("tonemap");

    const RenderPlan plan = compiler.compile({64, 64, 1}, intent).withOverrides(overrides);

    ASSERT_EQ(2u, plan.passes().size());
    EXPECT_FALSE(plan.passes()[1].enabled);
    EXPECT_EQ(DisabledBehavior::Passthrough, plan.passes()[1].disabledBehavior);
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, WireframeOverlayIntentAddsOverlayPassBeforeTonemap) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.enableWireframeOverlay = true;

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent);

    ASSERT_EQ(3u, plan.resources().size());
    ASSERT_NE(nullptr, plan.findResource("beauty_color"));
    EXPECT_EQ(RenderResourceLifetime::Transient, plan.findResource("beauty_color")->lifetime);
    ASSERT_NE(nullptr, plan.findResource("overlay_color"));
    EXPECT_EQ(RenderResourceLifetime::Transient, plan.findResource("overlay_color")->lifetime);
    ASSERT_NE(nullptr, plan.findResource("main_color"));
    EXPECT_EQ(RenderResourceLifetime::Exported, plan.findResource("main_color")->lifetime);

    ASSERT_EQ(3u, plan.passes().size());
    EXPECT_EQ("raytrace_beauty", plan.passes()[0].id);
    EXPECT_EQ("wireframe_overlay", plan.passes()[1].id);
    EXPECT_EQ(RenderPassKind::Overlay, plan.passes()[1].kind);
    EXPECT_EQ(RenderExecutorKind::Wireframe, plan.passes()[1].executor);
    EXPECT_EQ(DisabledBehavior::Passthrough, plan.passes()[1].disabledBehavior);
    ASSERT_EQ(1u, plan.passes()[1].reads.size());
    ASSERT_EQ(1u, plan.passes()[1].writes.size());
    EXPECT_EQ("beauty_color", plan.passes()[1].reads[0].resource);
    EXPECT_EQ("overlay_color", plan.passes()[1].writes[0].resource);
    EXPECT_EQ("tonemap", plan.passes()[2].id);
    ASSERT_EQ(1u, plan.passes()[2].reads.size());
    EXPECT_EQ("overlay_color", plan.passes()[2].reads[0].resource);
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, WireframeOverlayPassCanBeDisabledWithPassthrough) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.enableWireframeOverlay = true;

    RenderGraphOverrides overrides;
    overrides.disabledPasses.insert("wireframe_overlay");

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent).withOverrides(overrides);

    ASSERT_EQ(3u, plan.passes().size());
    EXPECT_FALSE(plan.passes()[1].enabled);
    EXPECT_EQ(DisabledBehavior::Passthrough, plan.passes()[1].disabledBehavior);
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, CurveOverlayIntentAddsOverlayPassBeforeTonemap) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.enableCurveOverlay = true;

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent);

    ASSERT_EQ(3u, plan.resources().size());
    ASSERT_NE(nullptr, plan.findResource("beauty_color"));
    ASSERT_NE(nullptr, plan.findResource("curve_overlay_color"));
    EXPECT_EQ(RenderResourceLifetime::Transient,
              plan.findResource("curve_overlay_color")->lifetime);
    ASSERT_NE(nullptr, plan.findResource("main_color"));

    ASSERT_EQ(3u, plan.passes().size());
    EXPECT_EQ("raytrace_beauty", plan.passes()[0].id);
    EXPECT_EQ("curve_overlay", plan.passes()[1].id);
    EXPECT_EQ(RenderPassKind::Overlay, plan.passes()[1].kind);
    EXPECT_EQ(RenderExecutorKind::Wireframe, plan.passes()[1].executor);
    EXPECT_TRUE(hasFeature(plan.passes()[1], "curve_overlay"));
    EXPECT_TRUE(hasFeature(plan.passes()[1], "curve"));
    EXPECT_EQ(DisabledBehavior::Passthrough, plan.passes()[1].disabledBehavior);
    ASSERT_EQ(1u, plan.passes()[1].reads.size());
    ASSERT_EQ(1u, plan.passes()[1].writes.size());
    EXPECT_EQ("beauty_color", plan.passes()[1].reads[0].resource);
    EXPECT_EQ("curve_overlay_color", plan.passes()[1].writes[0].resource);
    EXPECT_EQ("tonemap", plan.passes()[2].id);
    ASSERT_EQ(1u, plan.passes()[2].reads.size());
    EXPECT_EQ("curve_overlay_color", plan.passes()[2].reads[0].resource);
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, RasterPreviewShadowsAddGraphShadowPass) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.enablePreviewShadows = true;

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent);

    ASSERT_EQ(3u, plan.passes().size());
    EXPECT_EQ("raster_preview_shadows", plan.passes()[0].id);
    EXPECT_EQ(RenderPassKind::Shadow, plan.passes()[0].kind);
    EXPECT_EQ(RenderExecutorKind::Rasterizer, plan.passes()[0].executor);
    EXPECT_EQ(DisabledBehavior::SubstituteDefault, plan.passes()[0].disabledBehavior);
    ASSERT_NE(nullptr, RasterShadowPassState::fromPass(plan.passes()[0]));
    EXPECT_TRUE(RasterShadowPassState::fromPass(plan.passes()[0])->shadows().enabled());
    ASSERT_EQ(1u, plan.passes()[0].writes.size());
    EXPECT_EQ("preview_shadow_map", plan.passes()[0].writes[0].resource);

    EXPECT_EQ("raster_beauty", plan.passes()[1].id);
    ASSERT_EQ(1u, plan.passes()[1].reads.size());
    EXPECT_EQ("preview_shadow_map", plan.passes()[1].reads[0].resource);
    ASSERT_NE(nullptr, plan.findResource("preview_shadow_map"));
    const auto* shadowMap = plan.findResource("preview_shadow_map");
    EXPECT_EQ(RenderResourceType::ShadowMap, shadowMap->type);
    EXPECT_EQ(RenderResourceFormat::DepthDouble, shadowMap->format);
    EXPECT_EQ(256, shadowMap->width);
    EXPECT_EQ(256, shadowMap->height);
    EXPECT_EQ(RenderResourceLifetime::PersistentCache, shadowMap->lifetime);
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, SceneAnalysisCanSuppressUnneededRasterPreviewShadowPass) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.enablePreviewShadows = true;
    RenderSceneAnalysis analysis;
    analysis.recordVisibleSurface();

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent, analysis);

    ASSERT_EQ(2u, plan.passes().size());
    EXPECT_EQ("raster_beauty", plan.passes()[0].id);
    EXPECT_EQ("tonemap", plan.passes()[1].id);
    EXPECT_EQ(nullptr, plan.findPass("raster_preview_shadows"));
    EXPECT_EQ(nullptr, plan.findResource("preview_shadow_map"));
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, RasterPreviewShadowPassCanSubstituteDefaultWhenDisabled) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.enablePreviewShadows = true;

    RenderGraphOverrides overrides;
    overrides.disabledPasses.insert("raster_preview_shadows");

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent).withOverrides(overrides);

    ASSERT_EQ(3u, plan.passes().size());
    EXPECT_FALSE(plan.passes()[0].enabled);
    EXPECT_TRUE(plan.passes()[1].enabled);
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, RaytracerFxaaIntentAddsGraphPostProcessPass) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.postProcessAA = RenderPostProcessAA::FXAA;

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent);

    ASSERT_EQ(3u, plan.resources().size());
    EXPECT_NE(nullptr, plan.findResource("beauty_color"));
    EXPECT_NE(nullptr, plan.findResource("post_aa_color"));
    EXPECT_NE(nullptr, plan.findResource("main_color"));

    ASSERT_EQ(3u, plan.passes().size());
    EXPECT_EQ("raytrace_beauty", plan.passes()[0].id);
    EXPECT_EQ("post_fxaa", plan.passes()[1].id);
    EXPECT_EQ(RenderPassKind::PostProcess, plan.passes()[1].kind);
    EXPECT_EQ(RenderExecutorKind::PostProcess, plan.passes()[1].executor);
    EXPECT_TRUE(hasFeature(plan.passes()[1], "raytracer"));
    ASSERT_EQ(1u, plan.passes()[1].reads.size());
    ASSERT_EQ(1u, plan.passes()[1].writes.size());
    EXPECT_EQ("beauty_color", plan.passes()[1].reads[0].resource);
    EXPECT_EQ("post_aa_color", plan.passes()[1].writes[0].resource);
    ASSERT_NE(nullptr, PostProcessAAState::fromPass(plan.passes()[1]));
    EXPECT_EQ("fxaa", std::string(PostProcessAAState::fromPass(plan.passes()[1])->modeName()));
    EXPECT_EQ("post_aa_color", plan.passes()[2].reads[0].resource);
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, WireframeSmaaIntentFeedsOverlayBeforeTonemap) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Wireframe;
    intent.postProcessAA = RenderPostProcessAA::SMAA;
    intent.enableWireframeOverlay = true;

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent);

    ASSERT_EQ(4u, plan.passes().size());
    EXPECT_EQ("wireframe_beauty", plan.passes()[0].id);
    EXPECT_EQ("post_smaa", plan.passes()[1].id);
    EXPECT_EQ("wireframe_overlay", plan.passes()[2].id);
    EXPECT_EQ("tonemap", plan.passes()[3].id);
    EXPECT_TRUE(hasFeature(plan.passes()[1], "wireframe"));
    ASSERT_EQ(1u, plan.passes()[2].reads.size());
    EXPECT_EQ("post_aa_color", plan.passes()[2].reads[0].resource);
    ASSERT_EQ(1u, plan.passes()[3].reads.size());
    EXPECT_EQ("overlay_color", plan.passes()[3].reads[0].resource);
    ASSERT_NE(nullptr, PostProcessAAState::fromPass(plan.passes()[1]));
    EXPECT_EQ("smaa", std::string(PostProcessAAState::fromPass(plan.passes()[1])->modeName()));
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, RasterTaaIntentDoesNotAddImagePostProcessPass) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.postProcessAA = RenderPostProcessAA::TAA;

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent);

    ASSERT_EQ(2u, plan.passes().size());
    EXPECT_EQ("raster_beauty", plan.passes()[0].id);
    EXPECT_EQ("tonemap", plan.passes()[1].id);
    EXPECT_TRUE(plan.validate().valid());
  }
}
