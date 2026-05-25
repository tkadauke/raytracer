#include <gtest/gtest.h>

#include "engine/graph/PostProcessPassState.h"
#include "engine/graph/RasterPassState.h"
#include "engine/graph/RenderPlan.h"

#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>

namespace RenderPlanTest {
  using namespace engine::graph;
  using Rasterizer = engine::raster::Rasterizer;

  RenderResourceDescriptor
  colorResource(const std::string& id,
                RenderResourceLifetime lifetime = RenderResourceLifetime::Transient) {
    RenderResourceDescriptor resource;
    resource.id = id;
    resource.name = id;
    resource.type = RenderResourceType::Color;
    resource.format = RenderResourceFormat::RGBDouble;
    resource.width = 640;
    resource.height = 360;
    resource.sampleCount = 1;
    resource.lifetime = lifetime;
    return resource;
  }

  RenderPassNode pass(const std::string& id, RenderPassKind kind = RenderPassKind::Beauty) {
    RenderPassNode node;
    node.id = id;
    node.name = id;
    node.kind = kind;
    node.executor = kind == RenderPassKind::Tonemap ? RenderExecutorKind::PostProcess
                                                    : RenderExecutorKind::Rasterizer;
    return node;
  }

  RenderPlan executionEquivalencePlan(
    int width = 640, bool postEnabled = true,
    std::shared_ptr<const RenderPassState> state = std::make_shared<FxaaPostProcessAAState>(),
    const std::string& nameSuffix = "") {
    RenderPlan plan;
    auto beautyColor = colorResource("beauty_color");
    beautyColor.width = width;
    beautyColor.name += nameSuffix;
    plan.addResource(beautyColor);
    auto mainColor = colorResource("main_color", RenderResourceLifetime::Exported);
    mainColor.width = width;
    mainColor.name += nameSuffix;
    plan.addResource(mainColor);

    auto beauty = pass("beauty", RenderPassKind::Beauty);
    beauty.name += nameSuffix;
    beauty.writes.push_back({"beauty_color"});
    plan.addPass(beauty);

    auto post = pass("post_fxaa", RenderPassKind::PostProcess);
    post.name += nameSuffix;
    post.features = {"post_aa", "fxaa"};
    post.reads.push_back({"beauty_color"});
    post.writes.push_back({"main_color"});
    post.state = std::move(state);
    post.enabled = postEnabled;
    post.disabledBehavior = DisabledBehavior::Passthrough;
    plan.addPass(post);
    return plan;
  }

  bool hasError(const RenderPlanValidation& validation, RenderPlanValidationError::Code code) {
    const auto& errors = validation.errors();
    return std::any_of(
      errors.begin(), errors.end(),
      [code](const RenderPlanValidationError& error) { return error.code == code; });
  }

  TEST(RenderPlan, ValidatesSimpleProducerConsumerPlan) {
    RenderPlan plan;
    plan.addResource(colorResource("main_color"));
    plan.addResource(colorResource("display_color", RenderResourceLifetime::Exported));

    auto main = pass("main");
    main.writes.push_back({"main_color"});
    plan.addPass(main);

    auto tonemap = pass("tonemap", RenderPassKind::Tonemap);
    tonemap.reads.push_back({"main_color"});
    tonemap.writes.push_back({"display_color"});
    plan.addPass(tonemap);

    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderPlan, UpdatesResourceDescriptorById) {
    RenderPlan plan;
    plan.addResource(colorResource("main_color"));

    auto updated = colorResource("main_color", RenderResourceLifetime::PersistentCache);
    updated.width = 128;
    updated.height = 64;

    EXPECT_EQ(1u, plan.setResourceDescriptor(updated));
    ASSERT_NE(nullptr, plan.findResource("main_color"));
    EXPECT_EQ(128, plan.findResource("main_color")->width);
    EXPECT_EQ(64, plan.findResource("main_color")->height);
    EXPECT_EQ(RenderResourceLifetime::PersistentCache, plan.findResource("main_color")->lifetime);
    EXPECT_EQ(0u, plan.setResourceDescriptor(colorResource("missing")));
  }

  TEST(RenderPlan, ReportsMissingProducerForTransientRead) {
    RenderPlan plan;
    plan.addResource(colorResource("main_color"));

    auto tonemap = pass("tonemap", RenderPassKind::Tonemap);
    tonemap.reads.push_back({"main_color"});
    plan.addPass(tonemap);

    const auto validation = plan.validate();

    ASSERT_FALSE(validation.valid());
    EXPECT_TRUE(hasError(validation, RenderPlanValidationError::Code::MissingProducer));
  }

