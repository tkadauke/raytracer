#include "engine/graph/RenderResource.h"

#include <limits>
#include <utility>

namespace engine::graph {
  namespace {
    Colord colorDefault(const RenderResourceDescriptor& descriptor,
                        RenderPassKind passKind,
                        const Colord& beautyDefaultColor) {
      if (descriptor.type == RenderResourceType::ShadowMask) {
        return Colord::white();
      }
      if (descriptor.type == RenderResourceType::Color && passKind == RenderPassKind::Beauty) {
        return beautyDefaultColor;
      }
      return Colord::black();
    }
  }

  RenderResource::RenderResource(RenderResourceDescriptor descriptor)
      : m_descriptor(std::move(descriptor)) {
  }

  RenderResource::~RenderResource() = default;

  std::unique_ptr<RenderResource> RenderResource::create(RenderResourceDescriptor descriptor) {
    if (descriptor.domain != RenderResourceDomain::CPU || !descriptor.hasImageShape()) {
      return std::make_unique<DescriptorOnlyRenderResource>(std::move(descriptor));
    }

    switch (descriptor.type) {
    case RenderResourceType::Color:
    case RenderResourceType::Normal:
    case RenderResourceType::WorldPosition:
    case RenderResourceType::MotionVector:
    case RenderResourceType::ShadowMask:
    case RenderResourceType::CustomTexture:
      return std::make_unique<ColorRenderResource>(std::move(descriptor));
    case RenderResourceType::Depth:
    case RenderResourceType::ShadowMap:
      return std::make_unique<DepthRenderResource>(std::move(descriptor));
    case RenderResourceType::Stencil:
      return std::make_unique<StencilRenderResource>(std::move(descriptor));
    case RenderResourceType::ObjectId:
    case RenderResourceType::MaterialId:
      return std::make_unique<ObjectIdRenderResource>(std::move(descriptor));
    }

    return std::make_unique<DescriptorOnlyRenderResource>(std::move(descriptor));
  }

  const RenderResourceDescriptor& RenderResource::descriptor() const {
    return m_descriptor;
  }

  bool RenderResource::hasBuffer() const {
    return false;
  }

  bool RenderResource::colorBacked() const {
    return false;
  }

  void RenderResource::clearSubstituteDefault(RenderPassKind, const Colord&) {
  }

  Buffer<Colord>& RenderResource::color() {
    throw missingBuffer("color");
  }

  const Buffer<Colord>& RenderResource::color() const {
    throw missingBuffer("color");
  }

  Buffer<double>& RenderResource::depth() {
    throw missingBuffer("depth");
  }

  const Buffer<double>& RenderResource::depth() const {
    throw missingBuffer("depth");
  }

  Buffer<std::uint8_t>& RenderResource::stencil() {
    throw missingBuffer("stencil");
  }

  const Buffer<std::uint8_t>& RenderResource::stencil() const {
    throw missingBuffer("stencil");
  }

  Buffer<std::uint32_t>& RenderResource::objectId() {
    throw missingBuffer("object id");
  }

  const Buffer<std::uint32_t>& RenderResource::objectId() const {
    throw missingBuffer("object id");
  }

  std::out_of_range RenderResource::missingBuffer(const char* typeName) const {
    return std::out_of_range("render resource '" + descriptor().id +
                             "' has no CPU " + typeName + " buffer");
  }

  ColorRenderResource::ColorRenderResource(RenderResourceDescriptor descriptor)
      : RenderResource(descriptor),
        m_buffer(this->descriptor().width, this->descriptor().height) {
  }

  bool ColorRenderResource::hasBuffer() const {
    return true;
  }

  bool ColorRenderResource::colorBacked() const {
    return true;
  }

  void ColorRenderResource::clearSubstituteDefault(RenderPassKind passKind,
                                                   const Colord& beautyDefaultColor) {
    m_buffer.clear(colorDefault(descriptor(), passKind, beautyDefaultColor));
  }

  Buffer<Colord>& ColorRenderResource::color() {
    return m_buffer;
  }

  const Buffer<Colord>& ColorRenderResource::color() const {
    return m_buffer;
  }

  DepthRenderResource::DepthRenderResource(RenderResourceDescriptor descriptor)
      : RenderResource(descriptor),
        m_buffer(this->descriptor().width, this->descriptor().height) {
  }

  bool DepthRenderResource::hasBuffer() const {
    return true;
  }

  void DepthRenderResource::clearSubstituteDefault(RenderPassKind, const Colord&) {
    m_buffer.clear(std::numeric_limits<double>::infinity());
  }

  Buffer<double>& DepthRenderResource::depth() {
    return m_buffer;
  }

  const Buffer<double>& DepthRenderResource::depth() const {
    return m_buffer;
  }

  StencilRenderResource::StencilRenderResource(RenderResourceDescriptor descriptor)
      : RenderResource(descriptor),
        m_buffer(this->descriptor().width, this->descriptor().height) {
  }

  bool StencilRenderResource::hasBuffer() const {
    return true;
  }

  void StencilRenderResource::clearSubstituteDefault(RenderPassKind, const Colord&) {
    m_buffer.clear(0);
  }

  Buffer<std::uint8_t>& StencilRenderResource::stencil() {
    return m_buffer;
  }

  const Buffer<std::uint8_t>& StencilRenderResource::stencil() const {
    return m_buffer;
  }

  ObjectIdRenderResource::ObjectIdRenderResource(RenderResourceDescriptor descriptor)
      : RenderResource(descriptor),
        m_buffer(this->descriptor().width, this->descriptor().height) {
  }

  bool ObjectIdRenderResource::hasBuffer() const {
    return true;
  }

  void ObjectIdRenderResource::clearSubstituteDefault(RenderPassKind, const Colord&) {
    m_buffer.clear(0);
  }

  Buffer<std::uint32_t>& ObjectIdRenderResource::objectId() {
    return m_buffer;
  }

  const Buffer<std::uint32_t>& ObjectIdRenderResource::objectId() const {
    return m_buffer;
  }
}
