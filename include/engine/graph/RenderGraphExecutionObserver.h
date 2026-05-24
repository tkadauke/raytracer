#pragma once

#include "engine/graph/RenderGraphTypes.h"

#include <string>

namespace engine::graph {
  /**
    * Receives live execution events from `GraphRenderEngine`.
    *
    * Observers may be called from render worker threads. Implementations should
    * return quickly, avoid throwing, and marshal to their UI thread when needed.
    */
  class RenderGraphExecutionObserver {
  public:
    virtual ~RenderGraphExecutionObserver() = default;

    virtual void passStarted(const RenderPassId& passId) = 0;
    virtual void passFinished(const RenderPassId& passId) = 0;
    virtual void passFailed(const RenderPassId& passId, const std::string& message) = 0;
  };
}
