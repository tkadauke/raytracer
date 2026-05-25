#pragma once

#include "core/Color.h"
#include "engine/graph/RenderPassState.h"
#include "engine/graph/RenderGraphTypes.h"

#include <QJsonObject>

#include <memory>
#include <string>

template<class T>
class Buffer;

namespace engine::graph {
  struct RenderPassNode;

  class PostProcessAAState;

  /**
    * Graph-pass definition for an image-space anti-aliasing filter.
    */
  class PostProcessAADefinition {
  public:
    virtual ~PostProcessAADefinition() = default;

    virtual RenderPostProcessAA mode() const = 0;
    virtual const char* passId() const = 0;
    virtual const char* passName() const = 0;
    virtual const char* feature() const = 0;
    virtual std::shared_ptr<const PostProcessAAState> createState() const = 0;

    bool matches(RenderPostProcessAA aa) const;
  };

  const PostProcessAADefinition* postProcessAADefinition(RenderPostProcessAA aa);

  /**
    * Typed state for image-space anti-aliasing postprocess graph passes.
    */
  class PostProcessAAState : public RenderPassState {
  public:
    static std::shared_ptr<const PostProcessAAState>
    fromJson(const QJsonObject& object, const std::string& path = "parameters");
    static std::shared_ptr<const PostProcessAAState> fromPass(const RenderPassNode& pass);

    const PostProcessAAState* asPostProcessAAState() const override;
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
