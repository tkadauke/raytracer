#include "engine/graph/GraphRenderEngine.h"

#include "core/Buffer.h"
#include "engine/raster/Rasterizer.h"
#include "engine/raytracer/Raytracer.h"
#include "engine/wireframe/Wireframe.h"
#include "render/cameras/Camera.h"
#include "render/primitives/Scene.h"

#include <atomic>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

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

    void pointDefaultCameraAtOrigin(const std::shared_ptr<render::Camera>& camera) {
      if (!camera) {
        return;
      }
      camera->setPosition(Vector3d(0, 0, -5));
      camera->setTarget(Vector3d::null);
    }

    const RenderPassNode& onlySupportedEnabledPass(const RenderPlan& plan) {
      std::vector<const RenderPassNode*> enabled;
      for (const auto& pass : plan.passes()) {
        if (pass.enabled) {
          enabled.push_back(&pass);
        }
      }

      if (enabled.size() != 1) {
        throw std::runtime_error(
          "GraphRenderEngine currently supports exactly one enabled beauty pass");
      }

      const RenderPassNode& pass = *enabled.front();
      if (pass.kind != RenderPassKind::Beauty) {
        throw std::runtime_error(
          "GraphRenderEngine currently supports only beauty-pass execution");
      }

      switch (pass.executor) {
      case RenderExecutorKind::Raytracer:
      case RenderExecutorKind::Rasterizer:
      case RenderExecutorKind::Wireframe:
        return pass;
      case RenderExecutorKind::Composite:
      case RenderExecutorKind::PostProcess:
        break;
      }

      throw std::runtime_error("GraphRenderEngine cannot execute pass '" + pass.id +
                               "' with executor '" + toString(pass.executor) + "'");
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
        engine = std::make_shared<::engine::raster::Rasterizer>(std::move(camera), scene);
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

      engine->setTonemap(graph.tonemap());
      if (graph.hasBackgroundColorOverride()) {
        engine->setBackgroundColor(graph.backgroundColor());
      }
      return engine;
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

    const RenderPassNode& pass = onlySupportedEnabledPass(plan);
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

    engine->render(buffer);
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
