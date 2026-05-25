#pragma once

#include "engine/graph/RenderGraphTypes.h"

#include <string>
#include <vector>

namespace engine::graph {
  /**
    * Graph-level executor metadata owned by each executor family.
    *
    * The render graph compiler asks these definitions for executor-specific
    * pass labels and feature names instead of switching on executor enums.
    */
  class RenderExecutorDefinition {
  public:
    virtual ~RenderExecutorDefinition() = default;

    virtual RenderExecutorKind kind() const = 0;
    virtual RenderExecutorPreference preference() const = 0;
    virtual RenderFeatureKind feature() const = 0;
    virtual std::string beautyPassId() const = 0;
    virtual std::string beautyPassName() const = 0;

    bool matches(RenderExecutorKind executor) const;
    bool matches(RenderExecutorPreference executor) const;
  };

  const RenderExecutorDefinition& renderExecutorDefinition(RenderExecutorPreference executor);
  const RenderExecutorDefinition* renderExecutorDefinition(RenderExecutorKind executor);
  std::vector<const RenderExecutorDefinition*> renderExecutorDefinitions();
}
