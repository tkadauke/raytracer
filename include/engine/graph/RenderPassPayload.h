#pragma once

#include <memory>

namespace engine::graph {
  class RenderExecutionContext;
}

template<class T>
class Buffer;

namespace render {
  class Tonemap;
}

namespace engine::graph {

  /**
    * Executor-specific implementation behind a compiled render pass node.
    *
    * `RenderPassNode` remains a serializable declaration of resource access and
    * scheduling requirements. Payload subclasses perform the actual work for a
    * raytracing, raster, wireframe, composite, or postprocess pass.
    */
  class RenderPassPayload {
  public:
    virtual ~RenderPassPayload() = default;

    /**
      * Executes the pass against the resources and frame state in @p context.
      */
    virtual void execute(RenderExecutionContext& context) = 0;

    /**
      * Optional display-buffer fast path.
      *
      * Payloads that can produce packed RGB directly return true after writing
      * @p buffer. The graph engine uses this for simple preview graphs so
      * wrapped engines such as the raytracer can keep publishing progressive
      * LDR pixels while the frame is still rendering.
      */
    virtual bool executeDisplay(RenderExecutionContext&,
                                Buffer<unsigned int>&,
                                std::shared_ptr<render::Tonemap>) {
      return false;
    }
  };
}
