#pragma once

#include "core/Buffer.h"
#include "core/Color.h"
#include "engine/graph/RenderGraphCacheMetadata.h"
#include "engine/graph/RenderGraphTypes.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace engine::graph {
  class RenderGraphCachedArtifact;
  class RenderPassState;

  struct RenderGpuResourceResidency {
    std::string backend;
    std::string description;

    bool operator==(const RenderGpuResourceResidency& other) const;
    bool operator!=(const RenderGpuResourceResidency& other) const;
  };

  /**
    * Runtime resource allocated from a `RenderResourceDescriptor`.
    *
    * Descriptors are the serializable plan data. `RenderResource` is the
    * execution-time object that owns or exposes a concrete backing buffer and
    * answers capability questions such as whether the resource is color-backed.
    */
  class RenderResource {
  public:
    explicit RenderResource(RenderResourceDescriptor descriptor);
    virtual ~RenderResource();

    static std::unique_ptr<RenderResource> create(RenderResourceDescriptor descriptor);

    const RenderResourceDescriptor& descriptor() const;

    /**
      * @returns true when the current contents were written by disabled-pass
      * substitute-default behavior instead of a normal producer.
      */
    bool substituteDefault() const;

    /**
      * Marks this resource as written by a normal enabled producer or by a
      * disabled passthrough copy.
      */
    void markProduced();

    /**
      * Attaches producer metadata to this runtime resource.
      *
      * The descriptor remains the serializable shape of the resource; this
      * optional typed state lets a pass publish execution settings alongside a
      * transient request resource for downstream consumers in the same frame.
      */
    void setState(std::shared_ptr<const RenderPassState> state);
    std::shared_ptr<const RenderPassState> state() const;

    /**
      * Attaches cache provenance for the resource's current contents.
      */
    void setCacheMetadata(RenderGraphCacheMetadata metadata);
    const std::optional<RenderGraphCacheMetadata>& cacheMetadata() const;

    /**
      * Attaches the immutable cache artifact that backs this runtime resource.
      */
    void setArtifact(std::shared_ptr<const RenderGraphCachedArtifact> artifact);
    std::shared_ptr<const RenderGraphCachedArtifact> artifact() const;

    /**
      * Attaches backend-specific GPU residency metadata for descriptor-only
      * resources whose concrete OpenGL/Vulkan/etc. object stays outside the CPU
      * buffer storage layer.
      */
    void setGpuResidency(RenderGpuResourceResidency residency);
    void clearGpuResidency();
    const std::optional<RenderGpuResourceResidency>& gpuResidency() const;
    bool gpuResident() const;

    /**
      * @returns true when this resource owns a concrete CPU buffer.
      */
    virtual bool hasBuffer() const;

    /**
      * @returns true when this resource can be accessed as `Buffer<Colord>`.
      */
    virtual bool colorBacked() const;

    /**
      * @returns true when this resource can be accessed as `Buffer<double>`.
      */
    virtual bool depthBacked() const;

    /**
      * @returns true when this resource can be accessed as `Buffer<std::uint8_t>`.
      */
    virtual bool stencilBacked() const;

    /**
      * @returns true when this resource can be accessed as `Buffer<std::uint32_t>`.
      */
    virtual bool objectIdBacked() const;

    /**
      * Clears this resource to the value used when a disabled pass substitutes
      * a default output. Non-buffer descriptors ignore the request.
      */
    virtual void clearSubstituteDefault(RenderPassKind passKind, const Colord& beautyDefaultColor);

    virtual Buffer<Colord>& color();
    virtual const Buffer<Colord>& color() const;

    virtual Buffer<double>& depth();
    virtual const Buffer<double>& depth() const;

    virtual Buffer<std::uint8_t>& stencil();
    virtual const Buffer<std::uint8_t>& stencil() const;

    virtual Buffer<std::uint32_t>& objectId();
    virtual const Buffer<std::uint32_t>& objectId() const;

  private:
    std::out_of_range missingBuffer(const char* typeName) const;

    RenderResourceDescriptor m_descriptor;
    bool m_substituteDefault{false};
    std::shared_ptr<const RenderPassState> m_state;
    std::optional<RenderGraphCacheMetadata> m_cacheMetadata;
    std::shared_ptr<const RenderGraphCachedArtifact> m_artifact;
    std::optional<RenderGpuResourceResidency> m_gpuResidency;
  };

  /**
    * Descriptor-only resource for GPU, invalid-shape, or future resource
    * domains that the CPU storage layer records but does not allocate.
    */
  class DescriptorOnlyRenderResource : public RenderResource {
  public:
    using RenderResource::RenderResource;
  };

  /**
    * CPU resource backed by `Buffer<Colord>`.
    */
  class ColorRenderResource : public RenderResource {
  public:
    explicit ColorRenderResource(RenderResourceDescriptor descriptor);

    bool hasBuffer() const override;
    bool colorBacked() const override;
    void clearSubstituteDefault(RenderPassKind passKind, const Colord& beautyDefaultColor) override;
    Buffer<Colord>& color() override;
    const Buffer<Colord>& color() const override;

  private:
    Buffer<Colord> m_buffer;
  };

  /**
    * CPU resource backed by `Buffer<double>`.
    */
  class DepthRenderResource : public RenderResource {
  public:
    explicit DepthRenderResource(RenderResourceDescriptor descriptor);

    bool hasBuffer() const override;
    void clearSubstituteDefault(RenderPassKind passKind, const Colord& beautyDefaultColor) override;
    bool depthBacked() const override;
    Buffer<double>& depth() override;
    const Buffer<double>& depth() const override;

  private:
    Buffer<double> m_buffer;
  };

  /**
    * CPU resource backed by `Buffer<std::uint8_t>`.
    */
  class StencilRenderResource : public RenderResource {
  public:
    explicit StencilRenderResource(RenderResourceDescriptor descriptor);

    bool hasBuffer() const override;
    void clearSubstituteDefault(RenderPassKind passKind, const Colord& beautyDefaultColor) override;
    bool stencilBacked() const override;
    Buffer<std::uint8_t>& stencil() override;
    const Buffer<std::uint8_t>& stencil() const override;

  private:
    Buffer<std::uint8_t> m_buffer;
  };

  /**
    * CPU resource backed by `Buffer<std::uint32_t>`.
    */
  class ObjectIdRenderResource : public RenderResource {
  public:
    explicit ObjectIdRenderResource(RenderResourceDescriptor descriptor);

    bool hasBuffer() const override;
    void clearSubstituteDefault(RenderPassKind passKind, const Colord& beautyDefaultColor) override;
    bool objectIdBacked() const override;
    Buffer<std::uint32_t>& objectId() override;
    const Buffer<std::uint32_t>& objectId() const override;

  private:
    Buffer<std::uint32_t> m_buffer;
  };
}