  TEST(RenderPlan, AllowsImportedResourcesWithoutProducer) {
    RenderPlan plan;
    plan.addResource(colorResource("history_color", RenderResourceLifetime::Imported));
    plan.addResource(colorResource("main_color"));

    auto temporal = pass("temporal", RenderPassKind::PostProcess);
    temporal.reads.push_back({"history_color"});
    temporal.writes.push_back({"main_color"});
    plan.addPass(temporal);

    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderPlan, ReportsUnproducedExportedResource) {
    RenderPlan plan;
    plan.addResource(colorResource("main_color", RenderResourceLifetime::Exported));

    const auto validation = plan.validate();

    ASSERT_FALSE(validation.valid());
    EXPECT_TRUE(hasError(validation, RenderPlanValidationError::Code::UnproducedExport));
  }

  TEST(RenderPlan, ReportsDuplicateWriters) {
    RenderPlan plan;
    plan.addResource(colorResource("main_color"));

    auto first = pass("first");
    first.writes.push_back({"main_color"});
    plan.addPass(first);

    auto second = pass("second");
    second.writes.push_back({"main_color"});
    plan.addPass(second);

    const auto validation = plan.validate();

    ASSERT_FALSE(validation.valid());
    EXPECT_TRUE(hasError(validation, RenderPlanValidationError::Code::DuplicateWriter));
  }

  TEST(RenderPlan, DetectsDependencyCycles) {
    RenderPlan plan;
    plan.addResource(colorResource("a"));
    plan.addResource(colorResource("b"));

    auto first = pass("first");
    first.reads.push_back({"b"});
    first.writes.push_back({"a"});
    plan.addPass(first);

    auto second = pass("second");
    second.reads.push_back({"a"});
    second.writes.push_back({"b"});
    plan.addPass(second);

    const auto validation = plan.validate();

    ASSERT_FALSE(validation.valid());
    EXPECT_TRUE(hasError(validation, RenderPlanValidationError::Code::Cycle));
  }

  TEST(RenderPlan, OrdersConsumerAfterProducerFromResourceEdges) {
    RenderPlan plan;
    plan.addResource(colorResource("beauty_color"));
    plan.addResource(colorResource("display_color", RenderResourceLifetime::Exported));

    auto tonemap = pass("tonemap", RenderPassKind::Tonemap);
    tonemap.reads.push_back({"beauty_color"});
    tonemap.writes.push_back({"display_color"});
    plan.addPass(tonemap);

    auto beauty = pass("beauty", RenderPassKind::Beauty);
    beauty.writes.push_back({"beauty_color"});
    plan.addPass(beauty);

    EXPECT_TRUE(plan.validate().valid());
    ASSERT_EQ(2u, plan.passes().size());
    EXPECT_EQ("tonemap", plan.passes()[0].id);
    EXPECT_EQ("beauty", plan.passes()[1].id);

    const auto order = plan.executionOrder();
    ASSERT_EQ(2u, order.size());
    EXPECT_EQ("beauty", order[0]->id);
    EXPECT_EQ("tonemap", order[1]->id);
    EXPECT_EQ(1, plan.executionOrderNumber("beauty"));
    EXPECT_EQ(2, plan.executionOrderNumber("tonemap"));
    EXPECT_FALSE(plan.executionOrderNumber("missing"));

    const std::string text = plan.toText();
    EXPECT_NE(std::string::npos, text.find("Execution order:\n- beauty\n- tonemap\n"));
    EXPECT_NE(std::string::npos,
              text.find("Dependencies:\n- beauty -> tonemap via beauty_color\n"));
    EXPECT_NE(std::string::npos, text.find("Passes:\n- tonemap"));
    EXPECT_NE(std::string::npos,
              text.find("- tonemap [tonemap/postprocess] enabled\n  schedule: stage=2, order=2"));
    EXPECT_NE(std::string::npos,
              text.find("- beauty [beauty/rasterizer] enabled\n  schedule: stage=1, order=1"));
  }

  TEST(RenderPlan, GroupsDependencyReadyPassesIntoExecutionStages) {
    RenderPlan plan;
    plan.addResource(colorResource("beauty_color"));
    plan.addResource(colorResource("normal_color", RenderResourceLifetime::Exported));
    plan.addResource(colorResource("depth_color", RenderResourceLifetime::Exported));
    plan.addResource(colorResource("main_color", RenderResourceLifetime::Exported));

    auto beauty = pass("beauty", RenderPassKind::Beauty);
    beauty.writes.push_back({"beauty_color"});
    plan.addPass(beauty);

    auto normal = pass("normal_aov", RenderPassKind::AOV);
    normal.writes.push_back({"normal_color"});
    plan.addPass(normal);

    auto depth = pass("depth_aov", RenderPassKind::AOV);
    depth.writes.push_back({"depth_color"});
    plan.addPass(depth);

    auto tonemap = pass("tonemap", RenderPassKind::Tonemap);
    tonemap.reads.push_back({"beauty_color"});
    tonemap.writes.push_back({"main_color"});
    plan.addPass(tonemap);

    EXPECT_TRUE(plan.validate().valid());

    const auto stages = plan.executionStages();
    ASSERT_EQ(2u, stages.size());
    ASSERT_EQ(3u, stages[0].size());
    EXPECT_EQ("beauty", stages[0][0]->id);
    EXPECT_EQ("normal_aov", stages[0][1]->id);
    EXPECT_EQ("depth_aov", stages[0][2]->id);
    ASSERT_EQ(1u, stages[1].size());
    EXPECT_EQ("tonemap", stages[1][0]->id);
    EXPECT_EQ(1, plan.executionStageNumber("beauty"));
    EXPECT_EQ(1, plan.executionStageNumber("normal_aov"));
    EXPECT_EQ(2, plan.executionStageNumber("tonemap"));
    EXPECT_FALSE(plan.executionStageNumber("missing"));

    const std::string text = plan.toText();
    EXPECT_NE(std::string::npos,
              text.find("Execution stages:\n- 1: beauty normal_aov depth_aov\n- 2: tonemap\n"));
  }

