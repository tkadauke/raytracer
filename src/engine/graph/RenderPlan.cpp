#include "engine/graph/RenderPlan.h"

#include "engine/graph/RenderPassState.h"

#include <QJsonArray>
#include <QJsonDocument>

#include <algorithm>
#include <functional>
#include <initializer_list>
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

    template<class T>
    const char* enumName(T value, std::initializer_list<std::pair<T, const char*>> values,
                         const char* fallback = "unknown") {
      for (const auto& [parsed, name] : values) {
        if (value == parsed)
          return name;
      }
      return fallback;
    }

    bool passReadsWhenExecuted(const RenderPassNode& pass) {
      return pass.enabled || pass.disabledBehavior == DisabledBehavior::Passthrough;
    }

    bool passProducesWhenExecuted(const RenderPassNode& pass) {
      return pass.enabled || pass.producesWhenDisabled();
    }

    bool sameResourceShape(const RenderResourceDescriptor& a, const RenderResourceDescriptor& b) {
      return a.type == b.type && a.format == b.format && a.width == b.width &&
             a.height == b.height && a.sampleCount == b.sampleCount && a.domain == b.domain;
    }

    bool sameResourceDescriptor(const RenderResourceDescriptor& a,
                                const RenderResourceDescriptor& b) {
      return a.id == b.id && a.features == b.features && a.type == b.type && a.format == b.format &&
             a.width == b.width && a.height == b.height && a.sampleCount == b.sampleCount &&
             a.domain == b.domain && a.lifetime == b.lifetime;
    }

    bool sameReads(const std::vector<ResourceRead>& a, const std::vector<ResourceRead>& b) {
      return a.size() == b.size() &&
             std::equal(a.begin(), a.end(), b.begin(), [](const auto& left, const auto& right) {
               return left.resource == right.resource;
             });
    }

    bool sameWrites(const std::vector<ResourceWrite>& a, const std::vector<ResourceWrite>& b) {
      return a.size() == b.size() &&
             std::equal(a.begin(), a.end(), b.begin(), [](const auto& left, const auto& right) {
               return left.resource == right.resource;
             });
    }

    bool sameCameraRef(const std::optional<RenderCameraRef>& a,
                       const std::optional<RenderCameraRef>& b) {
      if (a.has_value() != b.has_value()) {
        return false;
      }
      if (!a) {
        return true;
      }
      return a->sceneCameraId == b->sceneCameraId &&
             a->snapshot.has_value() == b->snapshot.has_value() &&
             (!a->snapshot || a->snapshot->parameters == b->snapshot->parameters);
    }

    bool sameShadingProfile(const std::optional<ShadingProfileRef>& a,
                            const std::optional<ShadingProfileRef>& b) {
      if (a.has_value() != b.has_value()) {
        return false;
      }
      if (!a) {
        return true;
      }
      return a->name == b->name && a->parameters == b->parameters;
    }

    bool sameSceneView(const SceneView& a, const SceneView& b) {
      return a.selector.kind == b.selector.kind && a.selector.value == b.selector.value &&
             sameCameraRef(a.camera, b.camera) &&
             sameShadingProfile(a.shadingProfile, b.shadingProfile);
    }

    bool samePassState(const std::shared_ptr<const RenderPassState>& a,
                       const std::shared_ptr<const RenderPassState>& b) {
      if (a.get() == b.get()) {
        return true;
      }
      if (!a || !b) {
        return false;
      }
      return a->toJson() == b->toJson();
    }

    bool samePassNode(const RenderPassNode& a, const RenderPassNode& b) {
      return a.id == b.id && a.kind == b.kind && a.executor == b.executor &&
             a.features == b.features && sameReads(a.reads, b.reads) &&
             sameWrites(a.writes, b.writes) &&
             a.supportedResourceDomains == b.supportedResourceDomains &&
             sameSceneView(a.sceneView, b.sceneView) && samePassState(a.state, b.state) &&
             a.disabledBehavior == b.disabledBehavior && a.enabled == b.enabled &&
             a.hasExternalSideEffects == b.hasExternalSideEffects &&
             a.canRunConcurrently == b.canRunConcurrently;
    }

    bool hasNonDefaultSceneView(const SceneView& sceneView) {
      return !sceneView.selector.selectsWholeFrame() || sceneView.camera.has_value() ||
             sceneView.shadingProfile.has_value();
    }

    bool hasNonDefaultResourceDomains(const RenderPassNode& pass) {
      return pass.supportedResourceDomains.size() != 1 ||
             !contains(pass.supportedResourceDomains, RenderResourceDomain::CPU);
    }

    std::string resourceDomainList(const std::set<RenderResourceDomain>& domains,
                                   const char* separator) {
      std::ostringstream out;
      bool first = true;
      for (const auto& domain : domains) {
        if (!first) {
          out << separator;
        }
        out << toString(domain);
        first = false;
      }
      return out.str();
    }

    std::string featureList(const std::vector<RenderFeatureKind>& features, const char* separator) {
      std::ostringstream out;
      bool first = true;
      for (const auto& feature : features) {
        if (!first) {
          out << separator;
        }
        out << feature;
        first = false;
      }
      return out.str();
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

    QJsonArray passIdArray(const std::vector<const RenderPassNode*>& passes) {
      QJsonArray result;
      for (const RenderPassNode* pass : passes) {
        result.append(QString::fromStdString(pass->id));
      }
      return result;
    }

    std::vector<std::vector<std::size_t>>
    executionStageIndexes(const std::vector<RenderPassNode>& passes,
                          const std::vector<RenderPassDependency>& dependencies) {
      std::map<const RenderPassNode*, std::size_t> passIndexes;
      for (std::size_t passIndex = 0; passIndex != passes.size(); ++passIndex) {
        passIndexes.emplace(&passes[passIndex], passIndex);
      }

      std::vector<std::set<std::size_t>> dependents(passes.size());
      std::vector<std::size_t> dependencyCounts(passes.size(), 0);
      for (const RenderPassDependency& dependency : dependencies) {
        if (!passReadsWhenExecuted(*dependency.consumer) ||
            !passProducesWhenExecuted(*dependency.producer)) {
          continue;
        }

        const auto producerIt = passIndexes.find(dependency.producer);
        const auto consumerIt = passIndexes.find(dependency.consumer);
        if (producerIt == passIndexes.end() || consumerIt == passIndexes.end()) {
          continue;
        }

        const std::size_t producerIndex = producerIt->second;
        const std::size_t consumerIndex = consumerIt->second;
        if (dependents[producerIndex].insert(consumerIndex).second) {
          ++dependencyCounts[consumerIndex];
        }
      }

      std::vector<std::size_t> ready;
      ready.reserve(passes.size());
      for (std::size_t passIndex = 0; passIndex != passes.size(); ++passIndex) {
        if (dependencyCounts[passIndex] == 0) {
          ready.push_back(passIndex);
        }
      }

      std::vector<std::vector<std::size_t>> stages;
      std::vector<bool> emitted(passes.size(), false);
      while (!ready.empty()) {
        std::sort(ready.begin(), ready.end());
        const std::vector<std::size_t> stage = ready;
        ready.clear();

        stages.push_back(stage);
        for (const std::size_t passIndex : stage) {
          emitted[passIndex] = true;
          for (const std::size_t dependent : dependents[passIndex]) {
            --dependencyCounts[dependent];
            if (dependencyCounts[dependent] == 0) {
              ready.push_back(dependent);
            }
          }
        }
      }

      std::vector<std::size_t> remaining;
      for (std::size_t passIndex = 0; passIndex != passes.size(); ++passIndex) {
        if (!emitted[passIndex]) {
          remaining.push_back(passIndex);
        }
      }
      if (!remaining.empty()) {
        stages.push_back(std::move(remaining));
      }

      return stages;
    }
  }

  const char* toString(RenderPlanValidationError::Code value) {
    return enumName<RenderPlanValidationError::Code>(
      value, {{RenderPlanValidationError::Code::EmptyPassId, "empty_pass_id"},
              {RenderPlanValidationError::Code::DuplicatePassId, "duplicate_pass_id"},
              {RenderPlanValidationError::Code::EmptyResourceId, "empty_resource_id"},
              {RenderPlanValidationError::Code::DuplicateResourceId, "duplicate_resource_id"},
              {RenderPlanValidationError::Code::UnknownResource, "unknown_resource"},
              {RenderPlanValidationError::Code::DuplicateWriter, "duplicate_writer"},
              {RenderPlanValidationError::Code::MissingProducer, "missing_producer"},
              {RenderPlanValidationError::Code::UnproducedExport, "unproduced_export"},
              {RenderPlanValidationError::Code::DisabledDependency, "disabled_dependency"},
              {RenderPlanValidationError::Code::DisabledRequiredPass, "disabled_required_pass"},
              {RenderPlanValidationError::Code::InvalidPassIO, "invalid_pass_io"},
              {RenderPlanValidationError::Code::InvalidResourceShape, "invalid_resource_shape"},
              {RenderPlanValidationError::Code::ResourceDomainMismatch, "resource_domain_mismatch"},
              {RenderPlanValidationError::Code::Cycle, "cycle"}});
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

  bool RenderPlan::hasPass(const RenderPassId& id) const {
    return findPass(id) != nullptr;
  }

  bool RenderPlan::hasResource(const RenderResourceId& id) const {
    return findResource(id) != nullptr;
  }

  std::set<RenderPassId> RenderPlan::passIds() const {
    std::set<RenderPassId> result;
    for (const auto& pass : m_passes) {
      result.insert(pass.id);
    }
    return result;
  }

  std::set<RenderResourceId> RenderPlan::resourceIds() const {
    std::set<RenderResourceId> result;
    for (const auto& resource : m_resources) {
      result.insert(resource.id);
    }
    return result;
  }

  std::set<RenderPassKind> RenderPlan::passKinds() const {
    std::set<RenderPassKind> result;
    for (const auto& pass : m_passes) {
      result.insert(pass.kind);
    }
    return result;
  }

  std::set<RenderExecutorKind> RenderPlan::passExecutors() const {
    std::set<RenderExecutorKind> result;
    for (const auto& pass : m_passes) {
      result.insert(pass.executor);
    }
    return result;
  }

  std::set<RenderFeatureKind> RenderPlan::passFeatures() const {
    std::set<RenderFeatureKind> result;
    for (const auto& pass : m_passes) {
      for (const auto& feature : pass.features) {
        result.insert(feature);
      }
    }
    return result;
  }

  std::vector<const RenderPassNode*>
  RenderPlan::passesWithFeature(const RenderFeatureKind& feature) const {
    std::vector<const RenderPassNode*> result;
    for (const auto& pass : m_passes) {
      if (pass.hasFeature(feature)) {
        result.push_back(&pass);
      }
    }
    return result;
  }

  std::vector<const RenderResourceDescriptor*>
  RenderPlan::resourcesWithFeature(const RenderFeatureKind& feature) const {
    std::vector<const RenderResourceDescriptor*> result;
    for (const auto& resource : m_resources) {
      if (resource.hasFeature(feature)) {
        result.push_back(&resource);
      }
    }
    return result;
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

  std::vector<RenderResourceId> RenderPlan::externalInputResourceIds() const {
    std::map<RenderResourceId, const RenderResourceDescriptor*> resources;
    for (const auto& resource : m_resources) {
      resources.emplace(resource.id, &resource);
    }

    std::set<RenderResourceId> producedResources;
    for (const auto& pass : m_passes) {
      for (const auto& write : pass.writes) {
        producedResources.insert(write.resource);
      }
    }

    std::set<RenderResourceId> result;
    for (const auto& pass : m_passes) {
      if (!passReadsWhenExecuted(pass)) {
        continue;
      }
      for (const auto& read : pass.reads) {
        const auto resourceIt = resources.find(read.resource);
        if (resourceIt == resources.end()) {
          continue;
        }
        if (resourceIt->second->requiresExternalBinding() &&
            producedResources.find(read.resource) == producedResources.end()) {
          result.insert(read.resource);
        }
      }
    }

    return {result.begin(), result.end()};
  }

  bool RenderPlan::resourceCanReach(const RenderResourceId& source,
                                    const RenderResourceId& destination) const {
    std::vector<RenderResourceId> pending{source};
    std::set<RenderResourceId> visitedResources;
    std::set<RenderPassId> visitedPasses;

    while (!pending.empty()) {
      const RenderResourceId current = pending.back();
      pending.pop_back();
      if (current == destination) {
        return true;
      }
      if (!visitedResources.insert(current).second) {
        continue;
      }

      for (const RenderPassNode* consumer : consumersOf(current)) {
        if (!consumer || !visitedPasses.insert(consumer->id).second) {
          continue;
        }
        for (const auto& write : consumer->writes) {
          pending.push_back(write.resource);
        }
      }
    }

    return false;
  }

  std::vector<RenderPassDependency> RenderPlan::dependencies() const {
    std::map<RenderResourceId, const RenderPassNode*> producerByResource;
    for (const auto& pass : m_passes) {
      for (const auto& write : pass.writes) {
        producerByResource.emplace(write.resource, &pass);
      }
    }

    std::vector<RenderPassDependency> result;
    for (const auto& consumer : m_passes) {
      for (const auto& read : consumer.reads) {
        const auto producerIt = producerByResource.find(read.resource);
        if (producerIt == producerByResource.end()) {
          continue;
        }

        const RenderPassNode* producer = producerIt->second;
        if (producer == &consumer) {
          continue;
        }

        result.push_back({producer, &consumer, read.resource});
      }
    }
    return result;
  }

  std::vector<RenderPassDependency> RenderPlan::dependenciesInto(const RenderPassId& pass) const {
    std::vector<RenderPassDependency> result;
    for (const auto& dependency : dependencies()) {
      if (dependency.consumer->id == pass) {
        result.push_back(dependency);
      }
    }
    return result;
  }

  std::vector<RenderPassDependency> RenderPlan::dependenciesOutOf(const RenderPassId& pass) const {
    std::vector<RenderPassDependency> result;
    for (const auto& dependency : dependencies()) {
      if (dependency.producer->id == pass) {
        result.push_back(dependency);
      }
    }
    return result;
  }

  std::vector<const RenderPassNode*> RenderPlan::executionOrder() const {
    std::vector<const RenderPassNode*> result;
    result.reserve(m_passes.size());

    for (const auto& stage : executionStageIndexes(m_passes, dependencies())) {
      for (const std::size_t passIndex : stage) {
        result.push_back(&m_passes[passIndex]);
      }
    }

    return result;
  }

  std::vector<std::vector<const RenderPassNode*>> RenderPlan::executionStages() const {
    std::vector<std::vector<const RenderPassNode*>> result;
    const auto stageIndexes = executionStageIndexes(m_passes, dependencies());
    result.reserve(stageIndexes.size());

    for (const auto& stage : stageIndexes) {
      std::vector<const RenderPassNode*> passes;
      passes.reserve(stage.size());
      for (const std::size_t passIndex : stage) {
        passes.push_back(&m_passes[passIndex]);
      }
      result.push_back(std::move(passes));
    }

    return result;
  }

  std::optional<int> RenderPlan::executionStageNumber(const RenderPassId& id) const {
    int stageNumber = 1;
    for (const auto& stage : executionStages()) {
      for (const RenderPassNode* pass : stage) {
        if (pass->id == id) {
          return stageNumber;
        }
      }
      ++stageNumber;
    }
    return std::nullopt;
  }

  std::optional<int> RenderPlan::executionOrderNumber(const RenderPassId& id) const {
    int orderNumber = 1;
    for (const RenderPassNode* pass : executionOrder()) {
      if (pass->id == id) {
        return orderNumber;
      }
      ++orderNumber;
    }
    return std::nullopt;
  }

  bool RenderPlan::executionEquivalentTo(const RenderPlan& other) const {
    return m_resources.size() == other.m_resources.size() &&
           std::equal(m_resources.begin(), m_resources.end(), other.m_resources.begin(),
                      sameResourceDescriptor) &&
           m_passes.size() == other.m_passes.size() &&
           std::equal(m_passes.begin(), m_passes.end(), other.m_passes.begin(), samePassNode);
  }

  void RenderPlan::addResource(RenderResourceDescriptor descriptor) {
    m_resources.push_back(std::move(descriptor));
  }

  std::size_t RenderPlan::setResourceDescriptor(RenderResourceDescriptor descriptor) {
    for (auto& resource : m_resources) {
      if (resource.id == descriptor.id) {
        resource = std::move(descriptor);
        return 1;
      }
    }
    return 0;
  }

  void RenderPlan::addPass(RenderPassNode pass) {
    m_passes.push_back(std::move(pass));
  }

  void RenderPlan::addResourceProducer(RenderPassNode producer, RenderResourceDescriptor resource) {
    producer.addWrite(resource.id);

    m_resources.push_back(std::move(resource));
    m_passes.push_back(std::move(producer));
  }

  void RenderPlan::connectProducerToConsumer(RenderPassNode producer,
                                             RenderResourceDescriptor resource,
                                             const RenderPassId& consumerPassId) {
    auto consumer = std::find_if(m_passes.begin(), m_passes.end(), [&](const RenderPassNode& pass) {
      return pass.id == consumerPassId;
    });
    if (consumer == m_passes.end()) {
      throw std::runtime_error("cannot connect producer '" + producer.id + "' to missing pass '" +
                               consumerPassId + "'");
    }

    producer.addWrite(resource.id);
    consumer->addRead(resource.id);

    m_resources.push_back(std::move(resource));
    m_passes.insert(consumer, std::move(producer));
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
        const auto resourceIt = resources.find(write.resource);
        if (resourceIt == resources.end()) {
          result.add({RenderPlanValidationError::Code::UnknownResource,
                      "pass '" + pass.id + "' writes unknown resource '" + write.resource + "'",
                      pass.id, write.resource});
          continue;
        }
        if (!pass.supportsResourceDomain(resourceIt->second->domain)) {
          result.add({RenderPlanValidationError::Code::ResourceDomainMismatch,
                      "pass '" + pass.id + "' cannot write " +
                        std::string(toString(resourceIt->second->domain)) + " resource '" +
                        write.resource + "'",
                      pass.id, write.resource});
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

    for (const auto& resource : m_resources) {
      if (resource.lifetime != RenderResourceLifetime::Exported) {
        continue;
      }

      const auto producerIt = producers.find(resource.id);
      if (producerIt == producers.end()) {
        result.add({RenderPlanValidationError::Code::UnproducedExport,
                    "exported resource '" + resource.id + "' has no producer", "", resource.id});
      }
    }

    std::map<RenderPassId, std::set<RenderPassId>> dependencies;
    for (const auto& pass : m_passes) {
      if (!passReadsWhenExecuted(pass)) {
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
        if (!pass.supportsResourceDomain(resourceIt->second->domain)) {
          result.add({RenderPlanValidationError::Code::ResourceDomainMismatch,
                      "pass '" + pass.id + "' cannot read " +
                        std::string(toString(resourceIt->second->domain)) + " resource '" +
                        read.resource + "'",
                      pass.id, read.resource});
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

        if (producer->id != pass.id && passProducesWhenExecuted(*producer)) {
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
      if (!resource.features.empty()) {
        out << "  features: " << featureList(resource.features, " ") << "\n";
      }
    }

    out << "Execution order:\n";
    for (const RenderPassNode* pass : executionOrder()) {
      out << "- " << pass->id << "\n";
    }

    out << "Execution stages:\n";
    int stageNumber = 1;
    for (const auto& stage : executionStages()) {
      out << "- " << stageNumber++ << ":";
      for (const RenderPassNode* pass : stage) {
        out << " " << pass->id;
      }
      out << "\n";
    }

    out << "Dependencies:\n";
    for (const RenderPassDependency& dependency : dependencies()) {
      out << "- " << dependency.producer->id << " -> " << dependency.consumer->id << " via "
          << dependency.resource << "\n";
    }

    out << "Passes:\n";
    for (const auto& pass : m_passes) {
      out << "- " << pass.id << " [" << toString(pass.kind) << "/" << toString(pass.executor)
          << "] " << (pass.enabled ? "enabled" : "disabled") << "\n";
      const auto stage = executionStageNumber(pass.id);
      const auto order = executionOrderNumber(pass.id);
      if (stage && order) {
        out << "  schedule: stage=" << *stage << ", order=" << *order << "\n";
      }
      out << "  scene: selector=" << pass.sceneView.selector.displayText()
          << ", camera=" << (pass.sceneView.camera ? pass.sceneView.camera->displayText() : "-")
          << ", shading="
          << (pass.sceneView.shadingProfile ? pass.sceneView.shadingProfile->displayText() : "-")
          << "\n";
      if (hasNonDefaultResourceDomains(pass)) {
        out << "  resource domains: " << resourceDomainList(pass.supportedResourceDomains, " ")
            << "\n";
      }
      if (!pass.features.empty()) {
        out << "  features:";
        for (const auto& feature : pass.features)
          out << " " << feature;
        out << "\n";
      }
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
          << toString(resource.type) << "/" << toString(resource.format) << "\\n"
          << toString(resource.domain) << "/" << toString(resource.lifetime) << "\\n"
          << resource.width << "x" << resource.height << ", samples=" << resource.sampleCount;
      if (!resource.features.empty()) {
        out << "\\nfeatures " << dotEscape(featureList(resource.features, ","));
      }
      out << "\"];\n";
    }

    for (const auto& pass : m_passes) {
      const auto stage = executionStageNumber(pass.id);
      const auto order = executionOrderNumber(pass.id);
      out << "  \"pass:" << dotEscape(pass.id) << "\""
          << " [shape=ellipse,label=\"" << dotEscape(pass.name.empty() ? pass.id : pass.name)
          << "\\n"
          << toString(pass.kind) << "/" << toString(pass.executor);
      if (stage && order) {
        out << "\\nstage " << *stage << ", order " << *order;
      }
      if (hasNonDefaultSceneView(pass.sceneView)) {
        out << "\\nselector " << dotEscape(pass.sceneView.selector.displayText());
        if (pass.sceneView.camera) {
          out << "\\ncamera " << dotEscape(pass.sceneView.camera->displayText());
        }
        if (pass.sceneView.shadingProfile) {
          out << "\\nshading " << dotEscape(pass.sceneView.shadingProfile->displayText());
        }
      }
      if (hasNonDefaultResourceDomains(pass)) {
        out << "\\ndomains " << dotEscape(resourceDomainList(pass.supportedResourceDomains, ","));
      }
      out << "\"";
      if (!pass.enabled) {
        out << ",style=dashed,color=gray50,fontcolor=gray50";
      }
      out << "];\n";

      for (const auto& read : pass.reads) {
        out << "  \"resource:" << dotEscape(read.resource) << "\" -> "
            << "\"pass:" << dotEscape(pass.id) << "\";\n";
      }
      for (const auto& write : pass.writes) {
        out << "  \"pass:" << dotEscape(pass.id) << "\" -> "
            << "\"resource:" << dotEscape(write.resource) << "\";\n";
      }
    }

    int stageNumber = 1;
    for (const auto& stage : executionStages()) {
      out << "  { rank=same; ";
      for (const RenderPassNode* pass : stage) {
        out << "\"pass:" << dotEscape(pass->id) << "\"; ";
      }
      out << "} // execution_stage_" << stageNumber++ << "\n";
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

    QJsonArray stageArray;
    int stageNumber = 1;
    for (const auto& stage : executionStages()) {
      QJsonObject stageObject;
      stageObject["index"] = stageNumber++;
      stageObject["passes"] = passIdArray(stage);
      stageArray.append(stageObject);
    }

    QJsonObject result;
    result["resources"] = resourceArray;
    result["passes"] = passArray;
    result["executionStages"] = stageArray;
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
          pass.hasAnyFeature(overrides.disabledFeatures)) {
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
