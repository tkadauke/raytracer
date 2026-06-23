#pragma once

#include "render/GpuDiffusePathStepReference.h"

#include <memory>
#include <string>
#include <vector>

namespace render {
  struct GpuDiffusePathLoopBackendSupport {
    bool supported{false};
    std::string reason;
  };

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

    [[nodiscard]] static std::shared_ptr<const GpuDiffusePathLoopBackend>
    defaultBackendForGpuRequest();
    [[nodiscard]] static std::shared_ptr<const GpuDiffusePathLoopBackend>
    defaultFullGpuBackendForGpuRequest();

    virtual const char* name() const = 0;
    [[nodiscard]] virtual bool fullGpuPathLoopAvailable() const;
    [[nodiscard]] virtual const char* fullGpuPathLoopUnavailableReason() const;
    [[nodiscard]] virtual const char* platformName() const;
    [[nodiscard]] virtual GpuDiffusePathLoopBackendSupport
    fullGpuPathLoopSupport(const GpuTracingSceneSections& scene) const;
    [[nodiscard]] virtual GpuDiffusePathLoopBackendSupport
    fullGpuPathLoopSupport(const GpuTracingSceneSections& scene,
                           const GpuDiffusePathLoopSettings& settings) const;

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

  class CompactingGpuDiffusePathLoopBackend final : public GpuDiffusePathLoopBackend {
  public:
    explicit CompactingGpuDiffusePathLoopBackend(
      std::shared_ptr<const GpuDiffusePathFrontierCompactionBackend> compactionBackend);

    const char* name() const override;
    [[nodiscard]] const GpuDiffusePathFrontierCompactionBackend& compactionBackend() const;
    [[nodiscard]] GpuDiffusePathLoopResult
    run(const GpuTracingSceneSections& scene,
        const std::vector<GpuDiffusePathStateRecord>& initialPathStates,
        const GpuDiffusePathLoopSettings& settings = {}) const override;

  private:
    std::shared_ptr<const GpuDiffusePathFrontierCompactionBackend> m_compactionBackend;
  };
}
