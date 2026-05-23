#include "engine/graph/RenderPlan.h"

#include <QJsonArray>

#include <algorithm>
#include <cmath>
#include <functional>
#include <initializer_list>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace engine::graph {
  namespace {
    QString qstr(const std::string& value) {
      return QString::fromStdString(value);
    }

    std::string dotEscape(const std::string& value) {
      std::string result;
      result.reserve(value.size());
      for (char c : value) {
        if (c == '"' || c == '\\')
          result.push_back('\\');
        result.push_back(c);
      }
      return result;
    }

    template<class T>
    bool contains(const std::set<T>& values, const T& value) {
      return values.find(value) != values.end();
    }

    bool hasFeature(const RenderPassNode& pass, const std::set<RenderFeatureKind>& features) {
      return std::any_of(
        pass.features.begin(), pass.features.end(),
        [&](const RenderFeatureKind& feature) { return contains(features, feature); });
    }

    bool isExternallyAvailable(RenderResourceLifetime lifetime) {
      return lifetime == RenderResourceLifetime::Imported ||
             lifetime == RenderResourceLifetime::History ||
             lifetime == RenderResourceLifetime::PersistentCache;
    }

    bool passProducesWhenDisabled(const RenderPassNode& pass) {
      return pass.disabledBehavior == DisabledBehavior::SubstituteDefault ||
             pass.disabledBehavior == DisabledBehavior::Passthrough;
    }

    QJsonArray stringArray(const std::vector<RenderFeatureKind>& values) {
      QJsonArray array;
      for (const auto& value : values)
        array.append(qstr(value));
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

    QJsonObject selectorJson(const SceneSelector& selector) {
      QJsonObject result;
      result["kind"] = toString(selector.kind);
      if (!selector.value.empty())
        result["value"] = qstr(selector.value);
      return result;
    }

    [[noreturn]] void jsonError(const std::string& path, const std::string& message) {
      throw std::runtime_error("Invalid render plan JSON at " + path + ": " + message);
    }

    QJsonArray arrayField(const QJsonObject& object, const char* key, const std::string& path) {
      const auto value = object.value(key);
      if (!value.isArray())
        jsonError(path + "." + key, "expected array");
      return value.toArray();
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

    SceneSelector selectorFromJson(const QJsonObject& object, const std::string& path) {
      const auto selector = object.value("sceneSelector");
      if (selector.isUndefined())
        return SceneSelector::all();
      if (!selector.isObject())
        jsonError(path + ".sceneSelector", "expected object");

      const auto selectorObject = selector.toObject();
      return {
        selectorKindFromJson(stringField(selectorObject, "kind", path + ".sceneSelector", "all"),
                             path + ".sceneSelector.kind"),
        stringField(selectorObject, "value", path + ".sceneSelector")};
    }
  }

  const char* toString(RenderPlanValidationError::Code value) {
    switch (value) {
    case RenderPlanValidationError::Code::EmptyPassId:
      return "empty_pass_id";
    case RenderPlanValidationError::Code::DuplicatePassId:
      return "duplicate_pass_id";
    case RenderPlanValidationError::Code::EmptyResourceId:
      return "empty_resource_id";
    case RenderPlanValidationError::Code::DuplicateResourceId:
      return "duplicate_resource_id";
    case RenderPlanValidationError::Code::UnknownResource:
      return "unknown_resource";
    case RenderPlanValidationError::Code::DuplicateWriter:
      return "duplicate_writer";
    case RenderPlanValidationError::Code::MissingProducer:
      return "missing_producer";
    case RenderPlanValidationError::Code::DisabledDependency:
      return "disabled_dependency";
    case RenderPlanValidationError::Code::DisabledRequiredPass:
      return "disabled_required_pass";
    case RenderPlanValidationError::Code::InvalidResourceShape:
      return "invalid_resource_shape";
    case RenderPlanValidationError::Code::Cycle:
      return "cycle";
    }
    return "unknown";
  }

  bool RenderPlanValidation::valid() const {
    return m_errors.empty();
  }

  const std::vector<RenderPlanValidationError>& RenderPlanValidation::errors() const {
    return m_errors;
  }

  void RenderPlanValidation::add(RenderPlanValidationError error) {
    m_errors.push_back(std::move(error));
  }

  const std::vector<RenderPassNode>& RenderPlan::passes() const {
    return m_passes;
  }

  const std::vector<RenderResourceDescriptor>& RenderPlan::resources() const {
    return m_resources;
  }

  void RenderPlan::addResource(RenderResourceDescriptor descriptor) {
    m_resources.push_back(std::move(descriptor));
  }

  void RenderPlan::addPass(RenderPassNode pass) {
    m_passes.push_back(std::move(pass));
  }

  RenderPlanValidation RenderPlan::validate() const {
    RenderPlanValidation result;
    std::map<RenderResourceId, const RenderResourceDescriptor*> resources;
    std::map<RenderPassId, const RenderPassNode*> passes;
    std::map<RenderResourceId, const RenderPassNode*> producers;

    for (const auto& resource : m_resources) {
      if (resource.id.empty()) {
        result.add({RenderPlanValidationError::Code::EmptyResourceId,
                    "resource id must not be empty", "", ""});
        continue;
      }

      if (!resources.emplace(resource.id, &resource).second) {
        result.add({RenderPlanValidationError::Code::DuplicateResourceId,
                    "duplicate resource id '" + resource.id + "'", "", resource.id});
      }

      if (resource.width < 0 || resource.height < 0 || resource.sampleCount <= 0) {
        result.add({RenderPlanValidationError::Code::InvalidResourceShape,
                    "resource '" + resource.id + "' has invalid dimensions or sample count", "",
                    resource.id});
      }
    }

    for (const auto& pass : m_passes) {
      if (pass.id.empty()) {
        result.add(
          {RenderPlanValidationError::Code::EmptyPassId, "pass id must not be empty", "", ""});
        continue;
      }

      if (!passes.emplace(pass.id, &pass).second) {
        result.add({RenderPlanValidationError::Code::DuplicatePassId,
                    "duplicate pass id '" + pass.id + "'", pass.id, ""});
      }

      if (!pass.enabled && pass.disabledBehavior == DisabledBehavior::Error) {
        result.add({RenderPlanValidationError::Code::DisabledRequiredPass,
                    "required pass '" + pass.id + "' is disabled", pass.id, ""});
      }

      for (const auto& write : pass.writes) {
        if (resources.find(write.resource) == resources.end()) {
          result.add({RenderPlanValidationError::Code::UnknownResource,
                      "pass '" + pass.id + "' writes unknown resource '" + write.resource + "'",
                      pass.id, write.resource});
          continue;
        }

        const auto inserted = producers.emplace(write.resource, &pass);
        if (!inserted.second) {
          result.add({RenderPlanValidationError::Code::DuplicateWriter,
                      "resource '" + write.resource + "' has multiple writers", pass.id,
                      write.resource});
        }
      }
    }

    std::map<RenderPassId, std::set<RenderPassId>> dependencies;
    for (const auto& pass : m_passes) {
      for (const auto& read : pass.reads) {
        const auto resourceIt = resources.find(read.resource);
        if (resourceIt == resources.end()) {
          result.add({RenderPlanValidationError::Code::UnknownResource,
                      "pass '" + pass.id + "' reads unknown resource '" + read.resource + "'",
                      pass.id, read.resource});
          continue;
        }

        const auto producerIt = producers.find(read.resource);
        if (producerIt == producers.end()) {
          if (!isExternallyAvailable(resourceIt->second->lifetime)) {
            result.add({RenderPlanValidationError::Code::MissingProducer,
                        "resource '" + read.resource + "' is read but has no producer", pass.id,
                        read.resource});
          }
          continue;
        }

        const RenderPassNode* producer = producerIt->second;
        if (!producer->enabled && !passProducesWhenDisabled(*producer)) {
          result.add({RenderPlanValidationError::Code::DisabledDependency,
                      "pass '" + pass.id + "' reads resource '" + read.resource +
                        "' from disabled pass '" + producer->id + "'",
                      pass.id, read.resource});
          continue;
        }

        if (producer->id != pass.id && producer->enabled && pass.enabled) {
          dependencies[producer->id].insert(pass.id);
        } else if (producer->id == pass.id) {
          result.add({RenderPlanValidationError::Code::Cycle,
                      "pass '" + pass.id + "' reads and writes resource '" + read.resource + "'",
                      pass.id, read.resource});
        }
      }
    }

    std::set<RenderPassId> visiting;
    std::set<RenderPassId> visited;
    std::function<bool(const RenderPassId&)> visit = [&](const RenderPassId& passId) {
      if (visiting.find(passId) != visiting.end())
        return true;
      if (visited.find(passId) != visited.end())
        return false;

      visiting.insert(passId);
      const auto deps = dependencies.find(passId);
      if (deps != dependencies.end()) {
        for (const auto& dependent : deps->second) {
          if (visit(dependent))
            return true;
        }
      }
      visiting.erase(passId);
      visited.insert(passId);
      return false;
    };

    for (const auto& [passId, pass] : passes) {
      (void)pass;
      if (visit(passId)) {
        result.add({RenderPlanValidationError::Code::Cycle,
                    "render plan contains a dependency cycle", passId, ""});
        break;
      }
    }

    return result;
  }

  std::string RenderPlan::toText() const {
    std::ostringstream out;
    out << "RenderPlan\n";
    out << "Resources:\n";
    for (const auto& resource : m_resources) {
      out << "- " << resource.id << " (" << toString(resource.type) << ", "
          << toString(resource.format) << ", " << toString(resource.domain) << ", "
          << toString(resource.lifetime) << ", " << resource.width << "x" << resource.height
          << ", samples=" << resource.sampleCount << ")\n";
    }

    out << "Passes:\n";
    for (const auto& pass : m_passes) {
      out << "- " << pass.id << " [" << toString(pass.kind) << "/" << toString(pass.executor)
          << "] " << (pass.enabled ? "enabled" : "disabled") << "\n";
      if (!pass.reads.empty()) {
        out << "  reads:";
        for (const auto& read : pass.reads)
          out << " " << read.resource;
        out << "\n";
      }
      if (!pass.writes.empty()) {
        out << "  writes:";
        for (const auto& write : pass.writes)
          out << " " << write.resource;
        out << "\n";
      }
    }
    return out.str();
  }

  std::string RenderPlan::toDot() const {
    std::ostringstream out;
    out << "digraph RenderPlan {\n";
    out << "  rankdir=LR;\n";

    for (const auto& resource : m_resources) {
      out << "  \"resource:" << dotEscape(resource.id) << "\""
          << " [shape=box,label=\"" << dotEscape(resource.id) << "\\n"
          << toString(resource.type) << "\"];\n";
    }

    for (const auto& pass : m_passes) {
      out << "  \"pass:" << dotEscape(pass.id) << "\""
          << " [shape=ellipse,label=\"" << dotEscape(pass.name.empty() ? pass.id : pass.name)
          << "\\n"
          << toString(pass.kind) << "/" << toString(pass.executor) << "\"];\n";

      for (const auto& read : pass.reads) {
        out << "  \"resource:" << dotEscape(read.resource) << "\" -> "
            << "\"pass:" << dotEscape(pass.id) << "\";\n";
      }
      for (const auto& write : pass.writes) {
        out << "  \"pass:" << dotEscape(pass.id) << "\" -> "
            << "\"resource:" << dotEscape(write.resource) << "\";\n";
      }
    }

    out << "}\n";
    return out.str();
  }

  QJsonObject RenderPlan::toJson() const {
    QJsonArray resourceArray;
    for (const auto& resource : m_resources) {
      QJsonObject object;
      object["id"] = qstr(resource.id);
      object["name"] = qstr(resource.name);
      object["type"] = toString(resource.type);
      object["format"] = toString(resource.format);
      object["width"] = resource.width;
      object["height"] = resource.height;
      object["sampleCount"] = resource.sampleCount;
      object["domain"] = toString(resource.domain);
      object["lifetime"] = toString(resource.lifetime);
      resourceArray.append(object);
    }

    QJsonArray passArray;
    for (const auto& pass : m_passes) {
      QJsonObject object;
      object["id"] = qstr(pass.id);
      object["name"] = qstr(pass.name);
      object["kind"] = toString(pass.kind);
      object["executor"] = toString(pass.executor);
      object["features"] = stringArray(pass.features);
      object["reads"] = readArray(pass.reads);
      object["writes"] = writeArray(pass.writes);
      object["sceneSelector"] = selectorJson(pass.sceneView.selector);
      object["disabledBehavior"] = toString(pass.disabledBehavior);
      object["enabled"] = pass.enabled;
      object["hasExternalSideEffects"] = pass.hasExternalSideEffects;
      object["canRunConcurrently"] = pass.canRunConcurrently;
      passArray.append(object);
    }

    QJsonObject result;
    result["resources"] = resourceArray;
    result["passes"] = passArray;
    return result;
  }

  RenderPlan RenderPlan::fromJson(const QJsonObject& object) {
    RenderPlan result;

    const auto resourceArray = arrayField(object, "resources", "$");
    for (int i = 0; i < resourceArray.size(); ++i) {
      const std::string path = "$.resources[" + std::to_string(i) + "]";
      if (!resourceArray.at(i).isObject())
        jsonError(path, "expected object");

      const auto resourceObject = resourceArray.at(i).toObject();
      RenderResourceDescriptor resource;
      resource.id = stringField(resourceObject, "id", path);
      resource.name = stringField(resourceObject, "name", path);
      resource.type =
        resourceTypeFromJson(stringField(resourceObject, "type", path, "color"), path + ".type");
      resource.format = resourceFormatFromJson(
        stringField(resourceObject, "format", path, "unknown"), path + ".format");
      resource.width = intField(resourceObject, "width", path, 0);
      resource.height = intField(resourceObject, "height", path, 0);
      resource.sampleCount = intField(resourceObject, "sampleCount", path, 1);
      resource.domain = resourceDomainFromJson(stringField(resourceObject, "domain", path, "cpu"),
                                               path + ".domain");
      resource.lifetime = resourceLifetimeFromJson(
        stringField(resourceObject, "lifetime", path, "transient"), path + ".lifetime");
      result.addResource(std::move(resource));
    }

    const auto passArray = arrayField(object, "passes", "$");
    for (int i = 0; i < passArray.size(); ++i) {
      const std::string path = "$.passes[" + std::to_string(i) + "]";
      if (!passArray.at(i).isObject())
        jsonError(path, "expected object");

      const auto passObject = passArray.at(i).toObject();
      RenderPassNode pass;
      pass.id = stringField(passObject, "id", path);
      pass.name = stringField(passObject, "name", path);
      pass.kind = passKindFromJson(stringField(passObject, "kind", path, "custom"), path + ".kind");
      pass.executor = executorKindFromJson(stringField(passObject, "executor", path, "postprocess"),
                                           path + ".executor");
      pass.features = featureArrayFromJson(passObject, "features", path);
      pass.reads = readsFromJson(passObject, "reads", path);
      pass.writes = writesFromJson(passObject, "writes", path);
      pass.sceneView.selector = selectorFromJson(passObject, path);
      pass.disabledBehavior = disabledBehaviorFromJson(
        stringField(passObject, "disabledBehavior", path, "error"), path + ".disabledBehavior");
      pass.enabled = boolField(passObject, "enabled", path, true);
      pass.hasExternalSideEffects = boolField(passObject, "hasExternalSideEffects", path, false);
      pass.canRunConcurrently = boolField(passObject, "canRunConcurrently", path, true);
      result.addPass(std::move(pass));
    }

    return result;
  }

  RenderPlan RenderPlan::withOverrides(const RenderGraphOverrides& overrides) const {
    RenderPlan result = *this;
    for (auto& pass : result.m_passes) {
      if (contains(overrides.disabledPasses, pass.id) ||
          contains(overrides.disabledPassKinds, pass.kind) ||
          contains(overrides.disabledExecutors, pass.executor) ||
          hasFeature(pass, overrides.disabledFeatures)) {
        pass.enabled = false;
      }
    }
    return result;
  }
}
