#pragma once

#include "engine/graph/RenderGraphTypes.h"

#include <cstdint>
#include <set>
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

    virtual void renderStarted(std::uint64_t) {
    }

    virtual void activePassesChanged(const std::set<RenderPassId>&) {
    }

    virtual void activePassesChanged(const std::set<RenderPassId>& passIds, std::uint64_t) {
      activePassesChanged(passIds);
    }

    virtual void passStarted(const RenderPassId& passId) = 0;
    virtual void passFinished(const RenderPassId& passId) = 0;
    virtual void passFailed(const RenderPassId& passId, const std::string& message) = 0;

    virtual void passStarted(const RenderPassId& passId, std::uint64_t) {
      passStarted(passId);
    }

    virtual void passFinished(const RenderPassId& passId, std::uint64_t) {
      passFinished(passId);
    }

    virtual void passFailed(const RenderPassId& passId, const std::string& message, std::uint64_t) {
      passFailed(passId, message);
    }
  };
}
