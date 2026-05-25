#include "engine/graph/RenderAOV.h"

#include "engine/graph/RenderGraphCompiler.h"

#include <algorithm>
#include <utility>

namespace engine::graph {
  namespace {
    std::string normalizedFeatureName(std::string value) {
      value.erase(std::remove(value.begin(), value.end(), '_'), value.end());
      value.erase(std::remove(value.begin(), value.end(), '-'), value.end());
      return value;
    }

    class BasicRenderAOVDefinition : public RenderAOVDefinition {
    public:
      BasicRenderAOVDefinition(RenderViewMode viewMode, std::string feature, std::string title,
                               RenderResourceType resourceType, RenderResourceFormat resourceFormat)
          : m_viewMode(viewMode),
            m_feature(std::move(feature)),
            m_title(std::move(title)),
            m_resourceType(resourceType),
            m_resourceFormat(resourceFormat) {
      }

      RenderViewMode viewMode() const override {
        return m_viewMode;
      }

      std::string feature() const override {
        return m_feature;
      }

      std::string title() const override {
        return m_title;
      }

      RenderResourceType resourceType() const override {
        return m_resourceType;
      }

      RenderResourceFormat resourceFormat() const override {
        return m_resourceFormat;
      }

    private:
      RenderViewMode m_viewMode;
      std::string m_feature;
      std::string m_title;
      RenderResourceType m_resourceType;
      RenderResourceFormat m_resourceFormat;
    };

    const std::vector<const RenderAOVDefinition*>& definitions() {
      static const BasicRenderAOVDefinition depth(RenderViewMode::Depth, "depth", "Depth",
                                                  RenderResourceType::Depth,
                                                  RenderResourceFormat::DepthDouble);
      static const BasicRenderAOVDefinition normal(RenderViewMode::Normal, "normal", "Normal",
                                                   RenderResourceType::Normal,
                                                   RenderResourceFormat::RGBDouble);
      static const BasicRenderAOVDefinition objectId(RenderViewMode::ObjectId, "object_id",
                                                     "Object ID", RenderResourceType::ObjectId,
                                                     RenderResourceFormat::UInt32);
      static const BasicRenderAOVDefinition materialId(
        RenderViewMode::MaterialId, "material_id", "Material ID", RenderResourceType::MaterialId,
        RenderResourceFormat::UInt32);
      static const BasicRenderAOVDefinition worldPosition(
        RenderViewMode::WorldPosition, "world_position", "World position",
        RenderResourceType::WorldPosition, RenderResourceFormat::RGBDouble);
      static const std::vector<const RenderAOVDefinition*> result = {&depth, &normal, &objectId,
                                                                     &materialId, &worldPosition};
      return result;
    }
  }

  RenderResourceId RenderAOVDefinition::resourceId() const {
    return feature() + "_aov";
  }

  RenderResourceId RenderAOVDefinition::previewColorResourceId() const {
    return resourceId() + "_color";
  }

  RenderResourceDescriptor
  RenderAOVDefinition::resourceDescriptor(const RenderTargetSpec& target,
                                          RenderResourceLifetime lifetime) const {
    RenderResourceDescriptor resource;
    resource.id = resourceId();
    resource.name = title() + " AOV";
    resource.type = resourceType();
    resource.format = resourceFormat();
    resource.width = target.width;
    resource.height = target.height;
    resource.sampleCount = 1;
    resource.domain = RenderResourceDomain::CPU;
    resource.lifetime = lifetime;
    return resource;
  }

  bool RenderAOVDefinition::matchesName(const std::string& normalizedName) const {
    return normalizedName == normalizedFeatureName(feature());
  }

  const RenderAOVDefinition* renderAOVDefinition(RenderViewMode viewMode) {
    const auto& all = definitions();
    const auto it = std::find_if(all.begin(), all.end(), [&](const RenderAOVDefinition* aov) {
      return aov->viewMode() == viewMode;
    });
    return it == all.end() ? nullptr : *it;
  }

  const RenderAOVDefinition* renderAOVDefinitionForName(const std::string& normalizedName) {
    const auto& all = definitions();
    const auto it = std::find_if(all.begin(), all.end(), [&](const RenderAOVDefinition* aov) {
      return aov->matchesName(normalizedName);
    });
    return it == all.end() ? nullptr : *it;
  }

  std::vector<const RenderAOVDefinition*> renderAOVDefinitions() {
    return definitions();
  }
}
