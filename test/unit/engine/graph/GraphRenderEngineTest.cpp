#include <gtest/gtest.h>

#include "core/Buffer.h"
#include "engine/graph/GraphRenderEngine.h"
#include "engine/graph/RenderGraphCompiler.h"
#include "render/cameras/PinholeCamera.h"
#include "render/materials/Material.h"
#include "render/primitives/Box.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "render/tonemap/ReinhardTonemap.h"
#include "test/helpers/ColorTestHelper.h"

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

namespace GraphRenderEngineTest {
  using namespace engine::graph;

  std::shared_ptr<render::Camera> camera() {
    return std::make_shared<render::PinholeCamera>(Vector3d(0, 0, -5), Vector3d::null);
  }

  RenderResourceDescriptor colorResource(const std::string& id,
                                         RenderResourceLifetime lifetime,
                                         int width = 2,
                                         int height = 2) {
    RenderResourceDescriptor color;
    color.id = id;
    color.type = RenderResourceType::Color;
    color.format = RenderResourceFormat::RGBDouble;
    color.width = width;
    color.height = height;
    color.lifetime = lifetime;
    return color;
  }

  class BlockingMaterial : public render::Material {
  public:
    Colord shade(const render::RayCaster*,
                 const render::Scene&,
                 const Rayd&,
                 const HitPoint&,
                 render::State&) const override {
      std::unique_lock<std::mutex> lock(m_mutex);
      ++m_calls;
      if (m_calls == 2) {
        m_secondCallEntered = true;
        m_changed.notify_all();
        m_changed.wait(lock, [&] { return m_releaseSecondCall; });
      }
      return Colord(0.25, 0.5, 0.75);
    }

    bool waitForSecondCall(std::chrono::milliseconds timeout) {
      std::unique_lock<std::mutex> lock(m_mutex);
      return m_changed.wait_for(lock, timeout, [&] { return m_secondCallEntered; });
    }

    void releaseSecondCall() {
      {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_releaseSecondCall = true;
      }
      m_changed.notify_all();
    }

  private:
    mutable std::mutex m_mutex;
    mutable std::condition_variable m_changed;
    mutable int m_calls{0};
    mutable bool m_secondCallEntered{false};
    mutable bool m_releaseSecondCall{false};
  };

  int countPixels(const Buffer<Colord>& buffer, const Colord& color) {
    int count = 0;
    for (int y = 0; y != buffer.height(); ++y) {
      for (int x = 0; x != buffer.width(); ++x) {
        if (buffer[y][x] == color) {
          ++count;
        }
      }
    }
    return count;
  }

  TEST(GraphRenderEngine, CompilesAndExecutesDefaultRaytracedBeautyPass) {
    auto scene = std::make_shared<render::Scene>();
    scene->setBackground(Colord(0.2, 0.4, 0.6));
    GraphRenderEngine engine(scene);

    Buffer<Colord> buffer(3, 2);
    engine.render(buffer);

    for (int y = 0; y != buffer.height(); ++y) {
      for (int x = 0; x != buffer.width(); ++x) {
        EXPECT_EQ(Colord(0.2, 0.4, 0.6), buffer[y][x]);
      }
    }

    ASSERT_EQ(2u, engine.lastPlan().passes().size());
    EXPECT_EQ(RenderExecutorKind::Raytracer, engine.lastPlan().passes()[0].executor);
    EXPECT_EQ(RenderPassKind::Tonemap, engine.lastPlan().passes()[1].kind);
    EXPECT_EQ(RenderExecutorKind::PostProcess, engine.lastPlan().passes()[1].executor);
  }

  TEST(GraphRenderEngine, ExecutesCallerProvidedRasterBeautyPlan) {
    auto scene = std::make_shared<render::Scene>();
    scene->setBackground(Colord(0.1, 0.3, 0.5));

    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    RenderGraphCompiler compiler;

    GraphRenderEngine engine(camera(), scene);
    engine.setPlan(compiler.compile({4, 4, 1}, intent));

    Buffer<Colord> buffer(4, 4);
    engine.render(buffer);

    EXPECT_TRUE(engine.hasExplicitPlan());
    ASSERT_EQ(2u, engine.lastPlan().passes().size());
    EXPECT_EQ(RenderExecutorKind::Rasterizer, engine.lastPlan().passes()[0].executor);
    EXPECT_EQ(RenderPassKind::Tonemap, engine.lastPlan().passes()[1].kind);
    EXPECT_EQ(Colord(0.1, 0.3, 0.5), buffer[0][0]);
  }

