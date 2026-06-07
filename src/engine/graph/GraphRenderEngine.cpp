#include "engine/graph/GraphRenderEngine.h"

#include "core/Buffer.h"
#include "core/util/BufferUtils.h"
#include "engine/graph/RenderGraphArtifactCache.h"
#include "engine/graph/RenderPassPayload.h"
#include "engine/graph/RenderExecutionContext.h"
#include "engine/graph/RenderGraphExecutionObserver.h"
#include "engine/graph/RenderGraphExecutionTrace.h"
#include "engine/graph/RenderResourceStorage.h"
#include "engine/raster/RasterVisibilitySceneCache.h"
#include "core/math/BoundingBox.h"
#include "render/cameras/Camera.h"
#include "render/lights/Light.h"
#include "render/materials/Material.h"
#include "render/primitives/Scene.h"
#include "render/tonemap/LinearTonemap.h"
#include "render/tonemap/Tonemap.h"

#include <QJsonObject>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <thread>
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

    struct ExternalResourceBindings {
      std::map<RenderResourceId, std::shared_ptr<const Buffer<Colord>>> colorResources;
      std::map<RenderResourceId, std::shared_ptr<const Buffer<double>>> depthResources;
      std::map<RenderResourceId, std::shared_ptr<const Buffer<std::uint8_t>>> stencilResources;
      std::map<RenderResourceId, std::shared_ptr<const Buffer<std::uint32_t>>> objectIdResources;

      bool contains(const RenderResourceId& id) const {
        return colorResources.find(id) != colorResources.end() ||
               depthResources.find(id) != depthResources.end() ||
               stencilResources.find(id) != stencilResources.end() ||
               objectIdResources.find(id) != objectIdResources.end();
      }

      void bind(RenderResourceStorage& storage, const RenderResourceId& id) const {
        const auto colorIt = colorResources.find(id);
        if (colorIt != colorResources.end() && colorIt->second) {
          storage.bindColor(id, *colorIt->second);
          return;
        }

        const auto depthIt = depthResources.find(id);
        if (depthIt != depthResources.end() && depthIt->second) {
          storage.bindDepth(id, *depthIt->second);
          return;
        }

        const auto stencilIt = stencilResources.find(id);
        if (stencilIt != stencilResources.end() && stencilIt->second) {
          storage.bindStencil(id, *stencilIt->second);
          return;
        }

        const auto objectIdIt = objectIdResources.find(id);
        if (objectIdIt != objectIdResources.end() && objectIdIt->second) {
          storage.bindObjectId(id, *objectIdIt->second);
        }
      }

      void setColor(RenderResourceId id, std::shared_ptr<const Buffer<Colord>> buffer) {
        depthResources.erase(id);
        stencilResources.erase(id);
        objectIdResources.erase(id);
        colorResources[std::move(id)] = std::move(buffer);
      }

      void setDepth(RenderResourceId id, std::shared_ptr<const Buffer<double>> buffer) {
        colorResources.erase(id);
        stencilResources.erase(id);
        objectIdResources.erase(id);
        depthResources[std::move(id)] = std::move(buffer);
      }

      void setStencil(RenderResourceId id, std::shared_ptr<const Buffer<std::uint8_t>> buffer) {
        colorResources.erase(id);
        depthResources.erase(id);
        objectIdResources.erase(id);
        stencilResources[std::move(id)] = std::move(buffer);
      }

      void setObjectId(RenderResourceId id, std::shared_ptr<const Buffer<std::uint32_t>> buffer) {
        colorResources.erase(id);
        depthResources.erase(id);
        stencilResources.erase(id);
        objectIdResources[std::move(id)] = std::move(buffer);
      }

      void clear(const RenderResourceId& id) {
        colorResources.erase(id);
        depthResources.erase(id);
        stencilResources.erase(id);
        objectIdResources.erase(id);
      }

      void clear() {
        colorResources.clear();
        depthResources.clear();
        stencilResources.clear();
        objectIdResources.clear();
      }
    };

    void requireExternalInputsBound(const RenderPlan& plan,
                                    const ExternalResourceBindings& bindings) {
      const auto externalInputs = plan.externalInputResourceIds();
      std::vector<RenderResourceId> missingInputs;
      for (const auto& input : externalInputs) {
        if (!bindings.contains(input)) {
          missingInputs.push_back(input);
        }
      }

      if (missingInputs.empty()) {
        return;
      }

      std::ostringstream out;
      out << "render plan requires external resource '" << missingInputs.front() << "'";
      if (missingInputs.size() > 1) {
        out << " and " << (missingInputs.size() - 1) << " more";
      }
      out << ", but it was not bound";
      throw std::runtime_error(out.str());
    }

    void bindExternalInputs(RenderResourceStorage& storage, const RenderPlan& plan,
                            const ExternalResourceBindings& bindings) {
      for (const auto& id : plan.externalInputResourceIds()) {
        bindings.bind(storage, id);
      }
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
      if (!core::util::bufferDimensionsEqual(source, destination)) {
        throw std::runtime_error(action + " requires matching color buffer dimensions");
      }
    }

    void requireMatchingSize(const Buffer<Colord>& source, const Buffer<unsigned int>& destination,
                             const std::string& action) {
      if (!core::util::bufferDimensionsEqual(source, destination)) {
        throw std::runtime_error(action + " requires matching color buffer dimensions");
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

    template<int Dimensions, class T, class VectorType, class Derived>
    void writeMatrixFingerprint(std::ostream& out, const std::string& name,
                                const Matrix<Dimensions, T, VectorType, Derived>& matrix) {
      out << name << '=';
      for (int row = 0; row != Dimensions; ++row) {
        for (int col = 0; col != Dimensions; ++col) {
          if (row != 0 || col != 0) {
            out << ',';
          }
          out << matrix[row][col];
        }
      }
      out << ';';
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

    void writeMaterialCullingFingerprint(std::ostream& out,
                                         const std::shared_ptr<render::Material>& material,
                                         const std::string& prefix) {
      if (!material) {
        out << prefix << "material=null;";
        return;
      }

      out << prefix << "material.ptr=" << material.get() << ';' << prefix
          << "material.sidedness=" << static_cast<int>(material->sidedness()) << ';';
    }

    void writeLightFingerprint(std::ostream& out, const std::shared_ptr<render::Light>& light,
                               std::size_t index) {
      const std::string prefix = "light[" + std::to_string(index) + "].";
      if (!light) {
        out << prefix << "null=true;";
        return;
      }

      light->writeFingerprint(out, prefix);
    }

    void writeCameraFingerprint(std::ostream& out, const std::shared_ptr<render::Camera>& camera) {
      if (camera) {
        out << "camera.type=" << camera->fingerprintType() << ';'
            << "camera.aspectMode=" << static_cast<int>(camera->aspectMode()) << ';'
            << "camera.aspectRatio=" << camera->aspectRatio() << ';';
        writeVectorFingerprint(out, "camera.position", camera->position());
        writeVectorFingerprint(out, "camera.target", camera->target());
      } else {
        out << "camera=null;";
      }
    }

    void writeCameraFingerprint(std::ostream& out, const GraphRenderEngine& graph) {
      writeCameraFingerprint(out, graph.camera());
    }

    void writeSceneFingerprint(std::ostream& out, const GraphRenderEngine& graph) {
      if (auto scene = graph.scene()) {
        out << "scene.ptr=" << scene.get() << ';' << "scene.lights=" << scene->lights().size()
            << ';';
        writeColorFingerprint(out, "scene.ambient", scene->ambient());
        writeColorFingerprint(out, "scene.background", scene->background());
        writeBoundingBoxFingerprint(out, scene->boundingBox());
        std::size_t lightIndex = 0;
        for (const auto& light : scene->lights()) {
          writeLightFingerprint(out, light, lightIndex++);
        }
      } else {
        out << "scene=null;";
      }
    }

    void writeSceneGeometryFingerprint(std::ostream& out, const GraphRenderEngine& graph) {
      if (auto scene = graph.scene()) {
        out << "scene.ptr=" << scene.get() << ';';
        writeBoundingBoxFingerprint(out, scene->boundingBox());

        std::size_t leafIndex = 0;
        static_cast<const render::Primitive&>(*scene).forEachTransformedLeaf(
          [&](const render::Primitive::TransformedLeaf& leaf) {
            const std::string prefix = "leaf[" + std::to_string(leafIndex++) + "].";
            out << prefix << "primitive.ptr=" << leaf.primitive << ';';
            writeMaterialCullingFingerprint(out, leaf.material, prefix);
            writeBoundingBoxFingerprint(out, leaf.boundingBox());
            writeMatrixFingerprint(out, prefix + "pointMatrix", leaf.pointMatrix);
            writeMatrixFingerprint(out, prefix + "normalMatrix", leaf.normalMatrix);
          });
        out << "scene.leaves=" << leafIndex << ';';
      } else {
        out << "scene=null;";
      }
    }

    void writeDisplayFingerprint(std::ostream& out, const GraphRenderEngine& graph) {
      writeColorFingerprint(out, "engine.background", graph.backgroundColor());
      if (auto tonemap = graph.tonemap()) {
        out << "tonemap.type=" << tonemap->fingerprintType() << ';';
      } else {
        out << "tonemap=null;";
      }
    }

    std::string renderInputFingerprintFor(const GraphRenderEngine& graph) {
      std::ostringstream out;
      out << std::setprecision(17);
      writeCameraFingerprint(out, graph);
      writeSceneFingerprint(out, graph);
      writeDisplayFingerprint(out, graph);
      return out.str();
    }

    std::string shadowCacheInputFingerprintFor(const GraphRenderEngine& graph,
                                               const RenderPassNode& pass) {
      std::ostringstream out;
      out << std::setprecision(17);
      writeCameraFingerprint(out, graph.cameraForPass(pass));
      writeSceneFingerprint(out, graph);
      return out.str();
    }

    std::string visibilityCacheInputFingerprintFor(const GraphRenderEngine& graph,
                                                   const RenderPassNode& pass) {
      std::ostringstream out;
      out << std::setprecision(17);
      writeCameraFingerprint(out, graph.cameraForPass(pass));
      writeSceneGeometryFingerprint(out, graph);
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
          requireMatchingSize(source, destination, "color copy");
          core::util::copyBuffer(destination, source);
        }
        storage.resource(write.resource).markProduced();
      }
    }

    class DisabledPassHandler {
    public:
      virtual ~DisabledPassHandler() = default;

      virtual DisabledBehavior behavior() const = 0;
      virtual void apply(const RenderPassNode& pass, RenderResourceStorage& storage,
                         const GraphRenderEngine& graph) const = 0;
      virtual std::string message() const = 0;
      virtual bool publishesWrites() const {
        return false;
      }

      bool matches(DisabledBehavior disabledBehavior) const {
        return behavior() == disabledBehavior;
      }
    };

    class SubstituteDefaultDisabledPassHandler : public DisabledPassHandler {
    public:
      DisabledBehavior behavior() const override {
        return DisabledBehavior::SubstituteDefault;
      }

      void apply(const RenderPassNode& pass, RenderResourceStorage& storage,
                 const GraphRenderEngine& graph) const override {
        substituteDefaultOutput(pass, storage, graph);
      }

      std::string message() const override {
        return "disabled pass substituted default output";
      }

      bool publishesWrites() const override {
        return true;
      }
    };

    class PassthroughDisabledPassHandler : public DisabledPassHandler {
    public:
      DisabledBehavior behavior() const override {
        return DisabledBehavior::Passthrough;
      }

      void apply(const RenderPassNode& pass, RenderResourceStorage& storage,
                 const GraphRenderEngine&) const override {
        passthroughColorOutput(pass, storage);
      }

      std::string message() const override {
        return "disabled pass passed color through";
      }

      bool publishesWrites() const override {
        return true;
      }
    };

    class CullDependentsDisabledPassHandler : public DisabledPassHandler {
    public:
      DisabledBehavior behavior() const override {
        return DisabledBehavior::CullDependents;
      }

      void apply(const RenderPassNode&, RenderResourceStorage&,
                 const GraphRenderEngine&) const override {
      }

      std::string message() const override {
        return "disabled pass culled dependents";
      }
    };

    class ErrorDisabledPassHandler : public DisabledPassHandler {
    public:
      DisabledBehavior behavior() const override {
        return DisabledBehavior::Error;
      }

      void apply(const RenderPassNode&, RenderResourceStorage&,
                 const GraphRenderEngine&) const override {
      }

      std::string message() const override {
        return "disabled pass did not execute";
      }
    };

    const std::vector<const DisabledPassHandler*>& disabledPassHandlers() {
      static const SubstituteDefaultDisabledPassHandler substituteDefault;
      static const PassthroughDisabledPassHandler passthrough;
      static const CullDependentsDisabledPassHandler cullDependents;
      static const ErrorDisabledPassHandler error;
      static const std::vector<const DisabledPassHandler*> result = {
        &substituteDefault, &passthrough, &cullDependents, &error};
      return result;
    }

    const DisabledPassHandler& disabledPassHandler(DisabledBehavior disabledBehavior) {
      const auto& all = disabledPassHandlers();
      const auto it = std::find_if(all.begin(), all.end(), [&](const auto* handler) {
        return handler->matches(disabledBehavior);
      });
      if (it == all.end()) {
        throw std::runtime_error("unsupported disabled render graph pass behavior");
      }
      return **it;
    }

    const DisabledPassHandler& applyDisabledPass(const RenderPassNode& pass,
                                                 RenderResourceStorage& storage,
                                                 const GraphRenderEngine& graph) {
      const DisabledPassHandler& handler = disabledPassHandler(pass.disabledBehavior);
      handler.apply(pass, storage, graph);
      return handler;
    }

    bool planAppliesTonemap(const RenderPlan& plan) {
      return std::any_of(plan.passes().begin(), plan.passes().end(), [](const auto& pass) {
        return pass.enabled && pass.kind == RenderPassKind::Tonemap &&
               pass.executor == RenderExecutorKind::PostProcess;
      });
    }

    std::map<std::string, std::shared_ptr<render::Camera>>
    cloneSceneCameras(const std::map<std::string, std::shared_ptr<render::Camera>>& cameras) {
      std::map<std::string, std::shared_ptr<render::Camera>> result;
      for (const auto& [id, camera] : cameras) {
        result.emplace(id, camera ? camera->clone() : nullptr);
      }
      return result;
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
        if (!plan.resourceCanReach(write.resource, output.id)) {
          continue;
        }

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
                         const RenderResourceStorage& storage, const QJsonObject* metadata,
                         Execute execute) {
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
        recorder->passCompleted(traceSession, pass, storage, metadata ? *metadata : QJsonObject());
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

    enum class ScheduledPassResult { Completed, SkippedWithoutWrites, SkippedWithWrites };

    bool passLimitAllowsStart(const RenderPassNode& pass, int runningForExecutor,
                              bool serialRunningForExecutor) {
      if (pass.concurrency.mode == RenderConcurrencyMode::Serial) {
        return runningForExecutor == 0;
      }
      if (serialRunningForExecutor) {
        return false;
      }
      if (pass.concurrency.mode == RenderConcurrencyMode::Limited) {
        return runningForExecutor < std::max(1, pass.concurrency.maxConcurrentPasses);
      }
      return true;
    }

    std::string skippedAfterFailureMessage(const RenderPassNode& failed,
                                           const RenderPassNode& skipped) {
      return "pass '" + skipped.id + "' skipped because dependency '" + failed.id + "' failed";
    }

    std::size_t maxGraphWorkerCount(std::size_t passCount) {
      if (passCount <= 1) {
        return 1;
      }
      const unsigned int hardware = std::thread::hardware_concurrency();
      const std::size_t available = hardware == 0 ? 2 : static_cast<std::size_t>(hardware);
      return std::max<std::size_t>(2, std::min(passCount, available));
    }

    struct GraphExecutionRuntime {
      std::atomic<bool>& cancelled;
      std::shared_ptr<RenderGraphExecutionTraceRecorder> traceRecorder;
    };

    template<class ExecutePass, class AfterPass>
    void executeDependencyReadyPasses(const GraphExecutionRuntime& runtime,
                                      const RenderPlan& plan, RenderResourceStorage& storage,
                                      const TraceSession& traceSession, ExecutePass executePass,
                                      AfterPass afterPass) {
      const auto executionOrder = plan.executionOrder();
      if (executionOrder.empty()) {
        return;
      }

      std::map<RenderPassId, std::size_t> indexByPass;
      for (std::size_t index = 0; index != executionOrder.size(); ++index) {
        indexByPass.emplace(executionOrder[index]->id, index);
      }

      std::vector<std::vector<std::size_t>> dependents(executionOrder.size());
      std::vector<int> unsatisfiedDependencies(executionOrder.size(), 0);
      for (const auto& dependency : plan.dependencies()) {
        const auto producer = indexByPass.find(dependency.producer->id);
        const auto consumer = indexByPass.find(dependency.consumer->id);
        if (producer == indexByPass.end() || consumer == indexByPass.end()) {
          continue;
        }
        dependents[producer->second].push_back(consumer->second);
        ++unsatisfiedDependencies[consumer->second];
      }

      enum class PassState { Pending, Ready, Running, Completed, Skipped, Failed };

      std::mutex mutex;
      std::condition_variable changed;
      std::deque<std::size_t> ready;
      std::vector<PassState> state(executionOrder.size(), PassState::Pending);
      std::map<RenderExecutorKind, int> runningByExecutor;
      std::map<RenderExecutorKind, int> serialRunningByExecutor;
      std::size_t unfinished = executionOrder.size();
      std::exception_ptr firstFailure;
      std::size_t firstFailureIndex = executionOrder.size();
      bool stopScheduling = false;
      bool cancelled = runtime.cancelled.load();

      auto traceSkipped = [&](std::size_t index, const std::string& message) {
        if (traceSession) {
          runtime.traceRecorder->passSkipped(traceSession.session, *executionOrder[index],
                                             storage, message);
        }
      };

      auto skipPending = [&](auto& self, std::size_t index, const std::string& message) -> void {
        if (state[index] == PassState::Completed || state[index] == PassState::Skipped ||
            state[index] == PassState::Failed || state[index] == PassState::Running) {
          return;
        }
        state[index] = PassState::Skipped;
        --unfinished;
        traceSkipped(index, message);
        for (std::size_t dependent : dependents[index]) {
          self(self, dependent, message);
        }
      };

      for (std::size_t index = 0; index != executionOrder.size(); ++index) {
        if (unsatisfiedDependencies[index] == 0) {
          state[index] = PassState::Ready;
          ready.push_back(index);
        }
      }

      auto claimReadyPass = [&]() -> std::optional<std::size_t> {
        for (auto it = ready.begin(); it != ready.end(); ++it) {
          const RenderPassNode& pass = *executionOrder[*it];
          const int running = runningByExecutor[pass.executor];
          const bool serialRunning = serialRunningByExecutor[pass.executor] > 0;
          if (!passLimitAllowsStart(pass, running, serialRunning)) {
            continue;
          }

          const std::size_t index = *it;
          ready.erase(it);
          state[index] = PassState::Running;
          ++runningByExecutor[pass.executor];
          if (pass.concurrency.mode == RenderConcurrencyMode::Serial) {
            ++serialRunningByExecutor[pass.executor];
          }
          return index;
        }
        return std::nullopt;
      };

      auto hasStartablePass = [&]() {
        if (stopScheduling) {
          return false;
        }
        return std::any_of(ready.begin(), ready.end(), [&](std::size_t index) {
          const RenderPassNode& pass = *executionOrder[index];
          const int running = runningByExecutor[pass.executor];
          const bool serialRunning = serialRunningByExecutor[pass.executor] > 0;
          return passLimitAllowsStart(pass, running, serialRunning);
        });
      };

      auto completePass = [&](std::size_t index, ScheduledPassResult result) {
        std::lock_guard<std::mutex> lock(mutex);
        const RenderPassNode& pass = *executionOrder[index];
        --runningByExecutor[pass.executor];
        if (pass.concurrency.mode == RenderConcurrencyMode::Serial) {
          --serialRunningByExecutor[pass.executor];
        }
        state[index] = result == ScheduledPassResult::Completed ? PassState::Completed
                                                                : PassState::Skipped;
        --unfinished;

        const bool publishesWrites = result != ScheduledPassResult::SkippedWithoutWrites;
        if (!publishesWrites) {
          for (std::size_t dependent : dependents[index]) {
            skipPending(skipPending, dependent,
                        "pass '" + executionOrder[dependent]->id +
                          "' skipped because dependency '" + pass.id +
                          "' did not produce its outputs");
          }
        } else {
          for (std::size_t dependent : dependents[index]) {
            if (state[dependent] != PassState::Pending) {
              continue;
            }
            --unsatisfiedDependencies[dependent];
            if (unsatisfiedDependencies[dependent] == 0) {
              state[dependent] = PassState::Ready;
              ready.push_back(dependent);
            }
          }
        }
        changed.notify_all();
      };

      auto failPass = [&](std::size_t index, std::exception_ptr failure) {
        std::lock_guard<std::mutex> lock(mutex);
        const RenderPassNode& pass = *executionOrder[index];
        --runningByExecutor[pass.executor];
        if (pass.concurrency.mode == RenderConcurrencyMode::Serial) {
          --serialRunningByExecutor[pass.executor];
        }
        state[index] = PassState::Failed;
        --unfinished;
        if (!firstFailure || index < firstFailureIndex) {
          firstFailure = failure;
          firstFailureIndex = index;
        }
        stopScheduling = true;
        for (std::size_t dependent : dependents[index]) {
          skipPending(skipPending, dependent,
                      skippedAfterFailureMessage(pass, *executionOrder[dependent]));
        }
        for (std::size_t queuedIndex = 0; queuedIndex != executionOrder.size(); ++queuedIndex) {
          if (state[queuedIndex] == PassState::Pending || state[queuedIndex] == PassState::Ready) {
            skipPending(skipPending, queuedIndex,
                        "pass '" + executionOrder[queuedIndex]->id +
                          "' skipped after render graph pass failure");
          }
        }
        ready.clear();
        changed.notify_all();
      };

      auto cancelQueued = [&]() {
        if (cancelled) {
          return;
        }
        cancelled = true;
        stopScheduling = true;
        for (std::size_t index = 0; index != executionOrder.size(); ++index) {
          if (state[index] == PassState::Pending || state[index] == PassState::Ready) {
            skipPending(skipPending, index,
                        "pass '" + executionOrder[index]->id +
                          "' skipped because render graph execution was cancelled");
          }
        }
        ready.clear();
      };

      auto worker = [&]() {
        while (true) {
          std::size_t index = 0;
          {
            std::unique_lock<std::mutex> lock(mutex);
            changed.wait(lock, [&] {
              if (runtime.cancelled.load()) {
                cancelQueued();
              }
              return unfinished == 0 || hasStartablePass();
            });

            if (unfinished == 0) {
              return;
            }
            if (stopScheduling) {
              continue;
            }

            auto claimed = claimReadyPass();
            if (!claimed) {
              continue;
            }
            index = *claimed;
          }

          try {
            const ScheduledPassResult result = executePass(*executionOrder[index]);
            afterPass(*executionOrder[index], result);
            completePass(index, result);
          } catch (...) {
            failPass(index, std::current_exception());
          }
        }
      };

      std::vector<std::thread> workers;
      const std::size_t workerCount = maxGraphWorkerCount(executionOrder.size());
      workers.reserve(workerCount);
      for (std::size_t i = 0; i != workerCount; ++i) {
        workers.emplace_back(worker);
      }
      changed.notify_all();
      for (auto& thread : workers) {
        thread.join();
      }

      if (firstFailure) {
        std::rethrow_exception(firstFailure);
      }
      if (cancelled || runtime.cancelled.load()) {
        throw std::runtime_error("render graph execution cancelled");
      }
    }
  }

  struct GraphRenderEngine::Private {
    RenderIntent intent;
    RenderSceneAnalysis sceneAnalysis{RenderSceneAnalysis::unknownScene()};
    std::optional<RenderPlan> explicitPlan;
    RenderPlan lastPlan;
    std::map<std::string, std::shared_ptr<render::Camera>> sceneCameras;
    ExternalResourceBindings externalResources;
    std::vector<std::shared_ptr<render::RenderEngine>> activeEngines;
    std::shared_ptr<RenderGraphExecutionObserver> executionObserver;
    std::shared_ptr<RenderGraphExecutionTraceRecorder> executionTraceRecorder{
      std::make_shared<RenderGraphExecutionTraceRecorder>()};
    std::shared_ptr<RenderGraphArtifactCache> artifactCache{
      std::make_shared<RenderGraphArtifactCache>()};
    std::shared_ptr<engine::raster::RasterVisibilitySceneCache> rasterVisibilitySceneCache{
      std::make_shared<engine::raster::RasterVisibilitySceneCache>()};
    std::shared_ptr<std::atomic<std::uint64_t>> nextExecutionGeneration{
      std::make_shared<std::atomic<std::uint64_t>>(1)};
    std::atomic<bool> executionTraceEnabled{false};
    std::atomic<bool> cancelled{false};
    mutable std::mutex activeEngineMutex;
    mutable std::mutex executionObserverMutex;

    std::function<void(std::shared_ptr<render::RenderEngine>)> activeEngineSetter() {
      auto slot = std::make_shared<std::shared_ptr<render::RenderEngine>>();
      return [this, slot](std::shared_ptr<render::RenderEngine> engine) {
        std::lock_guard<std::mutex> lock(activeEngineMutex);
        if (*slot) {
          const auto it = std::find(activeEngines.begin(), activeEngines.end(), *slot);
          if (it != activeEngines.end()) {
            activeEngines.erase(it);
          }
        }
        *slot = std::move(engine);
        if (*slot) {
          activeEngines.push_back(*slot);
        }
      };
    }

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
    copyRenderEngineStateTo(*result);
    result->setIntent(p->intent);
    result->setSceneAnalysis(p->sceneAnalysis);
    if (p->explicitPlan) {
      result->setPlan(*p->explicitPlan);
    }
    result->p->sceneCameras = cloneSceneCameras(p->sceneCameras);
    result->p->externalResources = p->externalResources;
    result->setExecutionObserver(executionObserver());
    result->setExecutionTraceEnabled(executionTraceEnabled());
    result->p->executionTraceRecorder = p->executionTraceRecorder;
    result->p->artifactCache = p->artifactCache;
    result->p->rasterVisibilitySceneCache = p->rasterVisibilitySceneCache;
    result->p->nextExecutionGeneration = p->nextExecutionGeneration;
    return result;
  }

  void GraphRenderEngine::setIntent(RenderIntent intent) {
    p->intent = std::move(intent);
  }

  const RenderIntent& GraphRenderEngine::intent() const {
    return p->intent;
  }

  void GraphRenderEngine::setSceneAnalysis(RenderSceneAnalysis analysis) {
    p->sceneAnalysis = std::move(analysis);
  }

  void GraphRenderEngine::clearSceneAnalysis() {
    p->sceneAnalysis = RenderSceneAnalysis::unknownScene();
  }

  const RenderSceneAnalysis& GraphRenderEngine::sceneAnalysis() const {
    return p->sceneAnalysis;
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

  void GraphRenderEngine::setSceneCamera(std::string sceneCameraId,
                                         std::shared_ptr<render::Camera> camera) {
    if (sceneCameraId.empty()) {
      throw std::runtime_error("scene camera id must not be empty");
    }
    if (!camera) {
      throw std::runtime_error("scene camera '" + sceneCameraId + "' must not be null");
    }
    p->sceneCameras.insert_or_assign(std::move(sceneCameraId), std::move(camera));
  }

  void GraphRenderEngine::clearSceneCameras() {
    p->sceneCameras.clear();
  }

  std::shared_ptr<render::Camera>
  GraphRenderEngine::cameraForPass(const RenderPassNode& pass) const {
    if (pass.sceneView.camera && pass.sceneView.camera->sceneCameraId) {
      const auto camera = p->sceneCameras.find(*pass.sceneView.camera->sceneCameraId);
      if (camera != p->sceneCameras.end()) {
        return camera->second;
      }
    }
    return camera();
  }

  void GraphRenderEngine::setExternalColorResource(RenderResourceId id,
                                                   std::shared_ptr<const Buffer<Colord>> buffer) {
    if (id.empty()) {
      throw std::runtime_error("external color resource id must not be empty");
    }
    if (!buffer) {
      throw std::runtime_error("external color resource '" + id + "' must not be null");
    }
    p->externalResources.setColor(std::move(id), std::move(buffer));
  }

  void GraphRenderEngine::setExternalDepthResource(RenderResourceId id,
                                                   std::shared_ptr<const Buffer<double>> buffer) {
    if (id.empty()) {
      throw std::runtime_error("external depth resource id must not be empty");
    }
    if (!buffer) {
      throw std::runtime_error("external depth resource '" + id + "' must not be null");
    }
    p->externalResources.setDepth(std::move(id), std::move(buffer));
  }

  void GraphRenderEngine::setExternalStencilResource(
    RenderResourceId id, std::shared_ptr<const Buffer<std::uint8_t>> buffer) {
    if (id.empty()) {
      throw std::runtime_error("external stencil resource id must not be empty");
    }
    if (!buffer) {
      throw std::runtime_error("external stencil resource '" + id + "' must not be null");
    }
    p->externalResources.setStencil(std::move(id), std::move(buffer));
  }

  void GraphRenderEngine::setExternalObjectIdResource(
    RenderResourceId id, std::shared_ptr<const Buffer<std::uint32_t>> buffer) {
    if (id.empty()) {
      throw std::runtime_error("external object-id resource id must not be empty");
    }
    if (!buffer) {
      throw std::runtime_error("external object-id resource '" + id + "' must not be null");
    }
    p->externalResources.setObjectId(std::move(id), std::move(buffer));
  }

  void GraphRenderEngine::clearExternalResource(const RenderResourceId& id) {
    p->externalResources.clear(id);
  }

  void GraphRenderEngine::clearExternalResources() {
    p->externalResources.clear();
  }

  RenderPlan GraphRenderEngine::compilePlan(const RenderTargetSpec& target) const {
    RenderGraphCompiler compiler;
    return compiler.compile(target, p->intent, p->sceneAnalysis);
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

  std::string GraphRenderEngine::cacheInputFingerprintForPass(const RenderPassNode& pass) const {
    if (pass.kind == RenderPassKind::Shadow && pass.executor == RenderExecutorKind::Rasterizer) {
      return shadowCacheInputFingerprintFor(*this, pass);
    }
    if (pass.kind == RenderPassKind::Visibility &&
        pass.executor == RenderExecutorKind::Rasterizer) {
      return visibilityCacheInputFingerprintFor(*this, pass);
    }
    return executionInputFingerprint();
  }

  std::shared_ptr<RenderGraphArtifactCache> GraphRenderEngine::artifactCache() const {
    return p->artifactCache;
  }

  std::shared_ptr<engine::raster::RasterVisibilitySceneCache>
  GraphRenderEngine::rasterVisibilitySceneCache() const {
    return p->rasterVisibilitySceneCache;
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
    if (plan.hasMultipleExecutionCameraRefs()) {
      throw std::runtime_error("GraphRenderEngine cannot execute a plan with multiple scene "
                               "camera references yet");
    }
    requireExternalInputsBound(plan, p->externalResources);
    const std::uint64_t renderGeneration = p->claimExecutionGeneration();
    TraceSession traceSession = p->beginTraceIfEnabled(plan, executionInputFingerprint());
    notifyRenderStarted(*this, renderGeneration);

    RenderResourceStorage storage;
    storage.allocate(plan.resources());
    bindExternalInputs(storage, plan, p->externalResources);

    GraphExecutionRuntime runtime{p->cancelled, p->executionTraceRecorder};
    auto executePass = [&](const RenderPassNode& pass) {
      if (!pass.enabled) {
        const DisabledPassHandler& handler = applyDisabledPass(pass, storage, *this);
        if (traceSession) {
          p->executionTraceRecorder->passSkipped(traceSession.session, pass, storage,
                                                 handler.message());
        }
        return handler.publishesWrites() ? ScheduledPassResult::SkippedWithWrites
                                         : ScheduledPassResult::SkippedWithoutWrites;
      }
      auto recordTraceMessage = [recorder = p->executionTraceRecorder,
                                 session = traceSession.session, &pass](std::string message) {
        if (recorder && session) {
          recorder->recordPassMessage(session, pass, std::move(message));
        }
      };

      RenderExecutionContext context(pass, storage, *this, p->cancelled.load(),
                                     p->activeEngineSetter(), recordTraceMessage);
      struct ActiveEngineReset {
        RenderExecutionContext& context;
        ~ActiveEngineReset() {
          context.clearActiveEngine();
        }
      } reset{context};

      executeObserved(*this, p->executionTraceRecorder, traceSession.session, renderGeneration,
                      pass, storage, &context.traceMetadata(), [&] {
                        auto payload = RenderPassPayload::createBuiltin(pass);
                        if (!payload) {
                          throw std::runtime_error(
                            "GraphRenderEngine cannot execute enabled pass '" + pass.id +
                            "' with kind '" + toString(pass.kind) + "' and executor '" +
                            toString(pass.executor) + "'");
                        }
                        payload->execute(context);
                      });
      for (const auto& write : pass.writes) {
        storage.resource(write.resource).markProduced();
      }
      return ScheduledPassResult::Completed;
    };
    auto afterPass = [](const RenderPassNode&, ScheduledPassResult) {};

    executeDependencyReadyPasses(runtime, plan, storage, traceSession, executePass, afterPass);

    requireMatchingSize(storage.color(plan.exportedColorResource().id), buffer, "color copy");
    core::util::copyBuffer(buffer, storage.color(plan.exportedColorResource().id));
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
    if (plan.hasMultipleExecutionCameraRefs()) {
      throw std::runtime_error("GraphRenderEngine cannot execute a plan with multiple scene "
                               "camera references yet");
    }
    requireExternalInputsBound(plan, p->externalResources);
    requireMatchingOutputSize(plan, buffer.width(), buffer.height());
    const std::uint64_t renderGeneration = p->claimExecutionGeneration();
    TraceSession traceSession = p->beginTraceIfEnabled(plan, executionInputFingerprint());
    notifyRenderStarted(*this, renderGeneration);

    RenderResourceStorage storage;
    storage.allocate(plan.resources());
    bindExternalInputs(storage, plan, p->externalResources);
    const auto displayTonemap = displayTonemapForPlan(plan, *this);

    GraphExecutionRuntime runtime{p->cancelled, p->executionTraceRecorder};
    auto executePass = [&](const RenderPassNode& pass) {
      if (!pass.enabled) {
        const DisabledPassHandler& handler = applyDisabledPass(pass, storage, *this);
        if (traceSession) {
          p->executionTraceRecorder->passSkipped(traceSession.session, pass, storage,
                                                 handler.message());
        }
        return handler.publishesWrites() ? ScheduledPassResult::SkippedWithWrites
                                         : ScheduledPassResult::SkippedWithoutWrites;
      }

      auto recordTraceMessage = [recorder = p->executionTraceRecorder,
                                 session = traceSession.session, &pass](std::string message) {
        if (recorder && session) {
          recorder->recordPassMessage(session, pass, std::move(message));
        }
      };

      RenderExecutionContext context(pass, storage, *this, p->cancelled.load(),
                                     p->activeEngineSetter(), recordTraceMessage);
      struct ActiveEngineReset {
        RenderExecutionContext& context;
        ~ActiveEngineReset() {
          context.clearActiveEngine();
        }
      } reset{context};

      executeObserved(*this, p->executionTraceRecorder, traceSession.session, renderGeneration,
                      pass, storage, &context.traceMetadata(), [&] {
                        auto payload = RenderPassPayload::createBuiltin(pass);
                        if (!payload) {
                          throw std::runtime_error(
                            "GraphRenderEngine cannot execute enabled pass '" + pass.id +
                            "' with kind '" + toString(pass.kind) + "' and executor '" +
                            toString(pass.executor) + "'");
                        }
                        const bool executedForDisplay =
                          pass.kind == RenderPassKind::Beauty &&
                          (pass.executor == RenderExecutorKind::Raytracer ||
                           pass.executor == RenderExecutorKind::Wavefront) &&
                          payload->executeDisplayAndStore(context, buffer, displayTonemap);
                        if (!executedForDisplay) {
                          payload->execute(context);
                        }
                      });

      for (const auto& write : pass.writes) {
        storage.resource(write.resource).markProduced();
      }
      return ScheduledPassResult::Completed;
    };
    auto afterPass = [&](const RenderPassNode& pass, ScheduledPassResult result) {
      if (result != ScheduledPassResult::SkippedWithoutWrites) {
        publishWritesForDisplay(pass, plan, storage, buffer, displayTonemap);
      }
    };

    executeDependencyReadyPasses(runtime, plan, storage, traceSession, executePass, afterPass);

    packColorBuffer(storage.color(plan.exportedColorResource().id), buffer);
  }

  void GraphRenderEngine::cancel() {
    p->cancelled.store(true);
    std::vector<std::shared_ptr<render::RenderEngine>> active;
    {
      std::lock_guard<std::mutex> lock(p->activeEngineMutex);
      active = p->activeEngines;
    }
    for (const auto& engine : active) {
      if (engine) {
        engine->cancel();
      }
    }
  }

  void GraphRenderEngine::uncancel() {
    p->cancelled.store(false);
    std::vector<std::shared_ptr<render::RenderEngine>> active;
    {
      std::lock_guard<std::mutex> lock(p->activeEngineMutex);
      active = p->activeEngines;
    }
    for (const auto& engine : active) {
      if (engine) {
        engine->uncancel();
      }
    }
  }

  std::list<Recti> GraphRenderEngine::activeTiles() const {
    std::vector<std::shared_ptr<render::RenderEngine>> active;
    {
      std::lock_guard<std::mutex> lock(p->activeEngineMutex);
      active = p->activeEngines;
    }
    std::list<Recti> result;
    for (const auto& engine : active) {
      if (!engine) {
        continue;
      }
      const auto tiles = engine->activeTiles();
      result.insert(result.end(), tiles.begin(), tiles.end());
    }
    return result;
  }

  std::list<Recti> GraphRenderEngine::completedTiles() const {
    std::vector<std::shared_ptr<render::RenderEngine>> active;
    {
      std::lock_guard<std::mutex> lock(p->activeEngineMutex);
      active = p->activeEngines;
    }
    std::list<Recti> result;
    for (const auto& engine : active) {
      if (!engine) {
        continue;
      }
      const auto tiles = engine->completedTiles();
      result.insert(result.end(), tiles.begin(), tiles.end());
    }
    return result;
  }
}
