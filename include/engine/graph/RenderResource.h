#pragma once

#include "core/Buffer.h"
#include "core/Color.h"
#include "engine/graph/RenderGraphTypes.h"

#include <cstdint>
#include <memory>
#include <stdexcept>

namespace engine::graph {
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
      * @returns true when this resource owns a concrete CPU buffer.
      */
    virtual bool hasBuffer() const;

    /**
      * @returns true when this resource can be accessed as `Buffer<Colord>`.
      */
    virtual bool colorBacked() const;

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
    Buffer<std::uint32_t>& objectId() override;
    const Buffer<std::uint32_t>& objectId() const override;

  private:
    Buffer<std::uint32_t> m_buffer;
  };
}
