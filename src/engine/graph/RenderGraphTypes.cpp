#include "engine/graph/RenderGraphTypes.h"

#include <QJsonArray>

#include <cstddef>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <utility>

namespace engine::graph {
  namespace {
    QString qstr(const std::string& value) {
      return QString::fromStdString(value);
    }

    [[noreturn]] void jsonError(const std::string& path, const std::string& message) {
      throw std::runtime_error("Invalid render intent JSON at " + path + ": " + message);
    }

    std::string stringField(const QJsonObject& object, const char* key, const std::string& path,
                            const std::string& fallback = {}) {
      const auto value = object.value(key);
      if (value.isUndefined())
        return fallback;
      if (!value.isString())
        jsonError(path + "." + key, "expected string");
      return value.toString().toStdString();
    }

    bool boolField(const QJsonObject& object, const char* key, const std::string& path,
                   bool fallback) {
      const auto value = object.value(key);
      if (value.isUndefined())
        return fallback;
      if (!value.isBool())
        jsonError(path + "." + key, "expected boolean");
      return value.toBool();
    }

    template<class T>
    T enumValue(const std::string& value, std::initializer_list<std::pair<const char*, T>> values,
                const std::string& path) {
      for (const auto& [name, parsed] : values) {
        if (value == name)
          return parsed;
      }
      jsonError(path, "unknown value '" + value + "'");
    }

    RenderExecutorPreference executorPreferenceFromJson(const std::string& value,
                                                        const std::string& path) {
      return enumValue<RenderExecutorPreference>(
        value,
        {{"raytracer", RenderExecutorPreference::Raytracer},
         {"rasterizer", RenderExecutorPreference::Rasterizer},
         {"wireframe", RenderExecutorPreference::Wireframe}},
        path);
    }

    RenderViewMode viewModeFromJson(const std::string& value, const std::string& path) {
      return enumValue<RenderViewMode>(value,
                                       {{"default", RenderViewMode::Default},
                                        {"beauty", RenderViewMode::Beauty},
                                        {"wireframe", RenderViewMode::Wireframe},
                                        {"depth", RenderViewMode::Depth},
                                        {"normal", RenderViewMode::Normal},
                                        {"object_id", RenderViewMode::ObjectId}},
                                       path);
    }

    SceneSelector::Kind selectorKindFromJson(const std::string& value, const std::string& path) {
      return enumValue<SceneSelector::Kind>(value,
                                            {{"all", SceneSelector::Kind::All},
                                             {"object_id", SceneSelector::Kind::ObjectId},
                                             {"object_name", SceneSelector::Kind::ObjectName},
                                             {"tag", SceneSelector::Kind::Tag},
                                             {"layer", SceneSelector::Kind::Layer},
                                             {"material_role", SceneSelector::Kind::MaterialRole}},
                                            path);
    }

    QJsonObject selectorToJson(const SceneSelector& selector) {
      QJsonObject result;
      result["kind"] = toString(selector.kind);
      if (!selector.value.empty())
        result["value"] = qstr(selector.value);
      return result;
    }

    SceneSelector selectorFromJson(const QJsonObject& object, const std::string& path) {
      return {selectorKindFromJson(stringField(object, "kind", path, "all"), path + ".kind"),
              stringField(object, "value", path)};
    }

    QJsonObject shadingProfileToJson(const ShadingProfileRef& profile) {
      QJsonObject result;
      result["name"] = qstr(profile.name);
      if (!profile.parameters.isEmpty())
        result["parameters"] = profile.parameters;
      return result;
    }

    ShadingProfileRef shadingProfileFromJson(const QJsonValue& value, const std::string& path) {
      ShadingProfileRef profile;
      if (value.isUndefined())
        return profile;

      if (value.isString()) {
        profile.name = value.toString().toStdString();
        return profile;
      }

      if (!value.isObject())
        jsonError(path, "expected string or object");

      const auto object = value.toObject();
      profile.name = stringField(object, "name", path, "default");

      const auto parameters = object.value("parameters");
      if (!parameters.isUndefined()) {
        if (!parameters.isObject())
          jsonError(path + ".parameters", "expected object");
        profile.parameters = parameters.toObject();
      }

      return profile;
    }

    QJsonObject cameraRefToJson(const RenderCameraRef& camera) {
      QJsonObject result;
      if (camera.sceneCameraId)
        result["sceneCameraId"] = qstr(*camera.sceneCameraId);
      if (camera.snapshot)
        result["snapshot"] = camera.snapshot->parameters;
      return result;
    }

    RenderCameraRef cameraRefFromJson(const QJsonValue& value, const std::string& path) {
      if (!value.isObject())
        jsonError(path, "expected object");

      const auto object = value.toObject();
      RenderCameraRef camera;
      const auto sceneCameraId = object.value("sceneCameraId");
      if (!sceneCameraId.isUndefined()) {
        if (!sceneCameraId.isString())
          jsonError(path + ".sceneCameraId", "expected string");
        camera.sceneCameraId = sceneCameraId.toString().toStdString();
      }

      const auto snapshot = object.value("snapshot");
      if (!snapshot.isUndefined()) {
        if (!snapshot.isObject())
          jsonError(path + ".snapshot", "expected object");
        camera.snapshot = CameraSnapshot{snapshot.toObject()};
      }
      return camera;
    }

