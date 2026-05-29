#pragma once

#include "engine/graph/RenderGraphTypes.h"

#include <string>
#include <vector>

namespace engine::graph {
  struct RenderTargetSpec;

  /**
    * Graph-visible auxiliary output definition.
    *
    * Each AOV owns the mapping from user-facing view mode to graph feature
    * names, resource ids, labels, and resource descriptors. Callers should ask
    * the definition object instead of switching on `RenderViewMode`.
    */
  class RenderAOVDefinition {
  public:
    virtual ~RenderAOVDefinition() = default;

    virtual RenderViewMode viewMode() const = 0;
    virtual std::string feature() const = 0;
    virtual std::string title() const = 0;
    virtual RenderResourceType resourceType() const = 0;
    virtual RenderResourceFormat resourceFormat() const = 0;
    virtual bool usesRasterTargetSampling() const = 0;
    virtual bool supportsExecutor(RenderExecutorKind executor) const = 0;
    virtual void configureProducerPass(RenderPassNode& pass) const;

    RenderResourceId resourceId() const;
    RenderResourceId previewColorResourceId() const;
    RenderResourceDescriptor resourceDescriptor(const RenderTargetSpec& target,
                                                RenderResourceLifetime lifetime) const;
    bool matchesName(const std::string& normalizedName) const;
  };

  const RenderAOVDefinition* renderAOVDefinition(RenderViewMode viewMode);
  const RenderAOVDefinition* renderAOVDefinitionForName(const std::string& normalizedName);
  std::vector<const RenderAOVDefinition*> renderAOVDefinitions();
}
