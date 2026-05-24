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
      OutOfOrderDependency,
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
    * Compiled render graph: a deterministic list of resource descriptors and
    * pass declarations. It can be validated, exported, inspected, modified with
    * graph overrides, and later executed by `GraphRenderEngine`.
    */
  class RenderPlan {
  public:
    const std::vector<RenderPassNode>& passes() const;
    const std::vector<RenderResourceDescriptor>& resources() const;

    const RenderPassNode* findPass(const RenderPassId& id) const;
    const RenderResourceDescriptor* findResource(const RenderResourceId& id) const;
    const RenderPassNode* producerOf(const RenderResourceId& resource) const;
    std::vector<const RenderPassNode*> consumersOf(const RenderResourceId& resource) const;

    void addResource(RenderResourceDescriptor descriptor);
    void addPass(RenderPassNode pass);
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
