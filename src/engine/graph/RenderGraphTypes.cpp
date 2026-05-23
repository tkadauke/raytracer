#include "engine/graph/RenderGraphTypes.h"

#include <utility>

namespace engine::graph {
  SceneSelector SceneSelector::all() {
    return SceneSelector{Kind::All, ""};
  }

  SceneSelector SceneSelector::objectId(std::string id) {
    return SceneSelector{Kind::ObjectId, std::move(id)};
  }

  SceneSelector SceneSelector::objectName(std::string name) {
    return SceneSelector{Kind::ObjectName, std::move(name)};
  }

  SceneSelector SceneSelector::tag(std::string tagName) {
    return SceneSelector{Kind::Tag, std::move(tagName)};
  }

  SceneSelector SceneSelector::layer(std::string layerName) {
    return SceneSelector{Kind::Layer, std::move(layerName)};
  }

  SceneSelector SceneSelector::materialRole(std::string role) {
    return SceneSelector{Kind::MaterialRole, std::move(role)};
  }

  const char* toString(RenderExecutorPreference value) {
    switch (value) {
    case RenderExecutorPreference::Raytracer:
      return "raytracer";
    case RenderExecutorPreference::Rasterizer:
      return "rasterizer";
    case RenderExecutorPreference::Wireframe:
      return "wireframe";
    }
    return "unknown";
  }

  const char* toString(RenderViewMode value) {
    switch (value) {
    case RenderViewMode::Default:
      return "default";
    case RenderViewMode::Beauty:
      return "beauty";
    case RenderViewMode::Wireframe:
      return "wireframe";
    case RenderViewMode::Depth:
      return "depth";
    case RenderViewMode::Normal:
      return "normal";
    case RenderViewMode::ObjectId:
      return "object_id";
    }
    return "unknown";
  }

  const char* toString(RenderExecutorKind value) {
    switch (value) {
    case RenderExecutorKind::Raytracer:
      return "raytracer";
    case RenderExecutorKind::Rasterizer:
      return "rasterizer";
    case RenderExecutorKind::Wireframe:
      return "wireframe";
    case RenderExecutorKind::Composite:
      return "composite";
    case RenderExecutorKind::PostProcess:
      return "postprocess";
    }
    return "unknown";
  }

  const char* toString(RenderPassKind value) {
    switch (value) {
    case RenderPassKind::Beauty:
      return "beauty";
    case RenderPassKind::Shadow:
      return "shadow";
    case RenderPassKind::Overlay:
      return "overlay";
    case RenderPassKind::Composite:
      return "composite";
    case RenderPassKind::Tonemap:
      return "tonemap";
    case RenderPassKind::PostProcess:
      return "postprocess";
    case RenderPassKind::AOV:
      return "aov";
    case RenderPassKind::Debug:
      return "debug";
    case RenderPassKind::Custom:
      return "custom";
    }
    return "unknown";
  }

  const char* toString(DisabledBehavior value) {
    switch (value) {
    case DisabledBehavior::Error:
      return "error";
    case DisabledBehavior::CullDependents:
      return "cull_dependents";
    case DisabledBehavior::SubstituteDefault:
      return "substitute_default";
    case DisabledBehavior::Passthrough:
      return "passthrough";
    }
    return "unknown";
  }

  const char* toString(RenderResourceType value) {
    switch (value) {
    case RenderResourceType::Color:
      return "color";
    case RenderResourceType::Depth:
      return "depth";
    case RenderResourceType::Stencil:
      return "stencil";
    case RenderResourceType::ObjectId:
      return "object_id";
    case RenderResourceType::MaterialId:
      return "material_id";
    case RenderResourceType::Normal:
      return "normal";
    case RenderResourceType::WorldPosition:
      return "world_position";
    case RenderResourceType::MotionVector:
      return "motion_vector";
    case RenderResourceType::ShadowMap:
      return "shadow_map";
    case RenderResourceType::ShadowMask:
      return "shadow_mask";
    case RenderResourceType::CustomTexture:
      return "custom_texture";
    }
    return "unknown";
  }

  const char* toString(RenderResourceDomain value) {
    switch (value) {
    case RenderResourceDomain::CPU:
      return "cpu";
    case RenderResourceDomain::GPU:
      return "gpu";
    }
    return "unknown";
  }

  const char* toString(RenderResourceLifetime value) {
    switch (value) {
    case RenderResourceLifetime::Transient:
      return "transient";
    case RenderResourceLifetime::Imported:
      return "imported";
    case RenderResourceLifetime::Exported:
      return "exported";
    case RenderResourceLifetime::History:
      return "history";
    case RenderResourceLifetime::PersistentCache:
      return "persistent_cache";
    }
    return "unknown";
  }

  const char* toString(RenderResourceFormat value) {
    switch (value) {
    case RenderResourceFormat::Unknown:
      return "unknown";
    case RenderResourceFormat::RGBDouble:
      return "rgb_double";
    case RenderResourceFormat::DepthDouble:
      return "depth_double";
    case RenderResourceFormat::UInt8:
      return "uint8";
    case RenderResourceFormat::UInt32:
      return "uint32";
    case RenderResourceFormat::ScalarDouble:
      return "scalar_double";
    }
    return "unknown";
  }

  const char* toString(SceneSelector::Kind value) {
    switch (value) {
    case SceneSelector::Kind::All:
      return "all";
    case SceneSelector::Kind::ObjectId:
      return "object_id";
    case SceneSelector::Kind::ObjectName:
      return "object_name";
    case SceneSelector::Kind::Tag:
      return "tag";
    case SceneSelector::Kind::Layer:
      return "layer";
    case SceneSelector::Kind::MaterialRole:
      return "material_role";
    }
    return "unknown";
  }
}
