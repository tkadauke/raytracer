#include "engine/graph/WireframePassState.h"

#include "engine/graph/RenderGraphTypes.h"
#include "engine/graph/RenderPlan.h"
#include "engine/graph/detail/JsonStateHelpers.h"

#include <algorithm>
#include <initializer_list>
#include <stdexcept>
#include <string>

namespace engine::graph {
  namespace {
    [[noreturn]] void stateError(const std::string& path, const std::string& message) {
      detail::throwPassStateError("wireframe", path, message);
    }

  }

  WireframePassState WireframePassState::fromJson(const QJsonObject& object,
                                                  const std::string& path) {
    detail::rejectUnknownFields(object, path, {"lod"}, stateError);

    WireframePassState state;
    if (detail::hasField(object, "lod"))
      state.setLod(detail::intField(object, "lod", path, stateError));
    return state;
  }

  const WireframePassState* WireframePassState::fromPass(const RenderPassNode& pass) {
    if (pass.executor != RenderExecutorKind::Wireframe)
      return nullptr;
    return pass.state ? pass.state->asWireframePassState() : nullptr;
  }

  WireframePassState WireframePassState::valueFromPass(const RenderPassNode& pass) {
    return detail::valueOrDefault(fromPass(pass));
  }

  QJsonObject WireframePassState::toJson() const {
    QJsonObject object;
    if (m_lod != 0)
      object["lod"] = m_lod;
    return object;
  }

  const WireframePassState* WireframePassState::asWireframePassState() const {
    return this;
  }

  bool WireframePassState::empty() const {
    return toJson().isEmpty();
  }

  void WireframePassState::applyTo(Wireframe& wireframe) const {
    wireframe.setLod(m_lod);
  }

  void WireframePassState::writeTo(RenderPassNode& pass) const {
    if (empty()) {
      pass.state.reset();
    } else {
      pass.state = std::make_shared<WireframePassState>(*this);
    }
  }

  std::size_t WireframePassState::writeToWireframePasses(RenderPlan& plan) const {
    return plan.setPassState(RenderPassKind::Beauty, RenderExecutorKind::Wireframe,
                             empty() ? nullptr : std::make_shared<WireframePassState>(*this)) +
           plan.setPassState(RenderPassKind::Overlay, RenderExecutorKind::Wireframe,
                             empty() ? nullptr : std::make_shared<WireframePassState>(*this));
  }

  void WireframePassState::setLod(int lod) {
    m_lod = detail::clampedLod(lod);
  }

  int WireframePassState::lod() const {
    return m_lod;
  }
}
