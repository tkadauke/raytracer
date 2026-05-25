#include "engine/graph/PostProcessPassState.h"

#include "engine/graph/RenderGraphTypes.h"
#include "render/postprocess/Fxaa.h"
#include "render/postprocess/Smaa.h"

#include <algorithm>
#include <initializer_list>
#include <stdexcept>
#include <utility>
#include <vector>

namespace engine::graph {
  namespace {
    [[noreturn]] void stateError(const std::string& path, const std::string& message) {
      throw std::runtime_error("Invalid postprocess pass state at " + path + ": " + message);
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

    std::string stringField(const QJsonObject& object, const char* key, const std::string& path) {
      const auto value = object.value(key);
      if (!value.isString())
        stateError(path + "." + key, "expected string");
      return value.toString().toStdString();
    }

    bool hasFeature(const RenderPassNode& pass, const char* feature) {
      return std::any_of(pass.features.begin(), pass.features.end(),
                         [feature](const RenderFeatureKind& value) { return value == feature; });
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
    rejectUnknownFields(object, path, {"type", "mode"});

    const std::string type = stringField(object, "type", path);
    if (type != "post_process_aa")
      stateError(path + ".type", "expected post_process_aa");

    return aaStateForMode(stringField(object, "mode", path), path + ".mode");
  }

  std::shared_ptr<const PostProcessAAState>
  PostProcessAAState::fromPass(const RenderPassNode& pass) {
    if (auto state = std::dynamic_pointer_cast<const PostProcessAAState>(pass.state))
      return state;

    if (hasFeature(pass, "post_aa") && hasFeature(pass, "fxaa"))
      return std::make_shared<FxaaPostProcessAAState>();
    if (hasFeature(pass, "post_aa") && hasFeature(pass, "smaa"))
      return std::make_shared<SmaaPostProcessAAState>();

    return nullptr;
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
