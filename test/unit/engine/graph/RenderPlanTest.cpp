#include <gtest/gtest.h>

#include "engine/graph/RenderPlan.h"

#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
#include <stdexcept>
#include <string>

namespace RenderPlanTest {
  using namespace engine::graph;

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

    const std::string dot = plan.toDot();
    EXPECT_NE(std::string::npos, dot.find("digraph RenderPlan"));
    EXPECT_NE(std::string::npos, dot.find("resource:main_color"));

    const QJsonObject json = plan.toJson();
    ASSERT_TRUE(json["resources"].isArray());
    ASSERT_TRUE(json["passes"].isArray());
    EXPECT_EQ(1, json["resources"].toArray().size());
    EXPECT_EQ(1, json["passes"].toArray().size());
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

    auto node = pass("tonemap", RenderPassKind::Tonemap);
    node.name = "Tone map";
    node.executor = RenderExecutorKind::PostProcess;
    node.features = {"main", "display"};
    node.reads.push_back({"history_color"});
    node.writes.push_back({"main_color"});
    node.sceneView.selector = SceneSelector::objectName("hero");
    node.disabledBehavior = DisabledBehavior::Passthrough;
    node.enabled = false;
    node.hasExternalSideEffects = true;
    node.canRunConcurrently = false;
    plan.addPass(node);

    const QJsonObject json = plan.toJson();
    const RenderPlan imported = RenderPlan::fromJson(json);

    EXPECT_EQ(json, imported.toJson());
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
