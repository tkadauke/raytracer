#include <gtest/gtest.h>

#include "engine/graph/PostProcessPassState.h"
#include "engine/graph/RasterPassState.h"
#include "engine/graph/RaytracerPassState.h"
#include "engine/graph/RenderGraphCompiler.h"
#include "engine/graph/WireframePassState.h"

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

  TEST(RenderGraphCompiler, UsesWavefrontExecutorPreference) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Wavefront;
    intent.engineOptions.raytracer().setSamplesPerPixel(4);
    intent.engineOptions.raytracer().setIntegrator("pathtracer");
    intent.engineOptions.raytracer().setConvergenceEnabled(true);
    intent.engineOptions.raytracer().setConvergenceActiveSampleFractionThreshold(0.25);
    intent.engineOptions.raytracer().setAdaptiveSamplingEnabled(true);
    intent.engineOptions.raytracer().setAdaptiveMinimumSamples(3);
    intent.engineOptions.raytracer().setAdaptiveStddevThreshold(0.05);
    intent.engineOptions.raytracer().setDenoiser("box");
    intent.engineOptions.raytracer().setDenoiseRadius(2);

    const RenderPlan plan = compiler.compile({64, 64, 4}, intent);

    ASSERT_EQ(2u, plan.passes().size());
    EXPECT_EQ("wavefront_beauty", plan.passes()[0].id);
    EXPECT_EQ(RenderExecutorKind::Wavefront, plan.passes()[0].executor);
    EXPECT_TRUE(hasFeature(plan.passes()[0], "wavefront"));
    const auto* state = RaytracerBeautyPassState::fromPass(plan.passes()[0]);
    ASSERT_NE(nullptr, state);
    ASSERT_TRUE(state->integrator().has_value());
    EXPECT_EQ("pathtracer", *state->integrator());
    ASSERT_TRUE(state->samplesPerPixel().has_value());
    EXPECT_EQ(4, *state->samplesPerPixel());
    ASSERT_TRUE(state->convergenceEnabled().has_value());
    EXPECT_TRUE(*state->convergenceEnabled());
    ASSERT_TRUE(state->convergenceActiveSampleFractionThreshold().has_value());
    EXPECT_DOUBLE_EQ(0.25, *state->convergenceActiveSampleFractionThreshold());
    ASSERT_TRUE(state->adaptiveSamplingEnabled().has_value());
    EXPECT_TRUE(*state->adaptiveSamplingEnabled());
    ASSERT_TRUE(state->adaptiveMinimumSamples().has_value());
    EXPECT_EQ(3, *state->adaptiveMinimumSamples());
    ASSERT_TRUE(state->adaptiveStddevThreshold().has_value());
    EXPECT_DOUBLE_EQ(0.05, *state->adaptiveStddevThreshold());
    ASSERT_TRUE(state->denoiser().has_value());
    EXPECT_EQ("box", *state->denoiser());
    ASSERT_TRUE(state->denoiseRadius().has_value());
    EXPECT_EQ(2, *state->denoiseRadius());
    EXPECT_EQ("tonemap", plan.passes()[1].id);
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, PathTracerPreferenceUsesWavefrontExecutorWithPathTracerState) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::PathTracer;
    intent.engineOptions.raytracer().setSamplesPerPixel(4);
    intent.engineOptions.raytracer().setIntegrator("whitted");

    const RenderPlan plan = compiler.compile({64, 64, 4}, intent);

    ASSERT_EQ(2u, plan.passes().size());
    EXPECT_EQ("wavefront_beauty", plan.passes()[0].id);
    EXPECT_EQ("Path traced beauty", plan.passes()[0].name);
    EXPECT_EQ(RenderExecutorKind::Wavefront, plan.passes()[0].executor);
    EXPECT_TRUE(hasFeature(plan.passes()[0], "pathtracer"));
    EXPECT_FALSE(hasFeature(plan.passes()[0], "wavefront"));
    const auto* state = RaytracerBeautyPassState::fromPass(plan.passes()[0]);
    ASSERT_NE(nullptr, state);
    ASSERT_TRUE(state->integrator().has_value());
    EXPECT_EQ("pathtracer", *state->integrator());
    ASSERT_TRUE(state->samplesPerPixel().has_value());
    EXPECT_EQ(4, *state->samplesPerPixel());
    EXPECT_EQ("tonemap", plan.passes()[1].id);
    EXPECT_TRUE(plan.validate().valid());
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
    ASSERT_NE(nullptr, plan.passes()[0].state);
    {
      const QJsonObject framebuffer =
        plan.passes()[0].state->toJson().value("framebuffer").toObject();
      EXPECT_EQ("none", framebuffer.value("colorWriteMask").toString().toStdString());
      EXPECT_TRUE(framebuffer.value("stencilTest").toBool());
      EXPECT_EQ(255, framebuffer.value("stencilReference").toInt());
      EXPECT_EQ("replace", framebuffer.value("stencilPassOp").toString().toStdString());
    }
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
    ASSERT_NE(nullptr, stencil->state);
    EXPECT_TRUE(
      stencil->state->toJson().value("framebuffer").toObject().value("stencilTest").toBool());
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

  TEST(RenderGraphCompiler, OpenGLStencilCompositeRoutesRasterInputsThroughReadbackPasses) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultViewMode = RenderViewMode::StencilComposite;
    intent.engineOptions.rasterizer().setBackend(engine::raster::RasterBackend::openGL());
    intent.exportedAOVs = {RenderViewMode::Stencil};

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent);

    const auto* baseReadback = plan.findPass("readback_base_color");
    ASSERT_NE(nullptr, baseReadback);
    EXPECT_EQ(RenderPassKind::Readback, baseReadback->kind);
    ASSERT_EQ(1u, baseReadback->reads.size());
    ASSERT_EQ(1u, baseReadback->writes.size());
    EXPECT_EQ("base_color", baseReadback->reads.front().resource);
    EXPECT_EQ("base_readback_color", baseReadback->writes.front().resource);
    EXPECT_TRUE(hasFeature(*baseReadback, "stencil_composite"));

    const auto* stencilProducer = plan.findPass("stencil_composite_mask");
    ASSERT_NE(nullptr, stencilProducer);
    EXPECT_EQ(RenderPassKind::AOV, stencilProducer->kind);
    EXPECT_EQ(RenderExecutorKind::Rasterizer, stencilProducer->executor);
    ASSERT_EQ(1u, stencilProducer->writes.size());
    EXPECT_EQ("stencil_composite_mask_source", stencilProducer->writes.front().resource);

    const auto* stencilReadback = plan.findPass("readback_stencil_composite_mask");
    ASSERT_NE(nullptr, stencilReadback);
    EXPECT_EQ(RenderPassKind::Readback, stencilReadback->kind);
    ASSERT_EQ(1u, stencilReadback->reads.size());
    ASSERT_EQ(1u, stencilReadback->writes.size());
    EXPECT_EQ("stencil_composite_mask_source", stencilReadback->reads.front().resource);
    EXPECT_EQ("stencil_composite_mask", stencilReadback->writes.front().resource);
    EXPECT_TRUE(hasFeature(*stencilReadback, "stencil_composite"));
    EXPECT_TRUE(hasFeature(*stencilReadback, "stencil"));

    const auto* composite = plan.findPass("stencil_composite");
    ASSERT_NE(nullptr, composite);
    ASSERT_EQ(3u, composite->reads.size());
    EXPECT_EQ("base_readback_color", composite->reads[0].resource);
    EXPECT_EQ("foreground_color", composite->reads[1].resource);
    EXPECT_EQ("stencil_composite_mask", composite->reads[2].resource);

    const auto* exportedStencil = plan.findResource("stencil_aov");
    ASSERT_NE(nullptr, exportedStencil);
    EXPECT_EQ(RenderResourceLifetime::Exported, exportedStencil->lifetime);
    const auto* exportedStencilReadback = plan.findPass("readback_stencil_aov");
    ASSERT_NE(nullptr, exportedStencilReadback);
    EXPECT_TRUE(hasFeature(*exportedStencilReadback, "export"));
    EXPECT_FALSE(hasFeature(*exportedStencilReadback, "stencil_composite"));
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

  TEST(RenderGraphCompiler, SampleStddevViewModeCompilesWavefrontAOVPlan) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::PathTracer;
    intent.defaultViewMode = RenderViewMode::SampleStddev;
    intent.engineOptions.raytracer().setIntegrator("pathtracer");
    intent.engineOptions.raytracer().setSamplesPerPixel(4);

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent);

    ASSERT_EQ(2u, plan.resources().size());
    ASSERT_NE(nullptr, plan.findResource("sample_stddev_aov"));
    EXPECT_EQ(RenderResourceType::Color, plan.findResource("sample_stddev_aov")->type);
    EXPECT_EQ(RenderResourceFormat::RGBDouble, plan.findResource("sample_stddev_aov")->format);
    EXPECT_EQ(RenderResourceLifetime::Transient, plan.findResource("sample_stddev_aov")->lifetime);
    ASSERT_NE(nullptr, plan.findResource("main_color"));
    EXPECT_EQ(RenderResourceType::Color, plan.findResource("main_color")->type);
    EXPECT_EQ(RenderResourceLifetime::Exported, plan.findResource("main_color")->lifetime);

    ASSERT_EQ(2u, plan.passes().size());
    EXPECT_EQ("sample_stddev_aov", plan.passes()[0].id);
    EXPECT_EQ(RenderPassKind::AOV, plan.passes()[0].kind);
    EXPECT_EQ(RenderExecutorKind::Wavefront, plan.passes()[0].executor);
    EXPECT_TRUE(hasFeature(plan.passes()[0], "sample_stddev"));
    EXPECT_TRUE(hasFeature(plan.passes()[0], "wavefront"));
    const auto* state = RaytracerBeautyPassState::fromPass(plan.passes()[0]);
    ASSERT_NE(nullptr, state);
    ASSERT_TRUE(state->integrator().has_value());
    EXPECT_EQ("pathtracer", *state->integrator());
    ASSERT_TRUE(state->samplesPerPixel().has_value());
    EXPECT_EQ(4, *state->samplesPerPixel());
    ASSERT_EQ(1u, plan.passes()[0].writes.size());
    EXPECT_EQ("sample_stddev_aov", plan.passes()[0].writes[0].resource);

    EXPECT_EQ("visualize_sample_stddev_aov", plan.passes()[1].id);
    EXPECT_EQ(RenderPassKind::AOV, plan.passes()[1].kind);
    EXPECT_EQ(RenderExecutorKind::PostProcess, plan.passes()[1].executor);
    EXPECT_TRUE(hasFeature(plan.passes()[1], "visualization"));
    ASSERT_EQ(1u, plan.passes()[1].reads.size());
    ASSERT_EQ(1u, plan.passes()[1].writes.size());
    EXPECT_EQ("sample_stddev_aov", plan.passes()[1].reads[0].resource);
    EXPECT_EQ("main_color", plan.passes()[1].writes[0].resource);
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, SampleStddevAOVRejectsNonWavefrontExecutor) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Raytracer;
    intent.defaultViewMode = RenderViewMode::SampleStddev;

    EXPECT_THROW(compiler.compile({64, 32, 1}, intent), std::runtime_error);
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

  TEST(RenderGraphCompiler, RasterCounterViewModeCompilesRasterDiagnosticAOVPlan) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.defaultViewMode = RenderViewMode::RasterDepthTestCount;

    const RenderPlan plan = compiler.compile({64, 32, 4}, intent);

    ASSERT_EQ(2u, plan.resources().size());
    ASSERT_NE(nullptr, plan.findResource("raster_depth_test_count_aov"));
    EXPECT_EQ(RenderResourceType::CustomTexture,
              plan.findResource("raster_depth_test_count_aov")->type);
    EXPECT_EQ(RenderResourceFormat::RGBDouble,
              plan.findResource("raster_depth_test_count_aov")->format);
    EXPECT_EQ(RenderResourceLifetime::Transient,
              plan.findResource("raster_depth_test_count_aov")->lifetime);
    ASSERT_NE(nullptr, plan.findResource("main_color"));
    EXPECT_EQ(RenderResourceType::Color, plan.findResource("main_color")->type);
    EXPECT_EQ(RenderResourceLifetime::Exported, plan.findResource("main_color")->lifetime);

    ASSERT_EQ(2u, plan.passes().size());
    EXPECT_EQ("raster_depth_test_count_aov", plan.passes()[0].id);
    EXPECT_EQ(RenderPassKind::AOV, plan.passes()[0].kind);
    EXPECT_EQ(RenderExecutorKind::Rasterizer, plan.passes()[0].executor);
    ASSERT_NE(nullptr, plan.passes()[0].state);
    EXPECT_EQ(4, RasterBeautyPassState::fromPass(plan.passes()[0])->sampling().msaaSamples());
    EXPECT_TRUE(hasFeature(plan.passes()[0], "raster_depth_test_count"));
    ASSERT_EQ(1u, plan.passes()[0].writes.size());
    EXPECT_EQ("raster_depth_test_count_aov", plan.passes()[0].writes[0].resource);

    EXPECT_EQ("visualize_raster_depth_test_count_aov", plan.passes()[1].id);
    EXPECT_EQ(RenderPassKind::AOV, plan.passes()[1].kind);
    EXPECT_EQ(RenderExecutorKind::PostProcess, plan.passes()[1].executor);
    EXPECT_TRUE(hasFeature(plan.passes()[1], "visualization"));
    ASSERT_EQ(1u, plan.passes()[1].reads.size());
    ASSERT_EQ(1u, plan.passes()[1].writes.size());
    EXPECT_EQ("raster_depth_test_count_aov", plan.passes()[1].reads[0].resource);
    EXPECT_EQ("main_color", plan.passes()[1].writes[0].resource);
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, RasterCounterViewModeRejectsNonRasterExecutor) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Raytracer;
    intent.defaultViewMode = RenderViewMode::RasterCoverageCount;

    EXPECT_THROW(compiler.compile({64, 32, 1}, intent), std::runtime_error);
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

  TEST(RenderGraphCompiler, CompilesSubviewIntentAsPrefixedRenderToTextureBranch) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.engineOptions.rasterizer().setMSAASamples(4);

    RenderSubviewIntent subview;
    subview.name = "mirror probe";
    subview.view.selector = SceneSelector::all();
    subview.view.executor = RenderExecutorPreference::Rasterizer;
    subview.view.camera = RenderCameraRef{"mirror-camera", std::nullopt};
    subview.view.engineOptions.rasterizer().setMSAASamples(2);
    intent.subviews.push_back(subview);

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent);

    ASSERT_NE(nullptr, plan.findPass("subview_mirror_probe_raster_beauty"));
    ASSERT_NE(nullptr, plan.findPass("subview_mirror_probe_depth_aov"));
    ASSERT_NE(nullptr, plan.findPass("subview_mirror_probe_tonemap"));
    ASSERT_NE(nullptr, plan.findResource("subview_mirror_probe_beauty_color"));
    const auto* depth = plan.findResource("subview_mirror_probe_depth_aov");
    ASSERT_NE(nullptr, depth);
    EXPECT_EQ(RenderResourceType::Depth, depth->type);
    EXPECT_EQ(RenderResourceLifetime::Exported, depth->lifetime);
    EXPECT_TRUE(depth->hasFeature("subview"));
    EXPECT_TRUE(depth->hasFeature("subview:subview_mirror_probe"));
    EXPECT_TRUE(depth->hasFeature("render_to_texture"));
    EXPECT_TRUE(depth->hasFeature("subview_output"));
    EXPECT_TRUE(depth->hasFeature("subview_depth_output"));
    const auto* output = plan.findResource("subview_mirror_probe_main_color");
    ASSERT_NE(nullptr, output);
    EXPECT_EQ(RenderResourceLifetime::Exported, output->lifetime);
    EXPECT_TRUE(output->hasFeature("subview"));
    EXPECT_TRUE(output->hasFeature("subview:subview_mirror_probe"));
    EXPECT_TRUE(output->hasFeature("render_to_texture"));
    EXPECT_TRUE(output->hasFeature("subview_output"));
    EXPECT_TRUE(output->hasFeature("subview_color_output"));

    const auto* subviewBeauty = plan.findPass("subview_mirror_probe_raster_beauty");
    ASSERT_NE(nullptr, subviewBeauty);
    EXPECT_TRUE(hasFeature(*subviewBeauty, "subview"));
    EXPECT_TRUE(hasFeature(*subviewBeauty, "subview:subview_mirror_probe"));
    EXPECT_TRUE(hasFeature(*subviewBeauty, "render_to_texture"));
    ASSERT_TRUE(subviewBeauty->sceneView.camera.has_value());
    ASSERT_TRUE(subviewBeauty->sceneView.camera->sceneCameraId.has_value());
    EXPECT_EQ("mirror-camera", *subviewBeauty->sceneView.camera->sceneCameraId);
    ASSERT_NE(nullptr, subviewBeauty->state);
    const auto state = subviewBeauty->state->toJson();
    EXPECT_EQ(2, state.value("sampling").toObject().value("msaaSamples").toInt());

    EXPECT_TRUE(plan.resourceCanReach("subview_mirror_probe_beauty_color",
                                      "subview_mirror_probe_main_color"));
    EXPECT_FALSE(plan.resourceCanReach("subview_mirror_probe_main_color", "main_color"));
    const auto subviewOutputs = plan.resourcesWithFeature("subview_output");
    ASSERT_EQ(2u, subviewOutputs.size());
    EXPECT_NE(nullptr, plan.findResource(subviewOutputs[0]->id));
    EXPECT_NE(nullptr, plan.findResource(subviewOutputs[1]->id));
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, OpenGLSubviewIntentRoutesRasterProductsThroughReadbackPasses) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.engineOptions.rasterizer().setBackend(engine::raster::RasterBackend::openGL());

    RenderSubviewIntent subview;
    subview.name = "portal view";
    subview.view.selector = SceneSelector::all();
    subview.view.executor = RenderExecutorPreference::Rasterizer;
    intent.subviews.push_back(subview);

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent);

    const auto* beauty = plan.findPass("subview_portal_view_raster_beauty");
    ASSERT_NE(nullptr, beauty);
    EXPECT_EQ(RenderExecutorKind::Rasterizer, beauty->executor);
    const auto* beautyState = RasterBeautyPassState::fromPass(*beauty);
    ASSERT_NE(nullptr, beautyState);
    EXPECT_TRUE(beautyState->execution().backend().isOpenGL());

    const auto* beautyReadback = plan.findPass("subview_portal_view_beauty_readback");
    ASSERT_NE(nullptr, beautyReadback);
    EXPECT_EQ(RenderPassKind::Readback, beautyReadback->kind);
    EXPECT_EQ(RenderExecutorKind::PostProcess, beautyReadback->executor);
    EXPECT_TRUE(hasFeature(*beautyReadback, "subview"));
    EXPECT_TRUE(hasFeature(*beautyReadback, "render_to_texture"));
    ASSERT_EQ(1u, beautyReadback->reads.size());
    ASSERT_EQ(1u, beautyReadback->writes.size());
    EXPECT_EQ("subview_portal_view_beauty_color", beautyReadback->reads.front().resource);
    EXPECT_EQ("subview_portal_view_beauty_readback_color", beautyReadback->writes.front().resource);

    const auto* tonemap = plan.findPass("subview_portal_view_tonemap");
    ASSERT_NE(nullptr, tonemap);
    ASSERT_EQ(1u, tonemap->reads.size());
    EXPECT_EQ("subview_portal_view_beauty_readback_color", tonemap->reads.front().resource);

    const auto* depthProducer = plan.findPass("subview_portal_view_depth_aov");
    ASSERT_NE(nullptr, depthProducer);
    const auto* depthState = RasterBeautyPassState::fromPass(*depthProducer);
    ASSERT_NE(nullptr, depthState);
    EXPECT_TRUE(depthState->execution().backend().isOpenGL());
    ASSERT_EQ(1u, depthProducer->writes.size());
    EXPECT_EQ("subview_portal_view_depth_aov_source", depthProducer->writes.front().resource);

    const auto* depthReadback = plan.findPass("subview_portal_view_readback_depth_aov");
    ASSERT_NE(nullptr, depthReadback);
    EXPECT_EQ(RenderPassKind::Readback, depthReadback->kind);
    EXPECT_TRUE(hasFeature(*depthReadback, "subview"));
    EXPECT_TRUE(hasFeature(*depthReadback, "render_to_texture"));
    ASSERT_EQ(1u, depthReadback->reads.size());
    ASSERT_EQ(1u, depthReadback->writes.size());
    EXPECT_EQ("subview_portal_view_depth_aov_source", depthReadback->reads.front().resource);
    EXPECT_EQ("subview_portal_view_depth_aov", depthReadback->writes.front().resource);

    EXPECT_TRUE(
      plan.resourceCanReach("subview_portal_view_beauty_color", "subview_portal_view_main_color"));
    EXPECT_TRUE(plan.resourceCanReach("subview_portal_view_depth_aov_source",
                                      "subview_portal_view_depth_aov"));
    EXPECT_FALSE(plan.resourceCanReach("subview_portal_view_main_color", "main_color"));
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, RejectsSubviewSelectorsUntilScenePartitioningExists) {
    RenderGraphCompiler compiler;
    RenderIntent intent;

    RenderSubviewIntent subview;
    subview.name = "monitor feed";
    subview.view.selector = SceneSelector::objectName("Monitor");
    subview.view.executor = RenderExecutorPreference::Rasterizer;
    intent.subviews.push_back(subview);

    try {
      compiler.compile({64, 32, 1}, intent);
      FAIL() << "Expected selector-specific subview graph compilation rejection";
    } catch (const std::runtime_error& error) {
      const std::string message = error.what();
      EXPECT_NE(std::string::npos, message.find("selector-specific render-to-texture subviews"));
      EXPECT_NE(std::string::npos, message.find("object_name: Monitor"));
    }
  }

  TEST(RenderGraphCompiler, RejectsSubviewIntentAtRenderToTextureRecursionLimit) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.setMaxRenderToTextureRecursionDepth(0);

    RenderSubviewIntent subview;
    subview.name = "mirror probe";
    subview.view.selector = SceneSelector::all();
    subview.view.executor = RenderExecutorPreference::Rasterizer;
    intent.subviews.push_back(subview);

    try {
      compiler.compile({64, 32, 1}, intent);
      FAIL() << "Expected render-to-texture recursion limit rejection";
    } catch (const std::runtime_error& error) {
      const std::string message = error.what();
      EXPECT_NE(std::string::npos, message.find("render-to-texture recursion limit 0 reached"));
      EXPECT_NE(std::string::npos, message.find("mirror probe"));
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

  TEST(RenderGraphCompiler, RaytracerOptionsBecomeBeautyPassState) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.engineOptions.raytracer().setIntegrator("pathtracer");
    intent.engineOptions.raytracer().setSampler("Jittered");
    intent.engineOptions.raytracer().setSamplesPerPixel(9);
    intent.engineOptions.raytracer().setSamplingSeed(2468);
    intent.engineOptions.raytracer().setViewPlane("TiledViewPlane");
    intent.engineOptions.raytracer().setMaximumRecursionDepth(6);
    intent.engineOptions.raytracer().setRussianRouletteDepth(4);
    intent.engineOptions.raytracer().setDirectLightSamples(7);

    const RenderPlan plan = compiler.compile({64, 64, intent.targetSampleCountHint()}, intent);

    const auto* pass = plan.findPass("raytrace_beauty");
    ASSERT_NE(nullptr, pass);
    const auto* state = RaytracerBeautyPassState::fromPass(*pass);
    ASSERT_NE(nullptr, state);
    ASSERT_TRUE(state->integrator().has_value());
    ASSERT_TRUE(state->sampler().has_value());
    ASSERT_TRUE(state->samplesPerPixel().has_value());
    ASSERT_TRUE(state->samplingSeed().has_value());
    ASSERT_TRUE(state->viewPlane().has_value());
    ASSERT_TRUE(state->maximumRecursionDepth().has_value());
    ASSERT_TRUE(state->russianRouletteDepth().has_value());
    ASSERT_TRUE(state->directLightSamples().has_value());
    EXPECT_EQ("pathtracer", *state->integrator());
    EXPECT_EQ("Jittered", *state->sampler());
    EXPECT_EQ(9, *state->samplesPerPixel());
    EXPECT_EQ(2468u, *state->samplingSeed());
    EXPECT_EQ("TiledViewPlane", *state->viewPlane());
    EXPECT_EQ(6, *state->maximumRecursionDepth());
    EXPECT_EQ(4, *state->russianRouletteDepth());
    EXPECT_EQ(7, *state->directLightSamples());
    EXPECT_EQ(9, plan.findResource("beauty_color")->sampleCount);
  }

  TEST(RenderGraphCompiler, RasterizerOptionsBecomeBeautyAndShadowPassState) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.enablePreviewShadows = true;
    intent.engineOptions.rasterizer().setBackend(engine::raster::RasterBackend::openGL());
    intent.engineOptions.rasterizer().setLod(3);
    intent.engineOptions.rasterizer().setMSAASamples(4);
    intent.engineOptions.rasterizer().setMSAAShadingMode("per_fragment");
    intent.engineOptions.rasterizer().setShadowMapSize(128);
    intent.engineOptions.rasterizer().setShadowBias(0.05);

    const RenderPlan plan = compiler.compile({64, 64, intent.targetSampleCountHint()}, intent);

    const auto* beauty = plan.findPass("raster_beauty");
    ASSERT_NE(nullptr, beauty);
    const auto* beautyState = RasterBeautyPassState::fromPass(*beauty);
    ASSERT_NE(nullptr, beautyState);
    EXPECT_TRUE(beautyState->execution().backend().isOpenGL());
    EXPECT_EQ(4, beautyState->sampling().msaaSamples());
    EXPECT_EQ(engine::raster::Rasterizer::MSAAShadingMode::PerFragment,
              beautyState->sampling().msaaShadingMode());

    const auto* shadow = plan.findPass("raster_preview_shadows");
    ASSERT_NE(nullptr, shadow);
    const auto* shadowState = RasterShadowPassState::fromPass(*shadow);
    ASSERT_NE(nullptr, shadowState);
    EXPECT_EQ(128, shadowState->shadows().mapSize());
    ASSERT_NE(nullptr, plan.findResource("preview_shadow_map"));
    EXPECT_EQ(128, plan.findResource("preview_shadow_map")->width);
  }

  TEST(RenderGraphCompiler, RasterVisibilityCullingOptionAddsVisibilityDependency) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.engineOptions.rasterizer().setLod(2);
    intent.engineOptions.rasterizer().setVisibilityCulling(RenderVisibilityCulling::On);

    const RenderPlan plan = compiler.compile({64, 64, 1}, intent);

    const auto* visibility = plan.findPass("raster_visibility");
    ASSERT_NE(nullptr, visibility);
    EXPECT_EQ(RenderPassKind::Visibility, visibility->kind);
    EXPECT_EQ(RenderExecutorKind::Rasterizer, visibility->executor);
    EXPECT_EQ(DisabledBehavior::SubstituteDefault, visibility->disabledBehavior);
    EXPECT_TRUE(hasFeature(*visibility, "visibility"));
    EXPECT_TRUE(hasFeature(*visibility, "culling"));
    ASSERT_NE(nullptr, RasterVisibilityPassState::fromPass(*visibility));
    EXPECT_EQ(2, RasterVisibilityPassState::fromPass(*visibility)->geometry().lod());
    EXPECT_TRUE(RasterVisibilityPassState::fromPass(*visibility)->frontToBackOrderingEnabled());
    ASSERT_EQ(1u, visibility->writes.size());
    EXPECT_EQ("raster_visibility_set", visibility->writes.front().resource);

    const auto* visibilityResource = plan.findResource("raster_visibility_set");
    ASSERT_NE(nullptr, visibilityResource);
    EXPECT_EQ(RenderResourceType::VisibilitySet, visibilityResource->type);
    EXPECT_EQ(RenderResourceFormat::Unknown, visibilityResource->format);
    EXPECT_TRUE(visibilityResource->hasFeature("visibility"));
    EXPECT_TRUE(visibilityResource->hasFeature("culling"));
    EXPECT_TRUE(visibilityResource->hasFeature("rasterizer"));
    EXPECT_EQ(64, visibilityResource->width);
    EXPECT_EQ(64, visibilityResource->height);
    EXPECT_EQ(1, visibilityResource->sampleCount);
    EXPECT_EQ(RenderResourceLifetime::PersistentCache, visibilityResource->lifetime);

    const auto* beauty = plan.findPass("raster_beauty");
    ASSERT_NE(nullptr, beauty);
    EXPECT_TRUE(beauty->readsResource("raster_visibility_set"));
    ASSERT_TRUE(plan.executionOrderNumber("raster_visibility").has_value());
    ASSERT_TRUE(plan.executionOrderNumber("raster_beauty").has_value());
    EXPECT_LT(*plan.executionOrderNumber("raster_visibility"),
              *plan.executionOrderNumber("raster_beauty"));
    EXPECT_TRUE(plan.resourceCanReach("raster_visibility_set", "main_color"));
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, RasterVisibilityCullingDisablesOrderingForBlending) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.engineOptions.rasterizer().setVisibilityCulling(RenderVisibilityCulling::On);
    intent.engineOptions.rasterizer().setBlendingEnabled(true);

    const RenderPlan plan = compiler.compile({64, 64, 1}, intent);

    const auto* visibility = plan.findPass("raster_visibility");
    ASSERT_NE(nullptr, visibility);
    const auto* state = RasterVisibilityPassState::fromPass(*visibility);
    ASSERT_NE(nullptr, state);
    EXPECT_FALSE(state->frontToBackOrderingEnabled());
  }

  TEST(RenderGraphCompiler, RasterAOVVisibilityCullingOptionAddsVisibilityDependency) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.defaultViewMode = RenderViewMode::Depth;
    intent.engineOptions.rasterizer().setVisibilityCulling(RenderVisibilityCulling::Auto);

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent);

    const auto* visibility = plan.findPass("raster_visibility");
    ASSERT_NE(nullptr, visibility);
    const auto* depth = plan.findPass("depth_aov");
    ASSERT_NE(nullptr, depth);
    EXPECT_TRUE(depth->readsResource("raster_visibility_set"));
    ASSERT_EQ(1u, plan.consumersOf("raster_visibility_set").size());
    EXPECT_EQ("depth_aov", plan.consumersOf("raster_visibility_set").front()->id);
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, OpenGLRasterMSAADefaultsToPerFragmentShading) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.engineOptions.rasterizer().setBackend(engine::raster::RasterBackend::openGL());
    intent.engineOptions.rasterizer().setMSAASamples(4);

    const RenderPlan plan = compiler.compile({64, 64, intent.targetSampleCountHint()}, intent);

    const auto* beauty = plan.findPass("raster_beauty");
    ASSERT_NE(nullptr, beauty);
    const auto* state = RasterBeautyPassState::fromPass(*beauty);
    ASSERT_NE(nullptr, state);
    EXPECT_EQ(4, state->sampling().msaaSamples());
    EXPECT_EQ(engine::raster::Rasterizer::MSAAShadingMode::PerFragment,
              state->sampling().msaaShadingMode());
    EXPECT_EQ("per_fragment", state->toJson()
                                .value("sampling")
                                .toObject()
                                .value("msaaShadingMode")
                                .toString()
                                .toStdString());
  }

  TEST(RenderGraphCompiler, OpenGLRasterBeautyRoutesThroughExplicitReadbackPass) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.engineOptions.rasterizer().setBackend(engine::raster::RasterBackend::openGL());

    const RenderPlan plan = compiler.compile({64, 64, 1}, intent);

    const auto* readback = plan.findPass("beauty_readback");
    ASSERT_NE(nullptr, readback);
    EXPECT_EQ(RenderPassKind::Readback, readback->kind);
    EXPECT_EQ(RenderExecutorKind::PostProcess, readback->executor);
    ASSERT_EQ(1u, readback->reads.size());
    ASSERT_EQ(1u, readback->writes.size());
    EXPECT_EQ("beauty_color", readback->reads.front().resource);
    EXPECT_EQ("beauty_readback_color", readback->writes.front().resource);
    EXPECT_TRUE(readback->supportsResourceDomain(RenderResourceDomain::CPU));
    EXPECT_TRUE(readback->supportsResourceDomain(RenderResourceDomain::GPU));
    EXPECT_TRUE(hasFeature(*readback, "main"));
    EXPECT_TRUE(hasFeature(*readback, "readback"));
    EXPECT_TRUE(hasFeature(*readback, "transfer"));

    const auto* tonemap = plan.findPass("tonemap");
    ASSERT_NE(nullptr, tonemap);
    ASSERT_EQ(1u, tonemap->reads.size());
    EXPECT_EQ("beauty_readback_color", tonemap->reads.front().resource);
    EXPECT_TRUE(plan.resourceCanReach("beauty_color", "main_color"));
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, OpenGLRasterAOVRoutesThroughExplicitReadbackPass) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.defaultViewMode = RenderViewMode::Depth;
    intent.engineOptions.rasterizer().setBackend(engine::raster::RasterBackend::openGL());

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent);

    const auto* readback = plan.findPass("readback_depth_aov");
    ASSERT_NE(nullptr, readback);
    EXPECT_EQ(RenderPassKind::Readback, readback->kind);
    EXPECT_EQ(RenderExecutorKind::PostProcess, readback->executor);
    ASSERT_EQ(1u, readback->reads.size());
    ASSERT_EQ(1u, readback->writes.size());
    EXPECT_EQ("depth_aov", readback->reads.front().resource);
    EXPECT_EQ("depth_aov_readback", readback->writes.front().resource);
    EXPECT_TRUE(readback->supportsResourceDomain(RenderResourceDomain::CPU));
    EXPECT_TRUE(readback->supportsResourceDomain(RenderResourceDomain::GPU));
    EXPECT_TRUE(hasFeature(*readback, "main"));
    EXPECT_TRUE(hasFeature(*readback, "depth"));
    EXPECT_TRUE(hasFeature(*readback, "readback"));

    const auto* readbackResource = plan.findResource("depth_aov_readback");
    ASSERT_NE(nullptr, readbackResource);
    EXPECT_EQ(RenderResourceType::Depth, readbackResource->type);
    EXPECT_EQ(RenderResourceFormat::DepthDouble, readbackResource->format);
    EXPECT_EQ(RenderResourceLifetime::Transient, readbackResource->lifetime);

    const auto* visualization = plan.findPass("visualize_depth_aov");
    ASSERT_NE(nullptr, visualization);
    ASSERT_EQ(1u, visualization->reads.size());
    EXPECT_EQ("depth_aov_readback", visualization->reads.front().resource);
    EXPECT_TRUE(plan.resourceCanReach("depth_aov", "main_color"));
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, OpenGLRasterExportedAOVRoutesThroughExplicitReadbackPass) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.engineOptions.rasterizer().setBackend(engine::raster::RasterBackend::openGL());
    intent.exportedAOVs = {RenderViewMode::Depth};

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent);

    const auto* source = plan.findResource("depth_aov_source");
    ASSERT_NE(nullptr, source);
    EXPECT_EQ(RenderResourceType::Depth, source->type);
    EXPECT_EQ(RenderResourceLifetime::Transient, source->lifetime);

    const auto* exported = plan.findResource("depth_aov");
    ASSERT_NE(nullptr, exported);
    EXPECT_EQ(RenderResourceType::Depth, exported->type);
    EXPECT_EQ(RenderResourceLifetime::Exported, exported->lifetime);

    const auto* producer = plan.findPass("depth_aov");
    ASSERT_NE(nullptr, producer);
    ASSERT_EQ(1u, producer->writes.size());
    EXPECT_EQ("depth_aov_source", producer->writes.front().resource);

    const auto* readback = plan.findPass("readback_depth_aov");
    ASSERT_NE(nullptr, readback);
    EXPECT_EQ(RenderPassKind::Readback, readback->kind);
    ASSERT_EQ(1u, readback->reads.size());
    ASSERT_EQ(1u, readback->writes.size());
    EXPECT_EQ("depth_aov_source", readback->reads.front().resource);
    EXPECT_EQ("depth_aov", readback->writes.front().resource);
    EXPECT_TRUE(hasFeature(*readback, "export"));
    EXPECT_FALSE(hasFeature(*readback, "main"));

    const auto* visualization = plan.findPass("visualize_depth_aov");
    ASSERT_NE(nullptr, visualization);
    ASSERT_EQ(1u, visualization->reads.size());
    EXPECT_EQ("depth_aov", visualization->reads.front().resource);
    EXPECT_TRUE(plan.resourceCanReach("depth_aov_source", "depth_aov_color"));
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, WireframeOptionsBecomeBeautyAndOverlayPassState) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Wireframe;
    intent.enableWireframeOverlay = true;
    intent.engineOptions.wireframe().setLod(2);

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent);

    const auto* beauty = plan.findPass("wireframe_beauty");
    ASSERT_NE(nullptr, beauty);
    const auto* beautyState = WireframePassState::fromPass(*beauty);
    ASSERT_NE(nullptr, beautyState);
    EXPECT_EQ(2, beautyState->lod());

    const auto* overlay = plan.findPass("wireframe_overlay");
    ASSERT_NE(nullptr, overlay);
    const auto* overlayState = WireframePassState::fromPass(*overlay);
    ASSERT_NE(nullptr, overlayState);
    EXPECT_EQ(2, overlayState->lod());
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
