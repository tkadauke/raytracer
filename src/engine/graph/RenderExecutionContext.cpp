#include "engine/graph/RenderExecutionContext.h"

#include "engine/graph/GraphRenderEngine.h"
#include "engine/graph/RenderResourceStorage.h"

#include <utility>

namespace engine::graph {
  RenderExecutionContext::RenderExecutionContext(const RenderPassNode& pass,
                                                 RenderResourceStorage& storage,
                                                 const GraphRenderEngine& graph, bool cancelled,
                                                 ActiveEngineSetter activeEngineSetter)
      : m_pass(pass),
        m_storage(storage),
        m_graph(graph),
        m_cancelled(cancelled),
        m_activeEngineSetter(std::move(activeEngineSetter)) {
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
}
