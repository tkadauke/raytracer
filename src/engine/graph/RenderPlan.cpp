#include "engine/graph/RenderPlan.h"

#include "engine/graph/RenderPassState.h"

#include <QJsonArray>
#include <QJsonDocument>

#include <algorithm>
#include <functional>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace engine::graph {
  namespace {
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

    bool sameResourceShape(const RenderResourceDescriptor& a, const RenderResourceDescriptor& b) {
      return a.type == b.type && a.format == b.format && a.width == b.width &&
             a.height == b.height && a.sampleCount == b.sampleCount && a.domain == b.domain;
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
    case RenderPlanValidationError::Code::InvalidPassIO:
      return "invalid_pass_io";
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

  const RenderPassNode* RenderPlan::findPass(const RenderPassId& id) const {
    const auto it = std::find_if(m_passes.begin(), m_passes.end(),
                                 [&](const RenderPassNode& pass) { return pass.id == id; });
    return it == m_passes.end() ? nullptr : &*it;
  }

  const RenderResourceDescriptor* RenderPlan::findResource(const RenderResourceId& id) const {
    const auto it =
      std::find_if(m_resources.begin(), m_resources.end(),
                   [&](const RenderResourceDescriptor& resource) { return resource.id == id; });
    return it == m_resources.end() ? nullptr : &*it;
  }

  const RenderPassNode* RenderPlan::producerOf(const RenderResourceId& resource) const {
    const auto it = std::find_if(m_passes.begin(), m_passes.end(), [&](const RenderPassNode& pass) {
      return pass.writesResource(resource);
    });
    return it == m_passes.end() ? nullptr : &*it;
  }

  std::vector<const RenderPassNode*>
  RenderPlan::consumersOf(const RenderResourceId& resource) const {
    std::vector<const RenderPassNode*> result;
    for (const auto& pass : m_passes) {
      if (pass.readsResource(resource)) {
        result.push_back(&pass);
      }
    }
    return result;
  }

  void RenderPlan::addResource(RenderResourceDescriptor descriptor) {
    m_resources.push_back(std::move(descriptor));
  }

  void RenderPlan::addPass(RenderPassNode pass) {
    m_passes.push_back(std::move(pass));
  }

  std::size_t RenderPlan::routeResourceThroughPass(const RenderResourceId& sourceResource,
                                                   RenderResourceDescriptor routedResource,
                                                   RenderPassNode pass) {
    if (!pass.readsResource(sourceResource)) {
      throw std::runtime_error("inserted pass '" + pass.id + "' must read resource '" +
                               sourceResource + "'");
    }
    if (!pass.writesResource(routedResource.id)) {
      throw std::runtime_error("inserted pass '" + pass.id + "' must write resource '" +
                               routedResource.id + "'");
    }

    auto writer = std::find_if(m_passes.begin(), m_passes.end(), [&](const RenderPassNode& node) {
      return node.writesResource(sourceResource);
    });
    if (writer == m_passes.end()) {
      throw std::runtime_error("cannot route resource '" + sourceResource +
                               "' because no pass writes it");
    }

    m_resources.push_back(std::move(routedResource));
    const auto inserted = m_passes.insert(std::next(writer), std::move(pass));

    std::size_t redirected = 0;
    for (auto node = std::next(inserted); node != m_passes.end(); ++node) {
      for (auto& read : node->reads) {
        if (read.resource == sourceResource) {
          read.resource = m_resources.back().id;
          ++redirected;
        }
      }
    }
    return redirected;
  }

  std::size_t RenderPlan::setPassState(RenderPassKind kind, RenderExecutorKind executor,
                                       std::shared_ptr<const RenderPassState> state) {
    std::size_t updated = 0;
    for (auto& pass : m_passes) {
      if (pass.kind == kind && pass.executor == executor) {
        pass.state = state;
        ++updated;
      }
    }
    return updated;
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

    for (const auto& pass : m_passes) {
      if (pass.enabled || pass.disabledBehavior != DisabledBehavior::Passthrough) {
        continue;
      }

      if (pass.reads.size() != 1 || pass.writes.empty()) {
        result.add({RenderPlanValidationError::Code::InvalidPassIO,
                    "disabled passthrough pass '" + pass.id +
                      "' requires exactly one input and at least one output",
                    pass.id, ""});
        continue;
      }

      const auto readResource = resources.find(pass.reads.front().resource);
      if (readResource == resources.end()) {
        continue;
      }

      for (const auto& write : pass.writes) {
        const auto writeResource = resources.find(write.resource);
        if (writeResource == resources.end()) {
          continue;
        }
        if (!sameResourceShape(*readResource->second, *writeResource->second)) {
          result.add({RenderPlanValidationError::Code::InvalidResourceShape,
                      "disabled passthrough pass '" + pass.id +
                        "' requires matching input and output resource shapes",
                      pass.id, write.resource});
        }
      }
    }

    std::map<RenderPassId, std::set<RenderPassId>> dependencies;
    for (const auto& pass : m_passes) {
      if (!pass.enabled && pass.disabledBehavior != DisabledBehavior::Passthrough) {
        continue;
      }

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
          if (!resourceIt->second->externallyAvailable()) {
            result.add({RenderPlanValidationError::Code::MissingProducer,
                        "resource '" + read.resource + "' is read but has no producer", pass.id,
                        read.resource});
          }
          continue;
        }

        const RenderPassNode* producer = producerIt->second;
        if (!producer->enabled && !producer->producesWhenDisabled()) {
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
      if (pass.state) {
        const QJsonObject serializedState = pass.state->toJson();
        if (serializedState.isEmpty())
          continue;
        out << "  parameters: "
            << QJsonDocument(serializedState).toJson(QJsonDocument::Compact).toStdString() << "\n";
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
      resourceArray.append(resource.toJson());
    }

    QJsonArray passArray;
    for (const auto& pass : m_passes) {
      passArray.append(pass.toJson());
    }

    QJsonObject result;
    result["resources"] = resourceArray;
    result["passes"] = passArray;
    return result;
  }

  const RenderResourceDescriptor& RenderPlan::exportedColorResource() const {
    for (const auto& resource : m_resources) {
      if (resource.lifetime == RenderResourceLifetime::Exported &&
          resource.type == RenderResourceType::Color) {
        return resource;
      }
    }
    throw std::runtime_error("render plan has no exported color resource");
  }

  RenderPlan RenderPlan::fromJson(const QJsonObject& object) {
    RenderPlan result;

    const auto resourceArray = arrayField(object, "resources", "$");
    for (int i = 0; i < resourceArray.size(); ++i) {
      const std::string path = "$.resources[" + std::to_string(i) + "]";
      if (!resourceArray.at(i).isObject())
        jsonError(path, "expected object");

      result.addResource(RenderResourceDescriptor::fromJson(resourceArray.at(i).toObject(), path));
    }

    const auto passArray = arrayField(object, "passes", "$");
    for (int i = 0; i < passArray.size(); ++i) {
      const std::string path = "$.passes[" + std::to_string(i) + "]";
      if (!passArray.at(i).isObject())
        jsonError(path, "expected object");

      result.addPass(RenderPassNode::fromJson(passArray.at(i).toObject(), path));
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

    std::set<RenderPassId> culledPasses;
    for (const auto& pass : result.m_passes) {
      if (!pass.enabled && pass.disabledBehavior == DisabledBehavior::CullDependents) {
        culledPasses.insert(pass.id);
      }
    }

    bool changed = true;
    while (changed) {
      changed = false;
      for (auto& pass : result.m_passes) {
        if (!pass.enabled) {
          continue;
        }

        const bool readsCulledProducer =
          std::any_of(pass.reads.begin(), pass.reads.end(), [&](const ResourceRead& read) {
            const RenderPassNode* producer = result.producerOf(read.resource);
            return producer && contains(culledPasses, producer->id);
          });
        if (readsCulledProducer) {
          pass.enabled = false;
          pass.disabledBehavior = DisabledBehavior::CullDependents;
          culledPasses.insert(pass.id);
          changed = true;
        }
      }
    }

    return result;
  }
}
