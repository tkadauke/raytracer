#pragma once

namespace engine::graph {
  class RenderExecutionContext;

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
  };
}
