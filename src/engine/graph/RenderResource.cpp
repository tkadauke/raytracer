#include "engine/graph/RenderResource.h"

#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

namespace engine::graph {
  namespace {
    Colord colorDefault(const RenderResourceDescriptor& descriptor, RenderPassKind passKind,
                        const Colord& beautyDefaultColor) {
      if (descriptor.type == RenderResourceType::ShadowMask) {
        return Colord::white();
      }
      if (descriptor.type == RenderResourceType::Color && passKind == RenderPassKind::Beauty) {
        return beautyDefaultColor;
      }
      return Colord::black();
    }

    class RenderResourceFactory {
    public:
      virtual ~RenderResourceFactory() = default;

      virtual bool matches(const RenderResourceDescriptor& descriptor) const = 0;
      virtual std::unique_ptr<RenderResource> create(RenderResourceDescriptor descriptor) const = 0;
    };

    template<class Resource>
    class ResourceTypeFactory : public RenderResourceFactory {
    public:
      explicit ResourceTypeFactory(std::vector<RenderResourceType> types)
          : m_types(std::move(types)) {
      }

      bool matches(const RenderResourceDescriptor& descriptor) const override {
        return std::find(m_types.begin(), m_types.end(), descriptor.type) != m_types.end();
      }

      std::unique_ptr<RenderResource> create(RenderResourceDescriptor descriptor) const override {
        return std::make_unique<Resource>(std::move(descriptor));
      }

    private:
      std::vector<RenderResourceType> m_types;
    };

    const std::vector<const RenderResourceFactory*>& resourceFactories() {
      static const ResourceTypeFactory<ColorRenderResource> color(
        {RenderResourceType::Color, RenderResourceType::Normal, RenderResourceType::WorldPosition,
         RenderResourceType::MotionVector, RenderResourceType::ShadowMask,
         RenderResourceType::CustomTexture});
      static const ResourceTypeFactory<DepthRenderResource> depth(
        {RenderResourceType::Depth, RenderResourceType::ShadowMap});
      static const ResourceTypeFactory<StencilRenderResource> stencil(
        {RenderResourceType::Stencil});
      static const ResourceTypeFactory<ObjectIdRenderResource> objectId(
        {RenderResourceType::ObjectId, RenderResourceType::MaterialId});
      static const std::vector<const RenderResourceFactory*> result = {&color, &depth, &stencil,
                                                                       &objectId};
      return result;
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

    for (const auto* factory : resourceFactories()) {
      if (factory->matches(descriptor)) {
        return factory->create(std::move(descriptor));
      }
    }

    return std::make_unique<DescriptorOnlyRenderResource>(std::move(descriptor));
  }

  const RenderResourceDescriptor& RenderResource::descriptor() const {
    return m_descriptor;
  }

  bool RenderResource::substituteDefault() const {
    return m_substituteDefault;
  }

  void RenderResource::markProduced() {
    m_substituteDefault = false;
  }

  void RenderResource::setState(std::shared_ptr<const RenderPassState> state) {
    m_state = std::move(state);
  }

  std::shared_ptr<const RenderPassState> RenderResource::state() const {
    return m_state;
  }

  void RenderResource::setCacheMetadata(RenderGraphCacheMetadata metadata) {
    m_cacheMetadata = std::move(metadata);
  }

  const std::optional<RenderGraphCacheMetadata>& RenderResource::cacheMetadata() const {
    return m_cacheMetadata;
  }

  void RenderResource::setArtifact(std::shared_ptr<const RenderGraphCachedArtifact> artifact) {
    m_artifact = std::move(artifact);
  }

  std::shared_ptr<const RenderGraphCachedArtifact> RenderResource::artifact() const {
    return m_artifact;
  }

  bool RenderResource::hasBuffer() const {
    return false;
  }

  bool RenderResource::colorBacked() const {
    return false;
  }

  bool RenderResource::depthBacked() const {
    return false;
  }

  bool RenderResource::stencilBacked() const {
    return false;
  }

  bool RenderResource::objectIdBacked() const {
    return false;
  }

  void RenderResource::clearSubstituteDefault(RenderPassKind, const Colord&) {
    m_substituteDefault = true;
    m_state.reset();
    m_artifact.reset();
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
    return std::out_of_range("render resource '" + descriptor().id + "' has no CPU " + typeName +
                             " buffer");
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
    RenderResource::clearSubstituteDefault(passKind, beautyDefaultColor);
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

  bool DepthRenderResource::depthBacked() const {
    return true;
  }

  void DepthRenderResource::clearSubstituteDefault(RenderPassKind passKind,
                                                   const Colord& beautyDefaultColor) {
    RenderResource::clearSubstituteDefault(passKind, beautyDefaultColor);
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

  bool StencilRenderResource::stencilBacked() const {
    return true;
  }

  void StencilRenderResource::clearSubstituteDefault(RenderPassKind passKind,
                                                     const Colord& beautyDefaultColor) {
    RenderResource::clearSubstituteDefault(passKind, beautyDefaultColor);
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

  bool ObjectIdRenderResource::objectIdBacked() const {
    return true;
  }

  void ObjectIdRenderResource::clearSubstituteDefault(RenderPassKind passKind,
                                                      const Colord& beautyDefaultColor) {
    RenderResource::clearSubstituteDefault(passKind, beautyDefaultColor);
    m_buffer.clear(0);
  }

  Buffer<std::uint32_t>& ObjectIdRenderResource::objectId() {
    return m_buffer;
  }

  const Buffer<std::uint32_t>& ObjectIdRenderResource::objectId() const {
    return m_buffer;
  }
}
