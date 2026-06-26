#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "engine/graph/PostProcessPassState.h"
#include "engine/graph/RasterPassState.h"
#include "engine/graph/RaytracerPassState.h"
#include "engine/graph/RenderGraphCompiler.h"
#include "engine/graph/RenderSceneAnalysis.h"
#include "engine/graph/WireframePassState.h"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace RenderGraphCompilerTest {
  using namespace engine::graph;
  using ::testing::HasSubstr;

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

  TEST(RenderGraphCompiler, AcceptsResolvableSelectorSpecificOverrides) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    RenderViewOverride override;
    override.selector = SceneSelector::tag("hero");
    override.viewMode = RenderViewMode::Wireframe;
    intent.viewOverrides.push_back(override);

    RenderSceneAnalysis analysis;
    analysis.recordSelectableObject("sphere-1", "Hero Sphere", {"hero"}, {});

    const RenderPlan plan = compiler.compile({64, 64, 1}, intent, analysis);

    ASSERT_NE(nullptr, plan.findPass("raytrace_beauty"));
    ASSERT_NE(nullptr, plan.findPass("selector_1_stencil_aov"));
    ASSERT_NE(nullptr, plan.findPass("selector_1_wireframe_beauty"));
    ASSERT_NE(nullptr, plan.findPass("selector_1_composite"));
    ASSERT_NE(nullptr, plan.findPass("tonemap"));
    EXPECT_EQ(RenderExecutorKind::Wireframe,
              plan.findPass("selector_1_wireframe_beauty")->executor);
    EXPECT_EQ(SceneSelector::Kind::Tag,
              plan.findPass("selector_1_wireframe_beauty")->sceneView.selector.kind);
    EXPECT_EQ("hero", plan.findPass("selector_1_wireframe_beauty")->sceneView.selector.value);
    ASSERT_EQ(3u, plan.findPass("selector_1_composite")->reads.size());
    EXPECT_EQ("beauty_color", plan.findPass("selector_1_composite")->reads[0].resource);
    EXPECT_EQ("selector_1_beauty_color", plan.findPass("selector_1_composite")->reads[1].resource);
    EXPECT_EQ("selector_1_stencil_aov", plan.findPass("selector_1_composite")->reads[2].resource);
    ASSERT_EQ(1u, plan.findPass("tonemap")->reads.size());
    EXPECT_EQ("selector_1_composited_color", plan.findPass("tonemap")->reads.front().resource);
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, RoutesSelectorOverridesThroughRequestedExecutorAndCamera) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.defaultCamera = RenderCameraRef{"main-camera", std::nullopt};

    RenderViewOverride override;
    override.selector = SceneSelector::objectId("diagnostic-box");
    override.executor = RenderExecutorPreference::Wireframe;
    override.viewMode = RenderViewMode::Beauty;
    override.camera = RenderCameraRef{"inspection-camera", std::nullopt};
    intent.viewOverrides.push_back(override);

    RenderSceneAnalysis analysis;
    analysis.recordSelectableObject("diagnostic-box", "Diagnostic Box", {"debug"}, {});

    const RenderPlan plan = compiler.compile({64, 64, 1}, intent, analysis);

    const auto* base = plan.findPass("raster_beauty");
    ASSERT_NE(nullptr, base);
    EXPECT_EQ(RenderExecutorKind::Rasterizer, base->executor);
    ASSERT_TRUE(base->sceneView.camera.has_value());
    EXPECT_EQ("main-camera", *base->sceneView.camera->sceneCameraId);

    const auto* route = plan.findPass("selector_1_wireframe_beauty");
    ASSERT_NE(nullptr, route);
    EXPECT_EQ(RenderExecutorKind::Wireframe, route->executor);
    EXPECT_EQ(SceneSelector::Kind::ObjectId, route->sceneView.selector.kind);
    EXPECT_EQ("diagnostic-box", route->sceneView.selector.value);
    ASSERT_TRUE(route->sceneView.camera.has_value());
    EXPECT_EQ("inspection-camera", *route->sceneView.camera->sceneCameraId);

    const auto* composite = plan.findPass("selector_1_composite");
    ASSERT_NE(nullptr, composite);
    EXPECT_EQ("selector_1_composited_color", composite->writes.front().resource);
    ASSERT_EQ(1u, plan.findPass("tonemap")->reads.size());
    EXPECT_EQ("selector_1_composited_color", plan.findPass("tonemap")->reads.front().resource);
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, CompilesSelectorSpecificAOVOverride) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;

    RenderViewOverride override;
    override.selector = SceneSelector::layer("debug");
    override.viewMode = RenderViewMode::Normal;
    intent.viewOverrides.push_back(override);

    RenderSceneAnalysis analysis;
    analysis.recordSelectableObject("sphere-1", "Sphere", {}, {"debug"});

    const RenderPlan plan = compiler.compile({64, 64, 1}, intent, analysis);

    const auto* normal = plan.findPass("selector_1_normal_aov");
    ASSERT_NE(nullptr, normal);
    EXPECT_EQ(RenderPassKind::AOV, normal->kind);
    EXPECT_EQ(RenderExecutorKind::Rasterizer, normal->executor);
    EXPECT_EQ(SceneSelector::Kind::Layer, normal->sceneView.selector.kind);
    EXPECT_EQ("debug", normal->sceneView.selector.value);
    ASSERT_NE(nullptr, plan.findResource("selector_1_normal_aov"));
    EXPECT_EQ(RenderResourceLifetime::Exported,
              plan.findResource("selector_1_normal_aov")->lifetime);

    ASSERT_NE(nullptr, plan.findPass("selector_1_visualize_normal_aov"));
    ASSERT_NE(nullptr, plan.findResource("selector_1_normal_aov_color"));
    ASSERT_NE(nullptr, plan.findPass("selector_1_composite"));
    EXPECT_EQ("selector_1_normal_aov_color",
              plan.findPass("selector_1_composite")->reads[1].resource);
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, RejectsConflictingSelectorSpecificOverrides) {
    RenderGraphCompiler compiler;
    RenderIntent intent;

    RenderViewOverride first;
    first.selector = SceneSelector::tag("hero");
    first.viewMode = RenderViewMode::Wireframe;
    intent.viewOverrides.push_back(first);

    RenderViewOverride second;
    second.selector = SceneSelector::tag("hero");
    second.viewMode = RenderViewMode::Depth;
    intent.viewOverrides.push_back(second);

    RenderSceneAnalysis analysis;
    analysis.recordSelectableObject("sphere-1", "Hero Sphere", {"hero"}, {});

    try {
      compiler.compile({64, 64, 1}, intent, analysis);
      FAIL() << "Expected conflicting selector diagnostic";
    } catch (const std::runtime_error& e) {
      EXPECT_THAT(e.what(), HasSubstr("conflicting selector-specific render intent"));
      EXPECT_THAT(e.what(), HasSubstr("tag: hero"));
    }
  }

  TEST(RenderGraphCompiler, RejectsMissingSelectorSpecificOverrides) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    RenderViewOverride override;
    override.selector = SceneSelector::tag("missing");
    intent.viewOverrides.push_back(override);

    RenderSceneAnalysis analysis;
    analysis.recordSelectableObject("sphere-1", "Hero Sphere", {"hero"}, {});

    try {
      compiler.compile({64, 64, 1}, intent, analysis);
      FAIL() << "Expected missing selector diagnostic";
    } catch (const std::runtime_error& e) {
      EXPECT_THAT(e.what(), HasSubstr("cannot resolve scene selector tag: missing"));
      EXPECT_THAT(e.what(), HasSubstr("no visible scene subset matches it"));
    }
  }

  TEST(RenderGraphCompiler, RejectsAmbiguousSelectorSpecificOverrides) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    RenderViewOverride override;
    override.selector = SceneSelector::objectName("Duplicate");
    intent.viewOverrides.push_back(override);

    RenderSceneAnalysis analysis;
    analysis.recordSelectableObject("sphere-1", "Duplicate", {}, {});
    analysis.recordSelectableObject("sphere-2", "Duplicate", {}, {});

    try {
      compiler.compile({64, 64, 1}, intent, analysis);
      FAIL() << "Expected ambiguous selector diagnostic";
    } catch (const std::runtime_error& e) {
      EXPECT_THAT(e.what(), HasSubstr("cannot resolve scene selector object_name: Duplicate"));
      EXPECT_THAT(e.what(), HasSubstr("selector is ambiguous"));
    }
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

  TEST(RenderGraphCompiler, TracingExecutionCpuForcesCpuIntersectionBackend) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::PathTracer;
    intent.engineOptions.raytracer().setTracingExecution(TracingExecutionPreference::CPU);

    const RenderPlan plan = compiler.compile({64, 64, 1}, intent);

    const auto* pass = plan.findPass("wavefront_beauty");
    ASSERT_NE(nullptr, pass);
    const auto* state = RaytracerBeautyPassState::fromPass(*pass);
    ASSERT_NE(nullptr, state);
    ASSERT_TRUE(state->predictedTracingExecution().has_value());
    EXPECT_EQ(TracingExecutionPreference::CPU, *state->predictedTracingExecution());
    ASSERT_TRUE(state->intersectionBackend().has_value());
    EXPECT_STREQ("cpu", state->intersectionBackend()->id());
    EXPECT_TRUE(state->tracingExecutionFallbackReason().empty());
  }

  TEST(RenderGraphCompiler, TracingExecutionHybridRequestsGpuIntersectionBackend) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::PathTracer;
    intent.engineOptions.raytracer().setTracingExecution(TracingExecutionPreference::Hybrid);

    const RenderPlan plan = compiler.compile({64, 64, 1}, intent);

    const auto* pass = plan.findPass("wavefront_beauty");
    ASSERT_NE(nullptr, pass);
    const auto* state = RaytracerBeautyPassState::fromPass(*pass);
    ASSERT_NE(nullptr, state);
    ASSERT_TRUE(state->predictedTracingExecution().has_value());
    EXPECT_EQ(TracingExecutionPreference::Hybrid, *state->predictedTracingExecution());
    ASSERT_TRUE(state->intersectionBackend().has_value());
    EXPECT_STREQ("gpu", state->intersectionBackend()->id());
    EXPECT_TRUE(state->tracingExecutionFallbackReason().empty());
  }

  TEST(RenderGraphCompiler, TracingExecutionGpuSupportedPredictsGpuMode) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::PathTracer;
    intent.engineOptions.raytracer().setTracingExecution(TracingExecutionPreference::GPU);

    RenderSceneAnalysis analysis;
    analysis.setFullGpuTracingSupported(true);
    analysis.setFullGpuTracingBackendAvailable(true);

    const RenderPlan plan = compiler.compile({64, 64, 1}, intent, analysis);

    const auto* pass = plan.findPass("wavefront_beauty");
    ASSERT_NE(nullptr, pass);
    const auto* state = RaytracerBeautyPassState::fromPass(*pass);
    ASSERT_NE(nullptr, state);
    ASSERT_TRUE(state->predictedTracingExecution().has_value());
    EXPECT_EQ(TracingExecutionPreference::GPU, *state->predictedTracingExecution());
    ASSERT_TRUE(state->intersectionBackend().has_value());
    EXPECT_STREQ("gpu", state->intersectionBackend()->id());
    EXPECT_TRUE(state->tracingExecutionFallbackReason().empty());
  }

  TEST(RenderGraphCompiler, TracingExecutionAutoPromotesEligibleFullGpuPathTracer) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::PathTracer;

    RenderSceneAnalysis analysis;
    analysis.setFullGpuTracingSupported(true);
    analysis.setFullGpuTracingBackendAvailable(true);

    const RenderPlan plan = compiler.compile({64, 64, 1}, intent, analysis);

    const auto* pass = plan.findPass("wavefront_beauty");
    ASSERT_NE(nullptr, pass);
    const auto* state = RaytracerBeautyPassState::fromPass(*pass);
    ASSERT_NE(nullptr, state);
    ASSERT_TRUE(state->predictedTracingExecution().has_value());
    EXPECT_EQ(TracingExecutionPreference::GPU, *state->predictedTracingExecution());
    ASSERT_TRUE(state->intersectionBackend().has_value());
    EXPECT_STREQ("gpu", state->intersectionBackend()->id());
    EXPECT_TRUE(state->tracingExecutionFallbackReason().empty());
  }

  TEST(RenderGraphCompiler, TracingExecutionAutoKeepsCpuForIncompatiblePathTracerSettings) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::PathTracer;
    intent.engineOptions.raytracer().setDenoiser("box");

    RenderSceneAnalysis analysis;
    analysis.setFullGpuTracingSupported(true);
    analysis.setFullGpuTracingBackendAvailable(true);

    const RenderPlan plan = compiler.compile({64, 64, 1}, intent, analysis);

    const auto* pass = plan.findPass("wavefront_beauty");
    ASSERT_NE(nullptr, pass);
    const auto* state = RaytracerBeautyPassState::fromPass(*pass);
    ASSERT_NE(nullptr, state);
    ASSERT_TRUE(state->predictedTracingExecution().has_value());
    EXPECT_EQ(TracingExecutionPreference::CPU, *state->predictedTracingExecution());
    EXPECT_FALSE(state->intersectionBackend().has_value());
    EXPECT_TRUE(state->tracingExecutionFallbackReason().empty());
  }

  TEST(RenderGraphCompiler, TracingExecutionAutoPreservesExplicitCpuIntersectionBackend) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::PathTracer;
    intent.engineOptions.raytracer().setIntersectionBackend("cpu");

    RenderSceneAnalysis analysis;
    analysis.setFullGpuTracingSupported(true);
    analysis.setFullGpuTracingBackendAvailable(true);

    const RenderPlan plan = compiler.compile({64, 64, 1}, intent, analysis);

    const auto* pass = plan.findPass("wavefront_beauty");
    ASSERT_NE(nullptr, pass);
    const auto* state = RaytracerBeautyPassState::fromPass(*pass);
    ASSERT_NE(nullptr, state);
    ASSERT_TRUE(state->predictedTracingExecution().has_value());
    EXPECT_EQ(TracingExecutionPreference::CPU, *state->predictedTracingExecution());
    ASSERT_TRUE(state->intersectionBackend().has_value());
    EXPECT_STREQ("cpu", state->intersectionBackend()->id());
    EXPECT_TRUE(state->tracingExecutionFallbackReason().empty());
  }

  TEST(RenderGraphCompiler, TracingExecutionAutoPreservesExplicitGpuIntersectionBackendAsHybrid) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::PathTracer;
    intent.engineOptions.raytracer().setIntersectionBackend("gpu");

    RenderSceneAnalysis analysis;
    analysis.setFullGpuTracingSupported(true);
    analysis.setFullGpuTracingBackendAvailable(true);

    const RenderPlan plan = compiler.compile({64, 64, 1}, intent, analysis);

    const auto* pass = plan.findPass("wavefront_beauty");
    ASSERT_NE(nullptr, pass);
    const auto* state = RaytracerBeautyPassState::fromPass(*pass);
    ASSERT_NE(nullptr, state);
    ASSERT_TRUE(state->predictedTracingExecution().has_value());
    EXPECT_EQ(TracingExecutionPreference::Hybrid, *state->predictedTracingExecution());
    ASSERT_TRUE(state->intersectionBackend().has_value());
    EXPECT_STREQ("gpu", state->intersectionBackend()->id());
    EXPECT_TRUE(state->tracingExecutionFallbackReason().empty());
  }

  TEST(RenderGraphCompiler, TracingExecutionGpuFallsBackWhenFullGpuUnavailable) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::PathTracer;
    intent.engineOptions.raytracer().setTracingExecution(TracingExecutionPreference::GPU);

    RenderSceneAnalysis analysis;
    analysis.setFullGpuTracingSupported(false, "transparent material requires CPU shading");
    analysis.setFullGpuTracingBackendAvailable(true);

    const RenderPlan plan = compiler.compile({64, 64, 1}, intent, analysis);

    const auto* pass = plan.findPass("wavefront_beauty");
    ASSERT_NE(nullptr, pass);
    const auto* state = RaytracerBeautyPassState::fromPass(*pass);
    ASSERT_NE(nullptr, state);
    ASSERT_TRUE(state->predictedTracingExecution().has_value());
    EXPECT_EQ(TracingExecutionPreference::Hybrid, *state->predictedTracingExecution());
    ASSERT_TRUE(state->intersectionBackend().has_value());
    EXPECT_STREQ("gpu", state->intersectionBackend()->id());
    EXPECT_EQ("transparent material requires CPU shading", state->tracingExecutionFallbackReason());
  }

  TEST(RenderGraphCompiler, RejectsCpuTracingWithGpuIntersectionBackend) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::PathTracer;
    intent.engineOptions.raytracer().setTracingExecution(TracingExecutionPreference::CPU);
    intent.engineOptions.raytracer().setIntersectionBackend("gpu");

    EXPECT_THROW(compiler.compile({64, 64, 1}, intent), std::runtime_error);
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

  TEST(RenderGraphCompiler, HybridVisibilityViewModeCompilesServiceAOVPlan) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Wavefront;
    intent.defaultViewMode = RenderViewMode::HybridVisibility;
    intent.engineOptions.raytracer().setIntersectionBackend("cpu");

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent);

    ASSERT_EQ(2u, plan.resources().size());
    ASSERT_NE(nullptr, plan.findResource("hybrid_visibility_aov"));
    EXPECT_EQ(RenderResourceType::Color, plan.findResource("hybrid_visibility_aov")->type);
    EXPECT_EQ(RenderResourceFormat::RGBDouble, plan.findResource("hybrid_visibility_aov")->format);
    EXPECT_EQ(RenderResourceLifetime::Transient,
              plan.findResource("hybrid_visibility_aov")->lifetime);
    ASSERT_NE(nullptr, plan.findResource("main_color"));
    EXPECT_EQ(RenderResourceType::Color, plan.findResource("main_color")->type);
    EXPECT_EQ(RenderResourceLifetime::Exported, plan.findResource("main_color")->lifetime);

    ASSERT_EQ(2u, plan.passes().size());
    EXPECT_EQ("hybrid_visibility_aov", plan.passes()[0].id);
    EXPECT_EQ(RenderPassKind::AOV, plan.passes()[0].kind);
    EXPECT_EQ(RenderExecutorKind::Wavefront, plan.passes()[0].executor);
    EXPECT_TRUE(hasFeature(plan.passes()[0], "hybrid_visibility"));
    ASSERT_NE(nullptr, RaytracerBeautyPassState::fromPass(plan.passes()[0]));
    ASSERT_EQ(1u, plan.passes()[0].writes.size());
    EXPECT_EQ("hybrid_visibility_aov", plan.passes()[0].writes[0].resource);

    EXPECT_EQ("visualize_hybrid_visibility_aov", plan.passes()[1].id);
    EXPECT_EQ(RenderExecutorKind::PostProcess, plan.passes()[1].executor);
    EXPECT_TRUE(hasFeature(plan.passes()[1], "visualization"));
    ASSERT_EQ(1u, plan.passes()[1].reads.size());
    ASSERT_EQ(1u, plan.passes()[1].writes.size());
    EXPECT_EQ("hybrid_visibility_aov", plan.passes()[1].reads[0].resource);
    EXPECT_EQ("main_color", plan.passes()[1].writes[0].resource);
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

  TEST(RenderGraphCompiler, SampleStddevColorViewModeCompilesWavefrontAOVPlan) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::PathTracer;
    intent.defaultViewMode = RenderViewMode::SampleStddevColor;
    intent.engineOptions.raytracer().setIntegrator("pathtracer");
    intent.engineOptions.raytracer().setSamplesPerPixel(4);

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent);

    ASSERT_EQ(2u, plan.resources().size());
    ASSERT_NE(nullptr, plan.findResource("sample_stddev_color_aov"));
    EXPECT_EQ(RenderResourceType::Color, plan.findResource("sample_stddev_color_aov")->type);
    EXPECT_EQ(RenderResourceFormat::RGBDouble,
              plan.findResource("sample_stddev_color_aov")->format);
    EXPECT_EQ(RenderResourceLifetime::Transient,
              plan.findResource("sample_stddev_color_aov")->lifetime);
    ASSERT_NE(nullptr, plan.findResource("main_color"));

    ASSERT_EQ(2u, plan.passes().size());
    EXPECT_EQ("sample_stddev_color_aov", plan.passes()[0].id);
    EXPECT_EQ(RenderPassKind::AOV, plan.passes()[0].kind);
    EXPECT_EQ(RenderExecutorKind::Wavefront, plan.passes()[0].executor);
    EXPECT_TRUE(hasFeature(plan.passes()[0], "sample_stddev_color"));
    EXPECT_TRUE(hasFeature(plan.passes()[0], "wavefront"));
    const auto* state = RaytracerBeautyPassState::fromPass(plan.passes()[0]);
    ASSERT_NE(nullptr, state);
    ASSERT_TRUE(state->integrator().has_value());
    EXPECT_EQ("pathtracer", *state->integrator());
    ASSERT_TRUE(state->samplesPerPixel().has_value());
    EXPECT_EQ(4, *state->samplesPerPixel());
    ASSERT_EQ(1u, plan.passes()[0].writes.size());
    EXPECT_EQ("sample_stddev_color_aov", plan.passes()[0].writes[0].resource);

    EXPECT_EQ("visualize_sample_stddev_color_aov", plan.passes()[1].id);
    EXPECT_EQ(RenderPassKind::AOV, plan.passes()[1].kind);
    EXPECT_EQ(RenderExecutorKind::PostProcess, plan.passes()[1].executor);
    EXPECT_TRUE(hasFeature(plan.passes()[1], "visualization"));
    ASSERT_EQ(1u, plan.passes()[1].reads.size());
    ASSERT_EQ(1u, plan.passes()[1].writes.size());
    EXPECT_EQ("sample_stddev_color_aov", plan.passes()[1].reads[0].resource);
    EXPECT_EQ("main_color", plan.passes()[1].writes[0].resource);
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, SampleStddevColorAOVRejectsNonWavefrontExecutor) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Raytracer;
    intent.defaultViewMode = RenderViewMode::SampleStddevColor;

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

  TEST(RenderGraphCompiler, PreservesUnknownSceneFallbackForSelectorSpecificOverrides) {
    RenderGraphCompiler compiler;
    RenderIntent intent;

    RenderViewOverride override;
    override.selector = SceneSelector::objectName("Monitor");
    override.executor = RenderExecutorPreference::Wireframe;
    intent.viewOverrides.push_back(override);

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent);

    EXPECT_TRUE(plan.validate().valid());
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

  TEST(RenderGraphCompiler, ConnectsSubviewOutputsToRenderTextureReceiverPasses) {
    RenderGraphCompiler compiler;
    RenderIntent intent;

    RenderSubviewIntent subview;
    subview.name = "monitor-feed";
    subview.view.selector = SceneSelector::all();
    subview.view.executor = RenderExecutorPreference::Rasterizer;
    subview.view.camera = RenderCameraRef{"monitor-camera", std::nullopt};
    intent.subviews.push_back(subview);

    RenderSceneAnalysis analysis;
    analysis.recordRenderTextureReceiver("monitor-feed");

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent, analysis);

    const auto* receiver = plan.findPass("raytrace_beauty");
    ASSERT_NE(nullptr, receiver);
    EXPECT_TRUE(receiver->readsResource("subview_monitor_feed_main_color"));
    EXPECT_TRUE(receiver->readsResource("subview_monitor_feed_depth_aov"));

    const auto dependencies = plan.dependenciesInto("raytrace_beauty");
    EXPECT_NE(dependencies.end(),
              std::find_if(dependencies.begin(), dependencies.end(), [](const auto& dependency) {
                return dependency.resource == "subview_monitor_feed_main_color" &&
                       dependency.producer->id == "subview_monitor_feed_tonemap";
              }));
    EXPECT_NE(dependencies.end(),
              std::find_if(dependencies.begin(), dependencies.end(), [](const auto& dependency) {
                return dependency.resource == "subview_monitor_feed_depth_aov" &&
                       dependency.producer->id == "subview_monitor_feed_depth_aov";
              }));
    ASSERT_TRUE(plan.executionOrderNumber("subview_monitor_feed_tonemap").has_value());
    ASSERT_TRUE(plan.executionOrderNumber("raytrace_beauty").has_value());
    EXPECT_LT(*plan.executionOrderNumber("subview_monitor_feed_tonemap"),
              *plan.executionOrderNumber("raytrace_beauty"));
    EXPECT_TRUE(plan.resourceCanReach("subview_monitor_feed_main_color", "main_color"));
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, CompilesPortalAndMirrorMarkersAsDerivedCameraSubviews) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.setDefaultCamera(RenderCameraRef{"active-camera", std::nullopt});

    RenderSceneAnalysis analysis;
    analysis.recordPortalReceiverSurface("portal-panel", "Portal Panel",
                                         Matrix4d::translate(2.0, 0.0, 0.0),
                                         Matrix4d::translate(12.0, 0.0, 0.0));
    analysis.recordPlanarMirrorSurface("mirror-panel", "Mirror Panel", Vector3d(0.0, 0.0, 0.0),
                                       Vector3d(0.0, 1.0, 0.0));

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent, analysis);

    const auto* portalPass = plan.findPass("subview_portal_portal_panel_raytrace_beauty");
    ASSERT_NE(nullptr, portalPass);
    ASSERT_TRUE(portalPass->sceneView.camera.has_value());
    ASSERT_TRUE(portalPass->sceneView.camera->derived.has_value());
    EXPECT_EQ(DerivedCameraRef::Kind::Portal, portalPass->sceneView.camera->derived->kind);
    ASSERT_TRUE(portalPass->sceneView.camera->derived->baseSceneCameraId.has_value());
    EXPECT_EQ("active-camera", *portalPass->sceneView.camera->derived->baseSceneCameraId);
    EXPECT_TRUE(portalPass->sceneView.camera->derived->requiresReceiverClip);
    EXPECT_TRUE(hasFeature(*portalPass, "subview:subview_portal_portal_panel"));
    EXPECT_TRUE(portalPass->readsResource("subview_portal_portal_panel_receiver_mask"));

    const auto* portalMask = plan.findPass("subview_portal_portal_panel_receiver_mask");
    ASSERT_NE(nullptr, portalMask);
    EXPECT_EQ(RenderPassKind::AOV, portalMask->kind);
    EXPECT_EQ(RenderExecutorKind::Rasterizer, portalMask->executor);
    EXPECT_TRUE(hasFeature(*portalMask, "receiver_mask"));
    EXPECT_TRUE(hasFeature(*portalMask, "portal_receiver"));
    EXPECT_EQ(SceneSelector::Kind::ObjectId, portalMask->sceneView.selector.kind);
    EXPECT_EQ("portal-panel", portalMask->sceneView.selector.value);
    ASSERT_EQ(1u, portalMask->writes.size());
    EXPECT_EQ("subview_portal_portal_panel_receiver_mask", portalMask->writes.front().resource);

    const auto* portalMaskResource = plan.findResource("subview_portal_portal_panel_receiver_mask");
    ASSERT_NE(nullptr, portalMaskResource);
    EXPECT_EQ(RenderResourceType::Stencil, portalMaskResource->type);
    EXPECT_EQ(RenderResourceFormat::UInt8, portalMaskResource->format);
    EXPECT_EQ(1, portalMaskResource->sampleCount);
    EXPECT_TRUE(portalMaskResource->hasFeature("receiver_mask"));
    EXPECT_TRUE(portalMaskResource->hasFeature("portal_receiver"));

    const auto* portalComposite = plan.findPass("subview_portal_portal_panel_composite");
    ASSERT_NE(nullptr, portalComposite);
    EXPECT_EQ(RenderPassKind::Composite, portalComposite->kind);
    EXPECT_EQ(RenderExecutorKind::Composite, portalComposite->executor);
    EXPECT_EQ(DisabledBehavior::Passthrough, portalComposite->disabledBehavior);
    EXPECT_TRUE(hasFeature(*portalComposite, "subview_composite"));
    EXPECT_TRUE(hasFeature(*portalComposite, "stencil_composite"));
    EXPECT_TRUE(hasFeature(*portalComposite, "portal_receiver"));
    ASSERT_EQ(3u, portalComposite->reads.size());
    EXPECT_EQ("beauty_color", portalComposite->reads[0].resource);
    EXPECT_EQ("subview_portal_portal_panel_main_color", portalComposite->reads[1].resource);
    EXPECT_EQ("subview_portal_portal_panel_receiver_mask", portalComposite->reads[2].resource);
    ASSERT_EQ(1u, portalComposite->writes.size());
    EXPECT_EQ("subview_portal_portal_panel_composited_color",
              portalComposite->writes.front().resource);
    ASSERT_NE(nullptr, plan.findResource("subview_portal_portal_panel_composited_color"));
    EXPECT_TRUE(plan.findResource("subview_portal_portal_panel_composited_color")
                  ->hasFeature("portal_receiver"));

    const auto* mirrorPass = plan.findPass("subview_mirror_mirror_panel_raytrace_beauty");
    ASSERT_NE(nullptr, mirrorPass);
    ASSERT_TRUE(mirrorPass->sceneView.camera.has_value());
    ASSERT_TRUE(mirrorPass->sceneView.camera->derived.has_value());
    EXPECT_EQ(DerivedCameraRef::Kind::PlanarMirror, mirrorPass->sceneView.camera->derived->kind);
    EXPECT_TRUE(mirrorPass->sceneView.camera->derived->requiresReceiverClip);
    EXPECT_TRUE(hasFeature(*mirrorPass, "subview:subview_mirror_mirror_panel"));
    EXPECT_TRUE(mirrorPass->readsResource("subview_mirror_mirror_panel_receiver_mask"));

    const auto* mirrorMask = plan.findPass("subview_mirror_mirror_panel_receiver_mask");
    ASSERT_NE(nullptr, mirrorMask);
    EXPECT_TRUE(hasFeature(*mirrorMask, "receiver_mask"));
    EXPECT_TRUE(hasFeature(*mirrorMask, "mirror_receiver"));
    EXPECT_EQ(SceneSelector::Kind::ObjectId, mirrorMask->sceneView.selector.kind);
    EXPECT_EQ("mirror-panel", mirrorMask->sceneView.selector.value);
    ASSERT_EQ(1u, mirrorMask->writes.size());
    EXPECT_EQ("subview_mirror_mirror_panel_receiver_mask", mirrorMask->writes.front().resource);

    const auto* mirrorComposite = plan.findPass("subview_mirror_mirror_panel_composite");
    ASSERT_NE(nullptr, mirrorComposite);
    EXPECT_TRUE(hasFeature(*mirrorComposite, "subview_composite"));
    EXPECT_TRUE(hasFeature(*mirrorComposite, "stencil_composite"));
    EXPECT_TRUE(hasFeature(*mirrorComposite, "mirror_receiver"));
    ASSERT_EQ(3u, mirrorComposite->reads.size());
    EXPECT_EQ("subview_portal_portal_panel_composited_color", mirrorComposite->reads[0].resource);
    EXPECT_EQ("subview_mirror_mirror_panel_main_color", mirrorComposite->reads[1].resource);
    EXPECT_EQ("subview_mirror_mirror_panel_receiver_mask", mirrorComposite->reads[2].resource);
    ASSERT_EQ(1u, mirrorComposite->writes.size());
    EXPECT_EQ("subview_mirror_mirror_panel_composited_color",
              mirrorComposite->writes.front().resource);

    const auto* tonemap = plan.findPass("tonemap");
    ASSERT_NE(nullptr, tonemap);
    ASSERT_EQ(1u, tonemap->reads.size());
    EXPECT_EQ("subview_mirror_mirror_panel_composited_color", tonemap->reads.front().resource);

    ASSERT_TRUE(plan.executionOrderNumber("subview_portal_portal_panel_receiver_mask").has_value());
    ASSERT_TRUE(
      plan.executionOrderNumber("subview_portal_portal_panel_raytrace_beauty").has_value());
    EXPECT_LT(*plan.executionOrderNumber("subview_portal_portal_panel_receiver_mask"),
              *plan.executionOrderNumber("subview_portal_portal_panel_raytrace_beauty"));
    EXPECT_TRUE(plan.resourceCanReach("subview_portal_portal_panel_receiver_mask",
                                      "subview_portal_portal_panel_main_color"));
    EXPECT_TRUE(plan.resourceCanReach("subview_portal_portal_panel_main_color", "main_color"));
    EXPECT_TRUE(plan.resourceCanReach("subview_mirror_mirror_panel_main_color", "main_color"));
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, AutomaticRasterSubviewCompositeUsesDepthWhenAvailable) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.requestExportedAOV(RenderViewMode::Depth);

    RenderSceneAnalysis analysis;
    analysis.recordPlanarMirrorSurface("mirror-panel", "Mirror Panel", Vector3d(0.0, 0.0, 0.0),
                                       Vector3d(0.0, 1.0, 0.0));

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent, analysis);

    const auto* composite = plan.findPass("subview_mirror_mirror_panel_composite");
    ASSERT_NE(nullptr, composite);
    EXPECT_TRUE(hasFeature(*composite, "stencil_composite"));
    EXPECT_TRUE(hasFeature(*composite, "depth_composite"));
    ASSERT_EQ(5u, composite->reads.size());
    EXPECT_EQ("beauty_color", composite->reads[0].resource);
    EXPECT_EQ("subview_mirror_mirror_panel_main_color", composite->reads[1].resource);
    EXPECT_EQ("depth_aov", composite->reads[2].resource);
    EXPECT_EQ("subview_mirror_mirror_panel_depth_aov", composite->reads[3].resource);
    EXPECT_EQ("subview_mirror_mirror_panel_receiver_mask", composite->reads[4].resource);
    EXPECT_TRUE(plan.resourceCanReach("depth_aov", "subview_mirror_mirror_panel_composited_color"));
    EXPECT_TRUE(plan.resourceCanReach("subview_mirror_mirror_panel_depth_aov",
                                      "subview_mirror_mirror_panel_composited_color"));
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, SkipsAutomaticSubviewMasksForOffscreenReceivers) {
    RenderGraphCompiler compiler;
    RenderIntent intent;

    RenderSceneAnalysis analysis;
    analysis.recordPortalReceiverSurface("portal-panel", "Portal Panel", Matrix4d(), Matrix4d(),
                                         false);
    analysis.recordPlanarMirrorSurface("mirror-panel", "Mirror Panel", Vector3d::null,
                                       Vector3d(0.0, 1.0, 0.0), false);

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent, analysis);

    EXPECT_EQ(nullptr, plan.findPass("subview_portal_portal_panel_raytrace_beauty"));
    EXPECT_EQ(nullptr, plan.findPass("subview_portal_portal_panel_receiver_mask"));
    EXPECT_EQ(nullptr, plan.findPass("subview_mirror_mirror_panel_raytrace_beauty"));
    EXPECT_EQ(nullptr, plan.findPass("subview_mirror_mirror_panel_receiver_mask"));
    EXPECT_TRUE(plan.passesWithFeature("receiver_mask").empty());
    EXPECT_TRUE(plan.resourcesWithFeature("receiver_mask").empty());
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, ReceiverMaskUsesConservativeSelectorForUnsupportedRasterState) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.engineOptions.rasterizer().setBlendingEnabled(true);

    RenderSceneAnalysis analysis;
    analysis.recordPortalReceiverSurface("portal-panel", "Portal Panel", Matrix4d(), Matrix4d());

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent, analysis);

    const auto* mask = plan.findPass("subview_portal_portal_panel_receiver_mask");
    ASSERT_NE(nullptr, mask);
    EXPECT_TRUE(hasFeature(*mask, "receiver_mask"));
    EXPECT_TRUE(hasFeature(*mask, "conservative_receiver_mask"));
    EXPECT_EQ(SceneSelector::Kind::All, mask->sceneView.selector.kind);

    const auto* maskState = RasterBeautyPassState::fromPass(*mask);
    ASSERT_NE(nullptr, maskState);
    EXPECT_TRUE(maskState->toJson().value("framebuffer").toObject().value("blending").toBool());
    EXPECT_EQ("none", maskState->toJson()
                        .value("framebuffer")
                        .toObject()
                        .value("colorWriteMask")
                        .toString()
                        .toStdString());
    EXPECT_TRUE(plan.findResource("subview_portal_portal_panel_receiver_mask")
                  ->hasFeature("conservative_receiver_mask"));
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

  TEST(RenderGraphCompiler, TruncatesSubviewIntentAtRenderToTextureRecursionLimit) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.setMaxRenderToTextureRecursionDepth(0);

    RenderSubviewIntent subview;
    subview.name = "mirror probe";
    subview.view.selector = SceneSelector::all();
    subview.view.executor = RenderExecutorPreference::Rasterizer;
    intent.subviews.push_back(subview);

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent);

    EXPECT_EQ(nullptr, plan.findPass("subview_mirror_probe_raster_beauty"));
    const auto* diagnostic = plan.findPass("subview_mirror_probe_recursion_limit");
    ASSERT_NE(nullptr, diagnostic);
    EXPECT_EQ("Subview mirror probe truncated at render-to-texture recursion limit 0",
              diagnostic->name);
    EXPECT_EQ(RenderPassKind::Debug, diagnostic->kind);
    EXPECT_EQ(RenderExecutorKind::PostProcess, diagnostic->executor);
    EXPECT_FALSE(diagnostic->enabled);
    EXPECT_EQ(DisabledBehavior::SubstituteDefault, diagnostic->disabledBehavior);
    EXPECT_TRUE(hasFeature(*diagnostic, "render_to_texture_recursion_limit"));
    EXPECT_TRUE(hasFeature(*diagnostic, "truncated"));
    EXPECT_TRUE(plan.toText().find("subview_mirror_probe_recursion_limit") != std::string::npos);
    EXPECT_TRUE(plan.toDot().find("recursion limit 0") != std::string::npos);
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, TruncatesSelfRecursivePortalAtConfiguredDepth) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.setMaxRenderToTextureRecursionDepth(2);

    RenderSceneAnalysis analysis;
    analysis.recordPortalReceiverSurface("portal-panel", "Portal Panel", Matrix4d(), Matrix4d());

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent, analysis);

    ASSERT_NE(nullptr, plan.findPass("subview_portal_portal_panel_raytrace_beauty"));
    ASSERT_NE(
      nullptr,
      plan.findPass("subview_portal_portal_panel_subview_portal_portal_panel_raytrace_beauty"));
    EXPECT_EQ(nullptr, plan.findPass("subview_portal_portal_panel_subview_portal_portal_panel_"
                                     "subview_portal_portal_panel_raytrace_beauty"));

    const auto* diagnostic = plan.findPass(
      "subview_portal_portal_panel_subview_portal_portal_panel_subview_portal_portal_panel_"
      "recursion_limit");
    ASSERT_NE(nullptr, diagnostic);
    EXPECT_EQ("Subview portal Portal Panel Subview portal Portal Panel Subview portal Portal Panel "
              "truncated at render-to-texture recursion limit 2",
              diagnostic->name);
    EXPECT_TRUE(hasFeature(*diagnostic, "render_to_texture_recursion_limit"));
    EXPECT_TRUE(hasFeature(*diagnostic, "subview:subview_portal_portal_panel"));
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, TruncatesMutualPortalMirrorRecursionDeterministically) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.setMaxRenderToTextureRecursionDepth(1);

    RenderSceneAnalysis analysis;
    analysis.recordPortalReceiverSurface("portal-panel", "Portal Panel", Matrix4d(), Matrix4d());
    analysis.recordPlanarMirrorSurface("mirror-panel", "Mirror Panel", Vector3d::null,
                                       Vector3d(0.0, 1.0, 0.0));

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent, analysis);

    ASSERT_NE(nullptr, plan.findPass("subview_portal_portal_panel_raytrace_beauty"));
    ASSERT_NE(nullptr, plan.findPass("subview_mirror_mirror_panel_raytrace_beauty"));

    const auto diagnostics = plan.passesWithFeature("render_to_texture_recursion_limit");
    ASSERT_EQ(4u, diagnostics.size());
    EXPECT_EQ("subview_portal_portal_panel_subview_portal_portal_panel_recursion_limit",
              diagnostics[0]->id);
    EXPECT_EQ("subview_portal_portal_panel_subview_mirror_mirror_panel_recursion_limit",
              diagnostics[1]->id);
    EXPECT_EQ("subview_mirror_mirror_panel_subview_portal_portal_panel_recursion_limit",
              diagnostics[2]->id);
    EXPECT_EQ("subview_mirror_mirror_panel_subview_mirror_mirror_panel_recursion_limit",
              diagnostics[3]->id);
    for (const auto* diagnostic : diagnostics) {
      EXPECT_FALSE(diagnostic->enabled);
      EXPECT_EQ(RenderPassKind::Debug, diagnostic->kind);
      EXPECT_TRUE(hasFeature(*diagnostic, "truncated"));
    }
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderGraphCompiler, RejectsUnknownRenderTextureReceiverSubview) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    RenderSceneAnalysis analysis;
    analysis.recordRenderTextureReceiver("missing-feed");

    try {
      compiler.compile({64, 32, 1}, intent, analysis);
      FAIL() << "Expected unknown render-to-texture receiver rejection";
    } catch (const std::runtime_error& error) {
      const std::string message = error.what();
      EXPECT_NE(std::string::npos, message.find("unknown subview 'missing-feed'"));
    }
  }

  TEST(RenderGraphCompiler, RejectsDuplicateRenderTextureReceiverSubviewNames) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    RenderSubviewIntent first;
    first.name = "monitor-feed";
    intent.subviews.push_back(first);
    RenderSubviewIntent second;
    second.name = "monitor-feed";
    intent.subviews.push_back(second);
    RenderSceneAnalysis analysis;
    analysis.recordRenderTextureReceiver("monitor-feed");

    try {
      compiler.compile({64, 32, 1}, intent, analysis);
      FAIL() << "Expected duplicate subview name rejection";
    } catch (const std::runtime_error& error) {
      const std::string message = error.what();
      EXPECT_NE(std::string::npos, message.find("subview name 'monitor-feed' is not unique"));
    }
  }

  TEST(RenderGraphCompiler, WholeSceneRenderTextureReceiversCompileAsBoundedDependencies) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    RenderSubviewIntent subview;
    subview.name = "monitor-feed";
    subview.view.selector = SceneSelector::all();
    subview.view.executor = RenderExecutorPreference::Rasterizer;
    intent.subviews.push_back(subview);
    RenderSceneAnalysis analysis;
    analysis.recordRenderTextureReceiver("monitor-feed");

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent, analysis);

    const auto* receiver = plan.findPass("raytrace_beauty");
    ASSERT_NE(nullptr, receiver);
    EXPECT_TRUE(receiver->readsResource("subview_monitor_feed_main_color"));
    EXPECT_TRUE(plan.validate().valid());
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

  TEST(RenderGraphCompiler, RayTracedPreviewShadowsAddHybridShadowMaskAndComposite) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.enablePreviewShadows = true;
    intent.engineOptions.rasterizer().setShadowMode(RenderRasterShadowMode::RayTraced);
    intent.engineOptions.raytracer().setIntersectionBackend("cpu");

    const RenderPlan plan = compiler.compile({64, 32, 1}, intent);

    ASSERT_NE(nullptr, plan.findPass("hybrid_ray_traced_shadows"));
    EXPECT_EQ(RenderPassKind::Shadow, plan.findPass("hybrid_ray_traced_shadows")->kind);
    EXPECT_EQ(RenderExecutorKind::Raytracer, plan.findPass("hybrid_ray_traced_shadows")->executor);
    EXPECT_TRUE(hasFeature(*plan.findPass("hybrid_ray_traced_shadows"), "ray_traced_shadows"));

    ASSERT_NE(nullptr, plan.findResource("hybrid_shadow_mask"));
    const auto* mask = plan.findResource("hybrid_shadow_mask");
    EXPECT_EQ(RenderResourceType::ShadowMask, mask->type);
    EXPECT_EQ(RenderResourceFormat::RGBDouble, mask->format);
    EXPECT_EQ(64, mask->width);
    EXPECT_EQ(32, mask->height);

    ASSERT_NE(nullptr, plan.findPass("hybrid_shadow_composite"));
    const auto* composite = plan.findPass("hybrid_shadow_composite");
    EXPECT_EQ(RenderPassKind::Composite, composite->kind);
    EXPECT_EQ(RenderExecutorKind::Composite, composite->executor);
    ASSERT_EQ(2u, composite->reads.size());
    EXPECT_EQ("beauty_color", composite->reads[0].resource);
    EXPECT_EQ("hybrid_shadow_mask", composite->reads[1].resource);
    ASSERT_EQ(1u, composite->writes.size());
    EXPECT_EQ("hybrid_shadowed_color", composite->writes[0].resource);

    EXPECT_EQ(nullptr, plan.findPass("raster_preview_shadows"));
    EXPECT_EQ(nullptr, plan.findResource("preview_shadow_map"));
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