  TEST(RenderPlan, ExecutionOrderIncludesDisabledPassthroughEdges) {
    RenderPlan plan;
    plan.addResource(colorResource("beauty_color"));
    plan.addResource(colorResource("post_color"));
    plan.addResource(colorResource("display_color", RenderResourceLifetime::Exported));

    auto final = pass("final", RenderPassKind::PostProcess);
    final.reads.push_back({"post_color"});
    final.writes.push_back({"display_color"});
    plan.addPass(final);

    auto post = pass("post", RenderPassKind::PostProcess);
    post.reads.push_back({"beauty_color"});
    post.writes.push_back({"post_color"});
    post.disabledBehavior = DisabledBehavior::Passthrough;
    post.enabled = false;
    plan.addPass(post);

    auto beauty = pass("beauty", RenderPassKind::Beauty);
    beauty.writes.push_back({"beauty_color"});
    plan.addPass(beauty);

    EXPECT_TRUE(plan.validate().valid());

    const auto order = plan.executionOrder();
    ASSERT_EQ(3u, order.size());
    EXPECT_EQ("beauty", order[0]->id);
    EXPECT_EQ("post", order[1]->id);
    EXPECT_EQ("final", order[2]->id);
  }

  TEST(RenderPlan, ReportsWhetherAResourceCanReachAnotherResourceThroughPassEdges) {
    RenderPlan plan;
    plan.addResource(colorResource("source"));
    plan.addResource(colorResource("mid"));
    plan.addResource(colorResource("main_color", RenderResourceLifetime::Exported));
    plan.addResource(colorResource("side_color", RenderResourceLifetime::Exported));

    auto first = pass("first", RenderPassKind::PostProcess);
    first.reads.push_back({"source"});
    first.writes.push_back({"mid"});
    plan.addPass(first);

    auto second = pass("second", RenderPassKind::PostProcess);
    second.reads.push_back({"mid"});
    second.writes.push_back({"main_color"});
    plan.addPass(second);

    auto side = pass("side", RenderPassKind::PostProcess);
    side.reads.push_back({"source"});
    side.writes.push_back({"side_color"});
    plan.addPass(side);

    EXPECT_TRUE(plan.resourceCanReach("source", "main_color"));
    EXPECT_TRUE(plan.resourceCanReach("mid", "main_color"));
    EXPECT_TRUE(plan.resourceCanReach("source", "side_color"));
    EXPECT_FALSE(plan.resourceCanReach("side_color", "main_color"));
    EXPECT_FALSE(plan.resourceCanReach("main_color", "source"));
  }

  TEST(RenderPlan, AppliesDisableOverridesByPassKindExecutorFeatureAndId) {
    RenderPlan plan;
    auto beauty = pass("beauty");
    beauty.features.push_back("main");
    plan.addPass(beauty);

    auto shadow = pass("shadow", RenderPassKind::Shadow);
    shadow.features.push_back("shadow_maps");
    plan.addPass(shadow);

    auto overlay = pass("overlay", RenderPassKind::Overlay);
    overlay.executor = RenderExecutorKind::Wireframe;
    plan.addPass(overlay);

    auto debug = pass("debug", RenderPassKind::Debug);
    plan.addPass(debug);

    RenderGraphOverrides overrides;
    overrides.disabledPasses.insert("debug");
    overrides.disabledPassKinds.insert(RenderPassKind::Shadow);
    overrides.disabledExecutors.insert(RenderExecutorKind::Wireframe);
    overrides.disabledFeatures.insert("main");

    const RenderPlan disabled = plan.withOverrides(overrides);

    ASSERT_EQ(4u, disabled.passes().size());
    EXPECT_FALSE(disabled.passes()[0].enabled);
    EXPECT_FALSE(disabled.passes()[1].enabled);
    EXPECT_FALSE(disabled.passes()[2].enabled);
    EXPECT_FALSE(disabled.passes()[3].enabled);
  }

