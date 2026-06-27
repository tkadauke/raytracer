#pragma once

#include "engine/graph/RenderGraphTypes.h"

#include <QJsonObject>

#include <functional>
#include <memory>
#include <string>

namespace render {
  class Camera;
  class RenderEngine;
}

namespace engine::graph {
  class GraphRenderEngine;
  class RenderResourceStorage;

  /**
    * Per-pass execution state passed to a `RenderPassPayload`.
    *
    * The compiled `RenderPassNode` remains serializable plan data. This context
    * supplies the runtime pieces a payload needs while executing that node:
    * resource storage, graph-wide render settings, cancellation state, and the
    * active child engine hook used by progress/cancellation forwarding.
    */
  class RenderExecutionContext {
  public:
    using ActiveEngineSetter = std::function<void(std::shared_ptr<render::RenderEngine>)>;
    using TraceMessageRecorder = std::function<void(std::string)>;

    RenderExecutionContext(const RenderPassNode& pass, RenderResourceStorage& storage,
                           const GraphRenderEngine& graph, bool cancelled,
                           ActiveEngineSetter activeEngineSetter,
                           TraceMessageRecorder traceMessageRecorder = {});

    /**
      * @returns the compiled pass node being executed.
      */
    const RenderPassNode& pass() const;

    /**
      * @returns mutable frame-local graph resources.
      */
    RenderResourceStorage& storage();
    const RenderResourceStorage& storage() const;

    /**
      * @returns the graph engine that owns this execution.
      */
    const GraphRenderEngine& graph() const;

    /**
      * @returns the runtime camera selected for this pass.
      */
    std::shared_ptr<render::Camera> camera() const;

    /**
      * @returns true if cancellation was already requested before the pass ran.
      */
    bool cancelled() const;

    /**
      * Publishes a child render engine while a payload is executing.
      *
      * `GraphRenderEngine` uses this to forward cancellation and tile-progress
      * queries into the wrapped raytracer, rasterizer, or wireframe engine.
      */
    void setActiveEngine(std::shared_ptr<render::RenderEngine> engine);

    /**
      * Clears the active child render engine.
      */
    void clearActiveEngine();

    /**
      * Records supplemental trace text for the currently executing pass.
      */
    void recordTraceMessage(std::string message) const;

    /**
      * Marks whether a display buffer written by the payload can be published
      * directly as the graph's current visible output.
      */
    void setDisplayTargetDirectlyPublishable(bool directlyPublishable);
    bool displayTargetDirectlyPublishable() const;

    void setTraceMetadata(QJsonObject metadata);
    const QJsonObject& traceMetadata() const;

  private:
    const RenderPassNode& m_pass;
    RenderResourceStorage& m_storage;
    const GraphRenderEngine& m_graph;
    bool m_cancelled;
    ActiveEngineSetter m_activeEngineSetter;
    TraceMessageRecorder m_traceMessageRecorder;
    bool m_displayTargetDirectlyPublishable{false};
    QJsonObject m_traceMetadata;
  };
}
