#include "engine/graph/RenderResourceStorage.h"

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

  const RenderResourceDescriptor& RenderResourceStorage::descriptor(const RenderResourceId& id) const {
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

  Buffer<Colord>& RenderResourceStorage::color(const RenderResourceId& id) {
    return resource(id).color();
  }

  const Buffer<Colord>& RenderResourceStorage::color(const RenderResourceId& id) const {
    return resource(id).color();
  }

  Buffer<double>& RenderResourceStorage::depth(const RenderResourceId& id) {
    return resource(id).depth();
  }

  const Buffer<double>& RenderResourceStorage::depth(const RenderResourceId& id) const {
    return resource(id).depth();
  }

  Buffer<std::uint8_t>& RenderResourceStorage::stencil(const RenderResourceId& id) {
    return resource(id).stencil();
  }

  const Buffer<std::uint8_t>& RenderResourceStorage::stencil(const RenderResourceId& id) const {
    return resource(id).stencil();
  }

  Buffer<std::uint32_t>& RenderResourceStorage::objectId(const RenderResourceId& id) {
    return resource(id).objectId();
  }

  const Buffer<std::uint32_t>& RenderResourceStorage::objectId(const RenderResourceId& id) const {
    return resource(id).objectId();
  }
}
