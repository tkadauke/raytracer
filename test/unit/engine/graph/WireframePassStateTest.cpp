#include <gtest/gtest.h>

#include "engine/graph/RenderPlan.h"
#include "engine/graph/RenderPassState.h"
#include "engine/graph/WireframePassState.h"

#include <QJsonObject>

#include <memory>
#include <stdexcept>

namespace WireframePassStateTest {
  using namespace engine::graph;

  TEST(WireframePassState, SerializesOnlyNonDefaultState) {
    WireframePassState empty;
    EXPECT_TRUE(empty.empty());

    WireframePassState state;
    state.setLod(3);

    const QJsonObject json = state.toJson();

    EXPECT_FALSE(state.empty());
    EXPECT_EQ(3, json.value("lod").toInt());
  }

  TEST(WireframePassState, ImportsAtJsonBoundaryAndAppliesToWireframe) {
    QJsonObject json;
    json["lod"] = 4;

    const auto state = RenderPassState::fromJson(RenderPassKind::Beauty,
                                                 RenderExecutorKind::Wireframe, json, "parameters");

    const auto* wireframeState = state->asWireframePassState();
    ASSERT_NE(nullptr, wireframeState);

    engine::wireframe::Wireframe wireframe(nullptr);
    wireframeState->applyTo(wireframe);

    EXPECT_EQ(4, wireframe.lod());
  }

  TEST(WireframePassState, WritesOnlyToWireframePasses) {
    RenderPlan plan;

    RenderPassNode beauty;
    beauty.id = "wireframe_beauty";
    beauty.kind = RenderPassKind::Beauty;
    beauty.executor = RenderExecutorKind::Wireframe;
    plan.addPass(beauty);

    RenderPassNode overlay;
    overlay.id = "wireframe_overlay";
    overlay.kind = RenderPassKind::Overlay;
    overlay.executor = RenderExecutorKind::Wireframe;
    plan.addPass(overlay);

    RenderPassNode raster;
    raster.id = "raster_beauty";
    raster.kind = RenderPassKind::Beauty;
    raster.executor = RenderExecutorKind::Rasterizer;
    plan.addPass(raster);

    WireframePassState state;
    state.setLod(2);

    EXPECT_EQ(2u, state.writeToWireframePasses(plan));
    ASSERT_NE(nullptr, plan.passes()[0].state);
    EXPECT_EQ(2, WireframePassState::fromPass(plan.passes()[0])->lod());
    ASSERT_NE(nullptr, plan.passes()[1].state);
    EXPECT_EQ(2, WireframePassState::fromPass(plan.passes()[1])->lod());
    EXPECT_EQ(nullptr, plan.passes()[2].state);
  }

  TEST(WireframePassState, RejectsUnknownFieldsDuringImport) {
    QJsonObject json;
    json["lod"] = 2;
    json["surprise"] = true;

    EXPECT_THROW(WireframePassState::fromJson(json), std::runtime_error);
  }
}
