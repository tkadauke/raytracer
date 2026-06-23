#pragma once

#include "render/GpuDiffusePathStepReference.h"

#include <memory>
#include <vector>

namespace render {
  /**
    * Backend boundary for the compiled diffuse path-loop subset.
    *
    * The default implementation is the CPU reference path loop. Platform
    * backends can implement this interface so the graph pass dispatches the
    * GPU-facing path-loop contract without constructing a specific backend.
    */
  class GpuDiffusePathLoopBackend {
  public:
    virtual ~GpuDiffusePathLoopBackend() = default;

    virtual const char* name() const = 0;

    [[nodiscard]] virtual GpuDiffusePathLoopResult
    run(const GpuTracingSceneSections& scene,
        const std::vector<GpuDiffusePathStateRecord>& initialPathStates,
        const GpuDiffusePathLoopSettings& settings = {}) const = 0;
  };

  class CpuReferenceGpuDiffusePathLoopBackend final : public GpuDiffusePathLoopBackend {
  public:
    [[nodiscard]] static std::shared_ptr<const CpuReferenceGpuDiffusePathLoopBackend>
    sharedInstance();

    const char* name() const override;
    [[nodiscard]] GpuDiffusePathLoopResult
    run(const GpuTracingSceneSections& scene,
        const std::vector<GpuDiffusePathStateRecord>& initialPathStates,
        const GpuDiffusePathLoopSettings& settings = {}) const override;
  };
}
