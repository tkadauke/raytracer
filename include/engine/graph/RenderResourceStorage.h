#pragma once

#include "engine/graph/RenderResource.h"
#include "engine/graph/RenderGraphTypes.h"

#include <map>
#include <memory>
#include <stdexcept>
#include <vector>

template<class T>
class Buffer;

namespace engine::graph {
  /**
    * CPU-backed storage for graph resources.
    *
    * The storage layer is intentionally separate from `RenderPlan`: the plan
    * describes resources, while storage owns the buffers used by one execution.
    * The first implementation supports the core image-like resources needed by
    * color/depth/stencil/object-id passes.
    */
  class RenderResourceStorage {
  public:
    /**
      * Allocates CPU buffers for supported descriptors and records every
      * descriptor for later lookup.
      */
    void allocate(const std::vector<RenderResourceDescriptor>& descriptors);

    /**
      * Clears all descriptors and owned buffers.
      */
    void clear();

    /**
      * @returns true if a descriptor with @p id has been allocated or imported.
      */
    bool contains(const RenderResourceId& id) const;

    /**
      * @returns true if @p id has an owned CPU buffer in this storage object.
      */
    bool hasBuffer(const RenderResourceId& id) const;

    /**
      * Looks up the descriptor for @p id.
      *
      * @throws std::out_of_range if no descriptor with that id exists.
      */
    const RenderResourceDescriptor& descriptor(const RenderResourceId& id) const;

    /**
      * Looks up the execution-time resource for @p id.
      *
      * @throws std::out_of_range if no resource with that id exists.
      */
    RenderResource& resource(const RenderResourceId& id);
    const RenderResource& resource(const RenderResourceId& id) const;

    Buffer<Colord>& color(const RenderResourceId& id);
    const Buffer<Colord>& color(const RenderResourceId& id) const;

    Buffer<double>& depth(const RenderResourceId& id);
    const Buffer<double>& depth(const RenderResourceId& id) const;

    Buffer<std::uint8_t>& stencil(const RenderResourceId& id);
    const Buffer<std::uint8_t>& stencil(const RenderResourceId& id) const;

    Buffer<std::uint32_t>& objectId(const RenderResourceId& id);
    const Buffer<std::uint32_t>& objectId(const RenderResourceId& id) const;

  private:
    std::map<RenderResourceId, std::unique_ptr<RenderResource>> m_resources;
  };
}