  TEST(RenderPlan, FindsPassResourcesAndResourceEdges) {
    RenderPlan plan;
    plan.addResource(colorResource("beauty_color"));
    plan.addResource(colorResource("display_color", RenderResourceLifetime::Exported));

    auto beauty = pass("raster_beauty", RenderPassKind::Beauty);
    beauty.writes.push_back({"beauty_color"});
    plan.addPass(beauty);

    auto tonemap = pass("tonemap", RenderPassKind::Tonemap);
    tonemap.reads.push_back({"beauty_color"});
    tonemap.writes.push_back({"display_color"});
    plan.addPass(tonemap);

    ASSERT_NE(nullptr, plan.findPass("raster_beauty"));
    EXPECT_EQ(RenderPassKind::Beauty, plan.findPass("raster_beauty")->kind);
    EXPECT_EQ(nullptr, plan.findPass("missing"));

    ASSERT_NE(nullptr, plan.findResource("display_color"));
    EXPECT_EQ(RenderResourceLifetime::Exported, plan.findResource("display_color")->lifetime);
    EXPECT_EQ(nullptr, plan.findResource("missing"));

    ASSERT_NE(nullptr, plan.producerOf("beauty_color"));
    EXPECT_EQ("raster_beauty", plan.producerOf("beauty_color")->id);
    EXPECT_EQ(nullptr, plan.producerOf("history_color"));

    const auto consumers = plan.consumersOf("beauty_color");
    ASSERT_EQ(1u, consumers.size());
    EXPECT_EQ("tonemap", consumers.front()->id);
    EXPECT_TRUE(plan.consumersOf("display_color").empty());

    const auto dependencies = plan.dependencies();
    ASSERT_EQ(1u, dependencies.size());
    EXPECT_EQ("raster_beauty", dependencies.front().producer->id);
    EXPECT_EQ("tonemap", dependencies.front().consumer->id);
    EXPECT_EQ("beauty_color", dependencies.front().resource);

    const auto intoTonemap = plan.dependenciesInto("tonemap");
    ASSERT_EQ(1u, intoTonemap.size());
    EXPECT_EQ("raster_beauty", intoTonemap.front().producer->id);
    EXPECT_EQ("beauty_color", intoTonemap.front().resource);
    EXPECT_TRUE(plan.dependenciesInto("raster_beauty").empty());

    const auto outOfBeauty = plan.dependenciesOutOf("raster_beauty");
    ASSERT_EQ(1u, outOfBeauty.size());
    EXPECT_EQ("tonemap", outOfBeauty.front().consumer->id);
    EXPECT_EQ("beauty_color", outOfBeauty.front().resource);
    EXPECT_TRUE(plan.dependenciesOutOf("tonemap").empty());
  }

  TEST(RenderPlan, ComparesExecutionEquivalentPlans) {
    const RenderPlan plan = executionEquivalencePlan();

    EXPECT_TRUE(plan.executionEquivalentTo(
      executionEquivalencePlan(640, true, std::make_shared<FxaaPostProcessAAState>(), " renamed")));
    EXPECT_FALSE(plan.executionEquivalentTo(executionEquivalencePlan(800)));
    EXPECT_FALSE(plan.executionEquivalentTo(executionEquivalencePlan(640, false)));
    EXPECT_FALSE(plan.executionEquivalentTo(
      executionEquivalencePlan(640, true, std::make_shared<SmaaPostProcessAAState>())));
  }