  TEST(GraphRenderEngine, DisabledBeautyPassCanSubstituteDefaultOutput) {
    auto scene = std::make_shared<render::Scene>();
    scene->setBackground(Colord(0.25, 0.5, 0.75));

    RenderPlan plan;
    plan.addResource(colorResource("main_color", RenderResourceLifetime::Exported));

    RenderPassNode beauty;
    beauty.id = "beauty";
    beauty.kind = RenderPassKind::Beauty;
    beauty.executor = RenderExecutorKind::Raytracer;
    beauty.writes.push_back({"main_color"});
    beauty.disabledBehavior = DisabledBehavior::SubstituteDefault;
    beauty.enabled = false;
    plan.addPass(beauty);

    GraphRenderEngine engine(camera(), scene);
    engine.setPlan(plan);

    Buffer<Colord> buffer(2, 2);
    engine.render(buffer);

    EXPECT_EQ(scene->background(), buffer[0][0]);
    EXPECT_EQ(scene->background(), buffer[1][1]);
  }

  TEST(GraphRenderEngine, EnabledTonemapPassTransformsColorResource) {
    auto scene = std::make_shared<render::Scene>();
    scene->setBackground(Colord(2.0, 4.0, 0.5));

    RenderPlan plan;
    plan.addResource(colorResource("hdr_color", RenderResourceLifetime::Transient));
    plan.addResource(colorResource("display_color", RenderResourceLifetime::Exported));

    RenderPassNode beauty;
    beauty.id = "beauty";
    beauty.kind = RenderPassKind::Beauty;
    beauty.executor = RenderExecutorKind::Raytracer;
    beauty.writes.push_back({"hdr_color"});
    plan.addPass(beauty);

    RenderPassNode tonemap;
    tonemap.id = "tonemap";
    tonemap.kind = RenderPassKind::Tonemap;
    tonemap.executor = RenderExecutorKind::PostProcess;
    tonemap.reads.push_back({"hdr_color"});
    tonemap.writes.push_back({"display_color"});
    plan.addPass(tonemap);

    GraphRenderEngine engine(camera(), scene);
    engine.setTonemap(std::make_shared<render::ReinhardTonemap>());
    engine.setPlan(plan);

    Buffer<Colord> buffer(2, 2);
    engine.render(buffer);

    ASSERT_COLOR_NEAR(Colord(2.0 / 3.0, 4.0 / 5.0, 0.5 / 1.5), buffer[0][0], 1e-12);
  }

  TEST(GraphRenderEngine, DisabledTonemapPassCanPassthroughColorResource) {
    auto scene = std::make_shared<render::Scene>();
    scene->setBackground(Colord(2.0, 4.0, 0.5));

    RenderPlan plan;
    plan.addResource(colorResource("hdr_color", RenderResourceLifetime::Transient));
    plan.addResource(colorResource("display_color", RenderResourceLifetime::Exported));

    RenderPassNode beauty;
    beauty.id = "beauty";
    beauty.kind = RenderPassKind::Beauty;
    beauty.executor = RenderExecutorKind::Raytracer;
    beauty.writes.push_back({"hdr_color"});
    plan.addPass(beauty);

    RenderPassNode tonemap;
    tonemap.id = "tonemap";
    tonemap.kind = RenderPassKind::Tonemap;
    tonemap.executor = RenderExecutorKind::PostProcess;
    tonemap.reads.push_back({"hdr_color"});
    tonemap.writes.push_back({"display_color"});
    tonemap.disabledBehavior = DisabledBehavior::Passthrough;
    tonemap.enabled = false;
    plan.addPass(tonemap);

    GraphRenderEngine engine(camera(), scene);
    engine.setTonemap(std::make_shared<render::ReinhardTonemap>());
    engine.setPlan(plan);

    Buffer<Colord> buffer(2, 2);
    engine.render(buffer);

    EXPECT_EQ(scene->background(), buffer[0][0]);
  }

  TEST(GraphRenderEngine, LdrRenderPacksGraphOutputWithoutApplyingTonemapAgain) {
    auto scene = std::make_shared<render::Scene>();
    scene->setBackground(Colord(2.0, 4.0, 0.5));

    RenderPlan plan;
    plan.addResource(colorResource("hdr_color", RenderResourceLifetime::Transient));
    plan.addResource(colorResource("display_color", RenderResourceLifetime::Exported));

    RenderPassNode beauty;
    beauty.id = "beauty";
    beauty.kind = RenderPassKind::Beauty;
    beauty.executor = RenderExecutorKind::Raytracer;
    beauty.writes.push_back({"hdr_color"});
    plan.addPass(beauty);

    RenderPassNode tonemap;
    tonemap.id = "tonemap";
    tonemap.kind = RenderPassKind::Tonemap;
    tonemap.executor = RenderExecutorKind::PostProcess;
    tonemap.reads.push_back({"hdr_color"});
    tonemap.writes.push_back({"display_color"});
    plan.addPass(tonemap);

    GraphRenderEngine engine(camera(), scene);
    engine.setTonemap(std::make_shared<render::ReinhardTonemap>());
    engine.setPlan(plan);

    Buffer<unsigned int> buffer(2, 2);
    engine.render(buffer);

    EXPECT_EQ(Colord(2.0 / 3.0, 4.0 / 5.0, 0.5 / 1.5).rgb(), buffer[0][0]);
  }

