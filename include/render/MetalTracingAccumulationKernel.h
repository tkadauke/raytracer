#pragma once

#include "core/Buffer.h"
#include "core/Color.h"
#include "render/TracingAccumulationLayout.h"

#include <memory>
#include <string>

namespace render {
  class TracingAccumulationBuffer;

  class MetalTracingAccumulationKernel {
  public:
    [[nodiscard]] bool deviceAvailable() const;
    [[nodiscard]] std::string deviceUnavailableReason() const;
    [[nodiscard]] bool accumulationPathAvailable() const;
    [[nodiscard]] std::string accumulationPathUnavailableReason() const;
  };

  class MetalTracingAccumulationBuffer {
  public:
    explicit MetalTracingAccumulationBuffer(const TracingAccumulationLayout& layout);
    MetalTracingAccumulationBuffer(int width, int height);
    ~MetalTracingAccumulationBuffer();

    MetalTracingAccumulationBuffer(const MetalTracingAccumulationBuffer&) = delete;
    MetalTracingAccumulationBuffer& operator=(const MetalTracingAccumulationBuffer&) = delete;

    [[nodiscard]] const TracingAccumulationLayout& layout() const;

    void clear();
    void addSamples(const Buffer<Colord>& colors);
    void resolve(Buffer<unsigned int>& target) const;
    void copyTo(TracingAccumulationBuffer& target) const;

  private:
    struct Private;
    std::unique_ptr<Private> p;
  };
}
