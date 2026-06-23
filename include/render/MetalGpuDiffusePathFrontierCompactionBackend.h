#pragma once

#include "render/GpuDiffusePathStepReference.h"

#include <string>
#include <vector>

namespace render {
  class MetalGpuDiffusePathFrontierCompactionBackend final
      : public GpuDiffusePathFrontierCompactionBackend {
  public:
    [[nodiscard]] bool deviceAvailable() const;
    [[nodiscard]] std::string deviceUnavailableReason() const;
    [[nodiscard]] bool compactionPathAvailable() const;
    [[nodiscard]] std::string compactionPathUnavailableReason() const;

    [[nodiscard]] const char* name() const override;
    [[nodiscard]] const char* pathStateResidency() const override;
    [[nodiscard]] GpuDiffusePathFrontierCompactionResult
    compact(const std::vector<GpuDiffusePathStateRecord>& sourceRecords,
            const std::vector<std::uint32_t>& retainedPathIndices) const override;
  };
}
