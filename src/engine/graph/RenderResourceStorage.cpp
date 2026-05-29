#include "engine/graph/RenderResourceStorage.h"

#include "core/util/BufferUtils.h"

#include <utility>

namespace engine::graph {
  void RenderResourceStorage::allocate(const std::vector<RenderResourceDescriptor>& descriptors) {
    clear();

    for (const auto& descriptor : descriptors) {
      m_resources[descriptor.id] = RenderResource::create(descriptor);
    }
  }

  void RenderResourceStorage::clear() {
    m_resources.clear();
  }

  bool RenderResourceStorage::contains(const RenderResourceId& id) const {
    return m_resources.find(id) != m_resources.end();
  }

  bool RenderResourceStorage::hasBuffer(const RenderResourceId& id) const {
    const auto it = m_resources.find(id);
    return it != m_resources.end() && it->second->hasBuffer();
  }

  const RenderResourceDescriptor&
  RenderResourceStorage::descriptor(const RenderResourceId& id) const {
    return resource(id).descriptor();
  }

  RenderResource& RenderResourceStorage::resource(const RenderResourceId& id) {
    const auto it = m_resources.find(id);
    if (it == m_resources.end())
      throw std::out_of_range("unknown render resource '" + id + "'");
    return *it->second;
  }

  const RenderResource& RenderResourceStorage::resource(const RenderResourceId& id) const {
    const auto it = m_resources.find(id);
    if (it == m_resources.end())
      throw std::out_of_range("unknown render resource '" + id + "'");
    return *it->second;
  }

  void RenderResourceStorage::setGpuResidency(const RenderResourceId& id,
                                              RenderGpuResourceResidency residency) {
    resource(id).setGpuResidency(std::move(residency));
  }

  void RenderResourceStorage::clearGpuResidency(const RenderResourceId& id) {
    resource(id).clearGpuResidency();
  }

  Buffer<Colord>& RenderResourceStorage::color(const RenderResourceId& id) {
    return resource(id).color();
  }

  const Buffer<Colord>& RenderResourceStorage::color(const RenderResourceId& id) const {
    return resource(id).color();
  }

  void RenderResourceStorage::bindColor(const RenderResourceId& id, const Buffer<Colord>& source) {
    RenderResource& destinationResource = resource(id);
    if (!destinationResource.colorBacked()) {
      throw std::out_of_range("render resource '" + id + "' is not color-backed");
    }

    Buffer<Colord>& destination = destinationResource.color();
    if (!core::util::bufferDimensionsEqual(source, destination)) {
      throw std::runtime_error("external color resource '" + id + "' has mismatched dimensions");
    }

    core::util::copyBuffer(destination, source);
    destinationResource.markProduced();
  }

  Buffer<double>& RenderResourceStorage::depth(const RenderResourceId& id) {
    return resource(id).depth();
  }

  const Buffer<double>& RenderResourceStorage::depth(const RenderResourceId& id) const {
    return resource(id).depth();
  }

  void RenderResourceStorage::bindDepth(const RenderResourceId& id, const Buffer<double>& source) {
    RenderResource& destinationResource = resource(id);
    if (!destinationResource.depthBacked()) {
      throw std::out_of_range("render resource '" + id + "' is not depth-backed");
    }

    Buffer<double>& destination = destinationResource.depth();
    if (!core::util::bufferDimensionsEqual(source, destination)) {
      throw std::runtime_error("external depth resource '" + id + "' has mismatched dimensions");
    }

    core::util::copyBuffer(destination, source);
    destinationResource.markProduced();
  }

  Buffer<std::uint8_t>& RenderResourceStorage::stencil(const RenderResourceId& id) {
    return resource(id).stencil();
  }

  const Buffer<std::uint8_t>& RenderResourceStorage::stencil(const RenderResourceId& id) const {
    return resource(id).stencil();
  }

  void RenderResourceStorage::bindStencil(const RenderResourceId& id,
                                          const Buffer<std::uint8_t>& source) {
    RenderResource& destinationResource = resource(id);
    if (!destinationResource.stencilBacked()) {
      throw std::out_of_range("render resource '" + id + "' is not stencil-backed");
    }

    Buffer<std::uint8_t>& destination = destinationResource.stencil();
    if (!core::util::bufferDimensionsEqual(source, destination)) {
      throw std::runtime_error("external stencil resource '" + id + "' has mismatched dimensions");
    }

    core::util::copyBuffer(destination, source);
    destinationResource.markProduced();
  }

  Buffer<std::uint32_t>& RenderResourceStorage::objectId(const RenderResourceId& id) {
    return resource(id).objectId();
  }

  const Buffer<std::uint32_t>& RenderResourceStorage::objectId(const RenderResourceId& id) const {
    return resource(id).objectId();
  }

  void RenderResourceStorage::bindObjectId(const RenderResourceId& id,
                                           const Buffer<std::uint32_t>& source) {
    RenderResource& destinationResource = resource(id);
    if (!destinationResource.objectIdBacked()) {
      throw std::out_of_range("render resource '" + id + "' is not object-id-backed");
    }

    Buffer<std::uint32_t>& destination = destinationResource.objectId();
    if (!core::util::bufferDimensionsEqual(source, destination)) {
      throw std::runtime_error("external object-id resource '" + id +
                               "' has mismatched dimensions");
    }

    core::util::copyBuffer(destination, source);
    destinationResource.markProduced();
  }

  void RenderResourceStorage::copy(const RenderResourceId& sourceId,
                                   const RenderResourceId& destinationId,
                                   const std::string& action) {
    resource(sourceId).copyContentsTo(resource(destinationId), action);
  }
}
