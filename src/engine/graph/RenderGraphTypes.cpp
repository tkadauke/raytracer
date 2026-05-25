#include "engine/graph/RenderGraphTypes.h"

#include "engine/graph/PostProcessPassState.h"
#include "engine/graph/RenderExecutor.h"
#include "engine/graph/RenderPassState.h"

#include <QJsonArray>

#include <algorithm>
#include <cstddef>
#include <cmath>
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
      throw std::runtime_error("Invalid render graph JSON at " + path + ": " + message);
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

    int intField(const QJsonObject& object, const char* key, const std::string& path,
                 int fallback) {
      const auto value = object.value(key);
      if (value.isUndefined())
        return fallback;
      if (!value.isDouble())
        jsonError(path + "." + key, "expected integer");

      const double number = value.toDouble();
      if (!std::isfinite(number) || std::floor(number) != number)
        jsonError(path + "." + key, "expected integer");
      return static_cast<int>(number);
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

    template<class T>
    const char* enumName(T value, std::initializer_list<std::pair<T, const char*>> values,
                         const char* fallback = "unknown") {
      for (const auto& [parsed, name] : values) {
        if (value == parsed)
          return name;
      }
      return fallback;
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
                                        {"object_id", RenderViewMode::ObjectId},
                                        {"material_id", RenderViewMode::MaterialId},
                                        {"world_position", RenderViewMode::WorldPosition}},
                                       path);
    }

    RenderPostProcessAA postProcessAAFromJson(const std::string& value, const std::string& path) {
      return enumValue<RenderPostProcessAA>(value,
                                            {{"none", RenderPostProcessAA::None},
                                             {"fxaa", RenderPostProcessAA::FXAA},
                                             {"smaa", RenderPostProcessAA::SMAA},
                                             {"taa", RenderPostProcessAA::TAA}},
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

    RenderResourceType resourceTypeFromJson(const std::string& value, const std::string& path) {
      return enumValue<RenderResourceType>(value,
                                           {{"color", RenderResourceType::Color},
                                            {"depth", RenderResourceType::Depth},
                                            {"stencil", RenderResourceType::Stencil},
                                            {"object_id", RenderResourceType::ObjectId},
                                            {"material_id", RenderResourceType::MaterialId},
                                            {"normal", RenderResourceType::Normal},
                                            {"world_position", RenderResourceType::WorldPosition},
                                            {"motion_vector", RenderResourceType::MotionVector},
                                            {"shadow_map", RenderResourceType::ShadowMap},
                                            {"shadow_mask", RenderResourceType::ShadowMask},
                                            {"custom_texture", RenderResourceType::CustomTexture}},
                                           path);
    }

    RenderResourceFormat resourceFormatFromJson(const std::string& value, const std::string& path) {
      return enumValue<RenderResourceFormat>(
        value,
        {{"unknown", RenderResourceFormat::Unknown},
         {"rgb_double", RenderResourceFormat::RGBDouble},
         {"depth_double", RenderResourceFormat::DepthDouble},
         {"uint8", RenderResourceFormat::UInt8},
         {"uint32", RenderResourceFormat::UInt32},
         {"scalar_double", RenderResourceFormat::ScalarDouble}},
        path);
    }

    RenderResourceDomain resourceDomainFromJson(const std::string& value, const std::string& path) {
      return enumValue<RenderResourceDomain>(
        value, {{"cpu", RenderResourceDomain::CPU}, {"gpu", RenderResourceDomain::GPU}}, path);
    }

    RenderResourceLifetime resourceLifetimeFromJson(const std::string& value,
                                                    const std::string& path) {
      return enumValue<RenderResourceLifetime>(
        value,
        {{"transient", RenderResourceLifetime::Transient},
         {"imported", RenderResourceLifetime::Imported},
         {"exported", RenderResourceLifetime::Exported},
         {"history", RenderResourceLifetime::History},
         {"persistent_cache", RenderResourceLifetime::PersistentCache}},
        path);
    }

    RenderPassKind passKindFromJson(const std::string& value, const std::string& path) {
      return enumValue<RenderPassKind>(value,
                                       {{"beauty", RenderPassKind::Beauty},
                                        {"shadow", RenderPassKind::Shadow},
                                        {"overlay", RenderPassKind::Overlay},
                                        {"composite", RenderPassKind::Composite},
                                        {"tonemap", RenderPassKind::Tonemap},
                                        {"postprocess", RenderPassKind::PostProcess},
                                        {"aov", RenderPassKind::AOV},
                                        {"debug", RenderPassKind::Debug},
                                        {"custom", RenderPassKind::Custom}},
                                       path);
    }

    RenderExecutorKind executorKindFromJson(const std::string& value, const std::string& path) {
      return enumValue<RenderExecutorKind>(value,
                                           {{"raytracer", RenderExecutorKind::Raytracer},
                                            {"rasterizer", RenderExecutorKind::Rasterizer},
                                            {"wireframe", RenderExecutorKind::Wireframe},
                                            {"composite", RenderExecutorKind::Composite},
                                            {"postprocess", RenderExecutorKind::PostProcess}},
                                           path);
    }

    DisabledBehavior disabledBehaviorFromJson(const std::string& value, const std::string& path) {
      return enumValue<DisabledBehavior>(
        value,
        {{"error", DisabledBehavior::Error},
         {"cull_dependents", DisabledBehavior::CullDependents},
         {"substitute_default", DisabledBehavior::SubstituteDefault},
         {"passthrough", DisabledBehavior::Passthrough}},
        path);
    }

    QJsonArray stringArray(const std::vector<RenderFeatureKind>& values) {
      QJsonArray array;
      for (const auto& value : values)
        array.append(qstr(value));
      return array;
    }

    QJsonArray viewModeArray(const std::vector<RenderViewMode>& values) {
      QJsonArray array;
      for (const auto& value : values)
        array.append(toString(value));
      return array;
    }

    QJsonArray readArray(const std::vector<ResourceRead>& reads) {
      QJsonArray array;
      for (const auto& read : reads)
        array.append(qstr(read.resource));
      return array;
    }

    QJsonArray writeArray(const std::vector<ResourceWrite>& writes) {
      QJsonArray array;
      for (const auto& write : writes)
        array.append(qstr(write.resource));
      return array;
    }

    std::vector<RenderFeatureKind> featureArrayFromJson(const QJsonObject& object, const char* key,
                                                        const std::string& path) {
      std::vector<RenderFeatureKind> result;
      const auto value = object.value(key);
      if (value.isUndefined())
        return result;
      if (!value.isArray())
        jsonError(path + "." + key, "expected array");

      const auto array = value.toArray();
      result.reserve(static_cast<std::size_t>(array.size()));
      for (int i = 0; i < array.size(); ++i) {
        if (!array.at(i).isString())
          jsonError(path + "." + key + "[" + std::to_string(i) + "]", "expected string");
        result.push_back(array.at(i).toString().toStdString());
      }
      return result;
    }

    std::vector<RenderViewMode> viewModeArrayFromJson(const QJsonObject& object, const char* key,
                                                      const std::string& path) {
      std::vector<RenderViewMode> result;
      const auto value = object.value(key);
      if (value.isUndefined())
        return result;
      if (!value.isArray())
        jsonError(path + "." + key, "expected array");

      const auto array = value.toArray();
      result.reserve(static_cast<std::size_t>(array.size()));
      for (int i = 0; i < array.size(); ++i) {
        if (!array.at(i).isString())
          jsonError(path + "." + key + "[" + std::to_string(i) + "]", "expected string");
        result.push_back(viewModeFromJson(array.at(i).toString().toStdString(),
                                          path + "." + key + "[" + std::to_string(i) + "]"));
      }
      return result;
    }

    std::vector<ResourceRead> readsFromJson(const QJsonObject& object, const char* key,
                                            const std::string& path) {
      std::vector<ResourceRead> result;
      const auto value = object.value(key);
      if (value.isUndefined())
        return result;
      if (!value.isArray())
        jsonError(path + "." + key, "expected array");

      const auto array = value.toArray();
      result.reserve(static_cast<std::size_t>(array.size()));
      for (int i = 0; i < array.size(); ++i) {
        if (!array.at(i).isString())
          jsonError(path + "." + key + "[" + std::to_string(i) + "]", "expected string");
        result.push_back({array.at(i).toString().toStdString()});
      }
      return result;
    }

    std::vector<ResourceWrite> writesFromJson(const QJsonObject& object, const char* key,
                                              const std::string& path) {
      std::vector<ResourceWrite> result;
      const auto value = object.value(key);
      if (value.isUndefined())
        return result;
      if (!value.isArray())
        jsonError(path + "." + key, "expected array");

      const auto array = value.toArray();
      result.reserve(static_cast<std::size_t>(array.size()));
      for (int i = 0; i < array.size(); ++i) {
        if (!array.at(i).isString())
          jsonError(path + "." + key + "[" + std::to_string(i) + "]", "expected string");
        result.push_back({array.at(i).toString().toStdString()});
      }
      return result;
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

  bool SceneSelector::selectsWholeFrame() const {
    return kind == Kind::All;
  }

  std::string SceneSelector::displayText() const {
    std::string result = toString(kind);
    if (!value.empty()) {
      result += ": " + value;
    }
    return result;
  }

  QJsonObject SceneSelector::toJson() const {
    QJsonObject result;
    result["kind"] = toString(kind);
    if (!value.empty())
      result["value"] = qstr(value);
    return result;
  }

  SceneSelector SceneSelector::fromJson(const QJsonObject& object, std::string path) {
    return {selectorKindFromJson(stringField(object, "kind", path, "all"), path + ".kind"),
            stringField(object, "value", path)};
  }

  QJsonObject ShadingProfileRef::toJson() const {
    QJsonObject result;
    result["name"] = qstr(name);
    if (!parameters.isEmpty())
      result["parameters"] = parameters;
    return result;
  }

  ShadingProfileRef ShadingProfileRef::fromJson(const QJsonValue& value, std::string path) {
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

  std::string RenderCameraRef::displayText() const {
    std::string result;
    if (sceneCameraId) {
      result += *sceneCameraId;
    }
    if (snapshot) {
      if (!result.empty()) {
        result += ", ";
      }
      result += "snapshot";
    }
    return result.empty() ? "-" : result;
  }

  QJsonObject RenderCameraRef::toJson() const {
    QJsonObject result;
    if (sceneCameraId)
      result["sceneCameraId"] = qstr(*sceneCameraId);
    if (snapshot)
      result["snapshot"] = snapshot->parameters;
    return result;
  }

  RenderCameraRef RenderCameraRef::fromJson(const QJsonValue& value, std::string path) {
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

  QJsonObject RenderViewOverride::toJson() const {
    QJsonObject result;
    result["selector"] = selector.toJson();
    if (executor)
      result["executor"] = toString(*executor);
    if (viewMode)
      result["viewMode"] = toString(*viewMode);
    if (shadingProfile)
      result["shadingProfile"] = shadingProfile->toJson();
    if (camera)
      result["camera"] = camera->toJson();
    return result;
  }

  RenderViewOverride RenderViewOverride::fromJson(const QJsonObject& object, std::string path) {
    RenderViewOverride viewOverride;
    const auto selector = object.value("selector");
    if (!selector.isUndefined()) {
      if (!selector.isObject())
        jsonError(path + ".selector", "expected object");
      viewOverride.selector = SceneSelector::fromJson(selector.toObject(), path + ".selector");
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
        ShadingProfileRef::fromJson(shadingProfile, path + ".shadingProfile");

    const auto camera = object.value("camera");
    if (!camera.isUndefined())
      viewOverride.camera = RenderCameraRef::fromJson(camera, path + ".camera");

    return viewOverride;
  }

  bool RenderViewOverride::appliesToWholeFrame() const {
    return selector.selectsWholeFrame();
  }

  const char* toString(RenderExecutorPreference value) {
    return enumName<RenderExecutorPreference>(value,
                                              {{RenderExecutorPreference::Raytracer, "raytracer"},
                                               {RenderExecutorPreference::Rasterizer, "rasterizer"},
                                               {RenderExecutorPreference::Wireframe, "wireframe"}});
  }

  const char* toString(RenderViewMode value) {
    return enumName<RenderViewMode>(value, {{RenderViewMode::Default, "default"},
                                            {RenderViewMode::Beauty, "beauty"},
                                            {RenderViewMode::Wireframe, "wireframe"},
                                            {RenderViewMode::Depth, "depth"},
                                            {RenderViewMode::Normal, "normal"},
                                            {RenderViewMode::ObjectId, "object_id"},
                                            {RenderViewMode::MaterialId, "material_id"},
                                            {RenderViewMode::WorldPosition, "world_position"}});
  }

  const char* toString(RenderPostProcessAA value) {
    return enumName<RenderPostProcessAA>(value, {{RenderPostProcessAA::None, "none"},
                                                 {RenderPostProcessAA::FXAA, "fxaa"},
                                                 {RenderPostProcessAA::SMAA, "smaa"},
                                                 {RenderPostProcessAA::TAA, "taa"}});
  }

  const char* toString(RenderExecutorKind value) {
    return enumName<RenderExecutorKind>(value, {{RenderExecutorKind::Raytracer, "raytracer"},
                                                {RenderExecutorKind::Rasterizer, "rasterizer"},
                                                {RenderExecutorKind::Wireframe, "wireframe"},
                                                {RenderExecutorKind::Composite, "composite"},
                                                {RenderExecutorKind::PostProcess, "postprocess"}});
  }

  const char* toString(RenderPassKind value) {
    return enumName<RenderPassKind>(value, {{RenderPassKind::Beauty, "beauty"},
                                            {RenderPassKind::Shadow, "shadow"},
                                            {RenderPassKind::Overlay, "overlay"},
                                            {RenderPassKind::Composite, "composite"},
                                            {RenderPassKind::Tonemap, "tonemap"},
                                            {RenderPassKind::PostProcess, "postprocess"},
                                            {RenderPassKind::AOV, "aov"},
                                            {RenderPassKind::Debug, "debug"},
                                            {RenderPassKind::Custom, "custom"}});
  }

  const char* toString(DisabledBehavior value) {
    return enumName<DisabledBehavior>(value,
                                      {{DisabledBehavior::Error, "error"},
                                       {DisabledBehavior::CullDependents, "cull_dependents"},
                                       {DisabledBehavior::SubstituteDefault, "substitute_default"},
                                       {DisabledBehavior::Passthrough, "passthrough"}});
  }

  const char* toString(RenderResourceType value) {
    return enumName<RenderResourceType>(value,
                                        {{RenderResourceType::Color, "color"},
                                         {RenderResourceType::Depth, "depth"},
                                         {RenderResourceType::Stencil, "stencil"},
                                         {RenderResourceType::ObjectId, "object_id"},
                                         {RenderResourceType::MaterialId, "material_id"},
                                         {RenderResourceType::Normal, "normal"},
                                         {RenderResourceType::WorldPosition, "world_position"},
                                         {RenderResourceType::MotionVector, "motion_vector"},
                                         {RenderResourceType::ShadowMap, "shadow_map"},
                                         {RenderResourceType::ShadowMask, "shadow_mask"},
                                         {RenderResourceType::CustomTexture, "custom_texture"}});
  }

  const char* toString(RenderResourceDomain value) {
    return enumName<RenderResourceDomain>(
      value, {{RenderResourceDomain::CPU, "cpu"}, {RenderResourceDomain::GPU, "gpu"}});
  }

  const char* toString(RenderResourceLifetime value) {
    return enumName<RenderResourceLifetime>(
      value, {{RenderResourceLifetime::Transient, "transient"},
              {RenderResourceLifetime::Imported, "imported"},
              {RenderResourceLifetime::Exported, "exported"},
              {RenderResourceLifetime::History, "history"},
              {RenderResourceLifetime::PersistentCache, "persistent_cache"}});
  }

  const char* toString(RenderResourceFormat value) {
    return enumName<RenderResourceFormat>(value,
                                          {{RenderResourceFormat::Unknown, "unknown"},
                                           {RenderResourceFormat::RGBDouble, "rgb_double"},
                                           {RenderResourceFormat::DepthDouble, "depth_double"},
                                           {RenderResourceFormat::UInt8, "uint8"},
                                           {RenderResourceFormat::UInt32, "uint32"},
                                           {RenderResourceFormat::ScalarDouble, "scalar_double"}});
  }

  const char* toString(SceneSelector::Kind value) {
    return enumName<SceneSelector::Kind>(value,
                                         {{SceneSelector::Kind::All, "all"},
                                          {SceneSelector::Kind::ObjectId, "object_id"},
                                          {SceneSelector::Kind::ObjectName, "object_name"},
                                          {SceneSelector::Kind::Tag, "tag"},
                                          {SceneSelector::Kind::Layer, "layer"},
                                          {SceneSelector::Kind::MaterialRole, "material_role"}});
  }

  QJsonObject RenderIntent::toJson() const {
    QJsonObject result;
    result["defaultExecutor"] = toString(defaultExecutor);
    result["defaultViewMode"] = toString(defaultViewMode);
    result["defaultShadingProfile"] = defaultShadingProfile.toJson();
    if (defaultCamera)
      result["defaultCamera"] = defaultCamera->toJson();
    result["enableAutomaticFeatures"] = enableAutomaticFeatures;
    result["enableWireframeOverlay"] = enableWireframeOverlay;
    result["enableCurveOverlay"] = enableCurveOverlay;
    result["enablePreviewShadows"] = enablePreviewShadows;
    result["postProcessAA"] = toString(postProcessAA);
    if (!exportedAOVs.empty()) {
      result["exportedAOVs"] = viewModeArray(exportedAOVs);
    }

    if (!viewOverrides.empty()) {
      QJsonArray overrides;
      for (const auto& viewOverride : viewOverrides)
        overrides.append(viewOverride.toJson());
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
    intent.defaultShadingProfile = ShadingProfileRef::fromJson(
      object.value("defaultShadingProfile"), "renderIntent.defaultShadingProfile");

    const auto defaultCamera = object.value("defaultCamera");
    if (!defaultCamera.isUndefined())
      intent.defaultCamera = RenderCameraRef::fromJson(defaultCamera, "renderIntent.defaultCamera");

    intent.enableAutomaticFeatures =
      boolField(object, "enableAutomaticFeatures", "renderIntent", intent.enableAutomaticFeatures);
    intent.enableWireframeOverlay =
      boolField(object, "enableWireframeOverlay", "renderIntent", intent.enableWireframeOverlay);
    intent.enableCurveOverlay =
      boolField(object, "enableCurveOverlay", "renderIntent", intent.enableCurveOverlay);
    intent.enablePreviewShadows =
      boolField(object, "enablePreviewShadows", "renderIntent", intent.enablePreviewShadows);
    intent.postProcessAA = postProcessAAFromJson(
      stringField(object, "postProcessAA", "renderIntent", toString(intent.postProcessAA)),
      "renderIntent.postProcessAA");
    intent.exportedAOVs = viewModeArrayFromJson(object, "exportedAOVs", "renderIntent");

    const auto overridesValue = object.value("viewOverrides");
    if (!overridesValue.isUndefined()) {
      if (!overridesValue.isArray())
        jsonError("renderIntent.viewOverrides", "expected array");

      const auto overrides = overridesValue.toArray();
      intent.viewOverrides.reserve(static_cast<std::size_t>(overrides.size()));
      for (int i = 0; i < overrides.size(); ++i) {
        if (!overrides.at(i).isObject())
          jsonError("renderIntent.viewOverrides[" + std::to_string(i) + "]", "expected object");
        intent.viewOverrides.push_back(RenderViewOverride::fromJson(
          overrides.at(i).toObject(), "renderIntent.viewOverrides[" + std::to_string(i) + "]"));
      }
    }

    return intent;
  }

  RenderIntent RenderIntent::withWholeFrameOverridesApplied() const {
    RenderIntent result = *this;
    result.viewOverrides.clear();
    result.viewOverrides.reserve(viewOverrides.size());

    for (const auto& viewOverride : viewOverrides) {
      if (!viewOverride.appliesToWholeFrame()) {
        result.viewOverrides.push_back(viewOverride);
        continue;
      }

      if (viewOverride.executor) {
        result.defaultExecutor = *viewOverride.executor;
      }
      if (viewOverride.viewMode) {
        result.defaultViewMode = *viewOverride.viewMode;
      }
      if (viewOverride.shadingProfile) {
        result.defaultShadingProfile = *viewOverride.shadingProfile;
      }
      if (viewOverride.camera) {
        result.defaultCamera = *viewOverride.camera;
      }
    }

    return result;
  }

  SceneView RenderIntent::defaultSceneView() const {
    SceneView view;
    view.selector = SceneSelector::all();
    view.camera = defaultCamera;
    return view;
  }

  RenderExecutorKind RenderIntent::defaultExecutorKind() const {
    if (defaultViewMode == RenderViewMode::Wireframe) {
      return RenderExecutorKind::Wireframe;
    }

    return renderExecutorDefinition(defaultExecutor).kind();
  }

  bool RenderIntent::usesGraphImagePostProcessAA() const {
    return postProcessAADefinition(postProcessAA) != nullptr;
  }

  bool RenderResourceDescriptor::hasImageShape() const {
    return width > 0 && height > 0;
  }

  bool RenderResourceDescriptor::externallyAvailable() const {
    return lifetime == RenderResourceLifetime::Imported ||
           lifetime == RenderResourceLifetime::History ||
           lifetime == RenderResourceLifetime::PersistentCache;
  }

  QJsonObject RenderResourceDescriptor::toJson() const {
    QJsonObject object;
    object["id"] = qstr(id);
    object["name"] = qstr(name);
    object["type"] = toString(type);
    object["format"] = toString(format);
    object["width"] = width;
    object["height"] = height;
    object["sampleCount"] = sampleCount;
    object["domain"] = toString(domain);
    object["lifetime"] = toString(lifetime);
    return object;
  }

  RenderResourceDescriptor RenderResourceDescriptor::fromJson(const QJsonObject& object,
                                                              std::string path) {
    RenderResourceDescriptor resource;
    resource.id = stringField(object, "id", path);
    resource.name = stringField(object, "name", path);
    resource.type =
      resourceTypeFromJson(stringField(object, "type", path, "color"), path + ".type");
    resource.format =
      resourceFormatFromJson(stringField(object, "format", path, "unknown"), path + ".format");
    resource.width = intField(object, "width", path, 0);
    resource.height = intField(object, "height", path, 0);
    resource.sampleCount = intField(object, "sampleCount", path, 1);
    resource.domain =
      resourceDomainFromJson(stringField(object, "domain", path, "cpu"), path + ".domain");
    resource.lifetime = resourceLifetimeFromJson(stringField(object, "lifetime", path, "transient"),
                                                 path + ".lifetime");
    return resource;
  }

  bool RenderPassNode::readsResource(const RenderResourceId& resource) const {
    return std::any_of(reads.begin(), reads.end(),
                       [&](const ResourceRead& read) { return read.resource == resource; });
  }

  bool RenderPassNode::writesResource(const RenderResourceId& resource) const {
    return std::any_of(writes.begin(), writes.end(),
                       [&](const ResourceWrite& write) { return write.resource == resource; });
  }

  bool RenderPassNode::producesWhenDisabled() const {
    return disabledBehavior == DisabledBehavior::SubstituteDefault ||
           disabledBehavior == DisabledBehavior::Passthrough;
  }

  const ResourceRead& RenderPassNode::singleRead() const {
    if (reads.size() != 1) {
      throw std::runtime_error("pass '" + id + "': requires exactly one input resource");
    }
    return reads.front();
  }

  const ResourceWrite& RenderPassNode::singleWrite() const {
    if (writes.size() != 1) {
      throw std::runtime_error("pass '" + id + "': requires exactly one output resource");
    }
    return writes.front();
  }

  QJsonObject RenderPassNode::toJson() const {
    QJsonObject object;
    object["id"] = qstr(id);
    object["name"] = qstr(name);
    object["kind"] = toString(kind);
    object["executor"] = toString(executor);
    object["features"] = stringArray(features);
    object["reads"] = readArray(reads);
    object["writes"] = writeArray(writes);
    object["sceneSelector"] = sceneView.selector.toJson();
    if (sceneView.camera) {
      object["sceneCamera"] = sceneView.camera->toJson();
    }
    if (state) {
      const QJsonObject serializedState = state->toJson();
      if (!serializedState.isEmpty()) {
        object["parameters"] = serializedState;
      }
    }
    object["disabledBehavior"] = toString(disabledBehavior);
    object["enabled"] = enabled;
    object["hasExternalSideEffects"] = hasExternalSideEffects;
    object["canRunConcurrently"] = canRunConcurrently;
    return object;
  }

  RenderPassNode RenderPassNode::fromJson(const QJsonObject& object, std::string path) {
    RenderPassNode pass;
    pass.id = stringField(object, "id", path);
    pass.name = stringField(object, "name", path);
    pass.kind = passKindFromJson(stringField(object, "kind", path, "custom"), path + ".kind");
    pass.executor = executorKindFromJson(stringField(object, "executor", path, "postprocess"),
                                         path + ".executor");
    pass.features = featureArrayFromJson(object, "features", path);
    pass.reads = readsFromJson(object, "reads", path);
    pass.writes = writesFromJson(object, "writes", path);

    const auto parameters = object.value("parameters");
    if (!parameters.isUndefined()) {
      if (!parameters.isObject())
        jsonError(path + ".parameters", "expected object");
      pass.state = RenderPassState::fromJson(pass.kind, pass.executor, parameters.toObject(),
                                             path + ".parameters");
    }

    const auto selector = object.value("sceneSelector");
    if (!selector.isUndefined()) {
      if (!selector.isObject())
        jsonError(path + ".sceneSelector", "expected object");
      pass.sceneView.selector =
        SceneSelector::fromJson(selector.toObject(), path + ".sceneSelector");
    } else {
      pass.sceneView.selector = SceneSelector::all();
    }

    const auto camera = object.value("sceneCamera");
    if (!camera.isUndefined()) {
      pass.sceneView.camera = RenderCameraRef::fromJson(camera, path + ".sceneCamera");
    }

    pass.disabledBehavior = disabledBehaviorFromJson(
      stringField(object, "disabledBehavior", path, "error"), path + ".disabledBehavior");
    pass.enabled = boolField(object, "enabled", path, true);
    pass.hasExternalSideEffects = boolField(object, "hasExternalSideEffects", path, false);
    pass.canRunConcurrently = boolField(object, "canRunConcurrently", path, true);
    return pass;
  }
}
