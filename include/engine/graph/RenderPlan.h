#pragma once

#include "engine/graph/RenderGraphTypes.h"

#include <QJsonObject>

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
    * Compiled render graph: a deterministic list of resource descriptors and
    * pass declarations. It can be validated, exported, inspected, modified with
    * graph overrides, and later executed by `GraphRenderEngine`.
    */
  class RenderPlan {
  public:
    const std::vector<RenderPassNode>& passes() const;
    const std::vector<RenderResourceDescriptor>& resources() const;

    void addResource(RenderResourceDescriptor descriptor);
    void addPass(RenderPassNode pass);

    RenderPlanValidation validate() const;
    std::string toText() const;
    std::string toDot() const;
    QJsonObject toJson() const;

    RenderPlan withOverrides(const RenderGraphOverrides& overrides) const;

  private:
    std::vector<RenderResourceDescriptor> m_resources;
    std::vector<RenderPassNode> m_passes;
  };

  const char* toString(RenderPlanValidationError::Code value);
}
