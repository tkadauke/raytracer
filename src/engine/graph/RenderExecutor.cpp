#include "engine/graph/RenderExecutor.h"

#include <algorithm>
#include <stdexcept>

namespace engine::graph {
  namespace {
    class RaytracerExecutorDefinition : public RenderExecutorDefinition {
    public:
      RenderExecutorKind kind() const override {
        return RenderExecutorKind::Raytracer;
      }

      RenderExecutorPreference preference() const override {
        return RenderExecutorPreference::Raytracer;
      }

      RenderFeatureKind feature() const override {
        return "raytracer";
      }

      std::string beautyPassId() const override {
        return "raytrace_beauty";
      }

      std::string beautyPassName() const override {
        return "Raytraced beauty";
      }
    };

    class RasterizerExecutorDefinition : public RenderExecutorDefinition {
    public:
      RenderExecutorKind kind() const override {
        return RenderExecutorKind::Rasterizer;
      }

      RenderExecutorPreference preference() const override {
        return RenderExecutorPreference::Rasterizer;
      }

      RenderFeatureKind feature() const override {
        return "rasterizer";
      }

      std::string beautyPassId() const override {
        return "raster_beauty";
      }

      std::string beautyPassName() const override {
        return "Raster beauty";
      }
    };

    class WireframeExecutorDefinition : public RenderExecutorDefinition {
    public:
      RenderExecutorKind kind() const override {
        return RenderExecutorKind::Wireframe;
      }

      RenderExecutorPreference preference() const override {
        return RenderExecutorPreference::Wireframe;
      }

      RenderFeatureKind feature() const override {
        return "wireframe";
      }

      std::string beautyPassId() const override {
        return "wireframe_beauty";
      }

      std::string beautyPassName() const override {
        return "Wireframe beauty";
      }
    };

    const std::vector<const RenderExecutorDefinition*>& definitions() {
      static const RaytracerExecutorDefinition raytracer;
      static const RasterizerExecutorDefinition rasterizer;
      static const WireframeExecutorDefinition wireframe;
      static const std::vector<const RenderExecutorDefinition*> result = {&raytracer, &rasterizer,
                                                                          &wireframe};
      return result;
    }
  }

  bool RenderExecutorDefinition::matches(RenderExecutorKind executor) const {
    return kind() == executor;
  }

  bool RenderExecutorDefinition::matches(RenderExecutorPreference executor) const {
    return preference() == executor;
  }

  const RenderExecutorDefinition& renderExecutorDefinition(RenderExecutorPreference executor) {
    const auto& all = definitions();
    const auto it = std::find_if(all.begin(), all.end(), [&](const auto* definition) {
      return definition->matches(executor);
    });
    if (it == all.end()) {
      throw std::runtime_error("unsupported render executor preference");
    }
    return **it;
  }

  const RenderExecutorDefinition* renderExecutorDefinition(RenderExecutorKind executor) {
    const auto& all = definitions();
    const auto it = std::find_if(all.begin(), all.end(), [&](const auto* definition) {
      return definition->matches(executor);
    });
    return it == all.end() ? nullptr : *it;
  }

  std::vector<const RenderExecutorDefinition*> renderExecutorDefinitions() {
    return definitions();
  }
}