  TEST(RenderPlan, DisabledSubstituteDefaultCanSatisfyConsumer) {
    RenderPlan plan;
    plan.addResource(colorResource("shadow_mask"));
    plan.addResource(colorResource("main_color"));

    auto shadow = pass("shadow", RenderPassKind::Shadow);
    shadow.writes.push_back({"shadow_mask"});
    shadow.disabledBehavior = DisabledBehavior::SubstituteDefault;
    shadow.enabled = false;
    plan.addPass(shadow);

    auto main = pass("main");
    main.reads.push_back({"shadow_mask"});
    main.writes.push_back({"main_color"});
    plan.addPass(main);

    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderPlan, DisabledPassthroughCanSatisfyConsumer) {
    RenderPlan plan;
    plan.addResource(colorResource("main_color"));
    plan.addResource(colorResource("display_color"));
    plan.addResource(colorResource("export_color", RenderResourceLifetime::Exported));

    auto main = pass("main");
    main.writes.push_back({"main_color"});
    plan.addPass(main);

    auto post = pass("post", RenderPassKind::PostProcess);
    post.reads.push_back({"main_color"});
    post.writes.push_back({"display_color"});
    post.disabledBehavior = DisabledBehavior::Passthrough;
    post.enabled = false;
    plan.addPass(post);

    auto final = pass("final", RenderPassKind::PostProcess);
    final.reads.push_back({"display_color"});
    final.writes.push_back({"export_color"});
    plan.addPass(final);

    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderPlan, ReportsInvalidDisabledPassthroughIO) {
    RenderPlan missingRead;
    auto noInput = pass("post", RenderPassKind::PostProcess);
    noInput.disabledBehavior = DisabledBehavior::Passthrough;
    noInput.enabled = false;
    missingRead.addPass(noInput);

    const auto missingReadValidation = missingRead.validate();

    ASSERT_FALSE(missingReadValidation.valid());
    EXPECT_TRUE(hasError(missingReadValidation, RenderPlanValidationError::Code::InvalidPassIO));

    RenderPlan mismatchedShape;
    mismatchedShape.addResource(colorResource("main_color"));
    auto display = colorResource("display_color", RenderResourceLifetime::Exported);
    display.width = 320;
    mismatchedShape.addResource(display);

    auto beauty = pass("beauty", RenderPassKind::Beauty);
    beauty.writes.push_back({"main_color"});
    mismatchedShape.addPass(beauty);

    auto post = pass("post", RenderPassKind::PostProcess);
    post.reads.push_back({"main_color"});
    post.writes.push_back({"display_color"});
    post.disabledBehavior = DisabledBehavior::Passthrough;
    post.enabled = false;
    mismatchedShape.addPass(post);

    const auto mismatchedValidation = mismatchedShape.validate();

    ASSERT_FALSE(mismatchedValidation.valid());
    EXPECT_TRUE(
      hasError(mismatchedValidation, RenderPlanValidationError::Code::InvalidResourceShape));
  }

  TEST(RenderPlan, ReportsInvalidDisabledPassthroughMultiOutputShape) {
    RenderPlan plan;
    plan.addResource(colorResource("main_color"));
    plan.addResource(colorResource("matching_color", RenderResourceLifetime::Exported));
    auto mismatched = colorResource("mismatched_color", RenderResourceLifetime::Exported);
    mismatched.height = 128;
    plan.addResource(mismatched);

    auto beauty = pass("beauty", RenderPassKind::Beauty);
    beauty.writes.push_back({"main_color"});
    plan.addPass(beauty);

    auto post = pass("post", RenderPassKind::PostProcess);
    post.reads.push_back({"main_color"});
    post.writes.push_back({"matching_color"});
    post.writes.push_back({"mismatched_color"});
    post.disabledBehavior = DisabledBehavior::Passthrough;
    post.enabled = false;
    plan.addPass(post);

    const auto validation = plan.validate();

    ASSERT_FALSE(validation.valid());
    const auto& errors = validation.errors();
    EXPECT_TRUE(std::any_of(errors.begin(), errors.end(), [](const auto& error) {
      return error.code == RenderPlanValidationError::Code::InvalidResourceShape &&
             error.resourceId == "mismatched_color";
    }));
  }

  TEST(RenderPlan, DisabledCullDependencyDoesNotSatisfyConsumer) {
    RenderPlan plan;
    plan.addResource(colorResource("shadow_mask"));
    plan.addResource(colorResource("main_color"));

    auto shadow = pass("shadow", RenderPassKind::Shadow);
    shadow.writes.push_back({"shadow_mask"});
    shadow.disabledBehavior = DisabledBehavior::CullDependents;
    shadow.enabled = false;
    plan.addPass(shadow);

    auto main = pass("main");
    main.reads.push_back({"shadow_mask"});
    main.writes.push_back({"main_color"});
    plan.addPass(main);

    const auto validation = plan.validate();

    ASSERT_FALSE(validation.valid());
    EXPECT_TRUE(hasError(validation, RenderPlanValidationError::Code::DisabledDependency));

    const auto dependencies = plan.dependencies();
    ASSERT_EQ(1u, dependencies.size());
    EXPECT_EQ("shadow", dependencies.front().producer->id);
    EXPECT_EQ("main", dependencies.front().consumer->id);
    EXPECT_EQ("shadow_mask", dependencies.front().resource);
  }

  TEST(RenderPlan, CullDependentsOverrideDisablesTransitiveConsumers) {
    RenderPlan plan;
    plan.addResource(colorResource("shadow_map"));
    plan.addResource(colorResource("beauty_color"));
    plan.addResource(colorResource("display_color", RenderResourceLifetime::Exported));

    auto shadow = pass("shadow", RenderPassKind::Shadow);
    shadow.writes.push_back({"shadow_map"});
    shadow.disabledBehavior = DisabledBehavior::CullDependents;
    plan.addPass(shadow);

    auto beauty = pass("beauty", RenderPassKind::Beauty);
    beauty.reads.push_back({"shadow_map"});
    beauty.writes.push_back({"beauty_color"});
    plan.addPass(beauty);

    auto tonemap = pass("tonemap", RenderPassKind::Tonemap);
    tonemap.reads.push_back({"beauty_color"});
    tonemap.writes.push_back({"display_color"});
    plan.addPass(tonemap);

    RenderGraphOverrides overrides;
    overrides.disabledPasses.insert("shadow");

    const RenderPlan disabled = plan.withOverrides(overrides);

    ASSERT_EQ(3u, disabled.passes().size());
    EXPECT_FALSE(disabled.passes()[0].enabled);
    EXPECT_FALSE(disabled.passes()[1].enabled);
    EXPECT_EQ(DisabledBehavior::CullDependents, disabled.passes()[1].disabledBehavior);
    EXPECT_FALSE(disabled.passes()[2].enabled);
    EXPECT_EQ(DisabledBehavior::CullDependents, disabled.passes()[2].disabledBehavior);
    EXPECT_TRUE(disabled.validate().valid());
  }

