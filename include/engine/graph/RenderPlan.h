#pragma once

#include "engine/graph/RenderGraphTypes.h"

#include <QJsonObject>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace engine::graph {
  /**
    * One human-readable validation issue for a compiled render plan.
    */
  struct RenderPlanValidationError {
    enum class Code {
      EmptyPassId,
      DuplicatePassId,
      EmptyResourceId,
      DuplicateResourceId,
      UnknownResource,
      DuplicateWriter,
      MissingProducer,
      DisabledDependency,
      DisabledRequiredPass,
      InvalidPassIO,
      InvalidResourceShape,
      Cycle
    };

    Code code;
    std::string message;
    RenderPassId passId;
    RenderResourceId resourceId;
  };

  /**
    * Validation result for a compiled render plan.
    */
  class RenderPlanValidation {
  public:
    bool valid() const;
    const std::vector<RenderPlanValidationError>& errors() const;
    void add(RenderPlanValidationError error);

  private:
    std::vector<RenderPlanValidationError> m_errors;
  };

  /**
    * One declared pass-to-pass dependency implied by a resource edge.
    */
  struct RenderPassDependency {
    const RenderPassNode* producer;
    const RenderPassNode* consumer;
    RenderResourceId resource;
  };

  /**
    * Compiled render graph: resource descriptors plus pass declarations joined
    * by explicit read/write edges. It can be validated, exported, inspected,
    * modified with graph overrides, and later executed by `GraphRenderEngine`.
    */
  class RenderPlan {
  public:
    const std::vector<RenderPassNode>& passes() const;
    const std::vector<RenderResourceDescriptor>& resources() const;

    const RenderPassNode* findPass(const RenderPassId& id) const;
    const RenderResourceDescriptor* findResource(const RenderResourceId& id) const;
    const RenderPassNode* producerOf(const RenderResourceId& resource) const;
    std::vector<const RenderPassNode*> consumersOf(const RenderResourceId& resource) const;
    bool resourceCanReach(const RenderResourceId& source,
                          const RenderResourceId& destination) const;
    std::vector<RenderPassDependency> dependencies() const;
    std::vector<const RenderPassNode*> executionOrder() const;

    /**
      * @returns true when @p other describes the same executable graph shape
      * and pass state.
      *
      * Human-readable names are ignored. Resource descriptors, pass ids,
      * read/write edges, enabled state, disabled behavior, scene/camera
      * selection, and typed pass state must match.
      */
    bool executionEquivalentTo(const RenderPlan& other) const;

    void addResource(RenderResourceDescriptor descriptor);
    std::size_t setResourceDescriptor(RenderResourceDescriptor descriptor);
    void addPass(RenderPassNode pass);
    /**
      * Adds @p resource and @p producer as the pass that writes it.
      *
      * If the pass does not already declare the write edge, this method adds
      * it before storing the pass.
      */
    void addResourceProducer(RenderPassNode producer, RenderResourceDescriptor resource);
    /**
      * Adds @p resource as the edge produced by @p producer and consumed by
      * the existing pass @p consumerPassId.
      *
      * The producer is inserted near the consumer for readable exports. The
      * resource edge is the dependency: this method adds the producer's write
      * and the consumer's read when they are not already present.
      */
    void connectProducerToConsumer(RenderPassNode producer, RenderResourceDescriptor resource,
                                   const RenderPassId& consumerPassId);
    std::size_t routeResourceThroughPass(const RenderResourceId& sourceResource,
                                         RenderResourceDescriptor routedResource,
                                         RenderPassNode pass);
    std::size_t setPassState(RenderPassKind kind, RenderExecutorKind executor,
                             std::shared_ptr<const RenderPassState> state);

    RenderPlanValidation validate() const;
    std::string toText() const;
    std::string toDot() const;
    QJsonObject toJson() const;
    const RenderResourceDescriptor& exportedColorResource() const;
    /**
      * Rebuild a plan from the JSON shape emitted by `toJson()`.
      *
      * Malformed JSON throws `std::runtime_error`; semantic plan problems such
      * as missing producers remain the job of `validate()`.
      */
    static RenderPlan fromJson(const QJsonObject& object);

    RenderPlan withOverrides(const RenderGraphOverrides& overrides) const;

  private:
    std::vector<RenderResourceDescriptor> m_resources;
    std::vector<RenderPassNode> m_passes;
  };

  const char* toString(RenderPlanValidationError::Code value);
}
