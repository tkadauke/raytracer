#pragma once

#include <QJsonObject>

#include <memory>
#include <string>

namespace engine::graph {
  class PostProcessAAState;
  class RasterBeautyPassState;
  class RasterShadowPassState;
  class RasterVisibilityPassState;
  class RaytracerBeautyPassState;
  enum class RenderExecutorKind;
  enum class RenderPassKind;
  class WireframePassState;

  /**
    * Typed payload state attached to a compiled render pass.
    *
    * JSON import/export remains a boundary concern: concrete subclasses parse
    * their JSON once when a plan is loaded, then graph execution works with
    * typed C++ state.
    */
  class RenderPassState {
  public:
    virtual ~RenderPassState() = default;

    /**
      * Parses the serialized pass-state object emitted under a graph pass's
      * `parameters` JSON field into the typed state used by execution.
      */
    static std::shared_ptr<const RenderPassState> fromJson(RenderPassKind kind,
                                                           RenderExecutorKind executor,
                                                           const QJsonObject& object,
                                                           const std::string& path);

    /**
      * Serializes this typed state for `RenderPlan::toJson()`.
      */
    virtual QJsonObject toJson() const = 0;

    virtual const RasterBeautyPassState* asRasterBeautyPassState() const;
    virtual const RasterShadowPassState* asRasterShadowPassState() const;
    virtual const RasterVisibilityPassState* asRasterVisibilityPassState() const;
    virtual const RaytracerBeautyPassState* asRaytracerBeautyPassState() const;
    virtual const WireframePassState* asWireframePassState() const;
    virtual const PostProcessAAState* asPostProcessAAState() const;
  };
}
