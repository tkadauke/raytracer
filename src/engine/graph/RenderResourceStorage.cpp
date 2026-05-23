#include "engine/graph/RenderResourceStorage.h"

#include "core/Buffer.h"

namespace engine::graph {
  namespace {
    bool hasImageShape(const RenderResourceDescriptor& descriptor) {
      return descriptor.width > 0 && descriptor.height > 0;
    }
  }

  void RenderResourceStorage::allocate(const std::vector<RenderResourceDescriptor>& descriptors) {
    clear();

    for (const auto& descriptor : descriptors) {
      m_descriptors[descriptor.id] = descriptor;

      if (descriptor.domain != RenderResourceDomain::CPU || !hasImageShape(descriptor))
        continue;

      switch (descriptor.type) {
      case RenderResourceType::Color:
      case RenderResourceType::Normal:
      case RenderResourceType::WorldPosition:
      case RenderResourceType::MotionVector:
      case RenderResourceType::ShadowMask:
      case RenderResourceType::CustomTexture:
        m_colorBuffers[descriptor.id] =
          std::make_unique<Buffer<Colord>>(descriptor.width, descriptor.height);
        break;
      case RenderResourceType::Depth:
      case RenderResourceType::ShadowMap:
        m_depthBuffers[descriptor.id] =
          std::make_unique<Buffer<double>>(descriptor.width, descriptor.height);
        break;
      case RenderResourceType::Stencil:
        m_stencilBuffers[descriptor.id] =
          std::make_unique<Buffer<std::uint8_t>>(descriptor.width, descriptor.height);
        break;
      case RenderResourceType::ObjectId:
      case RenderResourceType::MaterialId:
        m_objectIdBuffers[descriptor.id] =
          std::make_unique<Buffer<std::uint32_t>>(descriptor.width, descriptor.height);
        break;
      }
    }
  }

  void RenderResourceStorage::clear() {
    m_descriptors.clear();
    m_colorBuffers.clear();
    m_depthBuffers.clear();
    m_stencilBuffers.clear();
    m_objectIdBuffers.clear();
  }

  bool RenderResourceStorage::contains(const RenderResourceId& id) const {
    return m_descriptors.find(id) != m_descriptors.end();
  }

  bool RenderResourceStorage::hasBuffer(const RenderResourceId& id) const {
    return m_colorBuffers.find(id) != m_colorBuffers.end() ||
           m_depthBuffers.find(id) != m_depthBuffers.end() ||
           m_stencilBuffers.find(id) != m_stencilBuffers.end() ||
           m_objectIdBuffers.find(id) != m_objectIdBuffers.end();
  }

  const RenderResourceDescriptor& RenderResourceStorage::descriptor(const RenderResourceId& id) const {
    const auto it = m_descriptors.find(id);
    if (it == m_descriptors.end())
      throw std::out_of_range("unknown render resource '" + id + "'");
    return it->second;
  }

  Buffer<Colord>& RenderResourceStorage::color(const RenderResourceId& id) {
    return typedBuffer(m_colorBuffers, id, "color");
  }

  const Buffer<Colord>& RenderResourceStorage::color(const RenderResourceId& id) const {
    return typedBuffer(m_colorBuffers, id, "color");
  }

  Buffer<double>& RenderResourceStorage::depth(const RenderResourceId& id) {
    return typedBuffer(m_depthBuffers, id, "depth");
  }

  const Buffer<double>& RenderResourceStorage::depth(const RenderResourceId& id) const {
    return typedBuffer(m_depthBuffers, id, "depth");
  }

  Buffer<std::uint8_t>& RenderResourceStorage::stencil(const RenderResourceId& id) {
    return typedBuffer(m_stencilBuffers, id, "stencil");
  }

  const Buffer<std::uint8_t>& RenderResourceStorage::stencil(const RenderResourceId& id) const {
    return typedBuffer(m_stencilBuffers, id, "stencil");
  }

  Buffer<std::uint32_t>& RenderResourceStorage::objectId(const RenderResourceId& id) {
    return typedBuffer(m_objectIdBuffers, id, "object id");
  }

  const Buffer<std::uint32_t>& RenderResourceStorage::objectId(const RenderResourceId& id) const {
    return typedBuffer(m_objectIdBuffers, id, "object id");
  }

  template<class T>
  Buffer<T>& RenderResourceStorage::typedBuffer(BufferMap<T>& buffers, const RenderResourceId& id,
                                                const char* typeName) {
    const auto it = buffers.find(id);
    if (it == buffers.end())
      throw std::out_of_range("render resource '" + id + "' has no CPU " + typeName + " buffer");
    return *it->second;
  }

  template<class T>
  const Buffer<T>& RenderResourceStorage::typedBuffer(const BufferMap<T>& buffers,
                                                      const RenderResourceId& id,
                                                      const char* typeName) const {
    const auto it = buffers.find(id);
    if (it == buffers.end())
      throw std::out_of_range("render resource '" + id + "' has no CPU " + typeName + " buffer");
    return *it->second;
  }
}
