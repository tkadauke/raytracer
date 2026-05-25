#include <gtest/gtest.h>

#include "engine/graph/PostProcessPassState.h"
#include "engine/graph/RenderGraphTypes.h"
#include "engine/graph/RenderPassState.h"

#include <QJsonObject>

#include <memory>
#include <stdexcept>
#include <string>

namespace PostProcessPassStateTest {
  using namespace engine::graph;

  TEST(PostProcessAAState, SerializesTypedMode) {
    FxaaPostProcessAAState state;

    const QJsonObject json = state.toJson();

    EXPECT_EQ("post_process_aa", json.value("type").toString().toStdString());
    EXPECT_EQ("fxaa", json.value("mode").toString().toStdString());
  }

  TEST(PostProcessAAState, ImportsTypedModeAtJsonBoundary) {
    QJsonObject json;
    json["type"] = "post_process_aa";
    json["mode"] = "smaa";

    const auto state = RenderPassState::fromJson(
      RenderPassKind::PostProcess, RenderExecutorKind::PostProcess, json, "parameters");

    const auto aa = std::dynamic_pointer_cast<const PostProcessAAState>(state);
    ASSERT_NE(nullptr, aa);
    EXPECT_EQ("smaa", std::string(aa->modeName()));
    EXPECT_EQ(json, aa->toJson());
  }

  TEST(PostProcessAAState, InfersLegacyModeFromPassFeatures) {
    RenderPassNode pass;
    pass.kind = RenderPassKind::PostProcess;
    pass.executor = RenderExecutorKind::PostProcess;
    pass.features = {"post_aa", "fxaa"};

    const auto state = PostProcessAAState::fromPass(pass);

    ASSERT_NE(nullptr, state);
    EXPECT_EQ("fxaa", std::string(state->modeName()));
  }

  TEST(PostProcessAADefinition, CreatesGraphPassStateForSupportedModes) {
    const auto* fxaa = postProcessAADefinition(RenderPostProcessAA::FXAA);
    ASSERT_NE(nullptr, fxaa);
    EXPECT_EQ("post_fxaa", std::string(fxaa->passId()));
    EXPECT_EQ("FXAA", std::string(fxaa->passName()));
    EXPECT_EQ("fxaa", std::string(fxaa->feature()));
    EXPECT_EQ("fxaa", std::string(fxaa->createState()->modeName()));

    EXPECT_EQ(nullptr, postProcessAADefinition(RenderPostProcessAA::None));
    EXPECT_EQ(nullptr, postProcessAADefinition(RenderPostProcessAA::TAA));
  }

  TEST(PostProcessAAState, RejectsUnknownJsonMode) {
    QJsonObject json;
    json["type"] = "post_process_aa";
    json["mode"] = "taa";

    EXPECT_THROW(PostProcessAAState::fromJson(json), std::runtime_error);
  }
}