  TEST(RenderPlan, ExportsTextDotAndJson) {
    RenderPlan plan;
    plan.addResource(colorResource("main_color"));

    auto main = pass("main");
    main.features.push_back("beauty");
    main.writes.push_back({"main_color"});
    plan.addPass(main);

    const std::string text = plan.toText();
    EXPECT_NE(std::string::npos, text.find("main_color"));
    EXPECT_NE(std::string::npos, text.find("main"));
    EXPECT_NE(std::string::npos, text.find("features: beauty"));
    EXPECT_NE(std::string::npos, text.find("Execution order"));
    EXPECT_NE(std::string::npos, text.find("Dependencies"));

    const std::string dot = plan.toDot();
    EXPECT_NE(std::string::npos, dot.find("digraph RenderPlan"));
    EXPECT_NE(std::string::npos, dot.find("resource:main_color"));
    EXPECT_NE(std::string::npos, dot.find("color/rgb_double"));
    EXPECT_NE(std::string::npos, dot.find("transient"));
    EXPECT_NE(std::string::npos, dot.find("execution_stage_1"));
    EXPECT_NE(std::string::npos, dot.find("stage 1, order 1"));

    const QJsonObject json = plan.toJson();
    ASSERT_TRUE(json["resources"].isArray());
    ASSERT_TRUE(json["passes"].isArray());
    ASSERT_TRUE(json["executionStages"].isArray());
    EXPECT_EQ(1, json["resources"].toArray().size());
    EXPECT_EQ(1, json["passes"].toArray().size());
    ASSERT_EQ(1, json["executionStages"].toArray().size());
    const auto stage = json["executionStages"].toArray().at(0).toObject();
    EXPECT_EQ(1, stage["index"].toInt());
    ASSERT_TRUE(stage["passes"].isArray());
    EXPECT_EQ("main", stage["passes"].toArray().at(0).toString().toStdString());
  }

  TEST(RenderPlan, DotStylesDisabledPasses) {
    RenderPlan plan;
    plan.addResource(colorResource("main_color"));

    auto main = pass("main");
    main.writes.push_back({"main_color"});
    main.enabled = false;
    main.disabledBehavior = DisabledBehavior::SubstituteDefault;
    plan.addPass(main);

    const std::string dot = plan.toDot();

    EXPECT_NE(std::string::npos, dot.find("pass:main"));
    EXPECT_NE(std::string::npos, dot.find("style=dashed"));
    EXPECT_NE(std::string::npos, dot.find("color=gray50"));
  }

  TEST(RenderPlan, ImportsJsonExportRoundTrip) {
    RenderPlan plan;

    auto history = colorResource("history_color", RenderResourceLifetime::History);
    history.name = "History color";
    history.sampleCount = 4;
    plan.addResource(history);

    auto main = colorResource("main_color", RenderResourceLifetime::Exported);
    main.type = RenderResourceType::Normal;
    main.format = RenderResourceFormat::ScalarDouble;
    main.domain = RenderResourceDomain::GPU;
    plan.addResource(main);

    auto node = pass("raster_beauty", RenderPassKind::Beauty);
    node.name = "Raster beauty";
    node.executor = RenderExecutorKind::Rasterizer;
    node.features = {"main", "display"};
    node.reads.push_back({"history_color"});
    node.writes.push_back({"main_color"});
    node.sceneView.selector = SceneSelector::objectName("hero");
    node.disabledBehavior = DisabledBehavior::Passthrough;
    node.enabled = false;
    node.hasExternalSideEffects = true;
    node.canRunConcurrently = false;
    auto state = std::make_shared<RasterBeautyPassState>();
    state->sampling().setPostProcessAA(Rasterizer::PostProcessAA::FXAA);
    state->sampling().setMSAASamples(4);
    node.state = state;
    plan.addPass(node);

    const QJsonObject json = plan.toJson();
    const RenderPlan imported = RenderPlan::fromJson(json);

    EXPECT_EQ(json, imported.toJson());
  }

