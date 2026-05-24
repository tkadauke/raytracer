#include <gtest/gtest.h>

#include "engine/graph/GraphRenderEngine.h"
#include "engine/graph/RenderExecutionContext.h"
#include "engine/graph/RenderResourceStorage.h"
#include "render/primitives/Scene.h"

#include <memory>
#include <utility>

namespace RenderExecutionContextTest {
  using namespace engine::graph;

  class FakeRenderEngine : public render::RenderEngine {
  public:
    FakeRenderEngine()
        : RenderEngine(std::shared_ptr<render::Scene>()) {
    }

    void render(Buffer<Colord>&) override {}
    void cancel() override {}
    void uncancel() override {}
  };

  RenderResourceDescriptor colorResource() {
    RenderResourceDescriptor color;
    color.id = "main_color";
    color.type = RenderResourceType::Color;
    color.format = RenderResourceFormat::RGBDouble;
    color.width = 2;
    color.height = 2;
    color.lifetime = RenderResourceLifetime::Exported;
    return color;
  }

  TEST(RenderExecutionContext, ExposesPassStorageGraphAndActiveEngineHook) {
    RenderPassNode pass;
    pass.id = "beauty";

    RenderResourceStorage storage;
    storage.allocate({colorResource()});

    auto scene = std::make_shared<render::Scene>();
    GraphRenderEngine graph(scene);

    std::shared_ptr<render::RenderEngine> activeEngine;
    RenderExecutionContext context(
      pass, storage, graph, true,
      [&](std::shared_ptr<render::RenderEngine> engine) { activeEngine = std::move(engine); });

    EXPECT_EQ("beauty", context.pass().id);
    EXPECT_TRUE(context.storage().contains("main_color"));
    EXPECT_EQ(&graph, &context.graph());
    EXPECT_TRUE(context.cancelled());

    auto fake = std::make_shared<FakeRenderEngine>();
    context.setActiveEngine(fake);
    EXPECT_EQ(fake, activeEngine);

    context.clearActiveEngine();
    EXPECT_EQ(nullptr, activeEngine);
  }
}
