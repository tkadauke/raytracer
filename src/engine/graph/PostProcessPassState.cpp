#include "engine/graph/PostProcessPassState.h"

#include "engine/graph/RenderGraphTypes.h"
#include "engine/graph/detail/JsonStateHelpers.h"
#include "render/postprocess/Fxaa.h"
#include "render/postprocess/Smaa.h"

#include <initializer_list>
#include <stdexcept>
#include <utility>
#include <vector>

namespace engine::graph {
  namespace {
    [[noreturn]] void stateError(const std::string& path, const std::string& message) {
      throw std::runtime_error("Invalid postprocess pass state at " + path + ": " + message);
    }


    std::shared_ptr<const PostProcessAAState> aaStateForMode(const std::string& mode,
                                                             const std::string& path) {
      if (mode == "fxaa")
        return std::make_shared<FxaaPostProcessAAState>();
      if (mode == "smaa")
        return std::make_shared<SmaaPostProcessAAState>();
      stateError(path, "expected fxaa or smaa");
    }

    class FxaaPostProcessAADefinition : public PostProcessAADefinition {
    public:
      RenderPostProcessAA mode() const override {
        return RenderPostProcessAA::FXAA;
      }

      const char* passId() const override {
        return "post_fxaa";
      }

      const char* passName() const override {
        return "FXAA";
      }

      const char* feature() const override {
        return "fxaa";
      }

      std::shared_ptr<const PostProcessAAState> createState() const override {
        return std::make_shared<FxaaPostProcessAAState>();
      }
    };

    class SmaaPostProcessAADefinition : public PostProcessAADefinition {
    public:
      RenderPostProcessAA mode() const override {
        return RenderPostProcessAA::SMAA;
      }

      const char* passId() const override {
        return "post_smaa";
      }

      const char* passName() const override {
        return "SMAA";
      }

      const char* feature() const override {
        return "smaa";
      }

      std::shared_ptr<const PostProcessAAState> createState() const override {
        return std::make_shared<SmaaPostProcessAAState>();
      }
    };

    const std::vector<const PostProcessAADefinition*>& definitions() {
      static const FxaaPostProcessAADefinition fxaa;
      static const SmaaPostProcessAADefinition smaa;
      static const std::vector<const PostProcessAADefinition*> result = {&fxaa, &smaa};
      return result;
    }
  }

  bool PostProcessAADefinition::matches(RenderPostProcessAA aa) const {
    return mode() == aa;
  }

  const PostProcessAADefinition* postProcessAADefinition(RenderPostProcessAA aa) {
    const auto& all = definitions();
    const auto it = std::find_if(all.begin(), all.end(),
                                 [&](const auto* definition) { return definition->matches(aa); });
    return it == all.end() ? nullptr : *it;
  }

  std::shared_ptr<const PostProcessAAState> PostProcessAAState::fromJson(const QJsonObject& object,
                                                                         const std::string& path) {
    detail::rejectUnknownFields(object, path, {"type", "mode"}, stateError);

    const std::string type = detail::stringField(object, "type", path, stateError);
    if (type != "post_process_aa")
      stateError(path + ".type", "expected post_process_aa");

    return aaStateForMode(detail::stringField(object, "mode", path, stateError), path + ".mode");
  }

  std::shared_ptr<const PostProcessAAState>
  PostProcessAAState::fromPass(const RenderPassNode& pass) {
    if (pass.state) {
      if (auto* state = pass.state->asPostProcessAAState()) {
        return std::shared_ptr<const PostProcessAAState>(pass.state, state);
      }
    }

    if (pass.hasFeature("post_aa") && pass.hasFeature("fxaa"))
      return std::make_shared<FxaaPostProcessAAState>();
    if (pass.hasFeature("post_aa") && pass.hasFeature("smaa"))
      return std::make_shared<SmaaPostProcessAAState>();

    return nullptr;
  }

  const PostProcessAAState* PostProcessAAState::asPostProcessAAState() const {
    return this;
  }

  QJsonObject PostProcessAAState::toJson() const {
    QJsonObject object;
    object["type"] = "post_process_aa";
    object["mode"] = modeName();
    return object;
  }

  const char* FxaaPostProcessAAState::modeName() const {
    return "fxaa";
  }

  void FxaaPostProcessAAState::apply(Buffer<Colord>& buffer) const {
    render::postprocess::applyFxaa(buffer);
  }

  const char* SmaaPostProcessAAState::modeName() const {
    return "smaa";
  }

  void SmaaPostProcessAAState::apply(Buffer<Colord>& buffer) const {
    render::postprocess::applySmaa(buffer);
  }
}
