#include <gtest/gtest.h>

#include "engine/graph/PostProcessPassState.h"
#include "engine/graph/RasterPassState.h"
#include "engine/graph/RenderGraphCompiler.h"

#include <algorithm>
#include <string>

namespace RenderGraphCompilerTest {
  using namespace engine::graph;

  bool hasFeature(const RenderPassNode& pass, const std::string& feature) {
    return std::find(pass.features.begin(), pass.features.end(), feature) != pass.features.end();
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