  TEST(GraphRenderEngine, LdrRenderPublishesDisplayPixelsBeforeSimpleGraphCompletes) {
    auto scene = std::make_shared<render::Scene>();
    auto material = std::make_shared<BlockingMaterial>();
    auto sphere = std::make_shared<render::Sphere>(Vector3d::null, 100.0);
    sphere->setMaterial(material);
    scene->add(sphere);

    RenderPlan plan;
    plan.addResource(colorResource("hdr_color", RenderResourceLifetime::Transient, 1, 2));
    plan.addResource(colorResource("display_color", RenderResourceLifetime::Exported, 1, 2));

    RenderPassNode beauty;
    beauty.id = "beauty";
    beauty.kind = RenderPassKind::Beauty;
    beauty.executor = RenderExecutorKind::Raytracer;
    beauty.writes.push_back({"hdr_color"});
    plan.addPass(beauty);

    RenderPassNode tonemap;
    tonemap.id = "tonemap";
    tonemap.kind = RenderPassKind::Tonemap;
    tonemap.executor = RenderExecutorKind::PostProcess;
    tonemap.reads.push_back({"hdr_color"});
    tonemap.writes.push_back({"display_color"});
    plan.addPass(tonemap);

    GraphRenderEngine engine(camera(), scene);
    engine.setPlan(plan);

    Buffer<unsigned int> buffer(1, 2);
    buffer.clear(0);

    std::thread renderThread([&] { engine.render(buffer); });
    const bool renderBlockedOnSecondPixel =
      material->waitForSecondCall(std::chrono::seconds(2));

    EXPECT_TRUE(renderBlockedOnSecondPixel);
    EXPECT_EQ(Colord(0.25, 0.5, 0.75).rgb(), buffer[0][0]);

    material->releaseSecondCall();
    renderThread.join();
  }

  TEST(GraphRenderEngine, ExecutesWireframeOverlayPassOverBeautyColor) {
    auto scene = std::make_shared<render::Scene>();
    scene->setBackground(Colord(0.1, 0.2, 0.3));
    scene->add(std::make_shared<render::Box>(Vector3d::null, Vector3d(1, 1, 1)));

    RenderIntent intent;
    intent.enableWireframeOverlay = true;
    RenderGraphCompiler compiler;

    GraphRenderEngine engine(camera(), scene);
    engine.setPlan(compiler.compile({64, 64, 1}, intent));

    Buffer<Colord> buffer(64, 64);
    engine.render(buffer);

    EXPECT_GT(countPixels(buffer, Colord::white()), 0);
    EXPECT_EQ(Colord(0.1, 0.2, 0.3), buffer[0][0]);
  }

  TEST(GraphRenderEngine, RejectsUnsupportedMultiPassPlans) {
    auto scene = std::make_shared<render::Scene>();
    GraphRenderEngine engine(camera(), scene);

    RenderPlan plan;
    RenderResourceDescriptor color;
    color.id = "main_color";
    color.type = RenderResourceType::Color;
    color.format = RenderResourceFormat::RGBDouble;
    color.width = 2;
    color.height = 2;
    color.lifetime = RenderResourceLifetime::Exported;
    plan.addResource(color);

    RenderPassNode first;
    first.id = "first";
    first.kind = RenderPassKind::Beauty;
    first.executor = RenderExecutorKind::Raytracer;
    first.writes.push_back({"main_color"});
    first.disabledBehavior = DisabledBehavior::SubstituteDefault;
    plan.addPass(first);

    RenderPassNode second;
    second.id = "second";
    second.kind = RenderPassKind::PostProcess;
    second.executor = RenderExecutorKind::PostProcess;
    second.disabledBehavior = DisabledBehavior::Passthrough;
    plan.addPass(second);

    engine.setPlan(plan);

    Buffer<Colord> buffer(2, 2);
    EXPECT_THROW(engine.render(buffer), std::runtime_error);
  }
}
