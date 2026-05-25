#pragma once

#include "engine/graph/RenderPlan.h"

namespace engine::graph {
  /**
    * Dimensions and sampling shape of the image a render plan targets.
    *
    * Graph compilation is intentionally independent of drawing pixels, but the
    * compiler still needs the render target shape so it can declare resource
    * descriptors with concrete dimensions.
    */
  struct RenderTargetSpec {
    int width{0};
    int height{0};
    int sampleCount{1};
  };

  /**
    * Builds a declarative render plan from user-facing render intent.
    *
    * This first compiler slice emits a whole-frame beauty pass backed by one
    * existing engine executor, optionally routes that image through wireframe
    * or curve-overlay passes, then emits a tonemap pass that writes the exported
    * color resource. Later slices will expand scene features into shadow,
    * postprocess, composite, AOV, and history-resource passes.
    */
  class RenderGraphCompiler {
  public:
    /**
      * Compiles the minimal graph for @p target and @p intent.
      *
      * The returned plan can be inspected, exported, disabled through
      * `RenderGraphOverrides`, or handed to `GraphRenderEngine` for execution.
      */
    RenderPlan compile(const RenderTargetSpec& target, const RenderIntent& intent) const;
  };
}
