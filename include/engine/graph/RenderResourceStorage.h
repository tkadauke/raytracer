#pragma once

#include "core/Color.h"
#include "engine/graph/RenderGraphTypes.h"

#include <cstdint>
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

    Buffer<Colord>& color(const RenderResourceId& id);
    const Buffer<Colord>& color(const RenderResourceId& id) const;

    Buffer<double>& depth(const RenderResourceId& id);
    const Buffer<double>& depth(const RenderResourceId& id) const;

    Buffer<std::uint8_t>& stencil(const RenderResourceId& id);
    const Buffer<std::uint8_t>& stencil(const RenderResourceId& id) const;

    Buffer<std::uint32_t>& objectId(const RenderResourceId& id);
    const Buffer<std::uint32_t>& objectId(const RenderResourceId& id) const;

  private:
    template<class T>
    using BufferMap = std::map<RenderResourceId, std::unique_ptr<Buffer<T>>>;

    template<class T>
    Buffer<T>& typedBuffer(BufferMap<T>& buffers, const RenderResourceId& id, const char* typeName);

    template<class T>
    const Buffer<T>& typedBuffer(const BufferMap<T>& buffers, const RenderResourceId& id,
                                 const char* typeName) const;

    std::map<RenderResourceId, RenderResourceDescriptor> m_descriptors;
    BufferMap<Colord> m_colorBuffers;
    BufferMap<double> m_depthBuffers;
    BufferMap<std::uint8_t> m_stencilBuffers;
    BufferMap<std::uint32_t> m_objectIdBuffers;
  };
}
