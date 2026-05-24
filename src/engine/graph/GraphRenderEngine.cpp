#include "engine/graph/GraphRenderEngine.h"

#include "core/Buffer.h"
#include "engine/graph/RenderResourceStorage.h"
#include "engine/raster/Rasterizer.h"
#include "engine/raytracer/Raytracer.h"
#include "engine/wireframe/Wireframe.h"
#include "render/cameras/Camera.h"
#include "render/primitives/Scene.h"
#include "render/tonemap/Tonemap.h"

#include <atomic>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace engine::graph {
  namespace {
    std::string validationMessage(const RenderPlanValidation& validation) {
      std::ostringstream out;
      out << "render plan is invalid";
      for (const auto& error : validation.errors()) {
        out << "; " << toString(error.code) << ": " << error.message;
      }
      return out.str();
    }

    std::runtime_error passError(const RenderPassNode& pass, const std::string& message) {
      return std::runtime_error("pass '" + pass.id + "': " + message);
    }

    void requireColorResource(const RenderResourceStorage& storage,
                              const RenderResourceId& resource,
                              const RenderPassNode& pass) {
      if (!storage.resource(resource).colorBacked()) {
        throw passError(pass, "resource '" + resource + "' is not color-backed");
      }
    }

    void requireMatchingSize(const Buffer<Colord>& source,
                             const Buffer<Colord>& destination,
                             const std::string& action) {
      if (source.width() != destination.width() || source.height() != destination.height()) {
        throw std::runtime_error(action + " requires matching color buffer dimensions");
      }
    }

    void requireMatchingSize(const Buffer<Colord>& source,
                             const Buffer<unsigned int>& destination,
                             const std::string& action) {
      if (source.width() != destination.width() || source.height() != destination.height()) {
        throw std::runtime_error(action + " requires matching color buffer dimensions");
      }
    }

    void copyColorBuffer(const Buffer<Colord>& source, Buffer<Colord>& destination) {
      requireMatchingSize(source, destination, "color copy");
      for (int y = 0; y != source.height(); ++y) {
        for (int x = 0; x != source.width(); ++x) {
          destination[y][x] = source[y][x];
        }
      }
    }

    void packColorBuffer(const Buffer<Colord>& source, Buffer<unsigned int>& destination) {
      requireMatchingSize(source, destination, "color pack");
      for (int y = 0; y != source.height(); ++y) {
        for (int x = 0; x != source.width(); ++x) {
          destination[y][x] = source[y][x].rgb();
        }
      }
    }

    const ResourceRead& onlyRead(const RenderPassNode& pass) {
      if (pass.reads.size() != 1) {
        throw passError(pass, "requires exactly one input resource");
      }
      return pass.reads.front();
    }

    const ResourceWrite& onlyWrite(const RenderPassNode& pass) {
      if (pass.writes.size() != 1) {
        throw passError(pass, "requires exactly one output resource");
      }
      return pass.writes.front();
    }

    void pointDefaultCameraAtOrigin(const std::shared_ptr<render::Camera>& camera) {
      if (!camera) {
        return;
      }
      camera->setPosition(Vector3d(0, 0, -5));
      camera->setTarget(Vector3d::null);
    }

    void applyPreviewShadowPolicy(::engine::raster::Rasterizer& rasterizer) {
      rasterizer.setShadowMapsEnabled(true);
      rasterizer.setShadowMapSize(256);
      rasterizer.setShadowCascadeCount(4);
      rasterizer.setShadowBias(0.1);
      rasterizer.setShadowFilterRadius(1);
      rasterizer.setShadowFilterMode(engine::raster::Rasterizer::ShadowFilterMode::PCF);
    }

    std::shared_ptr<render::RenderEngine>
    makeBeautyEngine(RenderExecutorKind executor, const GraphRenderEngine& graph) {
      auto camera = graph.camera() ? graph.camera()->clone() : nullptr;
      auto scene = graph.scene();

      std::shared_ptr<render::RenderEngine> engine;
      switch (executor) {
      case RenderExecutorKind::Raytracer:
        engine = std::make_shared<::engine::raytracer::Raytracer>(std::move(camera), scene);
        break;
      case RenderExecutorKind::Rasterizer:
        {
          auto rasterizer =
            std::make_shared<::engine::raster::Rasterizer>(std::move(camera), scene);
          if (graph.intent().enablePreviewShadows) {
            applyPreviewShadowPolicy(*rasterizer);
          }
          engine = rasterizer;
        }
        break;
      case RenderExecutorKind::Wireframe:
        engine = std::make_shared<::engine::wireframe::Wireframe>(std::move(camera), scene);
        break;
      case RenderExecutorKind::Composite:
      case RenderExecutorKind::PostProcess:
        break;
      }

      if (!engine) {
        throw std::runtime_error("unsupported beauty executor");
      }

      if (graph.hasBackgroundColorOverride()) {
        engine->setBackgroundColor(graph.backgroundColor());
      }
      return engine;
    }

    void substituteDefaultOutput(const RenderPassNode& pass,
                                 RenderResourceStorage& storage,
                                 const GraphRenderEngine& graph) {
      for (const auto& write : pass.writes) {
        storage.resource(write.resource).clearSubstituteDefault(pass.kind, graph.backgroundColor());
      }
    }

    void passthroughColorOutput(const RenderPassNode& pass, RenderResourceStorage& storage) {
      const auto& read = onlyRead(pass);
      requireColorResource(storage, read.resource, pass);
      const Buffer<Colord>& source = storage.color(read.resource);

      for (const auto& write : pass.writes) {
        requireColorResource(storage, write.resource, pass);
        Buffer<Colord>& destination = storage.color(write.resource);
        if (&source != &destination) {
          copyColorBuffer(source, destination);
        }
      }
    }

    void executeTonemapPass(const RenderPassNode& pass,
                            RenderResourceStorage& storage,
                            const GraphRenderEngine& graph) {
      const auto& read = onlyRead(pass);
      const auto& write = onlyWrite(pass);
      requireColorResource(storage, read.resource, pass);
      requireColorResource(storage, write.resource, pass);

      const Buffer<Colord>& source = storage.color(read.resource);
      Buffer<Colord>& destination = storage.color(write.resource);
      requireMatchingSize(source, destination, "tonemap pass");

      auto tonemap = graph.tonemap();
      for (int y = 0; y != source.height(); ++y) {
        for (int x = 0; x != source.width(); ++x) {
          destination[y][x] = tonemap->apply(source[y][x]);
        }
      }
    }

    const RenderResourceDescriptor& outputColorResource(const RenderPlan& plan) {
      for (const auto& resource : plan.resources()) {
        if (resource.lifetime == RenderResourceLifetime::Exported &&
            resource.type == RenderResourceType::Color) {
          return resource;
        }
      }
      throw std::runtime_error("render plan has no exported color resource");
    }
  }

  struct GraphRenderEngine::Private {
    RenderIntent intent;
    std::optional<RenderPlan> explicitPlan;
    RenderPlan lastPlan;
    std::shared_ptr<render::RenderEngine> activeEngine;
    std::atomic<bool> cancelled{false};
    mutable std::mutex activeEngineMutex;
  };

  GraphRenderEngine::GraphRenderEngine(std::shared_ptr<render::Scene> scene)
      : RenderEngine(std::move(scene)),
        p(std::make_unique<Private>()) {
    pointDefaultCameraAtOrigin(m_camera);
  }

  GraphRenderEngine::GraphRenderEngine(std::shared_ptr<render::Camera> camera,
                                       std::shared_ptr<render::Scene> scene)
      : RenderEngine(std::move(camera), std::move(scene)),
        p(std::make_unique<Private>()) {
  }

  GraphRenderEngine::~GraphRenderEngine() = default;

  std::shared_ptr<render::RenderEngine> GraphRenderEngine::cloneForRender() const {
    auto result = std::make_shared<GraphRenderEngine>(m_camera ? m_camera->clone() : nullptr,
                                                      m_scene);
    result->setTonemap(tonemap());
    if (hasBackgroundColorOverride()) {
      result->setBackgroundColor(backgroundColor());
    }
    result->setIntent(p->intent);
    if (p->explicitPlan) {
      result->setPlan(*p->explicitPlan);
    }
    return result;
  }

  void GraphRenderEngine::setIntent(RenderIntent intent) {
    p->intent = std::move(intent);
  }

  const RenderIntent& GraphRenderEngine::intent() const {
    return p->intent;
  }

  void GraphRenderEngine::setPlan(RenderPlan plan) {
    p->explicitPlan = std::move(plan);
  }

  void GraphRenderEngine::clearPlan() {
    p->explicitPlan.reset();
  }

  bool GraphRenderEngine::hasExplicitPlan() const {
    return p->explicitPlan.has_value();
  }

  RenderPlan GraphRenderEngine::compilePlan(const RenderTargetSpec& target) const {
    RenderGraphCompiler compiler;
    return compiler.compile(target, p->intent);
  }

  const RenderPlan& GraphRenderEngine::lastPlan() const {
    return p->lastPlan;
  }

  void GraphRenderEngine::render(Buffer<Colord>& buffer) {
    if (buffer.width() <= 0 || buffer.height() <= 0 || !m_scene || !m_camera) {
      buffer.clear();
      return;
    }

    RenderPlan plan = p->explicitPlan ? *p->explicitPlan
                                      : compilePlan({buffer.width(), buffer.height(), 1});
    p->lastPlan = plan;

    const auto validation = plan.validate();
    if (!validation.valid()) {
      throw std::runtime_error(validationMessage(validation));
    }

    RenderResourceStorage storage;
    storage.allocate(plan.resources());

    for (const auto& pass : plan.passes()) {
      if (!pass.enabled) {
        switch (pass.disabledBehavior) {
        case DisabledBehavior::SubstituteDefault:
          substituteDefaultOutput(pass, storage, *this);
          break;
        case DisabledBehavior::Passthrough:
          passthroughColorOutput(pass, storage);
          break;
        case DisabledBehavior::CullDependents:
        case DisabledBehavior::Error:
          break;
        }
        continue;
      }

      if (pass.kind == RenderPassKind::Beauty) {
        const auto& write = onlyWrite(pass);
        requireColorResource(storage, write.resource, pass);

        auto engine = makeBeautyEngine(pass.executor, *this);
        if (p->cancelled.load()) {
          engine->cancel();
        } else {
          engine->uncancel();
        }

        {
          std::lock_guard<std::mutex> lock(p->activeEngineMutex);
          p->activeEngine = engine;
        }

        struct ActiveEngineReset {
          Private& p;
          ~ActiveEngineReset() {
            std::lock_guard<std::mutex> lock(p.activeEngineMutex);
            p.activeEngine.reset();
          }
        } reset{*p};

        engine->render(storage.color(write.resource));
        continue;
      }

      if (pass.kind == RenderPassKind::Tonemap &&
          pass.executor == RenderExecutorKind::PostProcess) {
        executeTonemapPass(pass, storage, *this);
        continue;
      }

      throw std::runtime_error("GraphRenderEngine cannot execute enabled pass '" + pass.id +
                               "' with kind '" + toString(pass.kind) +
                               "' and executor '" + toString(pass.executor) + "'");
    }

    copyColorBuffer(storage.color(outputColorResource(plan).id), buffer);
  }

  void GraphRenderEngine::render(Buffer<unsigned int>& buffer) {
    Buffer<Colord> graphOutput(buffer.width(), buffer.height());
    render(graphOutput);
    packColorBuffer(graphOutput, buffer);
  }

  void GraphRenderEngine::cancel() {
    p->cancelled.store(true);
    std::shared_ptr<render::RenderEngine> active;
    {
      std::lock_guard<std::mutex> lock(p->activeEngineMutex);
      active = p->activeEngine;
    }
    if (active) {
      active->cancel();
    }
  }

  void GraphRenderEngine::uncancel() {
    p->cancelled.store(false);
    std::shared_ptr<render::RenderEngine> active;
    {
      std::lock_guard<std::mutex> lock(p->activeEngineMutex);
      active = p->activeEngine;
    }
    if (active) {
      active->uncancel();
    }
  }

  std::list<Recti> GraphRenderEngine::activeTiles() const {
    std::shared_ptr<render::RenderEngine> active;
    {
      std::lock_guard<std::mutex> lock(p->activeEngineMutex);
      active = p->activeEngine;
    }
    return active ? active->activeTiles() : std::list<Recti>();
  }

  std::list<Recti> GraphRenderEngine::completedTiles() const {
    std::shared_ptr<render::RenderEngine> active;
    {
      std::lock_guard<std::mutex> lock(p->activeEngineMutex);
      active = p->activeEngine;
    }
    return active ? active->completedTiles() : std::list<Recti>();
  }
}