  TEST(RenderPlan, ImportsJsonRecomputesExecutionStages) {
    RenderPlan plan;
    plan.addResource(colorResource("beauty_color"));
    plan.addResource(colorResource("main_color", RenderResourceLifetime::Exported));

    auto tonemap = pass("tonemap", RenderPassKind::Tonemap);
    tonemap.reads.push_back({"beauty_color"});
    tonemap.writes.push_back({"main_color"});
    plan.addPass(tonemap);

    auto beauty = pass("beauty", RenderPassKind::Beauty);
    beauty.writes.push_back({"beauty_color"});
    plan.addPass(beauty);

    QJsonObject json = plan.toJson();
    QJsonObject staleStage;
    staleStage["index"] = 99;
    QJsonArray stalePasses;
    stalePasses.append("tonemap");
    stalePasses.append("beauty");
    staleStage["passes"] = stalePasses;
    QJsonArray staleStages;
    staleStages.append(staleStage);
    json["executionStages"] = staleStages;

    const RenderPlan imported = RenderPlan::fromJson(json);

    const auto stages = imported.executionStages();
    ASSERT_EQ(2u, stages.size());
    ASSERT_EQ(1u, stages[0].size());
    ASSERT_EQ(1u, stages[1].size());
    EXPECT_EQ("beauty", stages[0][0]->id);
    EXPECT_EQ("tonemap", stages[1][0]->id);

    const auto exportedStages = imported.toJson()["executionStages"].toArray();
    ASSERT_EQ(2, exportedStages.size());
    EXPECT_EQ(1, exportedStages.at(0).toObject()["index"].toInt());
    EXPECT_EQ("beauty",
              exportedStages.at(0).toObject()["passes"].toArray().at(0).toString().toStdString());
    EXPECT_EQ(2, exportedStages.at(1).toObject()["index"].toInt());
    EXPECT_EQ("tonemap",
              exportedStages.at(1).toObject()["passes"].toArray().at(0).toString().toStdString());
  }

  TEST(RenderPlan, AddsResourceProducerWithWriteEdge) {
    RenderPlan plan;
    auto beauty = pass("raster_beauty", RenderPassKind::Beauty);
    beauty.executor = RenderExecutorKind::Rasterizer;

    plan.addResourceProducer(beauty,
                             colorResource("beauty_color", RenderResourceLifetime::Exported));

    ASSERT_EQ(1u, plan.resources().size());
    EXPECT_EQ("beauty_color", plan.resources()[0].id);
    ASSERT_EQ(1u, plan.passes().size());
    EXPECT_EQ("raster_beauty", plan.passes()[0].id);
    ASSERT_EQ(1u, plan.passes()[0].writes.size());
    EXPECT_EQ("beauty_color", plan.passes()[0].writes[0].resource);
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderPlan, RoutesResourceThroughInsertedPass) {
    RenderPlan plan;
    plan.addResource(colorResource("beauty_color", RenderResourceLifetime::Transient));
    plan.addResource(colorResource("main_color", RenderResourceLifetime::Exported));

    auto beauty = pass("raster_beauty", RenderPassKind::Beauty);
    beauty.executor = RenderExecutorKind::Rasterizer;
    beauty.writes.push_back({"beauty_color"});
    plan.addPass(beauty);

    auto tonemap = pass("tonemap", RenderPassKind::Tonemap);
    tonemap.executor = RenderExecutorKind::PostProcess;
    tonemap.reads.push_back({"beauty_color"});
    tonemap.writes.push_back({"main_color"});
    plan.addPass(tonemap);

    auto filtered = colorResource("post_aa_color", RenderResourceLifetime::Transient);
    auto fxaa = pass("post_fxaa", RenderPassKind::PostProcess);
    fxaa.executor = RenderExecutorKind::PostProcess;
    fxaa.reads.push_back({"beauty_color"});
    fxaa.writes.push_back({"post_aa_color"});

    EXPECT_EQ(1u, plan.routeResourceThroughPass("beauty_color", filtered, fxaa));

    ASSERT_EQ(3u, plan.resources().size());
    EXPECT_EQ("post_aa_color", plan.resources()[2].id);
    ASSERT_EQ(3u, plan.passes().size());
    EXPECT_EQ("raster_beauty", plan.passes()[0].id);
    EXPECT_EQ("post_fxaa", plan.passes()[1].id);
    EXPECT_EQ("tonemap", plan.passes()[2].id);
    ASSERT_EQ(1u, plan.passes()[2].reads.size());
    EXPECT_EQ("post_aa_color", plan.passes()[2].reads[0].resource);
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderPlan, ConnectsProducerToExistingConsumerWithResourceEdge) {
    RenderPlan plan;
    plan.addResource(colorResource("beauty_color", RenderResourceLifetime::Transient));
    plan.addResource(colorResource("main_color", RenderResourceLifetime::Exported));

    auto beauty = pass("raster_beauty", RenderPassKind::Beauty);
    beauty.executor = RenderExecutorKind::Rasterizer;
    beauty.writes.push_back({"beauty_color"});
    plan.addPass(beauty);

    auto tonemap = pass("tonemap", RenderPassKind::Tonemap);
    tonemap.executor = RenderExecutorKind::PostProcess;
    tonemap.reads.push_back({"beauty_color"});
    tonemap.writes.push_back({"main_color"});
    plan.addPass(tonemap);

    RenderResourceDescriptor shadowMap;
    shadowMap.id = "preview_shadow_map";
    shadowMap.name = "Preview shadow map";
    shadowMap.type = RenderResourceType::ShadowMap;
    shadowMap.lifetime = RenderResourceLifetime::Transient;

    auto shadows = pass("raster_preview_shadows", RenderPassKind::Shadow);
    shadows.executor = RenderExecutorKind::Rasterizer;

    plan.connectProducerToConsumer(shadows, shadowMap, "raster_beauty");

    ASSERT_EQ(3u, plan.resources().size());
    EXPECT_EQ("preview_shadow_map", plan.resources()[2].id);
    ASSERT_EQ(3u, plan.passes().size());
    EXPECT_EQ("raster_preview_shadows", plan.passes()[0].id);
    EXPECT_EQ("raster_beauty", plan.passes()[1].id);
    EXPECT_EQ("tonemap", plan.passes()[2].id);
    ASSERT_EQ(1u, plan.passes()[0].writes.size());
    EXPECT_EQ("preview_shadow_map", plan.passes()[0].writes[0].resource);
    ASSERT_EQ(1u, plan.passes()[1].reads.size());
    EXPECT_EQ("preview_shadow_map", plan.passes()[1].reads[0].resource);
    EXPECT_TRUE(plan.validate().valid());
  }

