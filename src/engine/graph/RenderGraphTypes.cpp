#include "engine/graph/RenderGraphTypes.h"

#include "engine/graph/PostProcessPassState.h"
#include "engine/graph/RenderAOV.h"
#include "engine/graph/RenderExecutor.h"
#include "engine/graph/RenderPassState.h"

#include <QJsonArray>

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <cmath>
#include <initializer_list>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
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

    std::string lowercase(std::string value) {
      std::transform(value.begin(), value.end(), value.begin(),
                     [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
      return value;
    }

    std::string trim(std::string value) {
      const auto first = std::find_if_not(value.begin(), value.end(),
                                          [](unsigned char ch) { return std::isspace(ch); });
      const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
                          return std::isspace(ch);
                        }).base();
      return first < last ? std::string(first, last) : std::string();
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
         {"wavefront", RenderExecutorPreference::Wavefront},
         {"rasterizer", RenderExecutorPreference::Rasterizer},
         {"wireframe", RenderExecutorPreference::Wireframe}},
        path);
    }

    RenderViewMode viewModeFromJson(const std::string& value, const std::string& path) {
      return enumValue<RenderViewMode>(
        value,
        {{"default", RenderViewMode::Default},
         {"beauty", RenderViewMode::Beauty},
         {"wireframe", RenderViewMode::Wireframe},
         {"depth", RenderViewMode::Depth},
         {"stencil", RenderViewMode::Stencil},
         {"stencil_composite", RenderViewMode::StencilComposite},
         {"normal", RenderViewMode::Normal},
         {"object_id", RenderViewMode::ObjectId},
         {"material_id", RenderViewMode::MaterialId},
         {"world_position", RenderViewMode::WorldPosition},
         {"raster_coverage_count", RenderViewMode::RasterCoverageCount},
         {"raster_depth_test_count", RenderViewMode::RasterDepthTestCount},
         {"raster_depth_pass_count", RenderViewMode::RasterDepthPassCount},
         {"raster_shade_count", RenderViewMode::RasterShadeCount},
         {"raster_color_write_count", RenderViewMode::RasterColorWriteCount}},
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
                                            {"visibility_set", RenderResourceType::VisibilitySet},
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
                                        {"readback", RenderPassKind::Readback},
                                        {"visibility", RenderPassKind::Visibility},
                                        {"aov", RenderPassKind::AOV},
                                        {"debug", RenderPassKind::Debug},
                                        {"custom", RenderPassKind::Custom}},
                                       path);
    }

    RenderExecutorKind executorKindFromJson(const std::string& value, const std::string& path) {
      return enumValue<RenderExecutorKind>(value,
                                           {{"raytracer", RenderExecutorKind::Raytracer},
                                            {"wavefront", RenderExecutorKind::Wavefront},
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

    QJsonArray resourceDomainArray(const std::set<RenderResourceDomain>& domains) {
      QJsonArray array;
      for (const auto& domain : domains)
        array.append(toString(domain));
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

    std::set<RenderResourceDomain>
    resourceDomainSetFromJson(const QJsonObject& object, const char* key, const std::string& path) {
      std::set<RenderResourceDomain> result;
      const auto value = object.value(key);
      if (value.isUndefined())
        return {RenderResourceDomain::CPU};
      if (!value.isArray())
        jsonError(path + "." + key, "expected array");

      const auto array = value.toArray();
      for (int i = 0; i < array.size(); ++i) {
        if (!array.at(i).isString())
          jsonError(path + "." + key + "[" + std::to_string(i) + "]", "expected string");
        result.insert(resourceDomainFromJson(array.at(i).toString().toStdString(),
                                             path + "." + key + "[" + std::to_string(i) + "]"));
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

  ShadingProfileParameterValue::ShadingProfileParameterValue()
      : m_value(std::string()) {
  }

  ShadingProfileParameterValue::ShadingProfileParameterValue(bool value)
      : m_value(value) {
  }

  ShadingProfileParameterValue::ShadingProfileParameterValue(double value)
      : m_value(value) {
  }

  ShadingProfileParameterValue::ShadingProfileParameterValue(std::string value)
      : m_value(std::move(value)) {
  }

  const ShadingProfileParameterValue::Value& ShadingProfileParameterValue::value() const {
    return m_value;
  }

  QJsonValue ShadingProfileParameterValue::toJson() const {
    return std::visit(
      [](const auto& value) -> QJsonValue {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, std::string>) {
          return qstr(value);
        } else {
          return value;
        }
      },
      m_value);
  }

  std::string ShadingProfileParameterValue::displayText() const {
    return std::visit(
      [](const auto& value) -> std::string {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, std::string>) {
          return value;
        } else if constexpr (std::is_same_v<T, bool>) {
          return value ? "true" : "false";
        } else {
          std::ostringstream out;
          out << std::setprecision(12) << value;
          return out.str();
        }
      },
      m_value);
  }

  bool ShadingProfileParameterValue::operator==(const ShadingProfileParameterValue& other) const {
    return m_value == other.m_value;
  }

  bool ShadingProfileParameterValue::operator!=(const ShadingProfileParameterValue& other) const {
    return !(*this == other);
  }

  ShadingProfileParameterValue ShadingProfileParameterValue::fromText(std::string text) {
    text = trim(std::move(text));
    const std::string normalized = lowercase(text);
    if (normalized == "true")
      return ShadingProfileParameterValue(true);
    if (normalized == "false")
      return ShadingProfileParameterValue(false);

    char* end = nullptr;
    errno = 0;
    const double number = std::strtod(text.c_str(), &end);
    if (end != text.c_str() && *end == '\0' && errno != ERANGE && std::isfinite(number)) {
      return ShadingProfileParameterValue(number);
    }

    return ShadingProfileParameterValue(std::move(text));
  }

  ShadingProfileParameterValue ShadingProfileParameterValue::fromJson(const QJsonValue& value,
                                                                      std::string path) {
    if (value.isBool())
      return ShadingProfileParameterValue(value.toBool());
    if (value.isDouble())
      return ShadingProfileParameterValue(value.toDouble());
    if (value.isString())
      return ShadingProfileParameterValue(value.toString().toStdString());
    jsonError(path, "expected bool, number, or string");
  }

  QJsonObject ShadingProfileRef::toJson() const {
    QJsonObject result;
    result["name"] = qstr(name);
    if (!parameters.empty()) {
      QJsonObject parameterObject;
      for (const auto& [key, value] : parameters) {
        parameterObject[qstr(key)] = value.toJson();
      }
      result["parameters"] = parameterObject;
    }
    return result;
  }

  bool ShadingProfileRef::isDefault() const {
    return name == "default" && parameters.empty();
  }

  std::string ShadingProfileRef::displayText() const {
    if (parameters.empty()) {
      return name;
    }

    std::ostringstream out;
    out << name << "(";
    bool first = true;
    for (const auto& [key, value] : parameters) {
      if (!first) {
        out << ", ";
      }
      first = false;
      out << key << "=" << value.displayText();
    }
    out << ")";
    return out.str();
  }

  void ShadingProfileRef::setParameter(std::string key, ShadingProfileParameterValue value) {
    parameters.insert_or_assign(std::move(key), std::move(value));
  }

  const ShadingProfileParameterValue* ShadingProfileRef::parameter(const std::string& key) const {
    const auto it = parameters.find(key);
    return it == parameters.end() ? nullptr : &it->second;
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
      const auto parameterObject = parameters.toObject();
      for (auto it = parameterObject.begin(); it != parameterObject.end(); ++it) {
        profile.parameters.emplace(it.key().toStdString(),
                                   ShadingProfileParameterValue::fromJson(
                                     it.value(), path + ".parameters." + it.key().toStdString()));
      }
    }

    return profile;
  }

  bool RenderCameraRef::equivalentTo(const RenderCameraRef& other) const {
    return sceneCameraId == other.sceneCameraId &&
           snapshot.has_value() == other.snapshot.has_value() &&
           (!snapshot || snapshot->parameters == other.snapshot->parameters);
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
    if (inheritEngineOptions)
      result["inheritEngineOptions"] = *inheritEngineOptions;
    if (!engineOptions.empty())
      result["engineOptions"] = engineOptions.toJson();
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

    const auto inheritEngineOptions = object.value("inheritEngineOptions");
    if (!inheritEngineOptions.isUndefined()) {
      if (!inheritEngineOptions.isBool())
        jsonError(path + ".inheritEngineOptions", "expected boolean");
      viewOverride.inheritEngineOptions = inheritEngineOptions.toBool();
    }

    const auto engineOptions = object.value("engineOptions");
    if (!engineOptions.isUndefined()) {
      if (!engineOptions.isObject())
        jsonError(path + ".engineOptions", "expected object");
      viewOverride.engineOptions =
        RenderEngineOptions::fromJson(engineOptions.toObject(), path + ".engineOptions");
    }

    return viewOverride;
  }

  bool RenderViewOverride::appliesToWholeFrame() const {
    return selector.selectsWholeFrame();
  }

  RenderEngineOptions
  RenderSubviewIntent::resolvedEngineOptions(const RenderEngineOptions& globalOptions) const {
    return view.inheritEngineOptions.value_or(true) ? globalOptions.mergedWith(view.engineOptions)
                                                    : view.engineOptions;
  }

  QJsonObject RenderSubviewIntent::toJson() const {
    QJsonObject result;
    result["name"] = qstr(name);
    result["view"] = view.toJson();
    return result;
  }

  RenderSubviewIntent RenderSubviewIntent::fromJson(const QJsonObject& object, std::string path) {
    RenderSubviewIntent subview;
    subview.name = stringField(object, "name", path);

    const auto view = object.value("view");
    if (!view.isUndefined()) {
      if (!view.isObject())
        jsonError(path + ".view", "expected object");
      subview.view = RenderViewOverride::fromJson(view.toObject(), path + ".view");
    }

    const auto inheritEngineOptions = object.value("inheritEngineOptions");
    if (!inheritEngineOptions.isUndefined()) {
      if (!inheritEngineOptions.isBool())
        jsonError(path + ".inheritEngineOptions", "expected boolean");
      subview.view.inheritEngineOptions = inheritEngineOptions.toBool();
    }

    const auto engineOptions = object.value("engineOptions");
    if (!engineOptions.isUndefined()) {
      if (!engineOptions.isObject())
        jsonError(path + ".engineOptions", "expected object");
      subview.view.engineOptions =
        RenderEngineOptions::fromJson(engineOptions.toObject(), path + ".engineOptions");
    }

    return subview;
  }

  const char* toString(RenderExecutorPreference value) {
    return enumName<RenderExecutorPreference>(value,
                                              {{RenderExecutorPreference::Raytracer, "raytracer"},
                                               {RenderExecutorPreference::Wavefront, "wavefront"},
                                               {RenderExecutorPreference::Rasterizer, "rasterizer"},
                                               {RenderExecutorPreference::Wireframe, "wireframe"}});
  }

  const char* toString(RenderViewMode value) {
    return enumName<RenderViewMode>(
      value, {{RenderViewMode::Default, "default"},
              {RenderViewMode::Beauty, "beauty"},
              {RenderViewMode::Wireframe, "wireframe"},
              {RenderViewMode::Depth, "depth"},
              {RenderViewMode::Stencil, "stencil"},
              {RenderViewMode::StencilComposite, "stencil_composite"},
              {RenderViewMode::Normal, "normal"},
              {RenderViewMode::ObjectId, "object_id"},
              {RenderViewMode::MaterialId, "material_id"},
              {RenderViewMode::WorldPosition, "world_position"},
              {RenderViewMode::RasterCoverageCount, "raster_coverage_count"},
              {RenderViewMode::RasterDepthTestCount, "raster_depth_test_count"},
              {RenderViewMode::RasterDepthPassCount, "raster_depth_pass_count"},
              {RenderViewMode::RasterShadeCount, "raster_shade_count"},
              {RenderViewMode::RasterColorWriteCount, "raster_color_write_count"}});
  }

  const char* toString(RenderPostProcessAA value) {
    return enumName<RenderPostProcessAA>(value, {{RenderPostProcessAA::None, "none"},
                                                 {RenderPostProcessAA::FXAA, "fxaa"},
                                                 {RenderPostProcessAA::SMAA, "smaa"},
                                                 {RenderPostProcessAA::TAA, "taa"}});
  }

  const char* toString(RenderExecutorKind value) {
    return enumName<RenderExecutorKind>(value, {{RenderExecutorKind::Raytracer, "raytracer"},
                                                {RenderExecutorKind::Wavefront, "wavefront"},
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
                                            {RenderPassKind::Readback, "readback"},
                                            {RenderPassKind::Visibility, "visibility"},
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
                                         {RenderResourceType::VisibilitySet, "visibility_set"},
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
    if (!engineOptions.empty()) {
      result["engineOptions"] = engineOptions.toJson();
    }
    if (!exportedAOVs.empty()) {
      result["exportedAOVs"] = viewModeArray(exportedAOVs);
    }

    if (!viewOverrides.empty()) {
      QJsonArray overrides;
      for (const auto& viewOverride : viewOverrides)
        overrides.append(viewOverride.toJson());
      result["viewOverrides"] = overrides;
    }

    if (!subviews.empty()) {
      QJsonArray subviewArray;
      for (const auto& subview : subviews)
        subviewArray.append(subview.toJson());
      result["subviews"] = subviewArray;
    }
    if (maxRenderToTextureRecursionDepth != 1) {
      result["maxRenderToTextureRecursionDepth"] = maxRenderToTextureRecursionDepth;
    }

    return result;
  }

  void RenderIntent::requestExportedAOV(RenderViewMode viewMode) {
    if (!renderAOVDefinition(viewMode)) {
      throw std::runtime_error("view mode '" + std::string(toString(viewMode)) +
                               "' is not an exportable graph AOV");
    }
    if (!exportsAOV(viewMode)) {
      exportedAOVs.push_back(viewMode);
    }
  }

  bool RenderIntent::exportsAOV(RenderViewMode viewMode) const {
    return std::find(exportedAOVs.begin(), exportedAOVs.end(), viewMode) != exportedAOVs.end();
  }

  void RenderIntent::setDefaultExecutor(RenderExecutorPreference executor) {
    defaultExecutor = executor;
  }

  void RenderIntent::setDefaultViewMode(RenderViewMode viewMode) {
    defaultViewMode = viewMode;
  }

  void RenderIntent::setDefaultCamera(RenderCameraRef camera) {
    defaultCamera = std::move(camera);
  }

  void RenderIntent::setDefaultShadingProfile(ShadingProfileRef profile) {
    defaultShadingProfile = std::move(profile);
  }

  void RenderIntent::setDefaultShadingProfileParameter(std::string key,
                                                       ShadingProfileParameterValue value) {
    defaultShadingProfile.setParameter(std::move(key), std::move(value));
  }

  void RenderIntent::setPostProcessAA(RenderPostProcessAA aa) {
    postProcessAA = aa;
  }

  void RenderIntent::setMaxRenderToTextureRecursionDepth(int depth) {
    maxRenderToTextureRecursionDepth = std::max(0, depth);
  }

  void RenderIntent::setAutomaticFeaturesEnabled(bool enabled) {
    enableAutomaticFeatures = enabled;
  }

  void RenderIntent::setWireframeOverlayEnabled(bool enabled) {
    enableWireframeOverlay = enabled;
  }

  void RenderIntent::setPreviewShadowsEnabled(bool enabled) {
    enablePreviewShadows = enabled;
  }

  void RenderIntent::applyWholeFrameOverride(const RenderViewOverride& viewOverride) {
    if (!viewOverride.appliesToWholeFrame()) {
      return;
    }
    if (viewOverride.executor) {
      setDefaultExecutor(*viewOverride.executor);
    }
    if (viewOverride.viewMode) {
      setDefaultViewMode(*viewOverride.viewMode);
    }
    if (viewOverride.shadingProfile) {
      setDefaultShadingProfile(*viewOverride.shadingProfile);
    }
    if (viewOverride.camera) {
      setDefaultCamera(*viewOverride.camera);
    }
    if (!viewOverride.engineOptions.empty()) {
      engineOptions = viewOverride.inheritEngineOptions.value_or(true)
                        ? engineOptions.mergedWith(viewOverride.engineOptions)
                        : viewOverride.engineOptions;
    }
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

    const auto engineOptions = object.value("engineOptions");
    if (!engineOptions.isUndefined()) {
      if (!engineOptions.isObject())
        jsonError("renderIntent.engineOptions", "expected object");
      intent.engineOptions =
        RenderEngineOptions::fromJson(engineOptions.toObject(), "renderIntent.engineOptions");
    }

    for (RenderViewMode viewMode : viewModeArrayFromJson(object, "exportedAOVs", "renderIntent")) {
      try {
        intent.requestExportedAOV(viewMode);
      } catch (const std::runtime_error& error) {
        jsonError("renderIntent.exportedAOVs", error.what());
      }
    }

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

    const auto subviewsValue = object.value("subviews");
    if (!subviewsValue.isUndefined()) {
      if (!subviewsValue.isArray())
        jsonError("renderIntent.subviews", "expected array");

      const auto subviews = subviewsValue.toArray();
      intent.subviews.reserve(static_cast<std::size_t>(subviews.size()));
      for (int i = 0; i < subviews.size(); ++i) {
        if (!subviews.at(i).isObject())
          jsonError("renderIntent.subviews[" + std::to_string(i) + "]", "expected object");
        intent.subviews.push_back(RenderSubviewIntent::fromJson(
          subviews.at(i).toObject(), "renderIntent.subviews[" + std::to_string(i) + "]"));
      }
    }

    if (!object.value("maxRenderToTextureRecursionDepth").isUndefined()) {
      const int depth = intField(object, "maxRenderToTextureRecursionDepth", "renderIntent", 1);
      if (depth < 0) {
        jsonError("renderIntent.maxRenderToTextureRecursionDepth", "expected non-negative integer");
      }
      intent.setMaxRenderToTextureRecursionDepth(depth);
    }

    return intent;
  }

  RenderIntent RenderIntent::withWholeFrameOverridesApplied() const {
    RenderIntent result = *this;
    result.viewOverrides = selectorSpecificOverrides();

    for (const auto& viewOverride : viewOverrides) {
      result.applyWholeFrameOverride(viewOverride);
    }

    return result;
  }

  std::vector<RenderViewOverride> RenderIntent::selectorSpecificOverrides() const {
    std::vector<RenderViewOverride> result;
    result.reserve(viewOverrides.size());
    for (const auto& viewOverride : viewOverrides) {
      if (!viewOverride.appliesToWholeFrame()) {
        result.push_back(viewOverride);
      }
    }
    return result;
  }

  bool RenderIntent::hasSelectorSpecificOverrides() const {
    return std::any_of(
      viewOverrides.begin(), viewOverrides.end(),
      [](const RenderViewOverride& viewOverride) { return !viewOverride.appliesToWholeFrame(); });
  }

  void RenderIntent::requireWholeFrameOnly(const std::string& context) const {
    const auto overrides = selectorSpecificOverrides();
    if (overrides.empty()) {
      return;
    }

    std::ostringstream message;
    message << context << " does not support selector-specific render intent yet";
    message << " (" << overrides.front().selector.displayText();
    if (overrides.size() > 1) {
      message << ", +" << (overrides.size() - 1) << " more";
    }
    message << ")";
    throw std::runtime_error(message.str());
  }

  void RenderIntent::requireNoSubviews(const std::string& context) const {
    if (subviews.empty()) {
      return;
    }

    const std::string firstSubview =
      subviews.front().name.empty() ? "unnamed subview" : subviews.front().name;
    std::ostringstream message;
    message << context << " does not support render-to-texture subviews yet";
    message << " (" << firstSubview;
    if (subviews.size() > 1) {
      message << ", +" << (subviews.size() - 1) << " more";
    }
    message << ")";
    throw std::runtime_error(message.str());
  }

  SceneView RenderIntent::defaultSceneView() const {
    SceneView view;
    view.selector = SceneSelector::all();
    view.camera = defaultCamera;
    if (!defaultShadingProfile.isDefault()) {
      view.shadingProfile = defaultShadingProfile;
    }
    return view;
  }

  RenderExecutorKind RenderIntent::defaultExecutorKind() const {
    if (defaultViewMode == RenderViewMode::Wireframe) {
      return RenderExecutorKind::Wireframe;
    }
    if (defaultViewMode == RenderViewMode::StencilComposite) {
      return RenderExecutorKind::Rasterizer;
    }

    return renderExecutorDefinition(defaultExecutor).kind();
  }

  bool RenderIntent::usesGraphImagePostProcessAA() const {
    return postProcessAADefinition(postProcessAA) != nullptr;
  }

  int RenderIntent::targetSampleCountHint(int fallback) const {
    std::optional<int> hint;
    if (defaultExecutorKind() == RenderExecutorKind::Rasterizer) {
      hint = engineOptions.rasterizer().msaaSamples();
    } else if (defaultExecutorKind() == RenderExecutorKind::Raytracer ||
               defaultExecutorKind() == RenderExecutorKind::Wavefront) {
      hint = engineOptions.raytracer().samplesPerPixel();
    }
    return std::max(1, hint.value_or(fallback));
  }

  void RenderResourceDescriptor::addFeature(RenderFeatureKind feature) {
    if (!hasFeature(feature)) {
      features.push_back(std::move(feature));
    }
  }

  bool RenderResourceDescriptor::hasFeature(const RenderFeatureKind& feature) const {
    return std::find(features.begin(), features.end(), feature) != features.end();
  }

  bool RenderResourceDescriptor::hasAllFeatures(
    const std::set<RenderFeatureKind>& requestedFeatures) const {
    return std::all_of(requestedFeatures.begin(), requestedFeatures.end(),
                       [&](const RenderFeatureKind& feature) { return hasFeature(feature); });
  }

  bool RenderResourceDescriptor::hasImageShape() const {
    return width > 0 && height > 0;
  }

  bool RenderResourceDescriptor::externallyAvailable() const {
    return lifetime == RenderResourceLifetime::Imported ||
           lifetime == RenderResourceLifetime::History ||
           lifetime == RenderResourceLifetime::PersistentCache;
  }

  bool RenderResourceDescriptor::requiresExternalBinding() const {
    return lifetime == RenderResourceLifetime::Imported ||
           lifetime == RenderResourceLifetime::History;
  }

  QJsonObject RenderResourceDescriptor::toJson() const {
    QJsonObject object;
    object["id"] = qstr(id);
    object["name"] = qstr(name);
    if (!features.empty()) {
      object["features"] = stringArray(features);
    }
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
    resource.features = featureArrayFromJson(object, "features", path);
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

  bool RenderPassNode::hasFeature(const RenderFeatureKind& feature) const {
    return std::find(features.begin(), features.end(), feature) != features.end();
  }

  bool RenderPassNode::hasAnyFeature(const std::set<RenderFeatureKind>& requestedFeatures) const {
    return std::any_of(features.begin(), features.end(), [&](const RenderFeatureKind& feature) {
      return requestedFeatures.find(feature) != requestedFeatures.end();
    });
  }

  bool RenderPassNode::hasAllFeatures(const std::set<RenderFeatureKind>& requestedFeatures) const {
    return std::all_of(requestedFeatures.begin(), requestedFeatures.end(),
                       [&](const RenderFeatureKind& feature) { return hasFeature(feature); });
  }

  void RenderPassNode::addRead(RenderResourceId resource) {
    if (!readsResource(resource)) {
      reads.push_back({std::move(resource)});
    }
  }

  void RenderPassNode::addWrite(RenderResourceId resource) {
    if (!writesResource(resource)) {
      writes.push_back({std::move(resource)});
    }
  }

  bool RenderPassNode::readsResource(const RenderResourceId& resource) const {
    return std::any_of(reads.begin(), reads.end(),
                       [&](const ResourceRead& read) { return read.resource == resource; });
  }

  bool RenderPassNode::writesResource(const RenderResourceId& resource) const {
    return std::any_of(writes.begin(), writes.end(),
                       [&](const ResourceWrite& write) { return write.resource == resource; });
  }

  bool RenderPassNode::supportsResourceDomain(RenderResourceDomain domain) const {
    return supportedResourceDomains.find(domain) != supportedResourceDomains.end();
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
    if (supportedResourceDomains.size() != 1 ||
        supportedResourceDomains.find(RenderResourceDomain::CPU) ==
          supportedResourceDomains.end()) {
      object["supportedResourceDomains"] = resourceDomainArray(supportedResourceDomains);
    }
    object["sceneSelector"] = sceneView.selector.toJson();
    if (sceneView.camera) {
      object["sceneCamera"] = sceneView.camera->toJson();
    }
    if (sceneView.shadingProfile) {
      object["sceneShadingProfile"] = sceneView.shadingProfile->toJson();
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
    pass.supportedResourceDomains =
      resourceDomainSetFromJson(object, "supportedResourceDomains", path);

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

    const auto shadingProfile = object.value("sceneShadingProfile");
    if (!shadingProfile.isUndefined()) {
      pass.sceneView.shadingProfile =
        ShadingProfileRef::fromJson(shadingProfile, path + ".sceneShadingProfile");
    }

    pass.disabledBehavior = disabledBehaviorFromJson(
      stringField(object, "disabledBehavior", path, "error"), path + ".disabledBehavior");
    pass.enabled = boolField(object, "enabled", path, true);
    pass.hasExternalSideEffects = boolField(object, "hasExternalSideEffects", path, false);
    pass.canRunConcurrently = boolField(object, "canRunConcurrently", path, true);
    return pass;
  }
}
