#include <gtest/gtest.h>

#include "core/Buffer.h"
#include "engine/graph/GraphRenderEngine.h"
#include "engine/graph/RenderGraphArtifactCache.h"
#include "engine/graph/RenderGraphExecutionObserver.h"
#include "engine/graph/RenderGraphExecutionTrace.h"
#include "engine/graph/RenderGraphCompiler.h"
#include "engine/graph/RenderResourceStorage.h"
#include "engine/raster/RasterBackend.h"
#include "engine/raster/RasterVisibilitySceneCache.h"
#include "engine/raster/detail/RasterShadowMapBuilder.h"
#include "render/cameras/PinholeCamera.h"
#include "render/lights/DirectionalLight.h"
#include "render/lights/PointLight.h"
#include "render/materials/Material.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/Box.h"
#include "render/primitives/Curve.h"
#include "render/primitives/Rectangle.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "render/primitives/Triangle.h"
#include "render/textures/ConstantColorTexture.h"
#include "render/tonemap/ReinhardTonemap.h"
#include "test/helpers/ColorTestHelper.h"

#include <QJsonArray>
#include <QJsonObject>
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

  RenderResourceDescriptor depthResource(const std::string& id, RenderResourceLifetime lifetime,
                                         int width = 2, int height = 2) {
    RenderResourceDescriptor depth;
    depth.id = id;
    depth.type = RenderResourceType::Depth;
    depth.format = RenderResourceFormat::DepthDouble;
    depth.width = width;
    depth.height = height;
    depth.lifetime = lifetime;
    return depth;
  }

  RenderResourceDescriptor objectIdResource(const std::string& id, RenderResourceLifetime lifetime,
                                            int width = 2, int height = 2) {
    RenderResourceDescriptor objectId;
    objectId.id = id;
    objectId.type = RenderResourceType::ObjectId;
    objectId.format = RenderResourceFormat::UInt32;
    objectId.width = width;
    objectId.height = height;
    objectId.lifetime = lifetime;
    return objectId;
  }

  RenderResourceDescriptor stencilResource(const std::string& id, RenderResourceLifetime lifetime,
                                           int width = 2, int height = 2) {
    RenderResourceDescriptor stencil;
    stencil.id = id;
    stencil.type = RenderResourceType::Stencil;
    stencil.format = RenderResourceFormat::UInt8;
    stencil.width = width;
    stencil.height = height;
    stencil.lifetime = lifetime;
    return stencil;
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

  class BlockingObserver : public RenderGraphExecutionObserver {
  public:
    void renderStarted(std::uint64_t) override {
    }

    void passStarted(const RenderPassId& passId) override {
      passStarted(passId, 0);
    }

    void passFinished(const RenderPassId& passId) override {
      passFinished(passId, 0);
    }

    void passFailed(const RenderPassId& passId, const std::string& message) override {
      passFailed(passId, message, 0);
    }

    void passStarted(const RenderPassId& passId, std::uint64_t) override {
      {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_events.push_back("start:" + passId);
      }
      m_changed.notify_all();
    }

    void passFinished(const RenderPassId& passId, std::uint64_t) override {
      {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_events.push_back("finish:" + passId);
      }
      m_changed.notify_all();
    }

    void passFailed(const RenderPassId& passId, const std::string& message,
                    std::uint64_t) override {
      {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_events.push_back("fail:" + passId + ":" + message);
      }
      m_changed.notify_all();
    }

    bool waitForEvent(const std::string& event, std::chrono::milliseconds timeout) {
      std::unique_lock<std::mutex> lock(m_mutex);
      return m_changed.wait_for(lock, timeout, [&] { return hasEventLocked(event); });
    }

    bool waitForStartedCount(int count, std::chrono::milliseconds timeout) {
      std::unique_lock<std::mutex> lock(m_mutex);
      return m_changed.wait_for(lock, timeout, [&] {
        return std::count_if(m_events.begin(), m_events.end(), [](const std::string& event) {
                 return event.find("start:") == 0;
               }) >= count;
      });
    }

    std::vector<std::string> events() const {
      std::lock_guard<std::mutex> lock(m_mutex);
      return m_events;
    }

  private:
    bool hasEventLocked(const std::string& event) const {
      return std::find(m_events.begin(), m_events.end(), event) != m_events.end();
    }

    mutable std::mutex m_mutex;
    std::condition_variable m_changed;
    std::vector<std::string> m_events;
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

  int countDifferingFiniteDepths(const Buffer<double>& first, const Buffer<double>& second,
                                 double epsilon) {
    int count = 0;
    for (int y = 0; y != first.height(); ++y) {
      for (int x = 0; x != first.width(); ++x) {
        if (std::isfinite(first[y][x]) && std::isfinite(second[y][x]) &&
            std::abs(first[y][x] - second[y][x]) > epsilon) {
          ++count;
        }
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

  std::shared_ptr<render::Scene> singleRectangleScene() {
    auto scene = std::make_shared<render::Scene>();
    scene->setBackground(Colord::black());
    auto rectangle = std::make_shared<render::Rectangle>(
      Vector3d(-1.0, -1.0, 0.0), Vector3d(0.0, 2.0, 0.0), Vector3d(2.0, 0.0, 0.0));
    rectangle->setMaterial(matte(Colord::white()));
    scene->add(rectangle);
    return scene;
  }

  std::shared_ptr<render::Scene> visibleAndOffscreenBoxScene() {
    auto scene = std::make_shared<render::Scene>();
    scene->setBackground(Colord::black());

    auto visible = std::make_shared<render::Box>(Vector3d(0.0, 0.0, 0.0), Vector3d(0.5, 0.5, 0.5));
    visible->setMaterial(matte(Colord::white()));
    scene->add(visible);

    auto offscreen =
      std::make_shared<render::Box>(Vector3d(1000.0, 0.0, 0.0), Vector3d(0.5, 0.5, 0.5));
    offscreen->setMaterial(matte(Colord::white()));
    scene->add(offscreen);
    return scene;
  }

  std::shared_ptr<render::Scene> partiallyClippedBoxScene() {
    auto scene = std::make_shared<render::Scene>();
    scene->setBackground(Colord::black());

    auto clipped = std::make_shared<render::Rectangle>(
      Vector3d(-1.0, -1.0, 0.0), Vector3d(0.0, 2.0, 0.0), Vector3d(100.0, 0.0, 0.0));
    clipped->setMaterial(matte(Colord::white()));
    scene->add(clipped);
    return scene;
  }

  std::shared_ptr<render::Scene> twoVisibleDepthBoxScene() {
    auto scene = std::make_shared<render::Scene>();
    scene->setBackground(Colord::black());

    auto nearBox =
      std::make_shared<render::Box>(Vector3d(-0.75, 0.0, 0.0), Vector3d(0.35, 0.35, 0.35));
    nearBox->setMaterial(matte(Colord::white()));
    scene->add(nearBox);

    auto farBox =
      std::make_shared<render::Box>(Vector3d(0.75, 0.0, 1.0), Vector3d(0.35, 0.35, 0.35));
    farBox->setMaterial(matte(Colord::white()));
    scene->add(farBox);
    return scene;
  }

  std::shared_ptr<render::Scene> frontAndBackFacingTriangleScene() {
    auto scene = std::make_shared<render::Scene>();
    scene->setBackground(Colord::black());

    auto front = std::make_shared<render::Triangle>(
      Vector3d(-1.5, -1.0, 0.0), Vector3d(-0.5, 1.0, 0.0), Vector3d(0.5, -1.0, 0.0));
    front->setMaterial(matte(Colord::white()));
    scene->add(front);

    auto back = std::make_shared<render::Triangle>(
      Vector3d(-0.5, -1.0, 0.0), Vector3d(1.5, -1.0, 0.0), Vector3d(0.5, 1.0, 0.0));
    back->setMaterial(matte(Colord::white()));
    scene->add(back);
    return scene;
  }

  std::shared_ptr<render::Scene>
  frontAndBackFacingTriangleScene(render::Material::Sidedness sidedness) {
    auto scene = frontAndBackFacingTriangleScene();
    scene->forEachLeaf(
      [sidedness](const render::Primitive*, std::shared_ptr<render::Material> material) {
        if (material) {
          material->setSidedness(sidedness);
        }
      });
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

  TEST(GraphRenderEngine, ExecutesWavefrontBeautyPass) {
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Wavefront;

    RenderGraphCompiler compiler;
    GraphRenderEngine engine(camera(), highContrastScene());
    engine.setPlan(compiler.compile({32, 32, 1}, intent));

    Buffer<unsigned int> buffer(32, 32);
    engine.render(buffer);

    EXPECT_GT(countNonBlackPixels(buffer), 0);
    ASSERT_EQ(2u, engine.lastPlan().passes().size());
    EXPECT_EQ(RenderExecutorKind::Wavefront, engine.lastPlan().passes()[0].executor);
    EXPECT_EQ("wavefront_beauty", engine.lastPlan().passes()[0].id);
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

  TEST(GraphRenderEngine, ExecutesSampleStddevAOVViewAndRecordsColorTrace) {
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::PathTracer;
    intent.defaultViewMode = RenderViewMode::SampleStddev;
    intent.engineOptions.raytracer().setSamplesPerPixel(2);

    RenderGraphCompiler compiler;
    GraphRenderEngine engine(camera(), highContrastScene());
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({32, 32, 1}, intent));

    Buffer<unsigned int> buffer(32, 32);
    engine.render(buffer);

    ASSERT_EQ(2u, engine.lastPlan().passes().size());
    EXPECT_EQ("sample_stddev_aov", engine.lastPlan().passes()[0].id);
    EXPECT_EQ("visualize_sample_stddev_aov", engine.lastPlan().passes()[1].id);

    auto trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);
    const auto aovOutputs = trace->outputSnapshotsForResource("sample_stddev_aov");
    ASSERT_EQ(1u, aovOutputs.size());
    ASSERT_TRUE(aovOutputs.front()->hasColorPreview());

    const auto mainOutputs = trace->outputSnapshotsForResource("main_color");
    ASSERT_EQ(1u, mainOutputs.size());
    ASSERT_TRUE(mainOutputs.front()->hasColorPreview());
  }

  TEST(GraphRenderEngine, ExecutesSampleStddevColorAOVViewAndRecordsColorTrace) {
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::PathTracer;
    intent.defaultViewMode = RenderViewMode::SampleStddevColor;
    intent.engineOptions.raytracer().setSamplesPerPixel(2);

    RenderGraphCompiler compiler;
    GraphRenderEngine engine(camera(), highContrastScene());
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({32, 32, 1}, intent));

    Buffer<unsigned int> buffer(32, 32);
    engine.render(buffer);

    ASSERT_EQ(2u, engine.lastPlan().passes().size());
    EXPECT_EQ("sample_stddev_color_aov", engine.lastPlan().passes()[0].id);
    EXPECT_EQ("visualize_sample_stddev_color_aov", engine.lastPlan().passes()[1].id);

    auto trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);
    const auto aovOutputs = trace->outputSnapshotsForResource("sample_stddev_color_aov");
    ASSERT_EQ(1u, aovOutputs.size());
    ASSERT_TRUE(aovOutputs.front()->hasColorPreview());

    const auto mainOutputs = trace->outputSnapshotsForResource("main_color");
    ASSERT_EQ(1u, mainOutputs.size());
    ASSERT_TRUE(mainOutputs.front()->hasColorPreview());
  }

  TEST(GraphRenderEngine, RejectsExplicitPlanWithMultipleSceneCameras) {
    RenderPlan plan;
    plan.addResource(colorResource("beauty_color", RenderResourceLifetime::Transient));
    plan.addResource(colorResource("main_color", RenderResourceLifetime::Exported));

    RenderPassNode beauty;
    beauty.id = "beauty";
    beauty.kind = RenderPassKind::Beauty;
    beauty.executor = RenderExecutorKind::Raytracer;
    beauty.sceneView.camera = RenderCameraRef{"first-camera", std::nullopt};
    beauty.addWrite("beauty_color");
    plan.addPass(beauty);

    RenderPassNode tonemap;
    tonemap.id = "tonemap";
    tonemap.kind = RenderPassKind::Tonemap;
    tonemap.executor = RenderExecutorKind::PostProcess;
    tonemap.sceneView.camera = RenderCameraRef{"second-camera", std::nullopt};
    tonemap.addRead("beauty_color");
    tonemap.addWrite("main_color");
    plan.addPass(tonemap);

    GraphRenderEngine engine(camera(), highContrastScene());
    engine.setPlan(plan);
    Buffer<unsigned int> buffer(2, 2);

    try {
      engine.render(buffer);
      FAIL() << "expected multiple scene camera rejection";
    } catch (const std::runtime_error& error) {
      EXPECT_NE(std::string::npos, std::string(error.what()).find("multiple scene camera"));
    }
  }

  TEST(GraphRenderEngine, ExecutesPassWithBoundSceneCamera) {
    RenderPlan plan;
    plan.addResource(colorResource("beauty_color", RenderResourceLifetime::Transient, 16, 16));
    plan.addResource(colorResource("main_color", RenderResourceLifetime::Exported, 16, 16));

    RenderPassNode beauty;
    beauty.id = "beauty";
    beauty.kind = RenderPassKind::Beauty;
    beauty.executor = RenderExecutorKind::Raytracer;
    beauty.sceneView.camera = RenderCameraRef{"object-camera", std::nullopt};
    beauty.addWrite("beauty_color");
    plan.addPass(beauty);

    RenderPassNode tonemap;
    tonemap.id = "tonemap";
    tonemap.kind = RenderPassKind::Tonemap;
    tonemap.executor = RenderExecutorKind::PostProcess;
    tonemap.addRead("beauty_color");
    tonemap.addWrite("main_color");
    plan.addPass(tonemap);

    auto awayCamera =
      std::make_shared<render::PinholeCamera>(Vector3d(0.0, 0.0, -5.0), Vector3d(10.0, 0.0, 0.0));
    auto objectCamera = camera();
    GraphRenderEngine engine(awayCamera, highContrastScene());
    engine.setSceneCamera("object-camera", objectCamera);
    engine.setPlan(plan);

    Buffer<unsigned int> buffer(16, 16);
    engine.render(buffer);

    EXPECT_GT(countNonBlackPixels(buffer), 0);
  }

  TEST(GraphRenderEngine, ExecutesRasterVisibilityCullingBaseline) {
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.engineOptions.rasterizer().setVisibilityCulling(RenderVisibilityCulling::On);

    RenderGraphCompiler compiler;
    GraphRenderEngine engine(camera(), highContrastScene());
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({32, 32, 1}, intent));

    Buffer<unsigned int> buffer(32, 32);
    engine.render(buffer);

    EXPECT_GT(countNonBlackPixels(buffer), 0);
    const auto* visibility = engine.lastPlan().findPass("raster_visibility");
    ASSERT_NE(nullptr, visibility);
    EXPECT_EQ(RenderPassKind::Visibility, visibility->kind);

    auto trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);
    const RenderPassTrace* visibilityTrace = trace->findPass("raster_visibility");
    ASSERT_NE(nullptr, visibilityTrace);
    EXPECT_EQ(RenderPassExecutionStatus::Completed, visibilityTrace->status());
    EXPECT_NE(std::string::npos, visibilityTrace->message().find("CPU visibility set"));
    EXPECT_NE(std::string::npos, visibilityTrace->message().find("cache=stored"));
    EXPECT_NE(std::string::npos, visibilityTrace->message().find("lod=0"));
    EXPECT_NE(std::string::npos, visibilityTrace->message().find("inputLeaves=1"));
    EXPECT_NE(std::string::npos, visibilityTrace->message().find("inputTriangles="));
    EXPECT_NE(std::string::npos, visibilityTrace->message().find("visibleLeaves=1"));
    EXPECT_NE(std::string::npos, visibilityTrace->message().find("rejectedLeaves=0"));
  }

  TEST(GraphRenderEngine, RecordsRasterVisibilityFrustumRejectedMetrics) {
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.engineOptions.rasterizer().setVisibilityCulling(RenderVisibilityCulling::On);

    RenderGraphCompiler compiler;
    GraphRenderEngine engine(camera(), visibleAndOffscreenBoxScene());
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({32, 32, 1}, intent));

    Buffer<unsigned int> buffer(32, 32);
    engine.render(buffer);

    EXPECT_GT(countNonBlackPixels(buffer), 0);
    auto trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);
    const RenderPassTrace* visibilityTrace = trace->findPass("raster_visibility");
    ASSERT_NE(nullptr, visibilityTrace);
    const std::string& message = visibilityTrace->message();
    EXPECT_NE(std::string::npos, message.find("inputLeaves=2")) << message;
    EXPECT_NE(std::string::npos, message.find("visibleLeaves=1")) << message;
    EXPECT_NE(std::string::npos, message.find("rejectedLeaves=1")) << message;
    EXPECT_NE(std::string::npos, message.find("frustumRejectedLeaves=1")) << message;
    EXPECT_NE(std::string::npos, message.find("backfaceRejectedLeaves=0")) << message;
    EXPECT_NE(std::string::npos, message.find("tileGrid=1x1")) << message;
    EXPECT_NE(std::string::npos, message.find("visibleTileReferences=1")) << message;
    EXPECT_NE(std::string::npos, message.find("uncertainTileLeaves=0")) << message;
    EXPECT_NE(std::string::npos, message.find("depthSummarizedTiles=1")) << message;
    EXPECT_NE(std::string::npos, message.find("tileDepthReferences=1")) << message;

    const auto outputs = trace->outputSnapshotsForResource("raster_visibility_set");
    ASSERT_EQ(1u, outputs.size());
    EXPECT_TRUE(outputs.front()->hasColorPreview());
    EXPECT_EQ(32, outputs.front()->colorPreview().width());
    EXPECT_EQ(32, outputs.front()->colorPreview().height());
    EXPECT_NE(std::string::npos,
              outputs.front()->unavailableReason().find("visibility set tile preview"));
    EXPECT_NE(std::string::npos,
              outputs.front()->unavailableReason().find("frustumRejectedLeaves=1"));
    EXPECT_NE(std::string::npos, outputs.front()->unavailableReason().find("tileGrid=1x1"));
  }

  TEST(GraphRenderEngine, RasterVisibilityCullingReusesVisibilitySetArtifactCache) {
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.engineOptions.rasterizer().setVisibilityCulling(RenderVisibilityCulling::On);

    auto cam = camera();
    RenderGraphCompiler compiler;
    GraphRenderEngine engine(cam, visibleAndOffscreenBoxScene());
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({32, 32, 1}, intent));

    Buffer<unsigned int> buffer(32, 32);
    engine.render(buffer);

    auto trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);
    const RenderPassTrace* visibilityTrace = trace->findPass("raster_visibility");
    ASSERT_NE(nullptr, visibilityTrace);
    EXPECT_NE(std::string::npos, visibilityTrace->message().find("cache=stored"));
    EXPECT_NE(std::string::npos, visibilityTrace->message().find("meshCacheHits=0"));
    EXPECT_NE(std::string::npos, visibilityTrace->message().find("meshCacheMisses=2"));
    EXPECT_NE(std::string::npos, visibilityTrace->message().find("boundsCacheHits=0"));
    EXPECT_NE(std::string::npos, visibilityTrace->message().find("boundsCacheMisses=2"));
    EXPECT_NE(std::string::npos,
              visibilityTrace->message().find("materialCullabilityCacheMisses=1"));
    auto outputs = trace->outputSnapshotsForResource("raster_visibility_set");
    ASSERT_EQ(1u, outputs.size());
    EXPECT_EQ(RenderGraphCacheStatus::Stored, outputs.front()->cacheMetadata().status());
    EXPECT_EQ(1u, engine.artifactCache()->size());
    EXPECT_EQ(2u, engine.rasterVisibilitySceneCache()->size());
    EXPECT_EQ(2u, engine.rasterVisibilitySceneCache()->transformedBoundsSize());
    EXPECT_EQ(1u, engine.rasterVisibilitySceneCache()->materialCullabilitySize());

    engine.setTonemap(std::make_shared<render::ReinhardTonemap>());
    engine.render(buffer);

    trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);
    visibilityTrace = trace->findPass("raster_visibility");
    ASSERT_NE(nullptr, visibilityTrace);
    EXPECT_NE(std::string::npos, visibilityTrace->message().find("cache=hit"));
    EXPECT_NE(std::string::npos, visibilityTrace->message().find("meshCacheHits=0"));
    EXPECT_NE(std::string::npos, visibilityTrace->message().find("meshCacheMisses=0"));
    EXPECT_NE(std::string::npos, visibilityTrace->message().find("boundsCacheHits=0"));
    EXPECT_NE(std::string::npos, visibilityTrace->message().find("boundsCacheMisses=0"));
    EXPECT_NE(std::string::npos, visibilityTrace->message().find("materialCullabilityCacheHits=0"));
    EXPECT_NE(std::string::npos,
              visibilityTrace->message().find("materialCullabilityCacheMisses=0"));
    outputs = trace->outputSnapshotsForResource("raster_visibility_set");
    ASSERT_EQ(1u, outputs.size());
    EXPECT_EQ(RenderGraphCacheStatus::Hit, outputs.front()->cacheMetadata().status());
    EXPECT_EQ(1u, engine.artifactCache()->size());
    EXPECT_EQ(2u, engine.rasterVisibilitySceneCache()->size());
    EXPECT_EQ(2u, engine.rasterVisibilitySceneCache()->transformedBoundsSize());
    EXPECT_EQ(1u, engine.rasterVisibilitySceneCache()->materialCullabilitySize());

    cam->setPosition(Vector3d(0.25, 0.0, -5.0));
    engine.render(buffer);

    trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);
    visibilityTrace = trace->findPass("raster_visibility");
    ASSERT_NE(nullptr, visibilityTrace);
    EXPECT_NE(std::string::npos, visibilityTrace->message().find("cache=stored"));
    EXPECT_NE(std::string::npos, visibilityTrace->message().find("meshCacheHits=2"));
    EXPECT_NE(std::string::npos, visibilityTrace->message().find("meshCacheMisses=0"));
    EXPECT_NE(std::string::npos, visibilityTrace->message().find("boundsCacheHits=2"));
    EXPECT_NE(std::string::npos, visibilityTrace->message().find("boundsCacheMisses=0"));
    EXPECT_NE(std::string::npos, visibilityTrace->message().find("materialCullabilityCacheHits=1"));
    outputs = trace->outputSnapshotsForResource("raster_visibility_set");
    ASSERT_EQ(1u, outputs.size());
    EXPECT_EQ(RenderGraphCacheStatus::Stored, outputs.front()->cacheMetadata().status());
    EXPECT_EQ(2u, engine.artifactCache()->size());
    EXPECT_EQ(2u, engine.rasterVisibilitySceneCache()->size());
    EXPECT_EQ(2u, engine.rasterVisibilitySceneCache()->transformedBoundsSize());
    EXPECT_EQ(1u, engine.rasterVisibilitySceneCache()->materialCullabilitySize());
  }

  TEST(GraphRenderEngine, KeepsPartiallyClippedVisibilityLeavesUncertain) {
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.engineOptions.rasterizer().setVisibilityCulling(RenderVisibilityCulling::On);

    RenderGraphCompiler compiler;
    GraphRenderEngine engine(camera(), partiallyClippedBoxScene());
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({32, 32, 1}, intent));

    Buffer<unsigned int> buffer(32, 32);
    engine.render(buffer);

    EXPECT_GT(countNonBlackPixels(buffer), 0);
    auto trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);
    const RenderPassTrace* visibilityTrace = trace->findPass("raster_visibility");
    ASSERT_NE(nullptr, visibilityTrace);
    const std::string& message = visibilityTrace->message();
    EXPECT_NE(std::string::npos, message.find("visibleLeaves=1")) << message;
    EXPECT_NE(std::string::npos, message.find("rejectedLeaves=0")) << message;
    EXPECT_NE(std::string::npos, message.find("uncertainTileLeaves=1")) << message;
    EXPECT_NE(std::string::npos, message.find("depthSummarizedTiles=0")) << message;
  }

  TEST(GraphRenderEngine, RecordsRasterVisibilityFrontToBackOrderingMetrics) {
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.engineOptions.rasterizer().setVisibilityCulling(RenderVisibilityCulling::On);

    RenderGraphCompiler compiler;
    GraphRenderEngine engine(camera(), twoVisibleDepthBoxScene());
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({32, 32, 1}, intent));

    Buffer<unsigned int> buffer(32, 32);
    engine.render(buffer);

    EXPECT_GT(countNonBlackPixels(buffer), 0);
    auto trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);
    const RenderPassTrace* visibilityTrace = trace->findPass("raster_visibility");
    ASSERT_NE(nullptr, visibilityTrace);
    const std::string& message = visibilityTrace->message();
    EXPECT_NE(std::string::npos, message.find("visibleLeaves=2")) << message;
    EXPECT_NE(std::string::npos, message.find("frontToBackOrdering=enabled")) << message;
    EXPECT_NE(std::string::npos, message.find("frontToBackOrderedLeaves=2")) << message;
  }

  TEST(GraphRenderEngine, SkipsRasterVisibilityTileDepthSummariesForOrderDependentState) {
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.engineOptions.rasterizer().setVisibilityCulling(RenderVisibilityCulling::On);
    intent.engineOptions.rasterizer().setBlendingEnabled(true);

    RenderGraphCompiler compiler;
    GraphRenderEngine engine(camera(), visibleAndOffscreenBoxScene());
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({32, 32, 1}, intent));

    Buffer<unsigned int> buffer(32, 32);
    engine.render(buffer);

    auto trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);
    const RenderPassTrace* visibilityTrace = trace->findPass("raster_visibility");
    ASSERT_NE(nullptr, visibilityTrace);
    const std::string& message = visibilityTrace->message();
    EXPECT_NE(std::string::npos, message.find("frontToBackOrdering=disabled")) << message;
    EXPECT_NE(std::string::npos, message.find("visibleTileReferences=1")) << message;
    EXPECT_NE(std::string::npos, message.find("depthSummarizedTiles=0")) << message;
    EXPECT_NE(std::string::npos, message.find("tileDepthReferences=0")) << message;
  }

  TEST(GraphRenderEngine, RecordsRasterVisibilityBackfaceRejectedMetrics) {
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.engineOptions.rasterizer().setVisibilityCulling(RenderVisibilityCulling::On);
    intent.engineOptions.rasterizer().setCullMode("back");

    RenderGraphCompiler compiler;
    GraphRenderEngine engine(camera(), frontAndBackFacingTriangleScene());
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({32, 32, 1}, intent));

    Buffer<unsigned int> buffer(32, 32);
    engine.render(buffer);

    EXPECT_GT(countNonBlackPixels(buffer), 0);
    auto trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);
    const RenderPassTrace* visibilityTrace = trace->findPass("raster_visibility");
    ASSERT_NE(nullptr, visibilityTrace);
    const std::string& message = visibilityTrace->message();
    EXPECT_NE(std::string::npos, message.find("inputLeaves=2")) << message;
    EXPECT_NE(std::string::npos, message.find("visibleLeaves=1")) << message;
    EXPECT_NE(std::string::npos, message.find("backfaceRejectedLeaves=1")) << message;
    EXPECT_NE(std::string::npos, message.find("backfaceRejectedTriangles=1")) << message;
  }

  TEST(GraphRenderEngine, RasterVisibilityUsesMaterialSidednessForBackfaceRejection) {
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.engineOptions.rasterizer().setVisibilityCulling(RenderVisibilityCulling::On);

    RenderGraphCompiler compiler;
    GraphRenderEngine engine(camera(),
                             frontAndBackFacingTriangleScene(render::Material::Sidedness::Front));
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({32, 32, 1}, intent));

    Buffer<unsigned int> buffer(32, 32);
    engine.render(buffer);

    EXPECT_GT(countNonBlackPixels(buffer), 0);
    auto trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);
    const RenderPassTrace* visibilityTrace = trace->findPass("raster_visibility");
    ASSERT_NE(nullptr, visibilityTrace);
    const std::string& message = visibilityTrace->message();
    EXPECT_NE(std::string::npos, message.find("inputLeaves=2")) << message;
    EXPECT_NE(std::string::npos, message.find("visibleLeaves=1")) << message;
    EXPECT_NE(std::string::npos, message.find("backfaceRejectedLeaves=1")) << message;
    EXPECT_NE(std::string::npos, message.find("backfaceRejectedTriangles=1")) << message;
  }

  TEST(GraphRenderEngine, RasterVisibilityKeepsTwoSidedMaterialBackfacesVisible) {
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.engineOptions.rasterizer().setVisibilityCulling(RenderVisibilityCulling::On);

    RenderGraphCompiler compiler;
    GraphRenderEngine engine(
      camera(), frontAndBackFacingTriangleScene(render::Material::Sidedness::TwoSided));
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({32, 32, 1}, intent));

    Buffer<unsigned int> buffer(32, 32);
    engine.render(buffer);

    auto trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);
    const RenderPassTrace* visibilityTrace = trace->findPass("raster_visibility");
    ASSERT_NE(nullptr, visibilityTrace);
    const std::string& message = visibilityTrace->message();
    EXPECT_NE(std::string::npos, message.find("visibleLeaves=2")) << message;
    EXPECT_NE(std::string::npos, message.find("backfaceRejectedLeaves=0")) << message;
  }

  TEST(GraphRenderEngine, RasterVisibilityCullingPreservesOpaqueOutput) {
    RenderIntent fullIntent;
    fullIntent.defaultExecutor = RenderExecutorPreference::Rasterizer;

    RenderIntent culledIntent = fullIntent;
    culledIntent.engineOptions.rasterizer().setVisibilityCulling(RenderVisibilityCulling::On);

    RenderGraphCompiler compiler;
    const auto scene = visibleAndOffscreenBoxScene();

    Buffer<unsigned int> full(32, 32);
    GraphRenderEngine fullEngine(camera(), scene);
    fullEngine.setPlan(compiler.compile({32, 32, 1}, fullIntent));
    fullEngine.render(full);

    Buffer<unsigned int> culled(32, 32);
    GraphRenderEngine culledEngine(camera(), scene);
    culledEngine.setPlan(compiler.compile({32, 32, 1}, culledIntent));
    culledEngine.render(culled);

    EXPECT_EQ(0, countDifferingPixels(full, culled));
  }

  TEST(GraphRenderEngine, ExecutesStencilAOVViewAndRecordsColorTrace) {
    RenderIntent intent;
    intent.defaultViewMode = RenderViewMode::Stencil;

    RenderGraphCompiler compiler;
    GraphRenderEngine engine(camera(), highContrastScene());
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({32, 32, 1}, intent));

    Buffer<unsigned int> buffer(32, 32);
    engine.render(buffer);

    EXPECT_GT(countNonBlackPixels(buffer), 0);
    ASSERT_EQ(2u, engine.lastPlan().passes().size());
    EXPECT_EQ("stencil_aov", engine.lastPlan().passes()[0].id);
    EXPECT_EQ("visualize_stencil_aov", engine.lastPlan().passes()[1].id);

    auto trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);
    const auto outputs = trace->outputSnapshotsForResource("stencil_aov");
    ASSERT_EQ(1u, outputs.size());
    ASSERT_TRUE(outputs.front()->hasColorPreview());
    EXPECT_GT(countNonBlackPixels(outputs.front()->colorPreview()), 0);
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

  TEST(GraphRenderEngine, ExecutesObjectIdAOVViewAndRecordsColorTrace) {
    RenderIntent intent;
    intent.defaultViewMode = RenderViewMode::ObjectId;

    RenderGraphCompiler compiler;
    GraphRenderEngine engine(camera(), highContrastScene());
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({32, 32, 1}, intent));

    Buffer<unsigned int> buffer(32, 32);
    engine.render(buffer);

    EXPECT_GT(countNonBlackPixels(buffer), 0);
    ASSERT_EQ(2u, engine.lastPlan().passes().size());
    EXPECT_EQ("object_id_aov", engine.lastPlan().passes()[0].id);
    EXPECT_EQ("visualize_object_id_aov", engine.lastPlan().passes()[1].id);

    auto trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);
    const auto outputs = trace->outputSnapshotsForResource("object_id_aov");
    ASSERT_EQ(1u, outputs.size());
    ASSERT_TRUE(outputs.front()->hasColorPreview());
    EXPECT_GT(countNonBlackPixels(outputs.front()->colorPreview()), 0);
  }

  TEST(GraphRenderEngine, ExecutesMaterialIdAOVViewAndRecordsColorTrace) {
    RenderIntent intent;
    intent.defaultViewMode = RenderViewMode::MaterialId;

    RenderGraphCompiler compiler;
    GraphRenderEngine engine(camera(), highContrastScene());
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({32, 32, 1}, intent));

    Buffer<unsigned int> buffer(32, 32);
    engine.render(buffer);

    EXPECT_GT(countNonBlackPixels(buffer), 0);
    ASSERT_EQ(2u, engine.lastPlan().passes().size());
    EXPECT_EQ("material_id_aov", engine.lastPlan().passes()[0].id);
    EXPECT_EQ("visualize_material_id_aov", engine.lastPlan().passes()[1].id);

    auto trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);
    const auto outputs = trace->outputSnapshotsForResource("material_id_aov");
    ASSERT_EQ(1u, outputs.size());
    ASSERT_TRUE(outputs.front()->hasColorPreview());
    EXPECT_GT(countNonBlackPixels(outputs.front()->colorPreview()), 0);
  }

  TEST(GraphRenderEngine, RecordsOpenGLIdAOVDiagnosticFallbackTraceMessages) {
    RenderGraphCompiler compiler;

    RenderIntent objectIntent;
    objectIntent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    objectIntent.defaultViewMode = RenderViewMode::ObjectId;
    objectIntent.engineOptions.rasterizer().setBackend(engine::raster::RasterBackend::openGL());
    GraphRenderEngine objectEngine(camera(), highContrastScene());
    objectEngine.setExecutionTraceEnabled(true);
    objectEngine.setPlan(compiler.compile({32, 32, 1}, objectIntent));

    Buffer<unsigned int> objectBuffer(32, 32);
    objectEngine.render(objectBuffer);

    auto objectTrace = objectEngine.lastExecutionTrace();
    ASSERT_TRUE(objectTrace);
    const RenderPassTrace* objectId = objectTrace->findPass("object_id_aov");
    ASSERT_NE(nullptr, objectId);
    EXPECT_NE(objectId->message().find("software raster diagnostic fallback"), std::string::npos);

    RenderIntent materialIntent;
    materialIntent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    materialIntent.defaultViewMode = RenderViewMode::MaterialId;
    materialIntent.engineOptions.rasterizer().setBackend(engine::raster::RasterBackend::openGL());
    GraphRenderEngine materialEngine(camera(), highContrastScene());
    materialEngine.setExecutionTraceEnabled(true);
    materialEngine.setPlan(compiler.compile({32, 32, 1}, materialIntent));

    Buffer<unsigned int> materialBuffer(32, 32);
    materialEngine.render(materialBuffer);

    auto materialTrace = materialEngine.lastExecutionTrace();
    ASSERT_TRUE(materialTrace);
    const RenderPassTrace* materialId = materialTrace->findPass("material_id_aov");
    ASSERT_NE(nullptr, materialId);
    EXPECT_NE(materialId->message().find("software raster diagnostic fallback"), std::string::npos);
  }

  TEST(GraphRenderEngine, RecordsOpenGLGenericDiagnosticAOVFallbackTraceMessage) {
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.defaultViewMode = RenderViewMode::Normal;
    intent.engineOptions.rasterizer().setBackend(engine::raster::RasterBackend::openGL());

    RenderGraphCompiler compiler;
    GraphRenderEngine engine(camera(), highContrastScene());
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({32, 32, 1}, intent));

    Buffer<unsigned int> buffer(32, 32);
    engine.render(buffer);

    auto trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);
    const RenderPassTrace* normal = trace->findPass("normal_aov");
    ASSERT_NE(nullptr, normal);
    EXPECT_NE(normal->message().find("software raster fallback"), std::string::npos);
  }

  TEST(GraphRenderEngine, ExecutesWorldPositionAOVViewAndRecordsColorTrace) {
    RenderIntent intent;
    intent.defaultViewMode = RenderViewMode::WorldPosition;

    RenderGraphCompiler compiler;
    GraphRenderEngine engine(camera(), highContrastScene());
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({32, 32, 1}, intent));

    Buffer<unsigned int> buffer(32, 32);
    engine.render(buffer);

    EXPECT_GT(countNonBlackPixels(buffer), 0);
    ASSERT_EQ(2u, engine.lastPlan().passes().size());
    EXPECT_EQ("world_position_aov", engine.lastPlan().passes()[0].id);
    EXPECT_EQ("visualize_world_position_aov", engine.lastPlan().passes()[1].id);

    auto trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);
    const auto outputs = trace->outputSnapshotsForResource("world_position_aov");
    ASSERT_EQ(1u, outputs.size());
    ASSERT_TRUE(outputs.front()->hasColorPreview());
    EXPECT_GT(countNonBlackPixels(outputs.front()->colorPreview()), 0);
  }

  TEST(GraphRenderEngine, RasterDepthAOVUsesRasterizerDiagnostics) {
    RenderGraphCompiler compiler;
    RenderIntent raytracedIntent;
    raytracedIntent.defaultExecutor = RenderExecutorPreference::Raytracer;
    raytracedIntent.defaultViewMode = RenderViewMode::Depth;
    RenderIntent rasterIntent;
    rasterIntent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    rasterIntent.defaultViewMode = RenderViewMode::Depth;

    Buffer<unsigned int> raytracedOutput(48, 48);
    GraphRenderEngine raytraced(camera(), highContrastScene());
    raytraced.setExecutionTraceEnabled(true);
    raytraced.setPlan(compiler.compile({48, 48, 1}, raytracedIntent));
    raytraced.render(raytracedOutput);

    Buffer<unsigned int> rasterOutput(48, 48);
    GraphRenderEngine raster(camera(), highContrastScene());
    raster.setExecutionTraceEnabled(true);
    raster.setPlan(compiler.compile({48, 48, 1}, rasterIntent));
    raster.render(rasterOutput);

    ASSERT_EQ(RenderExecutorKind::Raytracer, raytraced.lastPlan().passes()[0].executor);
    ASSERT_EQ(RenderExecutorKind::Rasterizer, raster.lastPlan().passes()[0].executor);

    auto raytracedTrace = raytraced.lastExecutionTrace();
    auto rasterTrace = raster.lastExecutionTrace();
    ASSERT_TRUE(raytracedTrace);
    ASSERT_TRUE(rasterTrace);
    const auto raytracedOutputs = raytracedTrace->outputSnapshotsForResource("depth_aov");
    const auto rasterOutputs = rasterTrace->outputSnapshotsForResource("depth_aov");
    ASSERT_EQ(1u, raytracedOutputs.size());
    ASSERT_EQ(1u, rasterOutputs.size());
    ASSERT_TRUE(raytracedOutputs.front()->hasDepthPreview());
    ASSERT_TRUE(rasterOutputs.front()->hasDepthPreview());
    EXPECT_GT(countFiniteDepths(raytracedOutputs.front()->depthPreview()), 0);
    EXPECT_GT(countFiniteDepths(rasterOutputs.front()->depthPreview()), 0);
    EXPECT_GT(countDifferingFiniteDepths(raytracedOutputs.front()->depthPreview(),
                                         rasterOutputs.front()->depthPreview(), 1e-6),
              0);
  }

  TEST(GraphRenderEngine, RasterColorAOVViewsExecuteThroughRasterizerDiagnostics) {
    const std::vector<std::pair<RenderViewMode, std::string>> aovs = {
      {RenderViewMode::Normal, "normal_aov"},
      {RenderViewMode::ObjectId, "object_id_aov"},
      {RenderViewMode::MaterialId, "material_id_aov"},
      {RenderViewMode::WorldPosition, "world_position_aov"},
      {RenderViewMode::RasterCoverageCount, "raster_coverage_count_aov"},
      {RenderViewMode::RasterDepthTestCount, "raster_depth_test_count_aov"},
      {RenderViewMode::RasterDepthPassCount, "raster_depth_pass_count_aov"},
      {RenderViewMode::RasterShadeCount, "raster_shade_count_aov"},
      {RenderViewMode::RasterColorWriteCount, "raster_color_write_count_aov"},
    };

    RenderGraphCompiler compiler;
    for (const auto& aov : aovs) {
      RenderIntent intent;
      intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
      intent.defaultViewMode = aov.first;

      GraphRenderEngine engine(camera(), highContrastScene());
      engine.setExecutionTraceEnabled(true);
      engine.setPlan(compiler.compile({32, 32, 1}, intent));

      Buffer<unsigned int> output(32, 32);
      engine.render(output);

      ASSERT_EQ(RenderExecutorKind::Rasterizer, engine.lastPlan().passes()[0].executor);
      auto trace = engine.lastExecutionTrace();
      ASSERT_TRUE(trace);
      const auto outputs = trace->outputSnapshotsForResource(aov.second);
      ASSERT_EQ(1u, outputs.size());
      ASSERT_TRUE(outputs.front()->hasColorPreview());
      EXPECT_GT(countNonBlackPixels(outputs.front()->colorPreview()), 0);
    }
  }

  TEST(GraphRenderEngine, RasterCounterAOVUsesAbsoluteCoolColorForLowCounts) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.defaultViewMode = RenderViewMode::RasterColorWriteCount;

    GraphRenderEngine engine(camera(), singleRectangleScene());
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({32, 32, 1}, intent));

    Buffer<unsigned int> output(32, 32);
    engine.render(output);

    auto trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);
    const auto outputs = trace->outputSnapshotsForResource("raster_color_write_count_aov");
    ASSERT_EQ(1u, outputs.size());
    ASSERT_TRUE(outputs.front()->hasColorPreview());
    const Buffer<Colord>& preview = outputs.front()->colorPreview();
    EXPECT_GT(countNonBlackPixels(preview), 0);

    for (int y = 0; y != preview.height(); ++y) {
      for (int x = 0; x != preview.width(); ++x) {
        if (preview[y][x] == Colord::black()) {
          continue;
        }
        EXPECT_LT(preview[y][x].r(), preview[y][x].b());
      }
    }
  }

  TEST(GraphRenderEngine, RasterStencilAOVMarksVisibleFragments) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.defaultViewMode = RenderViewMode::Stencil;

    GraphRenderEngine engine(camera(), highContrastScene());
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({32, 32, 4}, intent));

    Buffer<unsigned int> output(32, 32);
    engine.render(output);

    ASSERT_EQ(RenderExecutorKind::Rasterizer, engine.lastPlan().passes()[0].executor);
    auto trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);
    const auto outputs = trace->outputSnapshotsForResource("stencil_aov");
    ASSERT_EQ(1u, outputs.size());
    ASSERT_TRUE(outputs.front()->hasColorPreview());
    EXPECT_GT(countNonBlackPixels(outputs.front()->colorPreview()), 0);
  }

  TEST(GraphRenderEngine, StencilCompositeViewExecutesSynthesizedHybridPlan) {
    RenderGraphCompiler compiler;
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Raytracer;
    intent.defaultViewMode = RenderViewMode::StencilComposite;
    intent.exportedAOVs = {RenderViewMode::Stencil};

    GraphRenderEngine engine(camera(), highContrastScene());
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({32, 32, 1}, intent));

    Buffer<unsigned int> output(32, 32);
    engine.render(output);

    EXPECT_GT(countNonBlackPixels(output), 0);
    ASSERT_NE(nullptr, engine.lastPlan().findPass("stencil_composite"));
    auto trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);

    const auto stencilOutputs = trace->outputSnapshotsForResource("stencil_aov");
    ASSERT_EQ(1u, stencilOutputs.size());
    ASSERT_TRUE(stencilOutputs.front()->hasColorPreview());
    EXPECT_GT(countNonBlackPixels(stencilOutputs.front()->colorPreview()), 0);

    const auto compositeOutputs = trace->outputSnapshotsForResource("composited_color");
    ASSERT_EQ(1u, compositeOutputs.size());
    ASSERT_TRUE(compositeOutputs.front()->hasColorPreview());
    EXPECT_GT(countNonBlackPixels(compositeOutputs.front()->colorPreview()), 0);

    const auto stencilPreviewOutputs = trace->outputSnapshotsForResource("stencil_aov_color");
    ASSERT_EQ(1u, stencilPreviewOutputs.size());
    ASSERT_TRUE(stencilPreviewOutputs.front()->hasColorPreview());
    EXPECT_GT(countNonBlackPixels(stencilPreviewOutputs.front()->colorPreview()), 0);
  }

  TEST(GraphRenderEngine, ExecutesRequestedAOVSideOutputsWhenTraceIsEnabled) {
    RenderIntent intent;
    intent.exportedAOVs = {RenderViewMode::Depth, RenderViewMode::Normal};

    RenderGraphCompiler compiler;
    GraphRenderEngine engine(camera(), highContrastScene());
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({32, 32, 1}, intent));

    Buffer<unsigned int> buffer(32, 32);
    engine.render(buffer);

    EXPECT_GT(countNonBlackPixels(buffer), 0);
    ASSERT_NE(nullptr, engine.lastPlan().findResource("depth_aov_color"));
    ASSERT_NE(nullptr, engine.lastPlan().findResource("normal_aov_color"));

    auto trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);
    const auto depthOutputs = trace->outputSnapshotsForResource("depth_aov_color");
    ASSERT_EQ(1u, depthOutputs.size());
    ASSERT_TRUE(depthOutputs.front()->hasColorPreview());
    EXPECT_GT(countNonBlackPixels(depthOutputs.front()->colorPreview()), 0);

    const auto normalOutputs = trace->outputSnapshotsForResource("normal_aov_color");
    ASSERT_EQ(1u, normalOutputs.size());
    ASSERT_TRUE(normalOutputs.front()->hasColorPreview());
    EXPECT_GT(countNonBlackPixels(normalOutputs.front()->colorPreview()), 0);
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

  TEST(GraphRenderEngine, ExecutionInputFingerprintUsesStableTypeNames) {
    auto scene = directionalShadowScene();
    scene->addLight(
      std::make_shared<render::PointLight>(Vector3d(1.0, 2.0, -3.0), Colord(0.25, 0.5, 0.75)));
    GraphRenderEngine engine(camera(), scene);
    engine.setTonemap(std::make_shared<render::ReinhardTonemap>());

    const std::string fingerprint = engine.executionInputFingerprint();

    EXPECT_NE(std::string::npos, fingerprint.find("camera.type=PinholeCamera;"));
    EXPECT_NE(std::string::npos, fingerprint.find("tonemap.type=ReinhardTonemap;"));
    EXPECT_NE(std::string::npos, fingerprint.find("light[0].type=DirectionalLight;"));
    EXPECT_NE(std::string::npos, fingerprint.find("light[0].direction="));
    EXPECT_NE(std::string::npos, fingerprint.find("light[1].type=PointLight;"));
    EXPECT_NE(std::string::npos, fingerprint.find("light[1].position=1,2,-3;"));
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

  TEST(GraphRenderEngine, RecordsRasterMetricsInExecutionTraceMetadata) {
    auto scene = std::make_shared<render::Scene>();
    scene->add(std::make_shared<render::Sphere>(Vector3d::null, 1.0));

    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    RenderGraphCompiler compiler;

    GraphRenderEngine engine(camera(), scene);
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({32, 32, 1}, intent));

    Buffer<Colord> buffer(32, 32);
    engine.render(buffer);

    auto trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);
    const RenderPassTrace* raster = trace->findPass("raster_beauty");
    ASSERT_NE(nullptr, raster);
    const QJsonObject metadata = raster->metadata();
    EXPECT_TRUE(metadata.contains("timings"));
    EXPECT_TRUE(metadata.contains("fragments"));
    EXPECT_TRUE(metadata.contains("depthPrepass"));
    EXPECT_EQ("off", metadata.value("depthPrepass").toObject().value("requested").toString());
    EXPECT_EQ("disabled", metadata.value("depthPrepass").toObject().value("decision").toString());
    EXPECT_GT(metadata.value("timings").toObject().value("totalRenderSeconds").toDouble(), 0.0);
    EXPECT_GT(metadata.value("tessellation").toObject().value("trianglesAfterClipping").toDouble(),
              0.0);
    EXPECT_GT(metadata.value("fragments").toObject().value("coveredSamples").toDouble(), 0.0);
    EXPECT_EQ(metadata, raster->toJson().value("metadata").toObject());
  }

  TEST(GraphRenderEngine, RecordsWavefrontMetricsInExecutionTraceMetadata) {
    auto scene = std::make_shared<render::Scene>();
    auto sphere = std::make_shared<render::Sphere>(Vector3d::null, 1.0);
    sphere->setMaterial(std::make_shared<render::MatteMaterial>(
      std::make_shared<render::ConstantColorTexture>(Colord::white())));
    scene->add(sphere);

    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Wavefront;
    intent.engineOptions.raytracer().setIntegrator("pathtracer");
    intent.engineOptions.raytracer().setSamplesPerPixel(4);
    intent.engineOptions.raytracer().setConvergenceEnabled(true);
    intent.engineOptions.raytracer().setConvergenceActiveSampleFractionThreshold(1.0);
    intent.engineOptions.raytracer().setConvergenceRadianceDeltaRmsThreshold(10.0);
    intent.engineOptions.raytracer().setDenoiser("box");
    intent.engineOptions.raytracer().setDenoiseRadius(2);
    RenderGraphCompiler compiler;

    GraphRenderEngine engine(camera(), scene);
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(compiler.compile({16, 16, 1}, intent));

    Buffer<Colord> buffer(16, 16);
    engine.render(buffer);

    auto trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);
    const RenderPassTrace* wavefront = trace->findPass("wavefront_beauty");
    ASSERT_NE(nullptr, wavefront);
    const QJsonObject metadata = wavefront->metadata();
    const QJsonObject batching = metadata.value("batching").toObject();
    const QJsonObject input = metadata.value("input").toObject();
    EXPECT_EQ("pathtracer", batching.value("integrator").toString());
    EXPECT_EQ("depth_major_paths", batching.value("executionMode").toString());
    EXPECT_EQ(0.0, batching.value("compatibilityShadeSamples").toDouble());
    EXPECT_GT(batching.value("activeSampleDepthsProcessed").toDouble(), 0.0);
    EXPECT_EQ(4, input.value("samplesPerPixel").toInt());
    EXPECT_EQ(1024.0, input.value("primarySamples").toDouble());
    const QJsonObject convergence = metadata.value("convergence").toObject();
    EXPECT_TRUE(convergence.value("enabled").toBool());
    EXPECT_DOUBLE_EQ(1.0, convergence.value("activeSampleFractionThreshold").toDouble());
    EXPECT_DOUBLE_EQ(10.0, convergence.value("radianceDeltaRmsThreshold").toDouble());
    EXPECT_GT(convergence.value("stoppedTileCount").toDouble(), 0.0);
    const QJsonArray stoppedTileDepthHistogram =
      convergence.value("stoppedTileDepthHistogram").toArray();
    ASSERT_GE(stoppedTileDepthHistogram.size(), 1);
    EXPECT_GT(stoppedTileDepthHistogram.at(0).toDouble(), 0.0);
    EXPECT_EQ("stopped_some_tiles", convergence.value("decision").toString().toStdString());
    const QJsonObject denoise = metadata.value("denoise").toObject();
    EXPECT_TRUE(denoise.value("enabled").toBool());
    EXPECT_EQ("box", denoise.value("denoiser").toString().toStdString());
    EXPECT_DOUBLE_EQ(2.0, denoise.value("parameters").toObject().value("radius").toDouble());
    EXPECT_FALSE(denoise.value("features").toObject().value("albedo").toBool());
    EXPECT_FALSE(denoise.value("features").toObject().value("normal").toBool());
    EXPECT_FALSE(denoise.value("features").toObject().value("depth").toBool());
    EXPECT_GE(denoise.value("seconds").toDouble(), 0.0);
    const QJsonArray activeSamples = batching.value("activeSamplesPerDepth").toArray();
    ASSERT_GE(activeSamples.size(), 1);
    EXPECT_EQ(1024.0, activeSamples.at(0).toDouble());
    const QJsonArray retainedActiveSamples =
      batching.value("retainedActiveSamplesPerDepth").toArray();
    ASSERT_GE(retainedActiveSamples.size(), 1);
    EXPECT_LE(retainedActiveSamples.at(0).toDouble(), activeSamples.at(0).toDouble());
    const QJsonArray frontierHits = batching.value("frontierRayHitsPerDepth").toArray();
    const QJsonArray frontierMisses = batching.value("frontierRayMissesPerDepth").toArray();
    ASSERT_GE(frontierHits.size(), 1);
    ASSERT_GE(frontierMisses.size(), 1);
    EXPECT_EQ(1024.0, frontierHits.at(0).toDouble() + frontierMisses.at(0).toDouble());
    const QJsonArray deltaRms = batching.value("radianceDeltaRmsPerDepth").toArray();
    ASSERT_GE(deltaRms.size(), 1);
    EXPECT_GT(deltaRms.at(0).toDouble(), 0.0);
    const QJsonArray maxDelta = batching.value("maxRadianceDeltaPerDepth").toArray();
    ASSERT_GE(maxDelta.size(), 1);
    EXPECT_GT(maxDelta.at(0).toDouble(), 0.0);
    const QJsonObject timings = metadata.value("timings").toObject();
    EXPECT_TRUE(timings.contains("sampleStreamWorkerSeconds"));
    EXPECT_TRUE(timings.contains("primaryRayWorkerSeconds"));
    EXPECT_TRUE(timings.contains("sampleEnqueueWorkerSeconds"));
    EXPECT_TRUE(timings.contains("sampleGenerationOverheadWorkerSeconds"));
    EXPECT_TRUE(timings.contains("integratorIntersectionWorkerSeconds"));
    EXPECT_TRUE(timings.contains("integratorShadingWorkerSeconds"));
    EXPECT_TRUE(timings.contains("integratorOverheadWorkerSeconds"));
    EXPECT_TRUE(timings.contains("integratorPathSetupWorkerSeconds"));
    EXPECT_TRUE(timings.contains("integratorFrontierPartitionWorkerSeconds"));
    EXPECT_TRUE(timings.contains("integratorFrontierBookkeepingWorkerSeconds"));
    EXPECT_TRUE(timings.contains("integratorProgressSnapshotWorkerSeconds"));
    EXPECT_TRUE(timings.contains("integratorConvergenceTestWorkerSeconds"));
    EXPECT_TRUE(timings.contains("integratorResidualWorkerSeconds"));
    EXPECT_GE(timings.value("sampleGenerationWorkerSeconds").toDouble(), 0.0);
    EXPECT_GE(timings.value("sampleStreamWorkerSeconds").toDouble(), 0.0);
    EXPECT_GE(timings.value("primaryRayWorkerSeconds").toDouble(), 0.0);
    EXPECT_GE(timings.value("sampleEnqueueWorkerSeconds").toDouble(), 0.0);
    EXPECT_GE(timings.value("sampleGenerationOverheadWorkerSeconds").toDouble(), 0.0);
    EXPECT_GE(timings.value("integratorBatchWorkerSeconds").toDouble(), 0.0);
    EXPECT_GE(timings.value("integratorIntersectionWorkerSeconds").toDouble(), 0.0);
    EXPECT_GE(timings.value("integratorShadingWorkerSeconds").toDouble(), 0.0);
    EXPECT_GE(timings.value("integratorOverheadWorkerSeconds").toDouble(), 0.0);
    EXPECT_GE(timings.value("integratorPathSetupWorkerSeconds").toDouble(), 0.0);
    EXPECT_GE(timings.value("integratorFrontierPartitionWorkerSeconds").toDouble(), 0.0);
    EXPECT_GE(timings.value("integratorFrontierBookkeepingWorkerSeconds").toDouble(), 0.0);
    EXPECT_GE(timings.value("integratorProgressSnapshotWorkerSeconds").toDouble(), 0.0);
    EXPECT_GE(timings.value("integratorConvergenceTestWorkerSeconds").toDouble(), 0.0);
    EXPECT_GE(timings.value("integratorResidualWorkerSeconds").toDouble(), 0.0);
    EXPECT_GT(timings.value("totalRenderSeconds").toDouble(), 0.0);
    EXPECT_EQ(metadata, wavefront->toJson().value("metadata").toObject());
  }

  TEST(GraphRenderEngine, CompilePlanUsesSceneAnalysisAndClonesIt) {
    auto scene = std::make_shared<render::Scene>();
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.enablePreviewShadows = true;
    RenderSceneAnalysis analysis;
    analysis.recordVisibleSurface();

    GraphRenderEngine engine(camera(), scene);
    engine.setIntent(intent);
    engine.setSceneAnalysis(analysis);

    const RenderPlan plan = engine.compilePlan({4, 4, 1});
    EXPECT_EQ(nullptr, plan.findPass("raster_preview_shadows"));

    auto clone = std::dynamic_pointer_cast<GraphRenderEngine>(engine.cloneForRender());
    ASSERT_NE(nullptr, clone);
    const RenderPlan clonePlan = clone->compilePlan({4, 4, 1});
    EXPECT_EQ(nullptr, clonePlan.findPass("raster_preview_shadows"));
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

  TEST(GraphRenderEngine, RunsIndependentCpuSafePassesConcurrently) {
    auto material = std::make_shared<BlockingMaterial>();
    auto scene = std::make_shared<render::Scene>();
    auto sphere = std::make_shared<render::Sphere>(Vector3d::null, 100.0);
    sphere->setMaterial(material);
    scene->add(sphere);

    RenderPlan plan;
    plan.addResource(colorResource("side_color", RenderResourceLifetime::Transient, 1, 2));
    plan.addResource(colorResource("display_color", RenderResourceLifetime::Exported, 1, 2));

    RenderPassNode first;
    first.id = "beauty_a";
    first.kind = RenderPassKind::Beauty;
    first.executor = RenderExecutorKind::Raytracer;
    first.writes.push_back({"side_color"});
    first.concurrency = RenderConcurrencyLimit::parallel();
    first.canRunConcurrently = true;
    plan.addPass(first);

    RenderPassNode second = first;
    second.id = "beauty_b";
    second.writes = {{"display_color"}};
    plan.addPass(second);

    GraphRenderEngine engine(camera(), scene);
    engine.setPlan(plan);
    auto observer = std::make_shared<BlockingObserver>();
    engine.setExecutionObserver(observer);

    Buffer<Colord> buffer(1, 2);
    std::exception_ptr renderFailure;
    std::thread renderThread([&] {
      try {
        engine.render(buffer);
      } catch (...) {
        renderFailure = std::current_exception();
      }
    });

    const bool renderBlocked = material->waitForSecondCall(std::chrono::seconds(2));
    EXPECT_TRUE(renderBlocked);
    if (renderBlocked) {
      EXPECT_TRUE(observer->waitForStartedCount(2, std::chrono::seconds(2)));
    }
    material->releaseSecondCall();
    renderThread.join();
    if (renderFailure) {
      std::rethrow_exception(renderFailure);
    }

    EXPECT_EQ(Colord(0.25, 0.5, 0.75), buffer[0][0]);
  }

  TEST(GraphRenderEngine, SerialExecutorLimitPreservesPassOrder) {
    auto material = std::make_shared<BlockingMaterial>();
    auto scene = std::make_shared<render::Scene>();
    auto sphere = std::make_shared<render::Sphere>(Vector3d::null, 100.0);
    sphere->setMaterial(material);
    scene->add(sphere);

    RenderPlan plan;
    plan.addResource(colorResource("side_color", RenderResourceLifetime::Transient, 1, 2));
    plan.addResource(colorResource("display_color", RenderResourceLifetime::Exported, 1, 2));

    RenderPassNode first;
    first.id = "beauty_a";
    first.kind = RenderPassKind::Beauty;
    first.executor = RenderExecutorKind::Raytracer;
    first.writes.push_back({"side_color"});
    first.concurrency = RenderConcurrencyLimit::serial();
    first.canRunConcurrently = false;
    plan.addPass(first);

    RenderPassNode second = first;
    second.id = "beauty_b";
    second.writes = {{"display_color"}};
    plan.addPass(second);

    GraphRenderEngine engine(camera(), scene);
    engine.setPlan(plan);
    auto observer = std::make_shared<BlockingObserver>();
    engine.setExecutionObserver(observer);

    Buffer<Colord> buffer(1, 2);
    std::exception_ptr renderFailure;
    std::thread renderThread([&] {
      try {
        engine.render(buffer);
      } catch (...) {
        renderFailure = std::current_exception();
      }
    });

    const bool renderBlocked = material->waitForSecondCall(std::chrono::seconds(2));
    EXPECT_TRUE(renderBlocked);
    if (renderBlocked) {
      EXPECT_TRUE(observer->waitForEvent("start:beauty_a", std::chrono::seconds(2)));
      EXPECT_FALSE(observer->waitForEvent("start:beauty_b", std::chrono::milliseconds(100)));
    }
    material->releaseSecondCall();
    renderThread.join();
    if (renderFailure) {
      std::rethrow_exception(renderFailure);
    }

    const auto events = observer->events();
    const auto firstFinish = std::find(events.begin(), events.end(), "finish:beauty_a");
    const auto secondStart = std::find(events.begin(), events.end(), "start:beauty_b");
    ASSERT_NE(events.end(), firstFinish);
    ASSERT_NE(events.end(), secondStart);
    EXPECT_LT(firstFinish, secondStart);
  }

  TEST(GraphRenderEngine, FailingPassSkipsDependentsDeterministically) {
    RenderPlan plan;
    plan.addResource(colorResource("bad_color", RenderResourceLifetime::Transient));
    plan.addResource(colorResource("display_color", RenderResourceLifetime::Exported));

    RenderPassNode bad;
    bad.id = "bad";
    bad.kind = RenderPassKind::Custom;
    bad.executor = RenderExecutorKind::PostProcess;
    bad.writes.push_back({"bad_color"});
    plan.addPass(bad);

    RenderPassNode dependent;
    dependent.id = "dependent";
    dependent.kind = RenderPassKind::Tonemap;
    dependent.executor = RenderExecutorKind::PostProcess;
    dependent.reads.push_back({"bad_color"});
    dependent.writes.push_back({"display_color"});
    plan.addPass(dependent);

    GraphRenderEngine engine(camera(), highContrastScene());
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(plan);

    Buffer<Colord> buffer(2, 2);
    EXPECT_THROW(engine.render(buffer), std::runtime_error);

    auto trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);
    const RenderPassTrace* badTrace = trace->findPass("bad");
    ASSERT_NE(nullptr, badTrace);
    EXPECT_EQ(RenderPassExecutionStatus::Failed, badTrace->status());
    const RenderPassTrace* dependentTrace = trace->findPass("dependent");
    ASSERT_NE(nullptr, dependentTrace);
    EXPECT_EQ(RenderPassExecutionStatus::Skipped, dependentTrace->status());
  }

  TEST(GraphRenderEngine, CancellationSkipsQueuedDependentPasses) {
    auto material = std::make_shared<BlockingMaterial>();
    auto scene = std::make_shared<render::Scene>();
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
    auto observer = std::make_shared<BlockingObserver>();
    engine.setExecutionObserver(observer);

    Buffer<Colord> buffer(1, 2);
    std::exception_ptr renderFailure;
    std::thread renderThread([&] {
      try {
        engine.render(buffer);
      } catch (...) {
        renderFailure = std::current_exception();
      }
    });

    const bool renderBlocked = material->waitForSecondCall(std::chrono::seconds(2));
    EXPECT_TRUE(renderBlocked);
    if (renderBlocked) {
      engine.cancel();
      EXPECT_FALSE(observer->waitForEvent("start:tonemap", std::chrono::milliseconds(100)));
    }
    material->releaseSecondCall();
    renderThread.join();

    if (renderBlocked) {
      ASSERT_NE(nullptr, renderFailure);
      try {
        std::rethrow_exception(renderFailure);
      } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string::npos, std::string(error.what()).find("cancelled"));
      }
    }
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

  TEST(GraphRenderEngine, ExecutesReadbackPassForCpuMaterializedColorResource) {
    auto scene = std::make_shared<render::Scene>();
    scene->setBackground(Colord(0.25, 0.5, 0.75));

    RenderPlan plan;
    plan.addResource(colorResource("gpu_boundary_color", RenderResourceLifetime::Transient));
    plan.addResource(colorResource("display_color", RenderResourceLifetime::Exported));

    RenderPassNode beauty;
    beauty.id = "beauty";
    beauty.kind = RenderPassKind::Beauty;
    beauty.executor = RenderExecutorKind::Raytracer;
    beauty.writes.push_back({"gpu_boundary_color"});
    plan.addPass(beauty);

    RenderPassNode readback;
    readback.id = "readback";
    readback.kind = RenderPassKind::Readback;
    readback.executor = RenderExecutorKind::PostProcess;
    readback.reads.push_back({"gpu_boundary_color"});
    readback.writes.push_back({"display_color"});
    readback.supportedResourceDomains = {RenderResourceDomain::CPU, RenderResourceDomain::GPU};
    plan.addPass(readback);

    GraphRenderEngine engine(camera(), scene);
    engine.setExecutionTraceEnabled(true);
    engine.setPlan(plan);

    Buffer<Colord> buffer(2, 2);
    engine.render(buffer);

    EXPECT_EQ(scene->background(), buffer[0][0]);
    const auto trace = engine.lastExecutionTrace();
    ASSERT_NE(nullptr, trace);
    const auto* passTrace = trace->findPass("readback");
    ASSERT_NE(nullptr, passTrace);
    EXPECT_NE(std::string::npos,
              passTrace->message().find("readback copied CPU-materialized resource"));
  }

  TEST(GraphRenderEngine, RejectsReadbackFromDescriptorOnlyGpuResource) {
    auto scene = std::make_shared<render::Scene>();

    RenderPlan plan;
    auto gpu = colorResource("resident_color", RenderResourceLifetime::PersistentCache);
    gpu.domain = RenderResourceDomain::GPU;
    plan.addResource(gpu);
    plan.addResource(colorResource("display_color", RenderResourceLifetime::Exported));

    RenderPassNode readback;
    readback.id = "readback";
    readback.kind = RenderPassKind::Readback;
    readback.executor = RenderExecutorKind::PostProcess;
    readback.reads.push_back({"resident_color"});
    readback.writes.push_back({"display_color"});
    readback.supportedResourceDomains = {RenderResourceDomain::CPU, RenderResourceDomain::GPU};
    plan.addPass(readback);
    ASSERT_TRUE(plan.validate().valid());

    GraphRenderEngine engine(camera(), scene);
    engine.setPlan(plan);

    Buffer<Colord> buffer(2, 2);
    try {
      engine.render(buffer);
      FAIL() << "Expected descriptor-only GPU readback rejection";
    } catch (const std::runtime_error& error) {
      const std::string message = error.what();
      EXPECT_NE(std::string::npos, message.find("resource 'resident_color' has no CPU buffer"));
      EXPECT_NE(std::string::npos, message.find("GPU readback is not implemented yet"));
    }
  }

  TEST(GraphRenderEngine, RejectsUnboundExternalInputResources) {
    auto scene = std::make_shared<render::Scene>();

    RenderPlan plan;
    plan.addResource(colorResource("history_color", RenderResourceLifetime::Imported));
    plan.addResource(colorResource("display_color", RenderResourceLifetime::Exported));

    RenderPassNode tonemap;
    tonemap.id = "tonemap";
    tonemap.kind = RenderPassKind::Tonemap;
    tonemap.executor = RenderExecutorKind::PostProcess;
    tonemap.reads.push_back({"history_color"});
    tonemap.writes.push_back({"display_color"});
    plan.addPass(tonemap);
    ASSERT_TRUE(plan.validate().valid());

    GraphRenderEngine engine(camera(), scene);
    engine.setPlan(plan);

    Buffer<Colord> buffer(2, 2);
    try {
      engine.render(buffer);
      FAIL() << "Expected unbound external graph input rejection";
    } catch (const std::runtime_error& error) {
      const std::string message = error.what();
      EXPECT_NE(std::string::npos, message.find("external resource 'history_color'"));
      EXPECT_NE(std::string::npos, message.find("was not bound"));
    }
  }

  TEST(GraphRenderEngine, RendersBoundExternalColorInputResources) {
    auto scene = std::make_shared<render::Scene>();

    RenderPlan plan;
    plan.addResource(colorResource("history_color", RenderResourceLifetime::History));
    plan.addResource(colorResource("display_color", RenderResourceLifetime::Exported));

    RenderPassNode tonemap;
    tonemap.id = "tonemap";
    tonemap.kind = RenderPassKind::Tonemap;
    tonemap.executor = RenderExecutorKind::PostProcess;
    tonemap.reads.push_back({"history_color"});
    tonemap.writes.push_back({"display_color"});
    plan.addPass(tonemap);
    ASSERT_TRUE(plan.validate().valid());

    auto history = std::make_shared<Buffer<Colord>>(2, 2);
    history->clear(Colord(0.25, 0.5, 0.75));

    GraphRenderEngine engine(camera(), scene);
    engine.setPlan(plan);
    engine.setExternalColorResource("history_color", history);

    Buffer<Colord> buffer(2, 2);
    engine.render(buffer);

    EXPECT_EQ(Colord(0.25, 0.5, 0.75), buffer[0][0]);
    EXPECT_EQ(Colord(0.25, 0.5, 0.75), buffer[1][1]);
  }

  TEST(GraphRenderEngine, RendersBoundExternalDepthInputResources) {
    auto scene = std::make_shared<render::Scene>();

    RenderPlan plan;
    plan.addResource(depthResource("history_depth", RenderResourceLifetime::History));
    plan.addResource(colorResource("display_color", RenderResourceLifetime::Exported));

    RenderPassNode visualizeDepth;
    visualizeDepth.id = "visualize_depth";
    visualizeDepth.kind = RenderPassKind::AOV;
    visualizeDepth.executor = RenderExecutorKind::PostProcess;
    visualizeDepth.features = {"depth", "visualization"};
    visualizeDepth.reads.push_back({"history_depth"});
    visualizeDepth.writes.push_back({"display_color"});
    plan.addPass(visualizeDepth);
    ASSERT_TRUE(plan.validate().valid());

    auto depth = std::make_shared<Buffer<double>>(2, 2);
    (*depth)[0][0] = 1.0;
    (*depth)[0][1] = 3.0;
    (*depth)[1][0] = 2.0;
    (*depth)[1][1] = 3.0;

    GraphRenderEngine engine(camera(), scene);
    engine.setPlan(plan);
    engine.setExternalDepthResource("history_depth", depth);

    Buffer<Colord> buffer(2, 2);
    engine.render(buffer);

    EXPECT_EQ(Colord(1.0, 1.0, 1.0), buffer[0][0]);
    EXPECT_EQ(Colord::black(), buffer[0][1]);
  }

  TEST(GraphRenderEngine, RendersBoundExternalObjectIdInputResources) {
    auto scene = std::make_shared<render::Scene>();

    RenderPlan plan;
    plan.addResource(objectIdResource("history_object_id", RenderResourceLifetime::History));
    plan.addResource(colorResource("display_color", RenderResourceLifetime::Exported));

    RenderPassNode visualizeObjectId;
    visualizeObjectId.id = "visualize_object_id";
    visualizeObjectId.kind = RenderPassKind::AOV;
    visualizeObjectId.executor = RenderExecutorKind::PostProcess;
    visualizeObjectId.features = {"object_id", "visualization"};
    visualizeObjectId.reads.push_back({"history_object_id"});
    visualizeObjectId.writes.push_back({"display_color"});
    plan.addPass(visualizeObjectId);
    ASSERT_TRUE(plan.validate().valid());

    auto objectIds = std::make_shared<Buffer<std::uint32_t>>(2, 2);
    objectIds->clear(0);
    (*objectIds)[0][1] = 7;

    GraphRenderEngine engine(camera(), scene);
    engine.setPlan(plan);
    engine.setExternalObjectIdResource("history_object_id", objectIds);

    Buffer<Colord> buffer(2, 2);
    engine.render(buffer);

    EXPECT_EQ(Colord::black(), buffer[0][0]);
    EXPECT_FALSE(buffer[0][1] == Colord::black());
  }

  TEST(GraphRenderEngine, RendersBoundExternalStencilInputResources) {
    auto scene = std::make_shared<render::Scene>();

    RenderPlan plan;
    plan.addResource(colorResource("base_color", RenderResourceLifetime::History));
    plan.addResource(colorResource("foreground_color", RenderResourceLifetime::History));
    plan.addResource(stencilResource("stencil_mask", RenderResourceLifetime::History));
    plan.addResource(colorResource("display_color", RenderResourceLifetime::Exported));

    RenderPassNode composite;
    composite.id = "stencil_composite";
    composite.kind = RenderPassKind::Composite;
    composite.executor = RenderExecutorKind::Composite;
    composite.features = {"stencil_composite"};
    composite.reads.push_back({"base_color"});
    composite.reads.push_back({"foreground_color"});
    composite.reads.push_back({"stencil_mask"});
    composite.writes.push_back({"display_color"});
    plan.addPass(composite);
    ASSERT_TRUE(plan.validate().valid());

    auto baseColor = std::make_shared<Buffer<Colord>>(2, 2);
    auto foregroundColor = std::make_shared<Buffer<Colord>>(2, 2);
    auto stencil = std::make_shared<Buffer<std::uint8_t>>(2, 2);
    baseColor->clear(Colord(0.0, 0.0, 1.0));
    foregroundColor->clear(Colord(1.0, 0.0, 0.0));
    stencil->clear(0);
    (*stencil)[0][1] = 1;
    (*stencil)[1][0] = 1;

    GraphRenderEngine engine(camera(), scene);
    engine.setPlan(plan);
    engine.setExternalColorResource("base_color", baseColor);
    engine.setExternalColorResource("foreground_color", foregroundColor);
    engine.setExternalStencilResource("stencil_mask", stencil);

    Buffer<Colord> buffer(2, 2);
    engine.render(buffer);

    EXPECT_EQ(Colord(0.0, 0.0, 1.0), buffer[0][0]);
    EXPECT_EQ(Colord(1.0, 0.0, 0.0), buffer[0][1]);
    EXPECT_EQ(Colord(1.0, 0.0, 0.0), buffer[1][0]);
    EXPECT_EQ(Colord(0.0, 0.0, 1.0), buffer[1][1]);
  }

  TEST(GraphRenderEngine, DepthCompositeUsesNearestFiniteForegroundPixels) {
    auto scene = std::make_shared<render::Scene>();

    RenderPlan plan;
    plan.addResource(colorResource("base_color", RenderResourceLifetime::History));
    plan.addResource(colorResource("foreground_color", RenderResourceLifetime::History));
    plan.addResource(depthResource("base_depth", RenderResourceLifetime::History));
    plan.addResource(depthResource("foreground_depth", RenderResourceLifetime::History));
    plan.addResource(colorResource("display_color", RenderResourceLifetime::Exported));

    RenderPassNode composite;
    composite.id = "depth_composite";
    composite.kind = RenderPassKind::Composite;
    composite.executor = RenderExecutorKind::Composite;
    composite.features = {"depth_composite"};
    composite.reads.push_back({"base_color"});
    composite.reads.push_back({"foreground_color"});
    composite.reads.push_back({"base_depth"});
    composite.reads.push_back({"foreground_depth"});
    composite.writes.push_back({"display_color"});
    plan.addPass(composite);
    ASSERT_TRUE(plan.validate().valid());

    auto baseColor = std::make_shared<Buffer<Colord>>(2, 2);
    auto foregroundColor = std::make_shared<Buffer<Colord>>(2, 2);
    auto baseDepth = std::make_shared<Buffer<double>>(2, 2);
    auto foregroundDepth = std::make_shared<Buffer<double>>(2, 2);
    baseColor->clear(Colord(0.0, 0.0, 1.0));
    foregroundColor->clear(Colord(1.0, 0.0, 0.0));
    baseDepth->clear(1.0);
    (*foregroundDepth)[0][0] = 0.5;
    (*foregroundDepth)[0][1] = 2.0;
    (*foregroundDepth)[1][0] = std::numeric_limits<double>::infinity();
    (*foregroundDepth)[1][1] = 0.25;

    GraphRenderEngine engine(camera(), scene);
    engine.setPlan(plan);
    engine.setExternalColorResource("base_color", baseColor);
    engine.setExternalColorResource("foreground_color", foregroundColor);
    engine.setExternalDepthResource("base_depth", baseDepth);
    engine.setExternalDepthResource("foreground_depth", foregroundDepth);

    Buffer<Colord> buffer(2, 2);
    engine.render(buffer);

    EXPECT_EQ(Colord(1.0, 0.0, 0.0), buffer[0][0]);
    EXPECT_EQ(Colord(0.0, 0.0, 1.0), buffer[0][1]);
    EXPECT_EQ(Colord(0.0, 0.0, 1.0), buffer[1][0]);
    EXPECT_EQ(Colord(1.0, 0.0, 0.0), buffer[1][1]);
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

  TEST(GraphRenderEngine, ExecutesCurveOverlayPassOverBeautyColor) {
    auto scene = std::make_shared<render::Scene>();
    scene->setBackground(Colord(0.1, 0.2, 0.3));
    scene->add(std::make_shared<render::Curve>(
      core::Polyline(
        {Vector3d(-1.0, -0.5, 0.0), Vector3d(0.0, 0.5, 0.0), Vector3d(1.0, -0.5, 0.0)}),
      0.0, render::Curve::TessellationMode::Ribbon));

    RenderIntent intent;
    intent.enableCurveOverlay = true;
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
    engine::raster::detail::RasterShadowMapBuilder::resetDepthPassCountForTests();
    engine.render(buffer);

    auto trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);
    const auto outputs = trace->outputSnapshotsForResource("preview_shadow_map");
    ASSERT_EQ(1u, outputs.size());
    ASSERT_TRUE(outputs.front()->hasDepthPreview());
    EXPECT_EQ(256, outputs.front()->depthPreview().width());
    EXPECT_GT(countFiniteDepths(outputs.front()->depthPreview()), 0);
    EXPECT_EQ(RenderGraphCacheStatus::Stored, outputs.front()->cacheMetadata().status());
    EXPECT_EQ(4u, engine::raster::detail::RasterShadowMapBuilder::depthPassCountForTests());
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

    engine::raster::detail::RasterShadowMapBuilder::resetDepthPassCountForTests();
    engine.setTonemap(std::make_shared<render::ReinhardTonemap>());
    engine.render(buffer);

    auto trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);
    const auto outputs = trace->outputSnapshotsForResource("preview_shadow_map");
    ASSERT_EQ(1u, outputs.size());
    EXPECT_EQ(RenderGraphCacheStatus::Hit, outputs.front()->cacheMetadata().status());
    EXPECT_GT(countFiniteDepths(outputs.front()->depthPreview()), 0);
    EXPECT_EQ(1u, engine.artifactCache()->size());
    EXPECT_EQ(0u, engine::raster::detail::RasterShadowMapBuilder::depthPassCountForTests());

    engine::raster::detail::RasterShadowMapBuilder::resetDepthPassCountForTests();
    cam->setPosition(Vector3d(0.25, 0.0, -5.0));
    engine.render(buffer);

    trace = engine.lastExecutionTrace();
    ASSERT_TRUE(trace);
    const auto movedOutputs = trace->outputSnapshotsForResource("preview_shadow_map");
    ASSERT_EQ(1u, movedOutputs.size());
    EXPECT_EQ(RenderGraphCacheStatus::Stored, movedOutputs.front()->cacheMetadata().status());
    EXPECT_EQ(2u, engine.artifactCache()->size());
    EXPECT_EQ(4u, engine::raster::detail::RasterShadowMapBuilder::depthPassCountForTests());
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