    QJsonObject viewOverrideToJson(const RenderViewOverride& viewOverride) {
      QJsonObject result;
      result["selector"] = selectorToJson(viewOverride.selector);
      if (viewOverride.executor)
        result["executor"] = toString(*viewOverride.executor);
      if (viewOverride.viewMode)
        result["viewMode"] = toString(*viewOverride.viewMode);
      if (viewOverride.shadingProfile)
        result["shadingProfile"] = shadingProfileToJson(*viewOverride.shadingProfile);
      if (viewOverride.camera)
        result["camera"] = cameraRefToJson(*viewOverride.camera);
      return result;
    }

    RenderViewOverride viewOverrideFromJson(const QJsonObject& object, const std::string& path) {
      RenderViewOverride viewOverride;
      const auto selector = object.value("selector");
      if (!selector.isUndefined()) {
        if (!selector.isObject())
          jsonError(path + ".selector", "expected object");
        viewOverride.selector = selectorFromJson(selector.toObject(), path + ".selector");
      } else {
        viewOverride.selector = SceneSelector::all();
      }

      const auto executor = object.value("executor");
      if (!executor.isUndefined())
        viewOverride.executor =
          executorPreferenceFromJson(stringField(object, "executor", path), path + ".executor");

      const auto viewMode = object.value("viewMode");
      if (!viewMode.isUndefined())
        viewOverride.viewMode =
          viewModeFromJson(stringField(object, "viewMode", path), path + ".viewMode");

      const auto shadingProfile = object.value("shadingProfile");
      if (!shadingProfile.isUndefined())
        viewOverride.shadingProfile =
          shadingProfileFromJson(shadingProfile, path + ".shadingProfile");

      const auto camera = object.value("camera");
      if (!camera.isUndefined())
        viewOverride.camera = cameraRefFromJson(camera, path + ".camera");

      return viewOverride;
    }
  }

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

  QJsonObject RenderIntent::toJson() const {
    QJsonObject result;
    result["defaultExecutor"] = toString(defaultExecutor);
    result["defaultViewMode"] = toString(defaultViewMode);
    result["defaultShadingProfile"] = shadingProfileToJson(defaultShadingProfile);
    if (defaultCamera)
      result["defaultCamera"] = cameraRefToJson(*defaultCamera);
    result["enableAutomaticFeatures"] = enableAutomaticFeatures;
    result["enableWireframeOverlay"] = enableWireframeOverlay;
    result["enablePreviewShadows"] = enablePreviewShadows;

    if (!viewOverrides.empty()) {
      QJsonArray overrides;
      for (const auto& viewOverride : viewOverrides)
        overrides.append(viewOverrideToJson(viewOverride));
      result["viewOverrides"] = overrides;
    }

    return result;
  }

  RenderIntent RenderIntent::fromJson(const QJsonObject& object) {
    RenderIntent intent;
    intent.defaultExecutor = executorPreferenceFromJson(
      stringField(object, "defaultExecutor", "renderIntent", toString(intent.defaultExecutor)),
      "renderIntent.defaultExecutor");
    intent.defaultViewMode = viewModeFromJson(
      stringField(object, "defaultViewMode", "renderIntent", toString(intent.defaultViewMode)),
      "renderIntent.defaultViewMode");
    intent.defaultShadingProfile = shadingProfileFromJson(object.value("defaultShadingProfile"),
                                                          "renderIntent.defaultShadingProfile");

    const auto defaultCamera = object.value("defaultCamera");
    if (!defaultCamera.isUndefined())
      intent.defaultCamera = cameraRefFromJson(defaultCamera, "renderIntent.defaultCamera");

    intent.enableAutomaticFeatures =
      boolField(object, "enableAutomaticFeatures", "renderIntent", intent.enableAutomaticFeatures);
    intent.enableWireframeOverlay =
      boolField(object, "enableWireframeOverlay", "renderIntent", intent.enableWireframeOverlay);
    intent.enablePreviewShadows =
      boolField(object, "enablePreviewShadows", "renderIntent", intent.enablePreviewShadows);

    const auto overridesValue = object.value("viewOverrides");
    if (!overridesValue.isUndefined()) {
      if (!overridesValue.isArray())
        jsonError("renderIntent.viewOverrides", "expected array");

      const auto overrides = overridesValue.toArray();
      intent.viewOverrides.reserve(static_cast<std::size_t>(overrides.size()));
      for (int i = 0; i < overrides.size(); ++i) {
        if (!overrides.at(i).isObject())
          jsonError("renderIntent.viewOverrides[" + std::to_string(i) + "]", "expected object");
        intent.viewOverrides.push_back(viewOverrideFromJson(
          overrides.at(i).toObject(), "renderIntent.viewOverrides[" + std::to_string(i) + "]"));
      }
    }

    return intent;
  }
}
