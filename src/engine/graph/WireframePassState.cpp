#include "engine/graph/WireframePassState.h"

#include "engine/graph/RenderGraphTypes.h"
#include "engine/graph/RenderPlan.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <stdexcept>
#include <string>

namespace engine::graph {
  namespace {
    [[noreturn]] void stateError(const std::string& path, const std::string& message) {
      throw std::runtime_error("Invalid wireframe pass state at " + path + ": " + message);
    }

    bool hasField(const QJsonObject& object, const char* key) {
      return !object.value(key).isUndefined();
    }

    void rejectUnknownFields(const QJsonObject& object, const std::string& path,
                             std::initializer_list<const char*> allowed) {
      for (auto it = object.begin(); it != object.end(); ++it) {
        const std::string key = it.key().toStdString();
        const bool matched = std::find(allowed.begin(), allowed.end(), key) != allowed.end();
        if (!matched)
          stateError(path + "." + key, "unknown field");
      }
    }

    int intField(const QJsonObject& object, const char* key, const std::string& path) {
      const auto value = object.value(key);
      if (!value.isDouble())
        stateError(path + "." + key, "expected integer");

      const double number = value.toDouble();
      if (!std::isfinite(number) || std::floor(number) != number)
        stateError(path + "." + key, "expected integer");
      return static_cast<int>(number);
    }
  }

  WireframePassState WireframePassState::fromJson(const QJsonObject& object,
                                                  const std::string& path) {
    rejectUnknownFields(object, path, {"lod"});

    WireframePassState state;
    if (hasField(object, "lod"))
      state.setLod(intField(object, "lod", path));
    return state;
  }

  const WireframePassState* WireframePassState::fromPass(const RenderPassNode& pass) {
    if (pass.executor != RenderExecutorKind::Wireframe)
      return nullptr;
    return pass.state ? pass.state->asWireframePassState() : nullptr;
  }

  WireframePassState WireframePassState::valueFromPass(const RenderPassNode& pass) {
    const auto* state = fromPass(pass);
    return state ? *state : WireframePassState();
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
    m_lod = std::max(0, lod);
  }

  int WireframePassState::lod() const {
    return m_lod;
  }
}