  TEST(RenderPlan, RoundTripsPostProcessAAState) {
    RenderPlan plan;
    plan.addResource(colorResource("beauty_color", RenderResourceLifetime::Transient));
    plan.addResource(colorResource("post_aa_color", RenderResourceLifetime::Exported));

    auto beauty = pass("raster_beauty", RenderPassKind::Beauty);
    beauty.executor = RenderExecutorKind::Rasterizer;
    beauty.writes.push_back({"beauty_color"});
    plan.addPass(beauty);

    auto fxaa = pass("post_fxaa", RenderPassKind::PostProcess);
    fxaa.executor = RenderExecutorKind::PostProcess;
    fxaa.features = {"post_aa", "fxaa"};
    fxaa.reads.push_back({"beauty_color"});
    fxaa.writes.push_back({"post_aa_color"});
    fxaa.state = std::make_shared<FxaaPostProcessAAState>();
    plan.addPass(fxaa);

    const RenderPlan imported = RenderPlan::fromJson(plan.toJson());

    ASSERT_EQ(2u, imported.passes().size());
    const auto state = PostProcessAAState::fromPass(imported.passes()[1]);
    ASSERT_NE(nullptr, state);
    EXPECT_EQ("fxaa", std::string(state->modeName()));
    EXPECT_EQ(plan.toJson(), imported.toJson());
  }

  TEST(RenderPlan, RoundTripsRasterShadowPassState) {
    RenderPlan plan;
    RenderResourceDescriptor shadowMap;
    shadowMap.id = "preview_shadow_map";
    shadowMap.name = "Preview shadow map";
    shadowMap.type = RenderResourceType::ShadowMap;
    shadowMap.lifetime = RenderResourceLifetime::Transient;
    plan.addResource(shadowMap);

    auto shadow = pass("raster_preview_shadows", RenderPassKind::Shadow);
    shadow.executor = RenderExecutorKind::Rasterizer;
    shadow.writes.push_back({"preview_shadow_map"});
    shadow.state =
      std::make_shared<RasterShadowPassState>(RasterShadowPassState::previewDefaults());
    plan.addPass(shadow);

    const RenderPlan imported = RenderPlan::fromJson(plan.toJson());

    ASSERT_EQ(1u, imported.passes().size());
    const auto state = RasterShadowPassState::fromPass(imported.passes()[0]);
    ASSERT_NE(nullptr, state);
    EXPECT_TRUE(state->shadows().enabled());
    EXPECT_EQ(plan.toJson(), imported.toJson());
  }

  TEST(RenderPlan, RejectsMalformedJsonImport) {
    QJsonObject badRoot;
    badRoot["resources"] = "not an array";
    badRoot["passes"] = QJsonArray();

    EXPECT_THROW(RenderPlan::fromJson(badRoot), std::runtime_error);

    QJsonObject badEnum;
    QJsonArray resources;
    QJsonObject resource;
    resource["id"] = "main_color";
    resource["type"] = "not_a_resource_type";
    resources.append(resource);
    badEnum["resources"] = resources;
    badEnum["passes"] = QJsonArray();

    EXPECT_THROW(RenderPlan::fromJson(badEnum), std::runtime_error);
  }
}
