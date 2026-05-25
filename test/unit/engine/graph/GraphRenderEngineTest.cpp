#include <gtest/gtest.h>

#include "core/Buffer.h"
#include "engine/graph/GraphRenderEngine.h"
#include "engine/graph/RenderGraphArtifactCache.h"
#include "engine/graph/RenderGraphExecutionObserver.h"
#include "engine/graph/RenderGraphExecutionTrace.h"
#include "engine/graph/RenderGraphCompiler.h"
#include "engine/graph/RenderResourceStorage.h"
#include "render/cameras/PinholeCamera.h"
#include "render/lights/DirectionalLight.h"
#include "render/lights/PointLight.h"
#include "render/materials/Material.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/Box.h"
#include "render/primitives/Rectangle.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "render/textures/ConstantColorTexture.h"
#include "render/tonemap/ReinhardTonemap.h"
#include "test/helpers/ColorTestHelper.h"

#include <QString>

#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace GraphRenderEngineTest {
  using namespace engine::graph;

  std::shared_ptr<render::Camera> camera() {
    return std::make_shared<render::PinholeCamera>(Vector3d(0, 0, -5), Vector3d::null);
  }

  std::shared_ptr<render::Camera> shadowReceiverCamera() {
    return std::make_shared<render::PinholeCamera>(Vector3d(0.0, 0.0, -5.0),
                                                   Vector3d(0.0, 0.0, 0.5));
  }

  RenderResourceDescriptor colorResource(const std::string& id, RenderResourceLifetime lifetime,
                                         int width = 2, int height = 2) {
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
    Colord shade(const render::RayCaster*, const render::Scene&, const Rayd&, const HitPoint&,
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

  class RecordingObserver : public RenderGraphExecutionObserver {
  public:
    void passStarted(const RenderPassId& passId) override {
      events.push_back("start:" + passId);
    }

    void passFinished(const RenderPassId& passId) override {
      events.push_back("finish:" + passId);
    }

    void passFailed(const RenderPassId& passId, const std::string& message) override {
      events.push_back("fail:" + passId + ":" + message);
    }

    std::vector<std::string> events;
  };

  class GenerationRecordingObserver : public RecordingObserver {
  public:
    using RecordingObserver::passFailed;
    using RecordingObserver::passFinished;
    using RecordingObserver::passStarted;

    void renderStarted(std::uint64_t generation) override {
      generations.push_back(generation);
      events.push_back("render:" + std::to_string(generation));
    }

    void passStarted(const RenderPassId& passId, std::uint64_t generation) override {
      generations.push_back(generation);
      events.push_back("start:" + passId + ":" + std::to_string(generation));
    }

    void passFinished(const RenderPassId& passId, std::uint64_t generation) override {
      generations.push_back(generation);
      events.push_back("finish:" + passId + ":" + std::to_string(generation));
    }

    void passFailed(const RenderPassId& passId, const std::string& message,
                    std::uint64_t generation) override {
      generations.push_back(generation);
      events.push_back("fail:" + passId + ":" + message + ":" + std::to_string(generation));
    }

    std::vector<std::uint64_t> generations;
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

  int countDifferingPixels(const Buffer<unsigned int>& first, const Buffer<unsigned int>& second) {
    int count = 0;
    for (int y = 0; y != first.height(); ++y) {
      for (int x = 0; x != first.width(); ++x) {
        if (first[y][x] != second[y][x]) {
          ++count;
        }
      }
    }
    return count;
  }

  int countNonBlackPixels(const Buffer<Colord>& buffer) {
    int count = 0;
    for (int y = 0; y != buffer.height(); ++y) {
      for (int x = 0; x != buffer.width(); ++x) {
        if (!(buffer[y][x] == Colord::black())) {
          ++count;
        }
      }
    }
    return count;
  }

  int countNonBlackPixels(const Buffer<unsigned int>& buffer) {
    int count = 0;
    for (int y = 0; y != buffer.height(); ++y) {
      for (int x = 0; x != buffer.width(); ++x) {
        if ((buffer[y][x] & 0x00ffffff) != 0) {
          ++count;
        }
      }
    }
    return count;
  }

  int countFiniteDepths(const Buffer<double>& buffer) {
    int count = 0;
    for (int y = 0; y != buffer.height(); ++y) {
      for (int x = 0; x != buffer.width(); ++x) {
        if (std::isfinite(buffer[y][x]))
          ++count;
      }
    }
    return count;
  }

  std::shared_ptr<render::Material> matte(const Colord& color) {
    return std::make_shared<render::MatteMaterial>(
      std::make_shared<render::ConstantColorTexture>(color));
  }

  std::shared_ptr<render::Scene> highContrastScene() {
    auto scene = std::make_shared<render::Scene>();
    scene->setBackground(Colord::black());
    auto sphere = std::make_shared<render::Sphere>(Vector3d::null, 1.25);
    sphere->setMaterial(matte(Colord::white()));
    scene->add(sphere);
    return scene;
  }

  std::shared_ptr<render::Scene> directionalShadowScene() {
    auto scene = std::make_shared<render::Scene>();
    scene->setAmbient(Colord(0.1, 0.1, 0.1));
    scene->setBackground(Colord::black());

    auto wall = std::make_shared<render::Rectangle>(
      Vector3d(-2.0, -2.0, 1.0), Vector3d(0.0, 4.0, 0.0), Vector3d(4.0, 0.0, 0.0));
    wall->setMaterial(matte(Colord::white()));
    scene->add(wall);

    auto caster =
      std::make_shared<render::Box>(Vector3d(0.0, 0.0, 0.0), Vector3d(0.35, 0.35, 0.35));
    caster->setMaterial(matte(Colord::white()));
    scene->add(caster);

    scene->addLight(
      std::make_shared<render::DirectionalLight>(Vector3d(-0.5, 0.2, -1.0), Colord::white()));
    return scene;
  }

  std::shared_ptr<render::Scene> pointShadowScene() {
    auto scene = std::make_shared<render::Scene>();
    scene->setAmbient(Colord(0.1, 0.1, 0.1));
    scene->setBackground(Colord::black());

    auto wall = std::make_shared<render::Rectangle>(
      Vector3d(-2.0, -2.0, 1.0), Vector3d(0.0, 4.0, 0.0), Vector3d(4.0, 0.0, 0.0));
    wall->setMaterial(matte(Colord::white()));
    scene->add(wall);

    auto caster =
      std::make_shared<render::Box>(Vector3d(0.0, 0.0, 0.0), Vector3d(0.35, 0.35, 0.35));
    caster->setMaterial(matte(Colord::white()));
    scene->add(caster);

    scene->addLight(
      std::make_shared<render::PointLight>(Vector3d(-0.6, 0.0, -1.0), Colord::white()));
    return scene;
  }

  unsigned int colorAtWorldPoint(const Buffer<unsigned int>& buffer,
                                 const std::shared_ptr<render::Camera>& camera,
                                 const Vector3d& point) {
    const Vector2d screen = camera->projectPoint(point);
    EXPECT_TRUE(screen.isDefined()) << "world point should project into the camera view";
    if (screen.isUndefined()) {
      return 0;
    }

    const int x = static_cast<int>(std::lround(screen.x()));
    const int y = static_cast<int>(std::lround(screen.y()));
    EXPECT_GE(x, 0);
    EXPECT_LT(x, buffer.width());
    EXPECT_GE(y, 0);
    EXPECT_LT(y, buffer.height());
    if (x < 0 || x >= buffer.width() || y < 0 || y >= buffer.height()) {
      return 0;
    }
    return buffer[y][x];
  }

  unsigned int red(unsigned int color) {
    return (color >> 16) & 0xff;
  }

  void renderGraph(RenderExecutorPreference executor, RenderPostProcessAA postAA,
                   Buffer<unsigned int>& buffer) {
    RenderIntent intent;
    intent.defaultExecutor = executor;
    intent.postProcessAA = postAA;

    RenderGraphCompiler compiler;
    GraphRenderEngine engine(camera(), highContrastScene());
    engine.setPlan(compiler.compile({64, 64, 1}, intent));
    engine.render(buffer);
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

  TEST(GraphRenderEngine, ExecutesDepthAOVViewAndRecordsDepthTrace) {
    RenderIntent intent;
    intent.defaultViewMode = RenderViewMode::Depth;

    RenderGraphCompiler compiler;
    GraphRenderEngine engine(camera(), highContrastScene());
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({32, 32, 1}, intent));

    Buffer<unsigned int> buffer(32, 32);
    engine.render(buffer);

    EXPECT_GT(countNonBlackPixels(buffer), 0);
    ASSERT_EQ(2u, engine.lastPlan().passes().size());
    EXPECT_EQ("depth_aov", engine.lastPlan().passes()[0].id);
    EXPECT_EQ("visualize_depth_aov", engine.lastPlan().passes()[1].id);

    auto trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);
    const auto outputs = trace->outputSnapshotsForResource("depth_aov");
    ASSERT_EQ(1u, outputs.size());
    ASSERT_TRUE(outputs.front()->hasDepthPreview());
    EXPECT_GT(countFiniteDepths(outputs.front()->depthPreview()), 0);
  }

  TEST(GraphRenderEngine, ExecutesNormalAOVViewAndRecordsColorTrace) {
    RenderIntent intent;
    intent.defaultViewMode = RenderViewMode::Normal;

    RenderGraphCompiler compiler;
    GraphRenderEngine engine(camera(), highContrastScene());
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({32, 32, 1}, intent));

    Buffer<unsigned int> buffer(32, 32);
    engine.render(buffer);

    EXPECT_GT(countNonBlackPixels(buffer), 0);
    ASSERT_EQ(2u, engine.lastPlan().passes().size());
    EXPECT_EQ("normal_aov", engine.lastPlan().passes()[0].id);
    EXPECT_EQ("visualize_normal_aov", engine.lastPlan().passes()[1].id);

    auto trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);
    const auto outputs = trace->outputSnapshotsForResource("normal_aov");
    ASSERT_EQ(1u, outputs.size());
    ASSERT_TRUE(outputs.front()->hasColorPreview());
    EXPECT_GT(countNonBlackPixels(outputs.front()->colorPreview()), 0);
  }

  TEST(GraphRenderEngine, NotifiesObserverAroundLdrPassExecution) {
    RenderIntent intent;
    intent.postProcessAA = RenderPostProcessAA::FXAA;

    RenderGraphCompiler compiler;
    GraphRenderEngine engine(camera(), highContrastScene());
    engine.setPlan(compiler.compile({16, 16, 1}, intent));
    auto observer = std::make_shared<RecordingObserver>();
    engine.setExecutionObserver(observer);

    Buffer<unsigned int> buffer(16, 16);
    engine.render(buffer);

    const std::vector<std::string> expected = {"start:raytrace_beauty", "finish:raytrace_beauty",
                                               "start:post_fxaa",       "finish:post_fxaa",
                                               "start:tonemap",         "finish:tonemap"};
    EXPECT_EQ(expected, observer->events);
  }

  TEST(GraphRenderEngine, TagsObserverEventsWithRenderGeneration) {
    RenderIntent intent;
    intent.postProcessAA = RenderPostProcessAA::FXAA;

    RenderGraphCompiler compiler;
    GraphRenderEngine engine(camera(), highContrastScene());
    engine.setPlan(compiler.compile({16, 16, 1}, intent));
    auto observer = std::make_shared<GenerationRecordingObserver>();
    engine.setExecutionObserver(observer);

    Buffer<unsigned int> buffer(16, 16);
    engine.render(buffer);

    ASSERT_FALSE(observer->generations.empty());
    const std::uint64_t generation = observer->generations.front();
    EXPECT_GT(generation, 0u);
    for (std::uint64_t value : observer->generations) {
      EXPECT_EQ(generation, value);
    }
    ASSERT_FALSE(observer->events.empty());
    EXPECT_EQ("render:" + std::to_string(generation), observer->events.front());
  }

  TEST(GraphRenderEngine, CopiesExecutionObserverToRenderClone) {
    GraphRenderEngine engine(camera(), highContrastScene());
    auto observer = std::make_shared<RecordingObserver>();
    engine.setExecutionObserver(observer);

    auto clone = engine.cloneForRender();
    Buffer<unsigned int> buffer(8, 8);
    clone->render(buffer);

    const std::vector<std::string> expected = {"start:raytrace_beauty", "finish:raytrace_beauty",
                                               "start:tonemap", "finish:tonemap"};
    EXPECT_EQ(expected, observer->events);
  }

  TEST(GraphRenderEngine, DoesNotRecordExecutionTraceByDefault) {
    GraphRenderEngine engine(camera(), highContrastScene());

    Buffer<unsigned int> buffer(8, 8);
    engine.render(buffer);

    EXPECT_FALSE(engine.executionTraceEnabled());
    EXPECT_EQ(nullptr, engine.lastExecutionTrace());
  }

  TEST(GraphRenderEngine, RecordsColorSnapshotsInExecutionTrace) {
    RenderIntent intent;
    intent.postProcessAA = RenderPostProcessAA::FXAA;

    RenderGraphCompiler compiler;
    GraphRenderEngine engine(camera(), highContrastScene());
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({64, 64, 1}, intent));

    Buffer<unsigned int> buffer(64, 64);
    engine.render(buffer);

    auto trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);
    EXPECT_EQ(engine.lastPlan().passes().size(), trace->passes().size());

    const RenderPassTrace* postAA = trace->findPass("post_fxaa");
    ASSERT_NE(nullptr, postAA);
    EXPECT_EQ(RenderPassExecutionStatus::Completed, postAA->status());
    ASSERT_EQ(1u, postAA->inputs().size());
    ASSERT_EQ(1u, postAA->outputs().size());
    EXPECT_EQ("beauty_color", postAA->inputs()[0].resourceId());
    EXPECT_EQ("post_aa_color", postAA->outputs()[0].resourceId());
    ASSERT_TRUE(postAA->inputs()[0].hasColorPreview());
    ASSERT_TRUE(postAA->outputs()[0].hasColorPreview());
    EXPECT_EQ(64, postAA->inputs()[0].colorPreview().width());
    EXPECT_EQ(64, postAA->outputs()[0].colorPreview().height());

    ASSERT_EQ(1u, postAA->diffs().size());
    ASSERT_TRUE(postAA->diffs()[0].hasPreview());
    EXPECT_EQ("beauty_color", postAA->diffs()[0].inputResourceId());
    EXPECT_EQ("post_aa_color", postAA->diffs()[0].outputResourceId());
    EXPECT_GT(countNonBlackPixels(postAA->diffs()[0].boostedPreview()), 0);
    EXPECT_GT(postAA->elapsed().count(), 0);

    const auto inputs = trace->inputSnapshotsForResource("beauty_color");
    const auto outputs = trace->outputSnapshotsForResource("post_aa_color");
    const auto diffs = trace->diffsForResource("post_aa_color");
    ASSERT_EQ(1u, inputs.size());
    ASSERT_EQ(1u, outputs.size());
    ASSERT_EQ(2u, diffs.size());
    EXPECT_EQ("beauty_color", inputs.front()->resourceId());
    EXPECT_EQ("post_aa_color", outputs.front()->resourceId());
    EXPECT_EQ("post_aa_color", diffs.front()->outputResourceId());
    EXPECT_EQ("post_aa_color", diffs.back()->inputResourceId());
  }

  TEST(GraphRenderEngine, RejectsExecutionTraceAfterInputChange) {
    RenderGraphCompiler compiler;
    GraphRenderEngine engine(camera(), highContrastScene());
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({16, 16, 1}, RenderIntent()));

    Buffer<unsigned int> buffer(16, 16);
    engine.render(buffer);

    const RenderPlan plan = engine.lastPlan();
    auto trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);
    EXPECT_FALSE(trace->inputFingerprint().empty());
    EXPECT_TRUE(trace->matchesPlanAndInputs(plan, engine.executionInputFingerprint()));
    EXPECT_TRUE(engine.lastExecutionTraceForPlan(plan));

    engine.camera()->setPosition(Vector3d(1.0, 0.0, -5.0));

    EXPECT_FALSE(trace->matchesPlanAndInputs(plan, engine.executionInputFingerprint()));
    EXPECT_EQ(nullptr, engine.lastExecutionTraceForPlan(plan));
  }

  TEST(GraphRenderEngine, SharesExecutionTraceRecorderWithRenderClone) {
    RenderIntent intent;
    intent.postProcessAA = RenderPostProcessAA::FXAA;

    GraphRenderEngine engine(camera(), highContrastScene());
    engine.setIntent(intent);
    engine.setExecutionTraceEnabled(true);

    auto clone = engine.cloneForRender();
    Buffer<unsigned int> buffer(32, 32);
    clone->render(buffer);

    auto trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);
    EXPECT_NE(nullptr, trace->findPass("raytrace_beauty"));
    const RenderPassTrace* postAA = trace->findPass("post_fxaa");
    ASSERT_NE(nullptr, postAA);
    EXPECT_EQ(RenderPassExecutionStatus::Completed, postAA->status());
  }

  TEST(GraphRenderEngine, MaterializesDefaultLdrTraceSnapshots) {
    GraphRenderEngine engine(camera(), highContrastScene());
    engine.setExecutionTraceEnabled(true);

    Buffer<unsigned int> buffer(8, 8);
    engine.render(buffer);

    auto trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);

    const RenderPassTrace* beauty = trace->findPass("raytrace_beauty");
    ASSERT_NE(nullptr, beauty);
    EXPECT_EQ(RenderPassExecutionStatus::Completed, beauty->status());
    ASSERT_EQ(1u, beauty->outputs().size());
    EXPECT_TRUE(beauty->outputs()[0].hasColorPreview());
    EXPECT_EQ("beauty_color", beauty->outputs()[0].resourceId());

    const RenderPassTrace* tonemap = trace->findPass("tonemap");
    ASSERT_NE(nullptr, tonemap);
    EXPECT_EQ(RenderPassExecutionStatus::Completed, tonemap->status());
    ASSERT_EQ(1u, tonemap->inputs().size());
    ASSERT_EQ(1u, tonemap->outputs().size());
    EXPECT_TRUE(tonemap->inputs()[0].hasColorPreview());
    EXPECT_TRUE(tonemap->outputs()[0].hasColorPreview());
  }

  TEST(GraphRenderEngine, DisablingExecutionTraceClearsLastTrace) {
    GraphRenderEngine engine(camera(), highContrastScene());
    engine.setExecutionTraceEnabled(true);

    Buffer<unsigned int> buffer(8, 8);
    engine.render(buffer);
    ASSERT_TRUE(engine.lastExecutionTrace());

    engine.setExecutionTraceEnabled(false);

    EXPECT_EQ(nullptr, engine.lastExecutionTrace());
  }

  TEST(GraphRenderEngine, RecordsCacheMetadataInResourceSnapshots) {
    RenderPlan plan;
    plan.addResource(colorResource("cache_color", RenderResourceLifetime::PersistentCache, 2, 2));

    RenderPassNode pass;
    pass.id = "cacheable";
    pass.kind = RenderPassKind::PostProcess;
    pass.executor = RenderExecutorKind::PostProcess;
    pass.writes.push_back({"cache_color"});
    plan.addPass(pass);

    RenderResourceStorage storage;
    storage.allocate(plan.resources());

    RenderGraphExecutionTraceRecorder recorder;
    const auto session = recorder.begin(plan, "inputs");
    recorder.passStarted(session, plan.passes().front(), storage);
    recorder.passCompleted(session, plan.passes().front(), storage);
    recorder.finish(session);

    auto trace = recorder.lastTrace();
    ASSERT_TRUE(trace);
    const auto outputs = trace->outputSnapshotsForResource("cache_color");
    ASSERT_EQ(1u, outputs.size());
    EXPECT_EQ(RenderGraphCacheStatus::Uncached, outputs.front()->cacheMetadata().status());
    EXPECT_EQ(QStringLiteral("uncached"),
              outputs.front()->toJson().value("cache").toObject().value("status").toString());
  }

  TEST(GraphRenderEngine, RecordsExplicitCacheMetadataInResourceSnapshots) {
    RenderPlan plan;
    plan.addResource(colorResource("cache_color", RenderResourceLifetime::PersistentCache, 2, 2));

    RenderPassNode pass;
    pass.id = "cacheable";
    pass.kind = RenderPassKind::PostProcess;
    pass.executor = RenderExecutorKind::PostProcess;
    pass.writes.push_back({"cache_color"});
    plan.addPass(pass);

    RenderResourceStorage storage;
    storage.allocate(plan.resources());
    storage.resource("cache_color")
      .setCacheMetadata({RenderGraphCacheStatus::Hit, "restored from test cache"});

    RenderGraphExecutionTraceRecorder recorder;
    const auto session = recorder.begin(plan, "inputs");
    recorder.passStarted(session, plan.passes().front(), storage);
    recorder.passCompleted(session, plan.passes().front(), storage);
    recorder.finish(session);

    auto trace = recorder.lastTrace();
    ASSERT_TRUE(trace);
    const auto outputs = trace->outputSnapshotsForResource("cache_color");
    ASSERT_EQ(1u, outputs.size());
    EXPECT_EQ(RenderGraphCacheStatus::Hit, outputs.front()->cacheMetadata().status());
    EXPECT_EQ("restored from test cache", outputs.front()->cacheMetadata().message());
  }

  TEST(GraphRenderEngine, IgnoresRetiredExecutionTraceSessionEvents) {
    RenderPlan plan;
    plan.addResource(colorResource("main_color", RenderResourceLifetime::Exported, 2, 2));

    RenderPassNode pass;
    pass.id = "beauty";
    pass.kind = RenderPassKind::Beauty;
    pass.executor = RenderExecutorKind::Raytracer;
    pass.writes.push_back({"main_color"});
    plan.addPass(pass);

    RenderResourceStorage storage;
    storage.allocate(plan.resources());
    storage.color("main_color").clear(Colord::white());

    RenderGraphExecutionTraceRecorder recorder;
    auto retired = recorder.begin(plan);
    recorder.passStarted(retired, plan.passes().front(), storage);

    auto current = recorder.begin(plan);
    recorder.passCompleted(retired, plan.passes().front(), storage);
    recorder.finish(retired);

    auto trace = recorder.lastTrace();
    ASSERT_TRUE(trace);
    const RenderPassTrace* pending = trace->findPass("beauty");
    ASSERT_NE(nullptr, pending);
    EXPECT_EQ(RenderPassExecutionStatus::Pending, pending->status());

    recorder.passStarted(current, plan.passes().front(), storage);
    recorder.passCompleted(current, plan.passes().front(), storage);
    recorder.finish(current);

    trace = recorder.lastTrace();
    ASSERT_TRUE(trace);
    const RenderPassTrace* completed = trace->findPass("beauty");
    ASSERT_NE(nullptr, completed);
    EXPECT_EQ(RenderPassExecutionStatus::Completed, completed->status());
  }

  TEST(GraphRenderEngine, KeepsFullResolutionColorTracePreviews) {
    RenderPlan plan;
    plan.addResource(colorResource("main_color", RenderResourceLifetime::Exported, 1200, 300));

    RenderPassNode pass;
    pass.id = "beauty";
    pass.kind = RenderPassKind::Beauty;
    pass.executor = RenderExecutorKind::Raytracer;
    pass.writes.push_back({"main_color"});
    plan.addPass(pass);

    RenderResourceStorage storage;
    storage.allocate(plan.resources());
    storage.color("main_color").clear(Colord::white());

    RenderGraphExecutionTraceRecorder recorder;
    auto session = recorder.begin(plan);
    recorder.passStarted(session, plan.passes().front(), storage);
    recorder.passCompleted(session, plan.passes().front(), storage);
    recorder.finish(session);

    auto trace = recorder.lastTrace();
    ASSERT_TRUE(trace);
    const RenderPassTrace* completed = trace->findPass("beauty");
    ASSERT_NE(nullptr, completed);
    ASSERT_EQ(1u, completed->outputs().size());
    ASSERT_TRUE(completed->outputs()[0].hasColorPreview());
    EXPECT_EQ(1200, completed->outputs()[0].colorPreview().width());
    EXPECT_EQ(300, completed->outputs()[0].colorPreview().height());
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

  TEST(GraphRenderEngine, ExecutesOutOfOrderPlanByResourceDependencies) {
    auto scene = std::make_shared<render::Scene>();
    scene->setBackground(Colord(2.0, 4.0, 0.5));

    RenderPlan plan;
    plan.addResource(colorResource("hdr_color", RenderResourceLifetime::Transient));
    plan.addResource(colorResource("display_color", RenderResourceLifetime::Exported));

    RenderPassNode tonemap;
    tonemap.id = "tonemap";
    tonemap.kind = RenderPassKind::Tonemap;
    tonemap.executor = RenderExecutorKind::PostProcess;
    tonemap.reads.push_back({"hdr_color"});
    tonemap.writes.push_back({"display_color"});
    plan.addPass(tonemap);

    RenderPassNode beauty;
    beauty.id = "beauty";
    beauty.kind = RenderPassKind::Beauty;
    beauty.executor = RenderExecutorKind::Raytracer;
    beauty.writes.push_back({"hdr_color"});
    plan.addPass(beauty);

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
    const bool renderBlockedOnSecondPixel = material->waitForSecondCall(std::chrono::seconds(2));

    EXPECT_TRUE(renderBlockedOnSecondPixel);
    EXPECT_EQ(Colord(0.25, 0.5, 0.75).rgb(), buffer[0][0]);

    material->releaseSecondCall();
    renderThread.join();
  }

  TEST(GraphRenderEngine, LdrRenderPublishesDisplayPixelsBeforePostProcessGraphCompletes) {
    auto scene = std::make_shared<render::Scene>();
    auto material = std::make_shared<BlockingMaterial>();
    auto sphere = std::make_shared<render::Sphere>(Vector3d::null, 100.0);
    sphere->setMaterial(material);
    scene->add(sphere);

    RenderPlan plan;
    plan.addResource(colorResource("hdr_color", RenderResourceLifetime::Transient, 1, 2));
    plan.addResource(colorResource("post_aa_color", RenderResourceLifetime::Transient, 1, 2));
    plan.addResource(colorResource("display_color", RenderResourceLifetime::Exported, 1, 2));

    RenderPassNode beauty;
    beauty.id = "beauty";
    beauty.kind = RenderPassKind::Beauty;
    beauty.executor = RenderExecutorKind::Raytracer;
    beauty.writes.push_back({"hdr_color"});
    plan.addPass(beauty);

    RenderPassNode postAA;
    postAA.id = "post_fxaa";
    postAA.kind = RenderPassKind::PostProcess;
    postAA.executor = RenderExecutorKind::PostProcess;
    postAA.features.push_back("post_aa");
    postAA.features.push_back("fxaa");
    postAA.reads.push_back({"hdr_color"});
    postAA.writes.push_back({"post_aa_color"});
    plan.addPass(postAA);

    RenderPassNode tonemap;
    tonemap.id = "tonemap";
    tonemap.kind = RenderPassKind::Tonemap;
    tonemap.executor = RenderExecutorKind::PostProcess;
    tonemap.reads.push_back({"post_aa_color"});
    tonemap.writes.push_back({"display_color"});
    plan.addPass(tonemap);

    GraphRenderEngine engine(camera(), scene);
    engine.setPlan(plan);

    Buffer<unsigned int> buffer(1, 2);
    buffer.clear(0);

    std::thread renderThread([&] { engine.render(buffer); });
    const bool renderBlockedOnSecondPixel = material->waitForSecondCall(std::chrono::seconds(2));

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

  TEST(GraphRenderEngine, ExecutesRasterPreviewShadowPassBeforeBeauty) {
    auto scene = std::make_shared<render::Scene>();
    scene->setBackground(Colord(0.1, 0.3, 0.5));

    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.enablePreviewShadows = true;
    RenderGraphCompiler compiler;

    GraphRenderEngine engine(camera(), scene);
    engine.setPlan(compiler.compile({4, 4, 1}, intent));

    Buffer<Colord> buffer(4, 4);
    engine.render(buffer);

    ASSERT_EQ(3u, engine.lastPlan().passes().size());
    EXPECT_EQ("raster_preview_shadows", engine.lastPlan().passes()[0].id);
    EXPECT_EQ("raster_beauty", engine.lastPlan().passes()[1].id);
    EXPECT_EQ(Colord(0.1, 0.3, 0.5), buffer[0][0]);
  }

  TEST(GraphRenderEngine, RasterPreviewShadowPassMaterializesDepthTrace) {
    auto cam = shadowReceiverCamera();
    auto scene = directionalShadowScene();

    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.enablePreviewShadows = true;
    RenderGraphCompiler compiler;

    GraphRenderEngine engine(cam, scene);
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({48, 48, 1}, intent));

    Buffer<unsigned int> buffer(48, 48);
    engine.render(buffer);

    auto trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);
    const auto outputs = trace->outputSnapshotsForResource("preview_shadow_map");
    ASSERT_EQ(1u, outputs.size());
    ASSERT_TRUE(outputs.front()->hasDepthPreview());
    EXPECT_EQ(256, outputs.front()->depthPreview().width());
    EXPECT_GT(countFiniteDepths(outputs.front()->depthPreview()), 0);
    EXPECT_EQ(RenderGraphCacheStatus::Stored, outputs.front()->cacheMetadata().status());
  }

  TEST(GraphRenderEngine, RasterPreviewShadowPassReusesDepthArtifactCache) {
    auto cam = shadowReceiverCamera();
    auto scene = directionalShadowScene();

    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.enablePreviewShadows = true;
    RenderGraphCompiler compiler;

    GraphRenderEngine engine(cam, scene);
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({48, 48, 1}, intent));

    Buffer<unsigned int> buffer(48, 48);
    engine.render(buffer);
    EXPECT_EQ(1u, engine.artifactCache()->size());

    engine.setTonemap(std::make_shared<render::ReinhardTonemap>());
    engine.render(buffer);

    auto trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);
    const auto outputs = trace->outputSnapshotsForResource("preview_shadow_map");
    ASSERT_EQ(1u, outputs.size());
    EXPECT_EQ(RenderGraphCacheStatus::Hit, outputs.front()->cacheMetadata().status());
    EXPECT_GT(countFiniteDepths(outputs.front()->depthPreview()), 0);
    EXPECT_EQ(1u, engine.artifactCache()->size());

    cam->setPosition(Vector3d(0.25, 0.0, -5.0));
    engine.render(buffer);

    trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);
    const auto movedOutputs = trace->outputSnapshotsForResource("preview_shadow_map");
    ASSERT_EQ(1u, movedOutputs.size());
    EXPECT_EQ(RenderGraphCacheStatus::Stored, movedOutputs.front()->cacheMetadata().status());
    EXPECT_EQ(2u, engine.artifactCache()->size());
  }

  TEST(GraphRenderEngine, LdrRasterPreviewShadowPassDarkensOccludedReceiver) {
    auto cam = shadowReceiverCamera();
    auto scene = directionalShadowScene();

    RenderIntent directIntent;
    directIntent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    RenderIntent shadowIntent = directIntent;
    shadowIntent.enablePreviewShadows = true;

    RenderGraphCompiler compiler;
    GraphRenderEngine direct(cam, scene);
    direct.setPlan(compiler.compile({96, 96, 1}, directIntent));
    GraphRenderEngine shadowed(cam, scene);
    shadowed.setPlan(compiler.compile({96, 96, 1}, shadowIntent));

    Buffer<unsigned int> directBuffer(96, 96);
    Buffer<unsigned int> shadowBuffer(96, 96);
    direct.render(directBuffer);
    shadowed.render(shadowBuffer);
    cam->viewPlane()->setup(cam->matrix(), Recti(96, 96));

    const unsigned int directOccluded =
      colorAtWorldPoint(directBuffer, cam, Vector3d(0.6, 0.0, 1.0));
    const unsigned int shadowedOccluded =
      colorAtWorldPoint(shadowBuffer, cam, Vector3d(0.6, 0.0, 1.0));
    const unsigned int shadowedLit = colorAtWorldPoint(shadowBuffer, cam, Vector3d(-1.2, 0.0, 1.0));

    EXPECT_GT(red(directOccluded), red(shadowedOccluded) + 100u);
    EXPECT_GT(red(shadowedLit), red(shadowedOccluded) + 100u);
    EXPECT_LT(red(shadowedOccluded), 90u);
  }

  TEST(GraphRenderEngine, LdrRasterPreviewShadowsTraceLightsWithoutShadowMaps) {
    auto cam = shadowReceiverCamera();
    auto scene = pointShadowScene();

    RenderIntent directIntent;
    directIntent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    RenderIntent shadowIntent = directIntent;
    shadowIntent.enablePreviewShadows = true;

    RenderGraphCompiler compiler;
    GraphRenderEngine direct(cam, scene);
    direct.setPlan(compiler.compile({96, 96, 1}, directIntent));
    GraphRenderEngine shadowed(cam, scene);
    shadowed.setPlan(compiler.compile({96, 96, 1}, shadowIntent));

    Buffer<unsigned int> directBuffer(96, 96);
    Buffer<unsigned int> shadowBuffer(96, 96);
    direct.render(directBuffer);
    shadowed.render(shadowBuffer);
    cam->viewPlane()->setup(cam->matrix(), Recti(96, 96));

    const unsigned int directOccluded =
      colorAtWorldPoint(directBuffer, cam, Vector3d(0.6, 0.0, 1.0));
    const unsigned int shadowedOccluded =
      colorAtWorldPoint(shadowBuffer, cam, Vector3d(0.6, 0.0, 1.0));
    const unsigned int shadowedLit = colorAtWorldPoint(shadowBuffer, cam, Vector3d(-1.2, 0.0, 1.0));

    EXPECT_GT(red(directOccluded), red(shadowedOccluded) + 100u);
    EXPECT_GT(red(shadowedLit), red(shadowedOccluded) + 100u);
    EXPECT_LT(red(shadowedOccluded), 90u);
  }

  TEST(GraphRenderEngine, ExecutesRasterFxaaPostProcessPassInLdrRender) {
    Buffer<unsigned int> aliased(64, 64);
    Buffer<unsigned int> filtered(64, 64);
    renderGraph(RenderExecutorPreference::Rasterizer, RenderPostProcessAA::None, aliased);
    renderGraph(RenderExecutorPreference::Rasterizer, RenderPostProcessAA::FXAA, filtered);

    EXPECT_GT(countDifferingPixels(aliased, filtered), 0);
  }

  TEST(GraphRenderEngine, ExecutesRaytracedSmaaPostProcessPassInLdrRender) {
    Buffer<unsigned int> aliased(64, 64);
    Buffer<unsigned int> filtered(64, 64);
    renderGraph(RenderExecutorPreference::Raytracer, RenderPostProcessAA::None, aliased);
    renderGraph(RenderExecutorPreference::Raytracer, RenderPostProcessAA::SMAA, filtered);
    EXPECT_GT(countDifferingPixels(aliased, filtered), 0);
  }

  TEST(GraphRenderEngine, ExecutesWireframeSmaaPostProcessPassInLdrRender) {
    Buffer<unsigned int> aliased(64, 64);
    Buffer<unsigned int> filtered(64, 64);
    renderGraph(RenderExecutorPreference::Wireframe, RenderPostProcessAA::None, aliased);
    renderGraph(RenderExecutorPreference::Wireframe, RenderPostProcessAA::SMAA, filtered);

    EXPECT_GT(countDifferingPixels(aliased, filtered), 0);
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
