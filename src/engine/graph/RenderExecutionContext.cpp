#include "engine/graph/RenderExecutionContext.h"

#include "engine/graph/GraphRenderEngine.h"
#include "engine/graph/RenderResourceStorage.h"

#include <utility>

namespace engine::graph {
  RenderExecutionContext::RenderExecutionContext(const RenderPassNode& pass,
                                                 RenderResourceStorage& storage,
                                                 const GraphRenderEngine& graph, bool cancelled,
                                                 ActiveEngineSetter activeEngineSetter,
                                                 TraceMessageRecorder traceMessageRecorder)
      : m_pass(pass),
        m_storage(storage),
        m_graph(graph),
        m_cancelled(cancelled),
        m_activeEngineSetter(std::move(activeEngineSetter)),
        m_traceMessageRecorder(std::move(traceMessageRecorder)) {
  }

  const RenderPassNode& RenderExecutionContext::pass() const {
    return m_pass;
  }

  RenderResourceStorage& RenderExecutionContext::storage() {
    return m_storage;
  }

  const RenderResourceStorage& RenderExecutionContext::storage() const {
    return m_storage;
  }

  const GraphRenderEngine& RenderExecutionContext::graph() const {
    return m_graph;
  }

  std::shared_ptr<render::Camera> RenderExecutionContext::camera() const {
    return m_graph.cameraForPass(m_pass);
  }

  bool RenderExecutionContext::cancelled() const {
    return m_cancelled;
  }

  void RenderExecutionContext::setActiveEngine(std::shared_ptr<render::RenderEngine> engine) {
    if (m_activeEngineSetter) {
      m_activeEngineSetter(std::move(engine));
    }
  }

  void RenderExecutionContext::clearActiveEngine() {
    setActiveEngine(nullptr);
  }

  void RenderExecutionContext::recordTraceMessage(std::string message) const {
    if (m_traceMessageRecorder) {
      m_traceMessageRecorder(std::move(message));
    }
  }

  void RenderExecutionContext::setTraceMetadata(QJsonObject metadata) {
    m_traceMetadata = std::move(metadata);
  }

  const QJsonObject& RenderExecutionContext::traceMetadata() const {
    return m_traceMetadata;
  }
}
