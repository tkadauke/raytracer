#pragma once

#include "core/Color.h"
#include "engine/graph/RenderPassState.h"

#include <QJsonObject>

#include <memory>
#include <string>

template<class T>
class Buffer;

namespace engine::graph {
  struct RenderPassNode;

  /**
    * Typed state for image-space anti-aliasing postprocess graph passes.
    */
  class PostProcessAAState : public RenderPassState {
  public:
    static std::shared_ptr<const PostProcessAAState>
    fromJson(const QJsonObject& object, const std::string& path = "parameters");
    static std::shared_ptr<const PostProcessAAState> fromPass(const RenderPassNode& pass);

    QJsonObject toJson() const override;

    virtual const char* modeName() const = 0;
    virtual void apply(Buffer<Colord>& buffer) const = 0;
  };

  /**
    * Fast approximate anti-aliasing filter state.
    */
  class FxaaPostProcessAAState : public PostProcessAAState {
  public:
    const char* modeName() const override;
    void apply(Buffer<Colord>& buffer) const override;
  };

  /**
    * Subpixel morphological anti-aliasing filter state.
    */
  class SmaaPostProcessAAState : public PostProcessAAState {
  public:
    const char* modeName() const override;
    void apply(Buffer<Colord>& buffer) const override;
  };
}
