#pragma once

#include "engine/graph/RenderPassState.h"
#include "engine/wireframe/Wireframe.h"

#include <QJsonObject>

#include <cstddef>
#include <memory>
#include <string>

namespace engine::graph {
  class RenderPlan;
  struct RenderPassNode;

  /**
    * Typed state for built-in wireframe beauty and overlay graph passes.
    */
  class WireframePassState : public RenderPassState {
  public:
    using Wireframe = engine::wireframe::Wireframe;

    static WireframePassState fromJson(const QJsonObject& object,
                                       const std::string& path = "parameters");
    static const WireframePassState* fromPass(const RenderPassNode& pass);
    static WireframePassState valueFromPass(const RenderPassNode& pass);

    QJsonObject toJson() const override;
    bool empty() const;
    void applyTo(Wireframe& wireframe) const;

    void writeTo(RenderPassNode& pass) const;
    std::size_t writeToWireframePasses(RenderPlan& plan) const;

    void setLod(int lod);
    int lod() const;

  private:
    int m_lod{0};
  };
}
