#pragma once

#include "render/TracingPathStateBuffer.h"

#include <string>
#include <vector>

namespace render {
  class MetalResidentPathCompactionBackend final : public ResidentPathCompactionBackend {
  public:
    [[nodiscard]] bool deviceAvailable() const;
    [[nodiscard]] std::string deviceUnavailableReason() const;
    [[nodiscard]] bool compactionPathAvailable() const;
    [[nodiscard]] std::string compactionPathUnavailableReason() const;

    [[nodiscard]] const char* name() const override;
    [[nodiscard]] const char* pathStateResidency() const override;
    [[nodiscard]] ResidentPathCompactionResult
    compact(const std::vector<GpuPathStateRecord>& sourceRecords,
            const std::vector<std::uint32_t>& retainedPathIndices) const override;
  };
}
