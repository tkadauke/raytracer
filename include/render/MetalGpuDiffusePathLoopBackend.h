#pragma once

#include "render/GpuDiffusePathLoopBackend.h"

#include <memory>

namespace render {
  class MetalGpuDiffusePathLoopBackend final : public GpuDiffusePathLoopBackend {
  public:
    [[nodiscard]] static std::shared_ptr<const MetalGpuDiffusePathLoopBackend> sharedInstance();

    [[nodiscard]] const char* name() const override;
    [[nodiscard]] bool fullGpuPathLoopAvailable() const override;
    [[nodiscard]] const char* fullGpuPathLoopUnavailableReason() const override;
    [[nodiscard]] const char* platformName() const override;
    [[nodiscard]] GpuDiffusePathLoopBackendSupport
    fullGpuPathLoopSupport(const GpuTracingSceneSections& scene,
                           const GpuDiffusePathLoopSettings& settings) const override;
    [[nodiscard]] GpuDiffusePathLoopResult
    run(const GpuTracingSceneSections& scene,
        const std::vector<GpuDiffusePathStateRecord>& initialPathStates,
        const GpuDiffusePathLoopSettings& settings = {}) const override;
    [[nodiscard]] GpuDiffusePathLoopResult
    run(const GpuTracingSceneSections& scene,
        const GpuDiffusePrimaryPathStateGeneration& primaryPathGeneration,
        const GpuDiffusePathLoopSettings& settings = {}) const override;
  };
}
