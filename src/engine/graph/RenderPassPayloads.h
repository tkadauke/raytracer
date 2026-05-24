#pragma once

#include "engine/graph/RenderPassPayload.h"

#include <memory>

namespace engine::graph {
  struct RenderPassNode;

  /**
    * Creates the built-in payload for a supported compiled pass.
    *
    * The serialized plan stores `RenderPassNode` data only. At execution time,
    * the graph engine maps supported node kind/executor combinations to these
    * small payload objects.
    */
  std::unique_ptr<RenderPassPayload> makeBuiltinPassPayload(const RenderPassNode& pass);
}
