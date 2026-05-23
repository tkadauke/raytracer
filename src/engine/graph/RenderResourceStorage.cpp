#include "engine/graph/RenderResourceStorage.h"

namespace engine::graph {
  namespace {
    bool hasImageShape(const RenderResourceDescriptor& descriptor) {
      return descriptor.width > 0 && descriptor.height > 0;
    }

    std::unique_ptr<RenderResource> makeResource(const RenderResourceDescriptor& descriptor) {
      if (descriptor.domain != RenderResourceDomain::CPU || !hasImageShape(descriptor)) {
        return std::make_unique<DescriptorOnlyRenderResource>(descriptor);
      }

      switch (descriptor.type) {
      case RenderResourceType::Color:
      case RenderResourceType::Normal:
      case RenderResourceType::WorldPosition:
      case RenderResourceType::MotionVector:
      case RenderResourceType::ShadowMask:
      case RenderResourceType::CustomTexture:
        return std::make_unique<ColorRenderResource>(descriptor);
      case RenderResourceType::Depth:
      case RenderResourceType::ShadowMap:
        return std::make_unique<DepthRenderResource>(descriptor);
      case RenderResourceType::Stencil:
        return std::make_unique<StencilRenderResource>(descriptor);
      case RenderResourceType::ObjectId:
      case RenderResourceType::MaterialId:
        return std::make_unique<ObjectIdRenderResource>(descriptor);
      }

      return std::make_unique<DescriptorOnlyRenderResource>(descriptor);
    }
  }

  void RenderResourceStorage::allocate(const std::vector<RenderResourceDescriptor>& descriptors) {
    clear();

    for (const auto& descriptor : descriptors) {
      m_resources[descriptor.id] = makeResource(descriptor);
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
