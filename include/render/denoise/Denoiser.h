#pragma once

#include "core/Color.h"
#include "core/math/Vector.h"

#include <memory>
#include <string>
#include <vector>

template<class T>
class Buffer;

namespace render {
  struct DenoiserFeatureRequest {
    bool albedo = false;
    bool normal = false;
    bool depth = false;

    bool any() const {
      return albedo || normal || depth;
    }
  };

  struct DenoiserFeatureBuffers {
    const Buffer<Colord>* albedo = nullptr;
    const Buffer<Vector3d>* normal = nullptr;
    const Buffer<double>* depth = nullptr;
  };

  struct DenoiserFrame {
    explicit DenoiserFrame(Buffer<Colord>& beautyBuffer)
        : beauty(beautyBuffer) {
    }

    Buffer<Colord>& beauty;
    DenoiserFeatureBuffers features;
  };

  struct DenoiserDiagnostics {
    struct NumericParameter {
      std::string name;
      double value = 0.0;
    };

    std::string name;
    std::vector<NumericParameter> numericParameters;
  };

  class Denoiser {
  public:
    virtual ~Denoiser() = default;

    virtual std::unique_ptr<Denoiser> clone() const = 0;
    virtual const char* diagnosticName() const = 0;
    virtual DenoiserDiagnostics diagnostics() const {
      return DenoiserDiagnostics{diagnosticName(), {}};
    }
    virtual DenoiserFeatureRequest requestedFeatures() const {
      return {};
    }
    void denoise(Buffer<Colord>& buffer) const {
      DenoiserFrame frame(buffer);
      denoiseFrame(frame);
    }
    virtual void denoiseFrame(DenoiserFrame& frame) const = 0;
  };
}
