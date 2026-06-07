#include "engine/graph/RenderResourceStorage.h"

#include "core/util/BufferUtils.h"
#include "engine/raster/detail/OpenGLRasterResource.h"

#include <stdexcept>
#include <utility>

namespace engine::graph {
  namespace {
    void requireOpenGLResourceShape(
      const RenderResourceDescriptor& descriptor,
      const ::engine::raster::detail::OpenGLRasterResource& openGLResource) {
      if (descriptor.width != openGLResource.width() ||
          descriptor.height != openGLResource.height()) {
        throw std::runtime_error("OpenGL resource '" + descriptor.id +
                                 "' has mismatched dimensions");
      }
      if (descriptor.sampleCount != openGLResource.sampleCount()) {
        throw std::runtime_error("OpenGL resource '" + descriptor.id +
                                 "' has mismatched sample count");
      }
    }
  }

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

  bool RenderResourceStorage::hasOpenGLResource(const RenderResourceId& id) const {
    const auto it = m_resources.find(id);
    return it != m_resources.end() && it->second->openGLResident();
  }

  void RenderResourceStorage::bindOpenGLResource(
    const RenderResourceId& id,
    std::shared_ptr<::engine::raster::detail::OpenGLRasterResource> openGLResource) {
    if (!openGLResource) {
      throw std::invalid_argument("OpenGL resource '" + id + "' is null");
    }

    RenderResource& destinationResource = resource(id);
    const RenderResourceDescriptor& descriptor = destinationResource.descriptor();
    if (descriptor.domain != RenderResourceDomain::GPU) {
      throw std::out_of_range("render resource '" + id + "' is not GPU-backed");
    }
    if (descriptor.type != openGLResource->resourceType()) {
      throw std::out_of_range(std::string("OpenGL resource '") + id + "' is " +
                              toString(openGLResource->resourceType()) + ", expected " +
                              toString(descriptor.type));
    }
    requireOpenGLResourceShape(descriptor, *openGLResource);

    destinationResource.setOpenGLResource(std::move(openGLResource));
    destinationResource.markProduced();
  }

  void RenderResourceStorage::clearOpenGLResource(const RenderResourceId& id) {
    resource(id).clearOpenGLResource();
  }

  std::shared_ptr<::engine::raster::detail::OpenGLRasterResource>
  RenderResourceStorage::openGLResource(const RenderResourceId& id) const {
    auto openGLResource = resource(id).openGLResource();
    if (!openGLResource) {
      throw std::out_of_range("render resource '" + id + "' has no OpenGL resident resource");
    }
    return openGLResource;
  }

  std::shared_ptr<::engine::raster::detail::OpenGLRasterResource>
  RenderResourceStorage::openGLResource(const RenderResourceId& id,
                                        RenderResourceType expectedType) const {
    auto residentResource = openGLResource(id);
    if (residentResource->resourceType() != expectedType) {
      throw std::out_of_range(std::string("OpenGL resource '") + id + "' is " +
                              toString(residentResource->resourceType()) + ", expected " +
                              toString(expectedType));
    }
    return residentResource;
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

  void RenderResourceStorage::setVisibilitySet(
    const RenderResourceId& id, std::shared_ptr<const ::engine::raster::RasterVisibilitySet> set) {
    RenderResource& destinationResource = resource(id);
    if (!destinationResource.visibilitySetBacked()) {
      throw std::out_of_range("render resource '" + id + "' is not visibility-set-backed");
    }
    destinationResource.setVisibilitySet(std::move(set));
  }

  std::shared_ptr<const ::engine::raster::RasterVisibilitySet>
  RenderResourceStorage::visibilitySet(const RenderResourceId& id) const {
    const RenderResource& sourceResource = resource(id);
    if (!sourceResource.visibilitySetBacked()) {
      throw std::out_of_range("render resource '" + id + "' is not visibility-set-backed");
    }
    return sourceResource.visibilitySet();
  }

  void RenderResourceStorage::copy(const RenderResourceId& sourceId,
                                   const RenderResourceId& destinationId,
                                   const std::string& action) {
    resource(sourceId).copyContentsTo(resource(destinationId), action);
  }
}
