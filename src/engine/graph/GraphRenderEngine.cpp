#include "engine/graph/GraphRenderEngine.h"

#include "core/Buffer.h"
#include "engine/graph/RenderGraphArtifactCache.h"
#include "engine/graph/RenderPassPayload.h"
#include "engine/graph/RenderExecutionContext.h"
#include "engine/graph/RenderGraphExecutionObserver.h"
#include "engine/graph/RenderGraphExecutionTrace.h"
#include "engine/graph/RenderResourceStorage.h"
#include "core/math/BoundingBox.h"
#include "render/cameras/Camera.h"
#include "render/primitives/Scene.h"
#include "render/tonemap/LinearTonemap.h"
#include "render/tonemap/Tonemap.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <typeinfo>
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
                              const RenderResourceId& resource, const RenderPassNode& pass) {
      if (!storage.resource(resource).colorBacked()) {
        throw passError(pass, "resource '" + resource + "' is not color-backed");
      }
    }

    void requireMatchingSize(const Buffer<Colord>& source, const Buffer<Colord>& destination,
                             const std::string& action) {
      if (source.width() != destination.width() || source.height() != destination.height()) {
        throw std::runtime_error(action + " requires matching color buffer dimensions");
      }
    }

    void requireMatchingSize(const Buffer<Colord>& source, const Buffer<unsigned int>& destination,
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

    void packColorBuffer(const Buffer<Colord>& source, Buffer<unsigned int>& destination,
                         const std::shared_ptr<render::Tonemap>& tonemap) {
      requireMatchingSize(source, destination, "color pack");
      for (int y = 0; y != source.height(); ++y) {
        for (int x = 0; x != source.width(); ++x) {
          destination[y][x] = (tonemap ? tonemap->apply(source[y][x]) : source[y][x]).rgb();
        }
      }
    }

    void pointDefaultCameraAtOrigin(const std::shared_ptr<render::Camera>& camera) {
      if (!camera) {
        return;
      }
      camera->setPosition(Vector3d(0, 0, -5));
      camera->setTarget(Vector3d::null);
    }

    void writeColorFingerprint(std::ostream& out, const char* name, const Colord& color) {
      out << name << '=' << color.r() << ',' << color.g() << ',' << color.b() << ';';
    }

    void writeVectorFingerprint(std::ostream& out, const char* name, const Vector3d& vector) {
      out << name << '=' << vector.x() << ',' << vector.y() << ',' << vector.z() << ';';
    }

    void writeBoundingBoxFingerprint(std::ostream& out, const BoundingBoxd& bounds) {
      out << "bounds.valid=" << bounds.isValid() << ';'
          << "bounds.undefined=" << bounds.isUndefined() << ';'
          << "bounds.infinite=" << bounds.isInfinite() << ';';
      if (bounds.isValid() && !bounds.isUndefined() && !bounds.isInfinite()) {
        writeVectorFingerprint(out, "bounds.min", bounds.min());
        writeVectorFingerprint(out, "bounds.max", bounds.max());
      }
    }

    template<class T>
    const char* dynamicTypeName(const T& value) {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpotentially-evaluated-expression"
#endif
      const char* result = typeid(value).name();
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
      return result;
    }

    std::string renderInputFingerprintFor(const GraphRenderEngine& graph) {
      std::ostringstream out;
      out << std::setprecision(17);

      if (auto camera = graph.camera()) {
        out << "camera.type=" << dynamicTypeName(*camera) << ';'
            << "camera.aspectMode=" << static_cast<int>(camera->aspectMode()) << ';'
            << "camera.aspectRatio=" << camera->aspectRatio() << ';';
        writeVectorFingerprint(out, "camera.position", camera->position());
        writeVectorFingerprint(out, "camera.target", camera->target());
      } else {
        out << "camera=null;";
      }

      if (auto scene = graph.scene()) {
        out << "scene.ptr=" << scene.get() << ';' << "scene.lights=" << scene->lights().size()
            << ';';
        writeColorFingerprint(out, "scene.ambient", scene->ambient());
        writeColorFingerprint(out, "scene.background", scene->background());
        writeBoundingBoxFingerprint(out, scene->boundingBox());
      } else {
        out << "scene=null;";
      }

      writeColorFingerprint(out, "engine.background", graph.backgroundColor());
      if (auto tonemap = graph.tonemap()) {
        out << "tonemap.type=" << dynamicTypeName(*tonemap) << ';';
      } else {
        out << "tonemap=null;";
      }
      return out.str();
    }

    void substituteDefaultOutput(const RenderPassNode& pass, RenderResourceStorage& storage,
                                 const GraphRenderEngine& graph) {
      for (const auto& write : pass.writes) {
        storage.resource(write.resource).clearSubstituteDefault(pass.kind, graph.backgroundColor());
      }
    }

    void passthroughColorOutput(const RenderPassNode& pass, RenderResourceStorage& storage) {
      const auto& read = pass.singleRead();
      requireColorResource(storage, read.resource, pass);
      const Buffer<Colord>& source = storage.color(read.resource);

      for (const auto& write : pass.writes) {
        requireColorResource(storage, write.resource, pass);
        Buffer<Colord>& destination = storage.color(write.resource);
        if (&source != &destination) {
          copyColorBuffer(source, destination);
        }
        storage.resource(write.resource).markProduced();
      }
    }

    bool planAppliesTonemap(const RenderPlan& plan) {
      return std::any_of(plan.passes().begin(), plan.passes().end(), [](const auto& pass) {
        return pass.enabled && pass.kind == RenderPassKind::Tonemap &&
               pass.executor == RenderExecutorKind::PostProcess;
      });
    }

    std::shared_ptr<render::Tonemap> displayTonemapForPlan(const RenderPlan& plan,
                                                           const GraphRenderEngine& graph) {
      if (planAppliesTonemap(plan)) {
        return graph.tonemap();
      }
      return std::make_shared<render::LinearTonemap>();
    }

    void publishWritesForDisplay(const RenderPassNode& pass, const RenderPlan& plan,
                                 RenderResourceStorage& storage, Buffer<unsigned int>& display,
                                 const std::shared_ptr<render::Tonemap>& displayTonemap) {
      const auto& output = plan.exportedColorResource();
      for (const auto& write : pass.writes) {
        RenderResource& resource = storage.resource(write.resource);
        if (!resource.colorBacked()) {
          continue;
        }

        const Buffer<Colord>& source = resource.color();
        if (write.resource == output.id) {
          packColorBuffer(source, display);
        } else if (pass.kind != RenderPassKind::Tonemap) {
          packColorBuffer(source, display, displayTonemap);
        }
      }
    }

    void requireMatchingOutputSize(const RenderPlan& plan, int width, int height) {
      const auto& output = plan.exportedColorResource();
      if (output.width != width || output.height != height) {
        throw std::runtime_error("color pack requires matching color buffer dimensions");
      }
    }

    void notifyRenderStarted(const GraphRenderEngine& graph, std::uint64_t generation) {
      if (auto observer = graph.executionObserver()) {
        observer->renderStarted(generation);
      }
    }

    void notifyPassStarted(const GraphRenderEngine& graph, const RenderPassNode& pass,
                           std::uint64_t generation) {
      if (auto observer = graph.executionObserver()) {
        observer->passStarted(pass.id, generation);
      }
    }

    void notifyPassFinished(const GraphRenderEngine& graph, const RenderPassNode& pass,
                            std::uint64_t generation) {
      if (auto observer = graph.executionObserver()) {
        observer->passFinished(pass.id, generation);
      }
    }

    void notifyPassFailed(const GraphRenderEngine& graph, const RenderPassNode& pass,
                          const std::string& message, std::uint64_t generation) {
      if (auto observer = graph.executionObserver()) {
        observer->passFailed(pass.id, message, generation);
      }
    }

    template<class Execute>
    void executeObserved(const GraphRenderEngine& graph,
                         const std::shared_ptr<RenderGraphExecutionTraceRecorder>& recorder,
                         std::shared_ptr<const RenderGraphExecutionTraceSession> traceSession,
                         std::uint64_t renderGeneration, const RenderPassNode& pass,
                         const RenderResourceStorage& storage, Execute execute) {
      notifyPassStarted(graph, pass, renderGeneration);
      if (recorder && traceSession) {
        recorder->passStarted(traceSession, pass, storage);
      }
      try {
        execute();
      } catch (const std::exception& error) {
        if (recorder && traceSession) {
          recorder->passFailed(traceSession, pass, storage, error.what());
        }
        notifyPassFailed(graph, pass, error.what(), renderGeneration);
        throw;
      } catch (...) {
        if (recorder && traceSession) {
          recorder->passFailed(traceSession, pass, storage, "unknown render graph pass failure");
        }
        notifyPassFailed(graph, pass, "unknown render graph pass failure", renderGeneration);
        throw;
      }
      if (recorder && traceSession) {
        recorder->passCompleted(traceSession, pass, storage);
      }
      notifyPassFinished(graph, pass, renderGeneration);
    }

    struct TraceSession {
      TraceSession() = default;
      TraceSession(std::shared_ptr<RenderGraphExecutionTraceRecorder> traceRecorder,
                   std::shared_ptr<const RenderGraphExecutionTraceSession> traceSession)
          : recorder(std::move(traceRecorder)),
            session(std::move(traceSession)) {
      }

      TraceSession(const TraceSession&) = delete;
      TraceSession& operator=(const TraceSession&) = delete;
      TraceSession(TraceSession&&) noexcept = default;
      TraceSession& operator=(TraceSession&&) noexcept = default;

      std::shared_ptr<RenderGraphExecutionTraceRecorder> recorder;
      std::shared_ptr<const RenderGraphExecutionTraceSession> session;

      ~TraceSession() {
        if (recorder && session) {
          recorder->finish(session);
        }
      }

      explicit operator bool() const {
        return recorder && session;
      }
    };
  }

  struct GraphRenderEngine::Private {
    RenderIntent intent;
    std::optional<RenderPlan> explicitPlan;
    RenderPlan lastPlan;
    std::shared_ptr<render::RenderEngine> activeEngine;
    std::shared_ptr<RenderGraphExecutionObserver> executionObserver;
    std::shared_ptr<RenderGraphExecutionTraceRecorder> executionTraceRecorder{
      std::make_shared<RenderGraphExecutionTraceRecorder>()};
    std::shared_ptr<RenderGraphArtifactCache> artifactCache{
      std::make_shared<RenderGraphArtifactCache>()};
    std::shared_ptr<std::atomic<std::uint64_t>> nextExecutionGeneration{
      std::make_shared<std::atomic<std::uint64_t>>(1)};
    std::atomic<bool> executionTraceEnabled{false};
    std::atomic<bool> cancelled{false};
    mutable std::mutex activeEngineMutex;
    mutable std::mutex executionObserverMutex;

    std::uint64_t claimExecutionGeneration() const {
      return nextExecutionGeneration->fetch_add(1);
    }

    TraceSession beginTraceIfEnabled(const RenderPlan& plan) const {
      return beginTraceIfEnabled(plan, std::string());
    }

    TraceSession beginTraceIfEnabled(const RenderPlan& plan,
                                     const std::string& inputFingerprint) const {
      if (!executionTraceEnabled.load()) {
        executionTraceRecorder->clear();
        return {};
      }
      return TraceSession(executionTraceRecorder,
                          executionTraceRecorder->begin(plan, inputFingerprint));
    }
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
    auto result =
      std::make_shared<GraphRenderEngine>(m_camera ? m_camera->clone() : nullptr, m_scene);
    result->setTonemap(tonemap());
    if (hasBackgroundColorOverride()) {
      result->setBackgroundColor(backgroundColor());
    }
    result->setIntent(p->intent);
    if (p->explicitPlan) {
      result->setPlan(*p->explicitPlan);
    }
    result->setExecutionObserver(executionObserver());
    result->setExecutionTraceEnabled(executionTraceEnabled());
    result->p->executionTraceRecorder = p->executionTraceRecorder;
    result->p->artifactCache = p->artifactCache;
    result->p->nextExecutionGeneration = p->nextExecutionGeneration;
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

  const RenderPlan* GraphRenderEngine::explicitPlan() const {
    return p->explicitPlan ? &*p->explicitPlan : nullptr;
  }

  RenderPlan GraphRenderEngine::compilePlan(const RenderTargetSpec& target) const {
    RenderGraphCompiler compiler;
    return compiler.compile(target, p->intent);
  }

  const RenderPlan& GraphRenderEngine::lastPlan() const {
    return p->lastPlan;
  }

  void
  GraphRenderEngine::setExecutionObserver(std::shared_ptr<RenderGraphExecutionObserver> observer) {
    std::lock_guard<std::mutex> lock(p->executionObserverMutex);
    p->executionObserver = std::move(observer);
  }

  std::shared_ptr<RenderGraphExecutionObserver> GraphRenderEngine::executionObserver() const {
    std::lock_guard<std::mutex> lock(p->executionObserverMutex);
    return p->executionObserver;
  }

  std::shared_ptr<const RenderGraphExecutionTrace> GraphRenderEngine::lastExecutionTrace() const {
    return p->executionTraceRecorder->lastTrace();
  }

  std::shared_ptr<const RenderGraphExecutionTrace>
  GraphRenderEngine::lastExecutionTraceForPlan(const RenderPlan& plan) const {
    auto trace = lastExecutionTrace();
    if (trace && trace->matchesPlanAndInputs(plan, executionInputFingerprint())) {
      return trace;
    }
    return nullptr;
  }

  void GraphRenderEngine::setExecutionTraceEnabled(bool enabled) {
    p->executionTraceEnabled.store(enabled);
    if (!enabled) {
      p->executionTraceRecorder->clear();
    }
  }

  bool GraphRenderEngine::executionTraceEnabled() const {
    return p->executionTraceEnabled.load();
  }

  std::string GraphRenderEngine::executionInputFingerprint() const {
    return renderInputFingerprintFor(*this);
  }

  std::shared_ptr<RenderGraphArtifactCache> GraphRenderEngine::artifactCache() const {
    return p->artifactCache;
  }

  void GraphRenderEngine::render(Buffer<Colord>& buffer) {
    if (buffer.width() <= 0 || buffer.height() <= 0 || !m_scene || !m_camera) {
      buffer.clear();
      return;
    }

    RenderPlan plan =
      p->explicitPlan ? *p->explicitPlan : compilePlan({buffer.width(), buffer.height(), 1});
    p->lastPlan = plan;

    const auto validation = plan.validate();
    if (!validation.valid()) {
      throw std::runtime_error(validationMessage(validation));
    }
    const std::uint64_t renderGeneration = p->claimExecutionGeneration();
    TraceSession traceSession = p->beginTraceIfEnabled(plan, executionInputFingerprint());
    notifyRenderStarted(*this, renderGeneration);

    RenderResourceStorage storage;
    storage.allocate(plan.resources());

    const auto executionOrder = plan.executionOrder();
    for (const RenderPassNode* passNode : executionOrder) {
      const RenderPassNode& pass = *passNode;
      if (!pass.enabled) {
        std::string disabledMessage;
        switch (pass.disabledBehavior) {
        case DisabledBehavior::SubstituteDefault:
          substituteDefaultOutput(pass, storage, *this);
          disabledMessage = "disabled pass substituted default output";
          break;
        case DisabledBehavior::Passthrough:
          passthroughColorOutput(pass, storage);
          disabledMessage = "disabled pass passed color through";
          break;
        case DisabledBehavior::CullDependents:
          disabledMessage = "disabled pass culled dependents";
          break;
        case DisabledBehavior::Error:
          disabledMessage = "disabled pass did not execute";
          break;
        }
        if (traceSession) {
          p->executionTraceRecorder->passSkipped(traceSession.session, pass, storage,
                                                 disabledMessage);
        }
        continue;
      }

      auto payload = RenderPassPayload::createBuiltin(pass);
      if (!payload) {
        throw std::runtime_error("GraphRenderEngine cannot execute enabled pass '" + pass.id +
                                 "' with kind '" + toString(pass.kind) + "' and executor '" +
                                 toString(pass.executor) + "'");
      }

      auto setActiveEngine = [this](std::shared_ptr<render::RenderEngine> engine) {
        std::lock_guard<std::mutex> lock(p->activeEngineMutex);
        p->activeEngine = std::move(engine);
      };

      RenderExecutionContext context(pass, storage, *this, p->cancelled.load(), setActiveEngine);
      struct ActiveEngineReset {
        RenderExecutionContext& context;
        ~ActiveEngineReset() {
          context.clearActiveEngine();
        }
      } reset{context};

      executeObserved(*this, p->executionTraceRecorder, traceSession.session, renderGeneration,
                      pass, storage, [&] { payload->execute(context); });
      for (const auto& write : pass.writes) {
        storage.resource(write.resource).markProduced();
      }
    }

    copyColorBuffer(storage.color(plan.exportedColorResource().id), buffer);
  }

  void GraphRenderEngine::render(Buffer<unsigned int>& buffer) {
    if (buffer.width() <= 0 || buffer.height() <= 0 || !m_scene || !m_camera) {
      buffer.clear();
      return;
    }

    RenderPlan plan =
      p->explicitPlan ? *p->explicitPlan : compilePlan({buffer.width(), buffer.height(), 1});
    p->lastPlan = plan;

    const auto validation = plan.validate();
    if (!validation.valid()) {
      throw std::runtime_error(validationMessage(validation));
    }
    requireMatchingOutputSize(plan, buffer.width(), buffer.height());
    const std::uint64_t renderGeneration = p->claimExecutionGeneration();
    TraceSession traceSession = p->beginTraceIfEnabled(plan, executionInputFingerprint());
    notifyRenderStarted(*this, renderGeneration);

    RenderResourceStorage storage;
    storage.allocate(plan.resources());
    const auto displayTonemap = displayTonemapForPlan(plan, *this);

    auto setActiveEngine = [this](std::shared_ptr<render::RenderEngine> engine) {
      std::lock_guard<std::mutex> lock(p->activeEngineMutex);
      p->activeEngine = std::move(engine);
    };

    const auto executionOrder = plan.executionOrder();
    for (const RenderPassNode* passNode : executionOrder) {
      const RenderPassNode& pass = *passNode;
      if (!pass.enabled) {
        std::string disabledMessage;
        switch (pass.disabledBehavior) {
        case DisabledBehavior::SubstituteDefault:
          substituteDefaultOutput(pass, storage, *this);
          publishWritesForDisplay(pass, plan, storage, buffer, displayTonemap);
          disabledMessage = "disabled pass substituted default output";
          break;
        case DisabledBehavior::Passthrough:
          passthroughColorOutput(pass, storage);
          publishWritesForDisplay(pass, plan, storage, buffer, displayTonemap);
          disabledMessage = "disabled pass passed color through";
          break;
        case DisabledBehavior::CullDependents:
          disabledMessage = "disabled pass culled dependents";
          break;
        case DisabledBehavior::Error:
          disabledMessage = "disabled pass did not execute";
          break;
        }
        if (traceSession) {
          p->executionTraceRecorder->passSkipped(traceSession.session, pass, storage,
                                                 disabledMessage);
        }
        continue;
      }

      auto payload = RenderPassPayload::createBuiltin(pass);
      if (!payload) {
        throw std::runtime_error("GraphRenderEngine cannot execute enabled pass '" + pass.id +
                                 "' with kind '" + toString(pass.kind) + "' and executor '" +
                                 toString(pass.executor) + "'");
      }

      RenderExecutionContext context(pass, storage, *this, p->cancelled.load(), setActiveEngine);
      struct ActiveEngineReset {
        RenderExecutionContext& context;
        ~ActiveEngineReset() {
          context.clearActiveEngine();
        }
      } reset{context};

      executeObserved(*this, p->executionTraceRecorder, traceSession.session, renderGeneration,
                      pass, storage, [&] {
                        const bool executedForDisplay =
                          pass.kind == RenderPassKind::Beauty &&
                          pass.executor == RenderExecutorKind::Raytracer &&
                          payload->executeDisplayAndStore(context, buffer, displayTonemap);
                        if (!executedForDisplay) {
                          payload->execute(context);
                        }
                      });

      for (const auto& write : pass.writes) {
        storage.resource(write.resource).markProduced();
      }
      publishWritesForDisplay(pass, plan, storage, buffer, displayTonemap);
    }

    packColorBuffer(storage.color(plan.exportedColorResource().id), buffer);
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
